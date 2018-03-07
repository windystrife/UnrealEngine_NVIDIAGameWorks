// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	World.cpp: UWorld implementation
=============================================================================*/

#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Logging/LogScopedVerbosityOverride.h"
#include "Stats/StatsMisc.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/UObjectAnnotation.h"
#include "Serialization/ArchiveTraceRoute.h"
#include "Misc/PackageName.h"
#include "Serialization/AsyncLoading.h"
#include "GameMapsSettings.h"
#include "TimerManager.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Controller.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Model.h"
#include "Engine/Brush.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/MetaData.h"
#include "UObject/PropertyPortFlags.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "GameFramework/OnlineReplStructs.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "StaticBoundShaderState.h"
#include "Materials/Material.h"
#include "Engine/Canvas.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/DefaultPhysicsVolume.h"
#include "GameFramework/WorldSettings.h"
#include "RendererInterface.h"
#include "Engine/Texture2D.h"
#include "Engine/LevelStreaming.h"
#include "Materials/MaterialInstance.h"
#include "Engine/LocalPlayer.h"
#include "ContentStreaming.h"
#include "ComponentReregisterContext.h"
#include "UnrealEngine.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/CullDistanceVolume.h"
#include "Engine/Console.h"
#include "Engine/WorldComposition.h"
#include "Matinee/MatineeActor.h"
#include "Misc/NetworkVersion.h"
#include "GameFramework/GameNetworkManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/NetworkProfiler.h"
#include "TickTaskManagerInterface.h"
#include "FXSystem.h"
#include "AudioDevice.h"
#include "VisualLogger/VisualLogger.h"
#include "LevelUtils.h"
#include "PhysicsPublic.h"
#include "AI/AISystemBase.h"
#include "Camera/CameraActor.h"
#include "Engine/NetworkObjectList.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Components/BrushComponent.h"
#include "Engine/Polys.h"
#include "Components/ModelComponent.h"
#include "GlobalShader.h"
#include "ShaderCompiler.h"
#include "Engine/LevelScriptBlueprint.h"

#include "Materials/MaterialParameterCollectionInstance.h"
#include "ProfilingDebugging/LoadTimeTracker.h"

#if WITH_EDITOR
	#include "DerivedDataCacheInterface.h"
	#include "Kismet2/KismetEditorUtilities.h"
	#include "ThumbnailRendering/WorldThumbnailInfo.h"
	#include "Editor/UnrealEdTypes.h"
	#include "Settings/LevelEditorPlaySettings.h"
	#include "HierarchicalLOD.h"
	#include "IHierarchicalLODUtilities.h"
	#include "HierarchicalLODUtilitiesModule.h"
	#include "ObjectTools.h"
	#include "Engine/LODActor.h"
#endif


#include "Particles/ParticleEventManager.h"

#include "EngineModule.h"
#include "Streaming/TextureStreamingHelpers.h"
#include "Net/DataChannel.h"
#include "Engine/LevelStreamingPersistent.h"
#include "AI/Navigation/AvoidanceManager.h"
#include "PhysicsEngine/PhysicsConstraintActor.h"
#include "PhysicsEngine/PhysicsCollisionHandler.h"
#include "Engine/ShadowMapTexture2D.h"
#include "Components/LineBatchComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Engine/LightMapTexture2D.h"
#include "UObject/UObjectThreadContext.h"
#include "Engine/CoreSettings.h"
#include "Net/PerfCountersHelpers.h"
#include "InGamePerformanceTracker.h"
#include "Engine/AssetManager.h"

#if WITH_FLEX
#include "PhysicsEngine/FlexFluidSurfaceActor.h"
#include "PhysicsEngine/FlexFluidSurfaceComponent.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWorld, Log, All);
DEFINE_LOG_CATEGORY(LogSpawn);

#define LOCTEXT_NAMESPACE "World"

template<class Function>
static void ForEachNetDriver(UEngine* Engine, UWorld* const World, const Function InFunction)
{
	if (Engine == nullptr || World == nullptr)
	{
		return;
	}

	FWorldContext* const Context = Engine->GetWorldContextFromWorld(World);
	if (Context != nullptr)
	{
		for (FNamedNetDriver& Driver : Context->ActiveNetDrivers)
		{
			InFunction(Driver.NetDriver);
		}
	}
}

FActorSpawnParameters::FActorSpawnParameters()
: Name(NAME_None)
, Template(NULL)
, Owner(NULL)
, Instigator(NULL)
, OverrideLevel(NULL)
, SpawnCollisionHandlingOverride(ESpawnActorCollisionHandlingMethod::Undefined)
, bRemoteOwned(false)
, bNoFail(false)
, bDeferConstruction(false)
, bAllowDuringConstructionScript(false)
#if WITH_EDITOR
, bTemporaryEditorActor(false)
#endif
, ObjectFlags(RF_Transactional)
{
}

FLevelCollection::FLevelCollection()
	: CollectionType(ELevelCollectionType::DynamicSourceLevels)
	, GameState(nullptr)
	, NetDriver(nullptr)
	, DemoNetDriver(nullptr)
	, PersistentLevel(nullptr)
	, bIsVisible(true)
{
}

FLevelCollection::~FLevelCollection()
{
	for (ULevel* Level : Levels)
	{
		if (Level)
		{
			check(Level->GetCachedLevelCollection() == this);
			Level->SetCachedLevelCollection(nullptr);
		}
	}
}

FLevelCollection::FLevelCollection(FLevelCollection&& Other)
	: CollectionType(Other.CollectionType)
	, GameState(Other.GameState)
	, NetDriver(Other.NetDriver)
	, DemoNetDriver(Other.DemoNetDriver)
	, PersistentLevel(Other.PersistentLevel)
	, Levels(MoveTemp(Other.Levels))
	, bIsVisible(Other.bIsVisible)
{
	for (ULevel* Level : Levels)
	{
		if (Level)
		{
			check(Level->GetCachedLevelCollection() == &Other);
			Level->SetCachedLevelCollection(this);
		}
	}
}

FLevelCollection& FLevelCollection::operator=(FLevelCollection&& Other)
{
	if (this != &Other)
	{
		CollectionType = Other.CollectionType;
		GameState = Other.GameState;
		NetDriver = Other.NetDriver;
		DemoNetDriver = Other.DemoNetDriver;
		PersistentLevel = Other.PersistentLevel;
		Levels = MoveTemp(Other.Levels);
		bIsVisible = Other.bIsVisible;

		for (ULevel* Level : Levels)
		{
			if (Level)
			{
				check(Level->GetCachedLevelCollection() == &Other);
				Level->SetCachedLevelCollection(this);
			}
		}
	}

	return *this;
}

void FLevelCollection::SetPersistentLevel(ULevel* const Level)
{
	PersistentLevel = Level;
}

void FLevelCollection::AddLevel(ULevel* const Level)
{
	if (Level)
	{
		// Sanity check that Level isn't already in another collection.
		check(Level->GetCachedLevelCollection() == nullptr);
		Levels.Add(Level);
		Level->SetCachedLevelCollection(this);
	}
}

void FLevelCollection::RemoveLevel(ULevel* const Level)
{
	if (Level)
	{
		check(Level->GetCachedLevelCollection() == this);
		Level->SetCachedLevelCollection(nullptr);
		Levels.Remove(Level);
	}
}

FScopedLevelCollectionContextSwitch::FScopedLevelCollectionContextSwitch(const FLevelCollection* const InLevelCollection, UWorld* const InWorld)
	: World(InWorld)
	, SavedTickingCollectionIndex(InWorld ? InWorld->GetActiveLevelCollectionIndex() : INDEX_NONE)
{
	if (World)
	{
		const int32 FoundIndex = World->GetLevelCollections().IndexOfByPredicate([InLevelCollection](const FLevelCollection& Collection)
		{
			return &Collection == InLevelCollection;
		});

		World->SetActiveLevelCollection(FoundIndex);
	}
}

FScopedLevelCollectionContextSwitch::FScopedLevelCollectionContextSwitch(int32 InLevelCollectionIndex, UWorld* const InWorld)
	: World(InWorld)
	, SavedTickingCollectionIndex(InWorld ? InWorld->GetActiveLevelCollectionIndex() : INDEX_NONE)
{
	if (World)
	{
		World->SetActiveLevelCollection(InLevelCollectionIndex);
	}
}

FScopedLevelCollectionContextSwitch::~FScopedLevelCollectionContextSwitch()
{
	if (World)
	{
		World->SetActiveLevelCollection(SavedTickingCollectionIndex);
	}
}



/*-----------------------------------------------------------------------------
	UWorld implementation.
-----------------------------------------------------------------------------*/

/** Global world pointer */
UWorldProxy GWorld;

TMap<FName, EWorldType::Type> UWorld::WorldTypePreLoadMap;

FWorldDelegates::FWorldEvent FWorldDelegates::OnPostWorldCreation;
FWorldDelegates::FWorldInitializationEvent FWorldDelegates::OnPreWorldInitialization;
FWorldDelegates::FWorldInitializationEvent FWorldDelegates::OnPostWorldInitialization;
#if WITH_EDITOR
FWorldDelegates::FWorldRenameEvent FWorldDelegates::OnPreWorldRename;
#endif // WITH_EDITOR
FWorldDelegates::FWorldPostDuplicateEvent FWorldDelegates::OnPostDuplicate;
FWorldDelegates::FWorldCleanupEvent FWorldDelegates::OnWorldCleanup;
FWorldDelegates::FWorldCleanupEvent FWorldDelegates::OnPostWorldCleanup;
FWorldDelegates::FWorldEvent FWorldDelegates::OnPreWorldFinishDestroy;
FWorldDelegates::FOnLevelChanged FWorldDelegates::LevelAddedToWorld;
FWorldDelegates::FOnLevelChanged FWorldDelegates::LevelRemovedFromWorld;
FWorldDelegates::FLevelOffsetEvent FWorldDelegates::PostApplyLevelOffset;
FWorldDelegates::FWorldGetAssetTags FWorldDelegates::GetAssetTags;
FWorldDelegates::FOnWorldTickStart FWorldDelegates::OnWorldTickStart;
FWorldDelegates::FOnWorldPostActorTick FWorldDelegates::OnWorldPostActorTick;
#if WITH_EDITOR
FWorldDelegates::FRefreshLevelScriptActionsEvent FWorldDelegates::RefreshLevelScriptActions;
#endif // WITH_EDITOR

UWorld::UWorld( const FObjectInitializer& ObjectInitializer )
: UObject(ObjectInitializer)
, ActiveLevelCollectionIndex(INDEX_NONE)
#if WITH_EDITOR
, HierarchicalLODBuilder(new FHierarchicalLODBuilder(this))
#endif
,	FeatureLevel(GMaxRHIFeatureLevel)
,	bShouldTick(true)
, URL(FURL(NULL))
,	FXSystem(NULL)
,	TickTaskLevel(FTickTaskManagerInterface::Get().AllocateTickTaskLevel())
,   bIsBuilt(false)
,   AudioDeviceHandle(INDEX_NONE)
,	FlushLevelStreamingType(EFlushLevelStreamingType::None)
,	NextTravelType(TRAVEL_Relative)
{
	TimerManager = new FTimerManager();
#if WITH_EDITOR
	bBroadcastSelectionChange = true; //Ed Only
	EditorViews.SetNum(ELevelViewportType::LVT_MAX);
#endif // WITH_EDITOR

	FWorldDelegates::OnPostWorldCreation.Broadcast(this);

	PerfTrackers = new FWorldInGamePerformanceTrackers();
}

UWorld::~UWorld()
{
	while (AsyncPreRegisterLevelStreamingTasks.GetValue())
	{
		FPlatformProcess::Sleep(0.0f);
	}	

	if (PerfTrackers)
	{
		delete PerfTrackers;
	}
}

void UWorld::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar << PersistentLevel;

	if (Ar.UE4Ver() < VER_UE4_ADD_EDITOR_VIEWS)
	{
#if WITH_EDITOR
		EditorViews.SetNum(4);
#endif
		for (int32 i = 0; i < 4; ++i)
		{
			FLevelViewportInfo TempViewportInfo;
			Ar << TempViewportInfo;
#if WITH_EDITOR
			if (Ar.IsLoading())
			{
				EditorViews[i] = TempViewportInfo;
			}
#endif
		}
	}
#if WITH_EDITOR
	if ( Ar.IsLoading() )
	{
		for (FLevelViewportInfo& ViewportInfo : EditorViews)
		{
			ViewportInfo.CamUpdated = true;

			if ( ViewportInfo.CamOrthoZoom == 0.f )
			{
				ViewportInfo.CamOrthoZoom = DEFAULT_ORTHOZOOM;
			}
		}
		EditorViews.SetNum(ELevelViewportType::LVT_MAX);
	}
#endif

	if (Ar.UE4Ver() < VER_UE4_REMOVE_SAVEGAMESUMMARY)
	{
		UObject* DummyObject;
		Ar << DummyObject;
	}

	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << Levels;
		Ar << CurrentLevel;
		Ar << URL;

		Ar << NetDriver;
		
		Ar << LineBatcher;
		Ar << PersistentLineBatcher;
		Ar << ForegroundLineBatcher;

		Ar << MyParticleEventManager;
		Ar << GameState;
		Ar << AuthorityGameMode;
		Ar << NetworkManager;

		Ar << NavigationSystem;
		Ar << AvoidanceManager;
	}

	Ar << ExtraReferencedObjects;
	Ar << StreamingLevels;
		
	// Mark archive and package as containing a map if we're serializing to disk.
	if( !HasAnyFlags( RF_ClassDefaultObject ) && Ar.IsPersistent() )
	{
		Ar.ThisContainsMap();
		GetOutermost()->ThisContainsMap();
	}

	// Serialize World composition for PIE
	if (Ar.GetPortFlags() & PPF_DuplicateForPIE)
	{
		Ar << OriginLocation;
		Ar << RequestedOriginLocation;
	}
	
	// UWorlds loaded/duplicated for PIE must lose RF_Public and RF_Standalone since they should not be referenced by objects in other packages and they should be GCed normally
	if (GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
	{
		ClearFlags(RF_Public|RF_Standalone);
	}
}

void UWorld::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{	
#if WITH_EDITOR
	UWorld* This = CastChecked<UWorld>(InThis);
	if( GIsEditor )
	{
		Collector.AddReferencedObject( This->PersistentLevel, This );
		Collector.AddReferencedObjects(This->Levels, This);
		Collector.AddReferencedObject( This->CurrentLevel, This );
		Collector.AddReferencedObject( This->NetDriver, This );
		Collector.AddReferencedObject( This->DemoNetDriver, This );
		Collector.AddReferencedObject( This->LineBatcher, This );
		Collector.AddReferencedObject( This->PersistentLineBatcher, This );
		Collector.AddReferencedObject( This->ForegroundLineBatcher, This );
		Collector.AddReferencedObject( This->MyParticleEventManager, This );
		Collector.AddReferencedObject( This->GameState, This );
		Collector.AddReferencedObject( This->AuthorityGameMode, This );
		Collector.AddReferencedObject( This->NetworkManager, This );
		Collector.AddReferencedObject( This->NavigationSystem, This );
		Collector.AddReferencedObject( This->AvoidanceManager, This );
	}
#endif

	Super::AddReferencedObjects( InThis, Collector );
}

#if WITH_EDITOR
bool UWorld::Rename(const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags)
{
	check(PersistentLevel);

	UPackage* OldPackage = GetOutermost();

	bool bShouldFail = false;
	FWorldDelegates::OnPreWorldRename.Broadcast(this, InName, NewOuter, Flags, bShouldFail);

	// Make sure our legacy lightmap data is initialized so it can be renamed
	PersistentLevel->HandleLegacyMapBuildData();

	FHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<FHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
	IHierarchicalLODUtilities* Utilities = Module.GetUtilities();
	UPackage* OldHLODPackage = nullptr;	
	// See if any LODActors were found in the level, and if so retrieve the HLOD Package
	if (PersistentLevel->Actors.ContainsByPredicate([](const AActor* Actor) { return Actor != nullptr && Actor->IsA<ALODActor>(); }))
	{
		OldHLODPackage = Utilities->CreateOrRetrieveLevelHLODPackage(PersistentLevel);
	}

	if (bShouldFail)
	{
		return false;
	}

	// Rename the world itself
	if ( !Super::Rename(InName, NewOuter, Flags) )
	{
		return false;
	}

	const bool bTestRename = (Flags & REN_Test) != 0;

	// We're moving the world to a new package, rename UObjects which are map data but don't have the UWorld in their Outer chain.  There are two cases:
	// 1) legacy lightmap textures and MapBuildData object will be in the same package as the UWorld
	// 2) MapBuildData will be in a separate package with lightmap textures underneath it
	if (PersistentLevel->MapBuildData)
	{
		FName NewMapBuildDataName = PersistentLevel->MapBuildData->GetFName();

		if (PersistentLevel->MapBuildData->IsLegacyBuildData())
		{
			TArray<UTexture2D*> LightMapsAndShadowMaps;
			GetLightMapsAndShadowMaps(PersistentLevel, LightMapsAndShadowMaps);

			for (UTexture2D* Tex : LightMapsAndShadowMaps)
			{
				if (Tex)
				{
					if (!Tex->Rename(*Tex->GetName(), NewOuter, Flags))
					{
						return false;
					}
				}
			}
		}
		else
		{
			FString NewPackageName = GetOutermost()->GetName() + TEXT("_BuiltData");
			NewMapBuildDataName = FPackageName::GetShortFName(*NewPackageName);
			UPackage* BuildDataPackage = PersistentLevel->MapBuildData->GetOutermost();

			if (!BuildDataPackage->Rename(*NewPackageName, NewOuter, Flags))
			{
				return false;
			}
		}

		if (!PersistentLevel->MapBuildData->Rename(*NewMapBuildDataName.ToString(), NewOuter, Flags))
		{
			return false;
		}
	}

	// Rename the level script blueprint now, unless we are in PostLoad. ULevel::PostLoad should handle renaming this at load time.
	if (!FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		const bool bDontCreate = true;
		UBlueprint* LevelScriptBlueprint = PersistentLevel->GetLevelScriptBlueprint(bDontCreate);
		if ( LevelScriptBlueprint )
		{
			// See if we are just testing for a rename. When testing, the world hasn't actually changed outers, so we need to test for name collisions at the target outer.
			if ( bTestRename )
			{
				// We are just testing. Check for name collisions in the new package. This is only needed because these objects use the supplied outer's Outermost() instead of the outer itself
				if (!LevelScriptBlueprint->RenameGeneratedClasses(InName, NewOuter, Flags))
				{
					return false;
				}
			}
			else
			{
				// The level blueprint must be named the same as the level/world.
				// If there is already something there with that name, rename it to something else.
				if (UObject* ExistingObject = StaticFindObject(nullptr, LevelScriptBlueprint->GetOuter(), InName))
				{
					ExistingObject->Rename(nullptr, nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
				}

				// This is a normal rename. Use LevelScriptBlueprint->GetOuter() instead of NULL to make sure the generated top level objects are moved appropriately
				if ( !LevelScriptBlueprint->Rename(InName, LevelScriptBlueprint->GetOuter(), Flags) )
				{
					return false;
				}
			}
		}
	}

	// Update the PKG_ContainsMap package flag
	UPackage* NewPackage = GetOutermost();
	if ( !bTestRename && NewPackage != OldPackage )
	{
		// If this is the last world removed from a package, clear the PKG_ContainsMap flag
		if ( UWorld::FindWorldInPackage(OldPackage) == NULL )
		{
			OldPackage->ClearPackageFlags(PKG_ContainsMap);
		}

		// Set the PKG_ContainsMap flag in the new package
		NewPackage->ThisContainsMap();
	}

	// Move over HLOD assets to new _HLOD Package
	if (OldHLODPackage)
	{
		UPackage* NewHLODPackage = Utilities->CreateOrRetrieveLevelHLODPackage(PersistentLevel);
		TArray<UObject*> Objects;
		// Retrieve all of the HLOD objects 
		ForEachObjectWithOuter(OldHLODPackage, [&Objects](UObject* Obj)
		{
			if (ObjectTools::IsObjectBrowsable(Obj))
			{
				Objects.Add(Obj);
			}
		});
		// Rename them 'into' the new HLOD package
		for (UObject* Object : Objects)
		{
			Object->Rename(*Object->GetName(), NewHLODPackage);
		}
		// Delete the old HLOD package
		TArray<UObject*> DeleteObjects = { Cast<UObject>(OldHLODPackage) };
		ObjectTools::DeleteObjectsUnchecked(DeleteObjects);
	}
	

	return true;
}
#endif

void UWorld::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	TArray<UObject*> ObjectsToFixReferences;
	TMap<UObject*, UObject*> ReplacementMap;

	// If we are not duplicating for PIE, fix up objects that travel with the world.
	// Note that these objects should really be inners of the world, so if they become inners later, most of this code should not be necessary
	if ( !bDuplicateForPIE )
	{
		check(PersistentLevel);

		// Update the persistent level's owning world. This is needed for some initialization
		if ( !PersistentLevel->OwningWorld )
		{
			PersistentLevel->OwningWorld = this;
		}

		// Update the current level as well
		if ( !CurrentLevel )
		{
			CurrentLevel = PersistentLevel;
		}

		UPackage* MyPackage = GetOutermost();

		// Make sure PKG_ContainsMap is set
		MyPackage->ThisContainsMap();

#if WITH_EDITOR
		// Add the world to the list of objects in which to fix up references.
		ObjectsToFixReferences.Add(this);

		// We're duplicating the world, also duplicate UObjects which are map data but don't have the UWorld in their Outer chain.  There are two cases:
		// 1) legacy lightmap textures and MapBuildData object will be in the same package as the UWorld
		// 2) MapBuildData will be in a separate package with lightmap textures underneath it
		if (PersistentLevel->MapBuildData)
		{
			UPackage* BuildDataPackage = MyPackage;
			FName NewMapBuildDataName = PersistentLevel->MapBuildData->GetFName();
			
			if (!PersistentLevel->MapBuildData->IsLegacyBuildData())
			{
				BuildDataPackage = PersistentLevel->CreateMapBuildDataPackage();
				NewMapBuildDataName = FPackageName::GetShortFName(BuildDataPackage->GetFName());
			}
			
			UObject* NewBuildData = StaticDuplicateObject(PersistentLevel->MapBuildData, BuildDataPackage, NewMapBuildDataName);
			ReplacementMap.Add(PersistentLevel->MapBuildData, NewBuildData);
			ObjectsToFixReferences.Add(NewBuildData);

			UObject* NewTextureOuter = MyPackage;

			if (!PersistentLevel->MapBuildData->IsLegacyBuildData())
			{
				NewTextureOuter = NewBuildData;
			}

			TArray<UTexture2D*> LightMapsAndShadowMaps;
			GetLightMapsAndShadowMaps(PersistentLevel, LightMapsAndShadowMaps);

			// Duplicate the textures, if any
			for (UTexture2D* Tex : LightMapsAndShadowMaps)
			{
				if (Tex && Tex->GetOutermost() != NewTextureOuter)
				{
					UObject* NewTex = StaticDuplicateObject(Tex, NewTextureOuter, Tex->GetFName());
					ReplacementMap.Add(Tex, NewTex);
				}
			}
		}

		// Duplicate the level script blueprint generated classes as well
		const bool bDontCreate = true;
		UBlueprint* LevelScriptBlueprint = PersistentLevel->GetLevelScriptBlueprint(bDontCreate);
		if (LevelScriptBlueprint)
		{
			UObject* OldGeneratedClass = LevelScriptBlueprint->GeneratedClass;
			if (OldGeneratedClass)
			{
				UObject* NewGeneratedClass = StaticDuplicateObject(OldGeneratedClass, MyPackage, OldGeneratedClass->GetFName());
				ReplacementMap.Add(OldGeneratedClass, NewGeneratedClass);

				// The class may have referenced a lightmap or landscape resource that is also being duplicated. Add it to the list of objects that need references fixed up.
				ObjectsToFixReferences.Add(NewGeneratedClass);
			}

			UObject* OldSkeletonClass = LevelScriptBlueprint->SkeletonGeneratedClass;
			if (OldSkeletonClass)
			{
				UObject* NewSkeletonClass = StaticDuplicateObject(OldSkeletonClass, MyPackage, OldSkeletonClass->GetFName());
				ReplacementMap.Add(OldSkeletonClass, NewSkeletonClass);

				// The class may have referenced a lightmap or landscape resource that is also being duplicated. Add it to the list of objects that need references fixed up.
				ObjectsToFixReferences.Add(NewSkeletonClass);
			}
		}
#endif // WITH_EDITOR
	}

	FWorldDelegates::OnPostDuplicate.Broadcast(this, bDuplicateForPIE, ReplacementMap, ObjectsToFixReferences);

#if WITH_EDITOR
	// Now replace references from the old textures/classes to the new ones, if any were duplicated
	if (ReplacementMap.Num() > 0)
	{
		const bool bNullPrivateRefs = false;
		const bool bIgnoreOuterRef = true;
		const bool bIgnoreArchetypeRef = false;
		for (UObject* Obj : ObjectsToFixReferences)
		{
			FArchiveReplaceObjectRef<UObject> ReplaceAr(Obj, ReplacementMap, bNullPrivateRefs, bIgnoreOuterRef, bIgnoreArchetypeRef);
		}
		// PostEditChange is required for some objects to react to the change, e.g. update render-thread proxies
		for (UObject* Obj : ObjectsToFixReferences)
		{
			Obj->PostEditChange();
		}
	}
#endif // WITH_EDITOR
}

void UWorld::FinishDestroy()
{
	// Avoid cleanup if the world hasn't been initialized, e.g., the default object or a world object that got loaded
	// due to level streaming.
	if (bIsWorldInitialized)
	{
		FWorldDelegates::OnPreWorldFinishDestroy.Broadcast(this);

		// Wait for Async Trace data to finish and reset global variable
		WaitForAllAsyncTraceTasks();

		// navigation system should be removed already by UWorld::CleanupWorld
		// unless it wanted to keep resources but got destroyed now
		SetNavigationSystem(nullptr);

		if (FXSystem)
		{
			FFXSystemInterface::Destroy( FXSystem );
			FXSystem = NULL;
		}

		if (PhysicsScene)
		{
			delete PhysicsScene;
			PhysicsScene = NULL;

			if(GPhysCommandHandler)
			{
				GPhysCommandHandler->Flush();
			}
		}

		if (Scene)
		{
			Scene->Release();
			Scene = NULL;
		}
	}

	// Clear GWorld pointer if it's pointing to this object.
	if( GWorld == this )
	{
		GWorld = NULL;
	}
	FTickTaskManagerInterface::Get().FreeTickTaskLevel(TickTaskLevel);
	TickTaskLevel = NULL;

	if (TimerManager)
	{
		delete TimerManager;
	}

#if WITH_EDITOR
	if (HierarchicalLODBuilder)
	{
		delete HierarchicalLODBuilder;
	}
#endif // WITH_EDITOR

	// Remove the PKG_ContainsMap flag from packages that no longer contain a world
	{
		UPackage* WorldPackage = GetOutermost();

		bool bContainsAnotherWorld = false;
		TArray<UObject*> PotentialWorlds;
		GetObjectsWithOuter(WorldPackage, PotentialWorlds, false);
		for (UObject* PotentialWorld : PotentialWorlds)
		{
			UWorld* World = Cast<UWorld>(PotentialWorld);
			if (World && World != this)
			{
				bContainsAnotherWorld = true;
				break;
			}
		}

		if ( !bContainsAnotherWorld )
		{
			WorldPackage->ClearPackageFlags(PKG_ContainsMap);
		}
	}

	Super::FinishDestroy();
}

