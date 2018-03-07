// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/StaticMeshComponent.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "Components.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Materials/Material.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/RenderingObjectVersion.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/CollisionProfile.h"
#include "ContentStreaming.h"
#include "ComponentReregisterContext.h"
#include "UnrealEngine.h"
#include "EngineUtils.h"
#include "StaticMeshResources.h"
#include "Net/UnrealNetwork.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Misc/MapErrors.h"
#if WITH_EDITOR
#include "Collision.h"
#include "IHierarchicalLODUtilities.h"
#include "HierarchicalLODUtilitiesModule.h"
#endif
#include "LightMap.h"
#include "ShadowMap.h"
#include "Engine/ShadowMapTexture2D.h"
#include "AI/Navigation/NavCollision.h"
#include "Engine/StaticMeshSocket.h"
#include "AI/NavigationSystemHelpers.h"
#include "AI/NavigationOctree.h"
#include "AI/Navigation/NavigationSystem.h"
#include "PhysicsEngine/BodySetup.h"
#include "EngineGlobals.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Engine/StaticMesh.h"
#include "HAL/LowLevelMemTracker.h"

#define LOCTEXT_NAMESPACE "StaticMeshComponent"

DECLARE_MEMORY_STAT( TEXT( "StaticMesh VxColor Inst Mem" ), STAT_InstVertexColorMemory, STATGROUP_MemoryStaticMesh );
DECLARE_MEMORY_STAT( TEXT( "StaticMesh PreCulled Index Memory" ), STAT_StaticMeshPreCulledIndexMemory, STATGROUP_MemoryStaticMesh );

class FStaticMeshComponentInstanceData : public FPrimitiveComponentInstanceData
{
public:
	FStaticMeshComponentInstanceData(const UStaticMeshComponent* SourceComponent)
		: FPrimitiveComponentInstanceData(SourceComponent)
		, StaticMesh(SourceComponent->GetStaticMesh())
	{
	}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FPrimitiveComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		if (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript)
		{
			CastChecked<UStaticMeshComponent>(Component)->ApplyComponentInstanceData(this);
		}
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		FPrimitiveComponentInstanceData::AddReferencedObjects(Collector);

		Collector.AddReferencedObject(StaticMesh);
	}

	/** Add vertex color data for a specified LOD before RerunConstructionScripts is called */
	void AddVertexColorData(const struct FStaticMeshComponentLODInfo& LODInfo, uint32 LODIndex)
	{
		if (VertexColorLODs.Num() <= (int32)LODIndex)
		{
			VertexColorLODs.SetNum(LODIndex + 1);
		}
		FVertexColorLODData& VertexColorData = VertexColorLODs[LODIndex];
		VertexColorData.LODIndex = LODIndex;
		VertexColorData.PaintedVertices = LODInfo.PaintedVertices;
		LODInfo.OverrideVertexColors->GetVertexColors(VertexColorData.VertexBufferColors);
	}

	/** Re-apply vertex color data after RerunConstructionScripts is called */
	bool ApplyVertexColorData(UStaticMeshComponent* StaticMeshComponent) const
	{
		bool bAppliedAnyData = false;

		if (StaticMeshComponent != NULL)
		{
			StaticMeshComponent->SetLODDataCount(VertexColorLODs.Num(), StaticMeshComponent->LODData.Num());

			for (int32 LODDataIndex = 0; LODDataIndex < VertexColorLODs.Num(); ++LODDataIndex)
			{
				const FVertexColorLODData& VertexColorLODData = VertexColorLODs[LODDataIndex];
				uint32 LODIndex = VertexColorLODData.LODIndex;

				if (StaticMeshComponent->LODData.IsValidIndex(LODIndex))
				{
					FStaticMeshComponentLODInfo& LODInfo = StaticMeshComponent->LODData[LODIndex];
					// this component could have been constructed from a template
					// that had its own vert color overrides; so before we apply
					// the instance's color data, we need to clear the old
					// vert colors (so we can properly call InitFromColorArray())
					StaticMeshComponent->RemoveInstanceVertexColorsFromLOD(LODIndex);
					// may not be null at the start (could have been initialized 
					// from a  component template with vert coloring), but should
					// be null at this point, after RemoveInstanceVertexColorsFromLOD()
					if (LODInfo.OverrideVertexColors == NULL)
					{
						LODInfo.PaintedVertices = VertexColorLODData.PaintedVertices;

						LODInfo.OverrideVertexColors = new FColorVertexBuffer;
						LODInfo.OverrideVertexColors->InitFromColorArray(VertexColorLODData.VertexBufferColors);

						BeginInitResource(LODInfo.OverrideVertexColors);
						bAppliedAnyData = true;
					}
				}
			}
		}

		return bAppliedAnyData;
	}

	/** Used to store lightmap data during RerunConstructionScripts */
	struct FLightMapInstanceData
	{
		/** MapBuildDataId from LODData */
		TArray<FGuid>	MapBuildDataIds;
	};

	/** Vertex data stored per-LOD */
	struct FVertexColorLODData
	{
		/** copy of painted vertex data */
		TArray<FPaintedVertex> PaintedVertices;

		/** Copy of vertex buffer colors */
		TArray<FColor> VertexBufferColors;

		/** Index of the LOD that this data came from */
		uint32 LODIndex;

		/** Check whether this contains valid data */
		bool IsValid() const
		{
			return PaintedVertices.Num() > 0 && VertexBufferColors.Num() > 0;
		}
	};

	/** Mesh being used by component */
	class UStaticMesh* StaticMesh;

	/** Array of cached vertex colors for each LOD */
	TArray<FVertexColorLODData> VertexColorLODs;

	FLightMapInstanceData CachedStaticLighting;

	/** Texture streaming build data */
	TArray<FStreamingTextureBuildInfo> StreamingTextureData;

#if WITH_EDITORONLY_DATA
	/** Texture streaming editor data (for viewmodes) */
	TArray<uint32> MaterialStreamingRelativeBoxes;
#endif
};

UStaticMeshComponent::UStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;

	// check BaseEngine.ini for profile setup
	SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

	WireframeColorOverride = FColor(255, 255, 255, 255);

	MinLOD = 0;
	bOverrideLightMapRes = false;
	OverriddenLightMapRes = 64;
	SubDivisionStepSize = 32;
	bUseSubDivisions = true;
	StreamingDistanceMultiplier = 1.0f;
	bBoundsChangeTriggersStreamingDataRebuild = true;
	bHasCustomNavigableGeometry = EHasCustomNavigableGeometry::Yes;
	bOverrideNavigationExport = false;
	bForceNavigationObstacle = true;
	bDisallowMeshPaintPerInstance = false;
	DistanceFieldIndirectShadowMinVisibility = .1f;

	GetBodyInstance()->bAutoWeld = true;	//static mesh by default has auto welding

#if WITH_EDITORONLY_DATA
	SelectedEditorSection = INDEX_NONE;
	SectionIndexPreview = INDEX_NONE;
	SelectedEditorMaterial = INDEX_NONE;
	MaterialIndexPreview = INDEX_NONE;
	StaticMeshImportVersion = BeforeImportStaticMeshVersionWasAdded;
	bCustomOverrideVertexColorPerLOD = false;
	bDisplayVertexColors = false;
#endif
}

UStaticMeshComponent::~UStaticMeshComponent()
{
	// Empty, but required because we don't want to have to include LightMap.h and ShadowMap.h in StaticMeshComponent.h, and they are required to compile FLightMapRef and FShadowMapRef
}

/// @cond DOXYGEN_WARNINGS

void UStaticMeshComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	DOREPLIFETIME( UStaticMeshComponent, StaticMesh );
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

/// @endcond

void UStaticMeshComponent::OnRep_StaticMesh(class UStaticMesh *OldStaticMesh)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// Only do stuff if this actually changed from the last local value
	if (OldStaticMesh!= StaticMesh)
	{
		// We have to force a call to SetStaticMesh with a new StaticMesh
		UStaticMesh *NewStaticMesh = StaticMesh;
		StaticMesh = NULL;
		
		SetStaticMesh(NewStaticMesh);
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool UStaticMeshComponent::HasAnySockets() const
{
	return (GetStaticMesh() != NULL) && (GetStaticMesh()->Sockets.Num() > 0);
}

void UStaticMeshComponent::QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const
{
	if (GetStaticMesh() != NULL)
	{
		for (int32 SocketIdx = 0; SocketIdx < GetStaticMesh()->Sockets.Num(); ++SocketIdx)
		{
			if (UStaticMeshSocket* Socket = GetStaticMesh()->Sockets[SocketIdx])
			{
				new (OutSockets) FComponentSocketDescription(Socket->SocketName, EComponentSocketType::Socket);
			}
		}
	}
}

FString UStaticMeshComponent::GetDetailedInfoInternal() const
{
	FString Result;  

	if( GetStaticMesh() != NULL )
	{
		Result = GetStaticMesh()->GetPathName( NULL );
	}
	else
	{
		Result = TEXT("No_StaticMesh");
	}

	return Result;  
}


void UStaticMeshComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{	
	UStaticMeshComponent* This = CastChecked<UStaticMeshComponent>(InThis);
	Super::AddReferencedObjects(This, Collector);

	for (int32 LODIndex = 0; LODIndex < This->LODData.Num(); LODIndex++)
	{
		if (This->LODData[LODIndex].OverrideMapBuildData)
		{
			This->LODData[LODIndex].OverrideMapBuildData->AddReferencedObjects(Collector);
		}
	}
}


void UStaticMeshComponent::Serialize(FArchive& Ar)
{
	LLM_SCOPE(ELLMTag::StaticMesh);

	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

	Ar << LODData;

	if (Ar.IsLoading())
	{
		for (int32 LODIndex = 0; LODIndex < LODData.Num(); LODIndex++)
		{
			LODData[ LODIndex ].OwningComponent = this;
		}
	}

#if WITH_EDITORONLY_DATA
	if (Ar.UE4Ver() < VER_UE4_COMBINED_LIGHTMAP_TEXTURES)
	{
		check(AttachmentCounter.GetValue() == 0);
		// Irrelevant lights were incorrect before VER_UE4_TOSS_IRRELEVANT_LIGHTS
		IrrelevantLights_DEPRECATED.Empty();
	}

	if (Ar.IsLoading() && Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::MapBuildDataSeparatePackage)
	{
		FMeshMapBuildLegacyData LegacyComponentData;

		for (int32 LODIndex = 0; LODIndex < LODData.Num(); LODIndex++)
		{
			if (LODData[LODIndex].LegacyMapBuildData)
			{
				LODData[LODIndex].LegacyMapBuildData->IrrelevantLights = IrrelevantLights_DEPRECATED;
				LegacyComponentData.Data.Emplace(LODData[LODIndex].MapBuildDataId, LODData[LODIndex].LegacyMapBuildData);
				LODData[LODIndex].LegacyMapBuildData = NULL;
			}
		}

		GComponentsWithLegacyLightmaps.AddAnnotation(this, LegacyComponentData);
	}

	if (Ar.UE4Ver() < VER_UE4_AUTO_WELDING)
	{
		GetBodyInstance()->bAutoWeld = false;	//existing content may rely on no auto welding
	}
#endif
}

void UStaticMeshComponent::PostInitProperties()
{
	Super::PostInitProperties();

	for (int32 LODIndex = 0; LODIndex < LODData.Num(); LODIndex++)
	{
		LODData[LODIndex].OwningComponent = this;
	}
}

bool UStaticMeshComponent::AreNativePropertiesIdenticalTo( UObject* Other ) const
{
	bool bNativePropertiesAreIdentical = Super::AreNativePropertiesIdenticalTo( Other );
	UStaticMeshComponent* OtherSMC = CastChecked<UStaticMeshComponent>(Other);

	if( bNativePropertiesAreIdentical )
	{
		// Components are not identical if they have lighting information.
		if( LODData.Num() || OtherSMC->LODData.Num() )
		{
			bNativePropertiesAreIdentical = false;
		}
	}
	
	return bNativePropertiesAreIdentical;
}

void UStaticMeshComponent::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);
#if WITH_EDITORONLY_DATA
	CachePaintedDataIfNecessary();
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void UStaticMeshComponent::CheckForErrors()
{
	Super::CheckForErrors();

	// Get the mesh owner's name.
	AActor* Owner = GetOwner();
	FString OwnerName(*(GNone.ToString()));
	if ( Owner )
	{
		OwnerName = Owner->GetName();
	}

	// Make sure any simplified meshes can still find their high res source mesh
	if( GetStaticMesh() != NULL && GetStaticMesh()->RenderData )
	{
		int32 ZeroTriangleElements = 0;

		// Check for element material index/material mismatches
		for (int32 LODIndex = 0; LODIndex < GetStaticMesh()->RenderData->LODResources.Num(); ++LODIndex)
		{
			FStaticMeshLODResources& MeshLODData = GetStaticMesh()->RenderData->LODResources[LODIndex];
			for (int32 SectionIndex = 0; SectionIndex < MeshLODData.Sections.Num(); SectionIndex++)
			{
				FStaticMeshSection& Element = MeshLODData.Sections[SectionIndex];
				if (Element.NumTriangles == 0)
				{
					ZeroTriangleElements++;
				}
			}
		}

		if (OverrideMaterials.Num() > GetStaticMesh()->StaticMaterials.Num())
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("OverridenCount"), OverrideMaterials.Num());
			Arguments.Add(TEXT("ReferencedCount"), GetStaticMesh()->StaticMaterials.Num());
			Arguments.Add(TEXT("MeshName"), FText::FromString(GetStaticMesh()->GetName()));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(Owner))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_MoreMaterialsThanReferenced", "More overridden materials ({OverridenCount}) on static mesh component than are referenced ({ReferencedCount}) in source mesh '{MeshName}'" ), Arguments ) ))
				->AddToken(FMapErrorToken::Create(FMapErrors::MoreMaterialsThanReferenced));
		}
		if (ZeroTriangleElements > 0)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("ElementCount"), ZeroTriangleElements);
			Arguments.Add(TEXT("MeshName"), FText::FromString(GetStaticMesh()->GetName()));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(Owner))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_ElementsWithZeroTriangles", "{ElementCount} element(s) with zero triangles in static mesh '{MeshName}'" ), Arguments ) ))
				->AddToken(FMapErrorToken::Create(FMapErrors::ElementsWithZeroTriangles));
		}
	}

	if (!GetStaticMesh() && (!Owner || !Owner->IsA(AWorldSettings::StaticClass())))	// Ignore worldsettings
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(Owner))
			->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_StaticMeshNull", "Static mesh actor has NULL StaticMesh property")))
			->AddToken(FMapErrorToken::Create(FMapErrors::StaticMeshNull));
	}

	if ( BodyInstance.bSimulatePhysics && GetStaticMesh() != NULL && GetStaticMesh()->BodySetup != NULL && GetStaticMesh()->BodySetup->AggGeom.GetElementCount() == 0) 
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_SimulatePhyNoSimpleCollision", "{0} : Using bSimulatePhysics but StaticMesh has not simple collision."), FText::FromString(GetName()) ) ));
	}

	// Warn if component with collision enabled, but no collision data
	if (GetStaticMesh() != NULL && GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		int32 NumSectionsWithCollision = GetStaticMesh()->GetNumSectionsWithCollision();
		int32 NumCollisionPrims = (GetStaticMesh()->BodySetup != nullptr) ? GetStaticMesh()->BodySetup->AggGeom.GetElementCount() : 0;

		if (NumSectionsWithCollision == 0 && NumCollisionPrims == 0)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
			Arguments.Add(TEXT("StaticMeshName"), FText::FromString(GetStaticMesh()->GetName()));

			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(Owner))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_CollisionEnabledNoCollisionGeom", "Collision enabled but StaticMesh ({StaticMeshName}) has no simple or complex collision."), Arguments)))
				->AddToken(FMapErrorToken::Create(FMapErrors::CollisionEnabledNoCollisionGeom));
		}
	}

	if( Mobility == EComponentMobility::Movable &&
		CastShadow && 
		bCastDynamicShadow && 
		IsRegistered() && 
		Bounds.SphereRadius > 2000.0f )
	{
		// Large shadow casting objects that create preshadows will cause a massive performance hit, since preshadows are meant for small shadow casters.
		FMessageLog("MapCheck").PerformanceWarning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_ActorLargeShadowCaster", "Large actor receives a pre-shadow and will cause an extreme performance hit unless bCastDynamicShadow is set to false.") ))
			->AddToken(FMapErrorToken::Create(FMapErrors::ActorLargeShadowCaster));
	}
}
#endif

