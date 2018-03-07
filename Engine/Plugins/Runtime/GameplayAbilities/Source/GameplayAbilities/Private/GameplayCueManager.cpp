// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayCueManager.h"
#include "Engine/ObjectLibrary.h"
#include "GameplayCueNotify_Actor.h"
#include "Misc/MessageDialog.h"
#include "Stats/StatsMisc.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "DrawDebugHelpers.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsModule.h"
#include "AbilitySystemGlobals.h"
#include "AssetRegistryModule.h"
#include "GameplayCueInterface.h"
#include "GameplayCueSet.h"
#include "GameplayCueNotify_Static.h"
#include "AbilitySystemComponent.h"
#include "Net/DataReplication.h"
#include "Engine/ActorChannel.h"
#include "Engine/NetConnection.h"
#include "Net/UnrealNetwork.h"
#include "Misc/CoreDelegates.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/Engine.h"
#include "ISequenceRecorder.h"
#define LOCTEXT_NAMESPACE "GameplayCueManager"
#endif

int32 LogGameplayCueActorSpawning = 0;
static FAutoConsoleVariableRef CVarLogGameplayCueActorSpawning(TEXT("AbilitySystem.LogGameplayCueActorSpawning"),	LogGameplayCueActorSpawning, TEXT("Log when we create GameplayCueNotify_Actors"), ECVF_Default	);

int32 DisplayGameplayCues = 0;
static FAutoConsoleVariableRef CVarDisplayGameplayCues(TEXT("AbilitySystem.DisplayGameplayCues"),	DisplayGameplayCues, TEXT("Display GameplayCue events in world as text."), ECVF_Default	);

int32 DisableGameplayCues = 0;
static FAutoConsoleVariableRef CVarDisableGameplayCues(TEXT("AbilitySystem.DisableGameplayCues"),	DisableGameplayCues, TEXT("Disables all GameplayCue events in the world."), ECVF_Default );

float DisplayGameplayCueDuration = 5.f;
static FAutoConsoleVariableRef CVarDurationeGameplayCues(TEXT("AbilitySystem.GameplayCue.DisplayDuration"),	DisplayGameplayCueDuration, TEXT("Disables all GameplayCue events in the world."), ECVF_Default );

int32 GameplayCueRunOnDedicatedServer = 0;
static FAutoConsoleVariableRef CVarDedicatedServerGameplayCues(TEXT("AbilitySystem.GameplayCue.RunOnDedicatedServer"), GameplayCueRunOnDedicatedServer, TEXT("Run gameplay cue events on dedicated server"), ECVF_Default );

#if WITH_EDITOR
USceneComponent* UGameplayCueManager::PreviewComponent = nullptr;
UWorld* UGameplayCueManager::PreviewWorld = nullptr;
FGameplayCueProxyTick UGameplayCueManager::PreviewProxyTick;
#endif

UWorld* UGameplayCueManager::CurrentWorld = nullptr;

UGameplayCueManager::UGameplayCueManager(const FObjectInitializer& PCIP)
: Super(PCIP)
{
#if WITH_EDITOR
	bAccelerationMapOutdated = true;
	EditorObjectLibraryFullyInitialized = false;
#endif
}

void UGameplayCueManager::OnCreated()
{
	FWorldDelegates::OnWorldCleanup.AddUObject(this, &UGameplayCueManager::OnWorldCleanup);
	FWorldDelegates::OnPreWorldFinishDestroy.AddUObject(this, &UGameplayCueManager::OnWorldCleanup, true, true);
	FNetworkReplayDelegates::OnPreScrub.AddUObject(this, &UGameplayCueManager::OnPreReplayScrub);
		
#if WITH_EDITOR
	FCoreDelegates::OnFEngineLoopInitComplete.AddUObject(this, &UGameplayCueManager::OnEngineInitComplete);
#endif
}

void UGameplayCueManager::OnEngineInitComplete()
{
#if WITH_EDITOR
	FCoreDelegates::OnFEngineLoopInitComplete.AddUObject(this, &UGameplayCueManager::OnEngineInitComplete);
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().OnInMemoryAssetCreated().AddUObject(this, &UGameplayCueManager::HandleAssetAdded);
	AssetRegistryModule.Get().OnInMemoryAssetDeleted().AddUObject(this, &UGameplayCueManager::HandleAssetDeleted);
	AssetRegistryModule.Get().OnAssetRenamed().AddUObject(this, &UGameplayCueManager::HandleAssetRenamed);
	FWorldDelegates::OnPreWorldInitialization.AddUObject(this, &UGameplayCueManager::ReloadObjectLibrary);

	InitializeEditorObjectLibrary();
#endif
}

bool IsDedicatedServerForGameplayCue()
{
#if WITH_EDITOR
	// This will handle dedicated server PIE case properly
	return GEngine->ShouldAbsorbCosmeticOnlyEvent();
#else
	// When in standalone non editor, this is the fastest way to check
	return IsRunningDedicatedServer();
#endif
}


void UGameplayCueManager::HandleGameplayCues(AActor* TargetActor, const FGameplayTagContainer& GameplayCueTags, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
#if WITH_EDITOR
	if (GIsEditor && TargetActor == nullptr && UGameplayCueManager::PreviewComponent)
	{
		TargetActor = Cast<AActor>(AActor::StaticClass()->GetDefaultObject());
	}
#endif

	if (ShouldSuppressGameplayCues(TargetActor))
	{
		return;
	}

	for (auto It = GameplayCueTags.CreateConstIterator(); It; ++It)
	{
		HandleGameplayCue(TargetActor, *It, EventType, Parameters);
	}
}

void UGameplayCueManager::HandleGameplayCue(AActor* TargetActor, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
#if WITH_EDITOR
	if (GIsEditor && TargetActor == nullptr && UGameplayCueManager::PreviewComponent)
	{
		TargetActor = Cast<AActor>(AActor::StaticClass()->GetDefaultObject());
	}
#endif

	if (ShouldSuppressGameplayCues(TargetActor))
	{
		return;
	}

	TranslateGameplayCue(GameplayCueTag, TargetActor, Parameters);

	RouteGameplayCue(TargetActor, GameplayCueTag, EventType, Parameters);
}

bool UGameplayCueManager::ShouldSuppressGameplayCues(AActor* TargetActor)
{
	if (DisableGameplayCues)
	{
		return true;
	}

	if (GameplayCueRunOnDedicatedServer == 0 && IsDedicatedServerForGameplayCue())
	{
		return true;
	}

	if (TargetActor == nullptr)
	{
		return true;
	}

	return false;
}

void UGameplayCueManager::RouteGameplayCue(AActor* TargetActor, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	IGameplayCueInterface* GameplayCueInterface = Cast<IGameplayCueInterface>(TargetActor);
	bool bAcceptsCue = true;
	if (GameplayCueInterface)
	{
		bAcceptsCue = GameplayCueInterface->ShouldAcceptGameplayCue(TargetActor, GameplayCueTag, EventType, Parameters);
	}

#if !UE_BUILD_SHIPPING
	if (OnRouteGameplayCue.IsBound())
	{
		OnRouteGameplayCue.Broadcast(TargetActor, GameplayCueTag, EventType, Parameters);
	}
#endif // !UE_BUILD_SHIPPING

#if ENABLE_DRAW_DEBUG
	if (DisplayGameplayCues)
	{
		FString DebugStr = FString::Printf(TEXT("%s - %s"), *GameplayCueTag.ToString(), *EGameplayCueEventToString(EventType) );
		FColor DebugColor = FColor::Green;
		DrawDebugString(TargetActor->GetWorld(), FVector(0.f, 0.f, 100.f), DebugStr, TargetActor, DebugColor, DisplayGameplayCueDuration);
	}
#endif // ENABLE_DRAW_DEBUG

	CurrentWorld = TargetActor->GetWorld();

	// Don't handle gameplay cues when world is tearing down
	if (!GetWorld() || GetWorld()->bIsTearingDown)
	{
		return;
	}

	// Give the global set a chance
	check(RuntimeGameplayCueObjectLibrary.CueSet);
	if (bAcceptsCue)
	{
		RuntimeGameplayCueObjectLibrary.CueSet->HandleGameplayCue(TargetActor, GameplayCueTag, EventType, Parameters);
	}

	// Use the interface even if it's not in the map
	if (GameplayCueInterface && bAcceptsCue)
	{
		GameplayCueInterface->HandleGameplayCue(TargetActor, GameplayCueTag, EventType, Parameters);
	}

	CurrentWorld = nullptr;
}

void UGameplayCueManager::TranslateGameplayCue(FGameplayTag& Tag, AActor* TargetActor, const FGameplayCueParameters& Parameters)
{
	TranslationManager.TranslateTag(Tag, TargetActor, Parameters);
}

