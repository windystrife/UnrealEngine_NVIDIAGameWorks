// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Level.cpp: Level-related functions
=============================================================================*/

#include "Engine/Level.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/RenderingObjectVersion.h"
#include "Templates/ScopedPointer.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Package.h"
#include "Serialization/AsyncLoading.h"
#include "EngineStats.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "RenderingThread.h"
#include "RawIndexBuffer.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "SceneInterface.h"
#include "AI/Navigation/NavigationData.h"
#include "PrecomputedLightVolume.h"
#include "PrecomputedVolumetricLightmap.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Components/LightComponent.h"
#include "Model.h"
#include "Engine/Brush.h"
#include "Engine/Engine.h"
#include "Containers/TransArray.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/PropertyPortFlags.h"
#include "Misc/PackageName.h"
#include "GameFramework/PlayerController.h"
#include "Engine/NavigationObjectBase.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Texture2D.h"
#include "ContentStreaming.h"
#include "Engine/AssetUserData.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/WorldComposition.h"
#include "StaticLighting.h"
#include "TickTaskManagerInterface.h"
#include "UObject/ReleaseObjectVersion.h"
#include "PhysicsEngine/BodySetup.h"
#include "EngineGlobals.h"
#include "Engine/LevelBounds.h"
#if WITH_EDITOR
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif
#include "Engine/LevelStreaming.h"
#include "LevelUtils.h"
#include "Components/ModelComponent.h"
#include "Engine/LevelActorContainer.h"
#include "Engine/StaticMeshActor.h"

DEFINE_LOG_CATEGORY(LogLevel);

int32 GActorClusteringEnabled = 1;
static FAutoConsoleVariableRef CVarUseBackgroundLevelStreaming(
	TEXT("gc.ActorClusteringEnabled"),
	GActorClusteringEnabled,
	TEXT("Whether to allow levels to create actor clusters for GC."),
	ECVF_Default
);

/*-----------------------------------------------------------------------------
ULevel implementation.
-----------------------------------------------------------------------------*/


/** Called when a level package has been dirtied. */
FSimpleMulticastDelegate ULevel::LevelDirtiedEvent;

int32 FPrecomputedVisibilityHandler::NextId = 0;

/** Updates visibility stats. */
void FPrecomputedVisibilityHandler::UpdateVisibilityStats(bool bAllocating) const
{
	if (bAllocating)
	{
		INC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets.GetAllocatedSize());
		for (int32 BucketIndex = 0; BucketIndex < PrecomputedVisibilityCellBuckets.Num(); BucketIndex++)
		{
			INC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets[BucketIndex].Cells.GetAllocatedSize());
			INC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets[BucketIndex].CellDataChunks.GetAllocatedSize());
			for (int32 ChunkIndex = 0; ChunkIndex < PrecomputedVisibilityCellBuckets[BucketIndex].CellDataChunks.Num(); ChunkIndex++)
			{
				INC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets[BucketIndex].CellDataChunks[ChunkIndex].Data.GetAllocatedSize());
			}
		}
	}
	else
	{ //-V523, disabling identical branch warning because PVS-Studio does not understate the stat system in all configurations:
		DEC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets.GetAllocatedSize());
		for (int32 BucketIndex = 0; BucketIndex < PrecomputedVisibilityCellBuckets.Num(); BucketIndex++)
		{
			DEC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets[BucketIndex].Cells.GetAllocatedSize());
			DEC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets[BucketIndex].CellDataChunks.GetAllocatedSize());
			for (int32 ChunkIndex = 0; ChunkIndex < PrecomputedVisibilityCellBuckets[BucketIndex].CellDataChunks.Num(); ChunkIndex++)
			{
				DEC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets[BucketIndex].CellDataChunks[ChunkIndex].Data.GetAllocatedSize());
			}
		}
	}
}

/** Sets this visibility handler to be actively used by the rendering scene. */
void FPrecomputedVisibilityHandler::UpdateScene(FSceneInterface* Scene) const
{
	if (Scene && PrecomputedVisibilityCellBuckets.Num() > 0)
	{
		Scene->SetPrecomputedVisibility(this);
	}
}

/** Invalidates the level's precomputed visibility and frees any memory used by the handler. */
void FPrecomputedVisibilityHandler::Invalidate(FSceneInterface* Scene)
{
	Scene->SetPrecomputedVisibility(NULL);
	// Block until the renderer no longer references this FPrecomputedVisibilityHandler so we can delete its data
	FlushRenderingCommands();
	UpdateVisibilityStats(false);
	PrecomputedVisibilityCellBucketOriginXY = FVector2D(0,0);
	PrecomputedVisibilityCellSizeXY = 0;
	PrecomputedVisibilityCellSizeZ = 0;
	PrecomputedVisibilityCellBucketSizeXY = 0;
	PrecomputedVisibilityNumCellBuckets = 0;
	PrecomputedVisibilityCellBuckets.Empty();
	// Bump the Id so FSceneViewState will know to discard its cached visibility data
	Id = NextId;
	NextId++;
}

void FPrecomputedVisibilityHandler::ApplyWorldOffset(const FVector& InOffset)
{
	PrecomputedVisibilityCellBucketOriginXY-= FVector2D(InOffset.X, InOffset.Y);
	for (FPrecomputedVisibilityBucket& Bucket : PrecomputedVisibilityCellBuckets)
	{
		for (FPrecomputedVisibilityCell& Cell : Bucket.Cells)
		{
			Cell.Min+= InOffset;
		}
	}
}

FArchive& operator<<( FArchive& Ar, FPrecomputedVisibilityHandler& D )
{
	Ar << D.PrecomputedVisibilityCellBucketOriginXY;
	Ar << D.PrecomputedVisibilityCellSizeXY;
	Ar << D.PrecomputedVisibilityCellSizeZ;
	Ar << D.PrecomputedVisibilityCellBucketSizeXY;
	Ar << D.PrecomputedVisibilityNumCellBuckets;
	Ar << D.PrecomputedVisibilityCellBuckets;
	if (Ar.IsLoading())
	{
		D.UpdateVisibilityStats(true);
	}
	return Ar;
}


/** Sets this volume distance field to be actively used by the rendering scene. */
void FPrecomputedVolumeDistanceField::UpdateScene(FSceneInterface* Scene) const
{
	if (Scene && Data.Num() > 0)
	{
		Scene->SetPrecomputedVolumeDistanceField(this);
	}
}

/** Invalidates the level's volume distance field and frees any memory used by it. */
void FPrecomputedVolumeDistanceField::Invalidate(FSceneInterface* Scene)
{
	if (Scene && Data.Num() > 0)
	{
		Scene->SetPrecomputedVolumeDistanceField(NULL);
		// Block until the renderer no longer references this FPrecomputedVolumeDistanceField so we can delete its data
		FlushRenderingCommands();
		Data.Empty();
	}
}

FArchive& operator<<( FArchive& Ar, FPrecomputedVolumeDistanceField& D )
{
	Ar << D.VolumeMaxDistance;
	Ar << D.VolumeBox;
	Ar << D.VolumeSizeX;
	Ar << D.VolumeSizeY;
	Ar << D.VolumeSizeZ;
	Ar << D.Data;

	return Ar;
}

FLevelSimplificationDetails::FLevelSimplificationDetails()
 : bCreatePackagePerAsset(true)
 , DetailsPercentage(70.0f)
 , bOverrideLandscapeExportLOD(false)
 , LandscapeExportLOD(7)
 , bBakeFoliageToLandscape(false)
 , bBakeGrassToLandscape(false)
 , bGenerateMeshNormalMap_DEPRECATED(true)
 , bGenerateMeshMetallicMap_DEPRECATED(false)
 , bGenerateMeshRoughnessMap_DEPRECATED(false)
 , bGenerateMeshSpecularMap_DEPRECATED(false)
 , bGenerateLandscapeNormalMap_DEPRECATED(true)
 , bGenerateLandscapeMetallicMap_DEPRECATED(false)
 , bGenerateLandscapeRoughnessMap_DEPRECATED(false)
 , bGenerateLandscapeSpecularMap_DEPRECATED(false)
{
}

bool FLevelSimplificationDetails::operator == (const FLevelSimplificationDetails& Other) const
{
	return bCreatePackagePerAsset == Other.bCreatePackagePerAsset
		&& DetailsPercentage == Other.DetailsPercentage
		&& StaticMeshMaterialSettings == Other.StaticMeshMaterialSettings
		&& bOverrideLandscapeExportLOD == Other.bOverrideLandscapeExportLOD
		&& LandscapeExportLOD == Other.LandscapeExportLOD
		&& LandscapeMaterialSettings == Other.LandscapeMaterialSettings
		&& bBakeFoliageToLandscape == Other.bBakeFoliageToLandscape
		&& bBakeGrassToLandscape == Other.bBakeGrassToLandscape;
}

void FLevelSimplificationDetails::PostLoadDeprecated()
{
	FLevelSimplificationDetails DefaultObject;

}

TMap<FName, TWeakObjectPtr<UWorld> > ULevel::StreamedLevelsOwningWorld;

