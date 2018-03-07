#include "FlowGridComponent.h"

#include "NvFlowCommon.h"
#include "FlowGridAsset.h"
#include "FlowGridActor.h"
#include "FlowEmitterComponent.h"
#include "FlowMaterial.h"
#include "FlowRenderMaterial.h"

/*=============================================================================
	FlowGridComponent.cpp: UFlowGridComponent methods.
=============================================================================*/

// NvFlow begin
#include "PhysicsEngine/PhysXSupport.h"
#include "Curves/CurveLinearColor.h"
#include "RHIStaticStates.h"
#include "PhysicsPublic.h"
#include "Collision/PhysXCollision.h"

#include "Engine/StaticMesh.h"

#include "StaticMeshResources.h"
#include "DistanceFieldAtlas.h"

// CPU stats, use "stat flow" to enable
DECLARE_CYCLE_STAT(TEXT("Tick Grid Component"), STAT_Flow_Tick, STATGROUP_Flow);
DECLARE_CYCLE_STAT(TEXT("Update Emit and Collide Shapes"), STAT_Flow_UpdateShapes, STATGROUP_Flow);
DECLARE_CYCLE_STAT(TEXT("Update Color Map"), STAT_Flow_UpdateColorMap, STATGROUP_Flow);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Grid Count"), STAT_Flow_GridCount, STATGROUP_Flow);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Emitter Count"), STAT_Flow_EmitterCount, STATGROUP_Flow);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Collider Count"), STAT_Flow_ColliderCount, STATGROUP_Flow);

FFlowTimeStepper::FFlowTimeStepper() 
{
	DeltaTime = 0.f;
	TimeError = 0.f;
	FixedDt = (1.f / 60.f);
	MaxSteps = 1;
	NumSteps = 0;
}

int32 FFlowTimeStepper::GetNumSteps(float TimeStep)
{
	DeltaTime = TimeStep;

	// compute time steps
	TimeError += DeltaTime;

	NumSteps = FPlatformMath::FloorToInt(TimeError / FixedDt);
	check(NumSteps >= 0);

	TimeError -= FixedDt * float(NumSteps);
	if (TimeError < 0.0f) TimeError = 0.0f;

	return FMath::Min(NumSteps, MaxSteps);
}

void UFlowGridComponent::InitializeGridProperties(FFlowGridProperties* FlowGridProperties)
{
	FlowGridProperties->Version = 0ul;
	FlowGridProperties->NumScheduledSubsteps = 1u;

	// set critical property defaults
	FlowGridProperties->bActive = false;
	FlowGridProperties->bMultiAdapterEnabled = false;
	FlowGridProperties->bAsyncComputeEnabled = false;
	FlowGridProperties->bParticlesInteractionEnabled = false;
	FlowGridProperties->bParticleModeEnabled = false;
	FlowGridProperties->SubstepSize = 0.0f;
	FlowGridProperties->VirtualGridExtents = FVector(0.f);
	FlowGridProperties->GridCellSize = 0.f;

	FlowGridProperties->ParticleToGridAccelTimeConstant = 0.01f;
	FlowGridProperties->ParticleToGridDecelTimeConstant = 10.0f;
	FlowGridProperties->ParticleToGridThresholdMultiplier = 2.f;
	FlowGridProperties->GridToParticleAccelTimeConstant = 0.01f;
	FlowGridProperties->GridToParticleDecelTimeConstant = 0.01f;
	FlowGridProperties->GridToParticleThresholdMultiplier = 1.f;

	FlowGridProperties->bDistanceFieldCollisionEnabled = false;
	FlowGridProperties->MinActiveDistance = -1.0f;
	FlowGridProperties->MaxActiveDistance = 0.0f;
	FlowGridProperties->VelocitySlipFactor = 0.0f;
	FlowGridProperties->VelocitySlipThickness = 0.0f;

	// initialize desc/param defaults
	NvFlowGridDescDefaultsInline(&FlowGridProperties->GridDesc);
	NvFlowGridParamsDefaultsInline(&FlowGridProperties->GridParams);

	FlowGridProperties->RenderParams.bGenerateDepth = false;
	FlowGridProperties->RenderParams.DepthAlphaThreshold = 1.f;
	FlowGridProperties->RenderParams.DepthIntensityThreshold = 10.f;
}

UFlowGridComponent::UFlowGridComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FlowGridAssetOverride(nullptr)
	, FlowGridAssetCurrent(&FlowGridAsset)
	, FlowGridAssetOld(nullptr)
{
	{
		FFlowGridProperties* gridProperties = new FFlowGridProperties();

		FlowGridPropertiesPool.Add(gridProperties);

		FlowGridProperties = gridProperties;
	}

	BodyInstance.SetUseAsyncScene(true);

	bFlowGridCollisionEnabled = true;

	static FName CollisionProfileName(TEXT("Flow"));
	SetCollisionProfileName(CollisionProfileName);

	bAlwaysCreatePhysicsState = true;
	bIsActive = true;
	bAutoActivate = true;

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	InitializeGridProperties(FlowGridProperties);

	DefaultFlowMaterial = CreateDefaultSubobject<UFlowMaterial>(TEXT("DefaultFlowMaterial0"));
}

UFlowGridComponent::~UFlowGridComponent()
{	
	for (int32 idx = 0; idx < FlowGridPropertiesPool.Num(); idx++)
	{
		FFlowGridProperties*& prop = FlowGridPropertiesPool[idx];
		if (prop)
		{
			prop->Release();
			prop = nullptr;
		}
	}
}

UFlowGridAsset* UFlowGridComponent::CreateOverrideAsset()
{
	// duplicate asset
	auto asset = DuplicateObject<UFlowGridAsset>(FlowGridAsset, this);
	return asset;
}

void UFlowGridComponent::SetOverrideAsset(class UFlowGridAsset* asset)
{
	FlowGridAssetOverride = asset;
	if(asset) FlowGridAssetCurrent = &FlowGridAssetOverride;
	else FlowGridAssetCurrent = &FlowGridAsset;
}

class UFlowMaterial* UFlowGridComponent::CreateOverrideMaterial(class UFlowMaterial* materialToDuplicate)
{
	// duplicate material
	auto material = DuplicateObject<UFlowMaterial>(materialToDuplicate, this);
	return material;
}

void UFlowGridComponent::SetOverrideMaterial(class UFlowMaterial* materialToOverride, class UFlowMaterial* overrideMaterial)
{
	if (materialToOverride != nullptr)
	{
		MaterialsMap.FindOrAdd(materialToOverride).OverrideMaterial = overrideMaterial;
	}
}


FBoxSphereBounds UFlowGridComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds NewBounds(ForceInit);

	auto& FlowGridAssetRef = (*FlowGridAssetCurrent);
	if (FlowGridAssetRef)
	{
		NewBounds.Origin = FVector(0.0f);
		NewBounds.BoxExtent = FVector(FlowGridAssetRef->GetVirtualGridExtent());
		NewBounds.SphereRadius = 0.0f;
	}

	return NewBounds.TransformBy( LocalToWorld );
}

FPrimitiveSceneProxy* UFlowGridComponent::CreateSceneProxy()
{
	return new FFlowGridSceneProxy(this);
}

namespace
{
	inline void CopyMaterialPerComponent(const FFlowMaterialPerComponent& In, NvFlowGridMaterialPerComponent& Out)
	{
		Out.damping = In.Damping;
		Out.fade = In.Fade;
		Out.macCormackBlendFactor = In.MacCormackBlendFactor;
		Out.macCormackBlendThreshold = In.MacCormackBlendThreshold;
		Out.allocWeight = In.AllocWeight;
		Out.allocThreshold = In.AllocThreshold;
	}

	inline void CopyRenderCompMask(const FFlowRenderCompMask & In, NvFlowFloat4& Out)
	{
		Out.x = In.Temperature;
		Out.y = In.Fuel;
		Out.z = In.Burn;
		Out.w = In.Smoke;
	}