void UGameplayCueManager::EndGameplayCuesFor(AActor* TargetActor)
{
	for (auto It = NotifyMapActor.CreateIterator(); It; ++It)
	{
		FGCNotifyActorKey& Key = It.Key();
		if (Key.TargetActor == TargetActor)
		{
			AGameplayCueNotify_Actor* InstancedCue = It.Value().Get();
			if (InstancedCue)
			{
				InstancedCue->OnOwnerDestroyed(TargetActor);
			}
			It.RemoveCurrent();
		}
	}
}

int32 GameplayCueActorRecycle = 1;
static FAutoConsoleVariableRef CVarGameplayCueActorRecycle(TEXT("AbilitySystem.GameplayCueActorRecycle"), GameplayCueActorRecycle, TEXT("Allow recycling of GameplayCue Actors"), ECVF_Default );

int32 GameplayCueActorRecycleDebug = 0;
static FAutoConsoleVariableRef CVarGameplayCueActorRecycleDebug(TEXT("AbilitySystem.GameplayCueActorRecycleDebug"), GameplayCueActorRecycleDebug, TEXT("Prints logs for GC actor recycling debugging"), ECVF_Default );

bool UGameplayCueManager::IsGameplayCueRecylingEnabled()
{
	return GameplayCueActorRecycle > 0;
}

bool UGameplayCueManager::ShouldSyncLoadMissingGameplayCues() const
{
	return false;
}

bool UGameplayCueManager::ShouldAsyncLoadMissingGameplayCues() const
{
	return true;
}

bool UGameplayCueManager::HandleMissingGameplayCue(UGameplayCueSet* OwningSet, struct FGameplayCueNotifyData& CueData, AActor* TargetActor, EGameplayCueEvent::Type EventType, FGameplayCueParameters& Parameters)
{
	if (ShouldSyncLoadMissingGameplayCues())
	{
		CueData.LoadedGameplayCueClass = Cast<UClass>(StreamableManager.LoadSynchronous(CueData.GameplayCueNotifyObj, false));

		if (CueData.LoadedGameplayCueClass)
		{
			ABILITY_LOG(Display, TEXT("GameplayCueNotify %s was not loaded when GameplayCue was invoked, did synchronous load."), *CueData.GameplayCueNotifyObj.ToString());
			return true;
		}
		else
		{
			ABILITY_LOG(Warning, TEXT("Late load of GameplayCueNotify %s failed!"), *CueData.GameplayCueNotifyObj.ToString());
		}
	}
	else if (ShouldAsyncLoadMissingGameplayCues())
	{
		// Not loaded: start async loading and call when loaded
		StreamableManager.RequestAsyncLoad(CueData.GameplayCueNotifyObj, FStreamableDelegate::CreateUObject(this, &UGameplayCueManager::OnMissingCueAsyncLoadComplete, 
			CueData.GameplayCueNotifyObj, TWeakObjectPtr<UGameplayCueSet>(OwningSet), CueData.GameplayCueTag, TWeakObjectPtr<AActor>(TargetActor), EventType, Parameters));

		ABILITY_LOG(Display, TEXT("GameplayCueNotify %s was not loaded when GameplayCue was invoked. Starting async loading."), *CueData.GameplayCueNotifyObj.ToString());
	}
	return false;
}

void UGameplayCueManager::OnMissingCueAsyncLoadComplete(FSoftObjectPath LoadedObject, TWeakObjectPtr<UGameplayCueSet> OwningSet, FGameplayTag GameplayCueTag, TWeakObjectPtr<AActor> TargetActor, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	if (!LoadedObject.ResolveObject())
	{
		// Load failed
		ABILITY_LOG(Warning, TEXT("Late load of GameplayCueNotify %s failed!"), *LoadedObject.ToString());
		return;
	}

	if (OwningSet.IsValid() && TargetActor.IsValid())
	{
		CurrentWorld = TargetActor->GetWorld();

		// Don't handle gameplay cues when world is tearing down
		if (!GetWorld() || GetWorld()->bIsTearingDown)
		{
			return;
		}

		// Objects are still valid, re-execute cue
		OwningSet->HandleGameplayCue(TargetActor.Get(), GameplayCueTag, EventType, Parameters);

		CurrentWorld = nullptr;
	}
}

AGameplayCueNotify_Actor* UGameplayCueManager::GetInstancedCueActor(AActor* TargetActor, UClass* CueClass, const FGameplayCueParameters& Parameters)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_GameplayCueManager_GetInstancedCueActor);


	// First, see if this actor already have a GameplayCueNotifyActor already going for this CueClass
	AGameplayCueNotify_Actor* CDO = Cast<AGameplayCueNotify_Actor>(CueClass->ClassDefaultObject);
	FGCNotifyActorKey	NotifyKey(TargetActor, CueClass, 
							CDO->bUniqueInstancePerInstigator ? Parameters.GetInstigator() : nullptr, 
							CDO->bUniqueInstancePerSourceObject ? Parameters.GetSourceObject() : nullptr);

	AGameplayCueNotify_Actor* SpawnedCue = nullptr;
	if (TWeakObjectPtr<AGameplayCueNotify_Actor>* WeakPtrPtr = NotifyMapActor.Find(NotifyKey))
	{		
		SpawnedCue = WeakPtrPtr->Get();
		// If the cue is scheduled to be destroyed, don't reuse it, create a new one instead
		if (SpawnedCue && SpawnedCue->GameplayCuePendingRemove() == false)
		{
			if (SpawnedCue->GetOwner() != TargetActor)
			{
#if WITH_EDITOR	
				if (TargetActor && TargetActor->HasAnyFlags(RF_ClassDefaultObject))
				{
					// Animation preview hack, reuse this one even though the owner doesnt match the CDO
					return SpawnedCue;
				}
#endif

				// This should not happen. This means we think we can recycle and GC actor that is currently being used by someone else.
				ABILITY_LOG(Warning, TEXT("GetInstancedCueActor attempting to reuse GC Actor with a different owner! %s (Target: %s). Using GC Actor: %s. Current Owner: %s"), *GetNameSafe(CueClass), *GetNameSafe(TargetActor), *GetNameSafe(SpawnedCue), *GetNameSafe(SpawnedCue->GetOwner()));
			}
			else
			{
				UE_CLOG((GameplayCueActorRecycleDebug>0), LogAbilitySystem, Display, TEXT("::GetInstancedCueActor Using Existing %s (Target: %s). Using GC Actor: %s"), *GetNameSafe(CueClass), *GetNameSafe(TargetActor), *GetNameSafe(SpawnedCue));
				return SpawnedCue;
			}
		}

		// We aren't going to use this existing cue notify actor, so clear it.
		SpawnedCue = nullptr;
	}

	UWorld* World = GetWorld();

	// We don't have an instance for this, and we need one, so make one
	if (ensure(TargetActor) && ensure(CueClass) && ensure(World))
	{
		AActor* NewOwnerActor = TargetActor;
		bool UseActorRecycling = (GameplayCueActorRecycle > 0);
		
#if WITH_EDITOR	
		// Animtion preview hack. If we are trying to play the GC on a CDO, then don't use actor recycling and don't set the owner (to the CDO, which would cause problems)
		if (TargetActor && TargetActor->HasAnyFlags(RF_ClassDefaultObject))
		{
			NewOwnerActor = nullptr;
			UseActorRecycling = false;
		}
#endif
		// Look to reuse an existing one that is stored on the CDO:
		if (UseActorRecycling)
		{
			FPreallocationInfo& Info = GetPreallocationInfo(World);
			TArray<AGameplayCueNotify_Actor*>* PreallocatedList = Info.PreallocatedInstances.Find(CueClass);
			if (PreallocatedList && PreallocatedList->Num() > 0)
			{
				SpawnedCue = nullptr;
				while (true)
				{
					SpawnedCue = PreallocatedList->Pop(false);

					// Temp: tracking down possible memory corruption
					// null is maybe ok. But invalid low level is bad and we want to crash hard to find out who/why.
					if (SpawnedCue && (SpawnedCue->IsValidLowLevelFast() == false))
					{
						checkf(false, TEXT("UGameplayCueManager::GetInstancedCueActor found an invalid SpawnedCue for class %s"), *GetNameSafe(CueClass));
					}

					// Normal check: if cue was destroyed or is pending kill, then don't use it.
					if (SpawnedCue && SpawnedCue->IsPendingKill() == false)
					{
						break;
					}
					
					// outside of replays, this should not happen. GC Notifies should not be actually destroyed.
					checkf(World->DemoNetDriver, TEXT("Spawned Cue is pending kill or null: %s."), *GetNameSafe(SpawnedCue));

					if (PreallocatedList->Num() <= 0)
					{
						// Ran out of preallocated instances... break and create a new one.
						break;
					}
				}

				if (SpawnedCue)
				{
					SpawnedCue->bInRecycleQueue = false;
					SpawnedCue->SetOwner(NewOwnerActor);
					SpawnedCue->SetActorLocationAndRotation(TargetActor->GetActorLocation(), TargetActor->GetActorRotation());
					SpawnedCue->ReuseAfterRecycle();
				}

				UE_CLOG((GameplayCueActorRecycleDebug>0), LogAbilitySystem, Display, TEXT("GetInstancedCueActor Popping Recycled %s (Target: %s). Using GC Actor: %s"), *GetNameSafe(CueClass), *GetNameSafe(TargetActor), *GetNameSafe(SpawnedCue));
#if WITH_EDITOR
				// let things know that we 'spawned'
				ISequenceRecorder& SequenceRecorder	= FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
				SequenceRecorder.NotifyActorStartRecording(SpawnedCue);
#endif
			}
		}

		// If we can't reuse, then spawn a new one
		if (SpawnedCue == nullptr)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = NewOwnerActor;
			if (LogGameplayCueActorSpawning)
			{
				ABILITY_LOG(Warning, TEXT("Spawning GameplaycueActor: %s"), *CueClass->GetName());
			}

			SpawnedCue = World->SpawnActor<AGameplayCueNotify_Actor>(CueClass, TargetActor->GetActorLocation(), TargetActor->GetActorRotation(), SpawnParams);
		}

		// Associate this GameplayCueNotifyActor with this target actor/key
		if (ensure(SpawnedCue))
		{
			SpawnedCue->NotifyKey = NotifyKey;
			NotifyMapActor.Add(NotifyKey, SpawnedCue);
		}
	}

	UE_CLOG((GameplayCueActorRecycleDebug>0), LogAbilitySystem, Display, TEXT("GetInstancedCueActor  Returning %s (Target: %s). Using GC Actor: %s"), *GetNameSafe(CueClass), *GetNameSafe(TargetActor), *GetNameSafe(SpawnedCue));
	return SpawnedCue;
}