void UWorld::PostLoad()
{
	EWorldType::Type * PreLoadWorldType = UWorld::WorldTypePreLoadMap.Find(GetOuter()->GetFName());
	if (PreLoadWorldType)
	{
		WorldType = *PreLoadWorldType;
	}
	else
	{
		// Since we did not specify a world type, assume it was loaded from a package to persist in memory
		WorldType = EWorldType::Inactive;
	}

	Super::PostLoad();
	CurrentLevel = PersistentLevel;
	RepairWorldSettings();

	// Remove null streaming level entries (could be if level was saved with transient level streaming objects)
	StreamingLevels.Remove(nullptr);
	
	// Make sure that the persistent level isn't in this world's list of streaming levels.  This should
	// never really happen, but was needed in at least one observed case of corrupt map data.
	if( PersistentLevel != NULL )
	{
		for( int32 LevelIndex = 0; LevelIndex < StreamingLevels.Num(); ++LevelIndex )
		{
			ULevelStreaming* const StreamingLevel = StreamingLevels[ LevelIndex ];
			if( StreamingLevel != NULL )
			{
				if( StreamingLevel->WorldAsset == this || StreamingLevel->GetLoadedLevel() == PersistentLevel )
				{
					// Remove this streaming level
					StreamingLevels.RemoveAt( LevelIndex );
					MarkPackageDirty();
					--LevelIndex;
				}
			}
		}
	}

	// Add the garbage collection callbacks
	FLevelStreamingGCHelper::AddGarbageCollectorCallback();

	// Initially set up the parameter collection list. This may be run again in UWorld::InitWorld.
	SetupParameterCollectionInstances();

#if WITH_EDITOR
	if (GIsEditor)
	{
		if (!GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
		{
			// Needed for VER_UE4_WORLD_NAMED_AFTER_PACKAGE. If this file was manually renamed outside of the editor, this is needed anyway
			const FString ShortPackageName = FPackageName::GetLongPackageAssetName(GetOutermost()->GetName());
			if (GetName() != ShortPackageName)
			{
				Rename(*ShortPackageName, NULL, REN_NonTransactional | REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
			}

			// Worlds are assets so they need RF_Public and RF_Standalone (for the editor)
			SetFlags(RF_Public | RF_Standalone);
		}

		// Ensure the DefaultBrush's model has the same outer as the default brush itself. Older packages erroneously stored this object as a top-level package
		ABrush* DefaultBrush = PersistentLevel->Actors.Num() < 2 ? NULL : Cast<ABrush>(PersistentLevel->Actors[1]);
		UModel* Model = DefaultBrush ? DefaultBrush->Brush : NULL;
		if (Model != NULL)
		{
			if (Model->GetOuter() != DefaultBrush->GetOuter())
			{
				Model->Rename(TEXT("Brush"), DefaultBrush->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
			}
		}

		// Make sure thumbnail info exists
		if ( !ThumbnailInfo )
		{
			ThumbnailInfo = NewObject<UWorldThumbnailInfo>(this, NAME_None, RF_Transactional);
		}
	}
#endif
}


bool UWorld::PreSaveRoot(const TCHAR* Filename)
{
#if WITH_EDITOR
	// Rebuild all level blueprints now to ensure no stale data is stored on the actors
	if( !IsRunningCommandlet() )
	{
		for (UBlueprint* Blueprint : PersistentLevel->GetLevelBlueprints())
		{
			FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection | EBlueprintCompileOptions::SkipSave);
		}
	}
#endif

	// Update components and keep track off whether we need to clean them up afterwards.
	bool bCleanupIsRequired = false;
	if(!PersistentLevel->bAreComponentsCurrentlyRegistered)
	{
		PersistentLevel->UpdateLevelComponents(true);
		bCleanupIsRequired = true;
	}

	return bCleanupIsRequired;
}

void UWorld::PostSaveRoot( bool bCleanupIsRequired )
{
	if( bCleanupIsRequired )
	{
		PersistentLevel->ClearLevelComponents();
	}
}

UWorld* UWorld::GetWorld() const
{
	// Arg... rather hacky, but it seems conceptually ok because the object passed in should be able to fetch the
	// non-const world it's part of.  That's not a mutable action (normally) on the object, as we haven't changed
	// anything.
	return const_cast<UWorld*>(this);
}

void UWorld::SetupParameterCollectionInstances()
{
	// Create an instance for each parameter collection in memory
	for (UMaterialParameterCollection* CurrentCollection : TObjectRange<UMaterialParameterCollection>())
	{
		AddParameterCollectionInstance(CurrentCollection, false);
	}

	UpdateParameterCollectionInstances(false);
}

void UWorld::AddParameterCollectionInstance(UMaterialParameterCollection* Collection, bool bUpdateScene)
{
	int32 ExistingIndex = INDEX_NONE;

	for (int32 InstanceIndex = 0; InstanceIndex < ParameterCollectionInstances.Num(); InstanceIndex++)
	{
		if (ParameterCollectionInstances[InstanceIndex]->GetCollection() == Collection)
		{
			ExistingIndex = InstanceIndex;
			break;
		}
	}

	UMaterialParameterCollectionInstance* NewInstance = NewObject<UMaterialParameterCollectionInstance>();
	NewInstance->SetCollection(Collection, this);

	if (ExistingIndex != INDEX_NONE)
	{
		// Overwrite an existing instance
		ParameterCollectionInstances[ExistingIndex] = NewInstance;
	}
	else
	{
		// Add a new instance
		ParameterCollectionInstances.Add(NewInstance);
	}

	if (bUpdateScene)
	{
		// Update the scene's list of instances, needs to happen to prevent a race condition with GC 
		// (rendering thread still uses the FMaterialParameterCollectionInstanceResource when GC deletes the UMaterialParameterCollectionInstance)
		// However, if UpdateParameterCollectionInstances is going to be called after many AddParameterCollectionInstance's, this can be skipped for now.
		UpdateParameterCollectionInstances(false);
	}
}

UMaterialParameterCollectionInstance* UWorld::GetParameterCollectionInstance(const UMaterialParameterCollection* Collection)
{
	for (int32 InstanceIndex = 0; InstanceIndex < ParameterCollectionInstances.Num(); InstanceIndex++)
	{
		if (ParameterCollectionInstances[InstanceIndex]->GetCollection() == Collection)
		{
			return ParameterCollectionInstances[InstanceIndex];
		}
	}

	// Instance should always exist due to SetupParameterCollectionInstances() and UMaterialParameterCollection::PostLoad()
	check(0);
	return NULL;
}

void UWorld::UpdateParameterCollectionInstances(bool bUpdateInstanceUniformBuffers)
{
	if (Scene)
	{
		TArray<FMaterialParameterCollectionInstanceResource*> InstanceResources;

		for (int32 InstanceIndex = 0; InstanceIndex < ParameterCollectionInstances.Num(); InstanceIndex++)
		{
			UMaterialParameterCollectionInstance* Instance = ParameterCollectionInstances[InstanceIndex];

			if (bUpdateInstanceUniformBuffers)
			{
				Instance->UpdateRenderState();
			}

			InstanceResources.Add(Instance->GetResource());
		}

		Scene->UpdateParameterCollections(InstanceResources);
	}
}

UCanvas* UWorld::GetCanvasForRenderingToTarget()
{
	if (!CanvasForRenderingToTarget)
	{
		CanvasForRenderingToTarget = NewObject<UCanvas>(GetTransientPackage(), NAME_None);
	}

	return CanvasForRenderingToTarget;
}

UCanvas* UWorld::GetCanvasForDrawMaterialToRenderTarget()
{
	if (!CanvasForDrawMaterialToRenderTarget)
	{
		CanvasForDrawMaterialToRenderTarget = NewObject<UCanvas>(GetTransientPackage(), NAME_None);
	}

	return CanvasForDrawMaterialToRenderTarget;
}

UAISystemBase* UWorld::CreateAISystem()
{
	// create navigation system for editor and server targets, but remove it from game clients
	if (AISystem == NULL && UAISystemBase::ShouldInstantiateInNetMode(GetNetMode()) && PersistentLevel)
	{
		FName AIModuleName = UAISystemBase::GetAISystemModuleName();
		const AWorldSettings* WorldSettings = PersistentLevel->GetWorldSettings(false);
		if (AIModuleName.IsNone() == false && WorldSettings && WorldSettings->bEnableAISystem)
		{
			IAISystemModule* AISystemModule = FModuleManager::LoadModulePtr<IAISystemModule>(UAISystemBase::GetAISystemModuleName());
			if (AISystemModule)
			{
				AISystem = AISystemModule->CreateAISystemInstance(this);
				if (AISystem == NULL)
				{
					UE_LOG(LogWorld, Error, TEXT("Failed to create AISystem instance of class %s!"), *UAISystemBase::GetAISystemClassName().ToString());
				}
			}
		}
	}

	return AISystem; 
}

void UWorld::RepairWorldSettings()
{
	AWorldSettings* ExistingWorldSettings = PersistentLevel->GetWorldSettings(false);

	if (ExistingWorldSettings == nullptr && PersistentLevel->Actors.Num() > 0)
	{
		ExistingWorldSettings = Cast<AWorldSettings>(PersistentLevel->Actors[0]);
		if (ExistingWorldSettings)
		{
			// This means the WorldSettings member just wasn't initialized, get that resolved
			PersistentLevel->SetWorldSettings(ExistingWorldSettings);
		}
	}

	// If for some reason we don't have a valid WorldSettings object go ahead and spawn one to avoid crashing.
	// This will generally happen if a map is being moved from a different project.
	if (ExistingWorldSettings == nullptr || !ExistingWorldSettings->IsA(GEngine->WorldSettingsClass))
	{
		// Rename invalid WorldSettings to avoid name collisions
		if (ExistingWorldSettings)
		{
			ExistingWorldSettings->Rename(nullptr, PersistentLevel, REN_ForceNoResetLoaders);
		}
		
		bool bClearOwningWorld = false;

		if (PersistentLevel->OwningWorld == nullptr)
		{
			bClearOwningWorld = true;
			PersistentLevel->OwningWorld = this;
		}

		FActorSpawnParameters SpawnInfo;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.Name = GEngine->WorldSettingsClass->GetFName();
		AWorldSettings* const NewWorldSettings = SpawnActor<AWorldSettings>( GEngine->WorldSettingsClass, SpawnInfo );

		// If there was an existing actor, copy its properties to the new actor (the it will be destroyed by SetWorldSettings)
		if (ExistingWorldSettings)
		{
			NewWorldSettings->UnregisterAllComponents();
			UEngine::CopyPropertiesForUnrelatedObjects(ExistingWorldSettings, NewWorldSettings);
			NewWorldSettings->RegisterAllComponents();
		}

		PersistentLevel->SetWorldSettings(NewWorldSettings);

		// Re-sort actor list as we just shuffled things around.
		PersistentLevel->SortActorList();

		if (bClearOwningWorld)
		{
			PersistentLevel->OwningWorld = nullptr;
		}
	}
	check(GetWorldSettings());
}

void UWorld::InitWorld(const InitializationValues IVS)
{
	if (!ensure(!bIsWorldInitialized))
	{
		return;
	}

	FWorldDelegates::OnPreWorldInitialization.Broadcast(this, IVS);

	if (IVS.bInitializeScenes)
	{
		if (IVS.bCreatePhysicsScene)
		{
			// Create the physics scene
			CreatePhysicsScene();
		}

		bShouldSimulatePhysics = IVS.bShouldSimulatePhysics;
		
		// Save off the value used to create the scene, so this UWorld can recreate its scene later
		bRequiresHitProxies = IVS.bRequiresHitProxies;
		GetRendererModule().AllocateScene(this, bRequiresHitProxies, IVS.bCreateFXSystem, FeatureLevel);
	}

	// Prepare AI systems
	if (IVS.bCreateNavigation || IVS.bCreateAISystem)
	{
		AWorldSettings* WorldSettings = GetWorldSettings();
		if (WorldSettings)
		{
			if (IVS.bCreateNavigation && WorldSettings->bEnableNavigationSystem)
		{
			UNavigationSystem::CreateNavigationSystem(this);
		}
			if (IVS.bCreateAISystem && WorldSettings->bEnableAISystem)
	{
		CreateAISystem();
	}
		}
	}
	
	if (GEngine->AvoidanceManagerClass != NULL)
	{
		AvoidanceManager = NewObject<UAvoidanceManager>(this, GEngine->AvoidanceManagerClass);
	}

#if WITH_EDITOR
	bEnableTraceCollision = IVS.bEnableTraceCollision;
#endif

	SetupParameterCollectionInstances();

	if (PersistentLevel->GetOuter() != this)
	{
		// Move persistent level into world so the world object won't get garbage collected in the multi- level
		// case as it is still referenced via the level's outer. This is required for multi- level editing to work.
		PersistentLevel->Rename( *PersistentLevel->GetName(), this, REN_ForceNoResetLoaders );
	}

	Levels.Empty(1);
	Levels.Add( PersistentLevel );
	PersistentLevel->OwningWorld = this;
	PersistentLevel->bIsVisible = true;

	RepairWorldSettings();

	// initialize DefaultPhysicsVolume for the world
	// Spawned on demand by this function.
	DefaultPhysicsVolume = GetDefaultPhysicsVolume();

	// Find gravity
	if (GetPhysicsScene())
	{
		FVector Gravity = FVector( 0.f, 0.f, GetGravityZ() );
		GetPhysicsScene()->SetUpForFrame( &Gravity );
	}

	// Create physics collision handler, if we have a physics scene
	if (IVS.bCreatePhysicsScene)
	{
		AWorldSettings* WorldSettings = GetWorldSettings();
		// First look for world override
		TSubclassOf<UPhysicsCollisionHandler> PhysHandlerClass = WorldSettings->PhysicsCollisionHandlerClass;
		// Then fall back to engine default
		if(PhysHandlerClass == NULL)
		{
			PhysHandlerClass = GEngine->PhysicsCollisionHandlerClass;
		}

		if (PhysHandlerClass != NULL)
		{
			PhysicsCollisionHandler = NewObject<UPhysicsCollisionHandler>(this, PhysHandlerClass);
			PhysicsCollisionHandler->InitCollisionHandler();
		}
	}

	URL					= PersistentLevel->URL;
	CurrentLevel		= PersistentLevel;

	bAllowAudioPlayback = IVS.bAllowAudioPlayback;
#if WITH_EDITOR
	// Disable audio playback on PIE dedicated server
	bAllowAudioPlayback = bAllowAudioPlayback && (GetNetMode() != NM_DedicatedServer);
#endif // WITH_EDITOR

	bDoDelayedUpdateCullDistanceVolumes = false;

#if WITH_EDITOR
	// See whether we're missing the default brush. It was possible in earlier builds to accidentally delete the default
	// brush of sublevels so we simply spawn a new one if we encounter it missing.
	ABrush* DefaultBrush = PersistentLevel->Actors.Num()<2 ? NULL : Cast<ABrush>(PersistentLevel->Actors[1]);
	if (GIsEditor)
	{
		if (!DefaultBrush || !DefaultBrush->IsStaticBrush() || DefaultBrush->BrushType!=Brush_Default || !DefaultBrush->GetBrushComponent() || !DefaultBrush->Brush)
		{
			// Spawn the default brush.
			DefaultBrush = SpawnBrush();
			check(DefaultBrush->GetBrushComponent());
			DefaultBrush->Brush = NewObject<UModel>(DefaultBrush->GetOuter(), TEXT("Brush"));
			DefaultBrush->Brush->Initialize(DefaultBrush, 1);
			DefaultBrush->GetBrushComponent()->Brush = DefaultBrush->Brush;
			DefaultBrush->SetNotForClientOrServer();
			DefaultBrush->Brush->SetFlags( RF_Transactional );
			DefaultBrush->Brush->Polys->SetFlags( RF_Transactional );

			// The default brush is legacy but has to exist for some old bsp operations.  However it should not be interacted with in the editor. 
			DefaultBrush->SetIsTemporarilyHiddenInEditor(true);

			// Find the index in the array the default brush has been spawned at. Not necessarily
			// the last index as the code might spawn the default physics volume afterwards.
			const int32 DefaultBrushActorIndex = PersistentLevel->Actors.Find( DefaultBrush );

			// The default brush needs to reside at index 1.
			Exchange(PersistentLevel->Actors[1],PersistentLevel->Actors[DefaultBrushActorIndex]);

			// Re-sort actor list as we just shuffled things around.
			PersistentLevel->SortActorList();
		}
		else
		{
			// Ensure that the Brush and BrushComponent both point to the same model
			DefaultBrush->GetBrushComponent()->Brush = DefaultBrush->Brush;
		}

		// Reset the lightmass settings on the default brush; they can't be edited by the user but could have
		// been tainted if the map was created during a window where the memory was uninitialized
		if (DefaultBrush->Brush != NULL)
		{
			UModel* Model = DefaultBrush->Brush;

			const FLightmassPrimitiveSettings DefaultSettings;

			for (int32 i = 0; i < Model->LightmassSettings.Num(); ++i)
			{
				Model->LightmassSettings[i] = DefaultSettings;
			}

			if (Model->Polys != NULL) 
			{
				for (int32 i = 0; i < Model->Polys->Element.Num(); ++i)
				{
					Model->Polys->Element[i].LightmassSettings = DefaultSettings;
				}
			}
		}
	}
#endif // WITH_EDITOR


	// update it's bIsDefaultLevel
	bIsDefaultLevel = (FPaths::GetBaseFilename(GetMapName()) == FPaths::GetBaseFilename(UGameMapsSettings::GetGameDefaultMap()));

	ConditionallyCreateDefaultLevelCollections();

	// We're initialized now.
	bIsWorldInitialized = true;

	//@TODO: Should this happen here, or below here?
	FWorldDelegates::OnPostWorldInitialization.Broadcast(this, IVS);

	PersistentLevel->PrecomputedVisibilityHandler.UpdateScene(Scene);
	PersistentLevel->PrecomputedVolumeDistanceField.UpdateScene(Scene);
	PersistentLevel->InitializeRenderingResources();

	BroadcastLevelsChanged();
}

void UWorld::ConditionallyCreateDefaultLevelCollections()
{
	// Create main level collection. The persistent level will always be considered dynamic.
	if (!FindCollectionByType(ELevelCollectionType::DynamicSourceLevels))
	{
		// Default to the dynamic source collection
		ActiveLevelCollectionIndex = FindOrAddCollectionByType_Index(ELevelCollectionType::DynamicSourceLevels);
		LevelCollections[ActiveLevelCollectionIndex].SetPersistentLevel(PersistentLevel);
		
		// Don't add the persistent level if it is already a member of another collection.
		// This may be the case if, for example, this world is the outer of a streaming level,
		// in which case the persistent level may be in one of the collections in the streaming level's OwningWorld.
		if (PersistentLevel->GetCachedLevelCollection() == nullptr)
		{
			LevelCollections[ActiveLevelCollectionIndex].AddLevel(PersistentLevel);
		}
	}

	if (!FindCollectionByType(ELevelCollectionType::StaticLevels))
	{
		FLevelCollection& StaticCollection = FindOrAddCollectionByType(ELevelCollectionType::StaticLevels);
		StaticCollection.SetPersistentLevel(PersistentLevel);
	}
}

void UWorld::InitializeNewWorld(const InitializationValues IVS)
{
	if (!IVS.bTransactional)
	{
		ClearFlags(RF_Transactional);
	}

	PersistentLevel = NewObject<ULevel>(this, TEXT("PersistentLevel"));
	PersistentLevel->Initialize(FURL(nullptr));
	PersistentLevel->Model = NewObject<UModel>(PersistentLevel);
	PersistentLevel->Model->Initialize(nullptr, 1);
	PersistentLevel->OwningWorld = this;

	// Mark objects are transactional for undo/ redo.
	if (IVS.bTransactional)
	{
		PersistentLevel->SetFlags( RF_Transactional );
		PersistentLevel->Model->SetFlags( RF_Transactional );
	}
	else
	{
		PersistentLevel->ClearFlags( RF_Transactional );
		PersistentLevel->Model->ClearFlags( RF_Transactional );
	}

	// Need to associate current level so SpawnActor doesn't complain.
	CurrentLevel = PersistentLevel;

	// Create the WorldInfo actor.
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Set constant name for WorldSettings to make a network replication work between new worlds on host and client
	SpawnInfo.Name = GEngine->WorldSettingsClass->GetFName();
	AWorldSettings* WorldSettings = SpawnActor<AWorldSettings>( GEngine->WorldSettingsClass, SpawnInfo );
	PersistentLevel->SetWorldSettings(WorldSettings);
	check(GetWorldSettings());
#if WITH_EDITOR
	WorldSettings->SetIsTemporarilyHiddenInEditor(true);
#endif

	// Initialize the world
	InitWorld(IVS);

	// Update components.
	UpdateWorldComponents( true, false );
}


void UWorld::DestroyWorld( bool bInformEngineOfWorld, UWorld* NewWorld )
{
	// Clean up existing world and remove it from root set so it can be garbage collected.
	bIsLevelStreamingFrozen = false;
	bShouldForceUnloadStreamingLevels = true;
	FlushLevelStreaming();
	CleanupWorld(true, true, NewWorld);

	ForEachNetDriver(GEngine, this, [](UNetDriver* const Driver)
	{
		if (Driver != nullptr)
		{
			check(Driver->GetNetworkObjectList().GetAllObjects().Num() == 0);
			check(Driver->GetNetworkObjectList().GetActiveObjects().Num() == 0);
		}
	});

	// Tell the engine we are destroying the world.(unless we are asked not to)
	if( ( GEngine ) && ( bInformEngineOfWorld == true ) )
	{
		GEngine->WorldDestroyed( this );
	}		
	RemoveFromRoot();
	ClearFlags(RF_Standalone);
	
	for (int32 LevelIndex=0; LevelIndex < GetNumLevels(); ++LevelIndex)
	{
		UWorld* World = CastChecked<UWorld>(GetLevel(LevelIndex)->GetOuter());
		if (World != this && World != NewWorld)
		{
			World->ClearFlags(RF_Standalone);
		}
	}
}

void UWorld::MarkObjectsPendingKill()
{
	auto MarkObjectPendingKill = [](UObject* Object)
	{
		Object->MarkPendingKill();
	};
	ForEachObjectWithOuter(this, MarkObjectPendingKill, true, RF_NoFlags, EInternalObjectFlags::PendingKill);
}

UWorld* UWorld::CreateWorld(const EWorldType::Type InWorldType, bool bInformEngineOfWorld, FName WorldName, UPackage* InWorldPackage, bool bAddToRoot, ERHIFeatureLevel::Type InFeatureLevel)
{
	if (InFeatureLevel >= ERHIFeatureLevel::Num)
	{
		InFeatureLevel = GMaxRHIFeatureLevel;
	}

	UPackage* WorldPackage = InWorldPackage;
	if ( !WorldPackage )
	{
		WorldPackage = CreatePackage( NULL, NULL );
	}

	if (InWorldType == EWorldType::PIE)
	{
		WorldPackage->SetPackageFlags(PKG_PlayInEditor);
	}

	// Mark the package as containing a world.  This has to happen here rather than at serialization time,
	// so that e.g. the referenced assets browser will work correctly.
	if ( WorldPackage != GetTransientPackage() )
	{
		WorldPackage->ThisContainsMap();
	}

	// Create new UWorld, ULevel and UModel.
	const FString WorldNameString = (WorldName != NAME_None) ? WorldName.ToString() : TEXT("Untitled");
	UWorld* NewWorld = NewObject<UWorld>(WorldPackage, *WorldNameString);
	NewWorld->SetFlags(RF_Transactional);
	NewWorld->WorldType = InWorldType;
	NewWorld->FeatureLevel = InFeatureLevel;
	NewWorld->InitializeNewWorld(UWorld::InitializationValues().ShouldSimulatePhysics(false).EnableTraceCollision(true).CreateNavigation(InWorldType == EWorldType::Editor).CreateAISystem(InWorldType == EWorldType::Editor));

	// Clear the dirty flag set during SpawnActor and UpdateLevelComponents
	WorldPackage->SetDirtyFlag(false);

	if ( bAddToRoot )
	{
		// Add to root set so it doesn't get garbage collected.
		NewWorld->AddToRoot();
	}

	// Tell the engine we are adding a world (unless we are asked not to)
	if( ( GEngine ) && ( bInformEngineOfWorld == true ) )
	{
		GEngine->WorldAdded( NewWorld );
	}

	return NewWorld;
}

void UWorld::RemoveActor(AActor* Actor, bool bShouldModifyLevel)
{
	bool	bSuccessfulRemoval = false;
	ULevel* CheckLevel = Actor->GetLevel();
	int32 ActorListIndex = CheckLevel->Actors.Find( Actor );
	// Search the entire list.
	if( ActorListIndex != INDEX_NONE )
	{
		if ( bShouldModifyLevel && GUndo )
		{
			ModifyLevel( CheckLevel );
		}
		
		if (!IsGameWorld())
		{
			CheckLevel->Actors[ActorListIndex]->Modify();
		}
		
		CheckLevel->Actors[ActorListIndex] = NULL;
		bSuccessfulRemoval = true;		
	}

	// Remove actor from network list
	RemoveNetworkActor( Actor );

	// TTP 281860: Callstack will hopefully indicate how the actors array ends up without the required default actors
	check(CheckLevel->Actors.Num() >= 2);

	if ( !bSuccessfulRemoval && !( Actor->GetFlags() & RF_Transactional ) )
	{
		//CheckLevel->Actors is a transactional array so it is very likely that non-transactional
		//actors could be missing from the array if the array was reverted to a state before they
		//existed. (but they won't be reverted since they are non-transactional)
		bSuccessfulRemoval = true;
	}

	if ( !bSuccessfulRemoval )
	{
		// TTP 270000: Trying to track down why certain actors aren't in the level actor list when saving.  If we're reinstancing, dump the list
		{
			UE_LOG(LogWorld, Log, TEXT("--- Actors Currently in %s ---"), *CheckLevel->GetPathName());
			for (int32 ActorIdx = 0; ActorIdx < CheckLevel->Actors.Num(); ActorIdx++)
			{
				AActor* CurrentActor = CheckLevel->Actors[ActorIdx];
				UE_LOG(LogWorld, Log, TEXT("  %s"), (CurrentActor ? *CurrentActor->GetPathName() : TEXT("NONE")));
			}
		}
		ensureMsgf(false, TEXT("Could not remove actor %s from world (check level is %s)"), *Actor->GetPathName(), *CheckLevel->GetPathName());
	}
}


bool UWorld::ContainsActor( AActor* Actor )
{
	return (Actor && Actor->GetWorld() == this);
}

bool UWorld::AllowAudioPlayback()
{
	return bAllowAudioPlayback;
}

#if WITH_EDITOR
void UWorld::ShrinkLevel()
{
	GetModel()->ShrinkModel();
}
#endif // WITH_EDITOR

void UWorld::ClearWorldComponents()
{
	for( int32 LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
	{
		ULevel* Level = Levels[LevelIndex];
		Level->ClearLevelComponents();
	}

	if(LineBatcher && LineBatcher->IsRegistered())
	{
		LineBatcher->UnregisterComponent();
	}

	if(PersistentLineBatcher && PersistentLineBatcher->IsRegistered())
	{
		PersistentLineBatcher->UnregisterComponent();
	}

	if(ForegroundLineBatcher && ForegroundLineBatcher->IsRegistered())
	{
		ForegroundLineBatcher->UnregisterComponent();
	}
}


void UWorld::UpdateWorldComponents(bool bRerunConstructionScripts, bool bCurrentLevelOnly)
{
	if ( !IsRunningDedicatedServer() )
	{
		if(!LineBatcher)
		{
			LineBatcher = NewObject<ULineBatchComponent>();
			LineBatcher->bCalculateAccurateBounds = false;
		}

		if(!LineBatcher->IsRegistered())
		{	
			LineBatcher->RegisterComponentWithWorld(this);
		}

		if(!PersistentLineBatcher)
		{
			PersistentLineBatcher = NewObject<ULineBatchComponent>();
			PersistentLineBatcher->bCalculateAccurateBounds = false;
		}

		if(!PersistentLineBatcher->IsRegistered())	
		{
			PersistentLineBatcher->RegisterComponentWithWorld(this);
		}

		if(!ForegroundLineBatcher)
		{
			ForegroundLineBatcher = NewObject<ULineBatchComponent>();
			ForegroundLineBatcher->bCalculateAccurateBounds = false;
		}

		if(!ForegroundLineBatcher->IsRegistered())	
		{
			ForegroundLineBatcher->RegisterComponentWithWorld(this);
		}
	}

	if ( bCurrentLevelOnly )
	{
		check( CurrentLevel );
		CurrentLevel->UpdateLevelComponents(bRerunConstructionScripts);
	}
	else
	{
		for( int32 LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
		{
			ULevel* Level = Levels[LevelIndex];
			ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel(Level);
			// Update the level only if it is visible (or not a streamed level)
			if(!StreamingLevel || Level->bIsVisible)
			{
				Level->UpdateLevelComponents(bRerunConstructionScripts);
				IStreamingManager::Get().AddLevel(Level);
			}
		}
	}

	UpdateCullDistanceVolumes();
}


void UWorld::UpdateCullDistanceVolumes(AActor* ActorToUpdate, UPrimitiveComponent* ComponentToUpdate)
{
	// Map that will store new max draw distance for every primitive
	TMap<UPrimitiveComponent*,float> CompToNewMaxDrawMap;

	// Keep track of time spent.
	double Duration = 0.0;
	{
		SCOPE_SECONDS_COUNTER(Duration);

		TArray<ACullDistanceVolume*> CullDistanceVolumes;

		// Establish base line of LD specified cull distances.
		if (ActorToUpdate || ComponentToUpdate)
		{
			if (ComponentToUpdate)
			{
				check((ActorToUpdate == nullptr) || (ActorToUpdate == ComponentToUpdate->GetOwner()));
				if (ACullDistanceVolume::CanBeAffectedByVolumes(ComponentToUpdate))
				{
				CompToNewMaxDrawMap.Add(ComponentToUpdate, ComponentToUpdate->LDMaxDrawDistance);
			}
			}
			else
			{
				TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(ActorToUpdate);
				for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
				{
					if (ACullDistanceVolume::CanBeAffectedByVolumes(PrimitiveComponent))
					{
						CompToNewMaxDrawMap.Add(PrimitiveComponent, PrimitiveComponent->LDMaxDrawDistance);
					}
				}
			}

			if (CompToNewMaxDrawMap.Num() > 0)
			{
				for (TActorIterator<ACullDistanceVolume> It(this); It; ++It)
				{
					CullDistanceVolumes.Add(*It);
				}
			}
		}
		else
		{
			for( AActor* Actor : FActorRange(this) )
			{
				TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Actor);
				for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
				{
					if (ACullDistanceVolume::CanBeAffectedByVolumes(PrimitiveComponent))
					{
						CompToNewMaxDrawMap.Add(PrimitiveComponent, PrimitiveComponent->LDMaxDrawDistance);
					}
				}

				ACullDistanceVolume* CullDistanceVolume = Cast<ACullDistanceVolume>(Actor);
				if (CullDistanceVolume)
				{
					CullDistanceVolumes.Add(CullDistanceVolume);
				}
			}
		}

		// Iterate over all cull distance volumes and get new cull distances.
		if (CompToNewMaxDrawMap.Num() > 0)
		{
			for (ACullDistanceVolume* CullDistanceVolume : CullDistanceVolumes)
			{
				CullDistanceVolume->GetPrimitiveMaxDrawDistances(CompToNewMaxDrawMap);
			}
		}

		// Finally, go over all primitives, and see if they need to change.
		// Only if they do do we reregister them, as thats slow.
		for ( TMap<UPrimitiveComponent*,float>::TIterator It(CompToNewMaxDrawMap); It; ++It )
		{
			UPrimitiveComponent* PrimComp = It.Key();
			const float NewMaxDrawDist = It.Value();

			PrimComp->SetCachedMaxDrawDistance(NewMaxDrawDist);
		}
	}

	if( Duration > 1.f )
	{
		UE_LOG(LogWorld, Log, TEXT("Updating cull distance volumes took %5.2f seconds"),Duration);
	}
}


void UWorld::ModifyLevel(ULevel* Level)
{
	if( Level && Level->HasAnyFlags(RF_Transactional))
	{
		Level->Modify( false );
		//Level->Actors.ModifyAllItems();
		Level->Model->Modify( false );
	}
}

void UWorld::EnsureCollisionTreeIsBuilt()
{
	if (bInTick || bIsBuilt)
	{
		// Current implementation of collision tree rebuild ticks physics scene and can not be called during world tick
		return;
	}

	if (GIsEditor && !IsPlayInEditor())
	{
		// Don't simulate physics in the editor
		return;
	}
	
	// Set physics to static loading mode
	if (PhysicsScene)
	{
		PhysicsScene->EnsureCollisionTreeIsBuilt(this);
	}

	bIsBuilt = true;
}

void UWorld::InvalidateModelGeometry(ULevel* InLevel)
{
	if ( InLevel )
	{
		InLevel->InvalidateModelGeometry();
	}
	else
	{
		for( int32 LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
		{
			ULevel* Level = Levels[LevelIndex];
			Level->InvalidateModelGeometry();
		}
	}
}


void UWorld::InvalidateModelSurface(bool bCurrentLevelOnly)
{
	if ( bCurrentLevelOnly )
	{
		check( bCurrentLevelOnly );
		CurrentLevel->InvalidateModelSurface();
	}
	else
	{
		for( int32 LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
		{
			ULevel* Level = Levels[LevelIndex];
			Level->InvalidateModelSurface();
		}
	}
}


void UWorld::CommitModelSurfaces()
{
	for( int32 LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
	{
		ULevel* Level = Levels[LevelIndex];
		Level->CommitModelSurfaces();
	}
}


void UWorld::TransferBlueprintDebugReferences(UWorld* NewWorld)
{
#if WITH_EDITOR
	// First create a list of blueprints that already exist in the new world
	TArray< FString > NewWorldExistingBlueprintNames;
	for (FBlueprintToDebuggedObjectMap::TIterator It(NewWorld->BlueprintObjectsBeingDebugged); It; ++It)
	{
		if (UBlueprint* TargetBP = It.Key().Get())
		{	
			NewWorldExistingBlueprintNames.AddUnique( TargetBP->GetName() );
		}
	}

	// Move debugging object associations across from the old world to the new world that are not already there
	for (FBlueprintToDebuggedObjectMap::TIterator It(BlueprintObjectsBeingDebugged); It; ++It)
	{
		if (UBlueprint* TargetBP = It.Key().Get())
		{	
			FString SourceName = TargetBP->GetName();
			// If this blueprint is not already listed in the ones bieng debugged in the new world, add it.
			if( NewWorldExistingBlueprintNames.Find( SourceName ) == INDEX_NONE )			
			{
				TWeakObjectPtr<UObject>& WeakTargetObject = It.Value();
				UObject* NewTargetObject = nullptr;

				if (WeakTargetObject.IsValid())
				{
					UObject* OldTargetObject = WeakTargetObject.Get();
					check(OldTargetObject);

					NewTargetObject = FindObject<UObject>(NewWorld, *OldTargetObject->GetPathName(this));
				}

				if (NewTargetObject != nullptr)
				{
					// Check to see if the object we found to transfer to is of a different class.  LevelScripts are always exceptions, because a new level may have been loaded in PIE, and we have special handling for LSA debugging objects
					if (!NewTargetObject->IsA(TargetBP->GeneratedClass))
					{
						const FString BlueprintFullPath = TargetBP->GetPathName();

						if (BlueprintFullPath.StartsWith(TEXT("/Temp/Autosaves")) || BlueprintFullPath.StartsWith(TEXT("/Temp//Autosaves")))
						{
							// This map was an autosave for networked PIE; it's OK to fail to fix
							// up the blueprint object being debugged reference as the whole blueprint
							// is going away.
						}
						else if(!NewTargetObject->IsA(ALevelScriptActor::StaticClass()))
						{
							// Let the ensure fire
							UE_LOG(LogWorld, Warning, TEXT("Found object to debug in main world that isn't the correct type"));
							UE_LOG(LogWorld, Warning, TEXT("  TargetBP path is %s"), *TargetBP->GetPathName());
							UE_LOG(LogWorld, Warning, TEXT("  TargetBP gen class path is %s"), *TargetBP->GeneratedClass->GetPathName());
							UE_LOG(LogWorld, Warning, TEXT("  NewTargetObject path is %s"), *NewTargetObject->GetPathName());
							UE_LOG(LogWorld, Warning, TEXT("  NewTargetObject class path is %s"), *NewTargetObject->GetClass()->GetPathName());

							UObject* OldTargetObject = WeakTargetObject.Get();
							UE_LOG(LogWorld, Warning, TEXT("  OldObject path is %s"), *OldTargetObject->GetPathName());
							UE_LOG(LogWorld, Warning, TEXT("  OldObject class path is %s"), *OldTargetObject->GetClass()->GetPathName());

							ensureMsgf(false, TEXT("Failed to find an appropriate object to debug back in the editor world"));
						}

						NewTargetObject = nullptr;
					}
				}

				TargetBP->SetObjectBeingDebugged(NewTargetObject);
			}
		}
	}
	// Ensure the level script actor debug references are transferred to the new world.
	if (NewWorld)
	{
		if (ALevelScriptActor* LevelScript = NewWorld->GetLevelScriptActor())
		{
			if (UBlueprint* LevelScriptBlueprint = Cast<UBlueprint>(LevelScript->GetClass()->ClassGeneratedBy))
			{
				LevelScriptBlueprint->SetObjectBeingDebugged(LevelScript);
			}
		}
	}
	// Empty the map, anything useful got moved over the map in the new world
	BlueprintObjectsBeingDebugged.Empty();
#endif	//#if WITH_EDITOR
}


void UWorld::NotifyOfBlueprintDebuggingAssociation(class UBlueprint* Blueprint, UObject* DebugObject)
{
#if WITH_EDITOR
	TWeakObjectPtr<UBlueprint> Key(Blueprint);

	if (DebugObject)
	{
		BlueprintObjectsBeingDebugged.FindOrAdd(Key) = DebugObject;
	}
	else
	{
		BlueprintObjectsBeingDebugged.Remove(Key);
	}
#endif	//#if WITH_EDITOR
}

void UWorld::BroadcastLevelsChanged()
{
	LevelsChangedEvent.Broadcast();
#if WITH_EDITOR
	FWorldDelegates::RefreshLevelScriptActions.Broadcast(this);
#endif	//#if WITH_EDITOR
}

DEFINE_STAT(STAT_AddToWorldTime);
DEFINE_STAT(STAT_RemoveFromWorldTime);
DEFINE_STAT(STAT_UpdateLevelStreamingTime);

/**
 * Static helper function for AddToWorld to determine whether we've already spent all the allotted time.
 *
 * @param	CurrentTask		Description of last task performed
 * @param	StartTime		StartTime, used to calculate time passed
 * @param	Level			Level work has been performed on
 *
 * @return true if time limit has been exceeded, false otherwise
 */
static bool IsTimeLimitExceeded( const TCHAR* CurrentTask, double StartTime, ULevel* Level, double TimeLimit = 0.0 )
{
	bool bIsTimeLimitExceed = false;
	// We don't spread work across several frames in the Editor to avoid potential side effects.
	if( Level->OwningWorld->IsGameWorld() == true )
	{
		if (TimeLimit == 0.0)
		{
			TimeLimit = GLevelStreamingActorsUpdateTimeLimit;
		}
		double CurrentTime	= FPlatformTime::Seconds();
		// Delta time in ms.
		double DeltaTime	= (CurrentTime - StartTime) * 1000;
		if( DeltaTime > TimeLimit )
		{
			// Log if a single event took way too much time.
			if( DeltaTime > 20 )
			{
				UE_LOG(LogStreaming, Display, TEXT("UWorld::AddToWorld: %s for %s took (less than) %5.2f ms"), CurrentTask, *Level->GetOutermost()->GetName(), DeltaTime );
			}
			bIsTimeLimitExceed = true;
		}
	}
	return bIsTimeLimitExceed;
}

#if PERF_TRACK_DETAILED_ASYNC_STATS
// Variables for tracking how long each part of the AddToWorld process takes
double MoveActorTime = 0.0;
double ShiftActorsTime = 0.0;
double UpdateComponentsTime = 0.0;
double InitBSPPhysTime = 0.0;
double InitActorPhysTime = 0.0;
double InitActorTime = 0.0;
double RouteActorInitializeTime = 0.0;
double CrossLevelRefsTime = 0.0;
double SortActorListTime = 0.0;
double PerformLastStepTime = 0.0;

/** Helper class, to add the time between this objects creating and destruction to passed in variable. */
class FAddWorldScopeTimeVar
{
public:
	FAddWorldScopeTimeVar(double* Time)
	{
		TimeVar = Time;
		Start = FPlatformTime::Seconds();
	}

	~FAddWorldScopeTimeVar()
	{
		*TimeVar += (FPlatformTime::Seconds() - Start);
	}

private:
	/** Pointer to value to add to when object is destroyed */
	double* TimeVar;
	/** The time at which this object was created */
	double	Start;
};

/** Macro to create a scoped timer for the supplied var */
#define SCOPE_TIME_TO_VAR(V) FAddWorldScopeTimeVar TimeVar(V)

#else

/** Empty macro, when not doing timing */
#define SCOPE_TIME_TO_VAR(V)

#endif // PERF_TRACK_DETAILED_ASYNC_STATS

void UWorld::AddToWorld( ULevel* Level, const FTransform& LevelTransform )
{
	SCOPE_CYCLE_COUNTER(STAT_AddToWorldTime);

	check(Level);
	check(!Level->IsPendingKill());
	check(!Level->IsUnreachable());

	FScopeCycleCounterUObject ContextScope(Level);

	// Set flags to indicate that we are associating a level with the world to e.g. perform slower/ better octree insertion 
	// and such, as opposed to the fast path taken for run-time/ gameplay objects.
	Level->bIsAssociatingLevel = true;

	const double StartTime = FPlatformTime::Seconds();

	// Don't consider the time limit if the match hasn't started as we need to ensure that the levels are fully loaded
	const bool bConsiderTimeLimit = bMatchStarted;

	bool bExecuteNextStep = CurrentLevelPendingVisibility == Level || CurrentLevelPendingVisibility == NULL;
	bool bPerformedLastStep	= false;
	
	// Don't make this level visible if it's currently being made invisible
	if( bExecuteNextStep && CurrentLevelPendingVisibility == NULL && CurrentLevelPendingInvisibility != Level )
	{
		Level->OwningWorld = this;
		
		// Mark level as being the one in process of being made visible.
		CurrentLevelPendingVisibility = Level;

		// Add to the UWorld's array of levels, which causes it to be rendered et al.
		Levels.AddUnique( Level );
		
#if PERF_TRACK_DETAILED_ASYNC_STATS
		MoveActorTime = 0.0;
		ShiftActorsTime = 0.0;
		UpdateComponentsTime = 0.0;
		InitBSPPhysTime = 0.0;
		InitActorPhysTime = 0.0;
		InitActorTime = 0.0;
		RouteActorInitializeTime = 0.0;
		CrossLevelRefsTime = 0.0;
		SortActorListTime = 0.0;
		PerformLastStepTime = 0.0;
#endif
	}

	if( bExecuteNextStep && !Level->bAlreadyMovedActors )
	{
		SCOPE_TIME_TO_VAR(&MoveActorTime);

		FLevelUtils::ApplyLevelTransform( Level, LevelTransform, false );

		Level->bAlreadyMovedActors = true;
		bExecuteNextStep = (!bConsiderTimeLimit || !IsTimeLimitExceeded( TEXT("moving actors"), StartTime, Level ));
	}

	if( bExecuteNextStep && !Level->bAlreadyShiftedActors )
	{
		SCOPE_TIME_TO_VAR(&ShiftActorsTime);

		// Notify world composition: will place level actors according to current world origin
		if (WorldComposition)
		{
			WorldComposition->OnLevelAddedToWorld(Level);
		}
		
		Level->bAlreadyShiftedActors = true;
		bExecuteNextStep = (!bConsiderTimeLimit || !IsTimeLimitExceeded( TEXT("shifting actors"), StartTime, Level ));
	}

	if (bExecuteNextStep && AsyncPreRegisterLevelStreamingTasks.GetValue())
	{
		if (!bConsiderTimeLimit)
		{
			QUICK_SCOPE_CYCLE_COUNTER(UWorld_AddToWorld_WaitFor_AsyncPreRegisterLevelStreamingTasks);
			while (AsyncPreRegisterLevelStreamingTasks.GetValue())
			{
				FPlatformProcess::Sleep(0.001f);
			}
		}
		else
		{
			bExecuteNextStep = false;
		}
	}

	// Wait on any async DDC handles
#if WITH_EDITOR
	if (bExecuteNextStep && AsyncPreRegisterDDCRequests.Num())
	{
		if (!bConsiderTimeLimit)
		{
			QUICK_SCOPE_CYCLE_COUNTER(UWorld_AddToWorld_WaitFor_AsyncPreRegisterLevelStreamingTasks);

			for (TSharedPtr<FAsyncPreRegisterDDCRequest>& Request : AsyncPreRegisterDDCRequests)
			{
				Request->WaitAsynchronousCompletion();
			}
			AsyncPreRegisterDDCRequests.Empty();
		}
		else
		{
			for (int32 Index = 0; Index < AsyncPreRegisterDDCRequests.Num(); Index++)
			{
				if (AsyncPreRegisterDDCRequests[Index]->PollAsynchronousCompletion())
				{
					AsyncPreRegisterDDCRequests.RemoveAtSwap(Index--);
				}
				else
				{
					bExecuteNextStep = false;
					break;
				}
			}
		}
	}
#endif

	// Updates the level components (Actor components and UModelComponents).
	if( bExecuteNextStep && !Level->bAlreadyUpdatedComponents )
	{
		SCOPE_TIME_TO_VAR(&UpdateComponentsTime);

		// Make sure code thinks components are not currently attached.
		Level->bAreComponentsCurrentlyRegistered = false;

#if WITH_EDITOR
			// Pretend here that we are loading package to avoid package dirtying during components registration
		TGuardValue<bool> IsEditorLoadingPackage(GIsEditorLoadingPackage, (GIsEditor ? true : GIsEditorLoadingPackage));
#endif

		// Config bool that allows disabling all construction scripts during PIE level streaming.
		bool bRerunConstructionDuringEditorStreaming = true;
		GConfig->GetBool(TEXT("Kismet"), TEXT("bRerunConstructionDuringEditorStreaming"), /*out*/ bRerunConstructionDuringEditorStreaming, GEngineIni);

		// We don't need to rerun construction scripts if we have cooked data or we are playing in editor unless the PIE world was loaded
		// from disk rather than duplicated
		const bool bRerunConstructionScript = !(FPlatformProperties::RequiresCookedData() || (IsGameWorld() && (Level->bHasRerunConstructionScripts || !bRerunConstructionDuringEditorStreaming)));
		
		// Incrementally update components.
		int32 NumComponentsToUpdate = GLevelStreamingComponentsRegistrationGranularity;
		do
		{
			Level->IncrementalUpdateComponents( (!IsGameWorld() || IsRunningCommandlet()) ? 0 : NumComponentsToUpdate, bRerunConstructionScript );
		}
		while( !Level->bAreComponentsCurrentlyRegistered && (!bConsiderTimeLimit || !IsTimeLimitExceeded( TEXT("updating components"), StartTime, Level )));

		// We are done once all components are attached.
		Level->bAlreadyUpdatedComponents	= Level->bAreComponentsCurrentlyRegistered;
		bExecuteNextStep					= Level->bAreComponentsCurrentlyRegistered && (!bConsiderTimeLimit || !IsTimeLimitExceeded(TEXT("updating components"), StartTime, Level));
	}

	if( IsGameWorld() && AreActorsInitialized() )
	{
		// Initialize all actors and start execution.
		if (bExecuteNextStep && !Level->bAlreadyInitializedNetworkActors)
		{
			SCOPE_TIME_TO_VAR(&InitActorTime);

			Level->InitializeNetworkActors();
			Level->bAlreadyInitializedNetworkActors = true;
			bExecuteNextStep = (!bConsiderTimeLimit || !IsTimeLimitExceeded( TEXT("initializing network actors"), StartTime, Level ));
		}

		// Route various initialization functions and set volumes.
		if( bExecuteNextStep && !Level->bAlreadyRoutedActorInitialize )
		{
			SCOPE_TIME_TO_VAR(&RouteActorInitializeTime);
			bStartup = 1;
			Level->RouteActorInitialize();
			Level->bAlreadyRoutedActorInitialize = true;
			bStartup = 0;

			bExecuteNextStep = (!bConsiderTimeLimit || !IsTimeLimitExceeded( TEXT("routing Initialize on actors"), StartTime, Level ));
		}

		// Sort the actor list; can't do this on save as the relevant properties for sorting might have been changed by code
		if( bExecuteNextStep && !Level->bAlreadySortedActorList )
		{
			SCOPE_TIME_TO_VAR(&SortActorListTime);

			Level->SortActorList();
			Level->bAlreadySortedActorList = true;
			bExecuteNextStep = (!bConsiderTimeLimit || !IsTimeLimitExceeded( TEXT("sorting actor list"), StartTime, Level ));
			bPerformedLastStep = true;
		}
	}
	else
	{
		bPerformedLastStep = true;
	}

	Level->bIsAssociatingLevel = false;

	// We're done.
	if( bPerformedLastStep )
	{
		SCOPE_TIME_TO_VAR(&PerformLastStepTime);
		
		Level->bAlreadyShiftedActors					= false;
		Level->bAlreadyUpdatedComponents				= false;
		Level->bAlreadyInitializedNetworkActors			= false;
		Level->bAlreadyRoutedActorInitialize			= false;
		Level->bAlreadySortedActorList					= false;

		// Finished making level visible - allow other levels to be added to the world.
		CurrentLevelPendingVisibility					= NULL;

		// notify server that the client has finished making this level visible
		if (!Level->bClientOnlyVisible)
		{
			for (FLocalPlayerIterator It(GEngine, this); It; ++It)
			{
				APlayerController* LocalPlayerController = It->GetPlayerController(this);
				if (LocalPlayerController != NULL)
				{
					LocalPlayerController->ServerUpdateLevelVisibility(LocalPlayerController->NetworkRemapPath(Level->GetOutermost()->GetFName(), false), true);
				}
			}
		}

		Level->InitializeRenderingResources();

		// Notify the texture streaming system now that everything is set up.
		IStreamingManager::Get().AddLevel( Level );

		Level->bIsVisible = true;
	
		// send a callback that a level was added to the world
		FWorldDelegates::LevelAddedToWorld.Broadcast(Level, this);

		BroadcastLevelsChanged();

		ULevelStreaming::BroadcastLevelVisibleStatus(this, Level->GetOutermost()->GetFName(), true);
	}

#if PERF_TRACK_DETAILED_ASYNC_STATS
	if (bPerformedLastStep)
	{
		// Log out all of the timing information
		double TotalTime = 
			MoveActorTime + 
			ShiftActorsTime + 
			UpdateComponentsTime + 
			InitBSPPhysTime + 
			InitActorPhysTime + 
			InitActorTime + 
			RouteActorInitializeTime + 
			CrossLevelRefsTime + 
			SortActorListTime + 
			PerformLastStepTime;

		UE_LOG(LogStreaming, Display, TEXT("Detailed AddToWorld stats for '%s' - Total %6.2fms"), *Level->GetOutermost()->GetName(), TotalTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Move Actors             : %6.2f ms"), MoveActorTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Shift Actors            : %6.2f ms"), ShiftActorsTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Update Components       : %6.2f ms"), UpdateComponentsTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Init BSP Phys           : %6.2f ms"), InitBSPPhysTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Init Actor Phys         : %6.2f ms"), InitActorPhysTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Init Actors             : %6.2f ms"), InitActorTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Initialize              : %6.2f ms"), RouteActorInitializeTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Cross Level Refs        : %6.2f ms"), CrossLevelRefsTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Sort Actor List         : %6.2f ms"), SortActorListTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Perform Last Step       : %6.2f ms"), SortActorListTime * 1000 );
	}
#endif // PERF_TRACK_DETAILED_ASYNC_STATS
}

void UWorld::RemoveFromWorld( ULevel* Level, bool bAllowIncrementalRemoval )
{
	SCOPE_CYCLE_COUNTER(STAT_RemoveFromWorldTime);
	
	FScopeCycleCounterUObject Context(Level);
	check(Level);
	check(!Level->IsPendingKill());
	check(!Level->IsUnreachable());

	if ( CurrentLevelPendingVisibility == nullptr && Level->bIsVisible )
	{
		// Keep track of timing.
		double StartTime = FPlatformTime::Seconds();	

		bool bFinishRemovingLevel = true;
		if ( bAllowIncrementalRemoval && GLevelStreamingUnregisterComponentsTimeLimit > 0.0 )
		{
			bFinishRemovingLevel = false;
			if (CurrentLevelPendingInvisibility == nullptr)
			{
				// Mark level as being the one in process of being made invisible. 
				// This will prevent this level from being unloaded or made visible in the meantime
				CurrentLevelPendingInvisibility = Level;
				Level->bIsBeingRemoved = true;
			}

			if ( CurrentLevelPendingInvisibility == Level )
			{
				// Incrementally unregister actor components. 
				// This avoids spikes on the renderthread and gamethread when we subsequently call ClearLevelComponents() further down
				check(IsGameWorld());
				int32 NumComponentsToUnregister = GLevelStreamingComponentsUnregistrationGranularity;
				do
				{
					if (Level->IncrementalUnregisterComponents(NumComponentsToUnregister))
					{
						// We're done, so the level can be removed
						CurrentLevelPendingInvisibility = nullptr;
						bFinishRemovingLevel = true;
						break;
					}
				} while (!IsTimeLimitExceeded(TEXT("unregistering components"), StartTime, Level, GLevelStreamingUnregisterComponentsTimeLimit));
			}
		}
		else
		{
			Level->bIsBeingRemoved = true;
		}

		if ( bFinishRemovingLevel )
		{
			for (int32 ActorIdx = 0; ActorIdx < Level->Actors.Num(); ActorIdx++)
			{
				if (AActor* Actor = Level->Actors[ActorIdx])
				{
					Actor->RouteEndPlay(EEndPlayReason::RemovedFromWorld);
				}
			}

			// Remove any pawns from the pawn list that are about to be streamed out
			for( FConstPawnIterator Iterator = GetPawnIterator(); Iterator; ++Iterator )
			{
				APawn* Pawn = Iterator->Get();
				if (Pawn->IsInLevel(Level))
				{
					RemovePawn(Pawn);
					--Iterator;
				}
				else if (UCharacterMovementComponent* CharacterMovement = Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent()))
				{
					// otherwise force floor check in case the floor was streamed out from under it
					CharacterMovement->bForceNextFloorCheck = true;
				}
			}

			Level->ReleaseRenderingResources();

			// Remove from the world's level array and destroy actor components.
			IStreamingManager::Get().RemoveLevel( Level );
		
			Level->ClearLevelComponents();

			// notify server that the client has removed this level
			if (!Level->bClientOnlyVisible)
			{
				for (FLocalPlayerIterator It(GEngine, this); It; ++It)
				{
					APlayerController* LocalPlayerController = It->GetPlayerController(this);
					if (LocalPlayerController != NULL)
					{
						LocalPlayerController->ServerUpdateLevelVisibility(LocalPlayerController->NetworkRemapPath(Level->GetOutermost()->GetFName(), false), false);
					}
				}
			}

			Level->bIsVisible = false;

			// Notify world composition: will place a level at original position
			if (WorldComposition)
			{
				WorldComposition->OnLevelRemovedFromWorld(Level);
			}

			// Make sure level always has OwningWorld in the editor
			if (IsGameWorld())
			{
				Levels.Remove(Level);
				Level->OwningWorld = nullptr;
			}
				
			// let the universe know we have removed a level
			FWorldDelegates::LevelRemovedFromWorld.Broadcast(Level, this);
			BroadcastLevelsChanged();

			ULevelStreaming::BroadcastLevelVisibleStatus(this, Level->GetOutermost()->GetFName(), false);

			Level->bIsBeingRemoved = false;
		} // if ( bFinishRemovingLevel )

#if PERF_TRACK_DETAILED_ASYNC_STATS
		UE_LOG(LogStreaming, Display, TEXT("UWorld::RemoveFromWorld for %s took %5.2f ms"), *Level->GetOutermost()->GetName(), (FPlatformTime::Seconds() - StartTime) * 1000.0);
#endif // PERF_TRACK_DETAILED_ASYNC_STATS
	}
}