	float ShadowResidentBlocksToScale(int32 ResidentBlocks, EFlowShadowResolution ShadowResolution)
	{
		const int32 ShadowDim = (1 << ShadowResolution);

		const int ShadowBlockDim = 16;
		const int32 ShadowGridDim = (ShadowDim + ShadowBlockDim - 1) / ShadowBlockDim;

		const int32 MaxBlocks = ShadowGridDim * ShadowGridDim * ShadowGridDim;

		return FMath::Min(float(ResidentBlocks) / MaxBlocks, 1.0f);
	}

	inline NvFlowFloat4x4 ConvertToNvFlowFloat4x4(const FMatrix& Mat)
	{
		return NvFlowFloat4x4{
			{Mat.M[0][0], Mat.M[0][1], Mat.M[0][2], Mat.M[0][3]},
			{Mat.M[1][0], Mat.M[1][1], Mat.M[1][2], Mat.M[1][3]},
			{Mat.M[2][0], Mat.M[2][1], Mat.M[2][2], Mat.M[2][3]},
			{Mat.M[3][0], Mat.M[3][1], Mat.M[3][2], Mat.M[3][3]}
		};
	}
}

namespace physx
{
	// helpers to find actor, shape pairs in a TSet
	inline bool operator == (const PxActorShape& lhs, const PxActorShape& rhs) { return lhs.actor == rhs.actor && lhs.shape == rhs.shape; }
	inline uint32 GetTypeHash(const PxActorShape& h) { return ::GetTypeHash((void*)(h.actor)) ^ ::GetTypeHash((void*)(h.shape)); }
}

void UFlowGridComponent::ResetShapes()
{
	//UE_LOG(LogNvFlow, Display, TEXT("NvFlow Reset begin (%d)"), FlowGridProperties->GridEmitParams.Num());

	FlowGridProperties->GridEmitParams.SetNum(0);
	FlowGridProperties->GridCollideParams.SetNum(0);
	FlowGridProperties->GridEmitShapeDescs.SetNum(0);
	FlowGridProperties->GridCollideShapeDescs.SetNum(0);
	FlowGridProperties->GridEmitMaterialKeys.SetNum(0);

	FlowGridProperties->NewDistanceFieldList.SetNum(0);
	FlowGridProperties->DistanceFieldKeys.SetNum(0);
}