ULevel::ULevel( const FObjectInitializer& ObjectInitializer )
	:	UObject( ObjectInitializer )
	,	Actors()
	,	OwningWorld(NULL)
	,	TickTaskLevel(FTickTaskManagerInterface::Get().AllocateTickTaskLevel())
	,	PrecomputedLightVolume(new FPrecomputedLightVolume())
	,	PrecomputedVolumetricLightmap(new FPrecomputedVolumetricLightmap())
{
#if WITH_EDITORONLY_DATA
	LevelColor = FLinearColor::White;
	FixupOverrideVertexColorsTime = 0;
	FixupOverrideVertexColorsCount = 0;
#endif	
	bActorClusterCreated = false;
}

void ULevel::Initialize(const FURL& InURL)
{
	URL = InURL;
}

ULevel::~ULevel()
{
	FTickTaskManagerInterface::Get().FreeTickTaskLevel(TickTaskLevel);
	TickTaskLevel = NULL;
}


void ULevel::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	ULevel* This = CastChecked<ULevel>(InThis);

	// Let GC know that we're referencing some AActor objects
	if (FPlatformProperties::RequiresCookedData() && GActorClusteringEnabled && This->bActorClusterCreated)
	{
		Collector.AddReferencedObjects(This->ActorsForGC, This);
	}
	else
	{
		Collector.AddReferencedObjects(This->Actors, This);
	}

	Super::AddReferencedObjects( This, Collector );
}

void ULevel::PostInitProperties()
{
	Super::PostInitProperties();

	// Initialize LevelBuildDataId to something unique, in case this is a new ULevel
	LevelBuildDataId = FGuid::NewGuid();
}

void ULevel::Serialize( FArchive& Ar )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("ULevel::Serialize"), STAT_Level_Serialize, STATGROUP_LoadTime);

	Super::Serialize( Ar );

	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);
	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

	if (Ar.IsLoading() && Ar.CustomVer(FReleaseObjectVersion::GUID) < FReleaseObjectVersion::LevelTransArrayConvertedToTArray)
	{
		TTransArray<AActor*> OldActors(this);
		Ar << OldActors;
		Actors.Reserve(OldActors.Num());
		for (AActor* Actor : OldActors)
		{
			Actors.Push(Actor);
		}
	}
	else
	{
		Ar << Actors;
	}

	Ar << URL;

	Ar << Model;

	Ar << ModelComponents;

	if(!Ar.IsFilterEditorOnly() || (Ar.UE4Ver() < VER_UE4_EDITORONLY_BLUEPRINTS) )
	{
#if WITH_EDITORONLY_DATA
		// Skip serializing the LSBP if this is a world duplication for PIE/SIE, as it is not needed, and it causes overhead in startup times
		if( (Ar.GetPortFlags() & PPF_DuplicateForPIE ) == 0 )
		{
			Ar << LevelScriptBlueprint;
		}
		else
#endif //WITH_EDITORONLY_DATA
		{
			UObject* DummyBP = NULL;
			Ar << DummyBP;
		}
	}

	if( !Ar.IsTransacting() )
	{
		Ar << LevelScriptActor;
	}

	// Stop serializing deprecated classes with new versions
	if ( Ar.IsLoading() && Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::RemovedTextureStreamingLevelData )
	{
		// Strip for unsupported platforms
		TMap< UTexture2D*, TArray< FStreamableTextureInstance > >		Dummy0;
		TMap< UPrimitiveComponent*, TArray< FDynamicTextureInstance > >	Dummy1;
		bool Dummy2;
		Ar << Dummy0;
		Ar << Dummy1;
		Ar << Dummy2;

		//@todo legacy, useless
		if (Ar.IsLoading())
		{
			uint32 Size;
			Ar << Size;
			Ar.Seek(Ar.Tell() + Size);
		}
		else if (Ar.IsSaving())
		{
			uint32 Len = 0;
			Ar << Len;
		}

		if(Ar.UE4Ver() < VER_UE4_REMOVE_LEVELBODYSETUP)
		{
			UBodySetup* DummySetup;
			Ar << DummySetup;
		}

		TMap< UTexture2D*, bool > Dummy3;
		Ar << Dummy3;
	}

	// Mark archive and package as containing a map if we're serializing to disk.
	if( !HasAnyFlags( RF_ClassDefaultObject ) && Ar.IsPersistent() )
	{
		Ar.ThisContainsMap();
		GetOutermost()->ThisContainsMap();
	}

	// serialize the nav list
	Ar << NavListStart;
	Ar << NavListEnd;

	if (Ar.IsLoading() && Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::MapBuildDataSeparatePackage)
	{
		FPrecomputedLightVolumeData* LegacyData = new FPrecomputedLightVolumeData();
		Ar << (*LegacyData);

		FLevelLegacyMapBuildData LegacyLevelData;
		LegacyLevelData.Id = LevelBuildDataId;
		LegacyLevelData.Data = LegacyData;
		GLevelsWithLegacyBuildData.AddAnnotation(this, LegacyLevelData);
	}

	Ar << PrecomputedVisibilityHandler;
	Ar << PrecomputedVolumeDistanceField;

	if (Ar.UE4Ver() >= VER_UE4_WORLD_LEVEL_INFO &&
		Ar.UE4Ver() < VER_UE4_WORLD_LEVEL_INFO_UPDATED)
	{
		FWorldTileInfo Info;
		Ar << Info;
	}
}

bool ULevel::IsNetActor(const AActor* Actor)
{
	if (Actor == nullptr)
	{
		return false;
	}

	// If this is a server, use RemoteRole.
	// If this is a client, use Role.
	const ENetRole NetRole = (!Actor->IsNetMode(NM_Client)) ? Actor->GetRemoteRole() : (ENetRole)Actor->Role;

	// This test will return true on clients for actors with ROLE_Authority, which might be counterintuitive,
	// but clients will need to consider these actors in some cases, such as if their bTearOff flag is true.
	return NetRole > ROLE_None;
}

void ULevel::SortActorList()
{
	if (Actors.Num() == 0)
	{
		// No need to sort an empty list
		return;
	}

	TArray<AActor*> NewActors;
	TArray<AActor*> NewNetActors;
	NewActors.Reserve(Actors.Num());
	NewNetActors.Reserve(Actors.Num());

	check(WorldSettings);

	// The WorldSettings tries to stay at index 0
	NewActors.Add(WorldSettings);

	if (OwningWorld != nullptr)
	{
		OwningWorld->AddNetworkActor(WorldSettings);
	}

	// Add non-net actors to the NewActors immediately, cache off the net actors to Append after
	for (AActor* Actor : Actors)
	{
		if (Actor != nullptr && Actor != WorldSettings && !Actor->IsPendingKill())
		{
			if (IsNetActor(Actor))
			{
				NewNetActors.Add(Actor);
				if (OwningWorld != nullptr)
				{
					OwningWorld->AddNetworkActor(Actor);
				}
			}
			else
			{
				NewActors.Add(Actor);
			}
		}
	}

	NewActors.Append(MoveTemp(NewNetActors));

	// Replace with sorted list.
	Actors = MoveTemp(NewActors);
}


void ULevel::ValidateLightGUIDs()
{
	for( TObjectIterator<ULightComponent> It; It; ++It )
	{
		ULightComponent*	LightComponent	= *It;
		bool				IsInLevel		= LightComponent->IsIn( this );

		if( IsInLevel )
		{
			LightComponent->ValidateLightGUIDs();
		}
	}
}


void ULevel::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);

#if WITH_EDITOR
	if( !IsTemplate() )
	{
		UPackage* Package = GetOutermost();

		ValidateLightGUIDs();

		// Clear out any crosslevel references
		for( int32 ActorIdx = 0; ActorIdx < Actors.Num(); ActorIdx++ )
		{
			AActor *Actor = Actors[ActorIdx];
			if( Actor != NULL )
			{
				Actor->ClearCrossLevelReferences();
			}
		}

		// CheckTextureStreamingBuild(this);
	}
#endif // WITH_EDITOR
}