/************************************************************************/
/* FLevelStreamingGCHelper implementation                               */
/************************************************************************/
FLevelStreamingGCHelper::FOnGCStreamedOutLevelsEvent FLevelStreamingGCHelper::OnGCStreamedOutLevels;
TArray<TWeakObjectPtr<ULevel> > FLevelStreamingGCHelper::LevelsPendingUnload;
TArray<FName> FLevelStreamingGCHelper::LevelPackageNames;

void FLevelStreamingGCHelper::AddGarbageCollectorCallback()
{
	// Only register for garbage collection once
	static bool GarbageCollectAdded = false;
	if ( GarbageCollectAdded == false )
	{
		FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddStatic( FLevelStreamingGCHelper::PrepareStreamedOutLevelsForGC );
		FCoreUObjectDelegates::GetPostGarbageCollect().AddStatic( FLevelStreamingGCHelper::VerifyLevelsGotRemovedByGC );
		GarbageCollectAdded = true;
	}
}

void FLevelStreamingGCHelper::RequestUnload( ULevel* InLevel )
{
	if (!IsRunningCommandlet())
	{
		check( InLevel );
		check( InLevel->bIsVisible == false );
		LevelsPendingUnload.AddUnique( InLevel );
	}
}

void FLevelStreamingGCHelper::CancelUnloadRequest( ULevel* InLevel )
{
	LevelsPendingUnload.Remove( InLevel );
}

void FLevelStreamingGCHelper::PrepareStreamedOutLevelsForGC()
{
	if (LevelsPendingUnload.Num() > 0)
	{
		OnGCStreamedOutLevels.Broadcast();
	}
	
	// Iterate over all level objects that want to be unloaded.
	for( int32 LevelIndex=0; LevelIndex < LevelsPendingUnload.Num(); LevelIndex++ )
	{
		ULevel*	Level = LevelsPendingUnload[LevelIndex].Get();

		if( Level && (!GIsEditor || Level->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor) ))
		{
			UPackage* LevelPackage = Level->GetOutermost();
			UE_LOG(LogStreaming, Log, TEXT("PrepareStreamedOutLevelsForGC called on '%s'"), *LevelPackage->GetName() );
			
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (World)
				{
					// This can never ever be called during tick; same goes for GC in general.
					check( !World->bInTick );
					UNetDriver *NetDriver = World->GetNetDriver();
					if (NetDriver)
					{
						// The net driver must remove this level and its actors from the packagemap, or else
						// the client package map will keep hard refs to them and prevent GC
						NetDriver->NotifyStreamingLevelUnload(Level);
					}

					// Broadcast level unloaded event to blueprints through level streaming objects
					ULevelStreaming::BroadcastLevelLoadedStatus(World, LevelPackage->GetFName(), false);
				}
			}

			// Make sure that this package has been unloaded after GC pass.
			LevelPackageNames.Add( LevelPackage->GetFName() );

			// Mark level as pending kill so references to it get deleted.
			UWorld* LevelWorld = CastChecked<UWorld>(Level->GetOuter());
			LevelWorld->MarkObjectsPendingKill();
			LevelWorld->MarkPendingKill();
			if (LevelPackage->MetaData)
			{
				LevelPackage->MetaData->MarkPendingKill();
			}
		}
	}

	LevelsPendingUnload.Empty();
}

void FLevelStreamingGCHelper::VerifyLevelsGotRemovedByGC()
{
	if( !GIsEditor )
	{
#if DO_GUARD_SLOW
		int32 FailCount = 0;
		// Iterate over all objects and find out whether they reside in a GC'ed level package.
		for( FObjectIterator It; It; ++It )
		{
			UObject* Object = *It;
			// Check whether object's outermost is in the list.
			if( LevelPackageNames.Find( Object->GetOutermost()->GetFName() ) != INDEX_NONE
			// But disregard package object itself.
			&&	!Object->IsA(UPackage::StaticClass()) )
			{
				UE_LOG(LogWorld, Log, TEXT("%s didn't get garbage collected! Trying to find culprit, though this might crash. Try increasing stack size if it does."), *Object->GetFullName());
				StaticExec(NULL, *FString::Printf(TEXT("OBJ REFS CLASS=%s NAME=%s shortest"),*Object->GetClass()->GetName(), *Object->GetPathName()));
				TMap<UObject*,UProperty*>	Route		= FArchiveTraceRoute::FindShortestRootPath( Object, true, GARBAGE_COLLECTION_KEEPFLAGS );
				FString						ErrorString	= FArchiveTraceRoute::PrintRootPath( Route, Object );
				// Print out error message. We don't assert here as there might be multiple culprits.
				UE_LOG(LogWorld, Warning, TEXT("%s didn't get garbage collected!") LINE_TERMINATOR TEXT("%s"), *Object->GetFullName(), *ErrorString );
				FailCount++;
			}
		}
		if( FailCount > 0 )
		{
			UE_LOG(LogWorld, Fatal,TEXT("Streamed out levels were not completely garbage collected! Please see previous log entries."));
		}
#endif
	}

	LevelPackageNames.Empty();
}

int32 FLevelStreamingGCHelper::GetNumLevelsPendingPurge()
{
	return LevelsPendingUnload.Num();
}

void UWorld::RenameToPIEWorld(int32 PIEInstanceID)
{
#if WITH_EDITOR
	UPackage* WorldPackage = GetOutermost();

	WorldPackage->PIEInstanceID = PIEInstanceID;
	WorldPackage->SetPackageFlags(PKG_PlayInEditor);

	const FString PIEPackageName = *UWorld::ConvertToPIEPackageName(WorldPackage->GetName(), PIEInstanceID);
	WorldPackage->Rename(*PIEPackageName);
	FSoftObjectPath::AddPIEPackageName(FName(*PIEPackageName));

	StreamingLevelsPrefix = UWorld::BuildPIEPackagePrefix(PIEInstanceID);
	
	if (WorldComposition)
	{
		WorldComposition->ReinitializeForPIE();
	}
	
	for (ULevelStreaming* LevelStreaming : StreamingLevels)
	{
		LevelStreaming->RenameForPIE(PIEInstanceID);
	}

	PersistentLevel->FixupForPIE(PIEInstanceID);
#endif
}

