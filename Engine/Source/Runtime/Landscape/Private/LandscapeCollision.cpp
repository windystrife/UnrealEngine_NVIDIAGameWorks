// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "Misc/Guid.h"
#include "Stats/Stats.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FeedbackContext.h"
#include "UObject/PropertyPortFlags.h"
#include "EngineDefines.h"
#include "Engine/EngineTypes.h"
#include "Components/SceneComponent.h"
#include "AI/Navigation/NavigationTypes.h"
#include "Misc/SecureHash.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "PhysxUserData.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "AI/Navigation/NavigationSystem.h"
#include "LandscapeComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapePrivate.h"
#include "PhysicsPublic.h"
#include "LandscapeDataAccess.h"
#include "PhysXPublic.h"
#include "PhysicsEngine/PhysXSupport.h"
#include "DerivedDataCacheInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeMeshCollisionComponent.h"
#include "FoliageInstanceBase.h"
#include "InstancedFoliageActor.h"
#include "InstancedFoliage.h"
#include "AI/NavigationSystemHelpers.h"
#include "Engine/CollisionProfile.h"
#include "ProfilingDebugging/CookStats.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "EngineGlobals.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceConstant.h"
#if WITH_EDITOR
	#include "IPhysXCooking.h"
#endif

#if ENABLE_COOK_STATS
namespace LandscapeCollisionCookStats
{
	static FCookStats::FDDCResourceUsageStats HeightfieldUsageStats;
	static FCookStats::FDDCResourceUsageStats MeshUsageStats;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		HeightfieldUsageStats.LogStats(AddStat, TEXT("LandscapeCollision.Usage"), TEXT("Heightfield"));
		MeshUsageStats.LogStats(AddStat, TEXT("LandscapeCollision.Usage"), TEXT("Mesh"));
	});
}
#endif

TMap<FGuid, ULandscapeHeightfieldCollisionComponent::FPhysXHeightfieldRef* > GSharedHeightfieldRefs;

ULandscapeHeightfieldCollisionComponent::FPhysXHeightfieldRef::~FPhysXHeightfieldRef()
{
#if WITH_PHYSX
	// Free the existing heightfield data.
	if (RBHeightfield)
	{
		GPhysXPendingKillHeightfield.Add(RBHeightfield);
		RBHeightfield = NULL;
	}
#if WITH_EDITOR
	if (RBHeightfieldEd)
	{
		GPhysXPendingKillHeightfield.Add(RBHeightfieldEd);
		RBHeightfieldEd = NULL;
	}
#endif// WITH_EDITOR
#endif// WITH_PHYSX

	// Remove ourselves from the shared map.
	GSharedHeightfieldRefs.Remove(Guid);
}

TMap<FGuid, ULandscapeMeshCollisionComponent::FPhysXMeshRef* > GSharedMeshRefs;

ULandscapeMeshCollisionComponent::FPhysXMeshRef::~FPhysXMeshRef()
{
#if WITH_PHYSX
	// Free the existing heightfield data.
	if (RBTriangleMesh)
	{
		GPhysXPendingKillTriMesh.Add(RBTriangleMesh);
		RBTriangleMesh = NULL;
	}

#if WITH_EDITOR
	if (RBTriangleMeshEd)
	{
		GPhysXPendingKillTriMesh.Add(RBTriangleMeshEd);
		RBTriangleMeshEd = NULL;
	}
#endif// WITH_EDITOR
#endif// WITH_PHYSX

	// Remove ourselves from the shared map.
	GSharedMeshRefs.Remove(Guid);
}

// Generate a new guid to force a recache of landscape collison derived data
#define LANDSCAPE_COLLISION_DERIVEDDATA_VER	TEXT("84A5A09B87CA4ED3B9B301DECE89D011")

static FString GetHFDDCKeyString(const FName& Format, bool bDefMaterial, const FGuid& StateId, const TArray<UPhysicalMaterial*>& PhysicalMaterials)
{
	FGuid CombinedStateId;
	
	ensure(StateId.IsValid());

	if (bDefMaterial)
	{
		CombinedStateId = StateId;
	}
	else
	{
		// Build a combined state ID based on both the heightfield state and all physical materials.
		FBufferArchive CombinedStateAr;

		// Add main heightfield state
		FGuid HeightfieldState = StateId;
		CombinedStateAr << HeightfieldState;

		// Add physical materials
		for (UPhysicalMaterial* PhysicalMaterial : PhysicalMaterials)
		{
			FString PhysicalMaterialName = PhysicalMaterial->GetPathName().ToUpper();
			CombinedStateAr << PhysicalMaterialName;
		}

		uint32 Hash[5];
		FSHA1::HashBuffer(CombinedStateAr.GetData(), CombinedStateAr.Num(), (uint8*)Hash);
		CombinedStateId = FGuid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]);
	}

	const FString KeyPrefix = FString::Printf(TEXT("%s_%s"), *Format.ToString(), (bDefMaterial ? TEXT("VIS") : TEXT("FULL")));
	return FDerivedDataCacheInterface::BuildCacheKey(*KeyPrefix, LANDSCAPE_COLLISION_DERIVEDDATA_VER, *CombinedStateId.ToString());
}

ECollisionEnabled::Type ULandscapeHeightfieldCollisionComponent::GetCollisionEnabled() const
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		ALandscapeProxy* Proxy = GetLandscapeProxy();

		return Proxy->BodyInstance.GetCollisionEnabled();
	}
	return ECollisionEnabled::QueryAndPhysics;
}

ECollisionResponse ULandscapeHeightfieldCollisionComponent::GetCollisionResponseToChannel(ECollisionChannel Channel) const
{
	ALandscapeProxy* Proxy = GetLandscapeProxy();

	return Proxy->BodyInstance.GetResponseToChannel(Channel);
}

ECollisionChannel ULandscapeHeightfieldCollisionComponent::GetCollisionObjectType() const
{
	ALandscapeProxy* Proxy = GetLandscapeProxy();

	return Proxy->BodyInstance.GetObjectType();
}

const FCollisionResponseContainer& ULandscapeHeightfieldCollisionComponent::GetCollisionResponseToChannels() const
{
	ALandscapeProxy* Proxy = GetLandscapeProxy();

	return Proxy->BodyInstance.GetResponseToChannels();
}