void UGameplayCueManager::NotifyGameplayCueActorFinished(AGameplayCueNotify_Actor* Actor)
{
	bool UseActorRecycling = (GameplayCueActorRecycle > 0);

#if WITH_EDITOR	
	// Don't recycle in preview worlds
	if (Actor->GetWorld()->IsPreviewWorld())
	{
		UseActorRecycling = false;
	}
#endif

	if (UseActorRecycling)
	{
		if (Actor->bInRecycleQueue)
		{
			// We are already in the recycle queue. This can happen normally
			// (For example the GC is removed and the owner is destroyed in the same frame)
			return;
		}

		AGameplayCueNotify_Actor* CDO = Actor->GetClass()->GetDefaultObject<AGameplayCueNotify_Actor>();
		if (CDO && Actor->Recycle())
		{
			if (Actor->IsPendingKill())
			{
				ensureMsgf(GetWorld()->DemoNetDriver, TEXT("GameplayCueNotify %s is pending kill in ::NotifyGameplayCueActorFinished (and not in network demo)"), *GetNameSafe(Actor));
				return;
			}
			Actor->bInRecycleQueue = true;

			// Remove this now from our internal map so that it doesn't get reused like a currently active cue would
			if (TWeakObjectPtr<AGameplayCueNotify_Actor>* WeakPtrPtr = NotifyMapActor.Find(Actor->NotifyKey))
			{
				// Only remove if this is the current actor in the map!
				// This could happen if a GC notify actor has a delayed removal and another GC event happens before the delayed removal happens (the old GC actor could replace the latest one in the map)
				if (WeakPtrPtr->Get() == Actor)
				{
					WeakPtrPtr->Reset();
				}
			}

			UE_CLOG((GameplayCueActorRecycleDebug>0), LogAbilitySystem, Display, TEXT("NotifyGameplayCueActorFinished %s"), *GetNameSafe(Actor));

			FPreallocationInfo& Info = GetPreallocationInfo(Actor->GetWorld());
			TArray<AGameplayCueNotify_Actor*>& PreAllocatedList = Info.PreallocatedInstances.FindOrAdd(Actor->GetClass());

			// Put the actor back in the list
			if (ensureMsgf(PreAllocatedList.Contains(Actor)==false, TEXT("GC Actor PreallocationList already contains Actor %s"), *GetNameSafe(Actor)))
			{
				PreAllocatedList.Push(Actor);
			}
			
#if WITH_EDITOR
			// let things know that we 'de-spawned'
			ISequenceRecorder& SequenceRecorder	= FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
			SequenceRecorder.NotifyActorStopRecording(Actor);
#endif
			return;
		}
	}	

	// We didn't recycle, so just destroy
	Actor->Destroy();
}

void UGameplayCueManager::NotifyGameplayCueActorEndPlay(AGameplayCueNotify_Actor* Actor)
{
	if (Actor && Actor->bInRecycleQueue)
	{
		FPreallocationInfo& Info = GetPreallocationInfo(Actor->GetWorld());
		TArray<AGameplayCueNotify_Actor*>& PreAllocatedList = Info.PreallocatedInstances.FindOrAdd(Actor->GetClass());
		PreAllocatedList.Remove(Actor);
	}
}

// ------------------------------------------------------------------------

bool UGameplayCueManager::ShouldSyncScanRuntimeObjectLibraries() const
{
	// Always sync scan the runtime object library
	return true;
}
bool UGameplayCueManager::ShouldSyncLoadRuntimeObjectLibraries() const
{
	// No real need to sync load it anymore
	return false;
}
bool UGameplayCueManager::ShouldAsyncLoadRuntimeObjectLibraries() const
{
	// Async load the run time library at startup
	return true;
}

void UGameplayCueManager::InitializeRuntimeObjectLibrary()
{
	RuntimeGameplayCueObjectLibrary.Paths = GetAlwaysLoadedGameplayCuePaths();
	if (RuntimeGameplayCueObjectLibrary.CueSet == nullptr)
	{
		RuntimeGameplayCueObjectLibrary.CueSet = NewObject<UGameplayCueSet>(this, TEXT("GlobalGameplayCueSet"));
	}

	RuntimeGameplayCueObjectLibrary.CueSet->Empty();
	RuntimeGameplayCueObjectLibrary.bHasBeenInitialized = true;
	
	RuntimeGameplayCueObjectLibrary.bShouldSyncScan = ShouldSyncScanRuntimeObjectLibraries();
	RuntimeGameplayCueObjectLibrary.bShouldSyncLoad = ShouldSyncLoadRuntimeObjectLibraries();
	RuntimeGameplayCueObjectLibrary.bShouldAsyncLoad = ShouldAsyncLoadRuntimeObjectLibraries();

	InitObjectLibrary(RuntimeGameplayCueObjectLibrary);
}

#if WITH_EDITOR
void UGameplayCueManager::InitializeEditorObjectLibrary()
{
	SCOPE_LOG_TIME_IN_SECONDS(*FString::Printf(TEXT("UGameplayCueManager::InitializeEditorObjectLibrary")), nullptr)

	EditorGameplayCueObjectLibrary.Paths = GetValidGameplayCuePaths();
	if (EditorGameplayCueObjectLibrary.CueSet == nullptr)
	{
		EditorGameplayCueObjectLibrary.CueSet = NewObject<UGameplayCueSet>(this, TEXT("EditorGameplayCueSet"));
	}

	EditorGameplayCueObjectLibrary.CueSet->Empty();
	EditorGameplayCueObjectLibrary.bHasBeenInitialized = true;

	// Don't load anything for the editor. Just read whatever the asset registry has.
	EditorGameplayCueObjectLibrary.bShouldSyncScan = IsRunningCommandlet();				// If we are cooking, then sync scan it right away so that we don't miss anything
	EditorGameplayCueObjectLibrary.bShouldAsyncLoad = false;
	EditorGameplayCueObjectLibrary.bShouldSyncLoad = false;

	InitObjectLibrary(EditorGameplayCueObjectLibrary);
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	if ( AssetRegistryModule.Get().IsLoadingAssets() )
	{
		// Let us know when we are done
		static FDelegateHandle DoOnce =
		AssetRegistryModule.Get().OnFilesLoaded().AddUObject(this, &UGameplayCueManager::InitializeEditorObjectLibrary);
	}
	else
	{
		EditorObjectLibraryFullyInitialized = true;
		if (EditorPeriodicUpdateHandle.IsValid())
		{
			GEditor->GetTimerManager()->ClearTimer(EditorPeriodicUpdateHandle);
			EditorPeriodicUpdateHandle.Invalidate();
		}
	}

	OnEditorObjectLibraryUpdated.Broadcast();
}

void UGameplayCueManager::RequestPeriodicUpdateOfEditorObjectLibraryWhileWaitingOnAssetRegistry()
{
	// Asset registry is still loading, so update every 15 seconds until its finished
	if (!EditorObjectLibraryFullyInitialized && !EditorPeriodicUpdateHandle.IsValid())
	{
		GEditor->GetTimerManager()->SetTimer( EditorPeriodicUpdateHandle, FTimerDelegate::CreateUObject(this, &UGameplayCueManager::InitializeEditorObjectLibrary), 15.f, true);
	}
}