FString UWorld::ConvertToPIEPackageName(const FString& PackageName, int32 PIEInstanceID)
{
	const FString PackageAssetName = FPackageName::GetLongPackageAssetName(PackageName);
	
	if (PackageAssetName.StartsWith(PLAYWORLD_PACKAGE_PREFIX))
	{
		return PackageName;
	}
	else
	{
		check(PIEInstanceID != -1);
		const FString PackageAssetPath = FPackageName::GetLongPackagePath(PackageName);
		const FString PackagePIEPrefix = BuildPIEPackagePrefix(PIEInstanceID);
		return FString::Printf(TEXT("%s/%s%s"), *PackageAssetPath, *PackagePIEPrefix, *PackageAssetName );
	}
}

FString UWorld::StripPIEPrefixFromPackageName(const FString& PrefixedName, const FString& Prefix)
{
	FString ResultName;
	FString ShortPrefixedName = FPackageName::GetLongPackageAssetName(PrefixedName);
	if (ShortPrefixedName.StartsWith(Prefix))
	{
		FString NamePath = FPackageName::GetLongPackagePath(PrefixedName);
		ResultName = NamePath + "/" + ShortPrefixedName.RightChop(Prefix.Len());
	}
	else
	{
		ResultName = PrefixedName;
	}

	return ResultName;
}

FString UWorld::BuildPIEPackagePrefix(int PIEInstanceID)
{
	check(PIEInstanceID != -1);
	return FString::Printf(TEXT("%s_%d_"), PLAYWORLD_PACKAGE_PREFIX, PIEInstanceID);
}

/**
 * Simple archive for updating lazy pointer GUIDs when a sub-level gets duplicated for PIE
 */
class FFixupSmartPointersForPIEArchive : public FArchiveUObject
{
	/** Keeps track of objects that have already been serialized */
	TSet<UObject*> VisitedObjects;

public:

	FFixupSmartPointersForPIEArchive()
	{
		ArIsObjectReferenceCollector = true;
		ArIsModifyingWeakAndStrongReferences = true;
		ArIsPersistent = false;
		ArIgnoreArchetypeRef = true;
	}
	virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override
	{
		FArchive& Ar = *this;
		FUniqueObjectGuid ID = LazyObjectPtr.GetUniqueID();
		
		// Remap unique ID if necessary
		ID = ID.FixupForPIE();

		LazyObjectPtr = ID;
		return Ar;
	}

	virtual FArchive& operator<<(UObject*& Object) override
	{
		if (Object && !VisitedObjects.Contains(Object))
		{
			VisitedObjects.Add(Object);
			Object->Serialize(*this);
		}
		return *this;
	}

	virtual FArchive& operator<<(FSoftObjectPtr& Value) override
	{
		// Explicitly do nothing, we don't want to accidentally do PIE fixups
		return *this;
	}

	virtual FArchive& operator<<(FSoftObjectPath& Value) override
	{
		// Explicitly do nothing, we don't want to accidentally do PIE fixups
		return *this;
	}
};

UWorld* UWorld::DuplicateWorldForPIE(const FString& PackageName, UWorld* OwningWorld)
{
	QUICK_SCOPE_CYCLE_COUNTER(UWorld_DuplicateWorldForPIE);
	FScopeCycleCounterUObject Context(OwningWorld);

	// Find the original (non-PIE) level package
	UPackage* EditorLevelPackage = FindObjectFast<UPackage>( NULL, FName(*PackageName));
	if( !EditorLevelPackage )
		return NULL;

	// Find world object and use its PersistentLevel pointer.
	UWorld* EditorLevelWorld = UWorld::FindWorldInPackage(EditorLevelPackage);

	// If the world was not found, try to follow a redirector, if there is one
	if ( !EditorLevelWorld )
	{
		EditorLevelWorld = UWorld::FollowWorldRedirectorInPackage(EditorLevelPackage);
		if ( EditorLevelWorld )
		{
			EditorLevelPackage = EditorLevelWorld->GetOutermost();
		}
	}

	if( !EditorLevelWorld )
		return NULL;
	
	int32 PIEInstanceID = -1;

	if (FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(OwningWorld))
	{
		PIEInstanceID = WorldContext->PIEInstance;
	}
	else if (OwningWorld)
	{
		PIEInstanceID = OwningWorld->GetOutermost()->PIEInstanceID;
	}
	else
	{
		checkf(false, TEXT("Unable to determine PIEInstanceID to duplicate for PIE."));
	}

	GPlayInEditorID = PIEInstanceID;

	FString PrefixedLevelName = ConvertToPIEPackageName(PackageName, PIEInstanceID);
	const FName PrefixedLevelFName = FName(*PrefixedLevelName);
	FSoftObjectPath::AddPIEPackageName(PrefixedLevelFName);

	UWorld::WorldTypePreLoadMap.FindOrAdd(PrefixedLevelFName) = EWorldType::PIE;
	UPackage* PIELevelPackage = CreatePackage(nullptr,*PrefixedLevelName);
	PIELevelPackage->SetPackageFlags(PKG_PlayInEditor);
	PIELevelPackage->PIEInstanceID = PIEInstanceID;
	PIELevelPackage->SetGuid( EditorLevelPackage->GetGuid() );
	PIELevelPackage->MarkAsFullyLoaded();

	ULevel::StreamedLevelsOwningWorld.Add(PIELevelPackage->GetFName(), OwningWorld);
	UWorld* PIELevelWorld = CastChecked<UWorld>(StaticDuplicateObject(EditorLevelWorld, PIELevelPackage, EditorLevelWorld->GetFName(), RF_AllFlags, nullptr, EDuplicateMode::PIE));

	{
		// The owning world may contain lazy pointers to actors in the sub-level we just duplicated so make sure they are fixed up with the PIE GUIDs
		FFixupSmartPointersForPIEArchive FixupLazyPointersAr;
		FixupLazyPointersAr << OwningWorld;
	}


	// Ensure the feature level matches the editor's, this is required as FeatureLevel is not a UPROPERTY and is not duplicated from EditorLevelWorld.
	PIELevelWorld->FeatureLevel = EditorLevelWorld->FeatureLevel;

	// Clean up the world type list and owning world list now that PostLoad has occurred
	UWorld::WorldTypePreLoadMap.Remove(PrefixedLevelFName);
	ULevel::StreamedLevelsOwningWorld.Remove(PIELevelPackage->GetFName());
	
	PIELevelWorld->StreamingLevelsPrefix = BuildPIEPackagePrefix(PIEInstanceID);
	{
		ULevel* EditorLevel = EditorLevelWorld->PersistentLevel;
		ULevel* PIELevel = PIELevelWorld->PersistentLevel;

		PIELevel->bHasRerunConstructionScripts = EditorLevel->bHasRerunConstructionScripts;

		// Fixup model components. The index buffers have been created for the components in the EditorWorld and the order
		// in which components were post-loaded matters. So don't try to guarantee a particular order here, just copy the
		// elements over.
		if (PIELevel->Model != NULL
			&& PIELevel->Model == EditorLevel->Model
			&& PIELevel->ModelComponents.Num() == EditorLevel->ModelComponents.Num() )
		{
			QUICK_SCOPE_CYCLE_COUNTER(UWorld_DuplicateWorldForPIE_UpdateModelComponents);

			PIELevel->Model->ClearLocalMaterialIndexBuffersData();
			for (int32 ComponentIndex = 0; ComponentIndex < PIELevel->ModelComponents.Num(); ++ComponentIndex)
			{
				UModelComponent* SrcComponent = EditorLevel->ModelComponents[ComponentIndex];
				UModelComponent* DestComponent = PIELevel->ModelComponents[ComponentIndex];
				DestComponent->CopyElementsFrom(SrcComponent);
			}
		}

		// We have to place PIELevel at his local position in case EditorLevel was visible
		// Correct placement will occur during UWorld::AddToWorld
		if (EditorLevel->OwningWorld->WorldComposition && EditorLevel->bIsVisible)
		{
			FIntVector LevelOffset = FIntVector::ZeroValue - EditorLevel->OwningWorld->WorldComposition->GetLevelOffset(EditorLevel);
			PIELevel->ApplyWorldOffset(FVector(LevelOffset), false);
		}
	}

	PIELevelWorld->ClearFlags(RF_Standalone);
	EditorLevelWorld->TransferBlueprintDebugReferences(PIELevelWorld);

	UE_LOG(LogWorld, Verbose, TEXT("PIE: Copying PIE streaming level from %s to %s. OwningWorld: %s"),
		*EditorLevelWorld->GetPathName(),
		*PIELevelWorld->GetPathName(),
		OwningWorld ? *OwningWorld->GetPathName() : TEXT("<null>"));

	return PIELevelWorld;
}

void UWorld::UpdateLevelStreamingInner(ULevelStreaming* StreamingLevel)
{
	check(StreamingLevel != nullptr);
		
	// Don't bother loading sub-levels in PIE for levels that aren't visible in editor
	if (IsPlayInEditor() && GEngine->OnlyLoadEditorVisibleLevelsInPIE())
	{
		if (!StreamingLevel->bShouldBeVisibleInEditor)
		{
			return;
		}
	}
	FScopeCycleCounterUObject ContextScope(StreamingLevel);

	// Work performed to make a level visible is spread across several frames and we can't unload/ hide a level that is currently pending
	// to be made visible, so we fulfill those requests first.
	bool bHasVisibilityRequestPending	= StreamingLevel->GetLoadedLevel() && StreamingLevel->GetLoadedLevel() == CurrentLevelPendingVisibility;
	bool bHasInvisibilityRequestPending	= StreamingLevel->GetLoadedLevel() && StreamingLevel->GetLoadedLevel() == CurrentLevelPendingInvisibility;
		
	// Figure out whether level should be loaded, visible and block on load if it should be loaded but currently isn't.
	bool bShouldBeLoaded = bHasVisibilityRequestPending || (!GUseBackgroundLevelStreaming && !bShouldForceUnloadStreamingLevels && !StreamingLevel->bIsRequestingUnloadAndRemoval);
	bool bShouldBeVisible	= bHasVisibilityRequestPending || bShouldForceVisibleStreamingLevels;
	bool bShouldBlockOnLoad	= StreamingLevel->bShouldBlockOnLoad || StreamingLevel->ShouldBeAlwaysLoaded();

	// Don't update if the code requested this level object to be unloaded and removed or it has an invisibility request pending
	if (!bShouldForceUnloadStreamingLevels && !StreamingLevel->bIsRequestingUnloadAndRemoval)
	{
		bShouldBeLoaded		= bShouldBeLoaded  || !IsGameWorld() || StreamingLevel->ShouldBeLoaded();
		bShouldBeVisible	= bShouldBeVisible || (bShouldBeLoaded && StreamingLevel->ShouldBeVisible());
	}

	// If we have an invisibility request pending, we can't make the level visible again until the request is completed
	if ( bHasInvisibilityRequestPending )
	{
		bShouldBeVisible = false;
	}

	// We want to give the garbage collector a chance to remove levels before we stream in more. We can't do this in the
	// case of a blocking load as it means those requests should be fulfilled right away. By waiting on GC before kicking
	// off new levels we potentially delay streaming in maps, but AllowLevelLoadRequests already looks and checks whether
	// async loading in general is active. E.g. normal package streaming would delay loading in this case. This is done
	// on purpose as well so the GC code has a chance to execute between consecutive loads of maps.
	//
	// NOTE: AllowLevelLoadRequests not an invariant as streaming might affect the result, do NOT pulled out of the loop.
	bool bAllowLevelLoadRequests =	bShouldBlockOnLoad || AllowLevelLoadRequests();

	// Figure out whether there are any levels we haven't collected garbage yet.
	bool bAreLevelsPendingPurge	=	FLevelStreamingGCHelper::GetNumLevelsPendingPurge() > 0;
	// Request a 'soft' GC if there are levels pending purge and there are levels to be loaded. In the case of a blocking
	// load this is going to guarantee GC firing first thing afterwards and otherwise it is going to sneak in right before
	// kicking off the async load.
	if (bAreLevelsPendingPurge)
	{
		GEngine->ForceGarbageCollection( false );
	}

	// See whether level is already loaded
	if (bShouldBeLoaded)
	{
		const bool bBlockOnLoad = (!IsGameWorld() || !GUseBackgroundLevelStreaming || bShouldBlockOnLoad);
		// Request to load or duplicate existing level
		StreamingLevel->RequestLevel(this, bAllowLevelLoadRequests, bBlockOnLoad ? ULevelStreaming::AlwaysBlock : ULevelStreaming::BlockAlwaysLoadedLevelsOnly );
	}
		
	// Cache pointer for convenience. This cannot happen before this point as e.g. flushing async loaders
	// or such will modify StreamingLevel->LoadedLevel.
	ULevel* Level = StreamingLevel->GetLoadedLevel();

	// See whether we have a loaded level.
	if (Level)
	{
		// Update loaded level visibility
		if (bShouldBeVisible)
		{
			// Add loaded level to a world if it's not there yet
			if (!Level->bIsVisible)
			{
				AddToWorld(Level, StreamingLevel->LevelTransform);
				// In case we have finished making level visible
				if (Level->bIsVisible)
				{
					// immediately discard previous level
					StreamingLevel->DiscardPendingUnloadLevel(this);

					if (Scene)
					{
						QUICK_SCOPE_CYCLE_COUNTER(STAT_UpdateLevelStreamingInner_OnLevelAddedToWorld);
						// Notify the new level has been added after the old has been discarded
						Scene->OnLevelAddedToWorld(Level->GetOutermost()->GetFName(), this, Level->bIsLightingScenario);
					}
				}
			}
		}
		else
		{
			// Discard previous LOD level
			StreamingLevel->DiscardPendingUnloadLevel(this);
			if (Level->bIsVisible)
			{
				// Hide loaded level, incrementally if necessary
				RemoveFromWorld(Level, IsGameWorld() );

				if (Scene)
				{
					Scene->OnLevelRemovedFromWorld(this, Level->bIsLightingScenario);
				}
			}
		}

		if (!bShouldBeLoaded)
		{
			if (!Level->bIsVisible && !IsVisibilityRequestPending())
			{
				StreamingLevel->DiscardPendingUnloadLevel(this);
				StreamingLevel->ClearLoadedLevel();
				StreamingLevel->DiscardPendingUnloadLevel(this);
			}
		}
	}
	else
	{
		StreamingLevel->DiscardPendingUnloadLevel(this);
	}
}

void UWorld::UpdateLevelStreaming()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateLevelStreamingTime);
	// do nothing if level streaming is frozen
	if (bIsLevelStreamingFrozen)
	{
		return;
	}

	// Store current number of pending unload levels, it may change in loop bellow
	const int32 NumLevelsPendingPurge = FLevelStreamingGCHelper::GetNumLevelsPendingPurge();
	
	for (int32 LevelIndex = 0; LevelIndex < StreamingLevels.Num(); LevelIndex++)
	{
		ULevelStreaming* StreamingLevel = StreamingLevels[LevelIndex];
		if (StreamingLevel)
		{
			UpdateLevelStreamingInner(StreamingLevel);

			// If requested, remove this level from iterated over array once it is unloaded.
			if (StreamingLevel->bIsRequestingUnloadAndRemoval)
			{
				if (StreamingLevel->HasLoadedLevel() == false && 
					StreamingLevel->bHasLoadRequestPending == false)
				{
					// The -- is required as we're forward iterating over the StreamingLevels array.
					StreamingLevels.RemoveAt(LevelIndex--);
				}
			}	
		}
	}
			
	// In case more levels has been requested to unload, force GC on next tick 
	if (NumLevelsPendingPurge < FLevelStreamingGCHelper::GetNumLevelsPendingPurge())
	{
		GEngine->ForceGarbageCollection(true); 
	}
}

void UWorld::FlushLevelStreaming(EFlushLevelStreamingType FlushType)
{
	if (!FPlatformProcess::SupportsMultithreading())
	{
		return;
	}

	AWorldSettings* WorldSettings = GetWorldSettings();

	TGuardValue<EFlushLevelStreamingType> FlushingLevelStreamingGuard(FlushLevelStreamingType, FlushType);

	// Update internals with current loaded/ visibility flags.
	UpdateLevelStreaming();

	// Make sure all outstanding loads are taken care of, other than ones associated with the excluded type
	FlushAsyncLoading();

	// Kick off making levels visible if loading finished by flushing.
	UpdateLevelStreaming();

	// Making levels visible is spread across several frames so we simply loop till it is done.
	bool bLevelsPendingVisibility = true;
	while( bLevelsPendingVisibility )
	{
		bLevelsPendingVisibility = IsVisibilityRequestPending();

		// Tick level streaming to make levels visible.
		if( bLevelsPendingVisibility )
		{
			// Only flush async loading if we're performing a full flush.
			if (FlushLevelStreamingType == EFlushLevelStreamingType::Full)
			{
				// Make sure all outstanding loads are taken care of...
				FlushAsyncLoading();
			}
	
			// Update level streaming.
			UpdateLevelStreaming();
		}
	}
	
	check(!IsVisibilityRequestPending());

	// we need this, or the traces will be abysmally slow
	EnsureCollisionTreeIsBuilt();

	// We already blocked on async loading.
	if (FlushLevelStreamingType == EFlushLevelStreamingType::Full)
	{
		bRequestedBlockOnAsyncLoading = false;
	}
}

/**
 * Forces streaming data to be rebuilt for the current world.
 */
static void ForceBuildStreamingData()
{
	for (TObjectIterator<UWorld> ObjIt;  ObjIt; ++ObjIt)
	{
		UWorld* WorldComp = *ObjIt;
		if (WorldComp && WorldComp->PersistentLevel && WorldComp->PersistentLevel->OwningWorld == WorldComp)
		{
			ULevel::BuildStreamingData(WorldComp);
		}		
	}
}

static FAutoConsoleCommand ForceBuildStreamingDataCmd(
	TEXT("ForceBuildStreamingData"),
	TEXT("Forces streaming data to be rebuilt for the current world."),
	FConsoleCommandDelegate::CreateStatic(ForceBuildStreamingData)
	);


void UWorld::TriggerStreamingDataRebuild()
{
	bStreamingDataDirty = true;
	BuildStreamingDataTimer = FPlatformTime::Seconds() + 5.0;
}


void UWorld::ConditionallyBuildStreamingData()
{
	if ( bStreamingDataDirty && FPlatformTime::Seconds() > BuildStreamingDataTimer )
	{
		bStreamingDataDirty = false;
		ULevel::BuildStreamingData( this );
	}
}

bool UWorld::IsVisibilityRequestPending() const
{
	if (!FPlatformProcess::SupportsMultithreading())
	{
		return false;
	}
	return (CurrentLevelPendingVisibility != nullptr || CurrentLevelPendingInvisibility != nullptr);
}

bool UWorld::AreAlwaysLoadedLevelsLoaded() const
{
	for (int32 LevelIndex = 0; LevelIndex < StreamingLevels.Num(); LevelIndex++)
	{
		ULevelStreaming* LevelStreaming = StreamingLevels[LevelIndex];

		// See whether there's a level with a pending request.
		if (LevelStreaming != NULL && LevelStreaming->ShouldBeAlwaysLoaded())
		{	
			const ULevel* LoadedLevel = LevelStreaming->GetLoadedLevel();

			if (LevelStreaming->bHasLoadRequestPending
				|| !LoadedLevel
				|| LoadedLevel->bIsVisible != LevelStreaming->bShouldBeVisible)
			{
				return false;
			}
		}
	}

	return true;
}

void UWorld::AsyncLoadAlwaysLoadedLevelsForSeamlessTravel()
{
	// Need to do this now so that data can be set correctly on the loaded world's collections.
	// This normally happens in InitWorld but that happens too late for seamless travel.
	ConditionallyCreateDefaultLevelCollections();

	for (int32 LevelIndex = 0; LevelIndex < StreamingLevels.Num(); LevelIndex++)
	{
		ULevelStreaming* LevelStreaming = StreamingLevels[LevelIndex];

		// See whether there's a level with a pending request.
		if (LevelStreaming != NULL && LevelStreaming->ShouldBeAlwaysLoaded())
		{	
			const ULevel* LoadedLevel = LevelStreaming->GetLoadedLevel();

			if (LevelStreaming->bHasLoadRequestPending || !LoadedLevel)
			{
				LevelStreaming->RequestLevel(this, true, ULevelStreaming::NeverBlock);				
			}
		}
	}
}

bool UWorld::AllowLevelLoadRequests()
{
	// Always allow level load request in the editor or when we do full streaming flush
	if (IsGameWorld() && FlushLevelStreamingType != EFlushLevelStreamingType::Full)
	{
		const bool bAreLevelsPendingPurge = FLevelStreamingGCHelper::GetNumLevelsPendingPurge() > 0;
		
		// Let code choose. Hold off queueing in case: 
		// We are only flushing levels visibility
		// There pending unload requests
		// There pending load requests and gameplay has already started.
		if (bAreLevelsPendingPurge || FlushLevelStreamingType == EFlushLevelStreamingType::Visibility || (IsAsyncLoading() && GetTimeSeconds() > 1.f))
		{
			return false;
		}
	}

	return true;
}

bool UWorld::HandleDemoScrubCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld)
{
	FString TimeString;
	if (!FParse::Token(Cmd, TimeString, 0))
	{
		Ar.Log(TEXT("You must specify a time"));
	}
	else if (DemoNetDriver != nullptr && DemoNetDriver->ReplayStreamer.IsValid() && DemoNetDriver->ServerConnection != nullptr && DemoNetDriver->ServerConnection->OwningActor != nullptr)
	{
		APlayerController* PlayerController = Cast<APlayerController>(DemoNetDriver->ServerConnection->OwningActor);
		if (PlayerController != nullptr)
		{
			GetWorldSettings()->Pauser = PlayerController->PlayerState;
			const uint32 Time = FCString::Atoi(*TimeString);
			DemoNetDriver->GotoTimeInSeconds(Time);
		}
	}
	return true;
}

bool UWorld::HandleDemoPauseCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld)
{
	FString TimeString;

	AWorldSettings* WorldSettings = GetWorldSettings();
	check(WorldSettings != nullptr);

	if (WorldSettings->Pauser == nullptr)
	{
		if (DemoNetDriver != nullptr && DemoNetDriver->ServerConnection != nullptr && DemoNetDriver->ServerConnection->OwningActor != nullptr)
		{
			APlayerController* PlayerController = Cast<APlayerController>(DemoNetDriver->ServerConnection->OwningActor);
			if (PlayerController != nullptr)
			{
				WorldSettings->Pauser = PlayerController->PlayerState;
			}
		}
	}
	else
	{
		WorldSettings->Pauser = nullptr;
	}
	return true;
}

bool UWorld::HandleDemoSpeedCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld)
{
	FString TimeString;

	AWorldSettings* WorldSettings = GetWorldSettings();
	check(WorldSettings != nullptr);

	FString SpeedString;
	if (!FParse::Token(Cmd, SpeedString, 0))
	{
		Ar.Log(TEXT("You must specify a speed in the form of a float"));
	}
	else
	{
		const float Speed = FCString::Atof(*SpeedString);
		WorldSettings->DemoPlayTimeDilation = Speed;
	}
	return true;
}

bool UWorld::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( FParse::Command( &Cmd, TEXT("TRACETAG") ) )
	{
		return HandleTraceTagCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT("TRACETAGALL")))
	{
		bDebugDrawAllTraceTags = !bDebugDrawAllTraceTags;
		return true;
	}
	else if( FParse::Command( &Cmd, TEXT("FLUSHPERSISTENTDEBUGLINES") ) )
	{		
		return HandleFlushPersistentDebugLinesCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("LOGACTORCOUNTS")))
	{		
		return HandleLogActorCountsCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("DEMOREC")))
	{		
		return HandleDemoRecordCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("DEMOPLAY") ) )
	{		
		return HandleDemoPlayCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("DEMOSTOP") ) )
	{		
		return HandleDemoStopCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("DEMOSCRUB")))
	{
		return HandleDemoScrubCommand(Cmd, Ar, InWorld);
	}
	else if (FParse::Command(&Cmd, TEXT("DEMOPAUSE")))
	{
		return HandleDemoPauseCommand(Cmd, Ar, InWorld);
	}
	else if (FParse::Command(&Cmd, TEXT("DEMOSPEED")))
	{
		return HandleDemoSpeedCommand(Cmd, Ar, InWorld);
	}
	else if( ExecPhysCommands( Cmd, &Ar, InWorld ) )
	{
		return HandleLogActorCountsCommand( Cmd, Ar, InWorld );
	}
	else 
	{
		return 0;
	}
}

bool UWorld::HandleTraceTagCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString TagStr;
	FParse::Token(Cmd, TagStr, 0);
	DebugDrawTraceTag = FName(*TagStr);
	return true;
}

bool UWorld::HandleFlushPersistentDebugLinesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	PersistentLineBatcher->Flush();
	return true;
}

bool UWorld::HandleLogActorCountsCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	Ar.Logf(TEXT("Num Actors: %i"), InWorld->GetActorCount());
	return true;
}

bool UWorld::HandleDemoRecordCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	if (InWorld != nullptr && InWorld->GetGameInstance() != nullptr)
	{
		FString DemoName;

		FParse::Token( Cmd, DemoName, 0 );

		// The friendly name will be the map name if no name is supplied
		const FString FriendlyName = DemoName.IsEmpty() ? InWorld->GetMapName() : DemoName;

		InWorld->GetGameInstance()->StartRecordingReplay( DemoName, FriendlyName );
	}

	return true;
}

bool UWorld::HandleDemoPlayCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	FString Temp;
	const TCHAR* ErrorString = nullptr;

	if ( !FParse::Token( Cmd, Temp, 0 ) )
	{
		ErrorString = TEXT( "You must specify a filename" );
	}
	else if ( InWorld == nullptr )
	{
		ErrorString = TEXT( "InWorld is null" );
	}
	else if ( InWorld->GetGameInstance() == nullptr )
	{
		ErrorString = TEXT( "InWorld->GetGameInstance() is null" );
	}
	
	if (ErrorString != nullptr)
	{
		Ar.Log(ErrorString);

		if (GetGameInstance() != nullptr)
		{
			GetGameInstance()->HandleDemoPlaybackFailure(EDemoPlayFailure::Generic, FString(ErrorString));
		}
	}
	else
	{
		InWorld->GetGameInstance()->PlayReplay(Temp);
	}

	return true;
}

bool UWorld::HandleDemoStopCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	if ( InWorld != nullptr && InWorld->GetGameInstance() != nullptr )
	{
		InWorld->GetGameInstance()->StopRecordingReplay();
	}

	return true;
}

void UWorld::DestroyDemoNetDriver()
{
	if ( DemoNetDriver != NULL )
	{
		const FName DemoNetDriverName = DemoNetDriver->NetDriverName;

		check( GEngine->FindNamedNetDriver( this, DemoNetDriverName ) == DemoNetDriver );

		DemoNetDriver->StopDemo();
		DemoNetDriver->SetWorld( NULL );

		GEngine->DestroyNamedNetDriver( this, DemoNetDriverName );

		check( GEngine->FindNamedNetDriver( this, DemoNetDriverName ) == NULL );

		DemoNetDriver = NULL;
	}
}

bool UWorld::SetGameMode(const FURL& InURL)
{
	if( IsServer() && !AuthorityGameMode )
	{
		AuthorityGameMode = GetGameInstance()->CreateGameModeForURL(InURL);
		if( AuthorityGameMode != NULL )
		{
			return true;
		}
		else
		{
			UE_LOG(LogWorld, Error, TEXT("Failed to spawn GameMode actor."));
			return false;
		}
	}

	return false;
}