void ULandscapeHeightfieldCollisionComponent::OnCreatePhysicsState()
{
	USceneComponent::OnCreatePhysicsState(); // route OnCreatePhysicsState, skip PrimitiveComponent implementation

	if (!BodyInstance.IsValidBodyInstance())
	{
#if WITH_PHYSX
		CreateCollisionObject();

		if (IsValidRef(HeightfieldRef))
		{
			// Make transform for this landscape component PxActor
			FTransform LandscapeComponentTransform = GetComponentToWorld();
			FMatrix LandscapeComponentMatrix = LandscapeComponentTransform.ToMatrixWithScale();
			bool bIsMirrored = LandscapeComponentMatrix.Determinant() < 0.f;
			if (!bIsMirrored)
			{
				// Unreal and PhysX have opposite handedness, so we need to translate the origin and rearrange the data
				LandscapeComponentMatrix = FTranslationMatrix(FVector(CollisionSizeQuads*CollisionScale, 0, 0)) * LandscapeComponentMatrix;
			}

			// Get the scale to give to PhysX
			FVector LandscapeScale = LandscapeComponentMatrix.ExtractScaling();

			// Reorder the axes
			FVector TerrainX = LandscapeComponentMatrix.GetScaledAxis(EAxis::X);
			FVector TerrainY = LandscapeComponentMatrix.GetScaledAxis(EAxis::Y);
			FVector TerrainZ = LandscapeComponentMatrix.GetScaledAxis(EAxis::Z);
			LandscapeComponentMatrix.SetAxis(0, TerrainX);
			LandscapeComponentMatrix.SetAxis(2, TerrainY);
			LandscapeComponentMatrix.SetAxis(1, TerrainZ);

			PxTransform PhysXLandscapeComponentTransform = U2PTransform(FTransform(LandscapeComponentMatrix));

			const bool bCreateSimpleCollision = SimpleCollisionSizeQuads > 0;
			const float SimpleCollisionScale = bCreateSimpleCollision ? CollisionScale * CollisionSizeQuads / SimpleCollisionSizeQuads : 0;

			// Create the geometry
			PxHeightFieldGeometry LandscapeComponentGeom(HeightfieldRef->RBHeightfield, PxMeshGeometryFlag::eDOUBLE_SIDED, LandscapeScale.Z * LANDSCAPE_ZSCALE, LandscapeScale.Y * CollisionScale, LandscapeScale.X * CollisionScale);

			if (LandscapeComponentGeom.isValid())
			{
				// Creating both a sync and async actor, since this object is static

				// Create the sync scene actor
				PxRigidStatic* HeightFieldActorSync = GPhysXSDK->createRigidStatic(PhysXLandscapeComponentTransform);
				PxShape* HeightFieldShapeSync = GPhysXSDK->createShape(LandscapeComponentGeom, HeightfieldRef->UsedPhysicalMaterialArray.GetData(), HeightfieldRef->UsedPhysicalMaterialArray.Num(), true);
				check(HeightFieldShapeSync);

				// Setup filtering
				PxFilterData PQueryFilterData, PSimFilterData;
				CreateShapeFilterData(GetCollisionObjectType(), FMaskFilter(0), GetOwner()->GetUniqueID(), GetCollisionResponseToChannels(), GetUniqueID(), 0, PQueryFilterData, PSimFilterData, true, false, true);

				// Heightfield is used for simple and complex collision
				PQueryFilterData.word3 |= bCreateSimpleCollision ? EPDF_ComplexCollision : (EPDF_SimpleCollision | EPDF_ComplexCollision);
				PSimFilterData.word3 |= bCreateSimpleCollision ? EPDF_ComplexCollision : (EPDF_SimpleCollision | EPDF_ComplexCollision);
				HeightFieldShapeSync->setQueryFilterData(PQueryFilterData);
				HeightFieldShapeSync->setSimulationFilterData(PSimFilterData);
				HeightFieldShapeSync->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
				HeightFieldShapeSync->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
				HeightFieldShapeSync->setFlag(PxShapeFlag::eVISUALIZATION, true);

				HeightFieldActorSync->attachShape(*HeightFieldShapeSync);

				// attachShape holds its own ref(), so release this here.
				HeightFieldShapeSync->release();

				if (bCreateSimpleCollision)
				{
					PxHeightFieldGeometry LandscapeComponentGeomSimple(HeightfieldRef->RBHeightfieldSimple, PxMeshGeometryFlags(), LandscapeScale.Z * LANDSCAPE_ZSCALE, LandscapeScale.Y * SimpleCollisionScale, LandscapeScale.X * SimpleCollisionScale);
					check(LandscapeComponentGeomSimple.isValid());
					PxShape* HeightFieldShapeSimpleSync = GPhysXSDK->createShape(LandscapeComponentGeomSimple, HeightfieldRef->UsedPhysicalMaterialArray.GetData(), HeightfieldRef->UsedPhysicalMaterialArray.Num(), true);
					check(HeightFieldShapeSimpleSync);

					// Setup filtering
					PxFilterData PQueryFilterDataSimple = PQueryFilterData;
					PxFilterData PSimFilterDataSimple = PSimFilterData;
					PQueryFilterDataSimple.word3 = (PQueryFilterDataSimple.word3 & ~EPDF_ComplexCollision) | EPDF_SimpleCollision;
					PSimFilterDataSimple.word3 = (PSimFilterDataSimple.word3 & ~EPDF_ComplexCollision) | EPDF_SimpleCollision;
					HeightFieldShapeSimpleSync->setQueryFilterData(PQueryFilterDataSimple);
					HeightFieldShapeSimpleSync->setSimulationFilterData(PSimFilterDataSimple);
					HeightFieldShapeSimpleSync->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
					HeightFieldShapeSimpleSync->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
					HeightFieldShapeSimpleSync->setFlag(PxShapeFlag::eVISUALIZATION, true);

					HeightFieldActorSync->attachShape(*HeightFieldShapeSimpleSync);

					// attachShape holds its own ref(), so release this here.
					HeightFieldShapeSimpleSync->release();
				}

#if WITH_EDITOR
				// Create a shape for a heightfield which is used only by the landscape editor
				if (!GetWorld()->IsGameWorld())
				{
					PxHeightFieldGeometry LandscapeComponentGeomEd(HeightfieldRef->RBHeightfieldEd, PxMeshGeometryFlags(), LandscapeScale.Z * LANDSCAPE_ZSCALE, LandscapeScale.Y * CollisionScale, LandscapeScale.X * CollisionScale);
					if (LandscapeComponentGeomEd.isValid())
					{
						PxMaterial* PDefaultMat = GEngine->DefaultPhysMaterial->GetPhysXMaterial();
						PxShape* HeightFieldEdShapeSync = GPhysXSDK->createShape(LandscapeComponentGeomEd, &PDefaultMat, 1, true);
						check(HeightFieldEdShapeSync);

						FCollisionResponseContainer CollisionResponse;
						CollisionResponse.SetAllChannels(ECollisionResponse::ECR_Ignore);
						CollisionResponse.SetResponse(ECollisionChannel::ECC_Visibility, ECR_Block);
						PxFilterData PQueryFilterDataEd, PSimFilterDataEd;
						CreateShapeFilterData(ECollisionChannel::ECC_Visibility, FMaskFilter(0), GetOwner()->GetUniqueID(), CollisionResponse, GetUniqueID(), 0, PQueryFilterDataEd, PSimFilterDataEd, true, false, true);

						PQueryFilterDataEd.word3 |= (EPDF_SimpleCollision | EPDF_ComplexCollision);
						HeightFieldEdShapeSync->setQueryFilterData(PQueryFilterDataEd);
						HeightFieldEdShapeSync->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);

						HeightFieldActorSync->attachShape(*HeightFieldEdShapeSync);

						// attachShape holds its own ref(), so release this here.
						HeightFieldEdShapeSync->release();
					}
				}
#endif// WITH_EDITOR

				FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();

				PxRigidStatic* HeightFieldActorAsync = nullptr;
				bool bHasAsyncScene = PhysScene->HasAsyncScene();
				if (bHasAsyncScene)
				{
					// Create the async scene actor
					HeightFieldActorAsync = GPhysXSDK->createRigidStatic(PhysXLandscapeComponentTransform);
					PxShape* HeightFieldShapeAsync = GPhysXSDK->createShape(LandscapeComponentGeom, HeightfieldRef->UsedPhysicalMaterialArray.GetData(), HeightfieldRef->UsedPhysicalMaterialArray.Num(), true);

					HeightFieldShapeAsync->setQueryFilterData(PQueryFilterData);
					HeightFieldShapeAsync->setSimulationFilterData(PSimFilterData);
					// Only perform scene queries in the synchronous scene for static shapes
					HeightFieldShapeAsync->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
					HeightFieldShapeAsync->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
					HeightFieldShapeAsync->setFlag(PxShapeFlag::eVISUALIZATION, true);

					HeightFieldActorAsync->attachShape(*HeightFieldShapeAsync);

					// attachShape holds its own ref(), so release this here.
					HeightFieldShapeAsync->release();

					if (bCreateSimpleCollision)
					{
						PxHeightFieldGeometry LandscapeComponentGeomSimple(HeightfieldRef->RBHeightfieldSimple, PxMeshGeometryFlags(), LandscapeScale.Z * LANDSCAPE_ZSCALE, LandscapeScale.Y * SimpleCollisionScale, LandscapeScale.X * SimpleCollisionScale);
						check(LandscapeComponentGeomSimple.isValid());
						PxShape* HeightFieldShapeSimpleAsync = GPhysXSDK->createShape(LandscapeComponentGeomSimple, HeightfieldRef->UsedPhysicalMaterialArray.GetData(), HeightfieldRef->UsedPhysicalMaterialArray.Num(), true);
						check(HeightFieldShapeSimpleAsync);

						// Setup filtering
						PxFilterData PQueryFilterDataSimple = PQueryFilterData;
						PxFilterData PSimFilterDataSimple = PSimFilterData;
						PQueryFilterDataSimple.word3 = (PQueryFilterDataSimple.word3 & ~EPDF_ComplexCollision) | EPDF_SimpleCollision;
						PSimFilterDataSimple.word3 = (PSimFilterDataSimple.word3 & ~EPDF_ComplexCollision) | EPDF_SimpleCollision;
						HeightFieldShapeSimpleAsync->setQueryFilterData(PQueryFilterDataSimple);
						HeightFieldShapeSimpleAsync->setSimulationFilterData(PSimFilterDataSimple);
						// Only perform scene queries in the synchronous scene for static shapes
						HeightFieldShapeSimpleAsync->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
						HeightFieldShapeSimpleAsync->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
						HeightFieldShapeSimpleAsync->setFlag(PxShapeFlag::eVISUALIZATION, true);

						HeightFieldActorAsync->attachShape(*HeightFieldShapeSimpleAsync);

						// attachShape holds its own ref(), so release this here.
						HeightFieldShapeSimpleAsync->release();
					}
				}

				// Set body instance data
				BodyInstance.PhysxUserData = FPhysxUserData(&BodyInstance);
				BodyInstance.OwnerComponent = this;
				BodyInstance.SceneIndexSync = PhysScene->PhysXSceneIndex[PST_Sync];
				BodyInstance.SceneIndexAsync = bHasAsyncScene ? PhysScene->PhysXSceneIndex[PST_Async] : 0;
				BodyInstance.RigidActorSync = HeightFieldActorSync;
				BodyInstance.RigidActorAsync = HeightFieldActorAsync;
				HeightFieldActorSync->userData = &BodyInstance.PhysxUserData;
				if (bHasAsyncScene)
				{
					HeightFieldActorAsync->userData = &BodyInstance.PhysxUserData;
				}

				// Add to scenes
				PxScene* SyncScene = PhysScene->GetPhysXScene(PST_Sync);
				SCOPED_SCENE_WRITE_LOCK(SyncScene);
				SyncScene->addActor(*HeightFieldActorSync);

				if (bHasAsyncScene)
				{
					PxScene* AsyncScene = PhysScene->GetPhysXScene(PST_Async);

					SCOPED_SCENE_WRITE_LOCK(AsyncScene);
					AsyncScene->addActor(*HeightFieldActorAsync);
				}
			}

		}
#endif// WITH_PHYSX
	}
}

void ULandscapeHeightfieldCollisionComponent::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);

	if (!bWorldShift || !FPhysScene::SupportsOriginShifting())
	{
		RecreatePhysicsState();
	}
}

void ULandscapeHeightfieldCollisionComponent::CreateCollisionObject()
{
#if WITH_PHYSX
	// If we have not created a heightfield yet - do it now.
	if (!IsValidRef(HeightfieldRef))
	{
		UWorld* World = GetWorld();

		FPhysXHeightfieldRef* ExistingHeightfieldRef = nullptr;
		bool bCheckDDC = true;

		if (!HeightfieldGuid.IsValid())
		{
			HeightfieldGuid = FGuid::NewGuid();
			bCheckDDC = false;
		}
		else
		{
			// Look for a heightfield object with the current Guid (this occurs with PIE)
			ExistingHeightfieldRef = GSharedHeightfieldRefs.FindRef(HeightfieldGuid);
		}

		if (ExistingHeightfieldRef)
		{
			HeightfieldRef = ExistingHeightfieldRef;
		}
		else
		{
#if WITH_EDITOR
			// This should only occur if a level prior to VER_UE4_LANDSCAPE_COLLISION_DATA_COOKING 
			// was resaved using a commandlet and not saved in the editor, or if a PhysicalMaterial asset was deleted.
			if (CookedPhysicalMaterials.Num() == 0 || CookedPhysicalMaterials.Contains(nullptr))
			{
				bCheckDDC = false;
			}

			// Prepare heightfield data
			static FName PhysicsFormatName(FPlatformProperties::GetPhysicsFormat());
			CookCollisionData(PhysicsFormatName, false, bCheckDDC, CookedCollisionData, CookedPhysicalMaterials);

			// The World will clean up any speculatively-loaded data we didn't end up using.
			SpeculativeDDCRequest.Reset();
#endif //WITH_EDITOR

			if (CookedCollisionData.Num())
			{
				HeightfieldRef = GSharedHeightfieldRefs.Add(HeightfieldGuid, new FPhysXHeightfieldRef(HeightfieldGuid));

				// Create heightfield shape
				{
					FPhysXInputStream HeightFieldStream(CookedCollisionData.GetData(), CookedCollisionData.Num());
					HeightfieldRef->RBHeightfield = GPhysXSDK->createHeightField(HeightFieldStream);
					if (SimpleCollisionSizeQuads > 0)
					{
						HeightfieldRef->RBHeightfieldSimple = GPhysXSDK->createHeightField(HeightFieldStream);
					}
				}

				for (UPhysicalMaterial* PhysicalMaterial : CookedPhysicalMaterials)
				{
					HeightfieldRef->UsedPhysicalMaterialArray.Add(PhysicalMaterial->GetPhysXMaterial());
				}

				// Release cooked collison data
				// In cooked builds created collision object will never be deleted while component is alive, so we don't need this data anymore
				if (FPlatformProperties::RequiresCookedData() || World->IsGameWorld())
				{
					CookedCollisionData.Empty();
				}

#if WITH_EDITOR
				// Create heightfield for the landscape editor (no holes in it)
				if (!World->IsGameWorld())
				{
					TArray<UPhysicalMaterial*> CookedMaterialsEd;
					if (CookCollisionData(PhysicsFormatName, true, bCheckDDC, CookedCollisionDataEd, CookedMaterialsEd))
					{
						FPhysXInputStream HeightFieldStream(CookedCollisionDataEd.GetData(), CookedCollisionDataEd.Num());
						HeightfieldRef->RBHeightfieldEd = GPhysXSDK->createHeightField(HeightFieldStream);
					}
				}
#endif //WITH_EDITOR
			}
		}
	}
#endif //WITH_PHYSX
}

