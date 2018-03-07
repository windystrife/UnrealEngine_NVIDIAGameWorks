// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "GameplayEffectTypes.h"
#include "GameplayPrediction.h"
#include "Engine/World.h"
#include "AssetData.h"
#include "Engine/StreamableManager.h"
#include "GameplayCue_Types.h"
#include "GameplayCueTranslator.h"
#include "GameplayCueManager.generated.h"

#define GAMEPLAYCUE_DEBUG 0

class AGameplayCueNotify_Actor;
class UAbilitySystemComponent;
class UObjectLibrary;

DECLARE_DELEGATE_OneParam(FOnGameplayCueNotifySetLoaded, TArray<FSoftObjectPath>);
DECLARE_DELEGATE_OneParam(FGameplayCueProxyTick, float);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FShouldLoadGCNotifyDelegate, const FAssetData&, FName);

/** An ObjectLibrary for the GameplayCue Notifies. Wraps 2 underlying UObjectLibraries plus options/delegates for how they are loaded */ 
USTRUCT()
struct FGameplayCueObjectLibrary
{
	GENERATED_BODY()
	FGameplayCueObjectLibrary()
	{
		bHasBeenInitialized = false;
	}

	// Paths to search for
	UPROPERTY()
	TArray<FString> Paths;

	// Callback for when load finishes
	FOnGameplayCueNotifySetLoaded OnLoaded;

	// Callback for "should I add this FAssetData to the set"
	FShouldLoadGCNotifyDelegate ShouldLoad;

	// Object library for actor based notifies
	UPROPERTY()
	UObjectLibrary* ActorObjectLibrary;

	// Object library for object based notifies
	UPROPERTY()
	UObjectLibrary* StaticObjectLibrary;

	// Priority to use if async loading
	TAsyncLoadPriority AsyncPriority;

	// Should we force a sync scan on the asset registry in order to discover asset data, or just use what is there?
	UPROPERTY()
	bool bShouldSyncScan;

	// Should we start async loading everything that we find (that passes ShouldLoad delegate check)
	UPROPERTY()
	bool bShouldAsyncLoad;

	// Should we sync load everything that we find (that passes ShouldLoad delegate check)
	UPROPERTY()
	bool bShouldSyncLoad;

	// Set to put the loaded asset data into. If null we will use the global set (RuntimeGameplayCueObjectLibrary.CueSet)
	UPROPERTY()
	UGameplayCueSet* CueSet;

	UPROPERTY()
	bool bHasBeenInitialized;
};

UCLASS()
class GAMEPLAYABILITIES_API UGameplayCueManager : public UDataAsset
{
	GENERATED_UCLASS_BODY()

	// -------------------------------------------------------------
	// Wrappers to handle replicating executed cues
	// -------------------------------------------------------------

	virtual void InvokeGameplayCueExecuted_FromSpec(UAbilitySystemComponent* OwningComponent, const FGameplayEffectSpec& Spec, FPredictionKey PredictionKey);
	virtual void InvokeGameplayCueExecuted(UAbilitySystemComponent* OwningComponent, const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext);
	virtual void InvokeGameplayCueExecuted_WithParams(UAbilitySystemComponent* OwningComponent, const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters);

	virtual void InvokeGameplayCueAddedAndWhileActive_FromSpec(UAbilitySystemComponent* OwningComponent, const FGameplayEffectSpec& Spec, FPredictionKey PredictionKey);

	/** Start or stop a gameplay cue send context. Used by FScopedGameplayCueSendContext above, when all contexts are removed the cues are flushed */
	void StartGameplayCueSendContext();
	void EndGameplayCueSendContext();

	/** Send out any pending cues */
	virtual void FlushPendingCues();

	virtual void OnCreated();

	virtual void OnEngineInitComplete();

	/** Process a pending cue, return false if the cue should be rejected. */
	virtual bool ProcessPendingCueExecute(FGameplayCuePendingExecute& PendingCue);

	/** Returns true if two pending cues match, can be overridden in game */
	virtual bool DoesPendingCueExecuteMatch(FGameplayCuePendingExecute& PendingCue, FGameplayCuePendingExecute& ExistingCue);

	// -------------------------------------------------------------
	// Handling GameplayCues at runtime:
	// -------------------------------------------------------------

	/** Main entry point for handling a gameplaycue event. These functions will call the 3 functions below to handle gameplay cues */
	virtual void HandleGameplayCues(AActor* TargetActor, const FGameplayTagContainer& GameplayCueTags, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);
	virtual void HandleGameplayCue(AActor* TargetActor, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);

	/** 1. returns true to ignore gameplay cues */
	virtual bool ShouldSuppressGameplayCues(AActor* TargetActor);

	/** 2. Allows Tag to be translated in place to a different Tag. See FGameplayCueTranslorManager */
	void TranslateGameplayCue(FGameplayTag& Tag, AActor* TargetActor, const FGameplayCueParameters& Parameters);

	/** 3. Actually routes the gameplaycue event to the right place.  */
	virtual void RouteGameplayCue(AActor* TargetActor, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);

	// -------------------------------------------------------------