void UWorld::InitializeActorsForPlay(const FURL& InURL, bool bResetTime)
{
	check(bIsWorldInitialized);
	double StartTime = FPlatformTime::Seconds();

	// Don't reset time for seamless world transitions.
	if (bResetTime)
	{
		TimeSeconds = 0.0f;
		UnpausedTimeSeconds = 0.0f;
		RealTimeSeconds = 0.0f;
		AudioTimeSeconds = 0.0f;
	}

	// Get URL Options
	FString Options(TEXT(""));
	FString	Error(TEXT(""));
	for( int32 i=0; i<InURL.Op.Num(); i++ )
	{
		Options += TEXT("?");
		Options += InURL.Op[i];
	}

	// Set level info.
	if( !InURL.GetOption(TEXT("load"),NULL) )
	{
		URL = InURL;
	}

	// Config bool that allows disabling all construction scripts during PIE level streaming.
	bool bRerunConstructionDuringEditorStreaming = true;
	GConfig->GetBool(TEXT("Kismet"), TEXT("bRerunConstructionDuringEditorStreaming"), /*out*/ bRerunConstructionDuringEditorStreaming, GEngineIni);

	// Update world and the components of all levels.	
	// We don't need to rerun construction scripts if we have cooked data or we are playing in editor unless the PIE world was loaded
	// from disk rather than duplicated
	const bool bRerunConstructionScript = !(FPlatformProperties::RequiresCookedData() || (IsGameWorld() && (PersistentLevel->bHasRerunConstructionScripts || PersistentLevel->bWasDuplicatedForPIE || !bRerunConstructionDuringEditorStreaming)));
	UpdateWorldComponents( bRerunConstructionScript, true );

	// Init level gameplay info.
	if( !AreActorsInitialized() )
	{
		// Check that paths are valid
		if ( !IsNavigationRebuilt() )
		{
			UE_LOG(LogWorld, Warning, TEXT("*** WARNING - PATHS MAY NOT BE VALID ***"));
		}

		if (GEngine != NULL)
		{
			// Lock the level.
			if (IsPreviewWorld())
			{
				UE_LOG(LogWorld, Verbose, TEXT("Bringing preview %s up for play (max tick rate %i) at %s"), *GetFullName(), FMath::RoundToInt(GEngine->GetMaxTickRate(0, false)), *FDateTime::Now().ToString());
			}
			else
			{
				UE_LOG(LogWorld, Log, TEXT("Bringing %s up for play (max tick rate %i) at %s"), *GetFullName(), FMath::RoundToInt(GEngine->GetMaxTickRate(0, false)), *FDateTime::Now().ToString());
			}
		}

		// Initialize network actors and start execution.
		for( int32 LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
		{
			ULevel*	const Level = Levels[LevelIndex];
			Level->InitializeNetworkActors();
		}

		// Enable actor script calls.
		bStartup = true;
		bActorsInitialized = true;

		// Spawn server actors
		ENetMode CurNetMode = GEngine != NULL ? GEngine->GetNetMode(this) : NM_Standalone;

		if (CurNetMode == NM_ListenServer || CurNetMode == NM_DedicatedServer)
		{
			GEngine->SpawnServerActors(this);
		}

		// Init the game mode.
		if (AuthorityGameMode && !AuthorityGameMode->IsActorInitialized())
		{
			AuthorityGameMode->InitGame( FPaths::GetBaseFilename(InURL.Map), Options, Error );
		}

		// Route various initialization functions and set volumes.
		for( int32 LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
		{
			ULevel*	const Level = Levels[LevelIndex];
			Level->RouteActorInitialize();
		}

		// Let server know client sub-levels visibility state
		for (int32 LevelIndex = 1; LevelIndex < Levels.Num(); LevelIndex++)
		{
			ULevel*	SubLevel = Levels[LevelIndex];

			for (FLocalPlayerIterator It(GEngine, this); It; ++It)
			{
				APlayerController* LocalPlayerController = It->GetPlayerController(this);
				if (LocalPlayerController != NULL)
				{
					LocalPlayerController->ServerUpdateLevelVisibility(LocalPlayerController->NetworkRemapPath(SubLevel->GetOutermost()->GetFName(), false), SubLevel->bIsVisible);
				}
			}
		}

		bStartup = 0;
	}

	// Rearrange actors: static not net relevant actors first, then static net relevant actors and then others.
	check( Levels.Num() );
	check( PersistentLevel );
	check( Levels[0] == PersistentLevel );
	for( int32 LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
	{
		ULevel*	Level = Levels[LevelIndex];
		Level->SortActorList();
	}

	// update the auto-complete list for the console
	UConsole* ViewportConsole = (GEngine->GameViewport != nullptr) ? GEngine->GameViewport->ViewportConsole : nullptr;
	if (ViewportConsole != nullptr)
	{
		ViewportConsole->BuildRuntimeAutoCompleteList();
	}

	// let all subsystems/managers know:
	// @note if UWorld starts to host more of these it might a be a good idea to create a generic IManagerInterface of some sort
	if (NavigationSystem != nullptr)
	{
		NavigationSystem->OnInitializeActors();
	}

	if (AISystem != nullptr)
	{
		AISystem->InitializeActorsForPlay(bResetTime);
	}

	for (int32 LevelIndex = 0; LevelIndex < Levels.Num(); LevelIndex++)
	{
		ULevel*	Level = Levels[LevelIndex];
		IStreamingManager::Get().AddLevel(Level);
	}

	CheckTextureStreamingBuildValidity(this);

	if(IsPreviewWorld())
	{
		UE_LOG(LogWorld, Verbose, TEXT("Bringing up preview level for play took: %f"), FPlatformTime::Seconds() - StartTime );
	}
	else
	{
		UE_LOG(LogWorld, Log, TEXT("Bringing up level for play took: %f"), FPlatformTime::Seconds() - StartTime );
	}
}

void UWorld::BeginPlay()
{
	AGameModeBase* const GameMode = GetAuthGameMode();
	if (GameMode)
	{
		GameMode->StartPlay();
		if (GetAISystem())
		{
			GetAISystem()->StartPlay();
		}
	}
}

bool UWorld::IsNavigationRebuilt() const
{
	return GetNavigationSystem() == NULL || GetNavigationSystem()->IsNavigationBuilt(GetWorldSettings());
}

void UWorld::CleanupWorld(bool bSessionEnded, bool bCleanupResources, UWorld* NewWorld)
{
	check(IsVisibilityRequestPending() == false);

	// Wait on current physics scenes if they are processing
	if(FPhysScene* CurrPhysicsScene = GetPhysicsScene())
	{
		CurrPhysicsScene->WaitPhysScenes();
	}

	FWorldDelegates::OnWorldCleanup.Broadcast(this, bSessionEnded, bCleanupResources);

	if (AISystem != nullptr)
	{
		AISystem->CleanupWorld(bSessionEnded, bCleanupResources, NewWorld);
	}

	if (bCleanupResources == true)
	{
		// cleanup & remove navigation system
		SetNavigationSystem(nullptr);
	}

	ForEachNetDriver(GEngine, this, [](UNetDriver* const Driver)
	{
		if (Driver != nullptr)
		{
			Driver->GetNetworkObjectList().Reset();
		}
	});

#if WITH_EDITOR
	// If we're server traveling, we need to break the reference dependency here (caused by levelscript)
	// to avoid a GC crash for not cleaning up the gameinfo referenced by levelscript
	if (IsGameWorld() && !GIsEditor && !IsRunningCommandlet() && bSessionEnded && bCleanupResources && PersistentLevel)
	{
		PersistentLevel->CleanupLevelScriptBlueprint();
	}
#endif //WITH_EDITOR

#if ENABLE_VISUAL_LOG
	FVisualLogger::Get().Cleanup(this);
#endif // ENABLE_VISUAL_LOG	

	// Tell actors to remove their components from the scene.
	ClearWorldComponents();

	if (bCleanupResources && PersistentLevel)
	{
		PersistentLevel->ReleaseRenderingResources();

		// Flush any render commands and released accessed UTextures and materials to give them a chance to be collected.
		if ( FSlateApplication::IsInitialized() )
		{
			FSlateApplication::Get().FlushRenderState();
		}
	}

#if WITH_EDITOR
	// Clear standalone flag when switching maps in the Editor. This causes resources placed in the map
	// package to be garbage collected together with the world.
	if( GIsEditor && !IsTemplate() && bCleanupResources && this != NewWorld )
	{
		// Iterate over all objects to find ones that reside in the same package as the world.
		ForEachObjectWithOuter(GetOutermost(), [this](UObject* CurrentObject)
		{
			if ( CurrentObject != this )
			{
				CurrentObject->ClearFlags( RF_Standalone );
			}
		});

		if (WorldType != EWorldType::PIE)
		{
			for (int32 LevelIndex = 0; LevelIndex < GetNumLevels(); ++LevelIndex)
			{
				ULevel* Level = GetLevel(LevelIndex);

				if (Level->MapBuildData)
				{
					Level->MapBuildData->ClearFlags(RF_Standalone);
				}
			}
		}
	}
#endif //WITH_EDITOR

	for (int32 LevelIndex=0; LevelIndex < GetNumLevels(); ++LevelIndex)
	{
		UWorld* World = CastChecked<UWorld>(GetLevel(LevelIndex)->GetOuter());
		if (World != this)
		{
			World->CleanupWorld(bSessionEnded, bCleanupResources, NewWorld);
		}
	}

	// Clean up any duplicated levels.
	const FLevelCollection* const DuplicateCollection = FindCollectionByType(ELevelCollectionType::DynamicDuplicatedLevels);
	if (DuplicateCollection)
	{
		for (const ULevel* Level : DuplicateCollection->GetLevels())
		{
			if (!Level)
			{
				continue;
			}

			UWorld* const LevelWorld = CastChecked<UWorld>(Level->GetOuter());
			if (LevelWorld != this)
			{
				LevelWorld->CleanupWorld(bSessionEnded, bCleanupResources, NewWorld);
			}
		}
	}

	FWorldDelegates::OnPostWorldCleanup.Broadcast(this, bSessionEnded, bCleanupResources);
}

UGameViewportClient* UWorld::GetGameViewport() const
{
	FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(this);
	return (WorldContext ? WorldContext->GameViewport : NULL);
}

FConstControllerIterator UWorld::GetControllerIterator() const
{
	auto Result = ControllerList.CreateConstIterator();
	return (const FConstControllerIterator&)Result;
}

FConstPlayerControllerIterator UWorld::GetPlayerControllerIterator() const
{
	auto Result = PlayerControllerList.CreateConstIterator();
	return (const FConstPlayerControllerIterator&)Result;
}

APlayerController* UWorld::GetFirstPlayerController() const
{
	APlayerController* PlayerController = NULL;
	if( GetPlayerControllerIterator() )
	{
		PlayerController = GetPlayerControllerIterator()->Get();
	}
	return PlayerController;
}

ULocalPlayer* UWorld::GetFirstLocalPlayerFromController() const
{
	for( FConstPlayerControllerIterator Iterator = GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* PlayerController = Iterator->Get();
		if( PlayerController )
		{
			ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PlayerController->Player);
			if( LocalPlayer )
			{
				return LocalPlayer;
			}
		}
	}
	return NULL;
}

void UWorld::AddController( AController* Controller )
{	
	check( Controller );
	ControllerList.AddUnique( Controller );	
	if( Cast<APlayerController>(Controller) )
	{
		PlayerControllerList.AddUnique( Cast<APlayerController>(Controller) );
	}
}


void UWorld::RemoveController( AController* Controller )
{
	check( Controller );
	if( ControllerList.Remove( Controller ) > 0 )
	{
		if( Cast<APlayerController>(Controller) )
		{
			PlayerControllerList.Remove( Cast<APlayerController>(Controller) );
		}
	}
}

FConstPawnIterator UWorld::GetPawnIterator() const
{
	auto Result = PawnList.CreateConstIterator();
	return (const FConstPawnIterator&)Result;
}

int32 UWorld::GetNumPawns() const
{
	return PawnList.Num();
}

void UWorld::AddPawn( APawn* Pawn )
{
	check( Pawn );
	PawnList.AddUnique( Pawn );
}


void UWorld::RemovePawn( APawn* Pawn )
{
	check( Pawn );
	
	AController* Controller = Pawn->GetController();
	if (Controller && (Controller->GetPawn() == Pawn))
	{
		Controller->UnPossess();
	}

	PawnList.Remove( Pawn );	
}


void UWorld::RegisterAutoActivateCamera(ACameraActor* CameraActor, int32 PlayerIndex)
{
	check(CameraActor);
	check(PlayerIndex >= 0);
	AutoCameraActorList.AddUnique(CameraActor);
}

FConstCameraActorIterator UWorld::GetAutoActivateCameraIterator() const
{
	auto Result = AutoCameraActorList.CreateConstIterator();
	return (const FConstCameraActorIterator&)Result;
}


void UWorld::AddNetworkActor( AActor* Actor )
{
	if ( Actor == nullptr )
	{
		return;
	}

	if ( Actor->IsPendingKill() ) 
	{
		return;
	}

	if ( !ContainsLevel(Actor->GetLevel()) )
	{
		return;
	}

	ForEachNetDriver(GEngine, this, [Actor](UNetDriver* const Driver)
	{
		if (Driver != nullptr)
		{
			// Special case the demo net driver, since actors currently only have one associated NetDriverName.
			Driver->GetNetworkObjectList().Add(Actor, Driver->NetDriverName);
		}
	});
}

void UWorld::RemoveNetworkActor( AActor* Actor )
{
	if (Actor)
	{
		ForEachNetDriver(GEngine, this, [Actor](UNetDriver* const Driver)
		{
			if (Driver != nullptr)
			{
				Driver->GetNetworkObjectList().Remove(Actor);
			}
		});
	}
}

FDelegateHandle UWorld::AddOnActorSpawnedHandler( const FOnActorSpawned::FDelegate& InHandler )
{
	return OnActorSpawned.Add(InHandler);
}

void UWorld::RemoveOnActorSpawnedHandler( FDelegateHandle InHandle )
{
	OnActorSpawned.Remove(InHandle);
}

ABrush* UWorld::GetDefaultBrush() const
{
	check(PersistentLevel);
	return PersistentLevel->GetDefaultBrush();
}

bool UWorld::HasBegunPlay() const
{
	return bBegunPlay && PersistentLevel && PersistentLevel->Actors.Num();
}

bool UWorld::AreActorsInitialized() const
{
	return bActorsInitialized && PersistentLevel && PersistentLevel->Actors.Num();
}

float UWorld::GetMonoFarFieldCullingDistance() const
{
	float Result = 0.0f;
	const AWorldSettings* const WorldSettings = GetWorldSettings(false, false);
	if (WorldSettings != nullptr)
	{
		Result = WorldSettings->MonoCullingDistance;
	}
	return Result;
}

void UWorld::CreatePhysicsScene()
{
	SetPhysicsScene(new FPhysScene());
}

void UWorld::SetPhysicsScene(FPhysScene* InScene)
{ 
	// Clear world pointer in old FPhysScene (if there is one)
	if(PhysicsScene != NULL)
	{
		PhysicsScene->SetOwningWorld(nullptr);
		delete PhysicsScene;
	}

	// Assign pointer
	PhysicsScene = InScene;

	// Set pointer in scene to know which world its coming from
	if(PhysicsScene != NULL)
	{
		PhysicsScene->SetOwningWorld(this);
	}
}

APhysicsVolume* UWorld::GetDefaultPhysicsVolume() const
{
	// Create on demand.
	if (DefaultPhysicsVolume == nullptr)
	{
		// Try WorldSettings first
		AWorldSettings* WorldSettings = GetWorldSettings(/*bCheckStreamingPesistent=*/ false, /*bChecked=*/ false);
		UClass* DefaultPhysicsVolumeClass = (WorldSettings ? WorldSettings->DefaultPhysicsVolumeClass : nullptr);

		// Fallback on DefaultPhysicsVolume static
		if (DefaultPhysicsVolumeClass == nullptr)
		{
			DefaultPhysicsVolumeClass = ADefaultPhysicsVolume::StaticClass();
		}

		// Spawn volume
		UWorld* MutableThis = const_cast<UWorld*>(this);
		MutableThis->DefaultPhysicsVolume = MutableThis->SpawnActor<APhysicsVolume>(DefaultPhysicsVolumeClass);
		MutableThis->DefaultPhysicsVolume->Priority = -1000000;
	}
	return DefaultPhysicsVolume;
}

void UWorld::AddPhysicsVolume(APhysicsVolume* Volume)
{
	if (!Cast<ADefaultPhysicsVolume>(Volume))
	{
		NonDefaultPhysicsVolumeList.Add(Volume);
	}
}

void UWorld::RemovePhysicsVolume(APhysicsVolume* Volume)
{
	NonDefaultPhysicsVolumeList.RemoveSwap(Volume);
	// Also remove null entries that may accumulate as items become invalidated
	NonDefaultPhysicsVolumeList.RemoveSwap(nullptr);
}

FConstPhysicsVolumeIterator UWorld::GetNonDefaultPhysicsVolumeIterator() const
{
	auto Result = NonDefaultPhysicsVolumeList.CreateConstIterator();
	return (const FConstPhysicsVolumeIterator&)Result;
}

int32 UWorld::GetNonDefaultPhysicsVolumeCount() const
{
	return NonDefaultPhysicsVolumeList.Num();
}

ALevelScriptActor* UWorld::GetLevelScriptActor( ULevel* OwnerLevel ) const
{
	if( OwnerLevel == NULL )
	{
		OwnerLevel = CurrentLevel;
	}
	check(OwnerLevel);
	return OwnerLevel->GetLevelScriptActor();
}


AWorldSettings* UWorld::GetWorldSettings( const bool bCheckStreamingPersistent, const bool bChecked ) const
{
	checkSlow(!IsInActualRenderingThread());
	AWorldSettings* WorldSettings = nullptr;
	if (PersistentLevel)
	{
		WorldSettings = PersistentLevel->GetWorldSettings(bChecked);

		if( bCheckStreamingPersistent )
		{
			if( StreamingLevels.Num() > 0 &&
				StreamingLevels[0] &&
				StreamingLevels[0]->IsA<ULevelStreamingPersistent>()) 
			{
				ULevel* Level = StreamingLevels[0]->GetLoadedLevel();
				if (Level != nullptr)
				{
					WorldSettings = Level->GetWorldSettings();
				}
			}
		}
	}
	return WorldSettings;
}


UModel* UWorld::GetModel() const
{
	check(CurrentLevel);
	return CurrentLevel->Model;
}


float UWorld::GetGravityZ() const
{
	AWorldSettings* WorldSettings = GetWorldSettings();
	return (WorldSettings != NULL) ? WorldSettings->GetGravityZ() : 0.f;
}


float UWorld::GetDefaultGravityZ() const
{
	UPhysicsSettings * PhysSetting = UPhysicsSettings::Get();
	return (PhysSetting != NULL) ? PhysSetting->DefaultGravityZ : 0.f;
}

/** This is our global function for retrieving the current MapName **/
ENGINE_API const FString GetMapNameStatic()
{
	FString Retval;

	FWorldContext const* ContextToUse = NULL;
	if ( GEngine )
	{
		// We're going to look through the WorldContexts and pull any Game context we find
		// If there isn't a Game context, we'll take the first PIE we find
		// and if none of those we'll use an Editor
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			if (WorldContext.WorldType == EWorldType::Game)
			{
				ContextToUse = &WorldContext;
				break;
			}
			else if (WorldContext.WorldType == EWorldType::PIE && (ContextToUse == NULL || ContextToUse->WorldType != EWorldType::PIE))
			{
				ContextToUse = &WorldContext;
			}
			else if (WorldContext.WorldType == EWorldType::Editor && ContextToUse == NULL)
			{	
				ContextToUse = &WorldContext;
			}
		}
	}

	if( ContextToUse != NULL )
	{
		Retval = ContextToUse->World()->GetMapName();
	}
	else if( UObjectInitialized() )
	{
		Retval = appGetStartupMap( FCommandLine::Get() );
	}

	return Retval;
}

const FString UWorld::GetMapName() const
{
	// Default to the world's package as the map name.
	FString MapName = GetOutermost()->GetName();
	
	// In the case of a seamless world check to see whether there are any persistent levels in the levels
	// array and use its name if there is one.
	for( int32 LevelIndex=0; LevelIndex<StreamingLevels.Num(); LevelIndex++ )
	{
		ULevelStreamingPersistent* PersistentStreamingLevel = Cast<ULevelStreamingPersistent>( StreamingLevels[LevelIndex] );
		// Use the name of the first found persistent level.
		if( PersistentStreamingLevel )
		{
			MapName = PersistentStreamingLevel->GetWorldAssetPackageName();
			break;
		}
	}

	// Just return the name of the map, not the rest of the path
	MapName = FPackageName::GetLongPackageAssetName(MapName);

	return MapName;
}

EAcceptConnection::Type UWorld::NotifyAcceptingConnection()
{
	check(NetDriver);
	if( NetDriver->ServerConnection )
	{
		// We are a client and we don't welcome incoming connections.
		UE_LOG(LogNet, Log, TEXT("NotifyAcceptingConnection: Client refused") );
		return EAcceptConnection::Reject;
	}
	else if( NextURL!=TEXT("") )
	{
		// Server is switching levels.
		UE_LOG(LogNet, Log, TEXT("NotifyAcceptingConnection: Server %s refused"), *GetName() );
		return EAcceptConnection::Ignore;
	}
	else
	{
		// Server is up and running.
		UE_LOG(LogNet, Verbose, TEXT("NotifyAcceptingConnection: Server %s accept"), *GetName() );
		return EAcceptConnection::Accept;
	}
}

void UWorld::NotifyAcceptedConnection( UNetConnection* Connection )
{
	check(NetDriver!=NULL);
	check(NetDriver->ServerConnection==NULL);
	UE_LOG(LogNet, Log, TEXT("NotifyAcceptedConnection: Name: %s, TimeStamp: %s, %s"), *GetName(), FPlatformTime::StrTimestamp(), *Connection->Describe() );
	NETWORK_PROFILER( GNetworkProfiler.TrackEvent( TEXT( "OPEN" ), *( GetName() + TEXT( " " ) + Connection->LowLevelGetRemoteAddress() ), Connection ) );
}

bool UWorld::NotifyAcceptingChannel( UChannel* Channel )
{
	check(Channel);
	check(Channel->Connection);
	check(Channel->Connection->Driver);
	UNetDriver* Driver = Channel->Connection->Driver;

	if( Driver->ServerConnection )
	{
		// We are a client and the server has just opened up a new channel.
		//UE_LOG(LogWorld, Log,  "NotifyAcceptingChannel %i/%i client %s", Channel->ChIndex, Channel->ChType, *GetName() );
		if( Channel->ChType==CHTYPE_Actor )
		{
			// Actor channel.
			//UE_LOG(LogWorld, Log,  "Client accepting actor channel" );
			return 1;
		}
		else if (Channel->ChType == CHTYPE_Voice)
		{
			// Accept server requests to open a voice channel, allowing for custom voip implementations
			// which utilize multiple server controlled voice channels.
			//UE_LOG(LogNet, Log,  "Client accepting voice channel" );
			return 1;
		}
		else
		{
			// Unwanted channel type.
			UE_LOG(LogNet, Log, TEXT("Client refusing unwanted channel of type %i"), (uint8)Channel->ChType );
			return 0;
		}
	}
	else
	{
		// We are the server.
		if( Channel->ChIndex==0 && Channel->ChType==CHTYPE_Control )
		{
			// The client has opened initial channel.
			UE_LOG(LogNet, Log, TEXT("NotifyAcceptingChannel Control %i server %s: Accepted"), Channel->ChIndex, *GetFullName() );
			return 1;
		}
		else if( Channel->ChType==CHTYPE_File )
		{
			// The client is going to request a file.
			UE_LOG(LogNet, Log, TEXT("NotifyAcceptingChannel File %i server %s: Accepted"), Channel->ChIndex, *GetFullName() );
			return 1;
		}
		else
		{
			// Client can't open any other kinds of channels.
			UE_LOG(LogNet, Log, TEXT("NotifyAcceptingChannel %i %i server %s: Refused"), (uint8)Channel->ChType, Channel->ChIndex, *GetFullName() );
			return 0;
		}
	}
}

void UWorld::WelcomePlayer(UNetConnection* Connection)
{
	check(CurrentLevel);
	Connection->SendPackageMap();
	
	FString LevelName = CurrentLevel->GetOutermost()->GetName();
	Connection->ClientWorldPackageName = CurrentLevel->GetOutermost()->GetFName();

	FString GameName;
	FString RedirectURL;
	if (AuthorityGameMode != NULL)
	{
		GameName = AuthorityGameMode->GetClass()->GetPathName();
		AuthorityGameMode->GameWelcomePlayer(Connection, RedirectURL);
	}

	FNetControlMessage<NMT_Welcome>::Send(Connection, LevelName, GameName, RedirectURL);
	Connection->FlushNet();
	// don't count initial join data for netspeed throttling
	// as it's unnecessary, since connection won't be fully open until it all gets received, and this prevents later gameplay data from being delayed to "catch up"
	Connection->QueuedBits = 0;
	Connection->SetClientLoginState( EClientLoginState::Welcomed );		// Client is fully logged in
}

bool UWorld::DestroySwappedPC(UNetConnection* Connection)
{
	for( FConstPlayerControllerIterator Iterator = GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* PlayerController = Iterator->Get();
		if (PlayerController->Player == NULL && PlayerController->PendingSwapConnection == Connection)
		{
			DestroyActor(PlayerController);
			return true;
		}
	}

	return false;
}

void UWorld::NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, class FInBunch& Bunch)
{
	if( NetDriver->ServerConnection )
	{
		check(Connection == NetDriver->ServerConnection);

		// We are the client, traveling to a new map with the same server
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		UE_LOG(LogNet, Verbose, TEXT("Level client received: %s"), FNetControlMessageInfo::GetName(MessageType));
#endif
		switch (MessageType)
		{
			case NMT_Failure:
			{
				// our connection attempt failed for some reason, for example a synchronization mismatch (bad GUID, etc) or because the server rejected our join attempt (too many players, etc)
				// here we can further parse the string to determine the reason that the server closed our connection and present it to the user
				FString EntryURL = TEXT("?failed");

				FString ErrorMsg;
				FNetControlMessage<NMT_Failure>::Receive(Bunch, ErrorMsg);
				if (ErrorMsg.IsEmpty())
				{
					ErrorMsg = NSLOCTEXT("NetworkErrors", "GenericConnectionFailed", "Connection Failed.").ToString();
				}

				GEngine->BroadcastNetworkFailure(this, NetDriver, ENetworkFailure::FailureReceived, ErrorMsg);
				if (Connection)
				{
					Connection->Close();
				}
				break;
			}
			case NMT_DebugText:
			{
				// debug text message
				FString Text;
				FNetControlMessage<NMT_DebugText>::Receive(Bunch,Text);

				UE_LOG(LogNet, Log, TEXT("%s received NMT_DebugText Text=[%s] Desc=%s DescRemote=%s"),
					*Connection->Driver->GetDescription(),*Text,*Connection->LowLevelDescribe(),*Connection->LowLevelGetRemoteAddress());

				break;
			}
			case NMT_NetGUIDAssign:
			{
				FNetworkGUID NetGUID;
				FString Path;
				FNetControlMessage<NMT_NetGUIDAssign>::Receive(Bunch, NetGUID, Path);

				UE_LOG(LogNet, Verbose, TEXT("NMT_NetGUIDAssign  NetGUID %s. Path: %s. "), *NetGUID.ToString(), *Path );
				Connection->PackageMap->ResolvePathAndAssignNetGUID( NetGUID, Path );
				break;
			}
		}
	}
	else
	{
		// We are the server.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		UE_LOG(LogNet, Verbose, TEXT("Level server received: %s"), FNetControlMessageInfo::GetName(MessageType));
#endif
		if ( !Connection->IsClientMsgTypeValid( MessageType ) )
		{
			// If we get here, either code is mismatched on the client side, or someone could be spoofing the client address
			UE_LOG( LogNet, Error, TEXT( "IsClientMsgTypeValid FAILED (%i): Remote Address = %s" ), (int)MessageType, *Connection->LowLevelGetRemoteAddress() );
			Bunch.SetError();
			return;
		}
		
		switch (MessageType)
		{
			case NMT_Hello:
			{
				uint8 IsLittleEndian = 0;
				uint32 RemoteNetworkVersion = 0;
				uint32 LocalNetworkVersion = FNetworkVersion::GetLocalNetworkVersion();
				FString EncryptionToken;

				FNetControlMessage<NMT_Hello>::Receive(Bunch, IsLittleEndian, RemoteNetworkVersion, EncryptionToken);

				if (!FNetworkVersion::IsNetworkCompatible(LocalNetworkVersion, RemoteNetworkVersion))
				{
					UE_LOG(LogNet, Log, TEXT("NotifyControlMessage: Client connecting with invalid version. LocalNetworkVersion: %i, RemoteNetworkVersion: %i"), LocalNetworkVersion, RemoteNetworkVersion);
					FNetControlMessage<NMT_Upgrade>::Send(Connection, LocalNetworkVersion);
					Connection->FlushNet(true);
					Connection->Close();

					PerfCountersIncrement(TEXT("ClosedConnectionsDueToIncompatibleVersion"));
				}
				else
				{
					if (EncryptionToken.IsEmpty())
					{
						SendChallengeControlMessage(Connection);
					}
					else
					{
						if (FNetDelegates::OnReceivedNetworkEncryptionToken.IsBound())
						{
							TWeakObjectPtr<UNetConnection> WeakConnection = Connection;
							FNetDelegates::OnReceivedNetworkEncryptionToken.Execute(EncryptionToken, FOnEncryptionKeyResponse::CreateUObject(this, &UWorld::SendChallengeControlMessage, WeakConnection));
						}
						else
						{
							FString FailureMsg(TEXT("Encryption failure"));
							UE_LOG(LogNet, Warning, TEXT("%s: No delegate available to handle encryption token, disconnecting."), *Connection->GetName());
							FNetControlMessage<NMT_Failure>::Send(Connection, FailureMsg);
							Connection->FlushNet(true);
						}
					}
				}
				break;
			}

			case NMT_Netspeed:
			{
				int32 Rate;
				FNetControlMessage<NMT_Netspeed>::Receive(Bunch, Rate);
				Connection->CurrentNetSpeed = FMath::Clamp(Rate, 1800, NetDriver->MaxClientRate);
				UE_LOG(LogNet, Log, TEXT("Client netspeed is %i"), Connection->CurrentNetSpeed);

				break;
			}
			case NMT_Abort:
			{
				break;
			}
			case NMT_Skip:
			{
				break;
			}
			case NMT_Login:
			{
				FUniqueNetIdRepl UniqueIdRepl;
				FString OnlinePlatformName;

				// Admit or deny the player here.
				TArray<uint8> RequestUrlBytes;
				FNetControlMessage<NMT_Login>::Receive(Bunch, Connection->ClientResponse, RequestUrlBytes, UniqueIdRepl, OnlinePlatformName);
				Connection->RequestURL = UTF8_TO_TCHAR(RequestUrlBytes.GetData());
				UE_LOG(LogNet, Log, TEXT("Login request: %s userId: %s"), *Connection->RequestURL, UniqueIdRepl.IsValid() ? *UniqueIdRepl->ToString() : TEXT("Invalid"));


				// Compromise for passing splitscreen playercount through to gameplay login code,
				// without adding a lot of extra unnecessary complexity throughout the login code.
				// NOTE: This code differs from NMT_JoinSplit, by counting + 1 for SplitscreenCount
				//			(since this is the primary connection, not counted in Children)
				FURL InURL( NULL, *Connection->RequestURL, TRAVEL_Absolute );

				if ( !InURL.Valid )
				{
					UE_LOG( LogNet, Error, TEXT( "NMT_Login: Invalid URL %s" ), *Connection->RequestURL );
					Bunch.SetError();
					break;
				}

				uint8 SplitscreenCount = FMath::Min(Connection->Children.Num() + 1, 255);

				// Don't allow clients to specify this value
				InURL.RemoveOption(TEXT("SplitscreenCount"));
				InURL.AddOption(*FString::Printf(TEXT("SplitscreenCount=%i"), SplitscreenCount));

				Connection->RequestURL = InURL.ToString();

				// skip to the first option in the URL
				const TCHAR* Tmp = *Connection->RequestURL;
				for (; *Tmp && *Tmp != '?'; Tmp++);

				// keep track of net id for player associated with remote connection
				Connection->PlayerId = UniqueIdRepl;

				// keep track of the online platform the player associated with this connection is using.
				Connection->SetPlayerOnlinePlatformName(FName(*OnlinePlatformName));

				// ask the game code if this player can join
				FString ErrorMsg;
				AGameModeBase* GameMode = GetAuthGameMode();

				if (GameMode)
				{
					GameMode->PreLogin(Tmp, Connection->LowLevelGetRemoteAddress(), Connection->PlayerId, ErrorMsg);
				}
				if (!ErrorMsg.IsEmpty())
				{
					UE_LOG(LogNet, Log, TEXT("PreLogin failure: %s"), *ErrorMsg);
					NETWORK_PROFILER(GNetworkProfiler.TrackEvent(TEXT("PRELOGIN FAILURE"), *ErrorMsg, Connection));
					FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
					Connection->FlushNet(true);
					//@todo sz - can't close the connection here since it will leave the failure message 
					// in the send buffer and just close the socket. 
					//Connection->Close();
				}
				else
				{
					WelcomePlayer(Connection);
				}
				break;
			}
			case NMT_Join:
			{
				if (Connection->PlayerController == NULL)
				{
					// Spawn the player-actor for this network player.
					FString ErrorMsg;
					UE_LOG(LogNet, Log, TEXT("Join request: %s"), *Connection->RequestURL);

					FURL InURL( NULL, *Connection->RequestURL, TRAVEL_Absolute );

					if ( !InURL.Valid )
					{
						UE_LOG( LogNet, Error, TEXT( "NMT_Login: Invalid URL %s" ), *Connection->RequestURL );
						Bunch.SetError();
						break;
					}

					Connection->PlayerController = SpawnPlayActor( Connection, ROLE_AutonomousProxy, InURL, Connection->PlayerId, ErrorMsg );
					if (Connection->PlayerController == NULL)
					{
						// Failed to connect.
						UE_LOG(LogNet, Log, TEXT("Join failure: %s"), *ErrorMsg);
						NETWORK_PROFILER(GNetworkProfiler.TrackEvent(TEXT("JOIN FAILURE"), *ErrorMsg, Connection));
						FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
						Connection->FlushNet(true);
						//@todo sz - can't close the connection here since it will leave the failure message 
						// in the send buffer and just close the socket. 
						//Connection->Close();
					}
					else
					{
						// Successfully in game.
						UE_LOG(LogNet, Log, TEXT("Join succeeded: %s"), *Connection->PlayerController->PlayerState->PlayerName);
						NETWORK_PROFILER(GNetworkProfiler.TrackEvent(TEXT("JOIN"), *Connection->PlayerController->PlayerState->PlayerName, Connection));
						// if we're in the middle of a transition or the client is in the wrong world, tell it to travel
						FString LevelName;
						FSeamlessTravelHandler &SeamlessTravelHandler = GEngine->SeamlessTravelHandlerForWorld( this );

						if (SeamlessTravelHandler.IsInTransition())
						{
							// tell the client to go to the destination map
							LevelName = SeamlessTravelHandler.GetDestinationMapName();
						}
						else if (!Connection->PlayerController->HasClientLoadedCurrentWorld())
						{
							// tell the client to go to our current map
							FString NewLevelName = GetOutermost()->GetName();
							UE_LOG(LogNet, Log, TEXT("Client joined but was sent to another level. Asking client to travel to: '%s'"), *NewLevelName);
							LevelName = NewLevelName;
						}
						if (LevelName != TEXT(""))
						{
							Connection->PlayerController->ClientTravel(LevelName, TRAVEL_Relative, true);
						}

						// @TODO FIXME - TEMP HACK? - clear queue on join
						Connection->QueuedBits = 0;
					}
				}
				break;
			}
			case NMT_JoinSplit:
			{
				// Handle server-side request for spawning a new controller using a child connection.
				FString SplitRequestURL;
				FUniqueNetIdRepl UniqueIdRepl;
				FNetControlMessage<NMT_JoinSplit>::Receive(Bunch, SplitRequestURL, UniqueIdRepl);

				// Compromise for passing splitscreen playercount through to gameplay login code,
				// without adding a lot of extra unnecessary complexity throughout the login code.
				// NOTE: This code differs from NMT_Login, by counting + 2 for SplitscreenCount
				//			(once for pending child connection, once for primary non-child connection)
				FURL InURL( NULL, *SplitRequestURL, TRAVEL_Absolute );

				if ( !InURL.Valid )
				{
					UE_LOG( LogNet, Error, TEXT( "NMT_JoinSplit: Invalid URL %s" ), *SplitRequestURL );
					Bunch.SetError();
					break;
				}

				uint8 SplitscreenCount = FMath::Min(Connection->Children.Num() + 2, 255);

				// Don't allow clients to specify this value
				InURL.RemoveOption(TEXT("SplitscreenCount"));
				InURL.AddOption(*FString::Printf(TEXT("SplitscreenCount=%i"), SplitscreenCount));

				SplitRequestURL = InURL.ToString();

				// skip to the first option in the URL
				const TCHAR* Tmp = *SplitRequestURL;
				for (; *Tmp && *Tmp != '?'; Tmp++);

				// keep track of net id for player associated with remote connection
				Connection->PlayerId = UniqueIdRepl;

				// go through the same full login process for the split player even though it's all in the same frame
				FString ErrorMsg;
				AGameModeBase* GameMode = GetAuthGameMode();
				if (GameMode)
				{
					GameMode->PreLogin(Tmp, Connection->LowLevelGetRemoteAddress(), Connection->PlayerId, ErrorMsg);
				}
				if (!ErrorMsg.IsEmpty())
				{
					// if any splitscreen viewport fails to join, all viewports on that client also fail
					UE_LOG(LogNet, Log, TEXT("PreLogin failure: %s"), *ErrorMsg);
					NETWORK_PROFILER(GNetworkProfiler.TrackEvent(TEXT("PRELOGIN FAILURE"), *ErrorMsg, Connection));
					FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
					Connection->FlushNet(true);
					//@todo sz - can't close the connection here since it will leave the failure message 
					// in the send buffer and just close the socket. 
					//Connection->Close();
				}
				else
				{
					// create a child network connection using the existing connection for its parent
					check(Connection->GetUChildConnection() == NULL);
					check(CurrentLevel);

					UChildConnection* ChildConn = NetDriver->CreateChild(Connection);
					ChildConn->PlayerId = Connection->PlayerId;
					ChildConn->SetPlayerOnlinePlatformName(Connection->GetPlayerOnlinePlatformName());
					ChildConn->RequestURL = SplitRequestURL;
					ChildConn->ClientWorldPackageName = CurrentLevel->GetOutermost()->GetFName();

					// create URL from string
					FURL JoinSplitURL(NULL, *SplitRequestURL, TRAVEL_Absolute);

					UE_LOG(LogNet, Log, TEXT("JOINSPLIT: Join request: URL=%s"), *JoinSplitURL.ToString());
					APlayerController* PC = SpawnPlayActor(ChildConn, ROLE_AutonomousProxy, JoinSplitURL, ChildConn->PlayerId, ErrorMsg, uint8(Connection->Children.Num()));
					if (PC == NULL)
					{
						// Failed to connect.
						UE_LOG(LogNet, Log, TEXT("JOINSPLIT: Join failure: %s"), *ErrorMsg);
						NETWORK_PROFILER(GNetworkProfiler.TrackEvent(TEXT("JOINSPLIT FAILURE"), *ErrorMsg, Connection));
						// remove the child connection
						Connection->Children.Remove(ChildConn);
						// if any splitscreen viewport fails to join, all viewports on that client also fail
						FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
						Connection->FlushNet(true);
						//@todo sz - can't close the connection here since it will leave the failure message 
						// in the send buffer and just close the socket. 
						//Connection->Close();
					}
					else
					{
						// Successfully spawned in game.
						UE_LOG(LogNet, Log, TEXT("JOINSPLIT: Succeeded: %s PlayerId: %s"), 
							*ChildConn->PlayerController->PlayerState->PlayerName,
							*ChildConn->PlayerController->PlayerState->UniqueId.ToDebugString());
					}
				}
				break;
			}
			case NMT_PCSwap:
			{
				UNetConnection* SwapConnection = Connection;
				int32 ChildIndex;
				FNetControlMessage<NMT_PCSwap>::Receive(Bunch, ChildIndex);
				if (ChildIndex >= 0)
				{
					SwapConnection = Connection->Children.IsValidIndex(ChildIndex) ? Connection->Children[ChildIndex] : NULL;
				}
				bool bSuccess = false;
				if (SwapConnection != NULL)
				{
					bSuccess = DestroySwappedPC(SwapConnection);
				}
				
				if (!bSuccess)
				{
					UE_LOG(LogNet, Log, TEXT("Received invalid swap message with child index %i"), ChildIndex);
				}
				break;
			}
			case NMT_DebugText:
			{
				// debug text message
				FString Text;
				FNetControlMessage<NMT_DebugText>::Receive(Bunch,Text);

				UE_LOG(LogNet, Log, TEXT("%s received NMT_DebugText Text=[%s] Desc=%s DescRemote=%s"),
					*Connection->Driver->GetDescription(),*Text,*Connection->LowLevelDescribe(),*Connection->LowLevelGetRemoteAddress());
				break;
			}
		}
	}
}