#if WITH_EDITOR
void ULandscapeHeightfieldCollisionComponent::SpeculativelyLoadAsyncDDCCollsionData()
{
#if WITH_PHYSX
	if (GetLinkerUE4Version() >= VER_UE4_LANDSCAPE_SERIALIZE_PHYSICS_MATERIALS)
	{
		UWorld* World = GetWorld();
		if (World && HeightfieldGuid.IsValid() && CookedPhysicalMaterials.Num() > 0 && GSharedHeightfieldRefs.FindRef(HeightfieldGuid) == nullptr)
		{
			static FName PhysicsFormatName(FPlatformProperties::GetPhysicsFormat());

			FString Key = GetHFDDCKeyString(PhysicsFormatName, false, HeightfieldGuid, CookedPhysicalMaterials);
			uint32 Handle = GetDerivedDataCacheRef().GetAsynchronous(*Key);
			check(!SpeculativeDDCRequest.IsValid());
			SpeculativeDDCRequest = MakeShareable(new FAsyncPreRegisterDDCRequest(Key, Handle));
			World->AsyncPreRegisterDDCRequests.Add(SpeculativeDDCRequest);
		}
	}
#endif
}

#if WITH_PHYSX
TArray<PxHeightFieldSample> ConvertHeightfieldDataForPhysx(const ULandscapeHeightfieldCollisionComponent* const Component, const int32 CollisionSizeVerts, const bool bIsMirrored, const uint16* Heights, const bool bUseDefMaterial, const uint8* DominantLayers, UPhysicalMaterial* const DefMaterial, TArray<UPhysicalMaterial*> &InOutMaterials)
{
	const int32 NumSamples = FMath::Square(CollisionSizeVerts);

	TArray<PxHeightFieldSample> Samples;
	Samples.Reserve(NumSamples);
	Samples.AddZeroed(NumSamples);

	for (int32 RowIndex = 0; RowIndex < CollisionSizeVerts; RowIndex++)
	{
		for (int32 ColIndex = 0; ColIndex < CollisionSizeVerts; ColIndex++)
		{
			int32 SrcSampleIndex = (ColIndex * CollisionSizeVerts) + (bIsMirrored ? RowIndex : (CollisionSizeVerts - RowIndex - 1));
			int32 DstSampleIndex = (RowIndex * CollisionSizeVerts) + ColIndex;

			PxHeightFieldSample& Sample = Samples[DstSampleIndex];
			Sample.height = ((int32)Heights[SrcSampleIndex] - 32768);

			// Materials are not relevant on the last row/column because they are per-triangle and the last row/column don't own any
			if (RowIndex < CollisionSizeVerts - 1 &&
				ColIndex < CollisionSizeVerts - 1)
			{
				int32 MaterialIndex = 0; // Default physical material.
				if (!bUseDefMaterial && DominantLayers)
				{
					uint8 DominantLayerIdx = DominantLayers[SrcSampleIndex];
					if (Component->ComponentLayerInfos.IsValidIndex(DominantLayerIdx))
					{
						ULandscapeLayerInfoObject* Layer = Component->ComponentLayerInfos[DominantLayerIdx];
						if (Layer == ALandscapeProxy::VisibilityLayer)
						{
							// If it's a hole, override with the hole flag.
							MaterialIndex = PxHeightFieldMaterial::eHOLE;
						}
						else
						{
							UPhysicalMaterial* DominantMaterial = Layer && Layer->PhysMaterial ? Layer->PhysMaterial : DefMaterial;
							MaterialIndex = InOutMaterials.AddUnique(DominantMaterial);
						}
					}
				}

				Sample.materialIndex0 = MaterialIndex;
				Sample.materialIndex1 = MaterialIndex;
			}

			// TODO: edge turning
		}
	}

	return Samples;
}
#endif // WITH_PHYSX

bool ULandscapeHeightfieldCollisionComponent::CookCollisionData(const FName& Format, bool bUseDefMaterial, bool bCheckDDC, TArray<uint8>& OutCookedData, TArray<UPhysicalMaterial*>& InOutMaterials) const
{
#if WITH_PHYSX
	COOK_STAT(auto Timer = LandscapeCollisionCookStats::HeightfieldUsageStats.TimeSyncWork());
	// we have 2 versions of collision objects
	const int32 CookedDataIndex = bUseDefMaterial ? 0 : 1;

	if (bCheckDDC && HeightfieldGuid.IsValid())
	{
		// Ensure that content was saved with physical materials before using DDC data
		if (GetLinkerUE4Version() >= VER_UE4_LANDSCAPE_SERIALIZE_PHYSICS_MATERIALS)
		{
			FString DDCKey = GetHFDDCKeyString(Format, bUseDefMaterial, HeightfieldGuid, InOutMaterials);

			// Check if the speculatively-loaded data loaded and is what we wanted
			if (SpeculativeDDCRequest.IsValid() && DDCKey == SpeculativeDDCRequest->GetKey())
			{
				// If we have a DDC request in flight, just time the synchronous cycles used.
				COOK_STAT(auto WaitTimer = LandscapeCollisionCookStats::HeightfieldUsageStats.TimeAsyncWait());
				SpeculativeDDCRequest->WaitAsynchronousCompletion();
				bool bSuccess = SpeculativeDDCRequest->GetAsynchronousResults(OutCookedData);
				// World will clean up remaining reference
				SpeculativeDDCRequest.Reset();
				if (bSuccess)
				{
					COOK_STAT(Timer.Cancel());
					COOK_STAT(WaitTimer.AddHit(OutCookedData.Num()));
					bShouldSaveCookedDataToDDC[CookedDataIndex] = false;
					return true;
				}
				else
				{
					// If the DDC request failed, then we waited for nothing and will build the resource anyway. Just ignore the wait timer and treat it all as sync time.
					COOK_STAT(WaitTimer.Cancel());
				}
			}

			if (GetDerivedDataCacheRef().GetSynchronous(*DDCKey, OutCookedData))
			{
				COOK_STAT(Timer.AddHit(OutCookedData.Num()));
				bShouldSaveCookedDataToDDC[CookedDataIndex] = false;
				return true;
			}
		}
	}

	ALandscapeProxy* Proxy = GetLandscapeProxy();
	if (!Proxy || !Proxy->GetRootComponent())
	{
		// We didn't actually build anything, so just track the cycles.
		COOK_STAT(Timer.TrackCyclesOnly());
		return false;
	}

	UPhysicalMaterial* DefMaterial = Proxy->DefaultPhysMaterial ? Proxy->DefaultPhysMaterial : GEngine->DefaultPhysMaterial;

	// GetComponentTransform() might not be initialized at this point, so use landscape transform
	const FVector LandscapeScale = Proxy->GetRootComponent()->RelativeScale3D;
	const bool bIsMirrored = (LandscapeScale.X*LandscapeScale.Y*LandscapeScale.Z) < 0.f;

	const bool bGenerateSimpleCollision = SimpleCollisionSizeQuads > 0 && !bUseDefMaterial;

	const int32 CollisionSizeVerts = CollisionSizeQuads + 1;
	const int32 SimpleCollisionSizeVerts = SimpleCollisionSizeQuads > 0 ? SimpleCollisionSizeQuads + 1 : 0;
	const int32 NumSamples = FMath::Square(CollisionSizeVerts);
	const int32 NumSimpleSamples = FMath::Square(SimpleCollisionSizeVerts);

	const uint16* Heights = (const uint16*)CollisionHeightData.LockReadOnly();
	check(CollisionHeightData.GetElementCount() == NumSamples + NumSimpleSamples);

	//
	const uint8* DominantLayers = nullptr;
	if (DominantLayerData.GetElementCount())
	{
		DominantLayers = (const uint8*)DominantLayerData.LockReadOnly();
		check(DominantLayerData.GetElementCount() == NumSamples + NumSimpleSamples);
	}

	// List of materials which is actually used by heightfield
	InOutMaterials.Empty();

	TArray<PxHeightFieldSample> Samples;
	TArray<PxHeightFieldSample> SimpleSamples;
	Samples = ConvertHeightfieldDataForPhysx(this, CollisionSizeVerts, bIsMirrored, Heights, bUseDefMaterial, DominantLayers, DefMaterial, InOutMaterials);

	if (bGenerateSimpleCollision)
	{
		SimpleSamples = ConvertHeightfieldDataForPhysx(this, SimpleCollisionSizeVerts, bIsMirrored, Heights + NumSamples, bUseDefMaterial, DominantLayers, DefMaterial, InOutMaterials);
	}

	CollisionHeightData.Unlock();
	if (DominantLayers)
	{
		DominantLayerData.Unlock();
	}

	// Add the default physical material to be used used when we have no dominant data.
	if (InOutMaterials.Num() == 0)
	{
		InOutMaterials.Add(DefMaterial);
	}

	//
	FIntPoint HFSize = FIntPoint(CollisionSizeVerts, CollisionSizeVerts);
	TArray<uint8> OutData;

	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	const IPhysXCooking* Cooker = TPM->FindPhysXCooking(Format);
	bool Result = Cooker->CookHeightField(Format, HFSize, Samples.GetData(), Samples.GetTypeSize(), OutData);

	if (Result && bGenerateSimpleCollision)
	{
		FIntPoint HFSizeSimple = FIntPoint(SimpleCollisionSizeVerts, SimpleCollisionSizeVerts);
		Result = Cooker->CookHeightField(Format, HFSizeSimple, SimpleSamples.GetData(), SimpleSamples.GetTypeSize(), OutData);
	}

	if (Result)
	{
		COOK_STAT(Timer.AddMiss(OutData.Num()));
		OutCookedData.SetNumUninitialized(OutData.Num());
		FMemory::Memcpy(OutCookedData.GetData(), OutData.GetData(), OutData.Num());

		if (bShouldSaveCookedDataToDDC[CookedDataIndex] && HeightfieldGuid.IsValid())
		{
			GetDerivedDataCacheRef().Put(*GetHFDDCKeyString(Format, bUseDefMaterial, HeightfieldGuid, InOutMaterials), OutCookedData);
			bShouldSaveCookedDataToDDC[CookedDataIndex] = false;
		}
	}
	else
	{
		// if we failed to build the resource, just time the cycles we spent.
		COOK_STAT(Timer.TrackCyclesOnly());
		OutCookedData.Empty();
		InOutMaterials.Empty();
	}

	return Result;
#endif	// WITH_PHYSX

	return false;
}