// send bodies from synchronous PhysX scene to Flow scene
void UFlowGridComponent::UpdateShapes(float DeltaTime, uint32 NumSimSubSteps)
{
	SCOPE_CYCLE_COUNTER(STAT_Flow_UpdateShapes);

	// only update if enabled
	if (!bFlowGridCollisionEnabled)
	{
		return;
	}

	//UE_LOG(LogNvFlow, Display, TEXT("NvFlow UpdateShapes begin (%d) DeltaTime(%f)"), FlowGridProperties->GridEmitParams.Num(), DeltaTime);

	// used to test if an actor shape pair has already been reported
	TSet<PxActorShape> OverlapSet;

	// buffer for overlaps
	TArray<FOverlapResult> Overlaps;
	TArray<PxShape*> Shapes;

	// get PhysX Scene
	FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();
	PxScene* SyncScene = PhysScene->GetPhysXScene(PST_Sync);

	// lock the scene to perform scene queries
	SCENE_LOCK_READ(SyncScene);

	// create FCollisionShape from flow grid domain
	FBoxSphereBounds ComponentBounds = CalcBounds(GetComponentTransform());
	const FVector Center = ComponentBounds.Origin;
	const FVector HalfEdge = ComponentBounds.BoxExtent;
	FCollisionShape Shape;
	Shape.SetBox(HalfEdge);

	auto& FlowGridAssetRef = (*FlowGridAssetCurrent);

	// do PhysX quer
	auto TraceChannel = FlowGridAssetRef->ObjectType;
	FCollisionQueryParams QueryParams(NAME_None, false);
	FCollisionResponseParams ResponseParams(FlowGridAssetRef->ResponseToChannels);

	Overlaps.Reset();
	GetWorld()->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, TraceChannel, Shape, QueryParams, ResponseParams);

	PxFilterData PFilter = CreateQueryFilterData(TraceChannel, QueryParams.bTraceComplex, ResponseParams.CollisionResponse, QueryParams, FCollisionObjectQueryParams::DefaultObjectQueryParam, true);
	FPxQueryFilterCallback PQueryCallback(QueryParams);

	for (int32 OverlapIdx = 0; OverlapIdx < Overlaps.Num(); ++OverlapIdx)
	{
		const FOverlapResult& hit = Overlaps[OverlapIdx];

		const UPrimitiveComponent* PrimComp = hit.Component.Get();
		if (!PrimComp)
			continue;

		ECollisionResponse Response = PrimComp->GetCollisionResponseToChannel(FlowGridAssetRef->ObjectType);

		// try to grab any attached component
		auto PrimCompOwner = PrimComp->GetOwner();
		auto rootComponent = PrimCompOwner ? PrimCompOwner->GetRootComponent() : nullptr;
		if (rootComponent)
		{
			auto& children = rootComponent->GetAttachChildren();
			for (int32 j = 0; j < children.Num(); j++)
			{
				//OverlapMultiple returns ECollisionResponse::ECR_Overlap types, which we want to ignore
				ECollisionResponse ResponseChild = children[j]->GetCollisionResponseToChannel(FlowGridAssetRef->ObjectType);
				if (ResponseChild != ECollisionResponse::ECR_Ignore)
				{
					Response = ResponseChild;
				}
			}
		}
		
		if (Response == ECollisionResponse::ECR_Ignore)
			continue;

		FBodyInstance* Body = PrimComp->GetBodyInstance();
		if (!Body)
			continue;

		PxRigidActor* PhysXActor = Body->GetPxRigidActor_AssumesLocked();
		if (!PhysXActor)
			continue;

		Shapes.SetNum(0);

		int32 NumSyncShapes;
		NumSyncShapes = Body->GetAllShapes_AssumesLocked(Shapes);

		// get emitter parameters, if available
		AActor* Actor = Body->OwnerComponent->GetOwner();
		UFlowEmitterComponent* FlowEmitterComponent = Actor->FindComponentByClass<UFlowEmitterComponent>();

		// search in attached component actors, as needed
		if (rootComponent && FlowEmitterComponent == nullptr)
		{
			auto& children = rootComponent->GetAttachChildren();
			for (int32 j = 0; j < children.Num(); j++)
			{
				auto owner = children[j]->GetOwner();
				if (owner)
				{
					FlowEmitterComponent = owner->FindComponentByClass<UFlowEmitterComponent>();
					if (FlowEmitterComponent)
					{
						break;
					}
				}
			}
		}

		const UStaticMeshComponent* StaticMeshComponent = nullptr;
		const UStaticMesh* StaticMesh = nullptr;
		const FDistanceFieldVolumeData* DistanceFieldVolumeData = nullptr;
		if (FlowEmitterComponent != nullptr && FlowEmitterComponent->bUseDistanceField)
		{
			StaticMeshComponent = Actor->FindComponentByClass<UStaticMeshComponent>();
			// search in attached component actors, as needed
			if (rootComponent && StaticMeshComponent == nullptr)
			{
				auto& children = rootComponent->GetAttachChildren();
				for (int32 j = 0; j < children.Num(); j++)
				{
					auto owner = children[j]->GetOwner();
					if (owner)
					{
						StaticMeshComponent = owner->FindComponentByClass<UStaticMeshComponent>();
						if (StaticMeshComponent)
						{
							break;
						}
					}
				}
			}

			if (StaticMeshComponent != nullptr)
			{
				StaticMesh = StaticMeshComponent->GetStaticMesh();
			}

			if (StaticMesh != nullptr)
			{
				const FStaticMeshRenderData* RenderData = StaticMesh->RenderData.Get();
				if (RenderData != nullptr && RenderData->LODResources.Num() > 0)
				{
					DistanceFieldVolumeData = RenderData->LODResources[0].DistanceFieldData;
					if (DistanceFieldVolumeData != nullptr && DistanceFieldVolumeData->Size.GetMax() == 0)
					{
						DistanceFieldVolumeData = nullptr;
					}
				}
			}
		}

		if (DistanceFieldVolumeData != nullptr)
		{
			NumSyncShapes = 1;
		}

		bool IsEmitter = FlowEmitterComponent != nullptr;
		bool IsCollider = Response == ECollisionResponse::ECR_Block;

		// optimization: kick out early if couple rate is zero
		if (IsEmitter)
		{
			if (FlowEmitterComponent->CoupleRate <= 0.f)
			{
				IsEmitter = false;
			}
		}

		FTransform ActorTransform;
		if (DistanceFieldVolumeData == nullptr)
		{
			ActorTransform = P2UTransform(PhysXActor->getGlobalPose());
		}
		else
		{
			check(StaticMeshComponent != nullptr);
			ActorTransform = StaticMeshComponent->GetComponentTransform();
		}

		// ActorCenterOfMass, ActorLinearVelocity, ActorAngularVelocity are in Actor's Local Space!!!
		// also assuming ActorCenterOfMass doesn't change in time
		FVector WorldCenterOfMass;
		FVector ActorCenterOfMass;
		if (PhysXActor->is<PxRigidStatic>() == nullptr)
		{
			WorldCenterOfMass = Body->OwnerComponent->GetCenterOfMass();
			ActorCenterOfMass = ActorTransform.InverseTransformPosition(WorldCenterOfMass);
		}
		else
		{
			WorldCenterOfMass = ActorTransform.GetLocation();
			ActorCenterOfMass = FVector::ZeroVector;
		}
		FVector ActorLinearVelocity = ActorTransform.InverseTransformVector(Body->GetUnrealWorldVelocity_AssumesLocked());
		FVector ActorAngularVelocity = ActorTransform.InverseTransformVector(FMath::RadiansToDegrees(Body->GetUnrealWorldAngularVelocityInRadians_AssumesLocked()));

		for (int ShapeIndex = 0; ShapeIndex < NumSyncShapes; ++ShapeIndex)
		{
			PxShape* PhysXShape = nullptr;
			bool IsSupported = true;
			auto PxGeometryType = PxGeometryType::eINVALID;

			if (DistanceFieldVolumeData == nullptr)
			{
				PhysXShape = Shapes[ShapeIndex];

				if (!PhysXActor || !PhysXShape)
					continue;

				PxSceneQueryFlags QueryFlags;
				if (PQueryCallback.preFilter(PFilter, PhysXShape, PhysXActor, QueryFlags) == PxQueryHitType::eNONE)
					continue;

				// check if we've already processed this actor-shape pair
				bool alreadyProcessed = false;
				OverlapSet.Add(PxActorShape(PhysXActor, PhysXShape), &alreadyProcessed);
				if (alreadyProcessed)
					continue;

				PxFilterData Filter = PhysXShape->getQueryFilterData();

				// only process simple collision shapes for now
				if ((Filter.word3 & EPDF_SimpleCollision) == 0)
					continue;

				PxGeometryType = PhysXShape->getGeometryType();
				IsSupported =
					(PxGeometryType == PxGeometryType::eSPHERE) ||
					(PxGeometryType == PxGeometryType::eBOX) ||
					(PxGeometryType == PxGeometryType::eCAPSULE) ||
					(PxGeometryType == PxGeometryType::eCONVEXMESH);
			}

			if (IsSupported && (IsEmitter || IsCollider))
			{
				FTransform ShapeTransform(FTransform::Identity);

				FVector UnitToActualScale(1.f, 1.f, 1.f);
				FVector LocalToWorldScale(1.f, 1.f, 1.f);
				FTransform BoundsTransform(FTransform::Identity);
				NvFlowShapeType FlowShapeType = eNvFlowShapeTypeSDF;
				float FlowShapeDistScale = 1.f;
				uint32 NumShapeDescs = 1u;

				if (DistanceFieldVolumeData == nullptr)
				{
					ShapeTransform = P2UTransform(PhysXShape->getLocalPose());
				}
				else
				{
					UnitToActualScale = FVector(NvFlow::scaleInv);

					auto LocalExtent = DistanceFieldVolumeData->LocalBoundingBox.GetExtent();
					auto LocalCenter = DistanceFieldVolumeData->LocalBoundingBox.GetCenter();

					ShapeTransform.SetLocation(LocalCenter);

					BoundsTransform.SetScale3D(LocalExtent);
					LocalToWorldScale = LocalExtent;

					FlowShapeDistScale = LocalExtent.GetMax() * NvFlow::scaleInv;

					const FDistanceFieldVolumeData* &OldDistanceFieldVolumeData = DistanceFieldMap.FindOrAdd(StaticMesh);
					if (OldDistanceFieldVolumeData != DistanceFieldVolumeData)
					{
						OldDistanceFieldVolumeData = DistanceFieldVolumeData;

						FlowGridProperties->NewDistanceFieldList.AddDefaulted(1);
						FFlowDistanceFieldParams& DistanceFieldParams = FlowGridProperties->NewDistanceFieldList.Last();

						DistanceFieldParams.StaticMesh = StaticMesh;
						DistanceFieldParams.Size = DistanceFieldVolumeData->Size;
						DistanceFieldParams.DistanceMinMax = DistanceFieldVolumeData->DistanceMinMax;
						DistanceFieldParams.CompressedDistanceFieldVolume = DistanceFieldVolumeData->CompressedDistanceFieldVolume;
					}

					FlowGridProperties->DistanceFieldKeys.Add(StaticMesh);
				}

				int32 EmitShapeStartIndex = FlowGridProperties->GridEmitShapeDescs.Num();
				int32 CollideShapeStartIndex = FlowGridProperties->GridCollideShapeDescs.Num();

				bool IsEnabledArray[2] = { IsEmitter, IsCollider };
				TArray<NvFlowShapeDesc>* ShapeDescsArray[2] = { 
					&FlowGridProperties->GridEmitShapeDescs,
					&FlowGridProperties->GridCollideShapeDescs
				};
				for (uint32 passID = 0u; passID < 2u; passID++)
				{
					if (!IsEnabledArray[passID])
					{
						continue;
					}

					TArray<NvFlowShapeDesc>& ShapeDescs = *ShapeDescsArray[passID];

					// compute number of NvFlowShapeDesc
					if (PxGeometryType == PxGeometryType::eCONVEXMESH)
					{
						PxConvexMeshGeometry ConvexGeometry;
						PhysXShape->getConvexMeshGeometry(ConvexGeometry);
						NumShapeDescs = ConvexGeometry.convexMesh->getNbPolygons();
					}

					// allocate shape descs
					int32 Index = ShapeDescs.AddUninitialized(NumShapeDescs);
					NvFlowShapeDesc* ShapeDescsPtr = &ShapeDescs[Index];

					if (PxGeometryType == PxGeometryType::eSPHERE)
					{
						FlowShapeType = eNvFlowShapeTypeSphere;
						PxSphereGeometry SphereGeometry;
						PhysXShape->getSphereGeometry(SphereGeometry);
						ShapeDescsPtr[0].sphere.radius = NvFlow::sdfRadius;
						UnitToActualScale = FVector(SphereGeometry.radius * (1.f / NvFlow::sdfRadius) * NvFlow::scaleInv);
					}
					else if (PxGeometryType == PxGeometryType::eBOX)
					{
						FlowShapeType = eNvFlowShapeTypeBox;
						PxBoxGeometry BoxGeometry;
						PhysXShape->getBoxGeometry(BoxGeometry);
						ShapeDescsPtr[0].box.halfSize.x = NvFlow::sdfRadius;
						ShapeDescsPtr[0].box.halfSize.y = NvFlow::sdfRadius;
						ShapeDescsPtr[0].box.halfSize.z = NvFlow::sdfRadius;
						UnitToActualScale = P2UVector(BoxGeometry.halfExtents) * (1.f / NvFlow::sdfRadius) * NvFlow::scaleInv;
						// distortion correction, makes LocalToWorld uniform scale
						FVector aspectRatio = UnitToActualScale;
						float aspectRatioMin = FMath::Min(aspectRatio.X, FMath::Min(aspectRatio.Y, aspectRatio.Z));
						aspectRatio.X /= aspectRatioMin;
						aspectRatio.Y /= aspectRatioMin;
						aspectRatio.Z /= aspectRatioMin;
						LocalToWorldScale = FVector(1.f) / aspectRatio;
						ShapeDescsPtr[0].box.halfSize.x *= aspectRatio.X;
						ShapeDescsPtr[0].box.halfSize.y *= aspectRatio.Y;
						ShapeDescsPtr[0].box.halfSize.z *= aspectRatio.Z;
					}
					else if (PxGeometryType == PxGeometryType::eCAPSULE)
					{
						FlowShapeType = eNvFlowShapeTypeCapsule;
						PxCapsuleGeometry CapsuleGeometry;
						PhysXShape->getCapsuleGeometry(CapsuleGeometry);
						ShapeDescsPtr[0].capsule.radius = NvFlow::sdfRadius;
						ShapeDescsPtr[0].capsule.length = NvFlow::sdfRadius * (2.f * CapsuleGeometry.halfHeight / CapsuleGeometry.radius);
						UnitToActualScale = FVector(CapsuleGeometry.radius * (1.f / NvFlow::sdfRadius) * NvFlow::scaleInv);

						// extends bounds on x axis
						FVector CapsuleBoundsScale = BoundsTransform.GetScale3D();
						CapsuleBoundsScale.X = (0.5f * ShapeDescsPtr[0].capsule.length + 1.f);
						BoundsTransform.SetScale3D(CapsuleBoundsScale);
					}
					else if (PxGeometryType == PxGeometryType::eCONVEXMESH)
					{
						FlowShapeType = eNvFlowShapeTypePlane;
						PxHullPolygon polygon;
						PxConvexMeshGeometry ConvexGeometry;
						PhysXShape->getConvexMeshGeometry(ConvexGeometry);
						auto localBounds = ConvexGeometry.convexMesh->getLocalBounds();
						auto meshScale = ConvexGeometry.scale;

						for (uint32 i = 0u; i < NumShapeDescs; i++)
						{
							ConvexGeometry.convexMesh->getPolygonData(i, polygon);
							ShapeDescsPtr[i].plane.normal.x = +polygon.mPlane[0];
							ShapeDescsPtr[i].plane.normal.y = +polygon.mPlane[1];
							ShapeDescsPtr[i].plane.normal.z = +polygon.mPlane[2];
							ShapeDescsPtr[i].plane.distance = -polygon.mPlane[3];
						}

						auto LocalMin = P2UVector(localBounds.minimum);
						auto LocalMax = P2UVector(localBounds.maximum);
						auto MeshScale = P2UVector(meshScale.scale);
						auto MeshRotation = P2UQuat(meshScale.rotation);
						const float Radius = (MeshScale * 0.5f * (LocalMax - LocalMin)).GetAbsMax();
						UnitToActualScale = FVector(Radius * (1.f / NvFlow::sdfRadius) * NvFlow::scaleInv);

						FlowShapeDistScale = NvFlow::scaleInv;

						// scale bounds
						FVector BoundsHalfSize = (MeshScale * 0.5f * (LocalMax - LocalMin));
						FVector BoundsOffset = MeshScale * 0.5f * (LocalMin + LocalMax);
						BoundsHalfSize *= (1.f / Radius);				// normalize against radius
						BoundsOffset *= (NvFlow::sdfRadius / Radius);	// normalize against radius, cancel out sdfRadius scale
						BoundsTransform.SetScale3D(BoundsHalfSize);
						BoundsTransform.SetLocation(BoundsOffset);

						// scale local to world, scaleInv cancels out because planes are in UE4 space
						LocalToWorldScale = MeshScale * NvFlow::scaleInv / UnitToActualScale;
					}
					else
					{
						//DistanceField
						check(FlowGridProperties->DistanceFieldKeys.Num() > 0);

						ShapeDescsPtr[0].sdf.sdfOffset = FlowGridProperties->DistanceFieldKeys.Num() - 1;
					}
				}

				if (IsEmitter)
				{
					// substep invariant params
					NvFlowGridEmitParams emitParams;
					NvFlowGridEmitParamsDefaultsInline(&emitParams);

					// emit values
					emitParams.fuel = FlowEmitterComponent->Fuel;
					emitParams.fuelReleaseTemp = FlowEmitterComponent->FuelReleaseTemp;
					emitParams.fuelRelease = FlowEmitterComponent->FuelRelease;
					emitParams.smoke = FlowEmitterComponent->Smoke;
					emitParams.temperature = FlowEmitterComponent->Temperature;
					emitParams.allocationPredict = FlowEmitterComponent->AllocationPredict;
					emitParams.allocationScale = {
						FlowEmitterComponent->AllocationScale,
						FlowEmitterComponent->AllocationScale,
						FlowEmitterComponent->AllocationScale
					};

					// alloc shape only mode
					if (FlowEmitterComponent->bAllocShapeOnly)
					{
						emitParams.emitMode = eNvFlowGridEmitModeAllocShapeOnly;
					}

					// couple rates
					float coupleRate = FlowEmitterComponent->CoupleRate;
					emitParams.fuelCoupleRate        = coupleRate * FlowEmitterComponent->FuelMask;
					emitParams.temperatureCoupleRate = coupleRate * FlowEmitterComponent->TemperatureMask;
					emitParams.smokeCoupleRate       = coupleRate * FlowEmitterComponent->SmokeMask;
					float velocityCoupleRate         = coupleRate * FlowEmitterComponent->VelocityMask;
					emitParams.velocityCoupleRate = { velocityCoupleRate, velocityCoupleRate, velocityCoupleRate };

					// max/min active dist
					float CollisionFactor = FlowEmitterComponent->CollisionFactor;
					float EmitterInflate = FlowEmitterComponent->EmitterInflate;
					emitParams.maxActiveDist = EmitterInflate;
					emitParams.minActiveDist = -1.f + CollisionFactor;

					// set shape type, shape base and range, distance scale
					emitParams.shapeType = FlowShapeType;
					emitParams.shapeRangeOffset = EmitShapeStartIndex;
					emitParams.shapeRangeSize = NumShapeDescs;
					emitParams.shapeDistScale = FlowShapeDistScale;

					// substep
					float NumSubsteps = FlowEmitterComponent->NumSubsteps;
					float EmitSubstepDt = FlowGridProperties->SubstepSize / NumSubsteps;

					emitParams.deltaTime = EmitSubstepDt;

					FTransform PreviousTransform = FlowEmitterComponent->PreviousTransform;
					FVector PreviousLinearVelocity = FlowEmitterComponent->PreviousLinearVelocity;
					FVector PreviousAngularVelocity = FlowEmitterComponent->PreviousAngularVelocity;
					if (FlowEmitterComponent->bPreviousStateInitialized == false)
					{
						PreviousTransform = ActorTransform;
						PreviousLinearVelocity = ActorLinearVelocity;
						PreviousAngularVelocity = ActorAngularVelocity;

						FlowEmitterComponent->bPreviousStateInitialized = true;
					}
					// Update Previous Transform
					FlowEmitterComponent->PreviousTransform = ActorTransform;
					FlowEmitterComponent->PreviousLinearVelocity = PreviousLinearVelocity;
					FlowEmitterComponent->PreviousAngularVelocity = PreviousAngularVelocity;

					float EmitTimerStepperError = 0.f;
					if (FlowEmitterComponent->NumSubsteps == 1u)
					{
						NumSubsteps = NumSimSubSteps;
					}
					else
					{
						auto& EmitTimerStepper = FlowEmitterComponent->EmitTimeStepper;

						EmitTimerStepper.FixedDt = EmitSubstepDt;
						EmitTimerStepper.MaxSteps = 64u;	// TODO: Maybe expose

						NumSubsteps = EmitTimerStepper.GetNumSteps(DeltaTime);

						EmitTimerStepperError = EmitTimerStepper.TimeError;
					}

					for (int32 SubStepIdx = 0; SubStepIdx < NumSubsteps; SubStepIdx++)
					{
						FTransform BlendedActorTransform = ActorTransform;
						FVector BlendedActorLinearVelocity = ActorLinearVelocity;
						FVector BlendedActorAngularVelocity = ActorAngularVelocity;

						// interpolate as needed
						if (FlowEmitterComponent->NumSubsteps > 1u)
						{
							int32 Substep_i = NumSubsteps - 1 - SubStepIdx;

							float Substep_t = EmitSubstepDt * Substep_i + EmitTimerStepperError;

							const float TimeNew = 0.f;
							const float TimeOld = DeltaTime;

							float Alpha = (Substep_t - TimeNew) / (TimeOld - TimeNew);

							BlendedActorTransform.Blend(ActorTransform, PreviousTransform, Alpha);
							BlendedActorLinearVelocity = FMath::Lerp(ActorLinearVelocity, PreviousLinearVelocity, Alpha);
							BlendedActorAngularVelocity = FMath::Lerp(ActorAngularVelocity, PreviousAngularVelocity, Alpha);
						}

						// physics
						FVector CollisionLinearVelocity = ShapeTransform.InverseTransformVector(BlendedActorLinearVelocity);
						FVector CollisionAngularVelocity = ShapeTransform.InverseTransformVector(BlendedActorAngularVelocity);
						FVector CollisionCenterOfRotationOffset = BlendedActorTransform.TransformPosition(ActorCenterOfMass);

						FVector CollisionScaledVelocityLinear = CollisionLinearVelocity * NvFlow::scaleInv;
						FVector CollisionScaledVelocityAngular = CollisionAngularVelocity * NvFlow::angularScale;
						FVector CollisionScaledCenterOfMass = CollisionCenterOfRotationOffset * NvFlow::scaleInv;

						FVector LinearVelocity = FlowEmitterComponent->LinearVelocity + CollisionLinearVelocity * FlowEmitterComponent->BlendInPhysicalVelocity;
						FVector AngularVelocity = FlowEmitterComponent->AngularVelocity + CollisionAngularVelocity * FlowEmitterComponent->BlendInPhysicalVelocity;

						FVector ScaledVelocityLinear = LinearVelocity * NvFlow::scaleInv;
						FVector ScaledVelocityAngular = AngularVelocity * NvFlow::angularScale;

						emitParams.velocityLinear = *(NvFlowFloat3*)(&ScaledVelocityLinear.X);
						emitParams.velocityAngular = *(NvFlowFloat3*)(&ScaledVelocityAngular.X);

						// scaled transform
						FTransform ScaledTransform = ShapeTransform * BlendedActorTransform;
						ScaledTransform.SetLocation(ScaledTransform.GetLocation() * NvFlow::scaleInv);
						ScaledTransform.SetScale3D(ScaledTransform.GetScale3D() * UnitToActualScale);

						// establish bounds and localToWorld
						FTransform BlendedBounds = ScaledTransform;
						FTransform BlendedLocalToWorld = ScaledTransform;
						BlendedBounds = BoundsTransform * BlendedBounds;
						BlendedLocalToWorld.SetScale3D(BlendedLocalToWorld.GetScale3D() * LocalToWorldScale);

						// scale bounds as a function of emitter inflate
						{
							const float k = (EmitterInflate + 1.f);
							BlendedBounds.SetScale3D(BlendedBounds.GetScale3D() * k);
						}

						// compute centerOfMass in bounds local space
						FVector centerOfMass = BlendedBounds.InverseTransformPosition(CollisionScaledCenterOfMass);
						emitParams.centerOfMass = *(NvFlowFloat3*)(&centerOfMass.X);

						emitParams.bounds = ConvertToNvFlowFloat4x4(BlendedBounds.ToMatrixWithScale());
						emitParams.localToWorld = ConvertToNvFlowFloat4x4(BlendedLocalToWorld.ToMatrixWithScale());

						// push parameters
						FlowGridProperties->GridEmitParams.Push(emitParams);

						// add material
						auto EmitterFlowMaterial = FlowEmitterComponent->FlowMaterial;
						FlowGridProperties->GridEmitMaterialKeys.Push(
							AddMaterialParams(EmitterFlowMaterial != nullptr ? EmitterFlowMaterial : DefaultFlowMaterial)
						);

						// collision factor support
						if (CollisionFactor > 0.f)
						{
							NvFlowGridEmitParams collideParams = emitParams;
							collideParams.allocationScale = { 0.f, 0.f, 0.f };

							collideParams.slipFactor = 0.9f;
							collideParams.slipThickness = 0.1f;

							collideParams.velocityLinear = *(NvFlowFloat3*)(&CollisionScaledVelocityLinear.X);
							collideParams.velocityAngular = *(NvFlowFloat3*)(&CollisionScaledVelocityAngular.X);
							float collideVelCR = 100.f * FlowEmitterComponent->VelocityMask;
							collideParams.velocityCoupleRate = { collideVelCR, collideVelCR, collideVelCR };

							collideParams.fuel = 0.f;
							collideParams.fuelCoupleRate = 100.f * FlowEmitterComponent->FuelMask;

							collideParams.smoke = 0.f;
							collideParams.smokeCoupleRate = 100.f * FlowEmitterComponent->SmokeMask;

							collideParams.temperature = 0.f;
							collideParams.temperatureCoupleRate = 100.0f * FlowEmitterComponent->TemperatureMask;

							collideParams.maxActiveDist = -1.f + CollisionFactor - collideParams.slipThickness;
							collideParams.minActiveDist = -1.f;

							FlowGridProperties->GridEmitParams.Push(collideParams);

							FlowMaterialKeyType LastMaterialKey = FlowGridProperties->GridEmitMaterialKeys.Last();
							FlowGridProperties->GridEmitMaterialKeys.Push(LastMaterialKey);
						}
					}
				}

				if (IsCollider && NumSimSubSteps > 0u)
				{
					// physics
					FVector CollisionLinearVelocity = ShapeTransform.InverseTransformVector(ActorLinearVelocity);
					FVector CollisionAngularVelocity = ShapeTransform.InverseTransformVector(ActorAngularVelocity);
					FVector CollisionCenterOfRotationOffset = WorldCenterOfMass;

					FVector CollisionScaledVelocityLinear = CollisionLinearVelocity * NvFlow::scaleInv;
					FVector CollisionScaledVelocityAngular = CollisionAngularVelocity * NvFlow::angularScale;
					FVector CollisionScaledCenterOfMass = CollisionCenterOfRotationOffset * NvFlow::scaleInv;

					// parameters
					NvFlowGridEmitParams emitParams;
					NvFlowGridEmitParamsDefaultsInline(&emitParams);

					// emit values
					emitParams.velocityLinear = *(NvFlowFloat3*)(&CollisionScaledVelocityLinear.X);
					emitParams.velocityAngular = *(NvFlowFloat3*)(&CollisionScaledVelocityAngular.X);
					emitParams.fuel = 0.f;
					emitParams.smoke = 0.f;
					emitParams.temperature = 0.f;
					emitParams.allocationScale = { 0.f, 0.f, 0.f };

					// couple rates
					float coupleRate = 100.f;
					emitParams.fuelCoupleRate = coupleRate;
					emitParams.temperatureCoupleRate = coupleRate;
					emitParams.smokeCoupleRate = coupleRate;
					float velocityCoupleRate = coupleRate;
					emitParams.velocityCoupleRate = { velocityCoupleRate, velocityCoupleRate, velocityCoupleRate };

					// set shape type, shape base and range, distance scale
					emitParams.shapeType = FlowShapeType;
					emitParams.shapeRangeOffset = CollideShapeStartIndex;
					emitParams.shapeRangeSize = NumShapeDescs;
					emitParams.shapeDistScale = FlowShapeDistScale;

					// scaled transform
					FTransform ScaledTransform = ShapeTransform * ActorTransform;
					ScaledTransform.SetLocation(ScaledTransform.GetLocation() * NvFlow::scaleInv);
					ScaledTransform.SetScale3D(ScaledTransform.GetScale3D() * UnitToActualScale);

					// establish bounds and localToWorld
					FTransform BlendedBounds = ScaledTransform;
					FTransform BlendedLocalToWorld = ScaledTransform;
					BlendedBounds = BoundsTransform * BlendedBounds;
					BlendedLocalToWorld.SetScale3D(BlendedLocalToWorld.GetScale3D() * LocalToWorldScale);

					// compute centerOfMass in bounds local space
					FVector centerOfMass = BlendedBounds.InverseTransformPosition(CollisionScaledCenterOfMass);
					emitParams.centerOfMass = *(NvFlowFloat3*)(&centerOfMass.X);

					emitParams.bounds = ConvertToNvFlowFloat4x4(BlendedBounds.ToMatrixWithScale());
					emitParams.localToWorld = ConvertToNvFlowFloat4x4(BlendedLocalToWorld.ToMatrixWithScale());

					// step size
					emitParams.deltaTime = FlowGridProperties->SubstepSize;

					// push parameters
					FlowGridProperties->GridCollideParams.Push(emitParams);
				}
			}
		}
	}

	SCENE_UNLOCK_READ(SyncScene);
}