void UWorld::SendChallengeControlMessage(UNetConnection* Connection)
{
	if (Connection)
	{
		if (Connection->State != USOCK_Invalid && Connection->State != USOCK_Closed && Connection->Driver)
		{
			Connection->Challenge = FString::Printf(TEXT("%08X"), FPlatformTime::Cycles());
			Connection->SetExpectedClientLoginMsgType(NMT_Login);
			FNetControlMessage<NMT_Challenge>::Send(Connection, Connection->Challenge);
			Connection->FlushNet();
		}
		else
		{
			UE_LOG(LogNet, Log, TEXT("UWorld::SendChallengeControlMessage: connection in invalid state. %s"), *Connection->Describe());
		}
	}
	else
	{
		UE_LOG(LogNet, Log, TEXT("UWorld::SendChallengeControlMessage: Connection is null."));
	}
}

void UWorld::SendChallengeControlMessage(const FEncryptionKeyResponse& Response, TWeakObjectPtr<UNetConnection> WeakConnection)
{
	UNetConnection* Connection = WeakConnection.Get();
	if (Connection)
	{
		if (Connection->State != USOCK_Invalid && Connection->State != USOCK_Closed && Connection->Driver)
		{
			if (Response.Response == EEncryptionResponse::Success)
			{
				Connection->EnableEncryptionWithKeyServer(Response.EncryptionKey);
				SendChallengeControlMessage(Connection);
			}
			else
			{
				FString ResponseStr(Lex::ToString(Response.Response));
				UE_LOG(LogNet, Warning, TEXT("UWorld::SendChallengeControlMessage: encryption failure [%s] %s"), *ResponseStr, *Response.ErrorMsg);
				FNetControlMessage<NMT_Failure>::Send(Connection, ResponseStr);
				Connection->FlushNet();
				// Can't close the connection here since it will leave the failure message in the send buffer and just close the socket. 
				// Connection->Close();
			}
		}
		else
		{
			UE_LOG(LogNet, Warning, TEXT("UWorld::SendChallengeControlMessage: connection in invalid state. %s"), *Connection->Describe());
		}
	}
	else
	{
		UE_LOG(LogNet, Warning, TEXT("UWorld::SendChallengeControlMessage: Connection is null."));
	}
}

bool UWorld::Listen( FURL& InURL )
{
#if WITH_SERVER_CODE
	if( NetDriver )
	{
		GEngine->BroadcastNetworkFailure(this, NetDriver, ENetworkFailure::NetDriverAlreadyExists);
		return false;
	}

	// Create net driver.
	if (GEngine->CreateNamedNetDriver(this, NAME_GameNetDriver, NAME_GameNetDriver))
	{
		NetDriver = GEngine->FindNamedNetDriver(this, NAME_GameNetDriver);
		NetDriver->SetWorld(this);
		FLevelCollection* const SourceCollection = FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
		if (SourceCollection)
		{
			SourceCollection->SetNetDriver(NetDriver);
		}
		FLevelCollection* const StaticCollection = FindCollectionByType(ELevelCollectionType::StaticLevels);
		if (StaticCollection)
		{
			StaticCollection->SetNetDriver(NetDriver);
		}
	}

	if (NetDriver == NULL)
	{
		GEngine->BroadcastNetworkFailure(this, NULL, ENetworkFailure::NetDriverCreateFailure);
		return false;
	}

	FString Error;
	if( !NetDriver->InitListen( this, InURL, false, Error ) )
	{
		GEngine->BroadcastNetworkFailure(this, NetDriver, ENetworkFailure::NetDriverListenFailure, Error);
		UE_LOG(LogWorld, Log,  TEXT("Failed to listen: %s"), *Error );
		NetDriver->SetWorld(NULL);
		NetDriver = NULL;
		FLevelCollection* SourceCollection = FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
		if (SourceCollection)
		{
			SourceCollection->SetNetDriver(nullptr);
		}
		FLevelCollection* StaticCollection = FindCollectionByType(ELevelCollectionType::StaticLevels);
		if (StaticCollection)
		{
			StaticCollection->SetNetDriver(nullptr);
		}
		return false;
	}
	static bool LanPlay = FParse::Param(FCommandLine::Get(),TEXT("lanplay"));
	if ( !LanPlay && (NetDriver->MaxInternetClientRate < NetDriver->MaxClientRate) && (NetDriver->MaxInternetClientRate > 2500) )
	{
		NetDriver->MaxClientRate = NetDriver->MaxInternetClientRate;
	}

	NextSwitchCountdown = NetDriver->ServerTravelPause;
	return true;
#else
	return false;
#endif // WITH_SERVER_CODE
}

bool UWorld::IsClient() const
{
	return GIsClient;
}

bool UWorld::IsServer() const
{
	if ( NetDriver != NULL )
	{
		return NetDriver->IsServer();
	}

	if ( DemoNetDriver != NULL )
	{
		return DemoNetDriver->IsServer();
	}

	return true;
}

void UWorld::PrepareMapChange(const TArray<FName>& LevelNames)
{
		// Kick off async loading request for those maps.
	if( !GEngine->PrepareMapChange(this, LevelNames) )
		{
		UE_LOG(LogWorld, Warning,TEXT("Preparing map change via %s was not successful: %s"), *GetFullName(), *GEngine->GetMapChangeFailureDescription(this) );
	}
}

bool UWorld::IsPreparingMapChange()
{
	return GEngine->IsPreparingMapChange(this);
}

bool UWorld::IsMapChangeReady()
{
	return GEngine->IsReadyForMapChange(this);
}

void UWorld::CancelPendingMapChange()
{
	GEngine->CancelPendingMapChange(this);
}

void UWorld::CommitMapChange()
{
	if( IsPreparingMapChange() )
	{
		GEngine->SetShouldCommitPendingMapChange(this, true);
	}
	else
	{
		UE_LOG(LogWorld, Log, TEXT("AWorldSettings::CommitMapChange being called without a pending map change!"));
	}
}

void UWorld::RequestNewWorldOrigin(FIntVector InNewOriginLocation)
{
	RequestedOriginLocation = InNewOriginLocation;
}

bool UWorld::SetNewWorldOrigin(FIntVector InNewOriginLocation)
{
	if (OriginLocation == InNewOriginLocation) 
	{
		return true;
	}
	
	// We cannot shift world origin while Level is in the process to be added to a world
	// it will cause wrong positioning for this level 
	if (IsVisibilityRequestPending())
	{
		return false;
	}
	
	UE_LOG(LogLevel, Log, TEXT("WORLD TRANSLATION BEGIN {%d, %d, %d} -> {%d, %d, %d}"), 
		OriginLocation.X, OriginLocation.Y, OriginLocation.Z, InNewOriginLocation.X, InNewOriginLocation.Y, InNewOriginLocation.Z);

	const double MoveStartTime = FPlatformTime::Seconds();

	// Broadcast that we going to shift world to the new origin
	FCoreDelegates::PreWorldOriginOffset.Broadcast(this, OriginLocation, InNewOriginLocation);

	FVector Offset(OriginLocation - InNewOriginLocation);
	OriginOffsetThisFrame = Offset;

	// Send offset command to rendering thread
	Scene->ApplyWorldOffset(Offset);

	// Shift physics scene
	if (PhysicsScene && FPhysScene::SupportsOriginShifting())
	{
		PhysicsScene->ApplyWorldOffset(Offset);
	}
		
	// Apply offset to all visible levels
	for(int32 LevelIndex = 0; LevelIndex < Levels.Num(); LevelIndex++)
	{
		ULevel* LevelToShift = Levels[LevelIndex];
		
		// Only visible sub-levels need to be shifted
		// Hidden sub-levels will be shifted once they become visible in UWorld::AddToWorld
		if (LevelToShift->bIsVisible || LevelToShift->IsPersistentLevel())
		{
			LevelToShift->ApplyWorldOffset(Offset, true);
		}
	}

	// Shift navigation meshes
	if (NavigationSystem)
	{
		NavigationSystem->ApplyWorldOffset(Offset, true);
	}

	// Apply offset to components with no actor (like UGameplayStatics::SpawnEmitterAtLocation) 
	{
		TArray <UObject*> WorldChildren; 
		GetObjectsWithOuter(this, WorldChildren, false);

		for (UObject* ChildObject : WorldChildren)
		{
		   UActorComponent* Component = Cast<UActorComponent>(ChildObject);
		   if (Component && Component->GetOwner() == nullptr)
		   {
				Component->ApplyWorldOffset(Offset, true);
		   }
		}
	}
			
	if (LineBatcher)
	{
		LineBatcher->ApplyWorldOffset(Offset, true);
	}
	
	if (PersistentLineBatcher)
	{
		PersistentLineBatcher->ApplyWorldOffset(Offset, true);
	}
	
	if (ForegroundLineBatcher)
	{
		ForegroundLineBatcher->ApplyWorldOffset(Offset, true);
	}

	FIntVector PreviosWorldOriginLocation = OriginLocation;
	// Set new world origin
	OriginLocation = InNewOriginLocation;
	RequestedOriginLocation = InNewOriginLocation;
	
	// Propagate event to a level blueprints
	for(int32 LevelIndex = 0; LevelIndex < Levels.Num(); LevelIndex++)
	{
		ULevel* Level = Levels[LevelIndex];
		if (Level->bIsVisible && 
			Level->LevelScriptActor)
		{
			Level->LevelScriptActor->WorldOriginLocationChanged(PreviosWorldOriginLocation, OriginLocation);
		}
	}

	if (AISystem != NULL)
	{
		AISystem->WorldOriginLocationChanged(PreviosWorldOriginLocation, OriginLocation);
	}

	// Broadcast that have finished world shifting
	FCoreDelegates::PostWorldOriginOffset.Broadcast(this, PreviosWorldOriginLocation, OriginLocation);

	const double CurrentTime = FPlatformTime::Seconds();
	const float TimeTaken = CurrentTime - MoveStartTime;
	UE_LOG(LogLevel, Log, TEXT("WORLD TRANSLATION END {%d, %d, %d} took %.4f ms"),
		OriginLocation.X, OriginLocation.Y, OriginLocation.Z, TimeTaken*1000);
	
	return true;
}

void UWorld::NavigateTo(FIntVector InLocation)
{
	check(WorldComposition != NULL);

	SetNewWorldOrigin(InLocation);
	WorldComposition->UpdateStreamingState(FVector::ZeroVector);
	FlushLevelStreaming();
}

void UWorld::GetMatineeActors(TArray<class AMatineeActor*>& OutMatineeActors)
{
	check( IsGameWorld() && GetCurrentLevel());

	ULevel* CurLevel = GetCurrentLevel();
	for( int32 ActorIndex = 0; ActorIndex < CurLevel->Actors.Num(); ++ActorIndex )
	{
		AActor* Actor = CurLevel->Actors[ ActorIndex ];
		AMatineeActor* MatineeActor = Cast<AMatineeActor>( Actor );
		if( MatineeActor )
		{
			OutMatineeActors.Add( MatineeActor );
		}
	}
}

/*-----------------------------------------------------------------------------
	Seamless world traveling
-----------------------------------------------------------------------------*/

void FSeamlessTravelHandler::SetHandlerLoadedData(UObject* InLevelPackage, UWorld* InLoadedWorld)
{
	LoadedPackage = InLevelPackage;
	LoadedWorld = InLoadedWorld;
	if (LoadedWorld != NULL)
	{
		LoadedWorld->AddToRoot();
	}

}

/** callback sent to async loading code to inform us when the level package is complete */
void FSeamlessTravelHandler::SeamlessTravelLoadCallback(const FName& PackageName, UPackage* LevelPackage, EAsyncLoadingResult::Type Result)
{
	// make sure we remove the name, even if travel was cancelled.
	const FName URLMapFName = FName(*PendingTravelURL.Map);
	UWorld::WorldTypePreLoadMap.Remove(URLMapFName);

	// defer until tick when it's safe to perform the transition
	if (IsInTransition())
	{
		UWorld* World = UWorld::FindWorldInPackage(LevelPackage);

		// If the world could not be found, follow a redirector if there is one.
		if (!World)
		{
			World = UWorld::FollowWorldRedirectorInPackage(LevelPackage);
			if (World)
			{
				LevelPackage = World->GetOutermost();
			}
		}

		SetHandlerLoadedData(LevelPackage, World);

		// Now that the p map is loaded, start async loading any always loaded levels
		if (World)
		{
			if (World->WorldType == EWorldType::PIE)
			{
				if (LevelPackage->PIEInstanceID != -1)
				{
					World->StreamingLevelsPrefix = UWorld::BuildPIEPackagePrefix(LevelPackage->PIEInstanceID);
				}
				else
				{
					// If this is a PIE world but the PIEInstanceID is -1, that implies this world is a temporary save
					// for multi-process PIE which should have been saved with the correct StreamingLevelsPrefix.
					ensure(!World->StreamingLevelsPrefix.IsEmpty());
				}
			}

			if (World->PersistentLevel)
			{
				World->PersistentLevel->HandleLegacyMapBuildData();
			}

			World->AsyncLoadAlwaysLoadedLevelsForSeamlessTravel();
		}
	}

	STAT_ADD_CUSTOMMESSAGE_NAME( STAT_NamedMarker, *(FString( TEXT( "StartTravelComplete - " ) + PackageName.ToString() )) );
}

bool FSeamlessTravelHandler::StartTravel(UWorld* InCurrentWorld, const FURL& InURL, const FGuid& InGuid)
{
	FWorldContext &Context = GEngine->GetWorldContextFromWorldChecked(InCurrentWorld);
	WorldContextHandle = Context.ContextHandle;

	SeamlessTravelStartTime = FPlatformTime::Seconds();

	if (!InURL.Valid)
	{
		UE_LOG(LogWorld, Error, TEXT("Invalid travel URL specified"));
		return false;
	}
	else
	{
		FLoadTimeTracker::Get().ResetRawLoadTimes();
		UE_LOG(LogWorld, Log, TEXT("SeamlessTravel to: %s"), *InURL.Map);
		FString MapName = UWorld::RemovePIEPrefix(InURL.Map);
		if (!FPackageName::DoesPackageExist(MapName, InGuid.IsValid() ? &InGuid : NULL))
		{
			UE_LOG(LogWorld, Error, TEXT("Unable to travel to '%s' - file not found"), *MapName);
			return false;
			// @todo: might have to handle this more gracefully to handle downloading (might also need to send GUID and check it here!)
		}
		else
		{
			CurrentWorld = InCurrentWorld;

			bool bCancelledExisting = false;
			if (IsInTransition())
			{
				if (PendingTravelURL.Map == InURL.Map)
				{
					// we are going to the same place so just replace the options
					PendingTravelURL = InURL;
					return true;
				}
				UE_LOG(LogWorld, Warning, TEXT("Cancelling travel to '%s' to go to '%s' instead"), *PendingTravelURL.Map, *InURL.Map);
				CancelTravel();
				bCancelledExisting = true;
			}

			if (CurrentWorld->DemoNetDriver && CurrentWorld->DemoNetDriver->IsRecording())
			{
				CurrentWorld->DemoNetDriver->OnSeamlessTravelStartDuringRecording(InURL.Map);
			}

			checkSlow(LoadedPackage == NULL);
			checkSlow(LoadedWorld == NULL);

			PendingTravelURL = InURL;
			PendingTravelGuid = InGuid;
			bSwitchedToDefaultMap = false;
			bTransitionInProgress = true;
			bPauseAtMidpoint = false;
			bNeedCancelCleanUp = false;

			FName CurrentMapName = CurrentWorld->GetOutermost()->GetFName();
			FName DestinationMapName = FName(*PendingTravelURL.Map);

			FString TransitionMap = GetDefault<UGameMapsSettings>()->TransitionMap.GetLongPackageName();
			FName DefaultMapFinalName(*TransitionMap);

			// if we're already in the default map, skip loading it and just go to the destination
			if (DefaultMapFinalName == CurrentMapName ||
				DefaultMapFinalName == DestinationMapName)
			{
				UE_LOG(LogWorld, Log, TEXT("Already in default map or the default map is the destination, continuing to destination"));
				bSwitchedToDefaultMap = true;
				if (bCancelledExisting)
				{
					// we need to fully finishing loading the old package and GC it before attempting to load the new one
					bPauseAtMidpoint = true;
					bNeedCancelCleanUp = true;
				}
				else
				{
					StartLoadingDestination();
				}
			}
			else if (TransitionMap.IsEmpty())
			{
				// If a default transition map doesn't exist, create a dummy World to use as the transition
				SetHandlerLoadedData(NULL, UWorld::CreateWorld(EWorldType::None, false));
			}
			else
			{
				if (CurrentMapName == DestinationMapName)
				{
					UNetDriver* const NetDriver = CurrentWorld->GetNetDriver();
					if (NetDriver)
					{
						for (int32 ClientIdx = 0; ClientIdx < NetDriver->ClientConnections.Num(); ClientIdx++)
						{
							UNetConnection* Connection = NetDriver->ClientConnections[ClientIdx];
							if (Connection)
							{
								// Empty the current map name in case we are going A -> transition -> A and the server loads fast enough
								// that the clients are not on the transition map yet causing the server to think its loaded
								Connection->ClientWorldPackageName = NAME_None;
							}
						}
					}
				}

				// Set the world type in the static map, so that UWorld::PostLoad can set the world type
				UWorld::WorldTypePreLoadMap.FindOrAdd(*TransitionMap) = CurrentWorld->WorldType;

				// first, load the entry level package
				STAT_ADD_CUSTOMMESSAGE_NAME( STAT_NamedMarker, *(FString( TEXT( "StartTravel - " ) + TransitionMap )) );
				LoadPackageAsync(TransitionMap, 
					FLoadPackageAsyncDelegate::CreateRaw(this, &FSeamlessTravelHandler::SeamlessTravelLoadCallback),
					0, 
					(CurrentWorld->WorldType == EWorldType::PIE ? PKG_PlayInEditor : PKG_None),
					Context.PIEInstance
					);
			}

			return true;
		}
	}
}

/** cancels transition in progress */
void FSeamlessTravelHandler::CancelTravel()
{
	LoadedPackage = NULL;
	if (LoadedWorld != NULL)
	{
		LoadedWorld->RemoveFromRoot();
		LoadedWorld->ClearFlags(RF_Standalone);
		LoadedWorld = NULL;
	}

	if (bTransitionInProgress)
	{
		UPackage* Package = CurrentWorld ? CurrentWorld->GetOutermost() : nullptr;
		if (Package)
		{
			FName CurrentPackageName = Package->GetFName();
			UNetDriver* const NetDriver = CurrentWorld->GetNetDriver();
			if (NetDriver)
			{
				for (int32 ClientIdx = 0; ClientIdx < NetDriver->ClientConnections.Num(); ClientIdx++)
				{
					UNetConnection* Connection = NetDriver->ClientConnections[ClientIdx];
					if (Connection)
					{
						UChildConnection* ChildConnection = Connection->GetUChildConnection();
						if (ChildConnection)
						{
							Connection = ChildConnection->Parent;
						}

						// Mark all clients as being where they are since this was set to None in StartTravel
						Connection->ClientWorldPackageName = CurrentPackageName;
					}
				}
			}
		}
	
		CurrentWorld = NULL;
		bTransitionInProgress = false;
		UE_LOG(LogWorld, Log, TEXT("----SeamlessTravel is cancelled!------"));
	}
}

void FSeamlessTravelHandler::SetPauseAtMidpoint(bool bNowPaused)
{
	if (!bTransitionInProgress)
	{
		UE_LOG(LogWorld, Warning, TEXT("Attempt to pause seamless travel when no transition is in progress"));
	}
	else if (bSwitchedToDefaultMap && bNowPaused)
	{
		UE_LOG(LogWorld, Warning, TEXT("Attempt to pause seamless travel after started loading final destination"));
	}
	else
	{
		bPauseAtMidpoint = bNowPaused;
		if (!bNowPaused && bSwitchedToDefaultMap)
		{
			// load the final destination now that we're done waiting
			StartLoadingDestination();
		}
	}
}