FBoxSphereBounds UStaticMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if(GetStaticMesh())
	{
		// Graphics bounds.
		FBoxSphereBounds NewBounds = GetStaticMesh()->GetBounds().TransformBy(LocalToWorld);
		NewBounds.BoxExtent *= BoundsScale;
		NewBounds.SphereRadius *= BoundsScale;

		return NewBounds;
	}
	else
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
	}
}

void UStaticMeshComponent::AddSpeedTreeWind()
{
	if (GetStaticMesh() && GetStaticMesh()->RenderData && GetStaticMesh()->SpeedTreeWind.IsValid() && GetScene())
	{
		for (int32 LODIndex = 0; LODIndex < GetStaticMesh()->RenderData->LODResources.Num(); ++LODIndex)
		{
			GetScene()->AddSpeedTreeWind(&GetStaticMesh()->RenderData->LODResources[LODIndex].VertexFactory, GetStaticMesh());
			GetScene()->AddSpeedTreeWind(&GetStaticMesh()->RenderData->LODResources[LODIndex].VertexFactoryOverrideColorVertexBuffer, GetStaticMesh());
		}
	}
}

void UStaticMeshComponent::RemoveSpeedTreeWind()
{
	if (GetStaticMesh() && GetStaticMesh()->RenderData && GetStaticMesh()->SpeedTreeWind.IsValid() && GetScene())
	{
		for (int32 LODIndex = 0; LODIndex < GetStaticMesh()->RenderData->LODResources.Num(); ++LODIndex)
		{
			GetScene()->RemoveSpeedTreeWind(&GetStaticMesh()->RenderData->LODResources[LODIndex].VertexFactoryOverrideColorVertexBuffer, GetStaticMesh());
			GetScene()->RemoveSpeedTreeWind(&GetStaticMesh()->RenderData->LODResources[LODIndex].VertexFactory, GetStaticMesh());
		}
	}
}

void UStaticMeshComponent::PropagateLightingScenarioChange()
{
	FComponentRecreateRenderStateContext Context(this);
}

const FMeshMapBuildData* UStaticMeshComponent::GetMeshMapBuildData(const FStaticMeshComponentLODInfo& LODInfo) const
{
	if (!GetStaticMesh() || !GetStaticMesh()->RenderData)
	{
		return NULL;
	}
	else
	{
		// Check that the static-mesh hasn't been changed to be incompatible with the cached light-map.
		int32 NumLODs = GetStaticMesh()->RenderData->LODResources.Num();
		bool bLODsShareStaticLighting = GetStaticMesh()->RenderData->bLODsShareStaticLighting;

		if (!bLODsShareStaticLighting && NumLODs != LODData.Num())
		{
			return NULL;
		}
	}

	if (LODInfo.OverrideMapBuildData)
	{
		return LODInfo.OverrideMapBuildData.Get();
	}

	AActor* Owner = GetOwner();

	if (Owner)
	{
		ULevel* OwnerLevel = Owner->GetLevel();

		if (OwnerLevel && OwnerLevel->OwningWorld)
		{
			ULevel* ActiveLightingScenario = OwnerLevel->OwningWorld->GetActiveLightingScenario();
			UMapBuildDataRegistry* MapBuildData = NULL;

			if (ActiveLightingScenario && ActiveLightingScenario->MapBuildData)
			{
				MapBuildData = ActiveLightingScenario->MapBuildData;
			}
			else if (OwnerLevel->MapBuildData)
			{
				MapBuildData = OwnerLevel->MapBuildData;
			}

			if (MapBuildData)
			{
				return MapBuildData->GetMeshBuildData(LODInfo.MapBuildDataId);
			}
		}
	}
	
	return NULL;
}

void UStaticMeshComponent::OnRegister()
{
	UpdateCollisionFromStaticMesh();
	
	if (GetStaticMesh() != NULL && GetStaticMesh()->RenderData)
	{
		AddSpeedTreeWind();
	}

#if WITH_EDITORONLY_DATA
	//Remap the override materials if the import version is different
	//We do the remap here because if the UStaticMeshComponent is already load when
	//a static mesh get re-imported the postload will not be call.
	if (GetStaticMesh() && StaticMeshImportVersion != GetStaticMesh()->ImportVersion)
	{
		if (OverrideMaterials.Num())
		{
			uint32 MaterialMapKey = ( (uint32)((StaticMeshImportVersion & 0xffff) << 16) | (uint32)(GetStaticMesh()->ImportVersion & 0xffff));
			for (const FMaterialRemapIndex &MaterialRemapIndex : GetStaticMesh()->MaterialRemapIndexPerImportVersion)
			{
				if (MaterialRemapIndex.ImportVersionKey == MaterialMapKey)
				{
					const TArray<int32> &RemapMaterials = MaterialRemapIndex.MaterialRemap;
					TArray<UMaterialInterface*> OldOverrideMaterials = OverrideMaterials;
					OverrideMaterials.Empty();
					for (int32 MaterialIndex = 0; MaterialIndex < OldOverrideMaterials.Num(); ++MaterialIndex)
					{
						if (!RemapMaterials.IsValidIndex(MaterialIndex))
						{
							continue; //TODO is it allow check() instead
						}
						int32 RemapIndex = RemapMaterials[MaterialIndex];
						if (RemapIndex >= OverrideMaterials.Num())
						{
							//Allocate space
							OverrideMaterials.AddZeroed((RemapIndex - OverrideMaterials.Num()) + 1);
						}
						OverrideMaterials[RemapIndex] = OldOverrideMaterials[MaterialIndex];
					}
					break;
				}
			}
		}
		StaticMeshImportVersion = GetStaticMesh()->ImportVersion;
	}
#endif //WITH_EDITORONLY_DATA

	Super::OnRegister();
}

void UStaticMeshComponent::OnUnregister()
{
	RemoveSpeedTreeWind();

	Super::OnUnregister();
}

void UStaticMeshComponent::OnCreatePhysicsState()
{
	Super::OnCreatePhysicsState();

	bNavigationRelevant = IsNavigationRelevant();
	UNavigationSystem::UpdateComponentInNavOctree(*this);
}

void UStaticMeshComponent::OnDestroyPhysicsState()
{
	Super::OnDestroyPhysicsState();

	UNavigationSystem::UpdateComponentInNavOctree(*this);
	bNavigationRelevant = IsNavigationRelevant();
}

#if WITH_EDITORONLY_DATA

/** Return the total number of LOD sections in the LOD resources */
static int32 GetNumberOfElements(const TIndirectArray<FStaticMeshLODResources>& LODResources)
{
	int32 Count = 0;
	for (int32 LODIndex = 0; LODIndex < LODResources.Num(); ++LODIndex)
	{
		Count += LODResources[LODIndex].Sections.Num();
	}
	return Count;
}

/**
 *	Pack the texture into data ready for saving. Also ensures a single entry per texture.
 *
 *	@param	LevelTextures			[in,out]	The list of textures referred by all component of a level. The array index maps to UTexture2D::LevelIndex.
 *	@param	UnpackedData			[in,out]	The unpacked data, emptied after the function executes.
 *	@param	StreamingTextureData	[out]		The resulting packed data.
 *	@param	RefBounds				[in]		The reference bounds used to packed the relative bounds.
 */
static void PackStreamingTextureData(ULevel* Level, TArray<FStreamingTexturePrimitiveInfo>& UnpackedData, TArray<FStreamingTextureBuildInfo>& StreamingTextureData, const FBoxSphereBounds& RefBounds)
{
	StreamingTextureData.Empty();

	while (UnpackedData.Num())
	{
		FStreamingTexturePrimitiveInfo Info = UnpackedData[0];
		UnpackedData.RemoveAtSwap(0);

		// Merge with any other lod section using the same texture.
		for (int32 Index = 0; Index < UnpackedData.Num(); ++Index)
		{
			const FStreamingTexturePrimitiveInfo& CurrInfo = UnpackedData[Index];

			if (CurrInfo.Texture == Info.Texture)
			{
				Info.Bounds = Union(Info.Bounds, CurrInfo.Bounds);
				// Take the max scale since it relates to higher texture resolution.
				Info.TexelFactor = FMath::Max<float>(Info.TexelFactor, CurrInfo.TexelFactor);

				UnpackedData.RemoveAtSwap(Index);
				--Index;
			}
		}

		FStreamingTextureBuildInfo PackedInfo;
		PackedInfo.PackFrom(Level, RefBounds, Info);
		StreamingTextureData.Push(PackedInfo);
	}
}

