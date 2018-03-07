// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/LevelStreaming.h"
#include "ContentStreaming.h"
#include "Misc/App.h"
#include "UObject/Package.h"
#include "Serialization/ArchiveTraceRoute.h"
#include "Misc/PackageName.h"
#include "UObject/LinkerLoad.h"
#include "EngineGlobals.h"
#include "Engine/Level.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "UObject/ObjectRedirector.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/LevelStreamingPersistent.h"
#include "Engine/LevelStreamingVolume.h"
#include "LevelUtils.h"
#include "EngineUtils.h"
#if WITH_EDITOR
	#include "Framework/Notifications/NotificationManager.h"
	#include "Widgets/Notifications/SNotificationList.h"
#endif
#include "Engine/LevelStreamingKismet.h"
#include "Components/BrushComponent.h"
#include "Engine/CoreSettings.h"
#include "PhysicsEngine/BodySetup.h"

DEFINE_LOG_CATEGORY_STATIC(LogLevelStreaming, Log, All);

#define LOCTEXT_NAMESPACE "World"

int32 ULevelStreamingKismet::UniqueLevelInstanceId = 0;

/**
 * This helper function is defined here so that it can go into the 4.18.1 hotfix (for UE-51791),
 * even though it would make more logical sense to have this logic in a member function of UNetDriver.
 * We're getting away with this because UNetDriver::GuidCache is (unfortunately) public.
 *
 * Renames any package entries in the GuidCache with a path matching UnPrefixedName to have a PIE prefix.
 * This is needed because a client may receive an export for a level package before it's loaded and
 * its name registered with FSoftObjectPath::AddPIEPackageName. In this case, the entry in the GuidCache
 * will not be PIE-prefixed, but when the level is actually loaded, its package will be renamed with the
 * prefix. Any subsequent references to this package won't resolve unless the name is fixed up.
 *
 * @param World the world whose NetDriver will be used for the rename
 * @param UnPrefixedPackageName the path of the package to rename
 */
static void NetDriverRenameStreamingLevelPackageForPIE(const UWorld* World, FName UnPrefixedPackageName)
{
	if (World == nullptr || World->NetDriver == nullptr || !World->NetDriver->GuidCache.IsValid())
	{
		UE_LOG(LogNet, Verbose, TEXT("NetDriverRenameStreamingLevelPackageForPIE, GuidCache is invalid! Package name %s"), *UnPrefixedPackageName.ToString());
		return;
	}

	const FWorldContext* const WorldContext = GEngine->GetWorldContextFromWorld(World);
	if (!WorldContext || WorldContext->WorldType != EWorldType::PIE)
	{
		return;
	}

	for (TPair<FNetworkGUID, FNetGuidCacheObject>& GuidPair : World->NetDriver->GuidCache->ObjectLookup)
	{
		// Only look for packages, which will have a static GUID and an invalid OuterGUID.
		const bool bIsPackage = GuidPair.Key.IsStatic() && !GuidPair.Value.OuterGUID.IsValid();
		if (bIsPackage && GuidPair.Value.PathName == UnPrefixedPackageName)
		{
			GuidPair.Value.PathName = *UWorld::ConvertToPIEPackageName(GuidPair.Value.PathName.ToString(), WorldContext->PIEInstance);
		}
	}
}

FStreamLevelAction::FStreamLevelAction(bool bIsLoading, const FName& InLevelName, bool bIsMakeVisibleAfterLoad, bool bIsShouldBlockOnLoad, const FLatentActionInfo& InLatentInfo, UWorld* World)
	: bLoading(bIsLoading)
	, bMakeVisibleAfterLoad(bIsMakeVisibleAfterLoad)
	, bShouldBlockOnLoad(bIsShouldBlockOnLoad)
	, LevelName(InLevelName)
	, LatentInfo(InLatentInfo)
{
	Level = FindAndCacheLevelStreamingObject( LevelName, World );
	ActivateLevel( Level );
}

void FStreamLevelAction::UpdateOperation(FLatentResponse& Response)
{
	ULevelStreaming* LevelStreamingObject = Level; // to avoid confusion.
	bool bIsOperationFinished = UpdateLevel( LevelStreamingObject );
	Response.FinishAndTriggerIf(bIsOperationFinished, LatentInfo.ExecutionFunction, LatentInfo.Linkage, LatentInfo.CallbackTarget);
}

#if WITH_EDITOR
FString FStreamLevelAction::GetDescription() const
{
	return FString::Printf(TEXT("Streaming Level in progress...(%s)"), *LevelName.ToString());
}
#endif

/**
* Helper function to potentially find a level streaming object by name
*
* @param	LevelName							Name of level to search streaming object for in case Level is NULL
* @return	level streaming object or NULL if none was found
*/
ULevelStreaming* FStreamLevelAction::FindAndCacheLevelStreamingObject( const FName LevelName, UWorld* InWorld )
{
	// Search for the level object by name.
	if( LevelName != NAME_None )
	{
		FString SearchPackageName = MakeSafeLevelName( LevelName, InWorld );
		if (FPackageName::IsShortPackageName(SearchPackageName))
		{
			// Make sure MyMap1 and Map1 names do not resolve to a same streaming level
			SearchPackageName = TEXT("/") + SearchPackageName;
		}

		for (ULevelStreaming* LevelStreaming : InWorld->StreamingLevels)
		{
			// We check only suffix of package name, to handle situations when packages were saved for play into a temporary folder
			// Like Saved/Autosaves/PackageName
			if (LevelStreaming && 
				LevelStreaming->GetWorldAssetPackageName().EndsWith(SearchPackageName, ESearchCase::IgnoreCase))
			{
				return LevelStreaming;
			}
		}
	}

	return NULL;
}