FlowMaterialKeyType UFlowGridComponent::AddMaterialParams(UFlowMaterial* InFlowMaterial)
{
	check(InFlowMaterial != nullptr);

	MaterialData& MaterialData = MaterialsMap.FindOrAdd(InFlowMaterial);

	UFlowMaterial* FlowMaterial = (MaterialData.OverrideMaterial != nullptr) ? MaterialData.OverrideMaterial : InFlowMaterial;
	FlowMaterialKeyType FlowMaterialKey = InFlowMaterial; //OverrideMaterial change only parameters without adding new material

	if (MaterialData.bUpdated)
	{
		return FlowMaterialKey;
	}
	MaterialData.bUpdated = true;

	FlowGridProperties->Materials.AddDefaulted(1);
	FlowGridProperties->Materials.Last().Key = FlowMaterialKey;
	FFlowMaterialParams& MaterialParams = FlowGridProperties->Materials.Last().Value;

	NvFlowGridMaterialParamsDefaultsInline(&MaterialParams.GridParams);

	//Grid part
	CopyMaterialPerComponent(FlowMaterial->Velocity, MaterialParams.GridParams.velocity);
	CopyMaterialPerComponent(FlowMaterial->Smoke, MaterialParams.GridParams.smoke);
	CopyMaterialPerComponent(FlowMaterial->Temperature, MaterialParams.GridParams.temperature);
	CopyMaterialPerComponent(FlowMaterial->Fuel, MaterialParams.GridParams.fuel);

	MaterialParams.GridParams.vorticityStrength = FlowMaterial->VorticityStrength;
	MaterialParams.GridParams.vorticityVelocityMask = FlowMaterial->VorticityVelocityMask;
	MaterialParams.GridParams.vorticityTemperatureMask = FlowMaterial->VorticityTemperatureMask;
	MaterialParams.GridParams.vorticitySmokeMask = FlowMaterial->VorticitySmokeMask;
	MaterialParams.GridParams.vorticityFuelMask = FlowMaterial->VorticityFuelMask;
	MaterialParams.GridParams.vorticityConstantMask = FlowMaterial->VorticityConstantMask;
	MaterialParams.GridParams.ignitionTemp = FlowMaterial->IgnitionTemp;
	MaterialParams.GridParams.burnPerTemp = FlowMaterial->BurnPerTemp;
	MaterialParams.GridParams.fuelPerBurn = FlowMaterial->FuelPerBurn;
	MaterialParams.GridParams.tempPerBurn = FlowMaterial->TempPerBurn;
	MaterialParams.GridParams.smokePerBurn = FlowMaterial->SmokePerBurn;
	MaterialParams.GridParams.divergencePerBurn = FlowMaterial->DivergencePerBurn;
	MaterialParams.GridParams.buoyancyPerTemp = FlowMaterial->BuoyancyPerTemp;
	MaterialParams.GridParams.coolingRate = FlowMaterial->CoolingRate;

	//Render part
	check(FlowMaterial->RenderMaterials.Num() > 0);
	MaterialParams.RenderMaterials.Reset();
	MaterialParams.RenderMaterials.Reserve(FlowMaterial->RenderMaterials.Num());
	for (auto it = FlowMaterial->RenderMaterials.CreateIterator(); it; ++it)
	{
		UFlowRenderMaterial* RenderMaterial = *it;
		if (RenderMaterial == nullptr)
		{
			continue;
		}

		MaterialParams.RenderMaterials.AddDefaulted(1);
		FFlowRenderMaterialParams& RenderMaterialParams = MaterialParams.RenderMaterials.Last();

		RenderMaterialParams.Key = RenderMaterial;

		RenderMaterialParams.alphaScale = RenderMaterial->AlphaScale;
		RenderMaterialParams.additiveFactor = RenderMaterial->AdditiveFactor;

		CopyRenderCompMask(RenderMaterial->ColorMapCompMask, RenderMaterialParams.colorMapCompMask);
		CopyRenderCompMask(RenderMaterial->AlphaCompMask, RenderMaterialParams.alphaCompMask);
		CopyRenderCompMask(RenderMaterial->IntensityCompMask, RenderMaterialParams.intensityCompMask);

		RenderMaterialParams.alphaBias = RenderMaterial->AlphaBias;
		RenderMaterialParams.intensityBias = RenderMaterial->IntensityBias;

		SCOPE_CYCLE_COUNTER(STAT_Flow_UpdateColorMap);

		//Alloc color map size to default specified by the flow library. NvFlowRendering.cpp assumes that for now.
		if (RenderMaterialParams.ColorMap.Num() == 0)
		{
			RenderMaterialParams.ColorMap.SetNum(64);
		}

		float xmin = RenderMaterial->ColorMapMinX;
		float xmax = RenderMaterial->ColorMapMaxX;
		RenderMaterialParams.colorMapMinX = xmin;
		RenderMaterialParams.colorMapMaxX = xmax;

		for (int32 i = 0; i < RenderMaterialParams.ColorMap.Num(); i++)
		{
			float t = float(i) / (RenderMaterialParams.ColorMap.Num() - 1);

			float s = (xmax - xmin) * t + xmin;

			RenderMaterialParams.ColorMap[i] = RenderMaterial->ColorMap ? RenderMaterial->ColorMap->GetLinearColorValue(s) : FLinearColor(0.f, 0.f, 0.f, 1.f);
		}
	}

	return FlowMaterialKey;
}


void UFlowGridComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	SCOPE_CYCLE_COUNTER(STAT_Flow_Tick);

	//UE_LOG(LogNvFlow, Display, TEXT("NvFlow TickComponent DeltaTime(%f)"), DeltaTime);

	auto& FlowGridAssetRef = (*FlowGridAssetCurrent);

	if (FlowGridAssetRef && SceneProxy)
	{
		// derive parameters from asset
		FVector CurrentHalfSize = NvFlow::scaleInv * FVector(FlowGridAssetRef->GetVirtualGridDimension() * 0.5f * FlowGridAssetRef->GridCellSize);
		FIntVector CurrentVirtualDim = FIntVector(FlowGridAssetRef->GetVirtualGridDimension());

		// grab default desc
		NvFlowGridDesc defaultGridDesc = {};
		NvFlowGridDescDefaultsInline(&defaultGridDesc);

		//NvFlowGridDesc
		NvFlowGridDesc newGridDesc = FlowGridProperties->GridDesc;
		newGridDesc.halfSize = { CurrentHalfSize.X, CurrentHalfSize.Y, CurrentHalfSize.Z };
		newGridDesc.virtualDim = { uint32(CurrentVirtualDim.X), uint32(CurrentVirtualDim.Y), uint32(CurrentVirtualDim.Z) };
		newGridDesc.densityMultiRes = FlowGridAssetRef->bParticleModeEnabled ? eNvFlowMultiRes1x1x1 : eNvFlowMultiRes2x2x2;
		newGridDesc.residentScale = defaultGridDesc.residentScale * FlowGridAssetRef->MemoryLimitScale;
		newGridDesc.lowLatencyMapping = FlowGridAssetRef->bLowLatencyMapping;

		bool changed = (
			newGridDesc.virtualDim.x != FlowGridProperties->GridDesc.virtualDim.x ||
			newGridDesc.virtualDim.y != FlowGridProperties->GridDesc.virtualDim.y ||
			newGridDesc.virtualDim.z != FlowGridProperties->GridDesc.virtualDim.z ||
			newGridDesc.densityMultiRes != FlowGridProperties->GridDesc.densityMultiRes ||
			newGridDesc.residentScale != FlowGridProperties->GridDesc.residentScale ||
			newGridDesc.lowLatencyMapping != FlowGridProperties->GridDesc.lowLatencyMapping ||
			FlowGridAssetRef->bMultiAdapterEnabled != FlowGridProperties->bMultiAdapterEnabled ||
			FlowGridAssetRef->bAsyncComputeEnabled != FlowGridProperties->bAsyncComputeEnabled ||
			FlowGridAssetRef->bParticleModeEnabled != FlowGridProperties->bParticleModeEnabled ||
			FlowGridAssetRef->ColorMapResolution   != FlowGridProperties->ColorMapResolution);

		if (changed || (FlowGridAssetOld != FlowGridAssetRef))
		{
			// make sure transform is good
			UpdateBounds();
			MarkRenderTransformDirty();

			FlowGridAssetOld = FlowGridAssetRef;
		}

		if (FlowGridProperties->bActive && changed)
		{
			// rebuild required
			FlowGridProperties->bActive = false;

			VersionCounter++;
			FlowGridProperties->Version = VersionCounter;

			MarkRenderDynamicDataDirty();
			return;
		}

		// Commit any changes
		FlowGridProperties->GridDesc = newGridDesc;
		FlowGridProperties->bMultiAdapterEnabled = FlowGridAssetRef->bMultiAdapterEnabled;
		FlowGridProperties->bAsyncComputeEnabled = FlowGridAssetRef->bAsyncComputeEnabled;
		FlowGridProperties->bParticlesInteractionEnabled = FlowGridAssetRef->bParticlesInteractionEnabled;
		FlowGridProperties->InteractionChannel = FlowGridAssetRef->InteractionChannel;
		FlowGridProperties->ResponseToInteractionChannels = FlowGridAssetRef->ResponseToInteractionChannels;
		FlowGridProperties->bParticleModeEnabled = FlowGridAssetRef->bParticleModeEnabled;

		FlowGridProperties->ParticleToGridAccelTimeConstant = FlowGridAssetRef->ParticleToGridAccelTimeConstant;
		FlowGridProperties->ParticleToGridDecelTimeConstant = FlowGridAssetRef->ParticleToGridDecelTimeConstant;
		FlowGridProperties->ParticleToGridThresholdMultiplier = FlowGridAssetRef->ParticleToGridThresholdMultiplier;
		FlowGridProperties->GridToParticleAccelTimeConstant = FlowGridAssetRef->GridToParticleAccelTimeConstant;
		FlowGridProperties->GridToParticleDecelTimeConstant = FlowGridAssetRef->GridToParticleDecelTimeConstant;
		FlowGridProperties->GridToParticleThresholdMultiplier = FlowGridAssetRef->GridToParticleThresholdMultiplier;

		FlowGridProperties->bDistanceFieldCollisionEnabled = FlowGridAssetRef->bDistanceFieldCollisionEnabled;
		FlowGridProperties->MinActiveDistance = FlowGridAssetRef->MinActiveDistance;
		FlowGridProperties->MaxActiveDistance = FlowGridAssetRef->MaxActiveDistance;
		FlowGridProperties->VelocitySlipFactor = FlowGridAssetRef->VelocitySlipFactor;
		FlowGridProperties->VelocitySlipThickness = FlowGridAssetRef->VelocitySlipThickness;


		//Properties that can be changed without rebuilding grid
		FlowGridProperties->VirtualGridExtents = FVector(FlowGridAssetRef->GetVirtualGridExtent());
		FlowGridProperties->GridCellSize = FlowGridAssetRef->GridCellSize;

		//NvFlowGridParams
		auto& GridParams = FlowGridProperties->GridParams;
		NvFlowGridParamsDefaultsInline(&GridParams);

		FVector ScaledGravity(FlowGridAssetRef->Gravity * NvFlow::scaleInv);
		GridParams.gravity = *(NvFlowFloat3*)(&ScaledGravity);
		GridParams.singlePassAdvection = FlowGridAssetRef->bSinglePassAdvection;
		GridParams.pressureLegacyMode = FlowGridAssetRef->bPressureLegacyMode;
		GridParams.bigEffectMode = FlowGridAssetRef->bBigEffectMode;

		FlowGridProperties->ColorMapResolution = FlowGridAssetRef->ColorMapResolution;

		//NvFlowVolumeRenderParams
		if (UFlowGridAsset::sGlobalDebugDraw)
		{
			FlowGridProperties->RenderParams.RenderMode = (NvFlowVolumeRenderMode)UFlowGridAsset::sGlobalRenderMode;
			FlowGridProperties->RenderParams.RenderChannel = (NvFlowGridTextureChannel)UFlowGridAsset::sGlobalRenderChannel;
		}
		else
		{
			FlowGridProperties->RenderParams.RenderMode = (NvFlowVolumeRenderMode)FlowGridAssetRef->RenderMode.GetValue();
			FlowGridProperties->RenderParams.RenderChannel = (NvFlowGridTextureChannel)FlowGridAssetRef->RenderChannel.GetValue();
		}
		FlowGridProperties->RenderParams.bAdaptiveScreenPercentage = FlowGridAssetRef->bAdaptiveScreenPercentage;
		FlowGridProperties->RenderParams.AdaptiveTargetFrameTime = FlowGridAssetRef->AdaptiveTargetFrameTime;
		FlowGridProperties->RenderParams.MaxScreenPercentage = FlowGridAssetRef->MaxScreenPercentage;
		FlowGridProperties->RenderParams.MinScreenPercentage = FlowGridAssetRef->MinScreenPercentage;
		
		if (UFlowGridAsset::sGlobalDebugDraw)
		{
			GridParams.debugVisFlags = NvFlowGridDebugVisFlags(UFlowGridAsset::sGlobalMode);
			FlowGridProperties->RenderParams.bDebugWireframe = true;
		}
		else
		{
			GridParams.debugVisFlags = eNvFlowGridDebugVisDisabled;
			FlowGridProperties->RenderParams.bDebugWireframe = FlowGridAssetRef->bDebugWireframe;
		}
		
		FlowGridProperties->RenderParams.bGenerateDepth = FlowGridAssetRef->bGenerateDepth;
		FlowGridProperties->RenderParams.DepthAlphaThreshold = FlowGridAssetRef->DepthAlphaThreshold;
		FlowGridProperties->RenderParams.DepthIntensityThreshold = FlowGridAssetRef->DepthIntensityThreshold;

		FlowGridProperties->RenderParams.bVolumeShadowEnabled = FlowGridAssetRef->bVolumeShadowEnabled;
		FlowGridProperties->RenderParams.ShadowIntensityScale = FlowGridAssetRef->ShadowIntensityScale;
		FlowGridProperties->RenderParams.ShadowMinIntensity = FlowGridAssetRef->ShadowMinIntensity;
		CopyRenderCompMask(FlowGridAssetRef->ShadowBlendCompMask, FlowGridProperties->RenderParams.ShadowBlendCompMask);
		FlowGridProperties->RenderParams.ShadowBlendBias = FlowGridAssetRef->ShadowBlendBias;

		FlowGridProperties->RenderParams.ShadowResolution = 1u << FlowGridAssetRef->ShadowResolution;
		FlowGridProperties->RenderParams.ShadowFrustrumScale = FlowGridAssetRef->ShadowFrustrumScale;
		FlowGridProperties->RenderParams.ShadowMinResidentScale = ShadowResidentBlocksToScale(FlowGridAssetRef->ShadowMinResidentBlocks, FlowGridAssetRef->ShadowResolution);
		FlowGridProperties->RenderParams.ShadowMaxResidentScale = ShadowResidentBlocksToScale(FlowGridAssetRef->ShadowMaxResidentBlocks, FlowGridAssetRef->ShadowResolution);

		FlowGridProperties->RenderParams.ShadowChannel = FlowGridAssetRef->ShadowChannel;
		FlowGridProperties->RenderParams.ShadowNearDistance = FlowGridAssetRef->ShadowNearDistance;

		for (auto It = MaterialsMap.CreateIterator(); It; ++It)
		{
			It.Value().bUpdated = false;
		}
		FlowGridProperties->Materials.Reset();
		FlowGridProperties->DefaultMaterialKey = AddMaterialParams(DefaultFlowMaterial);


		TimeStepper.FixedDt = 1.f / FlowGridAssetRef->SimulationRate;
		FlowGridProperties->SubstepSize = TimeStepper.FixedDt;

		//trigger simulation substeps in render thread
		int32 NumSubSteps = TimeStepper.GetNumSteps(DeltaTime);

		//UE_LOG(LogNvFlow, Display, TEXT("NvFlow TickComponent DeltaTime(%f), NumSubSteps(%d)"), DeltaTime, NumSubSteps);

		//EmitShapes & CollisionShapes
		UpdateShapes(DeltaTime, uint32(NumSubSteps));

		//set active, since we are ticking
		FlowGridProperties->bActive = true;

		if (NumSubSteps > 0)
		{
			VersionCounter++;
			FlowGridProperties->Version = VersionCounter;
		}

		//push all flow properties to proxy
		MarkRenderDynamicDataDirty();
	}
}