#endif

bool UStaticMeshComponent::GetMaterialStreamingData(int32 MaterialIndex, FPrimitiveMaterialInfo& MaterialData) const
{ 
	if (GetStaticMesh())
	{
		MaterialData.Material = GetMaterial(MaterialIndex);
		MaterialData.UVChannelData = GetStaticMesh()->GetUVChannelData(MaterialIndex);
#if WITH_EDITORONLY_DATA
		MaterialData.PackedRelativeBox = MaterialStreamingRelativeBoxes.IsValidIndex(MaterialIndex) ?  MaterialStreamingRelativeBoxes[MaterialIndex] : PackedRelativeBox_Identity;
#else
		MaterialData.PackedRelativeBox = PackedRelativeBox_Identity;
#endif
	}
	return MaterialData.IsValid();
}

bool UStaticMeshComponent::BuildTextureStreamingData(ETextureStreamingBuildType BuildType, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, TSet<FGuid>& DependentResources)
{
	bool bBuildDataValid = true;

#if WITH_EDITORONLY_DATA // Only rebuild the data in editor 
	if (FPlatformProperties::HasEditorOnlyData())
	{
		AActor* ComponentActor = GetOwner();

		if (!bIgnoreInstanceForTextureStreaming && Mobility == EComponentMobility::Static && GetStaticMesh() && GetStaticMesh()->RenderData && !bHiddenInGame)
		{
			// First generate the bounds. Will be used in the texture streaming build and also in the debug viewmode.
			const int32 NumMaterials = GetNumMaterials();

			// Build the material bounds if in full rebuild or if the data is incomplete.
			if (BuildType == TSB_MapBuild || (BuildType == TSB_ViewMode && MaterialStreamingRelativeBoxes.Num() != NumMaterials))
			{
				// Build the material bounds.
				MaterialStreamingRelativeBoxes.Empty(NumMaterials);
				for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
				{
					MaterialStreamingRelativeBoxes.Add(PackRelativeBox(Bounds.GetBox(), GetStaticMesh()->GetMaterialBox(MaterialIndex, GetComponentTransform())));
				}

				// Update since proxy has a copy of the material bounds.
				MarkRenderStateDirty();
			}
			else if (MaterialStreamingRelativeBoxes.Num() != NumMaterials)
			{
				bBuildDataValid = false; 
			}

			// The texture build data can only be recomputed on a map build because of how the the level StreamingTextureGuids are handled.
			if (BuildType == TSB_MapBuild)
			{
				ULevel* Level = ComponentActor ? ComponentActor->GetLevel() : nullptr;
				if (Level)
				{
					// Get the data without any component scaling as the built data does not include scale.
					FStreamingTextureLevelContext LevelContext(QualityLevel, FeatureLevel, true); // Use the boxes that were just computed!
					TArray<FStreamingTexturePrimitiveInfo> UnpackedData;
					GetStreamingTextureInfoInner(LevelContext, nullptr, 1.f, UnpackedData);
					PackStreamingTextureData(Level, UnpackedData, StreamingTextureData, Bounds);
				}
			}
			else if (StreamingTextureData.Num() == 0)
			{
				// Reset the validity here even if the bounds don't fit as the material might not use any streaming textures.
				// This is required as the texture streaming build only mark levels as dirty if they have texture related data.
				bBuildDataValid = true; 

				// In that case, check if the component refers a streaming texture. If so, the build data is missing.
				TArray<UMaterialInterface*> UsedMaterials;
				GetUsedMaterials(UsedMaterials);

				// Reset the validity here even if the bounds don't fit as the material might not use any streaming textures.
				// This is required as the texture streaming build only mark levels as dirty if they have texture related data.
				bBuildDataValid = true; 

				for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
				{
					FPrimitiveMaterialInfo MaterialData;
					if (GetMaterialStreamingData(MaterialIndex, MaterialData) && UsedMaterials.Contains(MaterialData.Material))
					{
						check(MaterialData.Material);

						// Sometimes there is missing data because the fallback density is 0
						if (MaterialData.Material->UseAnyStreamingTexture() && MaterialData.UVChannelData->LocalUVDensities[0] > 0)
						{
							bBuildDataValid = false; 
							break;
						}
					}
				}
			}

			// Generate the build reference guids
			if (StreamingTextureData.Num())
			{
				DependentResources.Add(GetStaticMesh()->GetLightingGuid());

				TArray<UMaterialInterface*> UsedMaterials;
				GetUsedMaterials(UsedMaterials);
				for (UMaterialInterface* UsedMaterial : UsedMaterials)
				{
					// Materials not having the RF_Public are instances created dynamically.
					if (UsedMaterial && UsedMaterial->UseAnyStreamingTexture() && UsedMaterial->GetOutermost() != GetTransientPackage() && UsedMaterial->HasAnyFlags(RF_Public))
					{
						TArray<FGuid> MaterialGuids;
						UsedMaterial->GetLightingGuidChain(false, MaterialGuids);
						DependentResources.Append(MaterialGuids);
					}
				}
			}
		}
		else // Otherwise clear any data.
		{
			StreamingTextureData.Empty();

			if (MaterialStreamingRelativeBoxes.Num())
			{
				MaterialStreamingRelativeBoxes.Empty();
				MarkRenderStateDirty(); // Update since proxy has a copy of the material bounds.
			}
		}
	}
#endif
	return bBuildDataValid;
}

float UStaticMeshComponent::GetTextureStreamingTransformScale() const
{
	return GetComponentTransform().GetMaximumAxisScale();
}

void UStaticMeshComponent::GetStreamingTextureInfo(FStreamingTextureLevelContext& LevelContext, TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const
{
	if (bIgnoreInstanceForTextureStreaming || !GetStaticMesh() || !GetStaticMesh()->HasValidRenderData())
	{
		return;
	}

	const float TransformScale = GetTextureStreamingTransformScale();
	GetStreamingTextureInfoInner(LevelContext, Mobility == EComponentMobility::Static ? &StreamingTextureData : nullptr, TransformScale * StreamingDistanceMultiplier, OutStreamingTextures);

	// Process the lightmaps and shadowmaps entries.
	for (int32 LODIndex = 0; LODIndex < LODData.Num(); ++LODIndex)
	{
		const FStaticMeshComponentLODInfo& LODInfo = LODData[LODIndex];
		const FMeshMapBuildData* MeshMapBuildData = GetMeshMapBuildData(LODInfo);
		FLightMap2D* Lightmap = MeshMapBuildData && MeshMapBuildData->LightMap ? MeshMapBuildData->LightMap->GetLightMap2D() : NULL;
		uint32 LightmapIndex = AllowHighQualityLightmaps(LevelContext.GetFeatureLevel()) ? 0 : 1;
		if (Lightmap && Lightmap->IsValid(LightmapIndex))
		{
			const FVector2D& Scale = Lightmap->GetCoordinateScale();
			if (Scale.X > SMALL_NUMBER && Scale.Y > SMALL_NUMBER)
			{
				const float TexelFactor = GetStaticMesh()->LightmapUVDensity * TransformScale / FMath::Min(Scale.X, Scale.Y);
				new (OutStreamingTextures) FStreamingTexturePrimitiveInfo(Lightmap->GetTexture(LightmapIndex), Bounds, TexelFactor, PackedRelativeBox_Identity);
				new (OutStreamingTextures) FStreamingTexturePrimitiveInfo(Lightmap->GetAOMaterialMaskTexture(), Bounds, TexelFactor, PackedRelativeBox_Identity);
				new (OutStreamingTextures) FStreamingTexturePrimitiveInfo(Lightmap->GetSkyOcclusionTexture(), Bounds, TexelFactor, PackedRelativeBox_Identity);
			}
		}

		FShadowMap2D* Shadowmap = MeshMapBuildData && MeshMapBuildData->ShadowMap ? MeshMapBuildData->ShadowMap->GetShadowMap2D() : NULL;
		if (Shadowmap && Shadowmap->IsValid())
		{
			const FVector2D& Scale = Shadowmap->GetCoordinateScale();
			if (Scale.X > SMALL_NUMBER && Scale.Y > SMALL_NUMBER)
			{
				const float TexelFactor = GetStaticMesh()->LightmapUVDensity * TransformScale / FMath::Min(Scale.X, Scale.Y);
				new (OutStreamingTextures) FStreamingTexturePrimitiveInfo(Shadowmap->GetTexture(), Bounds, TexelFactor, PackedRelativeBox_Identity);
			}
		}
	}
}

UBodySetup* UStaticMeshComponent::GetBodySetup()
{
	if(GetStaticMesh())
	{
		return GetStaticMesh()->BodySetup;
	}

	return NULL;
}

bool UStaticMeshComponent::CanEditSimulatePhysics()
{
	if (UBodySetup* BodySetup = GetBodySetup())
	{
		return (BodySetup->AggGeom.GetElementCount() > 0) || (BodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple);
	}
	else
	{
		return false;
	}
}

FColor UStaticMeshComponent::GetWireframeColor() const
{
	if(bOverrideWireframeColor)
	{
		return WireframeColorOverride;
	}
	else
	{
		if(Mobility == EComponentMobility::Static)
		{
			return FColor(0, 255, 255, 255);
		}
		else if(Mobility == EComponentMobility::Stationary)
		{
			return FColor(128, 128, 255, 255);
		}
		else // Movable
		{
			if(BodyInstance.bSimulatePhysics)
			{
				return FColor(0, 255, 128, 255);
			}
			else
			{
				return FColor(255, 0, 255, 255);
			}
		}
	}
}


bool UStaticMeshComponent::DoesSocketExist(FName InSocketName) const 
{
	return (GetSocketByName(InSocketName)  != NULL);
}

#if WITH_EDITOR
bool UStaticMeshComponent::ShouldRenderSelected() const
{
	const bool bShouldRenderSelected = UMeshComponent::ShouldRenderSelected();
	return bShouldRenderSelected || bDisplayVertexColors;
}
#endif // WITH_EDITOR

class UStaticMeshSocket const* UStaticMeshComponent::GetSocketByName(FName InSocketName) const
{
	UStaticMeshSocket const* Socket = NULL;

	if( GetStaticMesh() )
	{
		Socket = GetStaticMesh()->FindSocket( InSocketName );
	}

	return Socket;
}

FTransform UStaticMeshComponent::GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace) const
{
	if (InSocketName != NAME_None)
	{
		UStaticMeshSocket const* const Socket = GetSocketByName(InSocketName);
		if (Socket)
		{
			FTransform SocketWorldTransform;
			if ( Socket->GetSocketTransform(SocketWorldTransform, this) )
			{
				switch(TransformSpace)
				{
					case RTS_World:
					{
						return SocketWorldTransform;
					}
					case RTS_Actor:
					{
						if( const AActor* Actor = GetOwner() )
						{
							return SocketWorldTransform.GetRelativeTransform( GetOwner()->GetTransform() );
						}
						break;
					}
					case RTS_Component:
					{
						return SocketWorldTransform.GetRelativeTransform(GetComponentTransform());
					}
				}
			}
		}
	}

	return Super::GetSocketTransform(InSocketName, TransformSpace);
}


bool UStaticMeshComponent::RequiresOverrideVertexColorsFixup()
{
	bool bFixupRequired = false;

#if WITH_EDITORONLY_DATA
	if ( GetStaticMesh() && GetStaticMesh()->RenderData
		&& GetStaticMesh()->RenderData->DerivedDataKey != StaticMeshDerivedDataKey
		&& LODData.Num() > 0
		&& LODData[0].OverrideVertexColors
		&& LODData[0].OverrideVertexColors->GetNumVertices() > 0
		&& LODData[0].PaintedVertices.Num() > 0 )
	{
		bFixupRequired = true;
	}
#endif // WITH_EDITORONLY_DATA

	return bFixupRequired;
}

void UStaticMeshComponent::SetSectionPreview(int32 InSectionIndexPreview)
{
#if WITH_EDITORONLY_DATA
	if (SectionIndexPreview != InSectionIndexPreview)
	{
		SectionIndexPreview = InSectionIndexPreview;
		MarkRenderStateDirty();
	}
#endif
}