void ULevel::PostLoad()
{
	Super::PostLoad();

	// Ensure that the level is pointed to the owning world.  For streamed levels, this will be the world of the P map
	// they are streamed in to which we cached when the package loading was invoked
	OwningWorld = ULevel::StreamedLevelsOwningWorld.FindRef(GetOutermost()->GetFName()).Get();
	if (OwningWorld == NULL)
	{
		OwningWorld = CastChecked<UWorld>(GetOuter());
	}
	else
	{
		// This entry will not be used anymore, remove it
		ULevel::StreamedLevelsOwningWorld.Remove(GetOutermost()->GetFName());
	}

	UWorldComposition::OnLevelPostLoad(this);
		
#if WITH_EDITOR
	Actors.Remove(nullptr);
#endif

	if (WorldSettings == nullptr)
	{
		WorldSettings = Cast<AWorldSettings>(Actors[0]);
	}

	// in the Editor, sort Actor list immediately (at runtime we wait for the level to be added to the world so that it can be delayed in the level streaming case)
	if (GIsEditor)
	{
		SortActorList();
	}

	// Validate navigable geometry
	if (Model == NULL || Model->NumUniqueVertices == 0)
	{
		StaticNavigableGeometry.Empty();
	}

#if WITH_EDITOR
	if (!GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
	{
		// Rename the LevelScriptBlueprint after the outer world.
		UWorld* OuterWorld = Cast<UWorld>(GetOuter());
		if (LevelScriptBlueprint && OuterWorld && LevelScriptBlueprint->GetFName() != OuterWorld->GetFName())
		{
			// The level blueprint must be named the same as the level/world.
			// If there is already something there with that name, rename it to something else.
			if (UObject* ExistingObject = StaticFindObject(nullptr, LevelScriptBlueprint->GetOuter(), *OuterWorld->GetName()))
			{
				ExistingObject->Rename(nullptr, nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
			}

			// Use LevelScriptBlueprint->GetOuter() instead of NULL to make sure the generated top level objects are moved appropriately
			LevelScriptBlueprint->Rename(*OuterWorld->GetName(), LevelScriptBlueprint->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional | REN_SkipGeneratedClasses);
		}
	}

	// Fixup deprecated stuff in levels simplification settings
	for (int32 Index = 0; Index < ARRAY_COUNT(LevelSimplification); ++Index)
	{
		LevelSimplification[Index].PostLoadDeprecated();
	}

	if (LevelScriptActor)
	{
		if (ULevelScriptBlueprint* LevelBlueprint = Cast<ULevelScriptBlueprint>(LevelScriptActor->GetClass()->ClassGeneratedBy))
		{
			FBlueprintEditorUtils::FixLevelScriptActorBindings(LevelScriptActor, LevelBlueprint);
		}
	}
#endif
}

bool ULevel::CanBeClusterRoot() const
{
	// We don't want to create the cluster for levels in the same place as other clusters (after PostLoad)
	// because at this point some of the assets referenced by levels may still haven't created clusters themselves.
	return false;
}

void ULevel::CreateCluster()
{
	// ULevels are not cluster roots themselves, instead they create a special actor container
	// that holds a reference to all actors that are to be clustered. This is because only
	// specific actor types can be clustered so the remaining actors that are not clustered
	// need to be referenced through the level.
	// Also, we don't want the level to reference the actors that are clusters because that would
	// make things work even slower (references to clustered objects are expensive). That's why
	// we keep a separate array for referencing unclustered actors (ActorsForGC).
	if (FPlatformProperties::RequiresCookedData() && GActorClusteringEnabled && !bActorClusterCreated)
	{
		TArray<AActor*> ClusterActors;

		for (int32 ActorIndex = Actors.Num() - 1; ActorIndex >= 0; --ActorIndex)
		{
			AActor* Actor = Actors[ActorIndex];
			if (Actor && Actor->CanBeInCluster())
			{
				ClusterActors.Add(Actor);
			}
			else
			{
				ActorsForGC.Add(Actor);
			}
		}
		if (ClusterActors.Num())
		{
			ActorCluster = NewObject<ULevelActorContainer>(this, TEXT("ActorCluster"), RF_Transient);
			ActorCluster->Actors = MoveTemp(ClusterActors);
			ActorCluster->CreateCluster();
		}
		bActorClusterCreated = true;
	}
}

void ULevel::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	bWasDuplicatedForPIE = bDuplicateForPIE;
}

UWorld* ULevel::GetWorld() const
{
	return OwningWorld;
}

void ULevel::ClearLevelComponents()
{
	bAreComponentsCurrentlyRegistered = false;

	// Remove the model components from the scene.
	for (UModelComponent* ModelComponent : ModelComponents)
	{
		if (ModelComponent && ModelComponent->IsRegistered())
		{
			ModelComponent->UnregisterComponent();
		}
	}

	// Remove the actors' components from the scene and build a list of relevant worlds
	// In theory (though it is a terrible idea), users could spawn Actors from an OnUnregister event so don't use ranged-for
	for (int32 ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex)
	{
		AActor* Actor = Actors[ActorIndex];
		if (Actor)
		{
			Actor->UnregisterAllComponents();
		}
	}

	if (IsPersistentLevel())
	{
		FSceneInterface* WorldScene = GetWorld()->Scene;
		if (WorldScene)
		{
			WorldScene->SetClearMotionBlurInfoGameThread();
		}
	}
}

void ULevel::BeginDestroy()
{
	if (!IStreamingManager::HasShutdown())
	{
		// At this time, referenced UTexture2Ds are still in memory.
		IStreamingManager::Get().RemoveLevel( this );
	}

	Super::BeginDestroy();

	// Remove this level from its OwningWorld's collection
	if (CachedLevelCollection)
	{
		CachedLevelCollection->RemoveLevel(this);
	}

	if (OwningWorld && IsPersistentLevel() && OwningWorld->Scene)
	{
		OwningWorld->Scene->SetPrecomputedVisibility(NULL);
		OwningWorld->Scene->SetPrecomputedVolumeDistanceField(NULL);
	}

	ReleaseRenderingResources();

	RemoveFromSceneFence.BeginFence();
}

bool ULevel::IsReadyForFinishDestroy()
{
	const bool bReady = Super::IsReadyForFinishDestroy();
	return bReady && RemoveFromSceneFence.IsFenceComplete();
}

void ULevel::FinishDestroy()
{
	delete PrecomputedLightVolume;
	PrecomputedLightVolume = NULL;

	delete PrecomputedVolumetricLightmap;
	PrecomputedVolumetricLightmap = NULL;

	Super::FinishDestroy();
}

/**
* A TMap key type used to sort BSP nodes by locality and zone.
*/
struct FModelComponentKey
{
	uint32	X;
	uint32	Y;
	uint32	Z;
	uint32	MaskedPolyFlags;

	friend bool operator==(const FModelComponentKey& A,const FModelComponentKey& B)
	{
		return	A.X == B.X 
			&&	A.Y == B.Y 
			&&	A.Z == B.Z 
			&&	A.MaskedPolyFlags == B.MaskedPolyFlags;
	}

	friend uint32 GetTypeHash(const FModelComponentKey& Key)
	{
		return FCrc::MemCrc_DEPRECATED(&Key,sizeof(Key));
	}
};

void ULevel::UpdateLevelComponents(bool bRerunConstructionScripts)
{
	// Update all components in one swoop.
	IncrementalUpdateComponents( 0, bRerunConstructionScripts );
}

namespace FLevelSortUtils
{
	void AddToListSafe(AActor* TestActor, TArray<AActor*>& List)
	{
		if (TestActor)
		{
			const bool bAlreadyAdded = List.Contains(TestActor);
			if (bAlreadyAdded)
			{
				FString ListItemDesc;
				for (int32 Idx = 0; Idx < List.Num(); Idx++)
				{
					if (Idx > 0)
					{
						ListItemDesc += TEXT(", ");
					}

					ListItemDesc += GetNameSafe(List[Idx]);
				}

				UE_LOG(LogLevel, Warning, TEXT("Found a cycle in actor's parent chain: %s"), *ListItemDesc);
			}
			else
			{
				List.Add(TestActor);
			}
		}
	}

	// Finds list of parents from an entry in ParentMap, returns them in provided array and removes from map
	// Logs an error when cycle is found
	void FindAndRemoveParentChain(TMap<AActor*, AActor*>& ParentMap, TArray<AActor*>& ParentChain)
	{
		check(ParentMap.Num());
		
		// seed from first entry
		TMap<AActor*, AActor*>::TIterator It(ParentMap);
		ParentChain.Add(It.Key());
		ParentChain.Add(It.Value());
		It.RemoveCurrent();

		// fill chain's parent nodes
		bool bLoop = true;
		while (bLoop)
		{
			AActor* MapValue = nullptr;
			bLoop = ParentMap.RemoveAndCopyValue(ParentChain.Last(), MapValue);
			AddToListSafe(MapValue, ParentChain);
		}

		// find chain's child nodes, ignore cycle detection since it would've triggered already from previous loop
		for (AActor* const* MapKey = ParentMap.FindKey(ParentChain[0]); MapKey; MapKey = ParentMap.FindKey(ParentChain[0]))
		{
			AActor* MapValue = nullptr;
			ParentMap.RemoveAndCopyValue((AActor*)MapKey, MapValue);
			ParentChain.Insert(MapValue, 0);
		}
	}

	struct FDepthSort
	{
		TMap<AActor*, int32> DepthMap;

		bool operator()(AActor* A, AActor* B) const
		{
			const int32 DepthA = A ? DepthMap.FindRef(A) : MAX_int32;
			const int32 DepthB = B ? DepthMap.FindRef(B) : MAX_int32;
			return DepthA < DepthB;
		}
	};
}

/**
*	Sorts actors such that parent actors will appear before children actors in the list
*	Stable sort
*/
static void SortActorsHierarchy(TArray<AActor*>& Actors, UObject* Level)
{
	const double StartTime = FPlatformTime::Seconds();

	// Precalculate parent map to avoid processing cycles during sort
	TMap<AActor*, AActor*> ParentMap;
	for (int32 Idx = 0; Idx < Actors.Num(); Idx++)
	{
		if (Actors[Idx])
		{
			AActor* ParentActor = Actors[Idx]->GetAttachParentActor();
			if (ParentActor)
			{
				ParentMap.Add(Actors[Idx], ParentActor);
			}
		}
	}

	if (ParentMap.Num())
	{
		FLevelSortUtils::FDepthSort DepthSorter;
		TArray<AActor*> ParentChain;
		while (ParentMap.Num())
		{
			ParentChain.Reset();
			FLevelSortUtils::FindAndRemoveParentChain(ParentMap, ParentChain);

			for (int32 Idx = 0; Idx < ParentChain.Num(); Idx++)
			{
				DepthSorter.DepthMap.Add(ParentChain[Idx], ParentChain.Num() - Idx - 1);
			}
		}

		// Unfortunately TArray.StableSort assumes no null entries in the array
		// So it forces me to use internal unrestricted version
		StableSortInternal(Actors.GetData(), Actors.Num(), DepthSorter);
	}

	const float ElapsedTime = (float)(FPlatformTime::Seconds() - StartTime);
	if (ElapsedTime > 1.0f)
	{
		UE_LOG(LogLevel, Warning, TEXT("SortActorsHierarchy(%s) took %f seconds"), Level ? *GetNameSafe(Level->GetOutermost()) : TEXT("??"), ElapsedTime);
	}

	// Since all the null entries got sorted to the end, lop them off right now
	int32 RemoveAtIndex = Actors.Num();
	while (RemoveAtIndex > 0 && Actors[RemoveAtIndex - 1] == nullptr)
	{
		--RemoveAtIndex;
	}

	if (RemoveAtIndex < Actors.Num())
	{
		Actors.RemoveAt(RemoveAtIndex, Actors.Num() - RemoveAtIndex);
	}
}

void ULevel::IncrementalUpdateComponents(int32 NumComponentsToUpdate, bool bRerunConstructionScripts)
{
	// A value of 0 means that we want to update all components.
	if (NumComponentsToUpdate != 0)
	{
		// Only the game can use incremental update functionality.
		checkf(OwningWorld->IsGameWorld(), TEXT("Cannot call IncrementalUpdateComponents with non 0 argument in the Editor/ commandlets."));
	}

	// Do BSP on the first pass.
	if (CurrentActorIndexForUpdateComponents == 0)
	{
		UpdateModelComponents();
		// Sort actors to ensure that parent actors will be registered before child actors
		SortActorsHierarchy(Actors, this);
	}

	// Find next valid actor to process components registration
	while (CurrentActorIndexForUpdateComponents < Actors.Num())
	{
		AActor* Actor = Actors[CurrentActorIndexForUpdateComponents];
		bool bAllComponentsRegistered = true;
		if (Actor && !Actor->IsPendingKill())
		{
#if PERF_TRACK_DETAILED_ASYNC_STATS
			FScopeCycleCounterUObject ContextScope(Actor);
#endif
			bAllComponentsRegistered = Actor->IncrementalRegisterComponents(NumComponentsToUpdate);
		}

		if (bAllComponentsRegistered)
		{	
			// All components have been registered fro this actor, move to a next one
			CurrentActorIndexForUpdateComponents++;
		}

		// If we do an incremental registration return to outer loop after each processed actor 
		// so outer loop can decide whether we want to continue processing this frame
		if (NumComponentsToUpdate != 0)
		{
			break;
		}
	}

	// See whether we are done.
	if (CurrentActorIndexForUpdateComponents == Actors.Num())
	{
		CurrentActorIndexForUpdateComponents	= 0;
		bAreComponentsCurrentlyRegistered		= true;
		
		CreateCluster();

#if PERF_TRACK_DETAILED_ASYNC_STATS
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ULevel_IncrementalUpdateComponents_RerunConstructionScripts);
#endif
		if (bRerunConstructionScripts && !IsTemplate() && !GIsUCCMakeStandaloneHeaderGenerator)
		{
			// Don't rerun construction scripts until after all actors' components have been registered.  This
			// is necessary because child attachment lists are populated during registration, and running construction
			// scripts requires that the attachments are correctly initialized.
			// Don't use ranged for as construction scripts can manipulate the actor array
			for (int32 ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex)
			{
				if (AActor* Actor = Actors[ActorIndex])
				{
					// Child actors have already been built and initialized up by their parent and they should not be reconstructed again
					if (!Actor->IsChildActor())
					{
#if PERF_TRACK_DETAILED_ASYNC_STATS
						FScopeCycleCounterUObject ContextScope(Actor);
#endif
						Actor->RerunConstructionScripts();
					}
				}
			}
			bHasRerunConstructionScripts = true;
		}
	}
	// Only the game can use incremental update functionality.
	else
	{
		// The editor is never allowed to incrementally updated components.  Make sure to pass in a value of zero for NumActorsToUpdate.
		check(OwningWorld->IsGameWorld());
	}
}