	/** Force any instanced GameplayCueNotifies to stop */
	virtual void EndGameplayCuesFor(AActor* TargetActor);

	/** Returns the cached instance cue. Creates it if it doesn't exist */
	virtual AGameplayCueNotify_Actor* GetInstancedCueActor(AActor* TargetActor, UClass* CueClass, const FGameplayCueParameters& Parameters);

	/** Notify that this actor is finished and should be destroyed or recycled */
	virtual void NotifyGameplayCueActorFinished(AGameplayCueNotify_Actor* Actor);

	/** Notify to say the actor is about to be destroyed and the GC manager needs to remove references to it. This should not happen in normal play with recycling enabled, but could happen in replays. */
	virtual void NotifyGameplayCueActorEndPlay(AGameplayCueNotify_Actor* Actor);

	/** Resets preallocation for a given world */
	void ResetPreallocation(UWorld* World);

	/** Prespawns a single actor for gameplaycue notify actor classes that need prespawning (should be called by outside gamecode, such as gamestate) */
	void UpdatePreallocation(UWorld* World);

	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	void OnPreReplayScrub(UWorld* World);

	/** Prints what classess exceeded their preallocation sizes during runtime */
	void DumpPreallocationStats(UWorld* World);

	// -------------------------------------------------------------
	//  Loading GameplayCueNotifies from ObjectLibraries
	//
	//	There are two libraries in the GameplayCueManager:
	//	1. The RunTime object library, which is initialized with the "always loaded" paths, via UAbilitySystemGlobals::GetGameplayCueNotifyPaths()
	//		-GC Notifies in this path are loaded at startup
	//		-Everything loaded will go into the global gameplaycue set, which is where GC events will be routed to by default
	//
	//	2. The Editor object library, which is initialized with the "all valid" paths, via UGameplayCueManager::GetValidGameplayCuePaths()
	//		-Only used in the editor.
	//		-Never loads clases or scans directly itself. 
	//		-Just reflect asset registry to show about "all gameplay cue notifies the game could know about"
	//
	// -------------------------------------------------------------

	/** Returns the Ryntime cueset, which is the global cueset used in runtime, as opposed to the editor cue set which is used only when running the editor */
	UGameplayCueSet* GetRuntimeCueSet();

	/** Called to setup and initialize the runtime library. The passed in paths will be scanned and added to the global gameplay cue set where appropriate */
	void InitializeRuntimeObjectLibrary();

	// Will return the Runtime cue set and the EditorCueSet, if the EditorCueSet is available. This is used mainly for handling asset created/deleted in the editor
	TArray<UGameplayCueSet*> GetGlobalCueSets();

#if WITH_EDITOR
	/** Called from editor to soft load all gameplay cue notifies for the GameplayCueEditor */
	void InitializeEditorObjectLibrary();

	/** Calling this will make the GC manager periodically refresh the EditorObjectLibrary until the asset registry is finished scanning */
	void RequestPeriodicUpdateOfEditorObjectLibraryWhileWaitingOnAssetRegistry();

	/** Get filenames of all GC notifies we know about (loaded or not). Useful for cooking */
	void GetEditorObjectLibraryGameplayCueNotifyFilenames(TArray<FString>& Filenames) const;

	/** Looks in the EditorObjectLibrary for a notify for this tag, if it finds it, it loads it and puts it in the RuntimeObjectLibrary so that it can be previewed in the editor */
	void LoadNotifyForEditorPreview(FGameplayTag GameplayCueTag);

	UGameplayCueSet* GetEditorCueSet();

	FSimpleMulticastDelegate OnEditorObjectLibraryUpdated;
	bool EditorObjectLibraryFullyInitialized;

	FTimerHandle EditorPeriodicUpdateHandle;
#endif

protected:

	virtual bool ShouldSyncScanRuntimeObjectLibraries() const;
	virtual bool ShouldSyncLoadRuntimeObjectLibraries() const;
	virtual bool ShouldAsyncLoadRuntimeObjectLibraries() const;

	/** Refreshes the existing, already initialized, object libraries. */
	void RefreshObjectLibraries();

	/** Internal function to actually init the FGameplayCueObjectLibrary */
	void InitObjectLibrary(FGameplayCueObjectLibrary& Library);

	virtual TArray<FString> GetAlwaysLoadedGameplayCuePaths();

	/** returns list of valid gameplay cue paths. Subclasses may override this to specify locations that aren't part of the "always loaded" LoadedPaths array */
	virtual TArray<FString>	GetValidGameplayCuePaths() { return GetAlwaysLoadedGameplayCuePaths(); }

	UPROPERTY(transient)
	FGameplayCueObjectLibrary RuntimeGameplayCueObjectLibrary;

	UPROPERTY(transient)
	FGameplayCueObjectLibrary EditorGameplayCueObjectLibrary;

	/** Handle to maintain ownership of gameplay cue assets. */
	TSharedPtr<FStreamableHandle> GameplayCueAssetHandle;

public:		

	/** Called before loading any gameplay cue notifies from object libraries. Allows subclasses to skip notifies. */
	virtual bool ShouldLoadGameplayCueAssetData(const FAssetData& Data) const { return true; }
	