bool ULandscapeMeshCollisionComponent::CookCollisionData(const FName& Format, bool bUseDefMaterial, bool bCheckDDC, TArray<uint8>& OutCookedData, TArray<UPhysicalMaterial*>& InOutMaterials) const
{
#if WITH_PHYSX
	COOK_STAT(auto Timer = LandscapeCollisionCookStats::MeshUsageStats.TimeSyncWork());
	// we have 2 versions of collision objects
	const int32 CookedDataIndex = bUseDefMaterial ? 0 : 1;

	if (bCheckDDC)
	{
		// Ensure that content was saved with physical materials before using DDC data
		if (GetLinkerUE4Version() >= VER_UE4_LANDSCAPE_SERIALIZE_PHYSICS_MATERIALS && MeshGuid.IsValid())
		{
			FString DDCKey = GetHFDDCKeyString(Format, bUseDefMaterial, MeshGuid, InOutMaterials);

			// Check if the speculatively-loaded data loaded and is what we wanted
			if (SpeculativeDDCRequest.IsValid() && DDCKey == SpeculativeDDCRequest->GetKey())
			{
				// If we have a DDC request in flight, just time the synchronous cycles used.
				COOK_STAT(auto WaitTimer = LandscapeCollisionCookStats::MeshUsageStats.TimeAsyncWait());
				SpeculativeDDCRequest->WaitAsynchronousCompletion();
				bool bSuccess = SpeculativeDDCRequest->GetAsynchronousResults(OutCookedData);
				// World will clean up remaining reference
				SpeculativeDDCRequest.Reset();
				if (bSuccess)
				{
					COOK_STAT(Timer.Cancel());
					COOK_STAT(WaitTimer.AddHit(OutCookedData.Num()));
					bShouldSaveCookedDataToDDC[CookedDataIndex] = false;
					return true;
				}
				else
				{
					// If the DDC request failed, then we waited for nothing and will build the resource anyway. Just ignore the wait timer and treat it all as sync time.
					COOK_STAT(WaitTimer.Cancel());
				}
			}

			if (GetDerivedDataCacheRef().GetSynchronous(*DDCKey, OutCookedData))
			{
				COOK_STAT(Timer.AddHit(OutCookedData.Num()));
				bShouldSaveCookedDataToDDC[CookedDataIndex] = false;
				return true;
			}
		}
	}
	
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	UPhysicalMaterial* DefMaterial = (Proxy && Proxy->DefaultPhysMaterial != nullptr) ? Proxy->DefaultPhysMaterial : GEngine->DefaultPhysMaterial;

	// List of materials which is actually used by trimesh
	InOutMaterials.Empty();

	TArray<FVector>			Vertices;
	TArray<FTriIndices>		Indices;
	TArray<uint16>			MaterialIndices;

	const int32 CollisionSizeVerts = CollisionSizeQuads + 1;
	const int32 NumVerts = FMath::Square(CollisionSizeVerts);

	const uint16* Heights = (const uint16*)CollisionHeightData.LockReadOnly();
	const uint16* XYOffsets = (const uint16*)CollisionXYOffsetData.LockReadOnly();
	check(CollisionHeightData.GetElementCount() == NumVerts);
	check(CollisionXYOffsetData.GetElementCount() == NumVerts * 2);

	const uint8* DominantLayers = nullptr;
	if (DominantLayerData.GetElementCount() > 0)
	{
		DominantLayers = (const uint8*)DominantLayerData.LockReadOnly();
	}

	// Scale all verts into temporary vertex buffer.
	Vertices.SetNumUninitialized(NumVerts);
	for (int32 i = 0; i < NumVerts; i++)
	{
		int32 X = i % CollisionSizeVerts;
		int32 Y = i / CollisionSizeVerts;
		Vertices[i].Set(X + ((float)XYOffsets[i * 2] - 32768.f) * LANDSCAPE_XYOFFSET_SCALE, Y + ((float)XYOffsets[i * 2 + 1] - 32768.f) * LANDSCAPE_XYOFFSET_SCALE, ((float)Heights[i] - 32768.f) * LANDSCAPE_ZSCALE);
	}

	const int32 NumTris = FMath::Square(CollisionSizeQuads) * 2;
	Indices.SetNumUninitialized(NumTris);
	if (DominantLayers)
	{
		MaterialIndices.SetNumUninitialized(NumTris);
	}

	int32 TriangleIdx = 0;
	for (int32 y = 0; y < CollisionSizeQuads; y++)
	{
		for (int32 x = 0; x < CollisionSizeQuads; x++)
		{
			int32 DataIdx = x + y * CollisionSizeVerts;
			bool bHole = false;

			int32 MaterialIndex = 0; // Default physical material.
			if (!bUseDefMaterial && DominantLayers)
			{
				uint8 DominantLayerIdx = DominantLayers[DataIdx];
				if (ComponentLayerInfos.IsValidIndex(DominantLayerIdx))
				{
					ULandscapeLayerInfoObject* Layer = ComponentLayerInfos[DominantLayerIdx];
					if (Layer == ALandscapeProxy::VisibilityLayer)
					{
						// If it's a hole, override with the hole flag.
						bHole = true;
					}
					else
					{
						UPhysicalMaterial* DominantMaterial = Layer && Layer->PhysMaterial ? Layer->PhysMaterial : DefMaterial;
						MaterialIndex = InOutMaterials.AddUnique(DominantMaterial);
					}
				}
			}

			FTriIndices& TriIndex1 = Indices[TriangleIdx];
			if (bHole)
			{
				TriIndex1.v0 = (x + 0) + (y + 0) * CollisionSizeVerts;
				TriIndex1.v1 = TriIndex1.v0;
				TriIndex1.v2 = TriIndex1.v0;
			}
			else
			{
				TriIndex1.v0 = (x + 0) + (y + 0) * CollisionSizeVerts;
				TriIndex1.v1 = (x + 1) + (y + 1) * CollisionSizeVerts;
				TriIndex1.v2 = (x + 1) + (y + 0) * CollisionSizeVerts;
			}

			if (DominantLayers)
			{
				MaterialIndices[TriangleIdx] = MaterialIndex;
			}
			TriangleIdx++;

			FTriIndices& TriIndex2 = Indices[TriangleIdx];
			if (bHole)
			{
				TriIndex2.v0 = (x + 0) + (y + 0) * CollisionSizeVerts;
				TriIndex2.v1 = TriIndex2.v0;
				TriIndex2.v2 = TriIndex2.v0;
			}
			else
			{
				TriIndex2.v0 = (x + 0) + (y + 0) * CollisionSizeVerts;
				TriIndex2.v1 = (x + 0) + (y + 1) * CollisionSizeVerts;
				TriIndex2.v2 = (x + 1) + (y + 1) * CollisionSizeVerts;
			}

			if (DominantLayers)
			{
				MaterialIndices[TriangleIdx] = MaterialIndex;
			}
			TriangleIdx++;
		}
	}

	CollisionHeightData.Unlock();
	CollisionXYOffsetData.Unlock();
	if (DominantLayers)
	{
		DominantLayerData.Unlock();
	}

	// Add the default physical material to be used used when we have no dominant data.
	if (InOutMaterials.Num() == 0)
	{
		InOutMaterials.Add(DefMaterial);
	}

	bool bFlipNormals = true;
	TArray<uint8> OutData;
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	const IPhysXCooking* Cooker = TPM->FindPhysXCooking(Format);
	bool Result = Cooker->CookTriMesh(Format, EPhysXMeshCookFlags::Default, Vertices, Indices, MaterialIndices, bFlipNormals, OutData);

	if (Result)
	{
		COOK_STAT(Timer.AddMiss(OutData.Num()));
		OutCookedData.SetNumUninitialized(OutData.Num());
		FMemory::Memcpy(OutCookedData.GetData(), OutData.GetData(), OutData.Num());

		if (bShouldSaveCookedDataToDDC[CookedDataIndex] && MeshGuid.IsValid())
		{
			GetDerivedDataCacheRef().Put(*GetHFDDCKeyString(Format, bUseDefMaterial, MeshGuid, InOutMaterials), OutCookedData);
			bShouldSaveCookedDataToDDC[CookedDataIndex] = false;
		}
	}
	else
	{
		// We didn't actually build anything, so just track the cycles.
		COOK_STAT(Timer.TrackCyclesOnly());
		OutCookedData.Empty();
		InOutMaterials.Empty();
	}

	return Result;

#endif // WITH_PHYSX
	return false;
}
#endif //WITH_EDITOR

void ULandscapeMeshCollisionComponent::CreateCollisionObject()
{
#if WITH_PHYSX	
	// If we have not created a heightfield yet - do it now.
	if (!IsValidRef(MeshRef))
	{
		FPhysXMeshRef* ExistingMeshRef = nullptr;
		bool bCheckDDC = true;

		if (!MeshGuid.IsValid())
		{
			MeshGuid = FGuid::NewGuid();
			bCheckDDC = false;
		}
		else
		{
			// Look for a heightfield object with the current Guid (this occurs with PIE)
			ExistingMeshRef = GSharedMeshRefs.FindRef(MeshGuid);
		}

		if (ExistingMeshRef)
		{
			MeshRef = ExistingMeshRef;
		}
		else
		{
#if WITH_EDITOR
		    // This should only occur if a level prior to VER_UE4_LANDSCAPE_COLLISION_DATA_COOKING 
		    // was resaved using a commandlet and not saved in the editor, or if a PhysicalMaterial asset was deleted.
		    if (CookedPhysicalMaterials.Num() == 0 || CookedPhysicalMaterials.Contains(nullptr))
		    {
			    bCheckDDC = false;
		    }

			// Create cooked physics data
			static FName PhysicsFormatName(FPlatformProperties::GetPhysicsFormat());
			CookCollisionData(PhysicsFormatName, false, bCheckDDC, CookedCollisionData, CookedPhysicalMaterials);
#endif //WITH_EDITOR

			if (CookedCollisionData.Num())
			{
				MeshRef = GSharedMeshRefs.Add(MeshGuid, new FPhysXMeshRef(MeshGuid));

				// Create physics objects
				FPhysXInputStream Buffer(CookedCollisionData.GetData(), CookedCollisionData.Num());
				MeshRef->RBTriangleMesh = GPhysXSDK->createTriangleMesh(Buffer);

				for (UPhysicalMaterial* PhysicalMaterial : CookedPhysicalMaterials)
				{
					MeshRef->UsedPhysicalMaterialArray.Add(PhysicalMaterial->GetPhysXMaterial());
				}

				// Release cooked collison data
				// In cooked builds created collision object will never be deleted while component is alive, so we don't need this data anymore
				if (FPlatformProperties::RequiresCookedData() || GetWorld()->IsGameWorld())
				{
					CookedCollisionData.Empty();
				}

#if WITH_EDITOR
				// Create collision mesh for the landscape editor (no holes in it)
				if (!GetWorld()->IsGameWorld())
				{
					TArray<UPhysicalMaterial*> CookedMaterialsEd;
					if (CookCollisionData(PhysicsFormatName, true, bCheckDDC, CookedCollisionDataEd, CookedMaterialsEd))
					{
						FPhysXInputStream MeshStream(CookedCollisionDataEd.GetData(), CookedCollisionDataEd.Num());
						MeshRef->RBTriangleMeshEd = GPhysXSDK->createTriangleMesh(MeshStream);
					}
				}
#endif //WITH_EDITOR
			}
		}
	}
#endif //WITH_PHYSX
}