bool ULevel::IncrementalUnregisterComponents(int32 NumComponentsToUnregister)
{
	// A value of 0 means that we want to unregister all components.
	if (NumComponentsToUnregister != 0)
	{
		// Only the game can use incremental update functionality.
		checkf(OwningWorld->IsGameWorld(), TEXT("Cannot call IncrementalUnregisterComponents with non 0 argument in the Editor/ commandlets."));
	}

	// Find next valid actor to process components unregistration
	int32 NumComponentsUnregistered = 0;
	while (CurrentActorIndexForUnregisterComponents < Actors.Num())
	{
		AActor* Actor = Actors[CurrentActorIndexForUnregisterComponents];
		if (Actor)
		{
			int32 NumComponents = Actor->GetComponents().Num();
			NumComponentsUnregistered += NumComponents;
			Actor->UnregisterAllComponents();
		}
		CurrentActorIndexForUnregisterComponents++;
		if (NumComponentsUnregistered > NumComponentsToUnregister )
		{
			break;
		}
	}

	if (CurrentActorIndexForUnregisterComponents == Actors.Num())
	{
		CurrentActorIndexForUnregisterComponents = 0;
		return true;
	}
	return false;
}

#if WITH_EDITOR

void ULevel::MarkLevelComponentsRenderStateDirty()
{
	for (UModelComponent* ModelComponent : ModelComponents)
	{
		if (ModelComponent)
		{
			ModelComponent->MarkRenderStateDirty();
		}
	}

	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->MarkComponentsRenderStateDirty();
		}
	}
}