void UGameplayCueManager::ReloadObjectLibrary(UWorld* World, const UWorld::InitializationValues IVS)
{
	if (bAccelerationMapOutdated)
	{
		RefreshObjectLibraries();
	}
}

void UGameplayCueManager::GetEditorObjectLibraryGameplayCueNotifyFilenames(TArray<FString>& Filenames) const
{
	if (ensure(EditorGameplayCueObjectLibrary.CueSet))
	{
		EditorGameplayCueObjectLibrary.CueSet->GetFilenames(Filenames);
	}
}

void UGameplayCueManager::LoadNotifyForEditorPreview(FGameplayTag GameplayCueTag)
{
	if (ensure(EditorGameplayCueObjectLibrary.CueSet) && ensure(RuntimeGameplayCueObjectLibrary.CueSet))
	{
		EditorGameplayCueObjectLibrary.CueSet->CopyCueDataToSetForEditorPreview(GameplayCueTag, RuntimeGameplayCueObjectLibrary.CueSet);
	}
}

#endif // WITH_EDITOR

TArray<FString> UGameplayCueManager::GetAlwaysLoadedGameplayCuePaths()
{
	return UAbilitySystemGlobals::Get().GetGameplayCueNotifyPaths();
}

void UGameplayCueManager::RefreshObjectLibraries()
{
	if (RuntimeGameplayCueObjectLibrary.bHasBeenInitialized)
	{
		check(RuntimeGameplayCueObjectLibrary.CueSet);
		RuntimeGameplayCueObjectLibrary.CueSet->Empty();
		InitObjectLibrary(RuntimeGameplayCueObjectLibrary);
	}

	if (EditorGameplayCueObjectLibrary.bHasBeenInitialized)
	{
		check(EditorGameplayCueObjectLibrary.CueSet);
		EditorGameplayCueObjectLibrary.CueSet->Empty();
		InitObjectLibrary(EditorGameplayCueObjectLibrary);
	}
}

static void SearchDynamicClassCues(const FName PropertyName, const TArray<FString>& Paths, TArray<FGameplayCueReferencePair>& CuesToAdd, TArray<FSoftObjectPath>& AssetsToLoad)
{
	// Iterate over all Dynamic Classes (nativized Blueprints). Search for ones with GameplayCueName tag.

	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	TMap<FName, FDynamicClassStaticData>& DynamicClassMap = GetDynamicClassMap();
	for (auto PairIter : DynamicClassMap)
	{
		const FName* FoundGameplayTag = PairIter.Value.SelectedSearchableValues.Find(PropertyName);
		if (!FoundGameplayTag)
		{
			continue;
		}

		const FString ClassPath = PairIter.Key.ToString();
		for (const FString& Path : Paths)
		{
			const bool PathContainsClass = ClassPath.StartsWith(Path); // TODO: is it enough?
			if (!PathContainsClass)
			{
				continue;
			}

			ABILITY_LOG(Log, TEXT("GameplayCueManager Found a Dynamic Class: %s / %s"), *FoundGameplayTag->ToString(), *ClassPath);

			FGameplayTag GameplayCueTag = Manager.RequestGameplayTag(*FoundGameplayTag, false);
			if (GameplayCueTag.IsValid())
			{
				FSoftObjectPath StringRef(ClassPath); // TODO: is there any translation needed?
				ensure(StringRef.IsValid());

				CuesToAdd.Add(FGameplayCueReferencePair(GameplayCueTag, StringRef));
				AssetsToLoad.Add(StringRef);
			}
			else
			{
				ABILITY_LOG(Warning, TEXT("Found GameplayCue tag %s in Dynamic Class %s but there is no corresponding tag in the GameplayTagManager."), *FoundGameplayTag->ToString(), *ClassPath);
			}

			break;
		}
	}
}

void UGameplayCueManager::InitObjectLibrary(FGameplayCueObjectLibrary& Lib)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Loading Library"), STAT_ObjectLibrary, STATGROUP_LoadTime);

	// Instantiate the UObjectLibraries if they aren't there already
	if (!Lib.StaticObjectLibrary)
	{
		Lib.StaticObjectLibrary = UObjectLibrary::CreateLibrary(AGameplayCueNotify_Actor::StaticClass(), true, GIsEditor && !IsRunningCommandlet());
		if (GIsEditor)
		{
			Lib.StaticObjectLibrary->bIncludeOnlyOnDiskAssets = false;
		}
	}
	if (!Lib.ActorObjectLibrary)
	{
		Lib.ActorObjectLibrary = UObjectLibrary::CreateLibrary(UGameplayCueNotify_Static::StaticClass(), true, GIsEditor && !IsRunningCommandlet());
		if (GIsEditor)
		{
			Lib.ActorObjectLibrary->bIncludeOnlyOnDiskAssets = false;
		}
	}	

	Lib.bHasBeenInitialized = true;

#if WITH_EDITOR
	bAccelerationMapOutdated = false;
#endif

	FScopeCycleCounterUObject PreloadScopeActor(Lib.ActorObjectLibrary);

	// ------------------------------------------------------------------------------------------------------------------
	//	Scan asset data. If bShouldSyncScan is false, whatever state the asset registry is in will be what is returned.
	// ------------------------------------------------------------------------------------------------------------------

	{
		//SCOPE_LOG_TIME_IN_SECONDS(*FString::Printf(TEXT("UGameplayCueManager::InitObjectLibraries    Actors. Paths: %s"), *FString::Join(Lib.Paths, TEXT(", "))), nullptr)
		Lib.ActorObjectLibrary->LoadBlueprintAssetDataFromPaths(Lib.Paths, Lib.bShouldSyncScan);
	}
	{
		//SCOPE_LOG_TIME_IN_SECONDS(*FString::Printf(TEXT("UGameplayCueManager::InitObjectLibraries    Objects")), nullptr)
		Lib.StaticObjectLibrary->LoadBlueprintAssetDataFromPaths(Lib.Paths, Lib.bShouldSyncScan);
	}

	// ---------------------------------------------------------
	// Sync load if told to do so	
	// ---------------------------------------------------------
	if (Lib.bShouldSyncLoad)
	{
#if STATS
		FString PerfMessage = FString::Printf(TEXT("Fully Loaded GameplayCueNotify object library"));
		SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
		Lib.ActorObjectLibrary->LoadAssetsFromAssetData();
		Lib.StaticObjectLibrary->LoadAssetsFromAssetData();
	}

	// ---------------------------------------------------------
	// Look for GameplayCueNotifies that handle events
	// ---------------------------------------------------------
	
	TArray<FAssetData> ActorAssetDatas;
	Lib.ActorObjectLibrary->GetAssetDataList(ActorAssetDatas);

	TArray<FAssetData> StaticAssetDatas;
	Lib.StaticObjectLibrary->GetAssetDataList(StaticAssetDatas);

	TArray<FGameplayCueReferencePair> CuesToAdd;
	TArray<FSoftObjectPath> AssetsToLoad;

	// ------------------------------------------------------------------------------------------------------------------
	// Build Cue lists for loading. Determines what from the obj library needs to be loaded
	// ------------------------------------------------------------------------------------------------------------------
	BuildCuesToAddToGlobalSet(ActorAssetDatas, GET_MEMBER_NAME_CHECKED(AGameplayCueNotify_Actor, GameplayCueName), CuesToAdd, AssetsToLoad, Lib.ShouldLoad);
	BuildCuesToAddToGlobalSet(StaticAssetDatas, GET_MEMBER_NAME_CHECKED(UGameplayCueNotify_Static, GameplayCueName), CuesToAdd, AssetsToLoad, Lib.ShouldLoad);

	const FName PropertyName = GET_MEMBER_NAME_CHECKED(AGameplayCueNotify_Actor, GameplayCueName);
	check(PropertyName == GET_MEMBER_NAME_CHECKED(UGameplayCueNotify_Static, GameplayCueName));
	SearchDynamicClassCues(PropertyName, Lib.Paths, CuesToAdd, AssetsToLoad);

	// ------------------------------------------------------------------------------------------------------------------------------------
	// Add these cues to the set. The UGameplayCueSet is the data structure used in routing the gameplay cue events at runtime.
	// ------------------------------------------------------------------------------------------------------------------------------------
	UGameplayCueSet* SetToAddTo = Lib.CueSet;
	if (!SetToAddTo)
	{
		SetToAddTo = RuntimeGameplayCueObjectLibrary.CueSet;
	}
	check(SetToAddTo);
	SetToAddTo->AddCues(CuesToAdd);

	// --------------------------------------------
	// Start loading them if necessary
	// --------------------------------------------
	if (Lib.bShouldAsyncLoad)
	{
		auto ForwardLambda = [](TArray<FSoftObjectPath> AssetList, FOnGameplayCueNotifySetLoaded OnLoadedDelegate)
		{
			OnLoadedDelegate.ExecuteIfBound(AssetList);
		};

		if (AssetsToLoad.Num() > 0)
		{
			GameplayCueAssetHandle = StreamableManager.RequestAsyncLoad(AssetsToLoad, FStreamableDelegate::CreateStatic( ForwardLambda, AssetsToLoad, Lib.OnLoaded), Lib.AsyncPriority);
		}
		else
		{
			// Still fire the delegate even if nothing was found to load
			Lib.OnLoaded.ExecuteIfBound(AssetsToLoad);
		}
	}

	// Build Tag Translation table
	TranslationManager.BuildTagTranslationTable();
}