void ULandscapeMeshCollisionComponent::OnCreatePhysicsState()
{
	USceneComponent::OnCreatePhysicsState(); // route OnCreatePhysicsState, skip PrimitiveComponent implementation

	if (!BodyInstance.IsValidBodyInstance())
	{
#if WITH_PHYSX
		// This will do nothing, because we create trimesh at component PostLoad event, unless we destroyed it explicitly
		CreateCollisionObject();

		if (IsValidRef(MeshRef))
		{
			// Make transform for this landscape component PxActor
			FTransform LandscapeComponentTransform = GetComponentToWorld();
			FMatrix LandscapeComponentMatrix = LandscapeComponentTransform.ToMatrixWithScale();
			bool bIsMirrored = LandscapeComponentMatrix.Determinant() < 0.f;
			if (bIsMirrored)
			{
				// Unreal and PhysX have opposite handedness, so we need to translate the origin and rearrange the data
				LandscapeComponentMatrix = FTranslationMatrix(FVector(CollisionSizeQuads, 0, 0)) * LandscapeComponentMatrix;
			}

			// Get the scale to give to PhysX
			FVector LandscapeScale = LandscapeComponentMatrix.ExtractScaling();
			PxTransform PhysXLandscapeComponentTransform = U2PTransform(FTransform(LandscapeComponentMatrix));

			// Create tri-mesh shape
			PxTriangleMeshGeometry PTriMeshGeom;
			PTriMeshGeom.triangleMesh = MeshRef->RBTriangleMesh;
			PTriMeshGeom.scale.scale.x = LandscapeScale.X * CollisionScale;
			PTriMeshGeom.scale.scale.y = LandscapeScale.Y * CollisionScale;
			PTriMeshGeom.scale.scale.z = LandscapeScale.Z;

			if (PTriMeshGeom.isValid())
			{
				// Creating both a sync and async actor, since this object is static

				// Create the sync scene actor
				PxRigidStatic* MeshActorSync = GPhysXSDK->createRigidStatic(PhysXLandscapeComponentTransform);
				PxShape* MeshShapeSync = GPhysXSDK->createShape(PTriMeshGeom, MeshRef->UsedPhysicalMaterialArray.GetData(), MeshRef->UsedPhysicalMaterialArray.Num(), true);
				check(MeshShapeSync);

				// Setup filtering
				PxFilterData PQueryFilterData, PSimFilterData;
				CreateShapeFilterData(GetCollisionObjectType(), FMaskFilter(0), GetOwner()->GetUniqueID(), GetCollisionResponseToChannels(), GetUniqueID(), 0, PQueryFilterData, PSimFilterData, false, false, true);

				// Heightfield is used for simple and complex collision
				PQueryFilterData.word3 |= (EPDF_SimpleCollision | EPDF_ComplexCollision);
				PSimFilterData.word3 |= (EPDF_SimpleCollision | EPDF_ComplexCollision);
				MeshShapeSync->setQueryFilterData(PQueryFilterData);
				MeshShapeSync->setSimulationFilterData(PSimFilterData);
				MeshShapeSync->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
				MeshShapeSync->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
				MeshShapeSync->setFlag(PxShapeFlag::eVISUALIZATION, true);

				MeshActorSync->attachShape(*MeshShapeSync);
				MeshShapeSync->release();

				FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();

				PxRigidStatic* MeshActorAsync = nullptr;
				bool bHasAsyncScene = PhysScene->HasAsyncScene();
				if (bHasAsyncScene)
				{
					// Create the async scene actor
					MeshActorAsync = GPhysXSDK->createRigidStatic(PhysXLandscapeComponentTransform);
					PxShape* MeshShapeAsync = GPhysXSDK->createShape(PTriMeshGeom, MeshRef->UsedPhysicalMaterialArray.GetData(), MeshRef->UsedPhysicalMaterialArray.Num(), true);
					check(MeshShapeAsync);

					MeshShapeAsync->setQueryFilterData(PQueryFilterData);
					MeshShapeAsync->setSimulationFilterData(PSimFilterData);
					// Only perform scene queries in the synchronous scene for static shapes
					MeshShapeAsync->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
					MeshShapeAsync->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
					MeshShapeAsync->setFlag(PxShapeFlag::eVISUALIZATION, true);	// Setting visualization flag, in case we visualize only the async scene

					MeshActorAsync->attachShape(*MeshShapeAsync);
					MeshShapeAsync->release();
				}

#if WITH_EDITOR
				// Create a shape for a mesh which is used only by the landscape editor
				if (!GetWorld()->IsGameWorld())
				{
					PxTriangleMeshGeometry PTriMeshGeomEd;
					PTriMeshGeomEd.triangleMesh = MeshRef->RBTriangleMeshEd;
					PTriMeshGeomEd.scale.scale.x = LandscapeScale.X * CollisionScale;
					PTriMeshGeomEd.scale.scale.y = LandscapeScale.Y * CollisionScale;
					PTriMeshGeomEd.scale.scale.z = LandscapeScale.Z;
					if (PTriMeshGeomEd.isValid())
					{
						PxMaterial* PDefaultMat = GEngine->DefaultPhysMaterial->GetPhysXMaterial();
						PxShape* MeshShapeEdSync = GPhysXSDK->createShape(PTriMeshGeomEd, &PDefaultMat, 1, true);
						check(MeshShapeEdSync);

						FCollisionResponseContainer CollisionResponse;
						CollisionResponse.SetAllChannels(ECollisionResponse::ECR_Ignore);
						CollisionResponse.SetResponse(ECollisionChannel::ECC_Visibility, ECR_Block);
						PxFilterData PQueryFilterDataEd, PSimFilterDataEd;
						CreateShapeFilterData(ECollisionChannel::ECC_Visibility, FMaskFilter(0), GetOwner()->GetUniqueID(), CollisionResponse, GetUniqueID(), 0, PQueryFilterDataEd, PSimFilterDataEd, true, false, true);

						PQueryFilterDataEd.word3 |= (EPDF_SimpleCollision | EPDF_ComplexCollision);
						MeshShapeEdSync->setQueryFilterData(PQueryFilterDataEd);
						MeshShapeEdSync->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);

						MeshActorSync->attachShape(*MeshShapeEdSync);
						MeshShapeEdSync->release();
					}
				}
#endif// WITH_EDITOR

				// Set body instance data
				BodyInstance.PhysxUserData = FPhysxUserData(&BodyInstance);
				BodyInstance.OwnerComponent = this;
				BodyInstance.SceneIndexSync = PhysScene->PhysXSceneIndex[PST_Sync];
				BodyInstance.SceneIndexAsync = bHasAsyncScene ? PhysScene->PhysXSceneIndex[PST_Async] : 0;
				BodyInstance.RigidActorSync = MeshActorSync;
				BodyInstance.RigidActorAsync = MeshActorAsync;
				MeshActorSync->userData = &BodyInstance.PhysxUserData;
				if (bHasAsyncScene)
				{
					MeshActorAsync->userData = &BodyInstance.PhysxUserData;
				}

				// Add to scenes
				PxScene* SyncScene = PhysScene->GetPhysXScene(PST_Sync);
				SCOPED_SCENE_WRITE_LOCK(SyncScene);
				SyncScene->addActor(*MeshActorSync);

				if (bHasAsyncScene)
				{
					PxScene* AsyncScene = PhysScene->GetPhysXScene(PST_Async);

					SCOPED_SCENE_WRITE_LOCK(AsyncScene);
					AsyncScene->addActor(*MeshActorAsync);
				}
			}
			else
			{
				UE_LOG(LogLandscape, Log, TEXT("ULandscapeMeshCollisionComponent::OnCreatePhysicsState(): TriMesh invalid"));
			}
		}
#endif // WITH_PHYSX
	}
}

void ULandscapeMeshCollisionComponent::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);

	if (!bWorldShift || !FPhysScene::SupportsOriginShifting())
	{
		RecreatePhysicsState();
	}
}

void ULandscapeMeshCollisionComponent::DestroyComponent(bool bPromoteChildren/*= false*/)
{
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	if (Proxy)
	{
		Proxy->CollisionComponents.Remove(this);
	}

	Super::DestroyComponent(bPromoteChildren);
}

#if WITH_EDITOR
void ULandscapeHeightfieldCollisionComponent::UpdateHeightfieldRegion(int32 ComponentX1, int32 ComponentY1, int32 ComponentX2, int32 ComponentY2)
{
#if WITH_PHYSX
	if (IsValidRef(HeightfieldRef))
	{
		// If we're currently sharing this data with a PIE session, we need to make a new heightfield.
		if (HeightfieldRef->GetRefCount() > 1)
		{
			RecreateCollision();
			return;
		}

		if (BodyInstance.RigidActorSync == NULL)
		{
			return;
		}

		// We don't lock the async scene as we only set the geometry in the sync scene's RigidActor.
		// This function is used only during painting for line traces by the painting tools.
		SCOPED_SCENE_WRITE_LOCK(GetPhysXSceneFromIndex(BodyInstance.SceneIndexSync));

		int32 CollisionSizeVerts = CollisionSizeQuads + 1;
		int32 SimpleCollisionSizeVerts = SimpleCollisionSizeQuads > 0 ? SimpleCollisionSizeQuads + 1 : 0;

		bool bIsMirrored = GetComponentToWorld().GetDeterminant() < 0.f;

		uint16* Heights = (uint16*)CollisionHeightData.Lock(LOCK_READ_ONLY);
		check(CollisionHeightData.GetElementCount() == (FMath::Square(CollisionSizeVerts) + FMath::Square(SimpleCollisionSizeVerts)));

		// PhysX heightfield has the X and Y axis swapped, and the X component is also inverted
		int32 HeightfieldX1 = ComponentY1;
		int32 HeightfieldY1 = (bIsMirrored ? ComponentX1 : (CollisionSizeVerts - ComponentX2 - 1));
		int32 DstVertsX = ComponentY2 - ComponentY1 + 1;
		int32 DstVertsY = ComponentX2 - ComponentX1 + 1;

		TArray<PxHeightFieldSample> Samples;
		Samples.AddZeroed(DstVertsX*DstVertsY);

		// Traverse the area in destination heigthfield coordinates
		for (int32 RowIndex = 0; RowIndex < DstVertsY; RowIndex++)
		{
			for (int32 ColIndex = 0; ColIndex < DstVertsX; ColIndex++)
			{
				int32 SrcX = bIsMirrored ? (RowIndex + ComponentX1) : (ComponentX2 - RowIndex);
				int32 SrcY = ColIndex + ComponentY1;
				int32 SrcSampleIndex = (SrcY * CollisionSizeVerts) + SrcX;
				check(SrcSampleIndex < FMath::Square(CollisionSizeVerts));
				int32 DstSampleIndex = (RowIndex * DstVertsX) + ColIndex;

				PxHeightFieldSample& Sample = Samples[DstSampleIndex];
				Sample.height = FMath::Clamp<int32>(((int32)Heights[SrcSampleIndex] - 32768), -32768, 32767);

				Sample.materialIndex0 = 0;
				Sample.materialIndex1 = 0;
			}
		}

		CollisionHeightData.Unlock();

		PxHeightFieldDesc SubDesc;
		SubDesc.format = PxHeightFieldFormat::eS16_TM;
		SubDesc.nbColumns = DstVertsX;
		SubDesc.nbRows = DstVertsY;
		SubDesc.samples.data = Samples.GetData();
		SubDesc.samples.stride = sizeof(PxU32);
		SubDesc.flags = PxHeightFieldFlag::eNO_BOUNDARY_EDGES;


		HeightfieldRef->RBHeightfieldEd->modifySamples(HeightfieldX1, HeightfieldY1, SubDesc, true);

		//
		// Reset geometry of heightfield shape. Required by the modifySamples
		//
		FVector LandscapeScale = GetComponentToWorld().GetScale3D().GetAbs();
		// Create the geometry
		PxHeightFieldGeometry LandscapeComponentGeom(HeightfieldRef->RBHeightfieldEd, PxMeshGeometryFlags(), LandscapeScale.Z * LANDSCAPE_ZSCALE, LandscapeScale.Y * CollisionScale, LandscapeScale.X * CollisionScale);

		{
			SCOPED_SCENE_WRITE_LOCK(BodyInstance.RigidActorSync->getScene());

			FInlinePxShapeArray PShapes;
			const int32 NumShapes = FillInlinePxShapeArray_AssumesLocked(PShapes, *(BodyInstance.RigidActorSync));
			if (NumShapes > 1)
			{
				PShapes[1]->setGeometry(LandscapeComponentGeom);
			}
		}
	}

#endif// WITH_PHYSX
}
#endif// WITH_EDITOR