void UFlowGridComponent::OnCreatePhysicsState()
{
	UActorComponent::OnCreatePhysicsState();
}

void UFlowGridComponent::OnDestroyPhysicsState()
{
	UActorComponent::OnDestroyPhysicsState();
}

#if WITH_EDITOR	
void UFlowGridComponent::OnRegister()
{
	Super::OnRegister();
}

void UFlowGridComponent::OnUnregister()
{
	Super::OnUnregister();
}
#endif

void UFlowGridComponent::BeginPlay()
{
	Super::BeginPlay();
	INC_DWORD_STAT(STAT_Flow_GridCount);
}

void UFlowGridComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{	
	DEC_DWORD_STAT_BY(STAT_Flow_EmitterCount, GridEmitParams_Num_Old);
	DEC_DWORD_STAT_BY(STAT_Flow_ColliderCount, GridCollideParams_Num_Old);

	GridEmitParams_Num_Old = 0;
	GridCollideParams_Num_Old = 0;

	DEC_DWORD_STAT(STAT_Flow_GridCount);
	Super::EndPlay(EndPlayReason);
}

void UFlowGridComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport/* = ETeleportType::None*/)
{
	// We are handling the physics move below, so don't handle it at higher levels
	Super::OnUpdateTransform(EUpdateTransformFlags::SkipPhysicsUpdate, Teleport);

	/* Disabled, no longer needed with grid translation
	// Reset simulation - will get turned on with Tick again
	FlowGridProperties->bActive = false;

	VersionCounter++;
	FlowGridProperties->Version = VersionCounter;

	MarkRenderDynamicDataDirty();
	*/
}