	int32 FinishLoadingGameplayCueNotifies();

	FStreamableManager	StreamableManager;
	
	TMap<FGCNotifyActorKey, TWeakObjectPtr<AGameplayCueNotify_Actor> > NotifyMapActor;

	void PrintGameplayCueNotifyMap();

	virtual void PrintLoadedGameplayCueNotifyClasses();

	virtual class UWorld* GetWorld() const override;

	static UWorld* GetCachedWorldForGameplayCueNotifies();

	DECLARE_EVENT_FourParams(UGameplayCueManager, FOnRouteGameplayCue, AActor*, FGameplayTag, EGameplayCueEvent::Type, const FGameplayCueParameters&);
	FOnRouteGameplayCue& OnGameplayCueRouted() { return OnRouteGameplayCue; }

#if WITH_EDITOR

	/** Handles updating an object library when a new asset is created */
	void HandleAssetAdded(UObject *Object);

	/** Handles cleaning up an object library if it matches the passed in object */
	void HandleAssetDeleted(UObject *Object);

	/** Warns if we move a GameplayCue notify out of the valid search paths */
	void HandleAssetRenamed(const FAssetData& Data, const FString& String);

	bool VerifyNotifyAssetIsInValidPath(FString Path);

	bool bAccelerationMapOutdated;

	FOnGameplayCueNotifyChange	OnGameplayCueNotifyAddOrRemove;

	/** Animation Preview Hacks */
	static class USceneComponent* PreviewComponent;
	static UWorld* PreviewWorld;
	static FGameplayCueProxyTick PreviewProxyTick;
#endif

	static bool IsGameplayCueRecylingEnabled();
	
	virtual bool ShouldAsyncLoadObjectLibrariesAtStart() const { return true; }

	FGameplayCueTranslationManager	TranslationManager;

	// -------------------------------------------------------------
	//  Debugging Help
	// -------------------------------------------------------------

#if GAMEPLAYCUE_DEBUG
	virtual FGameplayCueDebugInfo* GetDebugInfo(int32 Handle, bool Reset=false);
#endif

	/** If true, this will synchronously load missing gameplay cues */
	virtual bool ShouldSyncLoadMissingGameplayCues() const;

	/** If true, this will asynchronously load missing gameplay cues, and execute cue when the load finishes */
	virtual bool ShouldAsyncLoadMissingGameplayCues() const;

	/** Handles what to do if a missing cue is requested. If return true, this was loaded and execution should continue */
	virtual bool HandleMissingGameplayCue(UGameplayCueSet* OwningSet, struct FGameplayCueNotifyData& CueData, AActor* TargetActor, EGameplayCueEvent::Type EventType, FGameplayCueParameters& Parameters);

protected:

#if WITH_EDITOR
	//This handles the case where GameplayCueNotifications have changed between sessions, which is possible in editor.
	virtual void ReloadObjectLibrary(UWorld* World, const UWorld::InitializationValues IVS);
#endif

	void BuildCuesToAddToGlobalSet(const TArray<FAssetData>& AssetDataList, FName TagPropertyName, TArray<struct FGameplayCueReferencePair>& OutCuesToAdd, TArray<FSoftObjectPath>& OutAssetsToLoad, FShouldLoadGCNotifyDelegate = FShouldLoadGCNotifyDelegate());

	/** The cue manager has a tendency to produce a lot of RPCs. This logs out when we are attempting to fire more RPCs than will actually go off */
	void CheckForTooManyRPCs(FName FuncName, const FGameplayCuePendingExecute& PendingCue, const FString& CueID, const FGameplayEffectContext* EffectContext);

	void OnGameplayCueNotifyAsyncLoadComplete(TArray<FSoftObjectPath> StringRef);

	void OnMissingCueAsyncLoadComplete(FSoftObjectPath LoadedObject, TWeakObjectPtr<UGameplayCueSet> OwningSet, FGameplayTag GameplayCueTag, TWeakObjectPtr<AActor> TargetActor, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	void CheckForPreallocation(UClass* GCClass);

	/** Hardref to the gameplaycue notify classes we have async loaded*/
	UPROPERTY(transient)
	TArray<UClass*> LoadedGameplayCueNotifyClasses;

	/** Classes that we need to preallocate instances for */
	UPROPERTY(transient)
	TArray<TSubclassOf<AGameplayCueNotify_Actor>> GameplayCueClassesForPreallocation;

	/** List of gameplay cue executes that haven't been processed yet */
	UPROPERTY(transient)
	TArray<FGameplayCuePendingExecute> PendingExecuteCues;

	/** Number of active gameplay cue send contexts, when it goes to 0 cues are flushed */
	UPROPERTY(transient)
	int32 GameplayCueSendContextCount;

	/** Cached world we are currently handling cues for. Used for non instanced GC Notifies that need world. */
	static UWorld* CurrentWorld;

	FPreallocationInfo& GetPreallocationInfo(UWorld* World);

	UPROPERTY(transient)
	TArray<FPreallocationInfo>	PreallocationInfoList_Internal;

	FOnRouteGameplayCue OnRouteGameplayCue;
};