void UStaticMeshComponent::SetMaterialPreview(int32 InMaterialIndexPreview)
{
#if WITH_EDITORONLY_DATA
	if (MaterialIndexPreview != InMaterialIndexPreview)
	{
		MaterialIndexPreview = InMaterialIndexPreview;
		MarkRenderStateDirty();
	}
#endif
}

void UStaticMeshComponent::RemoveInstanceVertexColorsFromLOD( int32 LODToRemoveColorsFrom )
{
#if WITH_EDITORONLY_DATA
	if (GetStaticMesh() && LODToRemoveColorsFrom < GetStaticMesh()->GetNumLODs() && LODToRemoveColorsFrom < LODData.Num())
	{
		FStaticMeshComponentLODInfo& CurrentLODInfo = LODData[LODToRemoveColorsFrom];

		CurrentLODInfo.ReleaseOverrideVertexColorsAndBlock();
		CurrentLODInfo.PaintedVertices.Empty();
		StaticMeshDerivedDataKey = GetStaticMesh()->RenderData->DerivedDataKey;
	}
#endif
}

void UStaticMeshComponent::RemoveInstanceVertexColors()
{
#if WITH_EDITORONLY_DATA
	for ( int32 i=0; i < GetStaticMesh()->GetNumLODs(); i++ )
	{
		RemoveInstanceVertexColorsFromLOD( i );
	}
#endif
}

void UStaticMeshComponent::CopyInstanceVertexColorsIfCompatible( UStaticMeshComponent* SourceComponent )
{
#if WITH_EDITORONLY_DATA
	// The static mesh assets have to match, currently.
	if (( GetStaticMesh()->GetPathName() == SourceComponent->GetStaticMesh()->GetPathName() ) &&
		( SourceComponent->LODData.Num() != 0 ))
	{
		Modify();

		bool bIsRegistered = IsRegistered();
		FComponentReregisterContext ReregisterContext(this);
		if (bIsRegistered)
		{
			FlushRenderingCommands(); // don't sync threads unless we have to
		}
		// Remove any and all vertex colors from the target static mesh, if they exist.
		RemoveInstanceVertexColors();

		int32 NumSourceLODs = SourceComponent->GetStaticMesh()->GetNumLODs();

		// This this will set up the LODData for all the LODs
		SetLODDataCount( NumSourceLODs, NumSourceLODs );

		// Copy vertex colors from Source to Target (this)
		for ( int32 CurrentLOD = 0; CurrentLOD != NumSourceLODs; CurrentLOD++ )
		{
			FStaticMeshLODResources& SourceLODModel = SourceComponent->GetStaticMesh()->RenderData->LODResources[CurrentLOD];
			if (SourceComponent->LODData.IsValidIndex(CurrentLOD))
			{
				FStaticMeshComponentLODInfo& SourceLODInfo = SourceComponent->LODData[CurrentLOD];

				FStaticMeshLODResources& TargetLODModel = GetStaticMesh()->RenderData->LODResources[CurrentLOD];
				FStaticMeshComponentLODInfo& TargetLODInfo = LODData[CurrentLOD];

				if ( SourceLODInfo.OverrideVertexColors != NULL )
				{
					// Copy vertex colors from source to target.
					FColorVertexBuffer* SourceColorBuffer = SourceLODInfo.OverrideVertexColors;

					TArray< FColor > CopiedColors;
					for ( uint32 ColorVertexIndex = 0; ColorVertexIndex < SourceColorBuffer->GetNumVertices(); ColorVertexIndex++ )
					{
						CopiedColors.Add( SourceColorBuffer->VertexColor( ColorVertexIndex ) );
					}

					if (TargetLODInfo.OverrideVertexColors != NULL || CopiedColors.Num() > 0)
					{
						FColorVertexBuffer* TargetColorBuffer = &TargetLODModel.ColorVertexBuffer;

						if ( TargetLODInfo.OverrideVertexColors != NULL )
						{
							TargetLODInfo.BeginReleaseOverrideVertexColors();
							FlushRenderingCommands();
						}
						else
						{
							TargetLODInfo.OverrideVertexColors = new FColorVertexBuffer;
							TargetLODInfo.OverrideVertexColors->InitFromColorArray( CopiedColors );
						}
						BeginInitResource( TargetLODInfo.OverrideVertexColors );
					}
				}
			}
		}

		CachePaintedDataIfNecessary();
		StaticMeshDerivedDataKey = GetStaticMesh()->RenderData->DerivedDataKey;

		MarkRenderStateDirty();
	}
#endif
}