void ULandscapeHeightfieldCollisionComponent::DestroyComponent(bool bPromoteChildren/*= false*/)
{
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	if (Proxy)
	{
		Proxy->CollisionComponents.Remove(this);
	}

	Super::DestroyComponent(bPromoteChildren);
}

FBoxSphereBounds ULandscapeHeightfieldCollisionComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return CachedLocalBox.TransformBy(LocalToWorld);
}

void ULandscapeHeightfieldCollisionComponent::BeginDestroy()
{
	HeightfieldRef = NULL;
	HeightfieldGuid = FGuid();
	Super::BeginDestroy();
}

void ULandscapeMeshCollisionComponent::BeginDestroy()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		MeshRef = NULL;
		MeshGuid = FGuid();
	}

	Super::BeginDestroy();
}

void ULandscapeHeightfieldCollisionComponent::RecreateCollision()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		HeightfieldRef = NULL;
		HeightfieldGuid = FGuid();

		RecreatePhysicsState();
	}
}

#if WITH_EDITORONLY_DATA
void ULandscapeHeightfieldCollisionComponent::SnapFoliageInstances(const FBox& InInstanceBox)
{
	UWorld* ComponentWorld = GetWorld();
	for (TActorIterator<AInstancedFoliageActor> It(ComponentWorld); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		const auto BaseId = IFA->InstanceBaseCache.GetInstanceBaseId(this);
		if (BaseId == FFoliageInstanceBaseCache::InvalidBaseId)
		{
			continue;
		}
			
		for (auto& MeshPair : IFA->FoliageMeshes)
		{
			// Find the per-mesh info matching the mesh.
			UFoliageType* Settings = MeshPair.Key;
			FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
			
			const auto* InstanceSet = MeshInfo.ComponentHash.Find(BaseId);
			if (InstanceSet)
			{
				float TraceExtentSize = Bounds.SphereRadius * 2.f + 10.f; // extend a little
				FVector TraceVector = GetOwner()->GetRootComponent()->GetComponentTransform().GetUnitAxis(EAxis::Z) * TraceExtentSize;

				bool bFirst = true;
				TArray<int32> InstancesToRemove;
				TSet<UHierarchicalInstancedStaticMeshComponent*> AffectedFoliageComponets;

				for (int32 InstanceIndex : *InstanceSet)
				{
					FFoliageInstance& Instance = MeshInfo.Instances[InstanceIndex];

					// Test location should remove any Z offset
					FVector TestLocation = FMath::Abs(Instance.ZOffset) > KINDA_SMALL_NUMBER
						? (FVector)Instance.GetInstanceWorldTransform().TransformPosition(FVector(0, 0, -Instance.ZOffset))
						: Instance.Location;

					if (InInstanceBox.IsInside(TestLocation))
					{
						if (bFirst)
						{
							bFirst = false;
							Modify();
						}

						FVector Start = TestLocation + TraceVector;
						FVector End = TestLocation - TraceVector;

						TArray<FHitResult> Results;
						UWorld* World = GetWorld();
						check(World);
						// Editor specific landscape heightfield uses ECC_Visibility collision channel
						World->LineTraceMultiByObjectType(Results, Start, End, FCollisionObjectQueryParams(ECollisionChannel::ECC_Visibility), FCollisionQueryParams(SCENE_QUERY_STAT(FoliageSnapToLandscape), true));

						bool bFoundHit = false;
						for (const FHitResult& Hit : Results)
						{
							if (Hit.Component == this)
							{
								bFoundHit = true;
								if ((TestLocation - Hit.Location).SizeSquared() > KINDA_SMALL_NUMBER)
								{
									IFA->Modify();

									// Remove instance location from the hash. Do not need to update ComponentHash as we re-add below.
									MeshInfo.InstanceHash->RemoveInstance(Instance.Location, InstanceIndex);

									// Update the instance editor data
									Instance.Location = Hit.Location;

									if (Instance.Flags & FOLIAGE_AlignToNormal)
									{
										// Remove previous alignment and align to new normal.
										Instance.Rotation = Instance.PreAlignRotation;
										Instance.AlignToNormal(Hit.Normal, Settings->AlignMaxAngle);
									}

									// Reapply the Z offset in local space
									if (FMath::Abs(Instance.ZOffset) > KINDA_SMALL_NUMBER)
									{
										Instance.Location = Instance.GetInstanceWorldTransform().TransformPosition(FVector(0, 0, Instance.ZOffset));
									}

									// Todo: add do validation with other parameters such as max/min height etc.

									check(MeshInfo.Component);
									MeshInfo.Component->Modify();
									MeshInfo.Component->UpdateInstanceTransform(InstanceIndex, Instance.GetInstanceWorldTransform(), true, false);
									// Re-add the new instance location to the hash
									MeshInfo.InstanceHash->InsertInstance(Instance.Location, InstanceIndex);

									MeshInfo.Component->MarkRenderStateDirty();
								}
								break;
							}
						}

						if (!bFoundHit)
						{
							// Couldn't find new spot - remove instance
							InstancesToRemove.Add(InstanceIndex);
						}

						AffectedFoliageComponets.Add(MeshInfo.Component);
					}
				}

				// Remove any unused instances
				MeshInfo.RemoveInstances(IFA, InstancesToRemove, true);

				for (UHierarchicalInstancedStaticMeshComponent* FoliageComp : AffectedFoliageComponets)
				{
					FoliageComp->InvalidateLightingCache();
				}
			}
		}
	}
}
#endif // WITH_EDITORONLY_DATA

void ULandscapeMeshCollisionComponent::RecreateCollision()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		MeshRef = NULL;
		MeshGuid = FGuid();
	}

	Super::RecreateCollision();
}

void ULandscapeHeightfieldCollisionComponent::Serialize(FArchive& Ar)
{
#if WITH_EDITOR
	if (Ar.UE4Ver() >= VER_UE4_LANDSCAPE_COLLISION_DATA_COOKING)
	{
		// Cook data here so CookedPhysicalMaterials is always up to date
		if (Ar.IsCooking() && !HasAnyFlags(RF_ClassDefaultObject))
		{
			FName Format = Ar.CookingTarget()->GetPhysicsFormat(nullptr);
			CookCollisionData(Format, false, true, CookedCollisionData, CookedPhysicalMaterials);
			if (HeightfieldGuid.IsValid())
			{
				GetDerivedDataCacheRef().Put(*GetHFDDCKeyString(Format, false, HeightfieldGuid, CookedPhysicalMaterials), CookedCollisionData);
			}
		}
	}
#endif// WITH_EDITOR

	// this will also serialize CookedPhysicalMaterials
	Super::Serialize(Ar);

	if (Ar.UE4Ver() < VER_UE4_LANDSCAPE_COLLISION_DATA_COOKING)
	{
#if WITH_EDITORONLY_DATA
		CollisionHeightData.Serialize(Ar, this);
		DominantLayerData.Serialize(Ar, this);
#endif//WITH_EDITORONLY_DATA
	}
	else
	{
		bool bCooked = Ar.IsCooking();
		Ar << bCooked;

		if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
		{
			UE_LOG(LogPhysics, Fatal, TEXT("This platform requires cooked packages, and physX data was not cooked into %s."), *GetFullName());
		}

		if (bCooked)
		{
			CookedCollisionData.BulkSerialize(Ar);
		}
		else
		{
#if WITH_EDITORONLY_DATA
			// For PIE, we won't need the source height data if we already have a shared reference to the heightfield
			if (!(Ar.GetPortFlags() & PPF_DuplicateForPIE) || !HeightfieldGuid.IsValid() || GSharedMeshRefs.FindRef(HeightfieldGuid) == nullptr)
			{
				CollisionHeightData.Serialize(Ar, this);
				DominantLayerData.Serialize(Ar, this);
			}
#endif//WITH_EDITORONLY_DATA
		}
	}
}

void ULandscapeMeshCollisionComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.UE4Ver() < VER_UE4_LANDSCAPE_COLLISION_DATA_COOKING)
	{
#if WITH_EDITORONLY_DATA
		// conditional serialization in later versions
		CollisionXYOffsetData.Serialize(Ar, this);
#endif// WITH_EDITORONLY_DATA
	}

	// PhysX cooking mesh data
	bool bCooked = false;
	if (Ar.UE4Ver() >= VER_UE4_ADD_COOKED_TO_LANDSCAPE)
	{
		bCooked = Ar.IsCooking();
		Ar << bCooked;
	}

	if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
	{
		UE_LOG(LogPhysics, Fatal, TEXT("This platform requires cooked packages, and physX data was not cooked into %s."), *GetFullName());
	}

	if (bCooked)
	{
		// triangle mesh cooked data should be serialized in ULandscapeHeightfieldCollisionComponent
	}
	else if (Ar.UE4Ver() >= VER_UE4_LANDSCAPE_COLLISION_DATA_COOKING)
	{
#if WITH_EDITORONLY_DATA		
		// we serialize raw collision data only with non-cooked content
		CollisionXYOffsetData.Serialize(Ar, this);
#endif// WITH_EDITORONLY_DATA
	}
}

#if WITH_EDITOR
void ULandscapeHeightfieldCollisionComponent::PostEditImport()
{
	Super::PostEditImport();
	// Reinitialize physics after paste
	if (CollisionSizeQuads > 0)
	{
		RecreateCollision();
	}
}

void ULandscapeHeightfieldCollisionComponent::PostEditUndo()
{
	Super::PostEditUndo();

	// Reinitialize physics after undo
	if (CollisionSizeQuads > 0)
	{
		RecreateCollision();
	}

	UNavigationSystem::UpdateComponentInNavOctree(*this);
}

bool ULandscapeHeightfieldCollisionComponent::ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	if (ShowFlags.Landscape)
	{
		return Super::ComponentIsTouchingSelectionBox(InSelBBox, ShowFlags, bConsiderOnlyBSP, bMustEncompassEntireComponent);
	}

	return false;
}

bool ULandscapeHeightfieldCollisionComponent::ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	if (ShowFlags.Landscape)
	{
		return Super::ComponentIsTouchingSelectionFrustum(InFrustum, ShowFlags, bConsiderOnlyBSP, bMustEncompassEntireComponent);
	}

	return false;
}
#endif

bool ULandscapeHeightfieldCollisionComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	check(IsInGameThread());
#if WITH_PHYSX
	if (IsValidRef(HeightfieldRef) && HeightfieldRef->RBHeightfield)
	{
		FTransform HFToW = GetComponentTransform();
		if (HeightfieldRef->RBHeightfieldSimple)
		{
			const float SimpleCollisionScale = CollisionScale * CollisionSizeQuads / SimpleCollisionSizeQuads;
			HFToW.MultiplyScale3D(FVector(SimpleCollisionScale, SimpleCollisionScale, LANDSCAPE_ZSCALE));
			GeomExport.ExportPxHeightField(HeightfieldRef->RBHeightfieldSimple, HFToW);
		}
		else
		{
			HFToW.MultiplyScale3D(FVector(CollisionScale, CollisionScale, LANDSCAPE_ZSCALE));
			GeomExport.ExportPxHeightField(HeightfieldRef->RBHeightfield, HFToW);
		}
	}
#endif// WITH_PHYSX
	return false;
}