void ULevel::CreateModelComponents()
{
	FScopedSlowTask SlowTask(10);
	SlowTask.MakeDialogDelayed(3.0f);

	SlowTask.EnterProgressFrame(4);

	// Update the model vertices and edges.
	Model->UpdateVertices();

	Model->InvalidSurfaces = 0;

	// Clear the model index buffers.
	Model->MaterialIndexBuffers.Empty();

	struct FNodeIndices
	{
		TArray<uint16> Nodes;
		TSet<uint16> UniqueNodes;

		FNodeIndices()
		{
			Nodes.Reserve(16);
			UniqueNodes.Reserve(16);
		}

		void AddUnique(uint16 Index)
		{
			if (!UniqueNodes.Contains(Index))
			{
				Nodes.Add(Index);
				UniqueNodes.Add(Index);
			}
		}
	};

	TMap< FModelComponentKey, FNodeIndices > ModelComponentMap;

	{
		FScopedSlowTask InnerTask(Model->Nodes.Num());
		InnerTask.MakeDialogDelayed(3.0f);

		// Sort the nodes by zone, grid cell and masked poly flags.
		for (int32 NodeIndex = 0; NodeIndex < Model->Nodes.Num(); NodeIndex++)
		{
			InnerTask.EnterProgressFrame(1);

			FBspNode& Node = Model->Nodes[NodeIndex];
			FBspSurf& Surf = Model->Surfs[Node.iSurf];

			if (Node.NumVertices > 0)
			{
				// Calculate the bounding box of this node.
				FBox NodeBounds(ForceInit);
				for (int32 VertexIndex = 0; VertexIndex < Node.NumVertices; VertexIndex++)
				{
					NodeBounds += Model->Points[Model->Verts[Node.iVertPool + VertexIndex].pVertex];
				}

				// Create a sort key for this node using the grid cell containing the center of the node's bounding box.
#define MODEL_GRID_SIZE_XY	2048.0f
#define MODEL_GRID_SIZE_Z	4096.0f
				FModelComponentKey Key;
				check(OwningWorld);
				if (OwningWorld->GetWorldSettings()->bMinimizeBSPSections)
				{
					Key.X = 0;
					Key.Y = 0;
					Key.Z = 0;
				}
				else
				{
					Key.X = FMath::FloorToInt(NodeBounds.GetCenter().X / MODEL_GRID_SIZE_XY);
					Key.Y = FMath::FloorToInt(NodeBounds.GetCenter().Y / MODEL_GRID_SIZE_XY);
					Key.Z = FMath::FloorToInt(NodeBounds.GetCenter().Z / MODEL_GRID_SIZE_Z);
				}

				Key.MaskedPolyFlags = 0;

				// Find an existing node list for the grid cell.
				FNodeIndices* ComponentNodes = ModelComponentMap.Find(Key);
				if (!ComponentNodes)
				{
					// This is the first node we found in this grid cell, create a new node list for the grid cell.
					ComponentNodes = &ModelComponentMap.Add(Key);
				}

				// Add the node to the grid cell's node list.
				ComponentNodes->AddUnique(NodeIndex);
			}
			else
			{
				// Put it in component 0 until a rebuild occurs.
				Node.ComponentIndex = 0;
			}
		}
	}

	// Create a UModelComponent for each grid cell's node list.
	for (TMap< FModelComponentKey, FNodeIndices >::TConstIterator It(ModelComponentMap); It; ++It)
	{
		const FModelComponentKey&		Key		= It.Key();
		const TArray<uint16>&			Nodes	= It.Value().Nodes;

		for(int32 NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
		{
			Model->Nodes[Nodes[NodeIndex]].ComponentIndex = ModelComponents.Num();							
			Model->Nodes[Nodes[NodeIndex]].ComponentNodeIndex = NodeIndex;
		}

		UModelComponent* ModelComponent = NewObject<UModelComponent>(this);
		ModelComponent->InitializeModelComponent(Model, ModelComponents.Num(), Key.MaskedPolyFlags, Nodes);
		ModelComponents.Add(ModelComponent);

		for(int32 NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
		{
			Model->Nodes[Nodes[NodeIndex]].ComponentElementIndex = INDEX_NONE;

			const uint16								Node	 = Nodes[NodeIndex];
			const TIndirectArray<FModelElement>&	Elements = ModelComponent->GetElements();
			for( int32 ElementIndex=0; ElementIndex<Elements.Num(); ElementIndex++ )
			{
				if( Elements[ElementIndex].Nodes.Find( Node ) != INDEX_NONE )
				{
					Model->Nodes[Nodes[NodeIndex]].ComponentElementIndex = ElementIndex;
					break;
				}
			}
		}
	}

	// Clear old cached data in case we don't regenerate it below, e.g. after removing all BSP from a level.
	Model->NumIncompleteNodeGroups = 0;
	Model->CachedMappings.Empty();

	SlowTask.EnterProgressFrame(4);

	// Work only needed if we actually have BSP in the level.
	if( ModelComponents.Num() )
	{
		check( OwningWorld );
		// Build the static lighting vertices!
		/** The lights in the world which the system is building. */
		TArray<ULightComponentBase*> Lights;
		// Prepare lights for rebuild.
		for(TObjectIterator<ULightComponent> LightIt;LightIt;++LightIt)
		{
			ULightComponent* const Light = *LightIt;
			const bool bLightIsInWorld = Light->GetOwner() && OwningWorld->ContainsActor(Light->GetOwner()) && !Light->GetOwner()->IsPendingKill();
			if (bLightIsInWorld && (Light->HasStaticLighting() || Light->HasStaticShadowing()))
			{
				// Make sure the light GUIDs and volumes are up-to-date.
				Light->ValidateLightGUIDs();

				// Add the light to the system's list of lights in the world.
				Lights.Add(Light);
			}
		}

		// For BSP, we aren't Component-centric, so we can't use the GetStaticLightingInfo 
		// function effectively. Instead, we look across all nodes in the Level's model and
		// generate NodeGroups - which are groups of nodes that are coplanar, adjacent, and 
		// have the same lightmap resolution (henceforth known as being "conodes"). Each 
		// NodeGroup will get a mapping created for it

		// create all NodeGroups
		Model->GroupAllNodes(this, Lights);

		// now we need to make the mappings/meshes
		for (TMap<int32, FNodeGroup*>::TIterator It(Model->NodeGroups); It; ++It)
		{
			FNodeGroup* NodeGroup = It.Value();

			if (NodeGroup->Nodes.Num())
			{
				// get one of the surfaces/components from the NodeGroup
				// @todo UE4: Remove need for GetSurfaceLightMapResolution to take a surfaceindex, or a ModelComponent :)
				UModelComponent* SomeModelComponent = ModelComponents[Model->Nodes[NodeGroup->Nodes[0]].ComponentIndex];
				int32 SurfaceIndex = Model->Nodes[NodeGroup->Nodes[0]].iSurf;

				// fill out the NodeGroup/mapping, as UModelComponent::GetStaticLightingInfo did
				SomeModelComponent->GetSurfaceLightMapResolution(SurfaceIndex, true, NodeGroup->SizeX, NodeGroup->SizeY, NodeGroup->WorldToMap, &NodeGroup->Nodes);
				NodeGroup->MapToWorld = NodeGroup->WorldToMap.InverseFast();

				// Cache the surface's vertices and triangles.
				NodeGroup->BoundingBox.Init();

				for(int32 NodeIndex = 0;NodeIndex < NodeGroup->Nodes.Num();NodeIndex++)
				{
					const FBspNode& Node = Model->Nodes[NodeGroup->Nodes[NodeIndex]];
					const FBspSurf& NodeSurf = Model->Surfs[Node.iSurf];
					const FVector& TextureBase = Model->Points[NodeSurf.pBase];
					const FVector& TextureX = Model->Vectors[NodeSurf.vTextureU];
					const FVector& TextureY = Model->Vectors[NodeSurf.vTextureV];
					const int32 BaseVertexIndex = NodeGroup->Vertices.Num();
					// Compute the surface's tangent basis.
					FVector NodeTangentX = Model->Vectors[NodeSurf.vTextureU].GetSafeNormal();
					FVector NodeTangentY = Model->Vectors[NodeSurf.vTextureV].GetSafeNormal();
					FVector NodeTangentZ = Model->Vectors[NodeSurf.vNormal].GetSafeNormal();

					// Generate the node's vertices.
					for(uint32 VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
					{
						/*const*/ FVert& Vert = Model->Verts[Node.iVertPool + VertexIndex];
						const FVector& VertexWorldPosition = Model->Points[Vert.pVertex];

						FStaticLightingVertex* DestVertex = new(NodeGroup->Vertices) FStaticLightingVertex;
						DestVertex->WorldPosition = VertexWorldPosition;
						DestVertex->TextureCoordinates[0].X = ((VertexWorldPosition - TextureBase) | TextureX) / 128.0f;
						DestVertex->TextureCoordinates[0].Y = ((VertexWorldPosition - TextureBase) | TextureY) / 128.0f;
						DestVertex->TextureCoordinates[1].X = NodeGroup->WorldToMap.TransformPosition(VertexWorldPosition).X;
						DestVertex->TextureCoordinates[1].Y = NodeGroup->WorldToMap.TransformPosition(VertexWorldPosition).Y;
						DestVertex->WorldTangentX = NodeTangentX;
						DestVertex->WorldTangentY = NodeTangentY;
						DestVertex->WorldTangentZ = NodeTangentZ;

						// TEMP - Will be overridden when lighting is build!
						Vert.ShadowTexCoord = DestVertex->TextureCoordinates[1];

						// Include the vertex in the surface's bounding box.
						NodeGroup->BoundingBox += VertexWorldPosition;
					}

					// Generate the node's vertex indices.
					for(uint32 VertexIndex = 2;VertexIndex < Node.NumVertices;VertexIndex++)
					{
						NodeGroup->TriangleVertexIndices.Add(BaseVertexIndex + 0);
						NodeGroup->TriangleVertexIndices.Add(BaseVertexIndex + VertexIndex);
						NodeGroup->TriangleVertexIndices.Add(BaseVertexIndex + VertexIndex - 1);

						// track the source surface for each triangle
						NodeGroup->TriangleSurfaceMap.Add(Node.iSurf);
					}
				}
			}
		}
	}
	Model->UpdateVertices();

	SlowTask.EnterProgressFrame(2);

	for (int32 UpdateCompIdx = 0; UpdateCompIdx < ModelComponents.Num(); UpdateCompIdx++)
	{
		UModelComponent* ModelComp = ModelComponents[UpdateCompIdx];
		ModelComp->GenerateElements(true);
		ModelComp->InvalidateCollisionData();
	}
}
#endif

void ULevel::UpdateModelComponents()
{
	// Create/update the level's BSP model components.
	if(!ModelComponents.Num())
	{
#if WITH_EDITOR
		CreateModelComponents();
#endif // WITH_EDITOR
	}
	else
	{
		for(int32 ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
		{
			if(ModelComponents[ComponentIndex] && ModelComponents[ComponentIndex]->IsRegistered())
			{
				ModelComponents[ComponentIndex]->UnregisterComponent();
			}
		}
	}

	if (ModelComponents.Num() > 0)
	{
		check( OwningWorld );
		// Update model components.
		for(int32 ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
		{
			if(ModelComponents[ComponentIndex])
			{
				ModelComponents[ComponentIndex]->RegisterComponentWithWorld(OwningWorld);
			}
		}
	}

	// Initialize the model's index buffers.
	for(TMap<UMaterialInterface*,TUniquePtr<FRawIndexBuffer16or32> >::TIterator IndexBufferIt(Model->MaterialIndexBuffers);
		IndexBufferIt;
		++IndexBufferIt)
	{
		BeginInitResource(IndexBufferIt->Value.Get());
	}

	// Can now release the model's vertex buffer, will have been used for collision
	if(!IsRunningCommandlet())
	{
		Model->ReleaseVertices();
	}

	Model->bInvalidForStaticLighting = true;
}

#if WITH_EDITOR
void ULevel::PreEditUndo()
{
	Super::PreEditUndo();

	// Release the model's resources.
	Model->BeginReleaseResources();
	Model->ReleaseResourcesFence.Wait();

	// Detach existing model components.  These are left in the array, so they are saved for undoing the undo.
	for(int32 ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
	{
		if(ModelComponents[ComponentIndex])
		{
			ModelComponents[ComponentIndex]->UnregisterComponent();
		}
	}

	ReleaseRenderingResources();

	// Wait for the components to be detached.
	FlushRenderingCommands();

	ABrush::GGeometryRebuildCause = TEXT("Undo");
}


void ULevel::PostEditUndo()
{
	Super::PostEditUndo();

	Model->UpdateVertices();
	// Update model components that were detached earlier
	UpdateModelComponents();

	ABrush::GGeometryRebuildCause = nullptr;

	// If it's a streaming level and was not visible, don't init rendering resources
	if (OwningWorld)
	{
		bool bIsStreamingLevelVisible = false;
		if (OwningWorld->PersistentLevel == this)
		{
			bIsStreamingLevelVisible = FLevelUtils::IsLevelVisible(OwningWorld->PersistentLevel);
		}
		else
		{
			const int32 NumStreamingLevels = OwningWorld->StreamingLevels.Num();
			for (int i = 0; i < NumStreamingLevels; ++i)
			{
				const ULevelStreaming* StreamedLevel = OwningWorld->StreamingLevels[i];
				if (StreamedLevel && StreamedLevel->GetLoadedLevel() == this)
				{
					bIsStreamingLevelVisible = FLevelUtils::IsLevelVisible(StreamedLevel);
					break;
				}
			}
		}

		if (bIsStreamingLevelVisible)
		{
			InitializeRenderingResources();
		}
	}

	// Non-transactional actors may disappear from the actors list but still exist, so we need to re-add them
	// Likewise they won't get recreated if we undo to before they were deleted, so we'll have nulls in the actors list to remove
	TSet<AActor*> ActorsSet(Actors);
	ForEachObjectWithOuter(this, [&ActorsSet, this](UObject* InnerObject)
	{
		AActor* InnerActor = Cast<AActor>(InnerObject);
		if (InnerActor && !ActorsSet.Contains(InnerActor))
		{
			Actors.Add(InnerActor);
		}
	}, /*bIncludeNestedObjects*/ false, /*ExclusionFlags*/ RF_NoFlags, /* InternalExclusionFlags */ EInternalObjectFlags::PendingKill);

	MarkLevelBoundsDirty();
}

void ULevel::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
	const FString PropertyName = PropertyThatChanged ? PropertyThatChanged->GetName() : TEXT("");

	if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULevel, MapBuildData))
	{
		// MapBuildData is not editable but can be modified by the editor's Force Delete
		ReleaseRenderingResources();
		InitializeRenderingResources();
	}
} 

#endif // WITH_EDITOR

void ULevel::MarkLevelBoundsDirty()
{
#if WITH_EDITOR
	if (LevelBoundsActor.IsValid())
	{
		LevelBoundsActor->MarkLevelBoundsDirty();
	}
#endif// WITH_EDITOR
}

void ULevel::InvalidateModelGeometry()
{
	// Save the level/model state for transactions.
	Model->Modify();
	Modify();

	// Begin releasing the model's resources.
	Model->BeginReleaseResources();

	// Remove existing model components.
	for(int32 ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
	{
		if(ModelComponents[ComponentIndex])
		{
			ModelComponents[ComponentIndex]->Modify();
			ModelComponents[ComponentIndex]->UnregisterComponent();
		}
	}
	ModelComponents.Empty();
}


void ULevel::InvalidateModelSurface()
{
	Model->InvalidSurfaces = true;
}

void ULevel::CommitModelSurfaces()
{
	if(Model->InvalidSurfaces)
	{
		if (!Model->bOnlyRebuildMaterialIndexBuffers)
		{
			// Unregister model components
			for (int32 ComponentIndex = 0; ComponentIndex < ModelComponents.Num(); ComponentIndex++)
			{
				if (ModelComponents[ComponentIndex] && ModelComponents[ComponentIndex]->IsRegistered())
				{
					ModelComponents[ComponentIndex]->UnregisterComponent();
				}
			}
		}

		// Begin releasing the model's resources.
		Model->BeginReleaseResources();

		// Wait for the model's resources to be released.
		FlushRenderingCommands();

		// Clear the model index buffers.
		Model->MaterialIndexBuffers.Empty();

		// Update the model vertices.
		Model->UpdateVertices();

		// Update the model components.
		for(int32 ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
		{
			if(ModelComponents[ComponentIndex])
			{
				ModelComponents[ComponentIndex]->CommitSurfaces();
			}
		}
		Model->InvalidSurfaces = false;

		// Register model components before init'ing index buffer so collision has access to index buffer data
		// This matches the order of operation in ULevel::UpdateModelComponents
		if (ModelComponents.Num() > 0)
		{
			check( OwningWorld );
			// Update model components.
			for(int32 ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
			{
				if(ModelComponents[ComponentIndex])
				{
					if (Model->bOnlyRebuildMaterialIndexBuffers)
					{
						ModelComponents[ComponentIndex]->MarkRenderStateDirty();
					}
					else
					{
						ModelComponents[ComponentIndex]->RegisterComponentWithWorld(OwningWorld);
					}
				}
			}
		}

		// Initialize the model's index buffers.
		for(TMap<UMaterialInterface*,TUniquePtr<FRawIndexBuffer16or32> >::TIterator IndexBufferIt(Model->MaterialIndexBuffers);
			IndexBufferIt;
			++IndexBufferIt)
		{
			BeginInitResource(IndexBufferIt->Value.Get());
		}

		Model->bOnlyRebuildMaterialIndexBuffers = false;
	}
}


void ULevel::BuildStreamingData(UWorld* World, ULevel* TargetLevel/*=NULL*/, UTexture2D* UpdateSpecificTextureOnly/*=NULL*/)
{
#if WITH_EDITORONLY_DATA
	double StartTime = FPlatformTime::Seconds();


	TArray<ULevel* > LevelsToCheck;
	if ( TargetLevel )
	{
		LevelsToCheck.Add(TargetLevel);
	}
	else if ( World )
	{
		for ( int32 LevelIndex=0; LevelIndex < World->GetNumLevels(); LevelIndex++ )
		{
			ULevel* Level = World->GetLevel(LevelIndex);
			LevelsToCheck.Add(Level);
		}
	}
	else
	{
		for (TObjectIterator<ULevel> It; It; ++It)
		{
			ULevel* Level = *It;
			LevelsToCheck.Add(Level);
		}
	}

	for ( int32 LevelIndex=0; LevelIndex < LevelsToCheck.Num(); LevelIndex++ )
	{
		ULevel* Level = LevelsToCheck[LevelIndex];
		if (!Level) continue;

		if (Level->bIsVisible || Level->IsPersistentLevel())
		{
			IStreamingManager::Get().AddLevel(Level);
		}
		//@todo : handle UpdateSpecificTextureOnly
	}

	UE_LOG(LogLevel, Verbose, TEXT("ULevel::BuildStreamingData took %.3f seconds."), FPlatformTime::Seconds() - StartTime);
#else
	UE_LOG(LogLevel, Fatal,TEXT("ULevel::BuildStreamingData should not be called on a console"));
#endif
}

ABrush* ULevel::GetDefaultBrush() const
{
	ABrush* DefaultBrush = nullptr;
	if (Actors.Num() >= 2)
	{
		// If the builder brush exists then it will be the 2nd actor in the actors array.
		DefaultBrush = Cast<ABrush>(Actors[1]);
		// If the second actor is not a brush then it certainly cannot be the builder brush.
		if (DefaultBrush != nullptr)
		{
			checkf(DefaultBrush->GetBrushComponent(), *GetPathName());
			checkf(DefaultBrush->Brush != nullptr, *GetPathName());
		}
	}
	return DefaultBrush;
}


AWorldSettings* ULevel::GetWorldSettings(bool bChecked) const
{
	if (bChecked)
	{
		checkf( WorldSettings != nullptr, *GetPathName() );
	}
	return WorldSettings;
}

void ULevel::SetWorldSettings(AWorldSettings* NewWorldSettings)
{
	check(NewWorldSettings); // Doesn't make sense to be clearing a world settings object
	if (WorldSettings != NewWorldSettings)
	{
		// We'll generally endeavor to keep the world settings at its traditional index 0
		const int32 NewWorldSettingsIndex = Actors.FindLast( NewWorldSettings );
		if (NewWorldSettingsIndex != 0)
		{
			if (Actors[0] == nullptr || Actors[0]->IsA<AWorldSettings>())
			{
				Exchange(Actors[0],Actors[NewWorldSettingsIndex]);
			}
			else
			{
				Actors[NewWorldSettingsIndex] = nullptr;
				Actors.Insert(NewWorldSettings,0);
			}
		}

		if (WorldSettings)
		{
			// Makes no sense to have two WorldSettings so destroy existing one
			WorldSettings->Destroy();
		}

		WorldSettings = NewWorldSettings;
	}
}

ALevelScriptActor* ULevel::GetLevelScriptActor() const
{
	return LevelScriptActor;
}


void ULevel::InitializeNetworkActors()
{
	check( OwningWorld );
	bool			bIsServer				= OwningWorld->IsServer();

	// Kill non relevant client actors and set net roles correctly
	for( int32 ActorIndex=0; ActorIndex<Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Actors[ActorIndex];
		if( Actor )
		{
			// Kill off actors that aren't interesting to the client.
			if( !Actor->IsActorInitialized() && !Actor->bActorSeamlessTraveled )
			{
				// Add to startup list
				if (Actor->bNetLoadOnClient)
				{
					Actor->bNetStartup = true;

					for (UActorComponent* Component : Actor->GetComponents())
					{
						if (Component)
						{
							Component->SetIsNetStartupComponent(true);
						}
					}
				}

				if (!bIsServer)
				{
					if (!Actor->bNetLoadOnClient)
					{
						Actor->Destroy(true);
					}
					else
					{
						// Exchange the roles if:
						//	-We are a client
						//  -This is bNetLoadOnClient=true
						//  -RemoteRole != ROLE_None
						Actor->ExchangeNetRoles(true);
					}
				}				
			}

			Actor->bActorSeamlessTraveled = false;
		}
	}
}

void ULevel::InitializeRenderingResources()
{
	// OwningWorld can be NULL when InitializeRenderingResources is called during undo, where a transient ULevel is created to allow undoing level move operations
	// At the point at which Pre/PostEditChange is called on that transient ULevel, it is not part of any world and therefore should not have its rendering resources initialized
	if (OwningWorld)
	{
		ULevel* ActiveLightingScenario = OwningWorld->GetActiveLightingScenario();
		UMapBuildDataRegistry* EffectiveMapBuildData = MapBuildData;

		if (ActiveLightingScenario && ActiveLightingScenario->MapBuildData)
		{
			EffectiveMapBuildData = ActiveLightingScenario->MapBuildData;
		}

		if (!PrecomputedLightVolume->IsAddedToScene())
		{
			PrecomputedLightVolume->AddToScene(OwningWorld->Scene, EffectiveMapBuildData, LevelBuildDataId);
		}

		if (!PrecomputedVolumetricLightmap->IsAddedToScene())
		{
			PrecomputedVolumetricLightmap->AddToScene(OwningWorld->Scene, EffectiveMapBuildData, LevelBuildDataId);
		}
	}
}

void ULevel::ReleaseRenderingResources()
{
	if (OwningWorld && PrecomputedLightVolume)
	{
		PrecomputedLightVolume->RemoveFromScene(OwningWorld->Scene);
	}

	if (OwningWorld && PrecomputedVolumetricLightmap)
	{
		PrecomputedVolumetricLightmap->RemoveFromScene(OwningWorld->Scene);
	}
}

void ULevel::RouteActorInitialize()
{
	// Send PreInitializeComponents and collect volumes.
	for( int32 Index = 0; Index < Actors.Num(); ++Index )
	{
		AActor* const Actor = Actors[Index];
		if( Actor && !Actor->IsActorInitialized() )
		{
			Actor->PreInitializeComponents();
		}
	}

	const bool bCallBeginPlay = OwningWorld->HasBegunPlay();
	TArray<AActor *> ActorsToBeginPlay;

	// Send InitializeComponents on components and PostInitializeComponents.
	for( int32 Index = 0; Index < Actors.Num(); ++Index )
	{
		AActor* const Actor = Actors[Index];
		if( Actor )
		{
			if( !Actor->IsActorInitialized() )
			{
				// Call Initialize on Components.
				Actor->InitializeComponents();

				Actor->PostInitializeComponents(); // should set Actor->bActorInitialized = true
				if (!Actor->IsActorInitialized() && !Actor->IsPendingKill())
				{
					UE_LOG(LogActor, Fatal, TEXT("%s failed to route PostInitializeComponents.  Please call Super::PostInitializeComponents() in your <className>::PostInitializeComponents() function. "), *Actor->GetFullName() );
				}

				if (bCallBeginPlay && !Actor->IsChildActor())
				{
					ActorsToBeginPlay.Add(Actor);
				}
			}

			// Components are all set up, init touching state.
			// Note: Not doing notifies here since loading or streaming in isn't actually conceptually beginning a touch.
			//	     Rather, it was always touching and the mechanics of loading is just an implementation detail.
			Actor->UpdateOverlaps(Actor->bGenerateOverlapEventsDuringLevelStreaming);
		}
	}

	// Do this in a second pass to make sure they're all initialized before begin play starts
	for (int32 ActorIndex = 0; ActorIndex < ActorsToBeginPlay.Num(); ActorIndex++)
	{
		AActor* Actor = ActorsToBeginPlay[ActorIndex];
		SCOPE_CYCLE_COUNTER(STAT_ActorBeginPlay);
		Actor->DispatchBeginPlay();
	}
}

UPackage* ULevel::CreateMapBuildDataPackage() const
{
	FString PackageName = GetOutermost()->GetName() + TEXT("_BuiltData");
	UPackage* BuiltDataPackage = CreatePackage(NULL, *PackageName);
	// PKG_ContainsMapData required so FEditorFileUtils::GetDirtyContentPackages can treat this as a map package
	BuiltDataPackage->SetPackageFlags(PKG_ContainsMapData);
	return BuiltDataPackage;
}

UMapBuildDataRegistry* ULevel::GetOrCreateMapBuildData()
{
	if (!MapBuildData 
		// If MapBuildData is in the level package we need to create a new one, see CreateRegistryForLegacyMap
		|| MapBuildData->IsLegacyBuildData()
		|| !MapBuildData->HasAllFlags(RF_Public | RF_Standalone))
	{
		if (MapBuildData)
		{
			// Allow the legacy registry to be GC'ed
			MapBuildData->ClearFlags(RF_Standalone);
		}

		UPackage* BuiltDataPackage = CreateMapBuildDataPackage();

		FName ShortPackageName = FPackageName::GetShortFName(BuiltDataPackage->GetFName());
		// Top level UObjects have to have both RF_Standalone and RF_Public to be saved into packages
		MapBuildData = NewObject<UMapBuildDataRegistry>(BuiltDataPackage, ShortPackageName, RF_Standalone | RF_Public);
		MarkPackageDirty();
	}

	return MapBuildData;
}

void ULevel::SetLightingScenario(bool bNewIsLightingScenario)
{
	bIsLightingScenario = bNewIsLightingScenario;

	OwningWorld->PropagateLightingScenarioChange(true);
}

#if WITH_EDITOR
void ULevel::OnApplyNewLightingData(bool bLightingSuccessful)
{
	// Store level offset that was used during static light data build
	// This will be used to find correct world position of precomputed lighting samples during origin rebasing
	LightBuildLevelOffset = FIntVector::ZeroValue;
	if (bLightingSuccessful && OwningWorld && OwningWorld->WorldComposition)
	{
		LightBuildLevelOffset = OwningWorld->WorldComposition->GetLevelOffset(this);
	}
}
#endif

bool ULevel::HasAnyActorsOfType(UClass *SearchType)
{
	// just search the actors array
	for (int32 Idx = 0; Idx < Actors.Num(); Idx++)
	{
		AActor *Actor = Actors[Idx];
		// if valid, not pending kill, and
		// of the correct type
		if (Actor != NULL &&
			!Actor->IsPendingKill() &&
			Actor->IsA(SearchType))
		{
			return true;
		}
	}
	return false;
}

#if WITH_EDITOR

TArray<UBlueprint*> ULevel::GetLevelBlueprints() const
{
	TArray<UBlueprint*> LevelBlueprints;

	ForEachObjectWithOuter(this, [&LevelBlueprints](UObject* LevelChild)
	{
		UBlueprint* LevelChildBP = Cast<UBlueprint>(LevelChild);
		if (LevelChildBP)
		{
			LevelBlueprints.Add(LevelChildBP);
		}
	}, false, RF_NoFlags, EInternalObjectFlags::PendingKill);

	return LevelBlueprints;
}

ULevelScriptBlueprint* ULevel::GetLevelScriptBlueprint(bool bDontCreate)
{
	const FString LevelScriptName = ULevelScriptBlueprint::CreateLevelScriptNameFromLevel(this);
	if( !LevelScriptBlueprint && !bDontCreate)
	{
		// The level blueprint must be named the same as the level/world.
		// If there is already something there with that name, rename it to something else.
		if (UObject* ExistingObject = StaticFindObject(nullptr, this, *LevelScriptName))
		{
			ExistingObject->Rename(nullptr, nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
		}

		// If no blueprint is found, create one. 
		LevelScriptBlueprint = Cast<ULevelScriptBlueprint>(FKismetEditorUtilities::CreateBlueprint(GEngine->LevelScriptActorClass, this, FName(*LevelScriptName), BPTYPE_LevelScript, ULevelScriptBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass()));

		// LevelScript blueprints should not be standalone
		LevelScriptBlueprint->ClearFlags(RF_Standalone);
		ULevel::LevelDirtiedEvent.Broadcast();
		// Refresh level script actions
		FWorldDelegates::RefreshLevelScriptActions.Broadcast(OwningWorld);
	}

	// Ensure that friendly name is always up-to-date
	if (LevelScriptBlueprint)
	{
		LevelScriptBlueprint->FriendlyName = LevelScriptName;
	}

	return LevelScriptBlueprint;
}

void ULevel::CleanupLevelScriptBlueprint()
{
	if( LevelScriptBlueprint )
	{
		if( LevelScriptBlueprint->SkeletonGeneratedClass )
		{
			LevelScriptBlueprint->SkeletonGeneratedClass->ClassGeneratedBy = nullptr; 
		}

		if( LevelScriptBlueprint->GeneratedClass )
		{
			LevelScriptBlueprint->GeneratedClass->ClassGeneratedBy = nullptr; 
		}
	}
}

void ULevel::OnLevelScriptBlueprintChanged(ULevelScriptBlueprint* InBlueprint)
{
	if( !InBlueprint->bIsRegeneratingOnLoad && 
		// Make sure this is OUR level scripting blueprint
		ensureMsgf(InBlueprint == LevelScriptBlueprint, TEXT("Level ('%s') recieved OnLevelScriptBlueprintChanged notification for the wrong Blueprint ('%s')."), LevelScriptBlueprint ? *LevelScriptBlueprint->GetPathName() : TEXT("NULL"), *InBlueprint->GetPathName()) )
	{
		UClass* SpawnClass = (LevelScriptBlueprint->GeneratedClass) ? LevelScriptBlueprint->GeneratedClass : LevelScriptBlueprint->SkeletonGeneratedClass;

		// Get rid of the old LevelScriptActor
		if( LevelScriptActor )
		{
			LevelScriptActor->MarkPendingKill();
			LevelScriptActor = nullptr;
		}

		check( OwningWorld );
		// Create the new one
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.OverrideLevel = this;
		LevelScriptActor = OwningWorld->SpawnActor<ALevelScriptActor>( SpawnClass, SpawnInfo );

		if( LevelScriptActor )
		{
			LevelScriptActor->ClearFlags(RF_Transactional);
			check(LevelScriptActor->GetOuter() == this);
			// Finally, fixup all the bound events to point to their new LSA
			FBlueprintEditorUtils::FixLevelScriptActorBindings(LevelScriptActor, InBlueprint);
		}		
	}
}	

void ULevel::BeginCacheForCookedPlatformData(const ITargetPlatform *TargetPlatform)
{
	Super::BeginCacheForCookedPlatformData(TargetPlatform);

	// Cook all level blueprints.
	for (auto LevelBlueprint : GetLevelBlueprints())
	{
		LevelBlueprint->BeginCacheForCookedPlatformData(TargetPlatform);
	}
}

void ULevel::FixupForPIE(int32 PIEInstanceID)
{
	TGuardValue<int32> SetPlayInEditorID(GPlayInEditorID, PIEInstanceID);

	struct FSoftPathPIEFixupSerializer : public FArchiveUObject
	{
		FSoftPathPIEFixupSerializer() 
		{
			ArIsSaving = true;
		}

		FArchive& operator<<(FSoftObjectPath& Value)
		{
			Value.FixupForPIE();
			return *this;
		}
	};

	FSoftPathPIEFixupSerializer FixupSerializer;

	TArray<UObject*> SubObjects;
	GetObjectsWithOuter(this, SubObjects);

	for (UObject* Object : SubObjects)
	{
		Object->Serialize(FixupSerializer);
	}
}

#endif	//WITH_EDITOR

bool ULevel::IsPersistentLevel() const
{
	bool bIsPersistent = false;
	if( OwningWorld )
	{
		bIsPersistent = (this == OwningWorld->PersistentLevel);
	}
	return bIsPersistent;
}

bool ULevel::IsCurrentLevel() const
{
	bool bIsCurrent = false;
	if( OwningWorld )
	{
		bIsCurrent = (this == OwningWorld->GetCurrentLevel());
	}
	return bIsCurrent;
}

void ULevel::ApplyWorldOffset(const FVector& InWorldOffset, bool bWorldShift)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ULevel_ApplyWorldOffset);

	// Move precomputed light samples
	if (PrecomputedLightVolume && !InWorldOffset.IsZero())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ULevel_ApplyWorldOffset_PrecomputedLightVolume);
		
		if (!PrecomputedLightVolume->IsAddedToScene())
		{
			// When we add level to world, move precomputed lighting data taking into account position of level at time when lighting was built  
			if (bIsAssociatingLevel)
			{
				FVector PrecomputedLightVolumeOffset = InWorldOffset - FVector(LightBuildLevelOffset);
				PrecomputedLightVolume->ApplyWorldOffset(PrecomputedLightVolumeOffset);
			}
		}
		// At world origin rebasing all registered volumes will be moved during FScene shifting
		// Otherwise we need to send a command to move just this volume
		else if (!bWorldShift) 
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
 				ApplyWorldOffset_PLV,
 				FPrecomputedLightVolume*, InPrecomputedLightVolume, PrecomputedLightVolume,
 				FVector, InWorldOffset, InWorldOffset,
 			{
				InPrecomputedLightVolume->ApplyWorldOffset(InWorldOffset);
 			});
		}
	}

	if (PrecomputedVolumetricLightmap && !InWorldOffset.IsZero())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ULevel_ApplyWorldOffset_PrecomputedLightVolume);
		
		if (!PrecomputedVolumetricLightmap->IsAddedToScene())
		{
			// When we add level to world, move precomputed lighting data taking into account position of level at time when lighting was built  
			if (bIsAssociatingLevel)
			{
				FVector PrecomputedVolumetricLightmapOffset = InWorldOffset - FVector(LightBuildLevelOffset);
				PrecomputedVolumetricLightmap->ApplyWorldOffset(PrecomputedVolumetricLightmapOffset);
			}
		}
		// At world origin rebasing all registered volumes will be moved during FScene shifting
		// Otherwise we need to send a command to move just this volume
		else if (!bWorldShift) 
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
 				ApplyWorldOffset_PLV,
 				FPrecomputedVolumetricLightmap*, InPrecomputedVolumetricLightmap, PrecomputedVolumetricLightmap,
 				FVector, InWorldOffset, InWorldOffset,
 			{
				InPrecomputedVolumetricLightmap->ApplyWorldOffset(InWorldOffset);
 			});
		}
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ULevel_ApplyWorldOffset_Actors);
		// Iterate over all actors in the level and move them
		for (int32 ActorIndex = 0; ActorIndex < Actors.Num(); ActorIndex++)
		{
			AActor* Actor = Actors[ActorIndex];
			if (Actor)
			{
				FVector Offset = (bWorldShift && Actor->bIgnoresOriginShifting) ? FVector::ZeroVector : InWorldOffset;

				if (!Actor->IsA(ANavigationData::StaticClass())) // Navigation data will be moved in NavigationSystem
				{
					FScopeCycleCounterUObject Context(Actor);
					Actor->ApplyWorldOffset(Offset, bWorldShift);
				}
			}
		}
	}
	
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ULevel_ApplyWorldOffset_Model);
		// Move model geometry
		for (int32 CompIdx = 0; CompIdx < ModelComponents.Num(); ++CompIdx)
		{
			ModelComponents[CompIdx]->ApplyWorldOffset(InWorldOffset, bWorldShift);
		}
	}

	if (!InWorldOffset.IsZero()) 
	{
		// Notify streaming managers that level primitives were shifted
		IStreamingManager::Get().NotifyLevelOffset(this, InWorldOffset);		
	}
	
	FWorldDelegates::PostApplyLevelOffset.Broadcast(this, OwningWorld, InWorldOffset, bWorldShift);
}