void UStaticMeshComponent::CachePaintedDataIfNecessary()
{
#if WITH_EDITORONLY_DATA
	// Only cache the vertex positions if we're in the editor
	if ( GIsEditor && GetStaticMesh() )
	{
		// Iterate over each component LOD info checking for the existence of override colors
		int32 NumLODs = GetStaticMesh()->GetNumLODs();
		for ( TArray<FStaticMeshComponentLODInfo>::TIterator LODIter( LODData ); LODIter; ++LODIter )
		{
			FStaticMeshComponentLODInfo& CurCompLODInfo = *LODIter;

			// Workaround for a copy-paste bug. If the number of painted vertices is <= 1 we know the data is garbage.
			if ( CurCompLODInfo.PaintedVertices.Num() <= 1 )
			{
				CurCompLODInfo.PaintedVertices.Empty();
			}

			// If the mesh has override colors but no cached vertex positions, then the current vertex positions should be cached to help preserve instanced vertex colors during mesh tweaks
			// NOTE: We purposefully do *not* cache the positions if cached positions already exist, as this would result in the loss of the ability to fixup the component if the source mesh
			// were changed multiple times before a fix-up operation was attempted
			if ( CurCompLODInfo.OverrideVertexColors && 
				 CurCompLODInfo.OverrideVertexColors->GetNumVertices() > 0 &&
				 CurCompLODInfo.PaintedVertices.Num() == 0 &&
				 LODIter.GetIndex() < NumLODs ) 
			{
				FStaticMeshLODResources* CurRenderData = &(GetStaticMesh()->RenderData->LODResources[ LODIter.GetIndex() ]);
				if ( CurRenderData->GetNumVertices() == CurCompLODInfo.OverrideVertexColors->GetNumVertices() )
				{
					// Cache the data.
					CurCompLODInfo.PaintedVertices.Reserve( CurRenderData->GetNumVertices() );
					for ( int32 VertIndex = 0; VertIndex < CurRenderData->GetNumVertices(); ++VertIndex )
					{
						FPaintedVertex* Vertex = new( CurCompLODInfo.PaintedVertices ) FPaintedVertex;
						Vertex->Position = CurRenderData->PositionVertexBuffer.VertexPosition( VertIndex );
						Vertex->Normal = CurRenderData->VertexBuffer.VertexTangentZ( VertIndex );
						Vertex->Color = CurCompLODInfo.OverrideVertexColors->VertexColor( VertIndex );
					}
				}
				else
				{					
					// At this point we can't resolve the colors, so just discard any isolated data we still have
					if (CurCompLODInfo.OverrideVertexColors && CurCompLODInfo.OverrideVertexColors->GetNumVertices() > 0)
					{
						UE_LOG(LogStaticMesh, Warning, TEXT("Level requires re-saving! Outdated vertex color overrides have been discarded for %s %s LOD%d. "), *GetFullName(), *GetStaticMesh()->GetFullName(), LODIter.GetIndex());
						CurCompLODInfo.ReleaseOverrideVertexColorsAndBlock();
					}
					else
					{
						UE_LOG(LogStaticMesh, Warning, TEXT("Unable to cache painted data for mesh component. Vertex color overrides will be lost if the mesh is modified. %s %s LOD%d."), *GetFullName(), *GetStaticMesh()->GetFullName(), LODIter.GetIndex() );
					}
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

bool UStaticMeshComponent::FixupOverrideColorsIfNecessary( bool bRebuildingStaticMesh )
{
#if WITH_EDITORONLY_DATA

	// Detect if there is a version mismatch between the source mesh and the component. If so, the component's LODs potentially
	// need to have their override colors updated to match changes in the source mesh.

	if ( RequiresOverrideVertexColorsFixup() )
	{
		// Check if we are building the static mesh.  If so we dont need to reregister this component as its already unregistered and will be reregistered
		// when the static mesh is done building.  Having nested reregister contexts is not supported.
		if( bRebuildingStaticMesh )
		{
			PrivateFixupOverrideColors();
		}
		else
		{
			// Detach this component because rendering changes are about to be applied
			FComponentReregisterContext ReregisterContext( this );
			PrivateFixupOverrideColors();
		}

		return true;
	}
#endif // WITH_EDITORONLY_DATA

	return false;
}

void UStaticMeshComponent::InitResources()
{
	for(int32 LODIndex = 0; LODIndex < LODData.Num(); LODIndex++)
	{
		FStaticMeshComponentLODInfo &LODInfo = LODData[LODIndex];
		if(LODInfo.OverrideVertexColors)
		{
			BeginInitResource(LODInfo.OverrideVertexColors);
			INC_DWORD_STAT_BY( STAT_InstVertexColorMemory, LODInfo.OverrideVertexColors->GetAllocatedSize() );
		}
	}
}

void UStaticMeshComponent::PrivateFixupOverrideColors()
{
#if WITH_EDITOR
	if (!GetStaticMesh() || !GetStaticMesh()->RenderData)
	{
		return;
	}

	const uint32 NumLODs = GetStaticMesh()->RenderData->LODResources.Num();

	// Initialize override vertex colors on any new LODs which have just been created
	SetLODDataCount(NumLODs, LODData.Num());
	bool UpdateStaticMeshDeriveDataKey = false;
	FStaticMeshComponentLODInfo& LOD0Info = LODData[0];
	if (!bCustomOverrideVertexColorPerLOD && LOD0Info.OverrideVertexColors == nullptr)
	{
		return;
	}

	FStaticMeshLODResources& SourceRenderData = GetStaticMesh()->RenderData->LODResources[0];
	for (uint32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
	{
		FStaticMeshComponentLODInfo& LODInfo = LODData[LODIndex];
		if (LODInfo.OverrideVertexColors == nullptr)
		{
			if (bCustomOverrideVertexColorPerLOD) //No fixup required if the component is in custom LOD paint and there is no paint on a LOD
				continue;
			LODInfo.OverrideVertexColors = new FColorVertexBuffer;
		}
		else
		{
			LODInfo.BeginReleaseOverrideVertexColors();
			FlushRenderingCommands();
		}


		FStaticMeshLODResources& CurRenderData = GetStaticMesh()->RenderData->LODResources[LODIndex];
		TArray<FColor> NewOverrideColors;
		if (bCustomOverrideVertexColorPerLOD)
		{
			//Since in custom we fix paint only if the component has some, the PaintedVertices should be allocate
			check(LODInfo.PaintedVertices.Num() > 0);
			//Use the existing LOD custom paint and remap it on the new mesh
			RemapPaintedVertexColors(
				LODInfo.PaintedVertices,
				*LODInfo.OverrideVertexColors,
				SourceRenderData.PositionVertexBuffer,
				SourceRenderData.VertexBuffer,
				CurRenderData.PositionVertexBuffer,
				&CurRenderData.VertexBuffer,
				NewOverrideColors
				);
		}
		else if(LOD0Info.PaintedVertices.Num() > 0)
		{
			RemapPaintedVertexColors(
				LOD0Info.PaintedVertices,
				*LOD0Info.OverrideVertexColors,
				SourceRenderData.PositionVertexBuffer,
				SourceRenderData.VertexBuffer,
				CurRenderData.PositionVertexBuffer,
				&CurRenderData.VertexBuffer,
				NewOverrideColors
				);
		}
		if (NewOverrideColors.Num())
		{
			LODInfo.OverrideVertexColors->InitFromColorArray(NewOverrideColors);

			// Update the PaintedVertices array
			const int32 NumVerts = CurRenderData.GetNumVertices();
			check(NumVerts == NewOverrideColors.Num());

			LODInfo.PaintedVertices.Empty(NumVerts);
			for (int32 VertIndex = 0; VertIndex < NumVerts; ++VertIndex)
			{
				FPaintedVertex* Vertex = new(LODInfo.PaintedVertices) FPaintedVertex;
				Vertex->Position = CurRenderData.PositionVertexBuffer.VertexPosition(VertIndex);
				Vertex->Normal = CurRenderData.VertexBuffer.VertexTangentZ(VertIndex);
				Vertex->Color = LODInfo.OverrideVertexColors->VertexColor(VertIndex);
			}
		}

		BeginInitResource(LODInfo.OverrideVertexColors);
		UpdateStaticMeshDeriveDataKey = true;
	}

	if (UpdateStaticMeshDeriveDataKey)
	{
		StaticMeshDerivedDataKey = GetStaticMesh()->RenderData->DerivedDataKey;
	}

#endif // WITH_EDITOR
}

float GKeepPreCulledIndicesThreshold = .95f;

FAutoConsoleVariableRef CKeepPreCulledIndicesThreshold(
	TEXT("r.KeepPreCulledIndicesThreshold"),
	GKeepPreCulledIndicesThreshold,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

void UStaticMeshComponent::UpdatePreCulledData(int32 LODIndex, const TArray<uint32>& PreCulledData, const TArray<int32>& NumTrianglesPerSection)
{
	const FStaticMeshLODResources& StaticMeshLODResources = GetStaticMesh()->RenderData->LODResources[LODIndex];

	int32 NumOriginalTriangles = 0;
	int32 NumVisibleTriangles = 0;

	for (int32 SectionIndex = 0; SectionIndex < StaticMeshLODResources.Sections.Num(); SectionIndex++)
	{
		const FStaticMeshSection& Section = StaticMeshLODResources.Sections[SectionIndex];
		NumOriginalTriangles += Section.NumTriangles;
		NumVisibleTriangles += NumTrianglesPerSection[SectionIndex];
	}

	if (NumVisibleTriangles / (float)NumOriginalTriangles < GKeepPreCulledIndicesThreshold)
	{
		SetLODDataCount(LODIndex + 1, LODData.Num());

		DEC_DWORD_STAT_BY(STAT_StaticMeshPreCulledIndexMemory, LODData[LODIndex].PreCulledIndexBuffer.GetAllocatedSize());
		//@todo - game thread
		check(IsInRenderingThread());
		LODData[LODIndex].PreCulledIndexBuffer.ReleaseResource();
		LODData[LODIndex].PreCulledIndexBuffer.SetIndices(PreCulledData, EIndexBufferStride::AutoDetect);
		LODData[LODIndex].PreCulledIndexBuffer.InitResource();

		INC_DWORD_STAT_BY(STAT_StaticMeshPreCulledIndexMemory, LODData[LODIndex].PreCulledIndexBuffer.GetAllocatedSize());
		LODData[LODIndex].PreCulledSections.Empty(StaticMeshLODResources.Sections.Num());

		int32 FirstIndex = 0;

		for (int32 SectionIndex = 0; SectionIndex < StaticMeshLODResources.Sections.Num(); SectionIndex++)
		{
			const FStaticMeshSection& Section = StaticMeshLODResources.Sections[SectionIndex];
			FPreCulledStaticMeshSection PreCulledSection;
			PreCulledSection.FirstIndex = FirstIndex;
			PreCulledSection.NumTriangles = NumTrianglesPerSection[SectionIndex];
			FirstIndex += PreCulledSection.NumTriangles * 3;
			LODData[LODIndex].PreCulledSections.Add(PreCulledSection);
		}
	}
	else if (LODIndex < LODData.Num())
	{
		LODData[LODIndex].PreCulledIndexBuffer.ReleaseResource();
		TArray<uint32> EmptyIndices;
		LODData[LODIndex].PreCulledIndexBuffer.SetIndices(EmptyIndices, EIndexBufferStride::AutoDetect);
		LODData[LODIndex].PreCulledSections.Empty(StaticMeshLODResources.Sections.Num());
	}
}

void UStaticMeshComponent::ReleaseResources()
{
	for(int32 LODIndex = 0;LODIndex < LODData.Num();LODIndex++)
	{
		LODData[LODIndex].BeginReleaseOverrideVertexColors();
		DEC_DWORD_STAT_BY(STAT_StaticMeshPreCulledIndexMemory, LODData[LODIndex].PreCulledIndexBuffer.GetAllocatedSize());
		BeginReleaseResource(&LODData[LODIndex].PreCulledIndexBuffer);
	}

	DetachFence.BeginFence();
}

void UStaticMeshComponent::BeginDestroy()
{
	Super::BeginDestroy();
	ReleaseResources();
}

void UStaticMeshComponent::ExportCustomProperties(FOutputDevice& Out, uint32 Indent)
{
	for (int32 LODIdx = 0; LODIdx < LODData.Num(); ++LODIdx)
	{
		Out.Logf(TEXT("%sCustomProperties "), FCString::Spc(Indent));

		FStaticMeshComponentLODInfo& LODInfo = LODData[LODIdx];

		if ((LODInfo.PaintedVertices.Num() > 0) || LODInfo.OverrideVertexColors)
		{
			Out.Logf( TEXT("CustomLODData LOD=%d "), LODIdx );
		}

		// Export the PaintedVertices array
		if (LODInfo.PaintedVertices.Num() > 0)
		{
			FString	ValueStr;
			LODInfo.ExportText(ValueStr);
			Out.Log(ValueStr);
		}
		
		// Export the OverrideVertexColors buffer
		if(LODInfo.OverrideVertexColors && LODInfo.OverrideVertexColors->GetNumVertices() > 0)
		{
			FString	Value;
			LODInfo.OverrideVertexColors->ExportText(Value);

			Out.Log(Value);
		}
		Out.Logf(TEXT("\r\n"));
	}
}

void UStaticMeshComponent::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	check(SourceText);
	check(Warn);

	if (FParse::Command(&SourceText,TEXT("CustomLODData")))
	{
		int32 MaxLODIndex = -1;
		int32 LODIndex;
		FString TmpStr;

		static const TCHAR LODString[] = TEXT("LOD=");
		if (FParse::Value(SourceText, LODString, LODIndex))
		{
			TmpStr = FString::Printf(TEXT("%d"), LODIndex);
			SourceText += TmpStr.Len() + (ARRAY_COUNT(LODString) - 1); // without the zero terminator

			// See if we need to add a new element to the LODData array
			if (LODIndex > MaxLODIndex)
			{
				SetLODDataCount(LODIndex + 1, LODIndex + 1);
				MaxLODIndex = LODIndex;
			}
		}

		FStaticMeshComponentLODInfo& LODInfo = LODData[LODIndex];

		// Populate the PaintedVertices array
		LODInfo.ImportText(&SourceText);

		// Populate the OverrideVertexColors buffer
		if (const TCHAR* VertColorStr = FCString::Stristr(SourceText, TEXT("ColorVertexData")))
		{
			SourceText = VertColorStr;

			// this component could have been constructed from a template that
			// had its own vert color overrides; so before we apply the
			// custom color data, we need to clear the old vert colors (so
			// we can properly call ImportText())
			RemoveInstanceVertexColorsFromLOD(LODIndex);

			// may not be null at the start (could have been initialized 
			// from a blueprint component template with vert coloring), but 
			// should be null by this point, after RemoveInstanceVertexColorsFromLOD()
			check(LODInfo.OverrideVertexColors == nullptr);

			LODInfo.OverrideVertexColors = new FColorVertexBuffer;
			LODInfo.OverrideVertexColors->ImportText(SourceText);
		}
	}
}

#if WITH_EDITOR

void UStaticMeshComponent::PreEditUndo()
{
	Super::PreEditUndo();

	// Undo can result in a resize of LODData which can calls ~FStaticMeshComponentLODInfo.
	// To safely delete FStaticMeshComponentLODInfo::OverrideVertexColors we
	// need to make sure the RT thread has no access to it any more.
	check(!IsRegistered());
	ReleaseResources();
	DetachFence.Wait();
}

void UStaticMeshComponent::PostEditUndo()
{
	// If the StaticMesh was also involved in this transaction, it may need reinitialization first
	if (GetStaticMesh())
	{
		GetStaticMesh()->InitResources();
	}

	// The component's light-maps are loaded from the transaction, so their resources need to be reinitialized.
	InitResources();

	// Debug check command trying to track down undo related uninitialized resource
	if (GetStaticMesh() != NULL && GetStaticMesh()->RenderData.IsValid() && GetStaticMesh()->RenderData->LODResources.Num() > 0)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ResourceCheckCommand,
			FRenderResource*, Resource, &GetStaticMesh()->RenderData->LODResources[0].IndexBuffer,
			{
				check( Resource->IsInitialized() );
			}
		);
	}
	Super::PostEditUndo();
}

void UStaticMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Ensure that OverriddenLightMapRes is a factor of 4
	OverriddenLightMapRes = FMath::Max(OverriddenLightMapRes + 3 & ~3,4);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (((PropertyThatChanged->GetName().Contains(TEXT("OverriddenLightMapRes")) ) && (bOverrideLightMapRes == true)) ||
			(PropertyThatChanged->GetName().Contains(TEXT("bOverrideLightMapRes")) ))
		{
			InvalidateLightingCache();
		}

		if ( PropertyThatChanged->GetName().Contains(TEXT("bIgnoreInstanceForTextureStreaming")) ||
			 PropertyThatChanged->GetName().Contains(TEXT("StreamingDistanceMultiplier")) )
		{
			GEngine->TriggerStreamingDataRebuild();
		}

		if ( PropertyThatChanged->GetName() == TEXT("StaticMesh") )
		{
			InvalidateLightingCache();

			RecreatePhysicsState();

			// If the owning actor is part of a cluster flag it as dirty
			IHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<IHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
			IHierarchicalLODUtilities* Utilities = Module.GetUtilities();
			Utilities->HandleActorModified(GetOwner());

			// Broadcast that the static mesh has changed
			OnStaticMeshChangedEvent.Broadcast(this);

			// If the staticmesh changed, then the component needs a texture streaming rebuild.
			StreamingTextureData.Empty();
			
			if (OverrideMaterials.Num())
			{
				// Static mesh was switched so we should clean up the override materials
				CleanUpOverrideMaterials();
			}
		}

		if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UStaticMeshComponent, OverrideMaterials))
		{
			// If the owning actor is part of a cluster flag it as dirty
			IHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<IHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
			IHierarchicalLODUtilities* Utilities = Module.GetUtilities();
			Utilities->HandleActorModified(GetOwner());

			// If the materials changed, then the component needs a texture streaming rebuild.
			StreamingTextureData.Empty();
		}
	}

	FBodyInstanceEditorHelpers::EnsureConsistentMobilitySimulationSettingsOnPostEditChange(this, PropertyChangedEvent);

	LightmassSettings.EmissiveBoost = FMath::Max(LightmassSettings.EmissiveBoost, 0.0f);
	LightmassSettings.DiffuseBoost = FMath::Max(LightmassSettings.DiffuseBoost, 0.0f);

	// Ensure properties are in sane range.
	SubDivisionStepSize = FMath::Clamp( SubDivisionStepSize, 1, 128 );

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool UStaticMeshComponent::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStaticMeshComponent, bCastDistanceFieldIndirectShadow))
		{
			return Mobility != EComponentMobility::Static && CastShadow && bCastDynamicShadow;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStaticMeshComponent, DistanceFieldIndirectShadowMinVisibility))
		{
			return Mobility != EComponentMobility::Static && bCastDistanceFieldIndirectShadow && CastShadow && bCastDynamicShadow;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStaticMeshComponent, bOverrideDistanceFieldSelfShadowBias))
		{
			return bAffectDistanceFieldLighting;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStaticMeshComponent, DistanceFieldSelfShadowBias))
		{
			return bOverrideDistanceFieldSelfShadowBias && bAffectDistanceFieldLighting;
		}
	}

	return Super::CanEditChange(InProperty);
}

#endif // WITH_EDITOR

bool UStaticMeshComponent::SupportsDefaultCollision()
{
	return GetStaticMesh() && GetBodySetup() == GetStaticMesh()->BodySetup;
}

bool UStaticMeshComponent::SupportsDitheredLODTransitions()
{
	// Only support dithered transitions if all materials do.
	TArray<class UMaterialInterface*> Materials = GetMaterials();
	for (UMaterialInterface* Material : Materials)
	{
		if (Material && !Material->IsDitheredLODTransition())
		{
			return false;
		}
	}
	return true;
}