void ULandscapeHeightfieldCollisionComponent::GatherGeometrySlice(FNavigableGeometryExport& GeomExport, const FBox& SliceBox) const
{
	// note that this function can get called off game thread
	if (CachedHeightFieldSamples.IsEmpty() == false)
	{
		FTransform HFToW = GetComponentTransform();
		HFToW.MultiplyScale3D(FVector(CollisionScale, CollisionScale, LANDSCAPE_ZSCALE));

		GeomExport.ExportHeightFieldSlice(CachedHeightFieldSamples, HeightfieldRowsCount, HeightfieldColumnsCount, HFToW, SliceBox);
	}
}

ENavDataGatheringMode ULandscapeHeightfieldCollisionComponent::GetGeometryGatheringMode() const
{ 
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	return Proxy ? Proxy->NavigationGeometryGatheringMode : ENavDataGatheringMode::Default;
}

void ULandscapeHeightfieldCollisionComponent::PrepareGeometryExportSync()
{
	//check(IsInGameThread());
#if WITH_PHYSX
	if (IsValidRef(HeightfieldRef) && HeightfieldRef->RBHeightfield != nullptr && CachedHeightFieldSamples.IsEmpty())
	{
		const UWorld* World = GetWorld();

		if (World != nullptr)
		{
			HeightfieldRowsCount = HeightfieldRef->RBHeightfield->getNbRows();
			HeightfieldColumnsCount = HeightfieldRef->RBHeightfield->getNbColumns();
				
			if (CachedHeightFieldSamples.Heights.Num() != HeightfieldRowsCount * HeightfieldRowsCount)
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_NavMesh_ExportPxHeightField_saveCells);

				CachedHeightFieldSamples.Heights.SetNumUninitialized(HeightfieldRowsCount * HeightfieldRowsCount);

				TArray<PxHeightFieldSample> HFSamples;
				HFSamples.SetNumUninitialized(HeightfieldRowsCount * HeightfieldRowsCount);
				HeightfieldRef->RBHeightfield->saveCells(HFSamples.GetData(), HFSamples.Num()*HFSamples.GetTypeSize());

				for (int32 SampleIndex = 0; SampleIndex < HFSamples.Num(); ++SampleIndex)
				{
					const PxHeightFieldSample& Sample = HFSamples[SampleIndex];
					CachedHeightFieldSamples.Heights[SampleIndex] = Sample.height;
					CachedHeightFieldSamples.Holes.Add((Sample.materialIndex0 == PxHeightFieldMaterial::eHOLE));
				}
			}
		}
	}
#endif// WITH_PHYSX
}

bool ULandscapeMeshCollisionComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	check(IsInGameThread());
#if WITH_PHYSX
	if (IsValidRef(MeshRef) && MeshRef->RBTriangleMesh != nullptr)
	{
		FTransform MeshToW = GetComponentTransform();
		MeshToW.MultiplyScale3D(FVector(CollisionScale, CollisionScale, 1.f));

		if (MeshRef->RBTriangleMesh->getTriangleMeshFlags() & PxTriangleMeshFlag::e16_BIT_INDICES)
		{
			GeomExport.ExportPxTriMesh16Bit(MeshRef->RBTriangleMesh, MeshToW);
		}
		else
		{
			GeomExport.ExportPxTriMesh32Bit(MeshRef->RBTriangleMesh, MeshToW);
		}
	}
#endif// WITH_PHYSX
	return false;
}

void ULandscapeHeightfieldCollisionComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	// PostLoad of the landscape can decide to recreate collision, in which case this components checks are irrelevant
	if (!HasAnyFlags(RF_ClassDefaultObject) && !IsPendingKill())
	{
		bShouldSaveCookedDataToDDC[0] = true;
		bShouldSaveCookedDataToDDC[1] = true;

		ALandscapeProxy* LandscapeProxy = GetLandscapeProxy();
		if (ensure(LandscapeProxy) && GIsEditor)
		{
			// This is to ensure that component relative location is exact section base offset value
			float CheckRelativeLocationX = float(SectionBaseX - LandscapeProxy->LandscapeSectionOffset.X);
			float CheckRelativeLocationY = float(SectionBaseY - LandscapeProxy->LandscapeSectionOffset.Y);
			if (CheckRelativeLocationX != RelativeLocation.X || 
				CheckRelativeLocationY != RelativeLocation.Y)
			{
				UE_LOG(LogLandscape, Warning, TEXT("ULandscapeHeightfieldCollisionComponent RelativeLocation disagrees with its section base, attempted automated fix: '%s', %f,%f vs %f,%f."),
					*GetFullName(), RelativeLocation.X, RelativeLocation.Y, CheckRelativeLocationX, CheckRelativeLocationY);
				RelativeLocation.X = CheckRelativeLocationX;
				RelativeLocation.Y = CheckRelativeLocationY;
			}
		}

		UWorld* World = GetWorld();
		if (World && World->IsGameWorld())
		{
			SpeculativelyLoadAsyncDDCCollsionData();
		}
	}
#endif//WITH_EDITOR
}

void ULandscapeHeightfieldCollisionComponent::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);

	if (!IsRunningCommandlet())
	{
#if WITH_EDITOR
		ALandscapeProxy* Proxy = GetLandscapeProxy();
		if (Proxy && Proxy->bBakeMaterialPositionOffsetIntoCollision)
		{
			if (!RenderComponent->GrassData->HasData() || RenderComponent->IsGrassMapOutdated())
			{
				if (!RenderComponent->CanRenderGrassMap())
				{
					RenderComponent->MaterialInstances[0]->GetMaterialResource(GetWorld()->FeatureLevel)->FinishCompilation();
				}
				RenderComponent->RenderGrassMap();
			}
		}

		static const FName PhysicsFormatName(FPlatformProperties::GetPhysicsFormat());
		if (CookedCollisionData.Num() && HeightfieldGuid.IsValid())
		{
			GetDerivedDataCacheRef().Put(*GetHFDDCKeyString(PhysicsFormatName, false, HeightfieldGuid, CookedPhysicalMaterials), CookedCollisionData);
		}

		if (CookedCollisionDataEd.Num() && HeightfieldGuid.IsValid())
		{
			GetDerivedDataCacheRef().Put(*GetHFDDCKeyString(PhysicsFormatName, true, HeightfieldGuid, TArray<UPhysicalMaterial*>()), CookedCollisionDataEd);
		}
#endif// WITH_EDITOR
	}
}

#if WITH_EDITOR
void ULandscapeInfo::UpdateAllAddCollisions()
{
	XYtoAddCollisionMap.Reset();

	// Don't recreate add collisions if the landscape is not registered. This can happen during Undo.
	if (GetLandscapeProxy())
	{
		for (auto It = XYtoComponentMap.CreateIterator(); It; ++It)
		{
			const ULandscapeComponent* const Component = It.Value();
			if (ensure(Component))
			{
				const FIntPoint ComponentBase = Component->GetSectionBase() / ComponentSizeQuads;

				const FIntPoint NeighborsKeys[8] =
				{
					ComponentBase + FIntPoint(-1, -1),
					ComponentBase + FIntPoint(+0, -1),
					ComponentBase + FIntPoint(+1, -1),
					ComponentBase + FIntPoint(-1, +0),
					ComponentBase + FIntPoint(+1, +0),
					ComponentBase + FIntPoint(-1, +1),
					ComponentBase + FIntPoint(+0, +1),
					ComponentBase + FIntPoint(+1, +1)
				};

				// Search for Neighbors...
				for (int32 i = 0; i < 8; ++i)
				{
					ULandscapeComponent* NeighborComponent = XYtoComponentMap.FindRef(NeighborsKeys[i]);

					// UpdateAddCollision() treats a null CollisionComponent as an empty hole
					if (!NeighborComponent || !NeighborComponent->CollisionComponent.IsValid())
					{
						UpdateAddCollision(NeighborsKeys[i]);
					}
				}
			}
		}
	}
}

void ULandscapeInfo::UpdateAddCollision(FIntPoint LandscapeKey)
{
	FLandscapeAddCollision& AddCollision = XYtoAddCollisionMap.FindOrAdd(LandscapeKey);

	// 8 Neighbors...
	// 0 1 2
	// 3   4
	// 5 6 7
	FIntPoint NeighborsKeys[8] =
	{
		LandscapeKey + FIntPoint(-1, -1),
		LandscapeKey + FIntPoint(+0, -1),
		LandscapeKey + FIntPoint(+1, -1),
		LandscapeKey + FIntPoint(-1, +0),
		LandscapeKey + FIntPoint(+1, +0),
		LandscapeKey + FIntPoint(-1, +1),
		LandscapeKey + FIntPoint(+0, +1),
		LandscapeKey + FIntPoint(+1, +1)
	};

	// Todo: Use data accessor not collision

	ULandscapeHeightfieldCollisionComponent* NeighborCollisions[8];
	// Search for Neighbors...
	for (int32 i = 0; i < 8; ++i)
	{
		ULandscapeComponent* Comp = XYtoComponentMap.FindRef(NeighborsKeys[i]);
		if (Comp)
		{
			NeighborCollisions[i] = Comp->CollisionComponent.Get();
		}
		else
		{
			NeighborCollisions[i] = NULL;
		}
	}

	uint8 CornerSet = 0;
	uint16 HeightCorner[4];

	// Corner Cases...
	if (NeighborCollisions[0])
	{
		uint16* Heights = (uint16*)NeighborCollisions[0]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		int32 CollisionSizeVerts = NeighborCollisions[0]->CollisionSizeQuads + 1;
		HeightCorner[0] = Heights[CollisionSizeVerts - 1 + (CollisionSizeVerts - 1)*CollisionSizeVerts];
		CornerSet |= 1;
		NeighborCollisions[0]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[2])
	{
		uint16* Heights = (uint16*)NeighborCollisions[2]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		int32 CollisionSizeVerts = NeighborCollisions[2]->CollisionSizeQuads + 1;
		HeightCorner[1] = Heights[(CollisionSizeVerts - 1)*CollisionSizeVerts];
		CornerSet |= 1 << 1;
		NeighborCollisions[2]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[5])
	{
		uint16* Heights = (uint16*)NeighborCollisions[5]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		int32 CollisionSizeVerts = NeighborCollisions[5]->CollisionSizeQuads + 1;
		HeightCorner[2] = Heights[(CollisionSizeVerts - 1)];
		CornerSet |= 1 << 2;
		NeighborCollisions[5]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[7])
	{
		uint16* Heights = (uint16*)NeighborCollisions[7]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		int32 CollisionSizeVerts = NeighborCollisions[7]->CollisionSizeQuads + 1;
		HeightCorner[3] = Heights[0];
		CornerSet |= 1 << 3;
		NeighborCollisions[7]->CollisionHeightData.Unlock();
	}

	// Other cases...
	if (NeighborCollisions[1])
	{
		uint16* Heights = (uint16*)NeighborCollisions[1]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		int32 CollisionSizeVerts = NeighborCollisions[1]->CollisionSizeQuads + 1;
		HeightCorner[0] = Heights[(CollisionSizeVerts - 1)*CollisionSizeVerts];
		CornerSet |= 1;
		HeightCorner[1] = Heights[CollisionSizeVerts - 1 + (CollisionSizeVerts - 1)*CollisionSizeVerts];
		CornerSet |= 1 << 1;
		NeighborCollisions[1]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[3])
	{
		uint16* Heights = (uint16*)NeighborCollisions[3]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		int32 CollisionSizeVerts = NeighborCollisions[3]->CollisionSizeQuads + 1;
		HeightCorner[0] = Heights[(CollisionSizeVerts - 1)];
		CornerSet |= 1;
		HeightCorner[2] = Heights[CollisionSizeVerts - 1 + (CollisionSizeVerts - 1)*CollisionSizeVerts];
		CornerSet |= 1 << 2;
		NeighborCollisions[3]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[4])
	{
		uint16* Heights = (uint16*)NeighborCollisions[4]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		int32 CollisionSizeVerts = NeighborCollisions[4]->CollisionSizeQuads + 1;
		HeightCorner[1] = Heights[0];
		CornerSet |= 1 << 1;
		HeightCorner[3] = Heights[(CollisionSizeVerts - 1)*CollisionSizeVerts];
		CornerSet |= 1 << 3;
		NeighborCollisions[4]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[6])
	{
		uint16* Heights = (uint16*)NeighborCollisions[6]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		int32 CollisionSizeVerts = NeighborCollisions[6]->CollisionSizeQuads + 1;
		HeightCorner[2] = Heights[0];
		CornerSet |= 1 << 2;
		HeightCorner[3] = Heights[(CollisionSizeVerts - 1)];
		CornerSet |= 1 << 3;
		NeighborCollisions[6]->CollisionHeightData.Unlock();
	}

	// Fill unset values
	// First iteration only for valid values distance 1 propagation
	// Second iteration fills left ones...
	FillCornerValues(CornerSet, HeightCorner);
	//check(CornerSet == 15);

	FIntPoint SectionBase = LandscapeKey * ComponentSizeQuads;

	// Transform Height to Vectors...
	FTransform LtoW = GetLandscapeProxy()->LandscapeActorToWorld();
	AddCollision.Corners[0] = LtoW.TransformPosition(FVector(SectionBase.X                     , SectionBase.Y                     , LandscapeDataAccess::GetLocalHeight(HeightCorner[0])));
	AddCollision.Corners[1] = LtoW.TransformPosition(FVector(SectionBase.X + ComponentSizeQuads, SectionBase.Y                     , LandscapeDataAccess::GetLocalHeight(HeightCorner[1])));
	AddCollision.Corners[2] = LtoW.TransformPosition(FVector(SectionBase.X                     , SectionBase.Y + ComponentSizeQuads, LandscapeDataAccess::GetLocalHeight(HeightCorner[2])));
	AddCollision.Corners[3] = LtoW.TransformPosition(FVector(SectionBase.X + ComponentSizeQuads, SectionBase.Y + ComponentSizeQuads, LandscapeDataAccess::GetLocalHeight(HeightCorner[3])));
}