static FAutoConsoleVariable CVarGameplyCueAddToGlobalSetDebug(TEXT("GameplayCue.AddToGlobalSet.DebugTag"), TEXT(""), TEXT("Debug Tag adding to global set"), ECVF_Default	);

void UGameplayCueManager::BuildCuesToAddToGlobalSet(const TArray<FAssetData>& AssetDataList, FName TagPropertyName, TArray<FGameplayCueReferencePair>& OutCuesToAdd, TArray<FSoftObjectPath>& OutAssetsToLoad, FShouldLoadGCNotifyDelegate ShouldLoad)
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	OutAssetsToLoad.Reserve(OutAssetsToLoad.Num() + AssetDataList.Num());

	for (const FAssetData& Data: AssetDataList)
	{
		const FName FoundGameplayTag = Data.GetTagValueRef<FName>(TagPropertyName);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (CVarGameplyCueAddToGlobalSetDebug->GetString().IsEmpty() == false && FoundGameplayTag.ToString().Contains(CVarGameplyCueAddToGlobalSetDebug->GetString()))
		{
			ABILITY_LOG(Display, TEXT("Adding Tag %s to GlobalSet"), *FoundGameplayTag.ToString());
		}
#endif

		// If ShouldLoad delegate is bound and it returns false, don't load this one
		if (ShouldLoad.IsBound() && (ShouldLoad.Execute(Data, FoundGameplayTag) == false))
		{
			continue;
		}
		
		if (ShouldLoadGameplayCueAssetData(Data) == false)
		{
			continue;
		}
		
		if (!FoundGameplayTag.IsNone())
		{
			const FString GeneratedClassTag = Data.GetTagValueRef<FString>("GeneratedClass");
			if (GeneratedClassTag.IsEmpty())
			{
				ABILITY_LOG(Warning, TEXT("Unable to find GeneratedClass value for AssetData %s"), *Data.ObjectPath.ToString());
				continue;
			}

			ABILITY_LOG(Log, TEXT("GameplayCueManager Found: %s / %s"), *FoundGameplayTag.ToString(), **GeneratedClassTag);

			FGameplayTag  GameplayCueTag = Manager.RequestGameplayTag(FoundGameplayTag, false);
			if (GameplayCueTag.IsValid())
			{
				// Add a new NotifyData entry to our flat list for this one
				FSoftObjectPath StringRef;
				StringRef.SetPath(FPackageName::ExportTextPathToObjectPath(*GeneratedClassTag));

				OutCuesToAdd.Add(FGameplayCueReferencePair(GameplayCueTag, StringRef));

				OutAssetsToLoad.Add(StringRef);
			}
			else
			{
				// Warn about this tag but only once to cut down on spam (we may build cue sets multiple times in the editor)
				static TSet<FName> WarnedTags;
				if (WarnedTags.Contains(FoundGameplayTag) == false)
				{
					ABILITY_LOG(Warning, TEXT("Found GameplayCue tag %s in asset %s but there is no corresponding tag in the GameplayTagManager."), *FoundGameplayTag.ToString(), *Data.PackageName.ToString());
					WarnedTags.Add(FoundGameplayTag);
				}
			}
		}
	}
}

int32 GameplayCueCheckForTooManyRPCs = 1;
static FAutoConsoleVariableRef CVarGameplayCueCheckForTooManyRPCs(TEXT("AbilitySystem.GameplayCueCheckForTooManyRPCs"), GameplayCueCheckForTooManyRPCs, TEXT("Warns if gameplay cues are being throttled by network code"), ECVF_Default );

void UGameplayCueManager::CheckForTooManyRPCs(FName FuncName, const FGameplayCuePendingExecute& PendingCue, const FString& CueID, const FGameplayEffectContext* EffectContext)
{
	if (GameplayCueCheckForTooManyRPCs)
	{
		static IConsoleVariable* MaxRPCPerNetUpdateCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("net.MaxRPCPerNetUpdate"));
		if (MaxRPCPerNetUpdateCVar)
		{
			AActor* Owner = PendingCue.OwningComponent ? PendingCue.OwningComponent->GetOwner() : nullptr;
			UWorld* World = Owner ? Owner->GetWorld() : nullptr;
			UNetDriver* NetDriver = World ? World->GetNetDriver() : nullptr;
			if (NetDriver)
			{
				const int32 MaxRPCs = MaxRPCPerNetUpdateCVar->GetInt();
				for (UNetConnection* ClientConnection : NetDriver->ClientConnections)
				{
					if (ClientConnection)
					{
						UActorChannel** OwningActorChannelPtr = ClientConnection->ActorChannels.Find(Owner);
						TSharedRef<FObjectReplicator>* ComponentReplicatorPtr = (OwningActorChannelPtr && *OwningActorChannelPtr) ? (*OwningActorChannelPtr)->ReplicationMap.Find(PendingCue.OwningComponent) : nullptr;
						if (ComponentReplicatorPtr)
						{
							const TArray<FObjectReplicator::FRPCCallInfo>& RemoteFuncInfo = (*ComponentReplicatorPtr)->RemoteFuncInfo;
							for (const FObjectReplicator::FRPCCallInfo& CallInfo : RemoteFuncInfo)
							{
								if (CallInfo.FuncName == FuncName)
								{
									if (CallInfo.Calls > MaxRPCs)
									{
										const FString Instigator = EffectContext ? EffectContext->ToString() : TEXT("None");
										ABILITY_LOG(Warning, TEXT("Attempted to fire %s when no more RPCs are allowed this net update. Max:%d Cue:%s Instigator:%s Component:%s"), *FuncName.ToString(), MaxRPCs, *CueID, *Instigator, *GetPathNameSafe(PendingCue.OwningComponent));
									
										// Returning here to only log once per offending RPC.
										return;
									}

									break;
								}
							}
						}
					}
				}
			}
		}
	}
}

void UGameplayCueManager::OnGameplayCueNotifyAsyncLoadComplete(TArray<FSoftObjectPath> AssetList)
{
	for (FSoftObjectPath StringRef : AssetList)
	{
		UClass* GCClass = FindObject<UClass>(nullptr, *StringRef.ToString());
		if (ensure(GCClass))
		{
			LoadedGameplayCueNotifyClasses.Add(GCClass);
			CheckForPreallocation(GCClass);
		}
	}
}

int32 UGameplayCueManager::FinishLoadingGameplayCueNotifies()
{
	int32 NumLoadeded = 0;
	return NumLoadeded;
}

UGameplayCueSet* UGameplayCueManager::GetRuntimeCueSet()
{
	return RuntimeGameplayCueObjectLibrary.CueSet;
}

TArray<UGameplayCueSet*> UGameplayCueManager::GetGlobalCueSets()
{
	TArray<UGameplayCueSet*> Set;
	if (RuntimeGameplayCueObjectLibrary.CueSet)
	{
		Set.Add(RuntimeGameplayCueObjectLibrary.CueSet);
	}
	if (EditorGameplayCueObjectLibrary.CueSet)
	{
		Set.Add(EditorGameplayCueObjectLibrary.CueSet);
	}
	return Set;
}

#if WITH_EDITOR

UGameplayCueSet* UGameplayCueManager::GetEditorCueSet()
{
	return EditorGameplayCueObjectLibrary.CueSet;
}

void UGameplayCueManager::HandleAssetAdded(UObject *Object)
{
	UBlueprint* Blueprint = Cast<UBlueprint>(Object);
	if (Blueprint && Blueprint->GeneratedClass)
	{
		UGameplayCueNotify_Static* StaticCDO = Cast<UGameplayCueNotify_Static>(Blueprint->GeneratedClass->ClassDefaultObject);
		AGameplayCueNotify_Actor* ActorCDO = Cast<AGameplayCueNotify_Actor>(Blueprint->GeneratedClass->ClassDefaultObject);
		
		if (StaticCDO || ActorCDO)
		{
			if (VerifyNotifyAssetIsInValidPath(Blueprint->GetOuter()->GetPathName()))
			{
				FSoftObjectPath StringRef;
				StringRef.SetPath(Blueprint->GeneratedClass->GetPathName());

				TArray<FGameplayCueReferencePair> CuesToAdd;
				if (StaticCDO)
				{
					CuesToAdd.Add(FGameplayCueReferencePair(StaticCDO->GameplayCueTag, StringRef));
				}
				else if (ActorCDO)
				{
					CuesToAdd.Add(FGameplayCueReferencePair(ActorCDO->GameplayCueTag, StringRef));
				}

				for (UGameplayCueSet* Set : GetGlobalCueSets())
				{
					Set->AddCues(CuesToAdd);
				}

				OnGameplayCueNotifyAddOrRemove.Broadcast();
			}
		}
	}
}