/**
 * Given a level name, returns a level name that will work with Play on Editor or Play on Console
 *
 * @param	InLevelName		Raw level name (no UEDPIE or UED<console> prefix)
 * @param	InWorld			World in which to check for other instances of the name
 */
FString FStreamLevelAction::MakeSafeLevelName( const FName& InLevelName, UWorld* InWorld )
{
	// Special case for PIE, the PackageName gets mangled.
	if (!InWorld->StreamingLevelsPrefix.IsEmpty())
	{
		FString PackageName = FPackageName::GetShortName(InLevelName);
		if (!PackageName.StartsWith(InWorld->StreamingLevelsPrefix))
		{
			PackageName = InWorld->StreamingLevelsPrefix + PackageName;
		}

		if (!FPackageName::IsShortPackageName(InLevelName))
		{
			PackageName = FPackageName::GetLongPackagePath(InLevelName.ToString()) + TEXT("/") + PackageName;
		}
		
		return PackageName;
	}
	
	return InLevelName.ToString();
}
/**
* Handles "Activated" for single ULevelStreaming object.
*
* @param	LevelStreamingObject	LevelStreaming object to handle "Activated" for.
*/
void FStreamLevelAction::ActivateLevel( ULevelStreaming* LevelStreamingObject )
{	
	if( LevelStreamingObject != NULL )
	{
		// Loading.
		if( bLoading )
		{
			UE_LOG(LogStreaming, Log, TEXT("Streaming in level %s (%s)..."),*LevelStreamingObject->GetName(),*LevelStreamingObject->GetWorldAssetPackageName());
			LevelStreamingObject->bShouldBeLoaded		= true;
			LevelStreamingObject->bShouldBeVisible		|= bMakeVisibleAfterLoad;
			LevelStreamingObject->bShouldBlockOnLoad	= bShouldBlockOnLoad;
		}
		// Unloading.
		else 
		{
			UE_LOG(LogStreaming, Log, TEXT("Streaming out level %s (%s)..."),*LevelStreamingObject->GetName(),*LevelStreamingObject->GetWorldAssetPackageName());
			LevelStreamingObject->bShouldBeLoaded		= false;
			LevelStreamingObject->bShouldBeVisible		= false;
		}

		UWorld* LevelWorld = CastChecked<UWorld>(LevelStreamingObject->GetOuter());
		// If we have a valid world
		if(LevelWorld)
		{
			// Notify players of the change
			for( FConstPlayerControllerIterator Iterator = LevelWorld->GetPlayerControllerIterator(); Iterator; ++Iterator )
			{
				APlayerController* PlayerController = Iterator->Get();

				UE_LOG(LogLevel, Log, TEXT("ActivateLevel %s %i %i %i"), 
					*LevelStreamingObject->GetWorldAssetPackageName(), 
					LevelStreamingObject->bShouldBeLoaded, 
					LevelStreamingObject->bShouldBeVisible, 
					LevelStreamingObject->bShouldBlockOnLoad );



				PlayerController->LevelStreamingStatusChanged( 
					LevelStreamingObject, 
					LevelStreamingObject->bShouldBeLoaded, 
					LevelStreamingObject->bShouldBeVisible,
					LevelStreamingObject->bShouldBlockOnLoad, 
					INDEX_NONE);

			}
		}
	}
	else
	{
		UE_LOG(LogLevel, Warning, TEXT("Failed to find streaming level object associated with '%s'"), *LevelName.ToString() );
	}
}

/**
* Handles "UpdateOp" for single ULevelStreaming object.
*
* @param	LevelStreamingObject	LevelStreaming object to handle "UpdateOp" for.
*
* @return true if operation has completed, false if still in progress
*/
bool FStreamLevelAction::UpdateLevel( ULevelStreaming* LevelStreamingObject )
{
	// No level streaming object associated with this sequence.
	if( LevelStreamingObject == NULL )
	{
		return true;
	}
	// Level is neither loaded nor should it be so we finished (in the sense that we have a pending GC request) unloading.
	else if( (LevelStreamingObject->GetLoadedLevel() == NULL) && !LevelStreamingObject->bShouldBeLoaded )
	{
		return true;
	}
	// Level shouldn't be loaded but is as background level streaming is enabled so we need to fire finished event regardless.
	else if (LevelStreamingObject->GetLoadedLevel() && !LevelStreamingObject->bShouldBeLoaded && !GUseBackgroundLevelStreaming)
	{
		return true;
	}
	// Level is both loaded and wanted so we finished loading.
	else if(	LevelStreamingObject->GetLoadedLevel() && LevelStreamingObject->bShouldBeLoaded 
	// Make sure we are visible if we are required to be so.
	&&	(!bMakeVisibleAfterLoad || LevelStreamingObject->GetLoadedLevel()->bIsVisible) )
	{
		return true;
	}

	// Loading/ unloading in progress.
	return false;
}

/*-----------------------------------------------------------------------------
	ULevelStreaming* implementation.
-----------------------------------------------------------------------------*/