void FSeamlessTravelHandler::StartLoadingDestination()
{
	if (bTransitionInProgress && bSwitchedToDefaultMap)
	{
		UE_LOG(LogWorld, Log, TEXT("StartLoadingDestination to: %s"), *PendingTravelURL.Map);

		CurrentWorld->GetGameInstance()->PreloadContentForURL(PendingTravelURL);

		// Set the world type in the static map, so that UWorld::PostLoad can set the world type
		const FName URLMapFName = FName(*PendingTravelURL.Map);
		UWorld::WorldTypePreLoadMap.FindOrAdd(URLMapFName) = CurrentWorld->WorldType;
		// In PIE we might want to mangle MapPackageName when traveling to a map loaded in the editor
		FString URLMapPackageName = PendingTravelURL.Map;
		FString URLMapPackageToLoadFrom = PendingTravelURL.Map;
		EPackageFlags PackageFlags = PKG_None;
		int32 PIEInstanceID = INDEX_NONE;
		
#if WITH_EDITOR
		if (GIsEditor)
		{
			FWorldContext &WorldContext = GEngine->GetWorldContextFromHandleChecked(WorldContextHandle);
			if (WorldContext.WorldType == EWorldType::PIE)
			{
				PackageFlags |= PKG_PlayInEditor;
			}
			PIEInstanceID = WorldContext.PIEInstance;
			UPackage* EditorLevelPackage = (UPackage*)StaticFindObjectFast(UPackage::StaticClass(), NULL, URLMapFName, 0, 0, RF_NoFlags, EInternalObjectFlags::PendingKill);
			if (EditorLevelPackage)
			{
				URLMapPackageName = UWorld::ConvertToPIEPackageName(URLMapPackageName, PIEInstanceID);
			}
		}
#endif
		LoadPackageAsync(
			URLMapPackageName, 
			PendingTravelGuid.IsValid() ? &PendingTravelGuid : NULL,
			*URLMapPackageToLoadFrom,
			FLoadPackageAsyncDelegate::CreateRaw(this, &FSeamlessTravelHandler::SeamlessTravelLoadCallback), 			
			PackageFlags,
			PIEInstanceID
			);
	}
	else
	{
		UE_LOG(LogWorld, Error, TEXT("Called StartLoadingDestination() when not ready! bTransitionInProgress: %u bSwitchedToDefaultMap: %u"), bTransitionInProgress, bSwitchedToDefaultMap);
		checkSlow(0);
	}
}

void FSeamlessTravelHandler::CopyWorldData()
{
	FLevelCollection* const CurrentCollection = CurrentWorld->FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
	FLevelCollection* const CurrentStaticCollection = CurrentWorld->FindCollectionByType(ELevelCollectionType::StaticLevels);
	FLevelCollection* const LoadedCollection = LoadedWorld->FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
	FLevelCollection* const LoadedStaticCollection = LoadedWorld->FindCollectionByType(ELevelCollectionType::StaticLevels);

	// If we are doing seamless travel for replay playback, then make sure to transfer the replay driver over to the new world
	if ( CurrentWorld->DemoNetDriver && ( CurrentWorld->DemoNetDriver->IsPlaying() || CurrentWorld->DemoNetDriver->bRecordMapChanges) )
	{
		UDemoNetDriver* OldDriver = CurrentWorld->DemoNetDriver;
		CurrentWorld->DemoNetDriver = nullptr;
		OldDriver->SetWorld( LoadedWorld );
		LoadedWorld->DemoNetDriver = OldDriver;

		if (CurrentCollection && LoadedCollection)
		{
			LoadedCollection->SetDemoNetDriver(OldDriver);
			CurrentCollection->SetDemoNetDriver(nullptr);
		}
	}
	else
	{
		CurrentWorld->DestroyDemoNetDriver();

		if (CurrentCollection)
		{
			CurrentCollection->SetNetDriver(nullptr);
		}
	}

	UNetDriver* const NetDriver = CurrentWorld->GetNetDriver();
	LoadedWorld->SetNetDriver(NetDriver);

	if (CurrentCollection && LoadedCollection)
	{
		LoadedCollection->SetNetDriver(NetDriver);
		CurrentCollection->SetNetDriver(nullptr);
	}
	if (CurrentStaticCollection && LoadedStaticCollection)
	{
		LoadedStaticCollection->SetNetDriver(NetDriver);
		CurrentStaticCollection->SetNetDriver(nullptr);
	}

	if (NetDriver != NULL)
	{
		CurrentWorld->SetNetDriver(NULL);
		NetDriver->SetWorld(LoadedWorld);
	}
	LoadedWorld->WorldType = CurrentWorld->WorldType;
	LoadedWorld->SetGameInstance(CurrentWorld->GetGameInstance());

	LoadedWorld->TimeSeconds = CurrentWorld->TimeSeconds;
	LoadedWorld->UnpausedTimeSeconds = CurrentWorld->UnpausedTimeSeconds;
	LoadedWorld->RealTimeSeconds = CurrentWorld->RealTimeSeconds;
	LoadedWorld->AudioTimeSeconds = CurrentWorld->AudioTimeSeconds;

	if (NetDriver != NULL)
	{
		LoadedWorld->NextSwitchCountdown = NetDriver->ServerTravelPause;
	}
}

UWorld* FSeamlessTravelHandler::Tick()
{
	bool bWorldChanged = false;
	if (bNeedCancelCleanUp)
	{
		if (!IsAsyncLoading())
		{
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
			bNeedCancelCleanUp = false;
			SetPauseAtMidpoint(false);
		}
	}
	//@fixme: wait for client to verify packages before finishing transition. Is this the right fix?
	// Once the default map is loaded, go ahead and start loading the destination map
	// Once the destination map is loaded, wait until all packages are verified before finishing transition

	check(CurrentWorld);

	UNetDriver* NetDriver = CurrentWorld->GetNetDriver();

	if ( ( LoadedPackage != nullptr || LoadedWorld != nullptr ) && CurrentWorld->NextURL == TEXT( "" ) )
	{
		// Wait for async loads to finish before finishing seamless. (E.g., we've loaded the persistent map but are still loading 'always loaded' sub levels)
		if (LoadedWorld)
		{
			if (IsAsyncLoading() )
			{
				return nullptr;
			}
		}

		// First some validity checks		
		if( CurrentWorld == LoadedWorld )
		{
			// We are not going anywhere - this is the same world. 
			FString Error = FString::Printf(TEXT("Travel aborted - new world is the same as current world" ) );
			UE_LOG(LogWorld, Error, TEXT("%s"), *Error);
			// abort
			CancelTravel();			
		}
		else if ( LoadedWorld->PersistentLevel == nullptr)
		{
			// Package isn't a level
			FString Error = FString::Printf(TEXT("Unable to travel to '%s' - package is not a level"), *LoadedPackage->GetName());
			UE_LOG(LogWorld, Error, TEXT("%s"), *Error);
			// abort
			CancelTravel();
			GEngine->BroadcastTravelFailure(CurrentWorld, ETravelFailure::NoLevel, Error);
		}
		else
		{
			// Make sure there are no pending visibility requests.
			CurrentWorld->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);
				
			if (!GIsEditor && !IsRunningDedicatedServer() && bSwitchedToDefaultMap)
			{
				// If requested, duplicate dynamic levels here after the source levels are created.
				LoadedWorld->DuplicateRequestedLevels(LoadedWorld->GetOuter()->GetFName());
			}

			if (CurrentWorld->GetGameState())
			{
				CurrentWorld->GetGameState()->SeamlessTravelTransitionCheckpoint(!bSwitchedToDefaultMap);
			}
			
			// If it's not still playing, destroy the demo net driver before we start renaming actors.
			if ( CurrentWorld->DemoNetDriver && !CurrentWorld->DemoNetDriver->IsPlaying() && !CurrentWorld->DemoNetDriver->bRecordMapChanges)
			{
				CurrentWorld->DestroyDemoNetDriver();
			}

			// mark actors we want to keep
			FUObjectAnnotationSparseBool KeepAnnotation;
			TArray<AActor*> KeepActors;

			if (AGameModeBase* AuthGameMode = CurrentWorld->GetAuthGameMode())
			{
				AuthGameMode->GetSeamlessTravelActorList(!bSwitchedToDefaultMap, KeepActors);
			}

			const bool bIsClient = (CurrentWorld->GetNetMode() == NM_Client);

			// always keep Controllers that belong to players
			if (bIsClient)
			{
				for (FLocalPlayerIterator It(GEngine, CurrentWorld); It; ++It)
				{
					if (It->PlayerController != nullptr)
					{
						KeepAnnotation.Set(It->PlayerController);
					}
				}
			}
			else
			{
				for( FConstControllerIterator Iterator = CurrentWorld->GetControllerIterator(); Iterator; ++Iterator )
				{
					AController* Player = Iterator->Get();
					if (Player->PlayerState || Cast<APlayerController>(Player) != nullptr)
					{
						KeepAnnotation.Set(Player);
					}
				}
			}

			// ask players what else we should keep
			for (FLocalPlayerIterator It(GEngine, CurrentWorld); It; ++It)
			{
				if (It->PlayerController != nullptr)
				{
					It->PlayerController->GetSeamlessTravelActorList(!bSwitchedToDefaultMap, KeepActors);
				}
			}
			// mark all valid actors specified
			for (AActor* KeepActor : KeepActors)
			{
				if (KeepActor != nullptr)
				{
					KeepAnnotation.Set(KeepActor);
				}
			} 

			TArray<AActor*> ActuallyKeptActors;
			ActuallyKeptActors.Reserve(KeepAnnotation.Num());

			// Rename dynamic actors in the old world's PersistentLevel that we want to keep into the new world
			auto ProcessActor = [this, &KeepAnnotation, &ActuallyKeptActors, NetDriver](AActor* TheActor) -> bool
			{
				const FNetworkObjectInfo* NetworkObjectInfo = NetDriver ? NetDriver->GetNetworkObjectInfo(TheActor) : nullptr;

				const bool bIsInCurrentLevel	= TheActor->GetLevel() == CurrentWorld->PersistentLevel;
				const bool bManuallyMarkedKeep	= KeepAnnotation.Get(TheActor);
				const bool bDormant				= NetworkObjectInfo && NetDriver && NetDriver->ServerConnection && NetworkObjectInfo->DormantConnections.Contains(NetDriver->ServerConnection);
				const bool bKeepNonOwnedActor	= TheActor->Role < ROLE_Authority && !bDormant && !TheActor->IsNetStartupActor();
				const bool bForceExcludeActor	= TheActor->IsA(ALevelScriptActor::StaticClass());

				// Keep if it's in the current level AND it isn't specifically excluded AND it was either marked as should keep OR we don't own this actor
				if (bIsInCurrentLevel && !bForceExcludeActor && (bManuallyMarkedKeep || bKeepNonOwnedActor))
				{
					ActuallyKeptActors.Add(TheActor);
					return true;
				}
				else
				{
					if (bManuallyMarkedKeep)
					{
						UE_LOG(LogWorld, Warning, TEXT("Actor '%s' was indicated to be kept but exists in level '%s', not the persistent level.  Actor will not travel."), *TheActor->GetName(), *TheActor->GetLevel()->GetOutermost()->GetName());
					}

					TheActor->RouteEndPlay(EEndPlayReason::LevelTransition);

					// otherwise, set to be deleted
					KeepAnnotation.Clear(TheActor);
					// close any channels for this actor
					if (NetDriver != nullptr)
					{
						NetDriver->NotifyActorLevelUnloaded(TheActor);
					}
					return false;
				}
			};

			// We move everything but the player controllers first, and then the controllers, keeping their relative order, to avoid breaking any call to GetPlayerController with a fixed index.
			for (FActorIterator It(CurrentWorld); It; ++It)
			{
				AActor* TheActor = *It;
				if (!TheActor->IsA(APlayerController::StaticClass()))
				{
					ProcessActor(TheActor);
				}
			}

			for (FConstPlayerControllerIterator Iterator = CurrentWorld->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				APlayerController* Player = Iterator->Get();
				ProcessActor(Player);
			}

			bool bCreateNewGameMode = !bIsClient;
			{
				// scope because after GC the kept pointers will be bad
				AGameModeBase* KeptGameMode = nullptr;
				AGameStateBase* KeptGameState = nullptr;

				// Second pass to rename and move actors that need to transition into the new world
				// This is done after cleaning up actors that aren't transitioning in case those actors depend on these
				// actors being in the same world.
				for (AActor* const TheActor : ActuallyKeptActors)
				{
					KeepAnnotation.Clear(TheActor);

					TheActor->Rename(nullptr, LoadedWorld->PersistentLevel);
					// if it's a Controller or a Pawn, add it to the appropriate list in the new world's WorldSettings
					if (TheActor->IsA<AController>())
					{
						LoadedWorld->AddController(static_cast<AController*>(TheActor));
					}
					else if (TheActor->IsA<APawn>())
					{
						LoadedWorld->AddPawn(static_cast<APawn*>(TheActor));
					}
					else if (TheActor->IsA<AGameModeBase>())
					{
						KeptGameMode = static_cast<AGameModeBase*>(TheActor);
					}
					else if (TheActor->IsA<AGameStateBase>())
					{
						KeptGameState = static_cast<AGameStateBase*>(TheActor);
					}
					// add to new world's actor list and remove from old
					LoadedWorld->PersistentLevel->Actors.Add(TheActor);
					LoadedWorld->PersistentLevel->ActorsForGC.Add(TheActor);

					TheActor->bActorSeamlessTraveled = true;
				}

				if (KeptGameMode)
				{
					LoadedWorld->CopyGameState(KeptGameMode, KeptGameState);
					bCreateNewGameMode = false;
				}

				CopyWorldData(); // This copies the net driver too (LoadedWorld now has whatever NetDriver was previously held by CurrentWorld)
			}

			// only consider session ended if we're making the final switch so that HUD, etc. UI elements stay around until the end
			CurrentWorld->CleanupWorld(bSwitchedToDefaultMap);
			CurrentWorld->RemoveFromRoot();
			CurrentWorld->ClearFlags(RF_Standalone);

			// Stop all audio to remove references to old world
			if (FAudioDevice* AudioDevice = CurrentWorld->GetAudioDevice())
			{
				AudioDevice->Flush(CurrentWorld);
			}

			// Copy the standby cheat status
			bool bHasStandbyCheatTriggered = (CurrentWorld->NetworkManager) ? CurrentWorld->NetworkManager->bHasStandbyCheatTriggered : false;

			// the new world should not be garbage collected
			LoadedWorld->AddToRoot();
			
			// Update the FWorldContext to point to the newly loaded world
			FWorldContext &CurrentContext = GEngine->GetWorldContextFromWorldChecked(CurrentWorld);
			CurrentContext.SetCurrentWorld(LoadedWorld);

			LoadedWorld->WorldType = CurrentContext.WorldType;
			if (CurrentContext.WorldType == EWorldType::PIE)
			{
				UPackage * WorldPackage = LoadedWorld->GetOutermost();
				check(WorldPackage);
				WorldPackage->SetPackageFlags(PKG_PlayInEditor);
			}

			// Clear any world specific state from NetDriver before switching World
			if (NetDriver)
			{
				NetDriver->PreSeamlessTravelGarbageCollect();
			}

			GWorld = nullptr;

			// mark everything else contained in the world to be deleted
			for (auto LevelIt(CurrentWorld->GetLevelIterator()); LevelIt; ++LevelIt)
			{
				const ULevel* Level = *LevelIt;
				if (Level)
				{
					CastChecked<UWorld>(Level->GetOuter())->MarkObjectsPendingKill();
				}
			}

			for (ULevelStreaming* LevelStreaming : CurrentWorld->StreamingLevels)
			{
				// If an unloaded levelstreaming still has a loaded level we need to mark its objects to be deleted as well
				if ((!LevelStreaming->bShouldBeLoaded || !LevelStreaming->bShouldBeVisible) && LevelStreaming->GetLoadedLevel())
				{
					CastChecked<UWorld>(LevelStreaming->GetLoadedLevel()->GetOuter())->MarkObjectsPendingKill();
				}
			}

			CurrentWorld = nullptr;

			// collect garbage to delete the old world
			// because we marked everything in it pending kill, references will be NULL'ed so we shouldn't end up with any dangling pointers
			CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS, true );

			if (GIsEditor)
			{
				CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS, true );
			}

			appDefragmentTexturePool();
			appDumpTextureMemoryStats(TEXT(""));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// verify that we successfully cleaned up the old world
			GEngine->VerifyLoadMapWorldCleanup();
#endif
			// Clean out NetDriver's Packagemaps, since they may have a lot of NULL object ptrs rotting in the lookup maps.
			if (NetDriver)
			{
				NetDriver->PostSeamlessTravelGarbageCollect();
			}

			// set GWorld to the new world and initialize it
			GWorld = LoadedWorld;
			if (!LoadedWorld->bIsWorldInitialized)
			{
				LoadedWorld->InitWorld();
			}
			bWorldChanged = true;
			// Track session change on seamless travel.
			NETWORK_PROFILER(GNetworkProfiler.TrackSessionChange(true, LoadedWorld->URL));


			checkSlow((LoadedWorld->GetNetMode() == NM_Client) == bIsClient);

			if (bCreateNewGameMode)
			{
				LoadedWorld->SetGameMode(PendingTravelURL);
			}

			// if we've already switched to entry before and this is the transition to the new map, re-create the gameinfo
			if (bSwitchedToDefaultMap && !bIsClient)
			{
				if (FAudioDevice* AudioDevice = LoadedWorld->GetAudioDevice())
				{
					AudioDevice->SetDefaultBaseSoundMix(LoadedWorld->GetWorldSettings()->DefaultBaseSoundMix);
				}

				// Copy cheat flags if the game info is present
				// @todo UE4 FIXMELH - see if this exists, it should not since it's created in GameMode or it's garbage info
				if (LoadedWorld->NetworkManager != nullptr)
				{
					LoadedWorld->NetworkManager->bHasStandbyCheatTriggered = bHasStandbyCheatTriggered;
				}
			}

			// Make sure "always loaded" sub-levels are fully loaded
			{
				SCOPE_LOG_TIME_IN_SECONDS(TEXT("    SeamlessTravel FlushLevelStreaming "), nullptr)
				LoadedWorld->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);	
			}
			
			// Note that AI system will be created only if ai-system-creation conditions are met
			LoadedWorld->CreateAISystem();

			// call initialize functions on everything that wasn't carried over from the old world
			LoadedWorld->InitializeActorsForPlay(PendingTravelURL, false);

			// calling it after InitializeActorsForPlay has been called to have all potential bounding boxed initialized
			UNavigationSystem::InitializeForWorld(LoadedWorld, FNavigationSystemRunMode::GameMode);

			// send loading complete notifications for all local players
			for (FLocalPlayerIterator It(GEngine, LoadedWorld); It; ++It)
			{
				UE_LOG(LogWorld, Log, TEXT("Sending NotifyLoadedWorld for LP: %s PC: %s"), *It->GetName(), It->PlayerController ? *It->PlayerController->GetName() : TEXT("NoPC"));
				if (It->PlayerController != nullptr)
				{
#if !UE_BUILD_SHIPPING
					LOG_SCOPE_VERBOSITY_OVERRIDE(LogNet, ELogVerbosity::VeryVerbose);
					LOG_SCOPE_VERBOSITY_OVERRIDE(LogNetTraffic, ELogVerbosity::VeryVerbose);
					UE_LOG(LogNet, Verbose, TEXT("NotifyLoadedWorld Begin"));
#endif
					It->PlayerController->NotifyLoadedWorld(LoadedWorld->GetOutermost()->GetFName(), bSwitchedToDefaultMap);
					It->PlayerController->ServerNotifyLoadedWorld(LoadedWorld->GetOutermost()->GetFName());
#if !UE_BUILD_SHIPPING
					UE_LOG(LogNet, Verbose, TEXT("NotifyLoadedWorld End"));
#endif
				}
				else
				{
					UE_LOG(LogNet, Error, TEXT("No Player Controller during seamless travel for LP: %s."), *It->GetName());
					// @todo add some kind of travel back to main menu
				}
			}

			// we've finished the transition
			LoadedWorld->bWorldWasLoadedThisTick = true;
			
			if (bSwitchedToDefaultMap)
			{
				// we've now switched to the final destination, so we're done
				
				// remember the last used URL
				CurrentContext.LastURL = PendingTravelURL;

				// Flag our transition as completed before we call PostSeamlessTravel.  This 
				// allows for chaining of maps.

				bTransitionInProgress = false;
				
				double TotalSeamlessTravelTime = FPlatformTime::Seconds() - SeamlessTravelStartTime;
				UE_LOG(LogWorld, Log, TEXT("----SeamlessTravel finished in %.2f seconds ------"), TotalSeamlessTravelTime );
				FLoadTimeTracker::Get().DumpRawLoadTimes();

				AGameModeBase* const GameMode = LoadedWorld->GetAuthGameMode();
				if (GameMode)
				{
					// inform the new GameMode so it can handle players that persisted
					GameMode->PostSeamlessTravel();					
				}

				// Called after post seamless travel to make sure players are setup correctly first
				LoadedWorld->BeginPlay();

				FCoreUObjectDelegates::PostLoadMapWithWorld.Broadcast(LoadedWorld);
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				FCoreUObjectDelegates::PostLoadMap.Broadcast();
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}
			else
			{
				bSwitchedToDefaultMap = true;
				CurrentWorld = LoadedWorld;
				if (!bPauseAtMidpoint)
				{
					StartLoadingDestination();
				}
			}			
		}		
	}
	UWorld* OutWorld = nullptr;
	if( bWorldChanged )
	{
		OutWorld = LoadedWorld;
		// Cleanup the old pointers
		LoadedPackage = nullptr;
		LoadedWorld = nullptr;
	}
	
	return OutWorld;
}

/** seamlessly travels to the given URL by first loading the entry level in the background,
 * switching to it, and then loading the specified level. Does not disrupt network communication or disconnect clients.
 * You may need to implement GameMode::GetSeamlessTravelActorList(), PlayerController::GetSeamlessTravelActorList(),
 * GameMode::PostSeamlessTravel(), and/or GameMode::HandleSeamlessTravelPlayer() to handle preserving any information
 * that should be maintained (player teams, etc)
 * This codepath is designed for worlds that use little or no level streaming and GameModes where the game state
 * is reset/reloaded when transitioning. (like UT)
 * @param URL the URL to travel to; must be relative to the current URL (same server)
 * @param bAbsolute (opt) - if true, URL is absolute, otherwise relative
 * @param MapPackageGuid (opt) - the GUID of the map package to travel to - this is used to find the file when it has been autodownloaded,
 * 				so it is only needed for clients
 */
void UWorld::SeamlessTravel(const FString& SeamlessTravelURL, bool bAbsolute, FGuid MapPackageGuid)
{
	// construct the URL
	FURL NewURL(&GEngine->LastURLFromWorld(this), *SeamlessTravelURL, bAbsolute ? TRAVEL_Absolute : TRAVEL_Relative);
	if (!NewURL.Valid)
	{
		const FString Error = FText::Format( NSLOCTEXT("Engine", "InvalidUrl", "Invalid URL: {0}"), FText::FromString( SeamlessTravelURL ) ).ToString();
		GEngine->BroadcastTravelFailure(this, ETravelFailure::InvalidURL, Error);
	}
	else
	{
		if (NewURL.HasOption(TEXT("Restart")))
		{
			//@todo url cleanup - we should merge the two URLs, not completely replace it
			NewURL = GEngine->LastURLFromWorld(this);
		}
		// tell the handler to start the transition
		FSeamlessTravelHandler &SeamlessTravelHandler = GEngine->SeamlessTravelHandlerForWorld( this );
		if (!SeamlessTravelHandler.StartTravel(this, NewURL, MapPackageGuid) && !SeamlessTravelHandler.IsInTransition())
		{
			const FString Error = FText::Format( NSLOCTEXT("Engine", "InvalidUrl", "Invalid URL: {0}"), FText::FromString( SeamlessTravelURL ) ).ToString();
			GEngine->BroadcastTravelFailure(this, ETravelFailure::InvalidURL, Error);
		}
	}
}

/** @return whether we're currently in a seamless transition */
bool UWorld::IsInSeamlessTravel()
{
	FSeamlessTravelHandler &SeamlessTravelHandler = GEngine->SeamlessTravelHandlerForWorld( this );
	return SeamlessTravelHandler.IsInTransition();
}

/** this function allows pausing the seamless travel in the middle,
 * right before it starts loading the destination (i.e. while in the transition level)
 * this gives the opportunity to perform any other loading tasks before the final transition
 * this function has no effect if we have already started loading the destination (you will get a log warning if this is the case)
 * @param bNowPaused - whether the transition should now be paused
 */
void UWorld::SetSeamlessTravelMidpointPause(bool bNowPaused)
{
	FSeamlessTravelHandler &SeamlessTravelHandler = GEngine->SeamlessTravelHandlerForWorld( this );
	SeamlessTravelHandler.SetPauseAtMidpoint(bNowPaused);
}

int32 UWorld::GetDetailMode()
{
	return GetCachedScalabilityCVars().DetailMode;
}

/**
 * Updates all physics constraint actor joint locations.
 */
void UWorld::UpdateConstraintActors()
{
	if( bAreConstraintsDirty )
	{
		for( TActorIterator<APhysicsConstraintActor> It(this); It; ++It )
		{
			APhysicsConstraintActor* ConstraintActor = *It;
			if( ConstraintActor->GetConstraintComp())
			{
				ConstraintActor->GetConstraintComp()->UpdateConstraintFrames();
			}
		}
		bAreConstraintsDirty = false;
	}
}

int32 UWorld::GetProgressDenominator()
{
	return GetActorCount();
}

int32 UWorld::GetActorCount()
{
	int32 TotalActorCount = 0;
	for( int32 LevelIndex=0; LevelIndex<GetNumLevels(); LevelIndex++ )
	{
		ULevel* Level = GetLevel(LevelIndex);
		TotalActorCount += Level->Actors.Num();
	}
	return TotalActorCount;
}

FConstLevelIterator	UWorld::GetLevelIterator() const
{
	return Levels.CreateConstIterator();
}

ULevel* UWorld::GetLevel( int32 InLevelIndex ) const
{
	check( InLevelIndex < Levels.Num() );
		check(Levels[InLevelIndex]);
	return Levels[ InLevelIndex ];
}

bool UWorld::ContainsLevel( ULevel* InLevel ) const
{
	return Levels.Find( InLevel ) != INDEX_NONE;
}

int32 UWorld::GetNumLevels() const
{
	return Levels.Num();
}

const TArray<class ULevel*>& UWorld::GetLevels() const
{
	return Levels;
}

bool UWorld::AddLevel( ULevel* InLevel )
{
	bool bAddedLevel = false;
	if(ensure(InLevel))
	{
		bAddedLevel = true;
		Levels.AddUnique( InLevel );
		BroadcastLevelsChanged();
	}
	return bAddedLevel;
}

bool UWorld::RemoveLevel( ULevel* InLevel )
{
	bool bRemovedLevel = false;
	if(ContainsLevel( InLevel ) == true )
	{
		bRemovedLevel = true;
		
#if WITH_EDITOR
		if( IsLevelSelected( InLevel ))
		{
			DeSelectLevel( InLevel );
		}
#endif //WITH_EDITOR
		Levels.Remove( InLevel );
		BroadcastLevelsChanged();
	}
	return bRemovedLevel;
}


FString UWorld::GetLocalURL() const
{
	return URL.ToString();
}

/** Returns whether script is executing within the editor. */
bool UWorld::IsPlayInEditor() const
{
	return WorldType == EWorldType::PIE;
}

bool UWorld::IsPlayInPreview() const
{
	return FParse::Param(FCommandLine::Get(), TEXT("PIEVIACONSOLE"));
}


bool UWorld::IsPlayInMobilePreview() const
{
	return FParse::Param(FCommandLine::Get(), TEXT("simmobile")) && !IsPlayInVulkanPreview();
}

bool UWorld::IsPlayInVulkanPreview() const
{
	return FParse::Param(FCommandLine::Get(), TEXT("vulkan"));
}

bool UWorld::IsGameWorld() const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::GamePreview;
}

bool UWorld::IsEditorWorld() const
{
	return WorldType == EWorldType::Editor || WorldType == EWorldType::EditorPreview || WorldType == EWorldType::PIE;
}

bool UWorld::IsPreviewWorld() const
{
	return WorldType == EWorldType::EditorPreview || WorldType == EWorldType::GamePreview;
}

bool UWorld::UsesGameHiddenFlags() const
{
	return IsGameWorld();
}

FString UWorld::GetAddressURL() const
{
	return FString::Printf( TEXT("%s:%i"), *URL.Host, URL.Port );
}

FString UWorld::RemovePIEPrefix(const FString &Source)
{
	// PIE prefix is: UEDPIE_X_MapName (where X is some decimal number)
	const FString LookFor = PLAYWORLD_PACKAGE_PREFIX;

	int32 idx = Source.Find(LookFor);
	if (idx >= 0)
	{
		int32 end = idx + LookFor.Len();
		if ((end >= Source.Len()) || (Source[end] != '_'))
		{
			UE_LOG(LogWorld, Warning, TEXT("Looks like World path invalid PIE prefix (expected '_' characeter after PIE prefix): %s"), *Source);
			return Source;
		}
		for (++end; (end < Source.Len()) && (Source[end] != '_'); ++end)
		{
			if ((Source[end] < '0') || (Source[end] > '9'))
			{
				UE_LOG(LogWorld, Warning, TEXT("Looks like World have invalid PIE prefix (PIE instance not number): %s"), *Source);
				return Source;
			}
		}
		if (end >= Source.Len())
		{
			UE_LOG(LogWorld, Warning, TEXT("Looks like World path invalid PIE prefix (can't find end of PIE prefix): %s"), *Source);
			return Source;
		}
		const FString Prefix = Source.Left(idx);
		const FString Suffix = Source.Right(Source.Len() - end - 1);
		return Prefix + Suffix;
	}

	return Source;
}

UWorld* UWorld::FindWorldInPackage(UPackage* Package)
{
	UWorld* RetVal = nullptr;
	TArray<UObject*> PotentialWorlds;
	GetObjectsWithOuter(Package, PotentialWorlds, false);
	for ( auto ObjIt = PotentialWorlds.CreateConstIterator(); ObjIt; ++ObjIt )
	{
		RetVal = Cast<UWorld>(*ObjIt);
		if ( RetVal )
		{
			break;
		}
	}

	return RetVal;
}

UWorld* UWorld::FollowWorldRedirectorInPackage(UPackage* Package, UObjectRedirector** OptionalOutRedirector)
{
	UWorld* RetVal = nullptr;
	TArray<UObject*> PotentialRedirectors;
	GetObjectsWithOuter(Package, PotentialRedirectors, false);
	for (auto ObjIt = PotentialRedirectors.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UObjectRedirector* Redirector = Cast<UObjectRedirector>(*ObjIt);
		if (Redirector)
		{
			RetVal = Cast<UWorld>(Redirector->DestinationObject);
			if ( RetVal )
			{
				// Patch up the WorldType if found in the PreLoad map
				EWorldType::Type* PreLoadWorldType = UWorld::WorldTypePreLoadMap.Find(Redirector->GetOuter()->GetFName());
				if (PreLoadWorldType)
				{
					RetVal->WorldType = *PreLoadWorldType;
				}

				if ( OptionalOutRedirector )
				{
					(*OptionalOutRedirector) = Redirector;
				}
				break;
			}
		}
	}

	return RetVal;
}