void UStaticMeshComponent::UpdateCollisionFromStaticMesh()
{
	if(bUseDefaultCollision && SupportsDefaultCollision())
	{
		if (UBodySetup* BodySetup = GetBodySetup())
		{
			BodyInstance.UseExternalCollisionProfile(BodySetup);	//static mesh component by default uses the same collision profile as its static mesh
		}
	}
}

void UStaticMeshComponent::PostLoad()
{
	// need to postload the StaticMesh because super initializes variables based on GetStaticLightingType() which we override and use from the StaticMesh
	if (GetStaticMesh())
	{
		GetStaticMesh()->ConditionalPostLoad();
	}

	Super::PostLoad();

	if ( GetStaticMesh() )
	{
		CachePaintedDataIfNecessary();

		double StartFixupTime = FPlatformTime::Seconds();

		if (FixupOverrideColorsIfNecessary())
		{
#if WITH_EDITORONLY_DATA

			AActor* Owner = GetOwner();

			if (Owner)
			{
				ULevel* Level = Owner->GetLevel();

				if (Level)
				{
					// Accumulate stats about the fixup so we don't spam log messages
					Level->FixupOverrideVertexColorsTime += (float)(FPlatformTime::Seconds() - StartFixupTime);
					Level->FixupOverrideVertexColorsCount++;
				}
			}
#endif
		}
	}

	// Empty after potential editor fix-up when we don't care about re-saving, e.g. game or client
	if (!GIsEditor && !IsRunningCommandlet())
	{
		for (FStaticMeshComponentLODInfo& LOD : LODData)
		{
			LOD.PaintedVertices.Empty();
		}
	}

#if WITH_EDITORONLY_DATA
	// Remap the materials array if the static mesh materials may have been remapped to remove zero triangle sections.
	if (GetStaticMesh() && GetLinkerUE4Version() < VER_UE4_REMOVE_ZERO_TRIANGLE_SECTIONS && OverrideMaterials.Num())
	{
		if (GetStaticMesh()->HasValidRenderData()
			&& GetStaticMesh()->RenderData->MaterialIndexToImportIndex.Num())
		{
			TArray<UMaterialInterface*> OldMaterials;
			const TArray<int32>& MaterialIndexToImportIndex = GetStaticMesh()->RenderData->MaterialIndexToImportIndex;

			Exchange(OverrideMaterials,OldMaterials);
			OverrideMaterials.Empty(MaterialIndexToImportIndex.Num());
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialIndexToImportIndex.Num(); ++MaterialIndex)
			{
				UMaterialInterface* Material = NULL;
				int32 OldMaterialIndex = MaterialIndexToImportIndex[MaterialIndex];
				if (OldMaterials.IsValidIndex(OldMaterialIndex))
				{
					Material = OldMaterials[OldMaterialIndex];
				}
				OverrideMaterials.Add(Material);
			}
		}

		if (OverrideMaterials.Num() > GetStaticMesh()->StaticMaterials.Num())
		{
			OverrideMaterials.RemoveAt(GetStaticMesh()->StaticMaterials.Num(), OverrideMaterials.Num() - GetStaticMesh()->StaticMaterials.Num());
		}
	}

#endif // #if WITH_EDITORONLY_DATA

	// Legacy content may contain a lightmap resolution of 0, which was valid when vertex lightmaps were supported, but not anymore with only texture lightmaps
	OverriddenLightMapRes = FMath::Max(OverriddenLightMapRes, 4);

	// Initialize the resources for the freshly loaded component.
	InitResources();
}

bool UStaticMeshComponent::SetStaticMesh(UStaticMesh* NewMesh)
{
	// Do nothing if we are already using the supplied static mesh
	if(NewMesh == GetStaticMesh())
	{
		return false;
	}

	// Don't allow changing static meshes if "static" and registered
	AActor* Owner = GetOwner();
	if (UWorld * World = GetWorld())
	{
		if (World->HasBegunPlay() && !AreDynamicDataChangesAllowed() && Owner != nullptr)
		{
			FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SetMeshOnStatic", "Calling SetStaticMesh on '{0}' but Mobility is Static."),
				FText::FromString(GetPathName())));
			return false;
		}
	}

	// Remove speed tree wind for this staticmesh from scene
	RemoveSpeedTreeWind();

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StaticMesh = NewMesh;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Add speed tree wind if required
	AddSpeedTreeWind();

	// Need to send this to render thread at some point
	MarkRenderStateDirty();

	// Update physics representation right away
	RecreatePhysicsState();

	// update navigation relevancy
	bNavigationRelevant = IsNavigationRelevant();

	// Notify the streaming system. Don't use Update(), because this may be the first time the mesh has been set
	// and the component may have to be added to the streaming system for the first time.
	IStreamingManager::Get().NotifyPrimitiveAttached( this, DPT_Spawned );

	// Since we have new mesh, we need to update bounds
	UpdateBounds();

	// Mark cached material parameter names dirty
	MarkCachedMaterialParameterNameIndicesDirty();

#if WITH_EDITOR
	// Broadcast that the static mesh has changed
	OnStaticMeshChangedEvent.Broadcast(this);
#endif

#if WITH_EDITORONLY_DATA
	if (GetStaticMesh())
	{
		StaticMeshImportVersion = GetStaticMesh()->ImportVersion;
	}
#endif
	return true;
}

void UStaticMeshComponent::SetForcedLodModel(int32 NewForcedLodModel)
{
	if (ForcedLodModel != NewForcedLodModel)
	{
		ForcedLodModel = NewForcedLodModel;
		MarkRenderStateDirty();
	}
}

void UStaticMeshComponent::SetDistanceFieldSelfShadowBias(float NewValue)
{
	if (DistanceFieldSelfShadowBias != NewValue)
	{
		DistanceFieldSelfShadowBias = NewValue;
		MarkRenderStateDirty();
	}
}

void UStaticMeshComponent::GetLocalBounds(FVector& Min, FVector& Max) const
{
	if (GetStaticMesh())
	{
		FBoxSphereBounds MeshBounds = GetStaticMesh()->GetBounds();
		Min = MeshBounds.Origin - MeshBounds.BoxExtent;
		Max = MeshBounds.Origin + MeshBounds.BoxExtent;
	}
}

void UStaticMeshComponent::SetCollisionProfileName(FName InCollisionProfileName)
{
	Super::SetCollisionProfileName(InCollisionProfileName);
	bUseDefaultCollision = false;
}

bool UStaticMeshComponent::UsesOnlyUnlitMaterials() const
{
	if (GetStaticMesh() && GetStaticMesh()->RenderData)
	{
		// Figure out whether any of the sections has a lit material assigned.
		bool bUsesOnlyUnlitMaterials = true;
		for (int32 LODIndex = 0; bUsesOnlyUnlitMaterials && LODIndex < GetStaticMesh()->RenderData->LODResources.Num(); ++LODIndex)
		{
			FStaticMeshLODResources& LOD = GetStaticMesh()->RenderData->LODResources[LODIndex];
			for (int32 ElementIndex=0; bUsesOnlyUnlitMaterials && ElementIndex<LOD.Sections.Num(); ElementIndex++)
			{
				UMaterialInterface*	MaterialInterface	= GetMaterial(LOD.Sections[ElementIndex].MaterialIndex);
				UMaterial*			Material			= MaterialInterface ? MaterialInterface->GetMaterial() : NULL;

				bUsesOnlyUnlitMaterials = Material && Material->GetShadingModel() == MSM_Unlit;
			}
		}
		return bUsesOnlyUnlitMaterials;
	}
	else
	{
		return false;
	}
}


bool UStaticMeshComponent::GetLightMapResolution( int32& Width, int32& Height ) const
{
	bool bPadded = false;
	if( GetStaticMesh() )
	{
		// Use overridden per component lightmap resolution.
		if( bOverrideLightMapRes )
		{
			Width	= OverriddenLightMapRes;
			Height	= OverriddenLightMapRes;
		}
		// Use the lightmap resolution defined in the static mesh.
		else
		{
			Width	= GetStaticMesh()->LightMapResolution;
			Height	= GetStaticMesh()->LightMapResolution;
		}
		bPadded = true;
	}
	// No associated static mesh!
	else
	{
		Width	= 0;
		Height	= 0;
	}

	return bPadded;
}


void UStaticMeshComponent::GetEstimatedLightMapResolution(int32& Width, int32& Height) const
{
	if (GetStaticMesh())
	{
		ELightMapInteractionType LMIType = GetStaticLightingType();

		bool bUseSourceMesh = false;

		// Use overridden per component lightmap resolution.
		// If the overridden LM res is > 0, then this is what would be used...
		if (bOverrideLightMapRes == true)
		{
			if (OverriddenLightMapRes != 0)
			{
				Width	= OverriddenLightMapRes;
				Height	= OverriddenLightMapRes;
			}
		}
		else
		{
			bUseSourceMesh = true;
		}

		// Use the lightmap resolution defined in the static mesh.
		if (bUseSourceMesh == true)
		{
			Width	= GetStaticMesh()->LightMapResolution;
			Height	= GetStaticMesh()->LightMapResolution;
		}

		// If it was not set by anything, give it a default value...
		if (Width == 0)
		{
			int32 TempInt = 0;
			verify(GConfig->GetInt(TEXT("DevOptions.StaticLighting"), TEXT("DefaultStaticMeshLightingRes"), TempInt, GLightmassIni));

			Width	= TempInt;
			Height	= TempInt;
		}
	}
	else
	{
		Width	= 0;
		Height	= 0;
	}
}


int32 UStaticMeshComponent::GetStaticLightMapResolution() const
{
	int32 Width, Height;
	GetLightMapResolution(Width, Height);
	return FMath::Max<int32>(Width, Height);
}

bool UStaticMeshComponent::HasValidSettingsForStaticLighting(bool bOverlookInvalidComponents) const
{
	if (bOverlookInvalidComponents && !GetStaticMesh())
	{
		// Return true for invalid components, this is used during the map check where those invalid components will be warned about separately
		return true;
	}
	else
	{
		int32 LightMapWidth = 0;
		int32 LightMapHeight = 0;
		GetLightMapResolution(LightMapWidth, LightMapHeight);

		return Super::HasValidSettingsForStaticLighting(bOverlookInvalidComponents) 
			&& GetStaticMesh()
			&& UsesTextureLightmaps(LightMapWidth, LightMapHeight);
	}
}

bool UStaticMeshComponent::UsesTextureLightmaps(int32 InWidth, int32 InHeight) const
{
	return (
		(HasLightmapTextureCoordinates()) &&
		(InWidth > 0) && 
		(InHeight > 0)
		);
}


bool UStaticMeshComponent::HasLightmapTextureCoordinates() const
{
	if ((GetStaticMesh() != NULL) &&
		(GetStaticMesh()->LightMapCoordinateIndex >= 0) &&
		(GetStaticMesh()->RenderData != NULL) &&
		(GetStaticMesh()->RenderData->LODResources.Num() > 0) &&
		(GetStaticMesh()->LightMapCoordinateIndex >= 0) &&	
		((uint32)GetStaticMesh()->LightMapCoordinateIndex < GetStaticMesh()->RenderData->LODResources[0].VertexBuffer.GetNumTexCoords()))
	{
		return true;
	}
	return false;
}


void UStaticMeshComponent::GetTextureLightAndShadowMapMemoryUsage(int32 InWidth, int32 InHeight, int32& OutLightMapMemoryUsage, int32& OutShadowMapMemoryUsage) const
{
	// Stored in texture.
	const float MIP_FACTOR = 1.33f;
	OutShadowMapMemoryUsage = FMath::TruncToInt(MIP_FACTOR * InWidth * InHeight); // G8

	auto FeatureLevel = GetWorld() ? GetWorld()->FeatureLevel : GMaxRHIFeatureLevel;

	if (AllowHighQualityLightmaps(FeatureLevel))
	{
		OutLightMapMemoryUsage = FMath::TruncToInt(NUM_HQ_LIGHTMAP_COEF * MIP_FACTOR * InWidth * InHeight); // DXT5
	}
	else
	{
		OutLightMapMemoryUsage = FMath::TruncToInt(NUM_LQ_LIGHTMAP_COEF * MIP_FACTOR * InWidth * InHeight / 2); // DXT1
	}
}