void ULevelStreaming::PostLoad()
{
	Super::PostLoad();

	const bool PIESession = GetWorld()->WorldType == EWorldType::PIE || GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor);

	// If this streaming level was saved with a short package name, try to convert it to a long package name
	if ( !PIESession && PackageName_DEPRECATED != NAME_None )
	{
		const FString DeprecatedPackageNameString = PackageName_DEPRECATED.ToString();
		if ( FPackageName::IsShortPackageName(PackageName_DEPRECATED) == false )
		{
			// Convert the FName reference to a TSoftObjectPtr, then broadcast that we loaded a reference
			// so this reference is gathered by the cooker without having to resave the package.
			SetWorldAssetByPackageName(PackageName_DEPRECATED);
			WorldAsset.GetUniqueID().PostLoadPath();
		}
		else
		{
			UE_LOG(LogLevelStreaming, Display, TEXT("Invalid streaming level package name (%s). Only long package names are supported. This streaming level may not load or save properly."), *DeprecatedPackageNameString);
		}
	}

	if ( !PIESession && !WorldAsset.IsNull() )
	{
		const FString WorldPackageName = GetWorldAssetPackageName();
		if (FPackageName::DoesPackageExist(WorldPackageName) == false)
		{
			UE_LOG(LogLevelStreaming, Display, TEXT("Failed to find streaming level package file: %s. This streaming level may not load or save properly."), *WorldPackageName);
#if WITH_EDITOR
			if (GIsEditor)
			{
				// Launch notification to inform user of default change
				FFormatNamedArguments Args;
				Args.Add(TEXT("PackageName"), FText::FromString(WorldPackageName));
				FNotificationInfo Info(FText::Format(LOCTEXT("LevelStreamingFailToStreamLevel", "Failed to find streamed level {PackageName}, please fix the reference to it in the Level Browser"), Args));
				Info.ExpireDuration = 7.0f;

				FSlateNotificationManager::Get().AddNotification(Info);
			}
#endif // WITH_EDITOR
		}
	}

	if (GetLinkerUE4Version() < VER_UE4_LEVEL_STREAMING_DRAW_COLOR_TYPE_CHANGE)
	{
		LevelColor = DrawColor_DEPRECATED;
	}
}

UWorld* ULevelStreaming::GetWorld() const
{
	// Fail gracefully if a CDO
	if(IsTemplate())
	{
		return nullptr;
	}
	// Otherwise 
	else
	{
		return CastChecked<UWorld>(GetOuter());
	}
}

void ULevelStreaming::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	
	if (Ar.IsLoading())
	{
		if (GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor) && GetOutermost()->PIEInstanceID != INDEX_NONE)
		{
			RenameForPIE(GetOutermost()->PIEInstanceID);
		}
	}
}

FName ULevelStreaming::GetLODPackageName() const
{
	if (LODPackageNames.IsValidIndex(LevelLODIndex))
	{
		return LODPackageNames[LevelLODIndex];
	}
	else
	{
		return GetWorldAssetPackageFName();
	}
}

FName ULevelStreaming::GetLODPackageNameToLoad() const
{
	if (LODPackageNames.IsValidIndex(LevelLODIndex))
	{
		return LODPackageNamesToLoad.IsValidIndex(LevelLODIndex) ? LODPackageNamesToLoad[LevelLODIndex] : NAME_None;
	}
	else
	{
		return PackageNameToLoad;
	}
}

void ULevelStreaming::SetLoadedLevel(class ULevel* Level)
{ 
	// Pending level should be unloaded at this point
	check(PendingUnloadLevel == nullptr);
	PendingUnloadLevel = LoadedLevel;
	LoadedLevel = Level;
	CachedLoadedLevelPackageName = (LoadedLevel ? LoadedLevel->GetOutermost()->GetFName() : NAME_None);

	// Cancel unloading for this level, in case it was queued for it
	FLevelStreamingGCHelper::CancelUnloadRequest(LoadedLevel);

	// Add this level to the correct collection
	const ELevelCollectionType CollectionType =	bIsStatic ? ELevelCollectionType::StaticLevels : ELevelCollectionType::DynamicSourceLevels;

	FLevelCollection& LC = GetWorld()->FindOrAddCollectionByType(CollectionType);
	LC.RemoveLevel(PendingUnloadLevel);

	// Remove the loaded level from its current collection, if any.
	if (LoadedLevel && LoadedLevel->GetCachedLevelCollection())
	{
		LoadedLevel->GetCachedLevelCollection()->RemoveLevel(LoadedLevel);
	}
	LC.AddLevel(LoadedLevel);
}

void ULevelStreaming::DiscardPendingUnloadLevel(UWorld* PersistentWorld)
{
	if (PendingUnloadLevel)
	{
		if (PendingUnloadLevel->bIsVisible)
		{
			PersistentWorld->RemoveFromWorld(PendingUnloadLevel);
		}

		if (!PendingUnloadLevel->bIsVisible)
		{
			FLevelStreamingGCHelper::RequestUnload(PendingUnloadLevel);
			PendingUnloadLevel = nullptr;
		}
	}
}