#if WITH_EDITOR


void UWorld::BroadcastSelectedLevelsChanged() 
{ 
	if( bBroadcastSelectionChange )
	{
		SelectedLevelsChangedEvent.Broadcast(); 
	}
}


void UWorld::SelectLevel( ULevel* InLevel )
{
	check( InLevel );
	if( IsLevelSelected( InLevel ) == false )
	{
		SelectedLevels.AddUnique( InLevel );
		BroadcastSelectedLevelsChanged();
	}
}

void UWorld::DeSelectLevel( ULevel* InLevel )
{
	check( InLevel );
	if( IsLevelSelected( InLevel ) == true )
	{
		SelectedLevels.Remove( InLevel );
		BroadcastSelectedLevelsChanged();
	}
}

bool UWorld::IsLevelSelected( ULevel* InLevel ) const
{
	return SelectedLevels.Find( InLevel ) != INDEX_NONE;
}

int32 UWorld::GetNumSelectedLevels() const 
{
	return SelectedLevels.Num();
}

TArray<class ULevel*>& UWorld::GetSelectedLevels()
{
	return SelectedLevels;
}

ULevel* UWorld::GetSelectedLevel( int32 InLevelIndex ) const
{
	check(SelectedLevels[ InLevelIndex ]);
	return SelectedLevels[ InLevelIndex ];
}

void UWorld::SetSelectedLevels( const TArray<class ULevel*>& InLevels )
{
	// Disable the broadcasting of selection changes - we will send a single broadcast when we have finished
	bBroadcastSelectionChange = false;
	SelectedLevels.Empty();
	for (int32 iSelected = 0; iSelected <  InLevels.Num(); iSelected++)
	{
		SelectLevel( InLevels[ iSelected ]);
	}
	// Enable the broadcasting of selection changes
	bBroadcastSelectionChange = true;
	// Broadcast we have change the selections
	BroadcastSelectedLevelsChanged();
}
#endif // WITH_EDITOR

/**
 * Jumps the server to new level.  If bAbsolute is true and we are using seamless traveling, we
 * will do an absolute travel (URL will be flushed).
 *
 * @param URL the URL that we are traveling to
 * @param bAbsolute whether we are using relative or absolute travel
 * @param bShouldSkipGameNotify whether to notify the clients/game or not
 */
bool UWorld::ServerTravel(const FString& FURL, bool bAbsolute, bool bShouldSkipGameNotify)
{
	AGameModeBase* GameMode = GetAuthGameMode();
	
	if (GameMode != nullptr && !GameMode->CanServerTravel(FURL, bAbsolute))
	{
		return false;
	}

	// Set the next travel type to use
	NextTravelType = bAbsolute ? TRAVEL_Absolute : TRAVEL_Relative;

	// if we're not already in a level change, start one now
	// If the bShouldSkipGameNotify is there, then don't worry about seamless travel recursion
	// and accept that we really want to travel
	if (NextURL.IsEmpty() && (!IsInSeamlessTravel() || bShouldSkipGameNotify))
	{
		NextURL = FURL;
		if (GameMode != NULL)
		{
			// Skip notifying clients if requested
			if (!bShouldSkipGameNotify)
			{
				GameMode->ProcessServerTravel(FURL, bAbsolute);
			}
		}
		else
		{
			NextSwitchCountdown = 0;
		}
	}

	return true;
}

void UWorld::SetNavigationSystem( UNavigationSystem* InNavigationSystem)
{
	if (NavigationSystem != NULL && NavigationSystem != InNavigationSystem)
	{
		NavigationSystem->CleanUp(UNavigationSystem::CleanupWithWorld);
	}

	NavigationSystem = InNavigationSystem;
}

/** Set the CurrentLevel for this world. **/
bool UWorld::SetCurrentLevel( class ULevel* InLevel )
{
	bool bChanged = false;
	if( CurrentLevel != InLevel )
	{
		CurrentLevel = InLevel;
		bChanged = true;
	}
	return bChanged;
}

/** Get the CurrentLevel for this world. **/
ULevel* UWorld::GetCurrentLevel() const
{
	return CurrentLevel;
}

ENetMode UWorld::InternalGetNetMode() const
{
	if ( NetDriver != NULL )
	{
		const bool bIsClientOnly = IsRunningClientOnly();
		return bIsClientOnly ? NM_Client : NetDriver->GetNetMode();
	}

	if ( DemoNetDriver )
	{
		return DemoNetDriver->GetNetMode();
	}

// PIE: NetDriver is not initialized so use PlayInSettings
// to determine the Net Mode
#if WITH_EDITOR
	return AttemptDeriveFromPlayInSettings();
#endif

	// Use NextURL or PendingNetURL to derive NetMode
	return AttemptDeriveFromURL();
}

bool UWorld::IsRecordingClientReplay() const
{
	if (GetNetDriver() != nullptr && !GetNetDriver()->IsServer())
	{
		if (DemoNetDriver != nullptr && DemoNetDriver->IsServer())
		{
			return true;
		}
	}

	return false;
}

#if WITH_EDITOR
ENetMode UWorld::AttemptDeriveFromPlayInSettings() const
{
	if (WorldType == EWorldType::PIE)
	{
		const ULevelEditorPlaySettings* PlayInSettings = GetDefault<ULevelEditorPlaySettings>();
		if (PlayInSettings)
		{
			EPlayNetMode PlayNetMode;
			PlayInSettings->GetPlayNetMode(PlayNetMode);

			switch (PlayNetMode)
			{
			case EPlayNetMode::PIE_Client:
			{
				int32 NumberOfClients = 0;
				PlayInSettings->GetPlayNumberOfClients(NumberOfClients);

				bool bAutoConnectToServer = false;
				PlayInSettings->GetAutoConnectToServer(bAutoConnectToServer);

				// Playing as client without listen server in single process,
				// or as a client not going to connect to a server
				if(NumberOfClients == 1 || bAutoConnectToServer == false)
				{
					return NM_Standalone;
				}
				return NM_Client;
			}
			case EPlayNetMode::PIE_ListenServer:
			{
				bool bDedicatedServer = false;
				PlayInSettings->GetPlayNetDedicated(bDedicatedServer);

				if(bDedicatedServer == true)
				{
					return NM_DedicatedServer;
				}

				return NM_ListenServer;
			}
			case EPlayNetMode::PIE_Standalone:
				return NM_Standalone;
			default:
				break;
			}
		}
	}
	return NM_Standalone;
}
#endif

ENetMode UWorld::AttemptDeriveFromURL() const
{
	if (GEngine != nullptr)
	{
		FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(this);

		if (WorldContext != nullptr)
		{
			// NetMode can be derived from the NextURL if it exists
			if (NextURL.Len() > 0)
			{
				FURL NextLevelURL(&WorldContext->LastURL, *NextURL, NextTravelType);

				if (NextLevelURL.Valid)
				{
					if (NextLevelURL.HasOption(TEXT("listen")))
					{
						return NM_ListenServer;
					}
					else if (NextLevelURL.Host.Len() > 0)
					{
						return NM_Client;
					}
				}
			}
			// NetMode can be derived from the PendingNetURL if it exists
			else if (WorldContext->PendingNetGame != nullptr && WorldContext->PendingNetGame->URL.Valid)
			{
				if (WorldContext->PendingNetGame->URL.HasOption(TEXT("listen")))
				{
					return NM_ListenServer;
				}
				else if (WorldContext->PendingNetGame->URL.Host.Len() > 0)
				{
					return NM_Client;
				}
			}
		}
	}

	return NM_Standalone;
}

void UWorld::SetGameState(AGameStateBase* NewGameState)
{
	if (NewGameState == GameState)
	{
		return;
	}

	GameState = NewGameState;

	// Set the GameState on the LevelCollection it's associated with.
	if (NewGameState != nullptr)
	{
	    const ULevel* const CachedLevel = NewGameState->GetLevel();
		if(CachedLevel != nullptr)
		{
	        FLevelCollection* const FoundCollection = CachedLevel->GetCachedLevelCollection();
	        if (FoundCollection)
	        {
		        FoundCollection->SetGameState(NewGameState);
        
		        // For now the static levels use the same GameState as the source dynamic levels.
		        if (FoundCollection->GetType() == ELevelCollectionType::DynamicSourceLevels)
		        {
			        FLevelCollection& StaticLevels = FindOrAddCollectionByType(ELevelCollectionType::StaticLevels);
			        StaticLevels.SetGameState(NewGameState);
		        }
	        }
		}
	}
}

void UWorld::CopyGameState(AGameModeBase* FromGameMode, AGameStateBase* FromGameState)
{
	AuthorityGameMode = FromGameMode;
	SetGameState(FromGameState);
}

void UWorld::GetLightMapsAndShadowMaps(ULevel* Level, TArray<UTexture2D*>& OutLightMapsAndShadowMaps)
{
	class FFindLightmapsArchive : public FArchiveUObject
	{
		/** The array of textures discovered */
		TArray<UTexture2D*>& TextureList;

	public:
		FFindLightmapsArchive(UObject* InSearch, TArray<UTexture2D*>& OutTextureList)
			: TextureList(OutTextureList)
		{
			ArIsObjectReferenceCollector = true;
			ArIsModifyingWeakAndStrongReferences = true; // While we are not modifying them, we want to follow weak references as well

			for (FObjectIterator It; It; ++It)
			{
				It->Mark(OBJECTMARK_TagExp);
			}

			*this << InSearch;
		}

		FArchive& operator<<(class UObject*& Obj)
		{
			// Don't check null references or objects already visited. Also, skip UWorlds as they will pull in more levels than desired
			if (Obj != NULL && Obj->HasAnyMarks(OBJECTMARK_TagExp) && !Obj->IsA(UWorld::StaticClass()))
			{
				if (Obj->IsA(ULightMapTexture2D::StaticClass()) || Obj->IsA(UShadowMapTexture2D::StaticClass()))
				{
					UTexture2D* Tex = Cast<UTexture2D>(Obj);
					if ( ensure(Tex) )
					{
						TextureList.Add(Tex);
					}
				}

				Obj->UnMark(OBJECTMARK_TagExp);
				Obj->Serialize(*this);
			}

			return *this;
		}
	};

	UObject* SearchObject = Level;
	if ( !SearchObject )
	{
		SearchObject = PersistentLevel;
	}

	FFindLightmapsArchive FindArchive(SearchObject, OutLightMapsAndShadowMaps);
}

void UWorld::CreateFXSystem()
{
	if ( !IsRunningDedicatedServer() && !IsRunningCommandlet() )
	{
		FXSystem = FFXSystemInterface::Create(FeatureLevel, Scene->GetShaderPlatform());
		Scene->SetFXSystem(FXSystem);
	}
	else
	{
		FXSystem = NULL;
		Scene->SetFXSystem(NULL);
	}
}

FLevelCollection& UWorld::FindOrAddCollectionByType(const ELevelCollectionType InType)
{
	for (FLevelCollection& LC : LevelCollections)
	{
		if (LC.GetType() == InType)
		{
			return LC;
		}
	}

	// Not found, add a new one.
	FLevelCollection NewLC;
	NewLC.SetType(InType);
	LevelCollections.Add(MoveTemp(NewLC));
	return LevelCollections.Last();
}

int32 UWorld::FindOrAddCollectionByType_Index(const ELevelCollectionType InType)
{
	const int32 FoundIndex = FindCollectionIndexByType(InType);

	if (FoundIndex != INDEX_NONE)
	{
		return FoundIndex;
	}

	// Not found, add a new one.
	FLevelCollection NewLC;
	NewLC.SetType(InType);
	return LevelCollections.Add(MoveTemp(NewLC));
}

FLevelCollection* UWorld::FindCollectionByType(const ELevelCollectionType InType)
{
	for (FLevelCollection& LC : LevelCollections)
	{
		if (LC.GetType() == InType)
		{
			return &LC;
		}
	}

	return nullptr;
}

const FLevelCollection* UWorld::FindCollectionByType(const ELevelCollectionType InType) const
{
	for (const FLevelCollection& LC : LevelCollections)
	{
		if (LC.GetType() == InType)
		{
			return &LC;
		}
	}

	return nullptr;
}

int32 UWorld::FindCollectionIndexByType(const ELevelCollectionType InType) const
{
	return LevelCollections.IndexOfByPredicate([InType](const FLevelCollection& Collection)
	{
		return Collection.GetType() == InType;
	});
}

const FLevelCollection* UWorld::GetActiveLevelCollection() const
{
	if (LevelCollections.IsValidIndex(ActiveLevelCollectionIndex))
	{
		return &LevelCollections[ActiveLevelCollectionIndex];
	}

	return nullptr;
}

void UWorld::SetActiveLevelCollection(int32 LevelCollectionIndex)
{
	ActiveLevelCollectionIndex = LevelCollectionIndex;
	const FLevelCollection* const ActiveLevelCollection = GetActiveLevelCollection();

	if (ActiveLevelCollection == nullptr)
	{
		return;
	}

	PersistentLevel = ActiveLevelCollection->GetPersistentLevel();
	if (IsGameWorld())
	{
		SetCurrentLevel(ActiveLevelCollection->GetPersistentLevel());
	}
	GameState = ActiveLevelCollection->GetGameState();
	NetDriver = ActiveLevelCollection->GetNetDriver();
	DemoNetDriver = ActiveLevelCollection->GetDemoNetDriver();

	// TODO: START TEMP FIX FOR UE-42508
	if (NetDriver && NetDriver->NetDriverName != NAME_None)
	{
		UNetDriver* TempNetDriver = GEngine->FindNamedNetDriver(this, NetDriver->NetDriverName);
		if (TempNetDriver != NetDriver)
		{
			UE_LOG(LogWorld, Warning, TEXT("SetActiveLevelCollection attempted to use an out of date NetDriver: %s"), *(NetDriver->NetDriverName.ToString()));
			NetDriver = TempNetDriver;
		}
	}

	if (DemoNetDriver && DemoNetDriver->NetDriverName != NAME_None)
	{
		UDemoNetDriver* TempDemoNetDriver = Cast<UDemoNetDriver>(GEngine->FindNamedNetDriver(this, DemoNetDriver->NetDriverName));
		if (TempDemoNetDriver != DemoNetDriver)
		{
			UE_LOG(LogWorld, Warning, TEXT("SetActiveLevelCollection attempted to use an out of date DemoNetDriver: %s"), *(DemoNetDriver->NetDriverName.ToString()));
			DemoNetDriver = TempDemoNetDriver;
		}
	}
	// TODO: END TEMP FIX FOR UE-42508
}

static ULevel* DuplicateLevelWithPrefix(ULevel* InLevel, int32 InstanceID )
{
	if (!InLevel || !InLevel->GetOutermost())
	{
		return nullptr;
	}

	const double DuplicateStart = FPlatformTime::Seconds();

	UWorld* OriginalOwningWorld = CastChecked<UWorld>(InLevel->GetOuter());
	UPackage* OriginalPackage = InLevel->GetOutermost();

	const FString OriginalPackageName = OriginalPackage->GetName();

	// Use a PIE prefix for the new package
	const FString PrefixedPackageName = UWorld::ConvertToPIEPackageName( OriginalPackageName, InstanceID );

	// Create a package for duplicated level
	UPackage* NewPackage = CreatePackage( nullptr, *PrefixedPackageName );
	NewPackage->SetPackageFlags( PKG_PlayInEditor );
	NewPackage->PIEInstanceID = InstanceID;
	NewPackage->FileName = OriginalPackage->FileName;
	NewPackage->SetGuid( OriginalPackage->GetGuid() );
	NewPackage->MarkAsFullyLoaded();

	FSoftObjectPath::AddPIEPackageName(NewPackage->GetFName());

	GPlayInEditorID = InstanceID;

	// Create "vestigial" world for the persistent level - it's OwningWorld will still be the main world,
	// but we're treating it like a streaming level (even though it's a duplicate of the persistent level).
	UWorld* NewWorld = NewObject<UWorld>(NewPackage, OriginalOwningWorld->GetFName());
	NewWorld->SetFlags(RF_Transactional);
	NewWorld->WorldType = EWorldType::Game;
	NewWorld->FeatureLevel = ERHIFeatureLevel::Num;

	ULevel::StreamedLevelsOwningWorld.Add(NewPackage->GetFName(), OriginalOwningWorld);

	FObjectDuplicationParameters Parameters( InLevel, NewWorld );
		
	Parameters.DestName			= InLevel->GetFName();
	Parameters.DestClass		= InLevel->GetClass();
	Parameters.FlagMask			= RF_AllFlags;
	Parameters.InternalFlagMask = EInternalObjectFlags::AllFlags;
	Parameters.PortFlags		= PPF_DuplicateForPIE;
	Parameters.DuplicateMode	= EDuplicateMode::PIE;

	ULevel* NewLevel = CastChecked<ULevel>( StaticDuplicateObjectEx( Parameters ) );

	ULevel::StreamedLevelsOwningWorld.Remove(NewPackage->GetFName());

	// Fixup model components. The index buffers have been created for the components in the source world and the order
	// in which components were post-loaded matters. So don't try to guarantee a particular order here, just copy the
	// elements over.
	if ( NewLevel->Model != NULL
			&& NewLevel->Model == InLevel->Model
			&& NewLevel->ModelComponents.Num() == InLevel->ModelComponents.Num() )
	{
		NewLevel->Model->ClearLocalMaterialIndexBuffersData();
		for ( int32 ComponentIndex = 0; ComponentIndex < NewLevel->ModelComponents.Num(); ++ComponentIndex )
		{
			UModelComponent* SrcComponent = InLevel->ModelComponents[ComponentIndex];
			UModelComponent* DestComponent = NewLevel->ModelComponents[ComponentIndex];
			DestComponent->CopyElementsFrom( SrcComponent );
		}
	}

	const double DuplicateEnd = FPlatformTime::Seconds();
	const float TotalSeconds = ( DuplicateEnd - DuplicateStart );

	UE_LOG( LogNet, Log, TEXT( "DuplicateLevelWithPrefix. TotalSeconds: %2.2f" ), TotalSeconds );

	GPlayInEditorID = -1;

	return NewLevel;
}

void UWorld::DuplicateRequestedLevels(const FName MapName)
{
	if (GEngine->Experimental_ShouldPreDuplicateMap(MapName))
	{
		// Duplicate the persistent level and only dynamic levels, but don't add them to the world.
		FLevelCollection DuplicateLevels;
		DuplicateLevels.SetType(ELevelCollectionType::DynamicDuplicatedLevels);
		DuplicateLevels.SetIsVisible(false);
		ULevel* const DuplicatePersistentLevel = DuplicateLevelWithPrefix(PersistentLevel, 1);
		if (!DuplicatePersistentLevel)
		{
			UE_LOG(LogWorld, Warning, TEXT("UWorld::DuplicateRequestedLevels: failed to duplicate persistent level %s. No duplicate level collection will be created."),
				*GetFullNameSafe(PersistentLevel));
			return;
		}
		// Don't tell the server about this level
		DuplicatePersistentLevel->bClientOnlyVisible = true;
		DuplicateLevels.SetPersistentLevel(DuplicatePersistentLevel);
		DuplicateLevels.AddLevel(DuplicatePersistentLevel);

		for (ULevelStreaming* StreamingLevel : StreamingLevels)
		{
			if (StreamingLevel && !StreamingLevel->bIsStatic)
			{
				ULevel* DuplicatedLevel = DuplicateLevelWithPrefix(StreamingLevel->LoadedLevel, 1);
				if (!DuplicatedLevel)
				{
					UE_LOG(LogWorld, Warning, TEXT("UWorld::DuplicateRequestedLevels: failed to duplicate streaming level %s. No duplicate level collection will be created."),
						*GetFullNameSafe(StreamingLevel->LoadedLevel));
					return;
				}
				// Don't tell the server about these levels
				DuplicatedLevel->bClientOnlyVisible = true;
				DuplicateLevels.AddLevel(DuplicatedLevel);
			}
		}

		LevelCollections.Add(MoveTemp(DuplicateLevels));
	}
}

#if WITH_EDITOR
void UWorld::ChangeFeatureLevel(ERHIFeatureLevel::Type InFeatureLevel, bool bShowSlowProgressDialog )
{
	if (InFeatureLevel != FeatureLevel)
	{
		UE_LOG(LogWorld, Log, TEXT("Changing Feature Level (Enum) from %i to %i"), (int)FeatureLevel, (int)InFeatureLevel);
		FScopedSlowTask SlowTask(100.f, NSLOCTEXT("Engine", "ChangingPreviewRenderingLevelMessage", "Changing Preview Rendering Level"), bShowSlowProgressDialog);
		SlowTask.MakeDialog();
		{
			SlowTask.EnterProgressFrame(10.0f);
			// Give all scene components the opportunity to prepare for pending feature level change.
			for (TObjectIterator<USceneComponent> It; It; ++It)
			{
				USceneComponent* SceneComponent = *It;
				if (SceneComponent->GetWorld() == this)
				{
					SceneComponent->PreFeatureLevelChange(InFeatureLevel);
				}
			}

			SlowTask.EnterProgressFrame(10.0f);
			FGlobalComponentReregisterContext RecreateComponents;
			FlushRenderingCommands();

			// Decrement refcount on old feature level
			UMaterialInterface::SetGlobalRequiredFeatureLevel(InFeatureLevel, true);

            SlowTask.EnterProgressFrame(10.0f);
            UMaterial::AllMaterialsCacheResourceShadersForRendering();
            SlowTask.EnterProgressFrame(10.0f);
            UMaterialInstance::AllMaterialsCacheResourceShadersForRendering();
            SlowTask.EnterProgressFrame(10.0f);
            CompileGlobalShaderMap(InFeatureLevel);
            SlowTask.EnterProgressFrame(10.0f);
            GShaderCompilingManager->ProcessAsyncResults(false, true);

			SlowTask.EnterProgressFrame(10.0f);
			//invalidate global bound shader states so they will be created with the new shaders the next time they are set (in SetGlobalBoundShaderState)
			for (TLinkedList<FGlobalBoundShaderStateResource*>::TIterator It(FGlobalBoundShaderStateResource::GetGlobalBoundShaderStateList()); It; It.Next())
			{
				BeginUpdateResourceRHI(*It);
			}

			FeatureLevel = InFeatureLevel;

			SlowTask.EnterProgressFrame(10.0f);
			RecreateScene(InFeatureLevel);

			SlowTask.EnterProgressFrame(10.0f);
			TriggerStreamingDataRebuild();
		}
	}
}

void UWorld::RecreateScene(ERHIFeatureLevel::Type InFeatureLevel)
{
	if (Scene)
	{
		ensure(InFeatureLevel == FeatureLevel);
		for (ULevel* Level : Levels)
		{
			Level->ReleaseRenderingResources();
		}

		Scene->Release();
		IRendererModule& RendererModule = GetRendererModule();
		RendererModule.RemoveScene(Scene);
		RendererModule.AllocateScene(this, bRequiresHitProxies, FXSystem != nullptr, InFeatureLevel);

		for (ULevel* Level : Levels)
		{
			Level->InitializeRenderingResources();
			Level->PrecomputedVisibilityHandler.UpdateScene(Scene);
			Level->PrecomputedVolumeDistanceField.UpdateScene(Scene);
		}
	}
}

void UWorld::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

	if(PersistentLevel && PersistentLevel->OwningWorld)
	{
		TArray<UBlueprint*> LevelBlueprints = PersistentLevel->GetLevelBlueprints();
		for (UBlueprint* Blueprint : LevelBlueprints)
		{
			Blueprint->GetAssetRegistryTags(OutTags);
		}

		// If there are no blueprints FiBData will be empty, the search manager will treat this as indexed
	}

	// Get the full file path with extension
	const FString FullFilePath = FPackageName::LongPackageNameToFilename(GetOutermost()->GetName(), FPackageName::GetMapPackageExtension());

	// Save/Display the file size and modify date
	FDateTime AssetDateModified = IFileManager::Get().GetTimeStamp(*FullFilePath);
	OutTags.Add(FAssetRegistryTag("DateModified", AssetDateModified.ToString(), FAssetRegistryTag::TT_Chronological, FAssetRegistryTag::TD_Date));
	OutTags.Add(FAssetRegistryTag("MapFileSize", Lex::ToString(IFileManager::Get().FileSize(*FullFilePath)), FAssetRegistryTag::TT_Numerical, FAssetRegistryTag::TD_Memory));

	FWorldDelegates::GetAssetTags.Broadcast(this, OutTags);
}
#endif

FPrimaryAssetId UWorld::GetPrimaryAssetId() const
{
	UPackage* Package = GetOutermost();

	if (!Package->HasAnyPackageFlags(PKG_PlayInEditor))
	{
		// Return Map:/path/to/map
		return FPrimaryAssetId(UAssetManager::MapType, Package->GetFName());
	}

	return FPrimaryAssetId();
}

#if WITH_FLEX
UFlexFluidSurfaceComponent* UWorld::GetFlexFluidSurface(UFlexFluidSurface* FlexFluidSurface)
{
	check(FlexFluidSurface);

	UFlexFluidSurfaceComponent** Component = FlexFluidSurfaceMap.Find(FlexFluidSurface);
	return (Component != NULL) ? *Component : NULL;
}

UFlexFluidSurfaceComponent* UWorld::AddFlexFluidSurface(UFlexFluidSurface* FlexFluidSurface)
{
	check(FlexFluidSurface);

	UFlexFluidSurfaceComponent** Component = FlexFluidSurfaceMap.Find(FlexFluidSurface);
	if (Component)
	{
		return *Component;
	}
	else
	{
		FActorSpawnParameters ActorSpawnParameters;
		//necessary for preview in blueprint editor
		ActorSpawnParameters.bAllowDuringConstructionScript = true;
		AFlexFluidSurfaceActor* NewActor = SpawnActor<AFlexFluidSurfaceActor>(AFlexFluidSurfaceActor::StaticClass(), ActorSpawnParameters);
		check(NewActor);

		UFlexFluidSurfaceComponent* NewComponent = NewActor->GetComponent();
		NewComponent->FlexFluidSurface = FlexFluidSurface;	//can't pass arbitrary parameters into SpawnActor			

		FlexFluidSurfaceMap.Add(FlexFluidSurface, NewComponent);
		return NewComponent;
	}
}

void UWorld::RemoveFlexFluidSurface(UFlexFluidSurfaceComponent* Component)
{
	check(Component && Component->FlexFluidSurface);
	FlexFluidSurfaceMap.Remove(Component->FlexFluidSurface);
	AFlexFluidSurfaceActor* Actor = (AFlexFluidSurfaceActor*)Component->GetOwner();
	Actor->Destroy();
}
#endif

/**
* Dump visible actors in current world.
*/
static void DumpVisibleActors(UWorld* InWorld)
{
	UE_LOG(LogWorld, Log, TEXT("------ START DUMP VISIBLE ACTORS ------"));
	for (FActorIterator ActorIterator(InWorld); ActorIterator; ++ActorIterator)
	{
		AActor* Actor = *ActorIterator;
		if (Actor && Actor->GetLastRenderTime() > (InWorld->GetTimeSeconds() - 0.05f))
		{
			UE_LOG(LogWorld, Log, TEXT("Visible Actor : %s"), *Actor->GetFullName());
		}
	}
	UE_LOG(LogWorld, Log, TEXT("------ END DUMP VISIBLE ACTORS ------"));
}

static FAutoConsoleCommandWithWorld DumpVisibleActorsCmd(
	TEXT("DumpVisibleActors"),
	TEXT("Dump visible actors in current world."),
	FConsoleCommandWithWorldDelegate::CreateStatic(DumpVisibleActors)
	);

static void DumpLevelCollections(UWorld* InWorld)
{
	if (!InWorld)
	{
		return;
	}

	UE_LOG(LogWorld, Log, TEXT("--- Dumping LevelCollections ---"));

	for(const FLevelCollection& LC : InWorld->GetLevelCollections())
	{
		UE_LOG(LogWorld, Log, TEXT("%d: %d levels."), static_cast<int32>(LC.GetType()), LC.GetLevels().Num());
		UE_LOG(LogWorld, Log, TEXT("  PersistentLevel: %s"), *GetFullNameSafe(LC.GetPersistentLevel()));
		UE_LOG(LogWorld, Log, TEXT("  GameState: %s"), *GetFullNameSafe(LC.GetGameState()));
		UE_LOG(LogWorld, Log, TEXT("  Levels:"));
		for (const ULevel* Level : LC.GetLevels())
		{
			UE_LOG(LogWorld, Log, TEXT("    %s"), *GetFullNameSafe(Level));
		}
	}
}

static FAutoConsoleCommandWithWorld DumpLevelCollectionsCmd(
	TEXT("DumpLevelCollections"),
	TEXT("Dump level collections in the current world."),
	FConsoleCommandWithWorldDelegate::CreateStatic(DumpLevelCollections)
	);

#if WITH_EDITOR
FAsyncPreRegisterDDCRequest::~FAsyncPreRegisterDDCRequest()
{
	// Discard any results
	if (Handle != 0)
	{
		WaitAsynchronousCompletion();
		TArray<uint8> Junk;
		GetAsynchronousResults(Junk);
	}
}

bool FAsyncPreRegisterDDCRequest::PollAsynchronousCompletion()
{
	if (Handle != 0)
	{
		return GetDerivedDataCacheRef().PollAsynchronousCompletion(Handle);
	}
	return true;
}

void FAsyncPreRegisterDDCRequest::WaitAsynchronousCompletion()
{
	if (Handle != 0)
	{
		GetDerivedDataCacheRef().WaitAsynchronousCompletion(Handle);
	}
}

bool FAsyncPreRegisterDDCRequest::GetAsynchronousResults(TArray<uint8>& OutData)
{
	check(Handle != 0);
	bool bResult = GetDerivedDataCacheRef().GetAsynchronousResults(Handle, OutData);
	// invalidate request after results received
	Handle = 0;
	DDCKey = TEXT("");
	return bResult;
}
#endif

#undef LOCTEXT_NAMESPACE 