void UStaticMeshComponent::GetLightAndShadowMapMemoryUsage( int32& LightMapMemoryUsage, int32& ShadowMapMemoryUsage ) const
{
	// Zero initialize.
	ShadowMapMemoryUsage = 0;
	LightMapMemoryUsage = 0;

	// Cache light/ shadow map resolution.
	int32 LightMapWidth = 0;
	int32	LightMapHeight = 0;
	GetLightMapResolution(LightMapWidth, LightMapHeight);

	// Determine whether static mesh/ static mesh component has static shadowing.
	if (HasStaticLighting() && GetStaticMesh())
	{
		// Determine whether we are using a texture or vertex buffer to store precomputed data.
		if (UsesTextureLightmaps(LightMapWidth, LightMapHeight) == true)
		{
			GetTextureLightAndShadowMapMemoryUsage(LightMapWidth, LightMapHeight, LightMapMemoryUsage, ShadowMapMemoryUsage);
		}
	}
}


bool UStaticMeshComponent::GetEstimatedLightAndShadowMapMemoryUsage( 
	int32& TextureLightMapMemoryUsage, int32& TextureShadowMapMemoryUsage,
	int32& VertexLightMapMemoryUsage, int32& VertexShadowMapMemoryUsage,
	int32& StaticLightingResolution, bool& bIsUsingTextureMapping, bool& bHasLightmapTexCoords) const
{
	TextureLightMapMemoryUsage	= 0;
	TextureShadowMapMemoryUsage	= 0;
	VertexLightMapMemoryUsage	= 0;
	VertexShadowMapMemoryUsage	= 0;
	bIsUsingTextureMapping		= false;
	bHasLightmapTexCoords		= false;

	// Cache light/ shadow map resolution.
	int32 LightMapWidth			= 0;
	int32 LightMapHeight		= 0;
	GetEstimatedLightMapResolution(LightMapWidth, LightMapHeight);
	StaticLightingResolution = LightMapWidth;

	int32 TrueLightMapWidth		= 0;
	int32 TrueLightMapHeight	= 0;
	GetLightMapResolution(TrueLightMapWidth, TrueLightMapHeight);

	// Determine whether static mesh/ static mesh component has static shadowing.
	if (HasStaticLighting() && GetStaticMesh())
	{
		// Determine whether we are using a texture or vertex buffer to store precomputed data.
		bHasLightmapTexCoords = HasLightmapTextureCoordinates();
		// Determine whether we are using a texture or vertex buffer to store precomputed data.
		bIsUsingTextureMapping = UsesTextureLightmaps(TrueLightMapWidth, TrueLightMapHeight);
		// Stored in texture.
		GetTextureLightAndShadowMapMemoryUsage(LightMapWidth, LightMapHeight, TextureLightMapMemoryUsage, TextureShadowMapMemoryUsage);

		return true;
	}

	return false;
}

int32 UStaticMeshComponent::GetNumMaterials() const
{
	// @note : you don't have to consider Materials.Num()
	// that only counts if overridden and it can't be more than GetStaticMesh()->Materials. 
	if(GetStaticMesh())
	{
		return GetStaticMesh()->StaticMaterials.Num();
	}
	else
	{
		return 0;
	}
}

int32 UStaticMeshComponent::GetMaterialIndex(FName MaterialSlotName) const
{
	return GetStaticMesh() ? GetStaticMesh()->GetMaterialIndex(MaterialSlotName) : -1;
}

TArray<FName> UStaticMeshComponent::GetMaterialSlotNames() const
{
	TArray<FName> MaterialNames;
	if (UStaticMesh* Mesh = GetStaticMesh())
	{
		for (int32 MaterialIndex = 0; MaterialIndex < Mesh->StaticMaterials.Num(); ++MaterialIndex)
		{
			const FStaticMaterial &StaticMaterial = Mesh->StaticMaterials[MaterialIndex];
			MaterialNames.Add(StaticMaterial.MaterialSlotName);
		}
	}
	return MaterialNames;
}

bool UStaticMeshComponent::IsMaterialSlotNameValid(FName MaterialSlotName) const
{
	return GetMaterialIndex(MaterialSlotName) >= 0;
}

UMaterialInterface* UStaticMeshComponent::GetMaterial(int32 MaterialIndex) const
{
	// If we have a base materials array, use that
	if(OverrideMaterials.IsValidIndex(MaterialIndex) && OverrideMaterials[MaterialIndex])
	{
		return OverrideMaterials[MaterialIndex];
	}
	// Otherwise get from static mesh
	else
	{
		return GetStaticMesh() ? GetStaticMesh()->GetMaterial(MaterialIndex) : nullptr;
	}
}

void UStaticMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if( GetStaticMesh() && GetStaticMesh()->RenderData )
	{
		for (int32 LODIndex = 0; LODIndex < GetStaticMesh()->RenderData->LODResources.Num(); LODIndex++)
		{
			FStaticMeshLODResources& LODResources = GetStaticMesh()->RenderData->LODResources[LODIndex];
			for (int32 SectionIndex = 0; SectionIndex < LODResources.Sections.Num(); SectionIndex++)
			{
				// Get the material for each element at the current lod index
				OutMaterials.AddUnique(GetMaterial(LODResources.Sections[SectionIndex].MaterialIndex));
			}
		}
	}
}

int32 UStaticMeshComponent::GetBlueprintCreatedComponentIndex() const
{
	int32 ComponentIndex = 0;
	for (const UActorComponent* Component : GetOwner()->BlueprintCreatedComponents)
	{
		if(Component == this)
		{
			return ComponentIndex;
		}

		ComponentIndex++;
	}

	return INDEX_NONE;
}

FActorComponentInstanceData* UStaticMeshComponent::GetComponentInstanceData() const
{
	FStaticMeshComponentInstanceData* StaticMeshInstanceData = new FStaticMeshComponentInstanceData(this);

	// Fill in info
	for (const FStaticMeshComponentLODInfo& LODDataEntry : LODData)
	{
		StaticMeshInstanceData->CachedStaticLighting.MapBuildDataIds.Add(LODDataEntry.MapBuildDataId);
	}

	// Backup the texture streaming data.
	StaticMeshInstanceData->StreamingTextureData = StreamingTextureData;
#if WITH_EDITORONLY_DATA
	StaticMeshInstanceData->MaterialStreamingRelativeBoxes = MaterialStreamingRelativeBoxes;
#endif

	// Cache instance vertex colors
	for( int32 LODIndex = 0; LODIndex < LODData.Num(); ++LODIndex )
	{
		const FStaticMeshComponentLODInfo& LODInfo = LODData[LODIndex];

		if ( LODInfo.OverrideVertexColors && LODInfo.OverrideVertexColors->GetNumVertices() > 0 && LODInfo.PaintedVertices.Num() > 0 )
		{
			if (!StaticMeshInstanceData)
			{
				StaticMeshInstanceData = new FStaticMeshComponentInstanceData(this);
			}

			StaticMeshInstanceData->AddVertexColorData(LODInfo, LODIndex);
		}
	}

	return StaticMeshInstanceData;
}

void UStaticMeshComponent::ApplyComponentInstanceData(FStaticMeshComponentInstanceData* StaticMeshInstanceData)
{
	check(StaticMeshInstanceData);

	// Note: ApplyComponentInstanceData is called while the component is registered so the rendering thread is already using this component
	// That means all component state that is modified here must be mirrored on the scene proxy, which will be recreated to receive the changes later due to MarkRenderStateDirty.

	if (GetStaticMesh() != StaticMeshInstanceData->StaticMesh)
	{
		return;
	}

	const int32 NumLODLightMaps = StaticMeshInstanceData->CachedStaticLighting.MapBuildDataIds.Num();

	if (HasStaticLighting() && NumLODLightMaps > 0)
	{
		// See if data matches current state
		if (StaticMeshInstanceData->GetComponentTransform().Equals(GetComponentTransform(), 1.e-3f))
		{
			SetLODDataCount(NumLODLightMaps, NumLODLightMaps);

			for (int32 i = 0; i < NumLODLightMaps; ++i)
			{
				LODData[i].MapBuildDataId = StaticMeshInstanceData->CachedStaticLighting.MapBuildDataIds[i];
			}
		}
		else
		{
			UE_ASSET_LOG(LogStaticMesh, Warning, this,
				TEXT("Cached component instance data transform did not match!  Discarding cached lighting data which will cause lighting to be unbuilt.\n%s\nCurrent: %s Cached: %s"), 
				*GetPathName(),
				*GetComponentTransform().ToString(),
				*StaticMeshInstanceData->GetComponentTransform().ToString());
		}
	}

	if (!bDisallowMeshPaintPerInstance)
	{
		FComponentReregisterContext ReregisterStaticMesh(this);
		StaticMeshInstanceData->ApplyVertexColorData(this);
	}

	// Restore the texture streaming data.
	StreamingTextureData = StaticMeshInstanceData->StreamingTextureData;
#if WITH_EDITORONLY_DATA
	MaterialStreamingRelativeBoxes = StaticMeshInstanceData->MaterialStreamingRelativeBoxes;
#endif
}

bool UStaticMeshComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	if (GetStaticMesh() != NULL && GetStaticMesh()->NavCollision != NULL)
	{
		UNavCollision* NavCollision = GetStaticMesh()->NavCollision;
		const bool bExportAsObstacle = bOverrideNavigationExport ? bForceNavigationObstacle : NavCollision->bIsDynamicObstacle;

		if (bExportAsObstacle)
		{
			return false;
		}
		
		if (NavCollision->bHasConvexGeometry)
		{
			const FVector Scale3D = GetComponentTransform().GetScale3D();
			// if any of scales is 0 there's no point in exporting it
			if (!Scale3D.IsZero())
			{
				GeomExport.ExportCustomMesh(NavCollision->ConvexCollision.VertexBuffer.GetData(), NavCollision->ConvexCollision.VertexBuffer.Num(),
					NavCollision->ConvexCollision.IndexBuffer.GetData(), NavCollision->ConvexCollision.IndexBuffer.Num(), GetComponentTransform());

				GeomExport.ExportCustomMesh(NavCollision->TriMeshCollision.VertexBuffer.GetData(), NavCollision->TriMeshCollision.VertexBuffer.Num(),
					NavCollision->TriMeshCollision.IndexBuffer.GetData(), NavCollision->TriMeshCollision.IndexBuffer.Num(), GetComponentTransform());
			}

			// regardless of above we don't want "regular" collision export for this mesh instance
			return false;
		}
	}

	return true;
}

UMaterialInterface* UStaticMeshComponent::GetMaterialFromCollisionFaceIndex(int32 FaceIndex, int32& SectionIndex) const
{
	UMaterialInterface* Result = nullptr;
	SectionIndex = 0;

	UStaticMesh* Mesh = GetStaticMesh();
	if (Mesh && Mesh->RenderData.IsValid() && FaceIndex >= 0)
	{
		// Get the info for the LOD that is used for collision
		int32 LODIndex = Mesh->LODForCollision;
		FStaticMeshRenderData* RenderData = Mesh->RenderData.Get();
		if (RenderData->LODResources.IsValidIndex(LODIndex))
		{
			const FStaticMeshLODResources& LODResource = RenderData->LODResources[LODIndex];

			// Look for section that corresponds to the supplied face
			int32 TotalFaceCount = 0;
			for (int32 SectionIdx = 0; SectionIdx < LODResource.Sections.Num(); SectionIdx++)
			{
				const FStaticMeshSection& Section = LODResource.Sections[SectionIdx];
				// Only count faces if collision is enabled
				if (Section.bEnableCollision)
				{
					TotalFaceCount += Section.NumTriangles;

					if (FaceIndex < TotalFaceCount)
					{
						// Get the current material for it, from this component
						Result = GetMaterial(Section.MaterialIndex);
						SectionIndex = SectionIdx;
						break;
					}
				}
			}
		}		
	}

	return Result;
}


bool UStaticMeshComponent::IsNavigationRelevant() const
{
	return GetStaticMesh() != nullptr && GetStaticMesh()->GetNavCollision() != nullptr && Super::IsNavigationRelevant();
}

void UStaticMeshComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	Super::GetNavigationData(Data);

	if (GetStaticMesh() && GetStaticMesh()->NavCollision)	
	{
		UNavCollision* NavCollision = GetStaticMesh()->NavCollision;
		const bool bExportAsObstacle = bOverrideNavigationExport ? bForceNavigationObstacle : NavCollision->bIsDynamicObstacle;

		if (bExportAsObstacle)
		{
			NavCollision->GetNavigationModifier(Data.Modifiers, GetComponentTransform());
		}
	}
}