void ULevel::RegisterActorForAutoReceiveInput(AActor* Actor, const int32 PlayerIndex)
{
	PendingAutoReceiveInputActors.Add(FPendingAutoReceiveInputActor(Actor, PlayerIndex));
};

void ULevel::PushPendingAutoReceiveInput(APlayerController* InPlayerController)
{
	check( InPlayerController );
	int32 PlayerIndex = -1;
	int32 Index = 0;
	for( FConstPlayerControllerIterator Iterator = InPlayerController->GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* PlayerController = Iterator->Get();
		if (InPlayerController == PlayerController)
		{
			PlayerIndex = Index;
			break;
		}
		Index++;
	}

	if (PlayerIndex >= 0)
	{
		TArray<AActor*> ActorsToAdd;
		for (int32 PendingIndex = PendingAutoReceiveInputActors.Num() - 1; PendingIndex >= 0; --PendingIndex)
		{
			FPendingAutoReceiveInputActor& PendingActor = PendingAutoReceiveInputActors[PendingIndex];
			if (PendingActor.PlayerIndex == PlayerIndex)
			{
				if (PendingActor.Actor.IsValid())
				{
					ActorsToAdd.Add(PendingActor.Actor.Get());
				}
				PendingAutoReceiveInputActors.RemoveAtSwap(PendingIndex);
			}
		}
		for (int32 ToAddIndex = ActorsToAdd.Num() - 1; ToAddIndex >= 0; --ToAddIndex)
		{
			APawn* PawnToPossess = Cast<APawn>(ActorsToAdd[ToAddIndex]);
			if (PawnToPossess)
			{
				InPlayerController->Possess(PawnToPossess);
			}
			else
			{
				ActorsToAdd[ToAddIndex]->EnableInput(InPlayerController);
			}
		}
	}
}

void ULevel::AddAssetUserData(UAssetUserData* InUserData)
{
	if(InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if(ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* ULevel::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for(int32 DataIdx=0; DataIdx<AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if(Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void ULevel::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for(int32 DataIdx=0; DataIdx<AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if(Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

bool ULevel::HasVisibilityRequestPending() const
{
	return (OwningWorld && this == OwningWorld->CurrentLevelPendingVisibility);
}

bool ULevel::HasVisibilityChangeRequestPending() const
{
	return (OwningWorld && ( this == OwningWorld->CurrentLevelPendingVisibility || this == OwningWorld->CurrentLevelPendingInvisibility ) );
}