void UFlowGridComponent::SendRenderDynamicData_Concurrent()
{
	Super::SendRenderDynamicData_Concurrent();
	if (SceneProxy)
	{
		if (FlowGridProperties->Version > LastVersionPushed)
		{
			LastVersionPushed = FlowGridProperties->Version;

			// Update emitter stat
			{
				DEC_DWORD_STAT_BY(STAT_Flow_EmitterCount, GridEmitParams_Num_Old);
				DEC_DWORD_STAT_BY(STAT_Flow_ColliderCount, GridCollideParams_Num_Old);

				GridEmitParams_Num_Old = FlowGridProperties->GridEmitParams.Num();
				GridCollideParams_Num_Old = FlowGridProperties->GridCollideParams.Num();

				INC_DWORD_STAT_BY(STAT_Flow_EmitterCount, GridEmitParams_Num_Old);
				INC_DWORD_STAT_BY(STAT_Flow_ColliderCount, GridCollideParams_Num_Old);
			}

			// Enqueue command to send to render thread
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				FSendFlowGridDynamicData,
				FFlowGridSceneProxy*, FlowGridSceneProxy, (FFlowGridSceneProxy*)SceneProxy,
				FFlowGridPropertiesRef, FlowGridPropertiesRef, FFlowGridPropertiesRef(FlowGridProperties),
				{
					FlowGridSceneProxy->SetDynamicData_RenderThread(FlowGridPropertiesRef.Ref);
				});

			// switch to new FlowGridProperties version
			int32 idx = 0;
			for (; idx < FlowGridPropertiesPool.Num(); idx++)
			{
				FFlowGridProperties* prop = FlowGridPropertiesPool[idx];
				if (prop && prop->RefCount == 1)
				{
					FlowGridProperties = prop;
					break;
				}
			}
			if (idx == FlowGridPropertiesPool.Num())
			{
				FFlowGridProperties* gridProperties = new FFlowGridProperties();

				FlowGridPropertiesPool.Add(gridProperties);

				FlowGridProperties = gridProperties;

				InitializeGridProperties(FlowGridProperties);
			}

			// Reset shape accumulation
			ResetShapes();
		}
	}
}