#if WITH_EDITOR
bool UStaticMeshComponent::ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	if (!bConsiderOnlyBSP && ShowFlags.StaticMeshes && GetStaticMesh() != nullptr && GetStaticMesh()->HasValidRenderData())
	{
		// Check if we are even inside it's bounding box, if we are not, there is no way we colliding via the more advanced checks we will do.
		if (Super::ComponentIsTouchingSelectionBox(InSelBBox, ShowFlags, bConsiderOnlyBSP, false))
		{
			TArray<FVector> Vertex;

			FStaticMeshLODResources& LODModel = GetStaticMesh()->RenderData->LODResources[0];
			FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();

			for (const auto& Section : LODModel.Sections)
			{
				// Iterate over each triangle.
				for (int32 TriangleIndex = 0; TriangleIndex < (int32)Section.NumTriangles; TriangleIndex++)
				{
					Vertex.Empty(3);

					int32 FirstIndex = TriangleIndex * 3 + Section.FirstIndex;
					for (int32 i = 0; i < 3; i++)
					{
						int32 VertexIndex = Indices[FirstIndex + i];
						FVector LocalPosition = LODModel.PositionVertexBuffer.VertexPosition(VertexIndex);
						Vertex.Emplace(GetComponentTransform().TransformPosition(LocalPosition));
					}

					// Check if the triangle is colliding with the bounding box.
					FSeparatingAxisPointCheck ThePointCheck(Vertex, InSelBBox.GetCenter(), InSelBBox.GetExtent(), false);
					if (!bMustEncompassEntireComponent && ThePointCheck.bHit)
					{
						// Needn't encompass entire component: any intersection, we consider as touching
						return true;
					}
					else if (bMustEncompassEntireComponent && !ThePointCheck.bHit)
					{
						// Must encompass entire component: any non intersection, we consider as not touching
						return false;
					}
				}
			}

			// Either:
			// a) It must encompass the entire component and all points were intersected (return true), or;
			// b) It needn't encompass the entire component but no points were intersected (return false)
			return bMustEncompassEntireComponent;
		}
	}

	return false;
}


bool UStaticMeshComponent::ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	if (!bConsiderOnlyBSP && ShowFlags.StaticMeshes && GetStaticMesh() != nullptr && GetStaticMesh()->HasValidRenderData())
	{
		// Check if we are even inside it's bounding box, if we are not, there is no way we colliding via the more advanced checks we will do.
		if (Super::ComponentIsTouchingSelectionFrustum(InFrustum, ShowFlags, bConsiderOnlyBSP, false))
		{
			TArray<FVector> Vertex;

			FStaticMeshLODResources& LODModel = GetStaticMesh()->RenderData->LODResources[0];

			uint32 NumVertices = LODModel.VertexBuffer.GetNumVertices();
			for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
			{
				const FVector& LocalPosition = LODModel.PositionVertexBuffer.VertexPosition(VertexIndex);
				const FVector WorldPosition = GetComponentTransform().TransformPosition(LocalPosition);
				bool bLocationIntersected = InFrustum.IntersectSphere(WorldPosition, 0.0f);
				if (bLocationIntersected && !bMustEncompassEntireComponent)
				{
					return true;
				}
				else if (!bLocationIntersected && bMustEncompassEntireComponent)
				{
					return false;
				}
			}

			// Either:
			// a) It must encompass the entire component and all points were intersected (return true), or;
			// b) It needn't encompass the entire component but no points were intersected (return false)
			return bMustEncompassEntireComponent;
		}
	}

	return false;
}
#endif


//////////////////////////////////////////////////////////////////////////
// StaticMeshComponentLODInfo



/** Default constructor */
FStaticMeshComponentLODInfo::FStaticMeshComponentLODInfo()
	: LegacyMapBuildData(NULL)
	, OverrideVertexColors(NULL)
	, OwningComponent(NULL)
{
	// Used by deserialization only, MapBuildDataId will be deserialized
}

FStaticMeshComponentLODInfo::FStaticMeshComponentLODInfo(UStaticMeshComponent* InOwningComponent)
	: LegacyMapBuildData(NULL)
	, OverrideVertexColors(NULL)
	, OwningComponent(InOwningComponent)
	{
	MapBuildDataId = FGuid::NewGuid();
}

/** Destructor */
FStaticMeshComponentLODInfo::~FStaticMeshComponentLODInfo()
{
	// Note: OverrideVertexColors had BeginReleaseResource called in UStaticMeshComponent::BeginDestroy, 
	// And waits on a fence for that command to complete in UStaticMeshComponent::IsReadyForFinishDestroy,
	// So we know it is safe to delete OverrideVertexColors here (RT can't be referencing it anymore)
	CleanUp();
}

void FStaticMeshComponentLODInfo::CleanUp()
{
	if(OverrideVertexColors)
	{
		DEC_DWORD_STAT_BY( STAT_InstVertexColorMemory, OverrideVertexColors->GetAllocatedSize() );
	}

	delete OverrideVertexColors;
	OverrideVertexColors = NULL;

	PaintedVertices.Empty();
}


void FStaticMeshComponentLODInfo::BeginReleaseOverrideVertexColors()
{
	if(OverrideVertexColors)
	{
		// enqueue a rendering command to release
		BeginReleaseResource(OverrideVertexColors);
	}
}

void FStaticMeshComponentLODInfo::ReleaseOverrideVertexColorsAndBlock()
{
	if(OverrideVertexColors)
	{
		// enqueue a rendering command to release
		BeginReleaseResource(OverrideVertexColors);
		// Ensure the RT no longer accessed the data, might slow down
		FlushRenderingCommands();
		// The RT thread has no access to it any more so it's safe to delete it.
		CleanUp();
	}
}

void FStaticMeshComponentLODInfo::ExportText(FString& ValueStr)
{
	ValueStr += FString::Printf(TEXT("PaintedVertices(%d)="), PaintedVertices.Num());

	// Rough approximation
	ValueStr.Reserve(ValueStr.Len() + PaintedVertices.Num() * 125);

	// Export the Position, Normal and Color info for each vertex
	for(int32 i = 0; i < PaintedVertices.Num(); ++i)
	{
		FPaintedVertex& Vert = PaintedVertices[i];

		ValueStr += FString::Printf(TEXT("((Position=(X=%.6f,Y=%.6f,Z=%.6f),"), Vert.Position.X, Vert.Position.Y, Vert.Position.Z);
		ValueStr += FString::Printf(TEXT("(Normal=(X=%d,Y=%d,Z=%d,W=%d),"), Vert.Normal.Vector.X, Vert.Normal.Vector.Y, Vert.Normal.Vector.Z, Vert.Normal.Vector.W);
		ValueStr += FString::Printf(TEXT("(Color=(B=%d,G=%d,R=%d,A=%d))"), Vert.Color.B, Vert.Color.G, Vert.Color.R, Vert.Color.A);

		// Seperate each vertex entry with a comma
		if ((i + 1) != PaintedVertices.Num())
		{
			ValueStr += TEXT(",");
		}
	}

	ValueStr += TEXT(" ");
}

void FStaticMeshComponentLODInfo::ImportText(const TCHAR** SourceText)
{
	FString TmpStr;
	int32 VertCount;
	if (FParse::Value(*SourceText, TEXT("PaintedVertices("), VertCount))
	{
		TmpStr = FString::Printf(TEXT("%d"), VertCount);
		*SourceText += TmpStr.Len() + 18;

		FString SourceTextStr = *SourceText;
		TArray<FString> Tokens;
		int32 TokenIdx = 0;
		bool bValidInput = true;

		// Tokenize the text
		SourceTextStr.ParseIntoArray(Tokens, TEXT(","), false);

		// There should be 11 tokens per vertex
		check(Tokens.Num() * 11 >= VertCount);

		PaintedVertices.AddUninitialized(VertCount);

		for (int32 Idx = 0; Idx < VertCount; ++Idx)
		{
			// Position
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("X="), PaintedVertices[Idx].Position.X);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("Y="), PaintedVertices[Idx].Position.Y);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("Z="), PaintedVertices[Idx].Position.Z);
			// Normal
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("X="), PaintedVertices[Idx].Normal.Vector.X);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("Y="), PaintedVertices[Idx].Normal.Vector.Y);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("Z="), PaintedVertices[Idx].Normal.Vector.Z);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("W="), PaintedVertices[Idx].Normal.Vector.W);
			// Color
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("B="), PaintedVertices[Idx].Color.B);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("G="), PaintedVertices[Idx].Color.G);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("R="), PaintedVertices[Idx].Color.R);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("A="), PaintedVertices[Idx].Color.A);

			// Verify that the info for this vertex was read correctly
			check(bValidInput);
		}

		// Advance the text pointer past all of the data we just read
		int32 LODDataStrLen = 0;
		for (int32 Idx = 0; Idx < TokenIdx - 1; ++Idx)
		{
			LODDataStrLen += Tokens[Idx].Len() + 1;
		}
		*SourceText += LODDataStrLen;
	}
}

int32 GKeepKeepOverrideVertexColorsOnCPU = 1;
FAutoConsoleVariableRef CKeepOverrideVertexColorsOnCPU(
	TEXT("r.KeepOverrideVertexColorsOnCPU"),
	GKeepKeepOverrideVertexColorsOnCPU,
	TEXT("Keeps a CPU copy of override vertex colors.  May be required for some blueprints / object spawning."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

FArchive& operator<<(FArchive& Ar,FStaticMeshComponentLODInfo& I)
{
	const uint8 OverrideColorsStripFlag = 1;
	bool bStrippedOverrideColors = false;
#if WITH_EDITORONLY_DATA
	if( Ar.IsCooking() )
	{
		// Check if override color should be stripped too	
		int32 LODIndex = 0;
		for( ; LODIndex < I.OwningComponent->LODData.Num() && &I != &I.OwningComponent->LODData[ LODIndex ]; LODIndex++ )
		{}
		check( LODIndex < I.OwningComponent->LODData.Num() );

		bStrippedOverrideColors = !I.OverrideVertexColors || 
			( I.OwningComponent->GetStaticMesh() == NULL || 
			I.OwningComponent->GetStaticMesh()->RenderData == NULL ||
			LODIndex >= I.OwningComponent->GetStaticMesh()->RenderData->LODResources.Num() ||
			I.OverrideVertexColors->GetNumVertices() != I.OwningComponent->GetStaticMesh()->RenderData->LODResources[LODIndex].VertexBuffer.GetNumVertices() );
	}
#endif // WITH_EDITORONLY_DATA
	FStripDataFlags StripFlags( Ar, bStrippedOverrideColors ? OverrideColorsStripFlag : 0 );

	if( !StripFlags.IsDataStrippedForServer() )
	{
		if (Ar.IsLoading() && Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::MapBuildDataSeparatePackage)
		{
			I.MapBuildDataId = FGuid::NewGuid();
			I.LegacyMapBuildData = new FMeshMapBuildData();
			Ar << I.LegacyMapBuildData->LightMap;
			Ar << I.LegacyMapBuildData->ShadowMap;
		}
		else
		{
			Ar << I.MapBuildDataId;
		}
	}

	if( !StripFlags.IsClassDataStripped( OverrideColorsStripFlag ) )
	{
		// Bulk serialization (new method)
		uint8 bLoadVertexColorData = (I.OverrideVertexColors != NULL);
		Ar << bLoadVertexColorData;

		if(bLoadVertexColorData)
		{
			if(Ar.IsLoading())
			{
				check(!I.OverrideVertexColors);
				I.OverrideVertexColors = new FColorVertexBuffer;
			}

			//we want to discard the vertex colors after rhi init when in cooked/client builds.
			const bool bNeedsCPUAccess = !Ar.IsLoading() || GIsEditor || IsRunningCommandlet() || (GKeepKeepOverrideVertexColorsOnCPU != 0);
			I.OverrideVertexColors->Serialize(Ar, bNeedsCPUAccess);
		}
	}

	// Serialize out cached vertex information if necessary.
	if ( !StripFlags.IsEditorDataStripped() )
	{
		Ar << I.PaintedVertices;
	}

	return Ar;
}

#undef LOCTEXT_NAMESPACE