void ULandscapeHeightfieldCollisionComponent::ExportCustomProperties(FOutputDevice& Out, uint32 Indent)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	uint16* Heights = (uint16*)CollisionHeightData.Lock(LOCK_READ_ONLY);
	int32 NumHeights = FMath::Square(CollisionSizeQuads + 1);
	check(CollisionHeightData.GetElementCount() == NumHeights);

	Out.Logf(TEXT("%sCustomProperties CollisionHeightData "), FCString::Spc(Indent));
	for (int32 i = 0; i < NumHeights; i++)
	{
		Out.Logf(TEXT("%d "), Heights[i]);
	}

	CollisionHeightData.Unlock();
	Out.Logf(TEXT("\r\n"));

	int32 NumDominantLayerSamples = DominantLayerData.GetElementCount();
	check(NumDominantLayerSamples == 0 || NumDominantLayerSamples == NumHeights);

	if (NumDominantLayerSamples > 0)
	{
		uint8* DominantLayerSamples = (uint8*)DominantLayerData.Lock(LOCK_READ_ONLY);

		Out.Logf(TEXT("%sCustomProperties DominantLayerData "), FCString::Spc(Indent));
		for (int32 i = 0; i < NumDominantLayerSamples; i++)
		{
			Out.Logf(TEXT("%02x"), DominantLayerSamples[i]);
		}

		DominantLayerData.Unlock();
		Out.Logf(TEXT("\r\n"));
	}
}

void ULandscapeHeightfieldCollisionComponent::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	if (FParse::Command(&SourceText, TEXT("CollisionHeightData")))
	{
		int32 NumHeights = FMath::Square(CollisionSizeQuads + 1);

		CollisionHeightData.Lock(LOCK_READ_WRITE);
		uint16* Heights = (uint16*)CollisionHeightData.Realloc(NumHeights);
		FMemory::Memzero(Heights, sizeof(uint16)*NumHeights);

		FParse::Next(&SourceText);
		int32 i = 0;
		while (FChar::IsDigit(*SourceText))
		{
			if (i < NumHeights)
			{
				Heights[i++] = FCString::Atoi(SourceText);
				while (FChar::IsDigit(*SourceText))
				{
					SourceText++;
				}
			}

			FParse::Next(&SourceText);
		}

		CollisionHeightData.Unlock();

		if (i != NumHeights)
		{
			Warn->Logf(*NSLOCTEXT("Core", "SyntaxError", "Syntax Error").ToString());
		}
	}
	else if (FParse::Command(&SourceText, TEXT("DominantLayerData")))
	{
		int32 NumDominantLayerSamples = FMath::Square(CollisionSizeQuads + 1);

		DominantLayerData.Lock(LOCK_READ_WRITE);
		uint8* DominantLayerSamples = (uint8*)DominantLayerData.Realloc(NumDominantLayerSamples);
		FMemory::Memzero(DominantLayerSamples, NumDominantLayerSamples);

		FParse::Next(&SourceText);
		int32 i = 0;
		while (SourceText[0] && SourceText[1])
		{
			if (i < NumDominantLayerSamples)
			{
				DominantLayerSamples[i++] = FParse::HexDigit(SourceText[0]) * 16 + FParse::HexDigit(SourceText[1]);
			}
			SourceText += 2;
		}

		DominantLayerData.Unlock();

		if (i != NumDominantLayerSamples)
		{
			Warn->Logf(*NSLOCTEXT("Core", "SyntaxError", "Syntax Error").ToString());
		}
	}
}

void ULandscapeMeshCollisionComponent::ExportCustomProperties(FOutputDevice& Out, uint32 Indent)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	Super::ExportCustomProperties(Out, Indent);

	uint16* XYOffsets = (uint16*)CollisionXYOffsetData.Lock(LOCK_READ_ONLY);
	int32 NumOffsets = FMath::Square(CollisionSizeQuads + 1) * 2;
	check(CollisionXYOffsetData.GetElementCount() == NumOffsets);

	Out.Logf(TEXT("%sCustomProperties CollisionXYOffsetData "), FCString::Spc(Indent));
	for (int32 i = 0; i < NumOffsets; i++)
	{
		Out.Logf(TEXT("%d "), XYOffsets[i]);
	}

	CollisionXYOffsetData.Unlock();
	Out.Logf(TEXT("\r\n"));
}

void ULandscapeMeshCollisionComponent::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	if (FParse::Command(&SourceText, TEXT("CollisionHeightData")))
	{
		int32 NumHeights = FMath::Square(CollisionSizeQuads + 1);

		CollisionHeightData.Lock(LOCK_READ_WRITE);
		uint16* Heights = (uint16*)CollisionHeightData.Realloc(NumHeights);
		FMemory::Memzero(Heights, sizeof(uint16)*NumHeights);

		FParse::Next(&SourceText);
		int32 i = 0;
		while (FChar::IsDigit(*SourceText))
		{
			if (i < NumHeights)
			{
				Heights[i++] = FCString::Atoi(SourceText);
				while (FChar::IsDigit(*SourceText))
				{
					SourceText++;
				}
			}

			FParse::Next(&SourceText);
		}

		CollisionHeightData.Unlock();

		if (i != NumHeights)
		{
			Warn->Logf(*NSLOCTEXT("Core", "SyntaxError", "Syntax Error").ToString());
		}
	}
	else if (FParse::Command(&SourceText, TEXT("DominantLayerData")))
	{
		int32 NumDominantLayerSamples = FMath::Square(CollisionSizeQuads + 1);

		DominantLayerData.Lock(LOCK_READ_WRITE);
		uint8* DominantLayerSamples = (uint8*)DominantLayerData.Realloc(NumDominantLayerSamples);
		FMemory::Memzero(DominantLayerSamples, NumDominantLayerSamples);

		FParse::Next(&SourceText);
		int32 i = 0;
		while (SourceText[0] && SourceText[1])
		{
			if (i < NumDominantLayerSamples)
			{
				DominantLayerSamples[i++] = FParse::HexDigit(SourceText[0]) * 16 + FParse::HexDigit(SourceText[1]);
			}
			SourceText += 2;
		}

		DominantLayerData.Unlock();

		if (i != NumDominantLayerSamples)
		{
			Warn->Logf(*NSLOCTEXT("Core", "SyntaxError", "Syntax Error").ToString());
		}
	}
	else if (FParse::Command(&SourceText, TEXT("CollisionXYOffsetData")))
	{
		int32 NumOffsets = FMath::Square(CollisionSizeQuads + 1) * 2;

		CollisionXYOffsetData.Lock(LOCK_READ_WRITE);
		uint16* Offsets = (uint16*)CollisionXYOffsetData.Realloc(NumOffsets);
		FMemory::Memzero(Offsets, sizeof(uint16)*NumOffsets);

		FParse::Next(&SourceText);
		int32 i = 0;
		while (FChar::IsDigit(*SourceText))
		{
			if (i < NumOffsets)
			{
				Offsets[i++] = FCString::Atoi(SourceText);
				while (FChar::IsDigit(*SourceText))
				{
					SourceText++;
				}
			}

			FParse::Next(&SourceText);
		}

		CollisionXYOffsetData.Unlock();

		if (i != NumOffsets)
		{
			Warn->Logf(*NSLOCTEXT("Core", "SyntaxError", "Syntax Error").ToString());
		}
	}
}

ULandscapeInfo* ULandscapeHeightfieldCollisionComponent::GetLandscapeInfo() const
{
	return GetLandscapeProxy()->GetLandscapeInfo();
}

#endif // WITH_EDITOR

ALandscapeProxy* ULandscapeHeightfieldCollisionComponent::GetLandscapeProxy() const
{
	return CastChecked<ALandscapeProxy>(GetOuter());
}

FIntPoint ULandscapeHeightfieldCollisionComponent::GetSectionBase() const
{
	return FIntPoint(SectionBaseX, SectionBaseY);
}

void ULandscapeHeightfieldCollisionComponent::SetSectionBase(FIntPoint InSectionBase)
{
	SectionBaseX = InSectionBase.X;
	SectionBaseY = InSectionBase.Y;
}

ULandscapeHeightfieldCollisionComponent::ULandscapeHeightfieldCollisionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	bGenerateOverlapEvents = false;
	CastShadow = false;
	bUseAsOccluder = true;
	bAllowCullDistanceVolume = false;
	Mobility = EComponentMobility::Static;
	bCanEverAffectNavigation = true;
	bHasCustomNavigableGeometry = EHasCustomNavigableGeometry::Yes;

	HeightfieldRowsCount = -1;
	HeightfieldColumnsCount = -1;

	// landscape collision components should be deterministically created and therefor are addressable over the network
	SetNetAddressable();
}