void UFlowGridComponent::SendRenderTransform_Concurrent()
{
	Super::SendRenderTransform_Concurrent();
}

void UFlowGridComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	auto This = CastChecked<UFlowGridComponent>(InThis);

	for (auto It = This->MaterialsMap.CreateIterator(); It; ++It)
	{
		auto OverrideMaterial = It.Value().OverrideMaterial;
		if (OverrideMaterial != nullptr)
		{
			Collector.AddReferencedObject(OverrideMaterial, This);
		}
	}
}

/*=============================================================================
FFlowGridSceneProxy
=============================================================================*/

FFlowGridSceneProxy::FFlowGridSceneProxy(UFlowGridComponent* Component)
	: FPrimitiveSceneProxy(Component)
	, FlowGridProperties(Component->FlowGridProperties)
	, scenePtr(nullptr)
	, cleanupSceneFunc(nullptr)
{
	FlowGridProperties->AddRef();

	FlowData.bFlowGrid = true;
}

FFlowGridSceneProxy::~FFlowGridSceneProxy()
{
	if (FlowGridProperties)
	{
		FlowGridProperties->Release();
		FlowGridProperties = nullptr;
	}

	if (scenePtr != nullptr)
	{
		check(cleanupSceneFunc != nullptr);
		cleanupSceneFunc(scenePtr);
	}
}


void FFlowGridSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	const FMatrix& ProxyLocalToWorld = GetLocalToWorld();

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];

			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

			if (FlowGridProperties && FlowGridProperties->RenderParams.bDebugWireframe)
			{
				const FLinearColor DrawColor = FLinearColor(1.0f, 1.0f, 1.0f);
				FBox Box(ProxyLocalToWorld.GetOrigin() - FlowGridProperties->VirtualGridExtents, ProxyLocalToWorld.GetOrigin() + FlowGridProperties->VirtualGridExtents);
				DrawWireBox(PDI, Box, DrawColor, SDPG_World, 2.0f);
			}
		}
	}
}

void FFlowGridSceneProxy::CreateRenderThreadResources()
{
}

FPrimitiveViewRelevance FFlowGridSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Relevance = FPrimitiveSceneProxy::GetViewRelevance(View);
	Relevance.bDynamicRelevance = true;
	Relevance.bStaticRelevance = false;
	Relevance.bDrawRelevance = IsShown(View);
	Relevance.bShadowRelevance = false;
	Relevance.bEditorPrimitiveRelevance = false;
	Relevance.bNormalTranslucencyRelevance = true;
	return Relevance;
}

void FFlowGridSceneProxy::SetDynamicData_RenderThread(FFlowGridProperties* InFlowGridProperties)
{
	FlowGridProperties->Release();
	FlowGridProperties = InFlowGridProperties;
	FlowGridProperties->AddRef();

	// if bActive was turned off, clean up the scheduled substeps
	if (!FlowGridProperties->bActive)
	{
		//FlowGridProperties->NumScheduledSubsteps = 0;
	}
}

#if LOG_FLOW_GRID_PROPERTIES

volatile int32 FFlowGridProperties::LogRefCount = 0;

void FFlowGridProperties::LogCreate(FFlowGridProperties* Ptr)
{
	auto Ref = FPlatformAtomics::InterlockedIncrement(&LogRefCount);
	UE_LOG(LogNvFlow, Display, TEXT("NvFlow Create Properties(%p) refCount(%d)"), Ptr, Ref);
}

void FFlowGridProperties::LogRelease(FFlowGridProperties* Ptr)
{
	auto Ref = FPlatformAtomics::InterlockedDecrement(&LogRefCount);
	UE_LOG(LogNvFlow, Display, TEXT("NvFlow Release Properties(%p) refCount(%d)"), Ptr, Ref);
}

#endif

// NvFlow end