bool ULevelStreaming::RequestLevel(UWorld* PersistentWorld, bool bAllowLevelLoadRequests, EReqLevelBlock BlockPolicy)
{
	// Quit early in case load request already issued
	if (bHasLoadRequestPending)
	{
		return true;
	}

	// Previous attempts have failed, no reason to try again
	if (bFailedToLoad)
	{
		return false;
	}
	
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ULevelStreaming_RequestLevel);
	FScopeCycleCounterUObject Context(PersistentWorld);

	// Package name we want to load
	const bool bIsGameWorld = PersistentWorld->IsGameWorld();
	const FName DesiredPackageName = bIsGameWorld ? GetLODPackageName() : GetWorldAssetPackageFName();

	// Check if currently loaded level is what we want right now
	if (LoadedLevel && CachedLoadedLevelPackageName == DesiredPackageName)
	{
		return true;
	}

	// Can not load new level now, there is still level pending unload
	if (PendingUnloadLevel != NULL)
	{
		return false;
	}

	// Can not load new level now either, we're still processing visibility for this one
	ULevel* PendingLevelVisOrInvis = (PersistentWorld->CurrentLevelPendingVisibility) ? PersistentWorld->CurrentLevelPendingVisibility : PersistentWorld->CurrentLevelPendingInvisibility;
    if (PendingLevelVisOrInvis && PendingLevelVisOrInvis == LoadedLevel)
    {
		UE_LOG(LogLevelStreaming, Verbose, TEXT("Delaying load of new level %s, because %s still processing visibility request."), *DesiredPackageName.ToString(), *CachedLoadedLevelPackageName.ToString());
		return false;
	}

	EPackageFlags PackageFlags = PKG_ContainsMap;
	int32 PIEInstanceID = INDEX_NONE;

	// copy streaming level on demand if we are in PIE
	// (the world is already loaded for the editor, just find it and copy it)
	if ( PersistentWorld->IsPlayInEditor() )
	{
		if (PersistentWorld->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
		{
			PackageFlags |= PKG_PlayInEditor;
		}
		PIEInstanceID = PersistentWorld->GetOutermost()->PIEInstanceID;

		const FString NonPrefixedLevelName = UWorld::StripPIEPrefixFromPackageName(DesiredPackageName.ToString(), PersistentWorld->StreamingLevelsPrefix);
		UPackage* EditorLevelPackage = FindObjectFast<UPackage>(nullptr, FName(*NonPrefixedLevelName));

		bool bShouldDuplicate = EditorLevelPackage && (BlockPolicy == AlwaysBlock || EditorLevelPackage->IsDirty() || !GEngine->PreferToStreamLevelsInPIE());
		if (bShouldDuplicate)
		{
			// Do the duplication
			UWorld* PIELevelWorld = UWorld::DuplicateWorldForPIE(NonPrefixedLevelName, PersistentWorld);
			if (PIELevelWorld)
			{
				PIELevelWorld->PersistentLevel->bAlreadyMovedActors = true; // As we have duplicated the world, the actors will already have been transformed
				check(PendingUnloadLevel == NULL);
				SetLoadedLevel(PIELevelWorld->PersistentLevel);

				// Broadcast level loaded event to blueprints
				{
					QUICK_SCOPE_CYCLE_COUNTER(STAT_OnLevelLoaded_Broadcast);
					OnLevelLoaded.Broadcast();
				}

				return true;
			}
			else if (PersistentWorld->WorldComposition == NULL) // In world composition streaming levels are not loaded by default
			{
				if ( bAllowLevelLoadRequests )
				{
					UE_LOG(LogLevelStreaming, Log, TEXT("World to duplicate for PIE '%s' not found. Attempting load."), *NonPrefixedLevelName);
				}
				else
				{
					UE_LOG(LogLevelStreaming, Warning, TEXT("Unable to duplicate PIE World: '%s'"), *NonPrefixedLevelName);
				}
			}
		}
	}

	// Try to find the [to be] loaded package.
	UPackage* LevelPackage = (UPackage*)StaticFindObjectFast(UPackage::StaticClass(), NULL, DesiredPackageName, 0, 0, RF_NoFlags, EInternalObjectFlags::PendingKill);

	// Package is already or still loaded.
	if (LevelPackage)
	{
		// Find world object and use its PersistentLevel pointer.
		UWorld* World = UWorld::FindWorldInPackage(LevelPackage);

		// Check for a redirector. Follow it, if found.
		if (!World)
		{
			World = UWorld::FollowWorldRedirectorInPackage(LevelPackage);
			if (World)
			{
				LevelPackage = World->GetOutermost();
			}
		}

		if (World != nullptr)
		{
			if (World->IsPendingKill())
			{
				// We're trying to reload a level that has very recently been marked for garbage collection, it might not have been cleaned up yet
				// So continue attempting to reload the package if possible
				UE_LOG(LogLevelStreaming, Verbose, TEXT("RequestLevel: World is pending kill %s"), *DesiredPackageName.ToString());
				return false;
			}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (World->PersistentLevel == NULL)
			{
				UE_LOG(LogLevelStreaming, Log, TEXT("World exists but PersistentLevel doesn't for %s, most likely caused by reference to world of unloaded level and GC setting reference to NULL while keeping world object"), *World->GetOutermost()->GetName());
				// print out some debug information...
				StaticExec(World, *FString::Printf(TEXT("OBJ REFS CLASS=WORLD NAME=%s shortest"), *World->GetPathName()));
				TMap<UObject*,UProperty*> Route = FArchiveTraceRoute::FindShortestRootPath( World, true, GARBAGE_COLLECTION_KEEPFLAGS );
				FString ErrorString = FArchiveTraceRoute::PrintRootPath( Route, World );
				UE_LOG(LogLevelStreaming, Log, TEXT("%s"), *ErrorString);
				// before asserting
				checkf(World->PersistentLevel,TEXT("Most likely caused by reference to world of unloaded level and GC setting reference to NULL while keeping world object"));
				return false;
			}
#endif
			if (World->PersistentLevel != LoadedLevel)
			{
#if WITH_EDITOR
				if (PIEInstanceID != INDEX_NONE)
				{
					World->PersistentLevel->FixupForPIE(PIEInstanceID);
				}
#endif

				SetLoadedLevel(World->PersistentLevel);
				// Broadcast level loaded event to blueprints
				OnLevelLoaded.Broadcast();
			}
			
			return true;
		}
	}

	// Async load package if world object couldn't be found and we are allowed to request a load.
	if (bAllowLevelLoadRequests)
	{
		const FName DesiredPackageNameToLoad = bIsGameWorld ? GetLODPackageNameToLoad() : PackageNameToLoad;
		const FString PackageNameToLoadFrom = DesiredPackageNameToLoad != NAME_None ? DesiredPackageNameToLoad.ToString() : DesiredPackageName.ToString();

		if (FPackageName::DoesPackageExist(PackageNameToLoadFrom, NULL, NULL))
		{
			bHasLoadRequestPending = true;
			
			ULevel::StreamedLevelsOwningWorld.Add(DesiredPackageName, PersistentWorld);
			UWorld::WorldTypePreLoadMap.FindOrAdd(DesiredPackageName) = PersistentWorld->WorldType;

			// Kick off async load request.
			STAT_ADD_CUSTOMMESSAGE_NAME( STAT_NamedMarker, *(FString( TEXT( "RequestLevel - " ) + DesiredPackageName.ToString() )) );
			LoadPackageAsync(DesiredPackageName.ToString(), nullptr, *PackageNameToLoadFrom, FLoadPackageAsyncDelegate::CreateUObject(this, &ULevelStreaming::AsyncLevelLoadComplete), PackageFlags, PIEInstanceID);

			// streamingServer: server loads everything?
			// Editor immediately blocks on load and we also block if background level streaming is disabled.
			if (BlockPolicy == AlwaysBlock || (ShouldBeAlwaysLoaded() && BlockPolicy != NeverBlock))
			{
				if (IsAsyncLoading())
				{
					UE_LOG(LogStreaming, Display, TEXT("ULevelStreaming::RequestLevel(%s) is flushing async loading"), *DesiredPackageName.ToString());
				}

				// Finish all async loading.
				FlushAsyncLoading();
			}
		}
		else
		{
			UE_LOG(LogStreaming, Error,TEXT("Couldn't find file for package %s."), *PackageNameToLoadFrom);
			bFailedToLoad = true;
			return false;
		}
	}

	return true;
}