/** Handles cleaning up an object library if it matches the passed in object */
void UGameplayCueManager::HandleAssetDeleted(UObject *Object)
{
	FSoftObjectPath StringRefToRemove;
	UBlueprint* Blueprint = Cast<UBlueprint>(Object);
	if (Blueprint && Blueprint->GeneratedClass)
	{
		UGameplayCueNotify_Static* StaticCDO = Cast<UGameplayCueNotify_Static>(Blueprint->GeneratedClass->ClassDefaultObject);
		AGameplayCueNotify_Actor* ActorCDO = Cast<AGameplayCueNotify_Actor>(Blueprint->GeneratedClass->ClassDefaultObject);
		
		if (StaticCDO || ActorCDO)
		{
			StringRefToRemove.SetPath(Blueprint->GeneratedClass->GetPathName());
		}
	}

	if (StringRefToRemove.IsValid())
	{
		TArray<FSoftObjectPath> StringRefs;
		StringRefs.Add(StringRefToRemove);
		
		
		for (UGameplayCueSet* Set : GetGlobalCueSets())
		{
			Set->RemoveCuesByStringRefs(StringRefs);
		}

		OnGameplayCueNotifyAddOrRemove.Broadcast();
	}
}

/** Handles cleaning up an object library if it matches the passed in object */
void UGameplayCueManager::HandleAssetRenamed(const FAssetData& Data, const FString& String)
{
	const FString ParentClassName = Data.GetTagValueRef<FString>("ParentClass");
	if (!ParentClassName.IsEmpty())
	{
		UClass* DataClass = FindObject<UClass>(nullptr, *ParentClassName);
		if (DataClass)
		{
			UGameplayCueNotify_Static* StaticCDO = Cast<UGameplayCueNotify_Static>(DataClass->ClassDefaultObject);
			AGameplayCueNotify_Actor* ActorCDO = Cast<AGameplayCueNotify_Actor>(DataClass->ClassDefaultObject);
			if (StaticCDO || ActorCDO)
			{
				VerifyNotifyAssetIsInValidPath(Data.PackagePath.ToString());

				for (UGameplayCueSet* Set : GetGlobalCueSets())
				{
					Set->UpdateCueByStringRefs(String + TEXT("_C"), Data.ObjectPath.ToString() + TEXT("_C"));
				}
				OnGameplayCueNotifyAddOrRemove.Broadcast();
			}
		}
	}
}

bool UGameplayCueManager::VerifyNotifyAssetIsInValidPath(FString Path)
{
	bool ValidPath = false;
	for (FString& str: GetValidGameplayCuePaths())
	{
		if (Path.Contains(str))
		{
			ValidPath = true;
		}
	}

	if (!ValidPath)
	{
		FString MessageTry = FString::Printf(TEXT("Warning: Invalid GameplayCue Path %s"));
		MessageTry += TEXT("\n\nGameplayCue Notifies should only be saved in the following folders:");

		ABILITY_LOG(Warning, TEXT("Warning: Invalid GameplayCuePath: %s"), *Path);
		ABILITY_LOG(Warning, TEXT("Valid Paths: "));
		for (FString& str: GetValidGameplayCuePaths())
		{
			ABILITY_LOG(Warning, TEXT("  %s"), *str);
			MessageTry += FString::Printf(TEXT("\n  %s"), *str);
		}

		MessageTry += FString::Printf(TEXT("\n\nThis asset must be moved to a valid location to work in game."));

		const FText MessageText = FText::FromString(MessageTry);
		const FText TitleText = NSLOCTEXT("GameplayCuePathWarning", "GameplayCuePathWarningTitle", "Invalid GameplayCue Path");
		FMessageDialog::Open(EAppMsgType::Ok, MessageText, &TitleText);
	}

	return ValidPath;
}

#endif


UWorld* UGameplayCueManager::GetWorld() const
{
	return GetCachedWorldForGameplayCueNotifies();
}

/* static */ UWorld* UGameplayCueManager::GetCachedWorldForGameplayCueNotifies()
{
#if WITH_EDITOR
	if (PreviewWorld)
		return PreviewWorld;
#endif

	return CurrentWorld;
}

void UGameplayCueManager::PrintGameplayCueNotifyMap()
{
	if (ensure(RuntimeGameplayCueObjectLibrary.CueSet))
	{
		RuntimeGameplayCueObjectLibrary.CueSet->PrintCues();
	}
}

void UGameplayCueManager::PrintLoadedGameplayCueNotifyClasses()
{
	for (UClass* NotifyClass : LoadedGameplayCueNotifyClasses)
	{
		ABILITY_LOG(Display, TEXT("%s"), *GetNameSafe(NotifyClass));
	}
	ABILITY_LOG(Display, TEXT("%d total classes"), LoadedGameplayCueNotifyClasses.Num());
}

static void	PrintGameplayCueNotifyMapConsoleCommandFunc(UWorld* InWorld)
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->PrintGameplayCueNotifyMap();
}

FAutoConsoleCommandWithWorld PrintGameplayCueNotifyMapConsoleCommand(
	TEXT("GameplayCue.PrintGameplayCueNotifyMap"),
	TEXT("Displays GameplayCue notify map"),
	FConsoleCommandWithWorldDelegate::CreateStatic(PrintGameplayCueNotifyMapConsoleCommandFunc)
	);

static void	PrintLoadedGameplayCueNotifyClasses(UWorld* InWorld)
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->PrintLoadedGameplayCueNotifyClasses();
}

FAutoConsoleCommandWithWorld PrintLoadedGameplayCueNotifyClassesCommand(
	TEXT("GameplayCue.PrintLoadedGameplayCueNotifyClasses"),
	TEXT("Displays GameplayCue Notify classes that are loaded"),
	FConsoleCommandWithWorldDelegate::CreateStatic(PrintLoadedGameplayCueNotifyClasses)
	);

FScopedGameplayCueSendContext::FScopedGameplayCueSendContext()
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->StartGameplayCueSendContext();
}
FScopedGameplayCueSendContext::~FScopedGameplayCueSendContext()
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->EndGameplayCueSendContext();
}

template<class AllocatorType>
void PullGameplayCueTagsFromSpec(const FGameplayEffectSpec& Spec, TArray<FGameplayTag, AllocatorType>& OutArray)
{
	// Add all GameplayCue Tags from the GE into the GameplayCueTags PendingCue.list
	for (const FGameplayEffectCue& EffectCue : Spec.Def->GameplayCues)
	{
		for (const FGameplayTag& Tag: EffectCue.GameplayCueTags)
		{
			if (Tag.IsValid())
			{
				OutArray.Add(Tag);
			}
		}
	}
}

/**
 *	Enabling AbilitySystemAlwaysConvertGESpecToGCParams will mean that all calls to gameplay cues with GameplayEffectSpecs will be converted into GameplayCue Parameters server side and then replicated.
 *	This potentially saved bandwidth but also has less information, depending on how the GESpec is converted to GC Parameters and what your GC's need to know.
 */

int32 AbilitySystemAlwaysConvertGESpecToGCParams = 0;
static FAutoConsoleVariableRef CVarAbilitySystemAlwaysConvertGESpecToGCParams(TEXT("AbilitySystem.AlwaysConvertGESpecToGCParams"), AbilitySystemAlwaysConvertGESpecToGCParams, TEXT("Always convert a GameplayCue from GE Spec to GC from GC Parameters on the server"), ECVF_Default );

void UGameplayCueManager::InvokeGameplayCueAddedAndWhileActive_FromSpec(UAbilitySystemComponent* OwningComponent, const FGameplayEffectSpec& Spec, FPredictionKey PredictionKey)
{
	if (Spec.Def->GameplayCues.Num() == 0)
	{
		return;
	}

	if (AbilitySystemAlwaysConvertGESpecToGCParams)
	{
		// Transform the GE Spec into GameplayCue parmameters here (on the server)

		FGameplayCueParameters Parameters;
		UAbilitySystemGlobals::Get().InitGameplayCueParameters_GESpec(Parameters, Spec);

		static TArray<FGameplayTag, TInlineAllocator<4> > Tags;
		Tags.Reset();

		PullGameplayCueTagsFromSpec(Spec, Tags);

		if (Tags.Num() == 1)
		{
			OwningComponent->NetMulticast_InvokeGameplayCueAddedAndWhileActive_WithParams(Tags[0], PredictionKey, Parameters);
			
		}
		else if (Tags.Num() > 1)
		{
			OwningComponent->NetMulticast_InvokeGameplayCuesAddedAndWhileActive_WithParams(FGameplayTagContainer::CreateFromArray(Tags), PredictionKey, Parameters);
		}
		else
		{
			ABILITY_LOG(Warning, TEXT("No actual gameplay cue tags found in GameplayEffect %s (despite it having entries in its gameplay cue list!"), *Spec.Def->GetName());

		}
	}
	else
	{
		OwningComponent->NetMulticast_InvokeGameplayCueAddedAndWhileActive_FromSpec(Spec, PredictionKey);

	}
}