void ULevelStreaming::AsyncLevelLoadComplete(const FName& InPackageName, UPackage* InLoadedPackage, EAsyncLoadingResult::Type Result)
{
	bHasLoadRequestPending = false;

	if (InLoadedPackage)
	{
		UPackage* LevelPackage = InLoadedPackage;
		
		// Try to find a UWorld object in the level package.
		UWorld* World = UWorld::FindWorldInPackage(LevelPackage);

		if (World)
		{
			ULevel* Level = World->PersistentLevel;
			if (Level)
			{
				UWorld* LevelOwningWorld = Level->OwningWorld;
				if (LevelOwningWorld)
				{
					ULevel* PendingLevelVisOrInvis = (LevelOwningWorld->CurrentLevelPendingVisibility) ? LevelOwningWorld->CurrentLevelPendingVisibility : LevelOwningWorld->CurrentLevelPendingInvisibility;
					if (PendingLevelVisOrInvis && PendingLevelVisOrInvis == LoadedLevel)
					{
						// We can't change current loaded level if it's still processing visibility request
						// On next UpdateLevelStreaming call this loaded package will be found in memory by RequestLevel function in case visibility request has finished
						UE_LOG(LogLevelStreaming, Verbose, TEXT("Delaying setting result of async load new level %s, because current loaded level still processing visibility request"), *LevelPackage->GetName());
					}
					else
					{
						check(PendingUnloadLevel == nullptr);
					
#if WITH_EDITOR
						int32 PIEInstanceID = GetOutermost()->PIEInstanceID;
						if (PIEInstanceID != INDEX_NONE)
						{
							World->PersistentLevel->FixupForPIE(PIEInstanceID);
						}
#endif

						SetLoadedLevel(Level);
						// Broadcast level loaded event to blueprints
						OnLevelLoaded.Broadcast();
					}
				}

				Level->HandleLegacyMapBuildData();

				// Notify the streamer to start building incrementally the level streaming data.
				IStreamingManager::Get().AddLevel(Level);

				// Make sure this level will start to render only when it will be fully added to the world
				if (LODPackageNames.Num() > 0)
				{
					Level->bRequireFullVisibilityToRender = true;
					// LOD levels should not be visible on server
					Level->bClientOnlyVisible = LODPackageNames.Contains(InLoadedPackage->GetFName());
				}
			
				// In the editor levels must be in the levels array regardless of whether they are visible or not
				if (ensure(LevelOwningWorld) && LevelOwningWorld->WorldType == EWorldType::Editor)
				{
					LevelOwningWorld->AddLevel(Level);
#if WITH_EDITOR
					// We should also at this point, apply the level's editor transform
					if (!Level->bAlreadyMovedActors)
					{
						FLevelUtils::ApplyEditorTransform(this, false);
						Level->bAlreadyMovedActors = true;
					}
#endif // WITH_EDITOR
				}
			}
			else
			{
				UE_LOG(LogLevelStreaming, Warning, TEXT("Couldn't find ULevel object in package '%s'"), *InPackageName.ToString() );
			}
		}
		else
		{
			// No world in this package
			LevelPackage->ClearPackageFlags(PKG_ContainsMap);

			// There could have been a redirector in the package. Attempt to follow it.
			UObjectRedirector* WorldRedirector = nullptr;
			UWorld* DestinationWorld = UWorld::FollowWorldRedirectorInPackage(LevelPackage, &WorldRedirector);
			if (DestinationWorld)
			{
				// To follow the world redirector for level streaming...
				// 1) Update all globals that refer to the redirector package by name
				// 2) Update the PackageNameToLoad to refer to the new package location
				// 3) If the package name to load was the same as the destination package name...
				//         ... update the package name to the new package and let the next RequestLevel try this process again.
				//    If the package name to load was different...
				//         ... it means the specified package name was explicit and we will just load from another file.

				FName OldDesiredPackageName = InPackageName;
				TWeakObjectPtr<UWorld>* OwningWorldPtr = ULevel::StreamedLevelsOwningWorld.Find(OldDesiredPackageName);
				UWorld* OwningWorld = OwningWorldPtr ? OwningWorldPtr->Get() : NULL;
				ULevel::StreamedLevelsOwningWorld.Remove(OldDesiredPackageName);

				// Try again with the destination package to load.
				// IMPORTANT: check this BEFORE changing PackageNameToLoad, otherwise you wont know if the package name was supposed to be different.
				const bool bLoadingIntoDifferentPackage = (GetWorldAssetPackageFName() != PackageNameToLoad) && (PackageNameToLoad != NAME_None);

				// ... now set PackageNameToLoad
				PackageNameToLoad = DestinationWorld->GetOutermost()->GetFName();

				if ( PackageNameToLoad != OldDesiredPackageName )
				{
					EWorldType::Type* OldPackageWorldType = UWorld::WorldTypePreLoadMap.Find(OldDesiredPackageName);
					if ( OldPackageWorldType )
					{
						UWorld::WorldTypePreLoadMap.FindOrAdd(PackageNameToLoad) = *OldPackageWorldType;
						UWorld::WorldTypePreLoadMap.Remove(OldDesiredPackageName);
					}
				}

				// Now determine if we are loading into the package explicitly or if it is okay to just load the other package.
				if ( bLoadingIntoDifferentPackage )
				{
					// Loading into a new custom package explicitly. Load the destination world directly into the package.
					// Detach the linker to load from a new file into the same package.
					FLinkerLoad* PackageLinker = FLinkerLoad::FindExistingLinkerForPackage(LevelPackage);
					if (PackageLinker)
					{
						PackageLinker->Detach();
						DeleteLoader(PackageLinker);
						PackageLinker = nullptr;
					}

					// Make sure the redirector is not in the way of the new world.
					// Pass NULL as the name to make a new unique name and GetTransientPackage() for the outer to remove it from the package.
					WorldRedirector->Rename(NULL, GetTransientPackage(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);

					// Change the loaded world's type back to inactive since it won't be used.
					DestinationWorld->WorldType = EWorldType::Inactive;
				}
				else
				{
					// Loading the requested package normally. Fix up the destination world then update the requested package to the destination.
					if (OwningWorld)
					{
						if (DestinationWorld->PersistentLevel)
						{
							DestinationWorld->PersistentLevel->OwningWorld = OwningWorld;
						}

						// In some cases, BSP render data is not created because the OwningWorld was not set correctly.
						// Regenerate that render data here
						DestinationWorld->PersistentLevel->InvalidateModelSurface();
						DestinationWorld->PersistentLevel->CommitModelSurfaces();
					}
					
					SetWorldAsset(DestinationWorld);
				}
			}
		}
	}
	else if (Result == EAsyncLoadingResult::Canceled)
	{
		// Cancel level streaming
		bHasLoadRequestPending = false;
		bShouldBeLoaded = false;
	}
	else
	{
		UE_LOG(LogLevelStreaming, Warning, TEXT("Failed to load package '%s'"), *InPackageName.ToString() );
		
		bFailedToLoad = true;
 		bHasLoadRequestPending = false;
 		bShouldBeLoaded = false;
	}

	// Clean up the world type list and owning world list now that PostLoad has occurred
	UWorld::WorldTypePreLoadMap.Remove(InPackageName);
	ULevel::StreamedLevelsOwningWorld.Remove(InPackageName);

	STAT_ADD_CUSTOMMESSAGE_NAME( STAT_NamedMarker, *(FString( TEXT( "RequestLevelComplete - " ) + InPackageName.ToString() )) );
}

bool ULevelStreaming::IsLevelVisible() const
{
	return LoadedLevel != NULL && LoadedLevel->bIsVisible;
}

bool ULevelStreaming::IsStreamingStatePending() const
{
	UWorld* PersistentWorld = GetWorld();
	if (PersistentWorld)
	{
		if (IsLevelLoaded() == ShouldBeLoaded() && 
			(IsLevelVisible() == ShouldBeVisible() || !ShouldBeLoaded())) // visibility state does not matter if sub-level set to be unloaded
		{
			const FName DesiredPackageName = PersistentWorld->IsGameWorld() ? GetLODPackageName() : GetWorldAssetPackageFName();
			if (!LoadedLevel || CachedLoadedLevelPackageName == DesiredPackageName)
			{
				return false;
			}
		}
		
		return true;
	}
	
	return false;
}

ULevelStreaming* ULevelStreaming::CreateInstance(FString InstanceUniqueName)
{
	ULevelStreaming* StreamingLevelInstance = nullptr;
	
	UWorld* InWorld = GetWorld();
	if (InWorld)
	{
		// Create instance long package name 
		FString InstanceShortPackageName = InWorld->StreamingLevelsPrefix + FPackageName::GetShortName(InstanceUniqueName);
		FString InstancePackagePath = FPackageName::GetLongPackagePath(GetWorldAssetPackageName()) + TEXT("/");
		FName	InstanceUniquePackageName = FName(*(InstancePackagePath + InstanceShortPackageName));

		// check if instance name is unique among existing streaming level objects
		const bool bUniqueName = (InWorld->StreamingLevels.IndexOfByPredicate(ULevelStreaming::FPackageNameMatcher(InstanceUniquePackageName)) == INDEX_NONE);
				
		if (bUniqueName)
		{
			StreamingLevelInstance = NewObject<ULevelStreaming>(InWorld, GetClass(), NAME_None, RF_Transient, NULL);
			// new level streaming instance will load the same map package as this object
			StreamingLevelInstance->PackageNameToLoad = (PackageNameToLoad == NAME_None ? GetWorldAssetPackageFName() : PackageNameToLoad);
			// under a provided unique name
			StreamingLevelInstance->SetWorldAssetByPackageName(InstanceUniquePackageName);
			StreamingLevelInstance->bShouldBeLoaded = false;
			StreamingLevelInstance->bShouldBeVisible = false;
			StreamingLevelInstance->LevelTransform = LevelTransform;

			// add a new instance to streaming level list
			InWorld->StreamingLevels.Add(StreamingLevelInstance);
		}
		else
		{
			UE_LOG(LogStreaming, Warning, TEXT("Provided streaming level instance name is not unique: %s"), *InstanceUniquePackageName.ToString());
		}
	}
	
	return StreamingLevelInstance;
}

void ULevelStreaming::BroadcastLevelLoadedStatus(UWorld* PersistentWorld, FName LevelPackageName, bool bLoaded)
{
	for (ULevelStreaming* StreamingLevel : PersistentWorld->StreamingLevels)
	{
		if (StreamingLevel->GetWorldAssetPackageFName() == LevelPackageName)
		{
			if (bLoaded)
			{
				StreamingLevel->OnLevelLoaded.Broadcast();
			}
			else
			{
				StreamingLevel->OnLevelUnloaded.Broadcast();
			}
		}
	}
}
	
void ULevelStreaming::BroadcastLevelVisibleStatus(UWorld* PersistentWorld, FName LevelPackageName, bool bVisible)
{
	for (ULevelStreaming* StreamingLevel : PersistentWorld->StreamingLevels)
	{
		if (StreamingLevel->GetWorldAssetPackageFName() == LevelPackageName)
		{
			if (bVisible)
			{
				StreamingLevel->OnLevelShown.Broadcast();
			}
			else
			{
				StreamingLevel->OnLevelHidden.Broadcast();
			}
		}
	}
}

void ULevelStreaming::SetWorldAsset(const TSoftObjectPtr<UWorld>& NewWorldAsset)
{
	WorldAsset = NewWorldAsset;
	bHasCachedWorldAssetPackageFName = false;
}

FString ULevelStreaming::GetWorldAssetPackageName() const
{
	return GetWorldAssetPackageFName().ToString();
}

FName ULevelStreaming::GetWorldAssetPackageFName() const
{
	if (!bHasCachedWorldAssetPackageFName)
	{
		ULevelStreaming* MutableThis = const_cast<ULevelStreaming*>(this);
		MutableThis->CachedWorldAssetPackageFName = FName(*FPackageName::ObjectPathToPackageName(WorldAsset.ToString()));
		MutableThis->bHasCachedWorldAssetPackageFName = true;
	}
	return CachedWorldAssetPackageFName;
}

void ULevelStreaming::SetWorldAssetByPackageName(FName InPackageName)
{
	const FString TargetWorldPackageName = InPackageName.ToString();
	const FString TargetWorldObjectName = FPackageName::GetLongPackageAssetName(TargetWorldPackageName);
	TSoftObjectPtr<UWorld> NewWorld;
	NewWorld = TargetWorldPackageName + TEXT(".") + TargetWorldObjectName;
	SetWorldAsset(NewWorld);
}

void ULevelStreaming::RenameForPIE(int32 PIEInstanceID)
{
	const UWorld* const World = GetWorld();

	// Apply PIE prefix so this level references
	if (!WorldAsset.IsNull())
	{
		// Store original name 
		if (PackageNameToLoad == NAME_None)
		{
			FString NonPrefixedName = UWorld::StripPIEPrefixFromPackageName(
				GetWorldAssetPackageName(), 
				UWorld::BuildPIEPackagePrefix(PIEInstanceID));
			PackageNameToLoad = FName(*NonPrefixedName);
		}
		FName PlayWorldStreamingPackageName = FName(*UWorld::ConvertToPIEPackageName(GetWorldAssetPackageName(), PIEInstanceID));
		FSoftObjectPath::AddPIEPackageName(PlayWorldStreamingPackageName);
		SetWorldAssetByPackageName(PlayWorldStreamingPackageName);

		NetDriverRenameStreamingLevelPackageForPIE(World, PackageNameToLoad);
	}
	
	// Rename LOD levels if any
	if (LODPackageNames.Num() > 0)
	{
		LODPackageNamesToLoad.Reset(LODPackageNames.Num());
		for (FName& LODPackageName : LODPackageNames)
		{
			// Store LOD level original package name
			LODPackageNamesToLoad.Add(LODPackageName); 
			// Apply PIE prefix to package name			
			const FName NonPrefixedLODPackageName = LODPackageName;
			LODPackageName = FName(*UWorld::ConvertToPIEPackageName(LODPackageName.ToString(), PIEInstanceID));
			FSoftObjectPath::AddPIEPackageName(LODPackageName);

			NetDriverRenameStreamingLevelPackageForPIE(World, NonPrefixedLODPackageName);
		}
	}
}

bool ULevelStreaming::ShouldBeLoaded() const
{
	return true;
}

bool ULevelStreaming::ShouldBeVisible() const
{
	if( GetWorld()->IsGameWorld() )
	{
		// Game and play in editor viewport codepath.
		return bShouldBeVisible && ShouldBeLoaded();
	}
	else
	{
		// Editor viewport codepath.
		return bShouldBeVisibleInEditor;
	}
}

FBox ULevelStreaming::GetStreamingVolumeBounds()
{
	FBox Bounds(ForceInit);

	// Iterate over each volume associated with this LevelStreaming object
	for(int32 VolIdx=0; VolIdx<EditorStreamingVolumes.Num(); VolIdx++)
	{
		ALevelStreamingVolume* StreamingVol = EditorStreamingVolumes[VolIdx];
		if(StreamingVol && StreamingVol->GetBrushComponent())
		{
			Bounds += StreamingVol->GetBrushComponent()->BrushBodySetup->AggGeom.CalcAABB(StreamingVol->GetBrushComponent()->GetComponentTransform());
		}
	}

	return Bounds;
}

#if WITH_EDITOR
void ULevelStreaming::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* OutermostProperty = PropertyChangedEvent.Property;
	if ( OutermostProperty != NULL )
	{
		const FName PropertyName = OutermostProperty->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(ULevelStreaming, LevelTransform))
		{
			GetWorld()->UpdateLevelStreaming();
		}

		if (PropertyName == GET_MEMBER_NAME_CHECKED(ULevelStreaming, EditorStreamingVolumes))
		{
			RemoveStreamingVolumeDuplicates();

			// Update levels references in each streaming volume 
			for (TActorIterator<ALevelStreamingVolume> It(GetWorld()); It; ++It)
			{
				(*It)->UpdateStreamingLevelsRefs();
			}
		}

		else if (PropertyName == GET_MEMBER_NAME_CHECKED(ULevelStreaming, LevelColor))
		{
			// Make sure the level's Level Color change is applied immediately by reregistering the
			// components of the actor's in the level
			if (LoadedLevel != nullptr)
			{
				LoadedLevel->MarkLevelComponentsRenderStateDirty();
			}
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(ULevelStreaming, WorldAsset))
		{
			bHasCachedWorldAssetPackageFName = false;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ULevelStreaming::RemoveStreamingVolumeDuplicates()
{
	for (int32 VolumeIdx = EditorStreamingVolumes.Num()-1; VolumeIdx >= 0; VolumeIdx--)
	{
		ALevelStreamingVolume* Volume = EditorStreamingVolumes[VolumeIdx];
		if (Volume) // Allow duplicate null entries, for array editor convenience
		{
			int32 DuplicateIdx = EditorStreamingVolumes.Find(Volume);
			check(DuplicateIdx != INDEX_NONE);
			if (DuplicateIdx != VolumeIdx)
			{
				EditorStreamingVolumes.RemoveAt(VolumeIdx);
			}
		}
	}
}

#endif // WITH_EDITOR

ULevelStreaming::ULevelStreaming(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsStatic(false)
{
	bShouldBeVisibleInEditor = true;
	LevelColor = FLinearColor::White;
	LevelTransform = FTransform::Identity;
	MinTimeBetweenVolumeUnloadRequests = 2.0f;
	bDrawOnLevelStatusMap = true;
	LevelLODIndex = INDEX_NONE;
}

#if WITH_EDITOR
void ULevelStreaming::PreEditUndo()
{
	FLevelUtils::RemoveEditorTransform(this, false);
}

void ULevelStreaming::PostEditUndo()
{
	FLevelUtils::ApplyEditorTransform(this, false);
}
#endif // WITH_EDITOR


#if WITH_EDITOR
const FName& ULevelStreaming::GetFolderPath() const
{
	return FolderPath;
}

void ULevelStreaming::SetFolderPath(const FName& InFolderPath)
{
	if (FolderPath != InFolderPath)
	{
		Modify();

		FolderPath = InFolderPath;

		// @TODO: Should this be broadcasted through the editor, similar to BroadcastLevelActorFolderChanged?
	}
}
#endif	// WITH_EDITOR

/*-----------------------------------------------------------------------------
	ULevelStreamingPersistent implementation.
-----------------------------------------------------------------------------*/
ULevelStreamingPersistent::ULevelStreamingPersistent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/*-----------------------------------------------------------------------------
	ULevelStreamingKismet implementation.
-----------------------------------------------------------------------------*/
ULevelStreamingKismet::ULevelStreamingKismet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULevelStreamingKismet::PostLoad()
{
	Super::PostLoad();

	// Initialize startup state of the streaming level
	if ( GetWorld()->IsGameWorld() )
	{
		bShouldBeLoaded = bInitiallyLoaded;
		bShouldBeVisible = bInitiallyVisible;
	}
}

bool ULevelStreamingKismet::ShouldBeLoaded() const
{
	return bShouldBeLoaded;
}

ALevelScriptActor* ULevelStreaming::GetLevelScriptActor()
{
	if (LoadedLevel)
	{
		return LoadedLevel->GetLevelScriptActor();
	}
	return nullptr;
}

ULevelStreamingKismet* ULevelStreamingKismet::LoadLevelInstance(UObject* WorldContextObject, const FString& LevelName, const FVector& Location, const FRotator& Rotation, bool& bOutSuccess)
{ 
	bOutSuccess = false; 
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) 
	{
		return nullptr;
	}
	
	// Check whether requested map exists, this could be very slow if LevelName is a short package name
	FString LongPackageName;
	bOutSuccess = FPackageName::SearchForPackageOnDisk(LevelName, &LongPackageName);
	if (!bOutSuccess) 
	{
		return nullptr;
	}
		 
    // Create Unique Name for sub-level package
	const FString ShortPackageName = FPackageName::GetShortName(LongPackageName);
	const FString PackagePath = FPackageName::GetLongPackagePath(LongPackageName);
	FString UniqueLevelPackageName = PackagePath + TEXT("/") + World->StreamingLevelsPrefix + ShortPackageName;
	UniqueLevelPackageName += TEXT("_LevelInstance_") + FString::FromInt(++UniqueLevelInstanceId);
    
	// Setup streaming level object that will load specified map
	ULevelStreamingKismet* StreamingLevel = NewObject<ULevelStreamingKismet>(World, ULevelStreamingKismet::StaticClass(), NAME_None, RF_Transient, NULL);
    StreamingLevel->SetWorldAssetByPackageName(FName(*UniqueLevelPackageName));
    StreamingLevel->LevelColor = FColor::MakeRandomColor();
    StreamingLevel->bShouldBeLoaded = true;
    StreamingLevel->bShouldBeVisible = true;
    StreamingLevel->bShouldBlockOnLoad = false;
    StreamingLevel->bInitiallyLoaded = true;
    StreamingLevel->bInitiallyVisible = true;
	// Transform
    StreamingLevel->LevelTransform = FTransform(Rotation, Location);
	// Map to Load
    StreamingLevel->PackageNameToLoad = FName(*LongPackageName);
          
    // Add the new level to world.
    World->StreamingLevels.Add(StreamingLevel);
      
	bOutSuccess = true;
    return StreamingLevel;
}	

/*-----------------------------------------------------------------------------
	ULevelStreamingAlwaysLoaded implementation.
-----------------------------------------------------------------------------*/

ULevelStreamingAlwaysLoaded::ULevelStreamingAlwaysLoaded(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShouldBeVisible = true;
}

void ULevelStreamingAlwaysLoaded::GetPrestreamPackages(TArray<UObject*>& OutPrestream)
{
	OutPrestream.Add(GetLoadedLevel()); // Nulls will be ignored later
}


bool ULevelStreamingAlwaysLoaded::ShouldBeLoaded() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