void UGameplayCueManager::InvokeGameplayCueExecuted_FromSpec(UAbilitySystemComponent* OwningComponent, const FGameplayEffectSpec& Spec, FPredictionKey PredictionKey)
{	
	if (Spec.Def->GameplayCues.Num() == 0)
	{
		// This spec doesn't have any GCs, so early out
		ABILITY_LOG(Verbose, TEXT("No GCs in this Spec, so early out: %s"), *Spec.Def->GetName());
		return;
	}

	FGameplayCuePendingExecute PendingCue;

	if (AbilitySystemAlwaysConvertGESpecToGCParams)
	{
		// Transform the GE Spec into GameplayCue parmameters here (on the server)
		PendingCue.PayloadType = EGameplayCuePayloadType::CueParameters;
		PendingCue.OwningComponent = OwningComponent;
		PendingCue.PredictionKey = PredictionKey;

		PullGameplayCueTagsFromSpec(Spec, PendingCue.GameplayCueTags);
		if (PendingCue.GameplayCueTags.Num() == 0)
		{
			ABILITY_LOG(Warning, TEXT("GE %s has GameplayCues but not valid GameplayCue tag."), *Spec.Def->GetName());			
			return;
		}
		
		UAbilitySystemGlobals::Get().InitGameplayCueParameters_GESpec(PendingCue.CueParameters, Spec);
	}
	else
	{
		// Transform the GE Spec into a FGameplayEffectSpecForRPC (holds less information than the GE Spec itself, but more information that the FGamepalyCueParameter)
		PendingCue.PayloadType = EGameplayCuePayloadType::FromSpec;
		PendingCue.OwningComponent = OwningComponent;
		PendingCue.FromSpec = FGameplayEffectSpecForRPC(Spec);
		PendingCue.PredictionKey = PredictionKey;
	}

	if (ProcessPendingCueExecute(PendingCue))
	{
		PendingExecuteCues.Add(PendingCue);
	}

	if (GameplayCueSendContextCount == 0)
	{
		// Not in a context, flush now
		FlushPendingCues();
	}
}

void UGameplayCueManager::InvokeGameplayCueExecuted(UAbilitySystemComponent* OwningComponent, const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext)
{
	FGameplayCuePendingExecute PendingCue;
	PendingCue.PayloadType = EGameplayCuePayloadType::CueParameters;
	PendingCue.GameplayCueTags.Add(GameplayCueTag);
	PendingCue.OwningComponent = OwningComponent;
	UAbilitySystemGlobals::Get().InitGameplayCueParameters(PendingCue.CueParameters, EffectContext);
	PendingCue.PredictionKey = PredictionKey;

	if (ProcessPendingCueExecute(PendingCue))
	{
		PendingExecuteCues.Add(PendingCue);
	}

	if (GameplayCueSendContextCount == 0)
	{
		// Not in a context, flush now
		FlushPendingCues();
	}
}

void UGameplayCueManager::InvokeGameplayCueExecuted_WithParams(UAbilitySystemComponent* OwningComponent, const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters)
{
	FGameplayCuePendingExecute PendingCue;
	PendingCue.PayloadType = EGameplayCuePayloadType::CueParameters;
	PendingCue.GameplayCueTags.Add(GameplayCueTag);
	PendingCue.OwningComponent = OwningComponent;
	PendingCue.CueParameters = GameplayCueParameters;
	PendingCue.PredictionKey = PredictionKey;

	if (ProcessPendingCueExecute(PendingCue))
	{
		PendingExecuteCues.Add(PendingCue);
	}

	if (GameplayCueSendContextCount == 0)
	{
		// Not in a context, flush now
		FlushPendingCues();
	}
}

void UGameplayCueManager::StartGameplayCueSendContext()
{
	GameplayCueSendContextCount++;
}

void UGameplayCueManager::EndGameplayCueSendContext()
{
	GameplayCueSendContextCount--;

	if (GameplayCueSendContextCount == 0)
	{
		FlushPendingCues();
	}
	else if (GameplayCueSendContextCount < 0)
	{
		ABILITY_LOG(Warning, TEXT("UGameplayCueManager::EndGameplayCueSendContext called too many times! Negative context count"));
	}
}

void UGameplayCueManager::FlushPendingCues()
{
	TArray<FGameplayCuePendingExecute> LocalPendingExecuteCues = PendingExecuteCues;
	PendingExecuteCues.Empty();
	for (int32 i = 0; i < LocalPendingExecuteCues.Num(); i++)
	{
		FGameplayCuePendingExecute& PendingCue = LocalPendingExecuteCues[i];

		// Our component may have gone away
		if (PendingCue.OwningComponent)
		{
			bool bHasAuthority = PendingCue.OwningComponent->IsOwnerActorAuthoritative();
			bool bLocalPredictionKey = PendingCue.PredictionKey.IsLocalClientKey();

			// TODO: Could implement non-rpc method for replicating if desired
			switch (PendingCue.PayloadType)
			{
			case EGameplayCuePayloadType::CueParameters:
				if (ensure(PendingCue.GameplayCueTags.Num() >= 1))
				{
					if (bHasAuthority)
					{
						PendingCue.OwningComponent->ForceReplication();
						if (PendingCue.GameplayCueTags.Num() > 1)
						{
							PendingCue.OwningComponent->NetMulticast_InvokeGameplayCuesExecuted_WithParams(FGameplayTagContainer::CreateFromArray(PendingCue.GameplayCueTags), PendingCue.PredictionKey, PendingCue.CueParameters);
						}
						else
						{
							PendingCue.OwningComponent->NetMulticast_InvokeGameplayCueExecuted_WithParams(PendingCue.GameplayCueTags[0], PendingCue.PredictionKey, PendingCue.CueParameters);
							static FName NetMulticast_InvokeGameplayCueExecuted_WithParamsName = TEXT("NetMulticast_InvokeGameplayCueExecuted_WithParams");
							CheckForTooManyRPCs(NetMulticast_InvokeGameplayCueExecuted_WithParamsName, PendingCue, PendingCue.GameplayCueTags[0].ToString(), nullptr);
						}
					}
					else if (bLocalPredictionKey)
					{
						for (const FGameplayTag& Tag : PendingCue.GameplayCueTags)
						{
							PendingCue.OwningComponent->InvokeGameplayCueEvent(Tag, EGameplayCueEvent::Executed, PendingCue.CueParameters);
						}
					}
				}
				break;
			case EGameplayCuePayloadType::EffectContext:
				if (ensure(PendingCue.GameplayCueTags.Num() >= 1))
				{
					if (bHasAuthority)
					{
						PendingCue.OwningComponent->ForceReplication();
						if (PendingCue.GameplayCueTags.Num() > 1)
						{
							PendingCue.OwningComponent->NetMulticast_InvokeGameplayCuesExecuted(FGameplayTagContainer::CreateFromArray(PendingCue.GameplayCueTags), PendingCue.PredictionKey, PendingCue.CueParameters.EffectContext);
						}
						else
						{
							PendingCue.OwningComponent->NetMulticast_InvokeGameplayCueExecuted(PendingCue.GameplayCueTags[0], PendingCue.PredictionKey, PendingCue.CueParameters.EffectContext);
							static FName NetMulticast_InvokeGameplayCueExecutedName = TEXT("NetMulticast_InvokeGameplayCueExecuted");
							CheckForTooManyRPCs(NetMulticast_InvokeGameplayCueExecutedName, PendingCue, PendingCue.GameplayCueTags[0].ToString(), PendingCue.CueParameters.EffectContext.Get());
						}
					}
					else if (bLocalPredictionKey)
					{
						for (const FGameplayTag& Tag : PendingCue.GameplayCueTags)
						{
							PendingCue.OwningComponent->InvokeGameplayCueEvent(Tag, EGameplayCueEvent::Executed, PendingCue.CueParameters.EffectContext);
						}
					}
				}
				break;
			case EGameplayCuePayloadType::FromSpec:
				if (bHasAuthority)
				{
					PendingCue.OwningComponent->ForceReplication();
					PendingCue.OwningComponent->NetMulticast_InvokeGameplayCueExecuted_FromSpec(PendingCue.FromSpec, PendingCue.PredictionKey);
					static FName NetMulticast_InvokeGameplayCueExecuted_FromSpecName = TEXT("NetMulticast_InvokeGameplayCueExecuted_FromSpec");
					CheckForTooManyRPCs(NetMulticast_InvokeGameplayCueExecuted_FromSpecName, PendingCue, PendingCue.FromSpec.Def ? PendingCue.FromSpec.ToSimpleString() : TEXT("FromSpecWithNoDef"), PendingCue.FromSpec.EffectContext.Get());
				}
				else if (bLocalPredictionKey)
				{
					PendingCue.OwningComponent->InvokeGameplayCueEvent(PendingCue.FromSpec, EGameplayCueEvent::Executed);
				}
				break;
			}
		}
	}
}

bool UGameplayCueManager::ProcessPendingCueExecute(FGameplayCuePendingExecute& PendingCue)
{
	// Subclasses can do something here
	return true;
}

bool UGameplayCueManager::DoesPendingCueExecuteMatch(FGameplayCuePendingExecute& PendingCue, FGameplayCuePendingExecute& ExistingCue)
{
	const FHitResult* PendingHitResult = NULL;
	const FHitResult* ExistingHitResult = NULL;

	if (PendingCue.PayloadType != ExistingCue.PayloadType)
	{
		return false;
	}

	if (PendingCue.OwningComponent != ExistingCue.OwningComponent)
	{
		return false;
	}

	if (PendingCue.PredictionKey.PredictiveConnection != ExistingCue.PredictionKey.PredictiveConnection)
	{
		// They can both by null, but if they were predicted by different people exclude it
		return false;
	}

	if (PendingCue.PayloadType == EGameplayCuePayloadType::FromSpec)
	{
		if (PendingCue.FromSpec.Def != ExistingCue.FromSpec.Def)
		{
			return false;
		}

		if (PendingCue.FromSpec.Level != ExistingCue.FromSpec.Level)
		{
			return false;
		}
	}
	else
	{
		if (PendingCue.GameplayCueTags != ExistingCue.GameplayCueTags)
		{
			return false;
		}
	}

	return true;
}

void UGameplayCueManager::CheckForPreallocation(UClass* GCClass)
{
	if (AGameplayCueNotify_Actor* InstancedCue = Cast<AGameplayCueNotify_Actor>(GCClass->ClassDefaultObject))
	{
		if (InstancedCue->NumPreallocatedInstances > 0 && GameplayCueClassesForPreallocation.Contains(GCClass) == false)
		{
			// Add this to the global list
			GameplayCueClassesForPreallocation.Add(GCClass);

			// Add it to any world specific lists
			for (FPreallocationInfo& Info : PreallocationInfoList_Internal)
			{
				ensure(Info.ClassesNeedingPreallocation.Contains(GCClass)==false);
				Info.ClassesNeedingPreallocation.Push(GCClass);
			}
		}
	}
}

// -------------------------------------------------------------

void UGameplayCueManager::ResetPreallocation(UWorld* World)
{
	FPreallocationInfo& Info = GetPreallocationInfo(World);

	Info.PreallocatedInstances.Reset();
	Info.ClassesNeedingPreallocation = GameplayCueClassesForPreallocation;
}

void UGameplayCueManager::UpdatePreallocation(UWorld* World)
{
#if WITH_EDITOR	
	// Don't preallocate
	if (World->IsPreviewWorld())
	{
		return;
	}
#endif

	FPreallocationInfo& Info = GetPreallocationInfo(World);

	if (Info.ClassesNeedingPreallocation.Num() > 0)
	{
		TSubclassOf<AGameplayCueNotify_Actor> GCClass = Info.ClassesNeedingPreallocation.Last();
		AGameplayCueNotify_Actor* CDO = GCClass->GetDefaultObject<AGameplayCueNotify_Actor>();
		TArray<AGameplayCueNotify_Actor*>& PreallocatedList = Info.PreallocatedInstances.FindOrAdd(CDO->GetClass());

		AGameplayCueNotify_Actor* PrespawnedInstance = Cast<AGameplayCueNotify_Actor>(World->SpawnActor(CDO->GetClass()));
		if (ensureMsgf(PrespawnedInstance, TEXT("Failed to prespawn GC notify for: %s"), *GetNameSafe(CDO)))
		{
			ensureMsgf(PrespawnedInstance->IsPendingKill() == false, TEXT("Newly spawned GC is PendingKILL: %s"), *GetNameSafe(CDO));

			if (LogGameplayCueActorSpawning)
			{
				ABILITY_LOG(Warning, TEXT("Prespawning GC %s"), *GetNameSafe(CDO));
			}

			PrespawnedInstance->bInRecycleQueue = true;
			PreallocatedList.Push(PrespawnedInstance);
			PrespawnedInstance->SetActorHiddenInGame(true);

			if (PreallocatedList.Num() >= CDO->NumPreallocatedInstances)
			{
				Info.ClassesNeedingPreallocation.Pop(false);
			}
		}
	}
}

FPreallocationInfo& UGameplayCueManager::GetPreallocationInfo(UWorld* World)
{
	FObjectKey ObjKey(World);

	for (FPreallocationInfo& Info : PreallocationInfoList_Internal)
	{
		if (ObjKey == Info.OwningWorldKey)
		{
			return Info;
		}
	}

	FPreallocationInfo NewInfo;
	NewInfo.OwningWorldKey = ObjKey;

	PreallocationInfoList_Internal.Add(NewInfo);
	return PreallocationInfoList_Internal.Last();
}

void UGameplayCueManager::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	DumpPreallocationStats(World);
#endif

	for (int32 idx=0; idx < PreallocationInfoList_Internal.Num(); ++idx)
	{
		if (PreallocationInfoList_Internal[idx].OwningWorldKey == FObjectKey(World))
		{
			ABILITY_LOG(Verbose, TEXT("UGameplayCueManager::OnWorldCleanup Removing PreallocationInfoList_Internal element %d"), idx);
			PreallocationInfoList_Internal.RemoveAtSwap(idx, 1, false);
			idx--;
		}
	}

	IGameplayCueInterface::ClearTagToFunctionMap();
}

void UGameplayCueManager::DumpPreallocationStats(UWorld* World)
{
	if (World == nullptr)
	{
		return;
	}

	FPreallocationInfo& Info = GetPreallocationInfo(World);
	for (auto &It : Info.PreallocatedInstances)
	{
		if (UClass* ThisClass = It.Key)
		{
			if (AGameplayCueNotify_Actor* CDO = ThisClass->GetDefaultObject<AGameplayCueNotify_Actor>())
			{
				TArray<AGameplayCueNotify_Actor*>& List = It.Value;
				if (List.Num() > CDO->NumPreallocatedInstances)
				{
					ABILITY_LOG(Display, TEXT("Notify class: %s was used simultaneously %d times. The CDO default is %d preallocated instanced."), *ThisClass->GetName(), List.Num(),  CDO->NumPreallocatedInstances); 
				}
			}
		}
	}
}

void UGameplayCueManager::OnPreReplayScrub(UWorld* World)
{
	// See if the World's demo net driver is the duplicated collection's driver,
	// and if so, don't reset preallocated instances. Since the preallocations are global
	// among all level collections, this would clear all current preallocated instances from the list,
	// but there's no need to, and the actor instances would still be around, causing a leak.
	const FLevelCollection* const DuplicateLevelCollection = World ? World->FindCollectionByType(ELevelCollectionType::DynamicDuplicatedLevels) : nullptr;
	if (DuplicateLevelCollection && DuplicateLevelCollection->GetDemoNetDriver() == World->DemoNetDriver)
	{
		return;
	}

	FPreallocationInfo& Info = GetPreallocationInfo(World);
	Info.PreallocatedInstances.Reset();
}

#if GAMEPLAYCUE_DEBUG
FGameplayCueDebugInfo* UGameplayCueManager::GetDebugInfo(int32 Handle, bool Reset)
{
	static const int32 MaxDebugEntries = 256;
	int32 Index = Handle % MaxDebugEntries;

	static TArray<FGameplayCueDebugInfo> DebugArray;
	if (DebugArray.Num() == 0)
	{
		DebugArray.AddDefaulted(MaxDebugEntries);
	}
	if (Reset)
	{
		DebugArray[Index] = FGameplayCueDebugInfo();
	}

	return &DebugArray[Index];
}
#endif

// ----------------------------------------------------------------

static void	RunGameplayCueTranslator(UWorld* InWorld)
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->TranslationManager.BuildTagTranslationTable();
}

FAutoConsoleCommandWithWorld RunGameplayCueTranslatorCmd(
	TEXT("GameplayCue.BuildGameplayCueTranslator"),
	TEXT("Displays GameplayCue notify map"),
	FConsoleCommandWithWorldDelegate::CreateStatic(RunGameplayCueTranslator)
	);

// -----------------------------------------------------

static void	PrintGameplayCueTranslator(UWorld* InWorld)
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->TranslationManager.PrintTranslationTable();
}

FAutoConsoleCommandWithWorld PrintGameplayCueTranslatorCmd(
	TEXT("GameplayCue.PrintGameplayCueTranslator"),
	TEXT("Displays GameplayCue notify map"),
	FConsoleCommandWithWorldDelegate::CreateStatic(PrintGameplayCueTranslator)
	);


#if WITH_EDITOR
#undef LOCTEXT_NAMESPACE
#endif
