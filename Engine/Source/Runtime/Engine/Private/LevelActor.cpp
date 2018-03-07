// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Misc/OutputDeviceFile.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "UObject/ScriptStackTracker.h"
#include "EngineStats.h"
#include "EngineGlobals.h"
#include "Engine/EngineTypes.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Engine/Brush.h"
#include "UObject/LinkerLoad.h"
#include "UObject/CoreOnline.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreaming.h"
#include "ContentStreaming.h"
#include "EditorSupportDelegates.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/DemoNetDriver.h"
#include "AudioDeviceManager.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/MapErrors.h"

#include "Components/BoxComponent.h"
#include "GameFramework/MovementComponent.h"

#include "Misc/TimeGuard.h"

#define LOCTEXT_NAMESPACE "LevelActor"

// CVars
static TAutoConsoleVariable<float> CVarEncroachEpsilon(
	TEXT("p.EncroachEpsilon"),
	0.15f,
	TEXT("Epsilon value used during encroachment checking for shape components\n")
	TEXT("0: use full sized shape. > 0: shrink shape size by this amount (world units)"),
	ECVF_Default);

#define LINE_CHECK_TRACING 0

#if LINE_CHECK_TRACING

/** Is tracking enabled */
int32 LineCheckTracker::bIsTrackingEnabled = false;
/** If this count is nonzero, dump the log when we exceed this number in any given frame */
int32 LineCheckTracker::TraceCountForSpikeDump = 0;
/** Number of traces recorded this frame */
int32 LineCheckTracker::CurrentCountForSpike = 0;

FStackTracker* LineCheckTracker::LineCheckStackTracker = NULL;
FScriptStackTracker* LineCheckTracker::LineCheckScriptStackTracker = NULL;

/** Updates an existing call stack trace with new data for this particular call*/
static void LineCheckUpdateFn(const FStackTracker::FCallStack& CallStack, void* UserData)
{
	if (UserData)
	{
		//Callstack has been called more than once, aggregate the data
		LineCheckTracker::FLineCheckData* NewLCData = static_cast<LineCheckTracker::FLineCheckData*>(UserData);
		LineCheckTracker::FLineCheckData* OldLCData = static_cast<LineCheckTracker::FLineCheckData*>(CallStack.UserData);

		OldLCData->Flags |= NewLCData->Flags;
		OldLCData->IsNonZeroExtent |= NewLCData->IsNonZeroExtent;

		if (NewLCData->LineCheckObjsMap.Num() > 0)
		{
			for (TMap<const FName, LineCheckTracker::FLineCheckData::LineCheckObj>::TConstIterator It(NewLCData->LineCheckObjsMap); It; ++It)
			{
				const LineCheckTracker::FLineCheckData::LineCheckObj &NewObj = It.Value();

				LineCheckTracker::FLineCheckData::LineCheckObj * OldObj = OldLCData->LineCheckObjsMap.Find(NewObj.ObjectName);
				if (OldObj)
				{
					OldObj->Count += NewObj.Count;
				}
				else
				{
					OldLCData->LineCheckObjsMap.Add(NewObj.ObjectName, NewObj);
				}
			}
		}
	}
}

/** After the stack tracker reports a given stack trace, it calls this function
*  which appends data particular to line checks
*/
static void LineCheckReportFn(const FStackTracker::FCallStack& CallStack, uint64 TotalStackCount, FOutputDevice& Ar)
{
	//Output to a csv file any relevant data
	LineCheckTracker::FLineCheckData* const LCData = static_cast<LineCheckTracker::FLineCheckData*>(CallStack.UserData);
	if (LCData)
	{
		FString UserOutput = LINE_TERMINATOR TEXT(",,,");
		UserOutput += (LCData->IsNonZeroExtent ? TEXT( "NonZeroExtent") : TEXT("ZeroExtent"));

		for (TMap<const FName, LineCheckTracker::FLineCheckData::LineCheckObj>::TConstIterator It(LCData->LineCheckObjsMap); It; ++It)
		{
			UserOutput += LINE_TERMINATOR TEXT(",,,");
			const LineCheckTracker::FLineCheckData::LineCheckObj &CurObj = It.Value();
			UserOutput += FString::Printf(TEXT("%s (%d) : %s"), *CurObj.ObjectName.ToString(), CurObj.Count, *CurObj.DetailedInfo);
		}

		UserOutput += LINE_TERMINATOR TEXT(",,,");
		
		Ar.Log(*UserOutput);
	}
}

/** Called at the beginning of each frame to check/reset spike count */
void LineCheckTracker::Tick()
{
	if(bIsTrackingEnabled && LineCheckStackTracker)
	{
		//Spike logging is enabled
		if (TraceCountForSpikeDump > 0)
		{
			//Dump if we exceeded the threshold this frame
			if (CurrentCountForSpike > TraceCountForSpikeDump)
			{
				DumpLineChecks(5);
			}
			//Reset for next frame
			ResetLineChecks();
		}

		CurrentCountForSpike = 0;
	}
}

/** Set the value which, if exceeded, will cause a dump of the line checks this frame */
void LineCheckTracker::SetSpikeMinTraceCount(int32 MinTraceCount)
{
	TraceCountForSpikeDump = FMath::Max(0, MinTraceCount);
	UE_LOG(LogSpawn, Log, TEXT("Line trace spike count is %d."), TraceCountForSpikeDump);
}

/** Dump out the results of all line checks called in the game since the last call to ResetLineChecks() */
void LineCheckTracker::DumpLineChecks(int32 Threshold)
{
	if( LineCheckStackTracker )
	{
		const FString Filename = FString::Printf(TEXT("%sLineCheckLog-%s.csv"), *FPaths::ProjectLogDir(), *FDateTime::Now().ToString());
		FOutputDeviceFile OutputFile(*Filename);
		LineCheckStackTracker->DumpStackTraces( Threshold, OutputFile );
		OutputFile.TearDown();
	}

	if( LineCheckScriptStackTracker )
	{
		const FString Filename = FString::Printf(TEXT("%sScriptLineCheckLog-%s.csv"), *FPaths::ProjectLogDir(), *FDateTime::Now().ToString());
		FOutputDeviceFile OutputFile(*Filename);
		LineCheckScriptStackTracker->DumpStackTraces( Threshold, OutputFile );
		OutputFile.TearDown();
	}
}

/** Reset the line check stack tracker (calls FMemory::Free() on all user data pointers)*/
void LineCheckTracker::ResetLineChecks()
{
	if( LineCheckStackTracker )
	{
		LineCheckStackTracker->ResetTracking();
	}

	if( LineCheckScriptStackTracker )
	{
		LineCheckScriptStackTracker->ResetTracking();
	}
}

/** Turn line check stack traces on and off, does not reset the actual data */
void LineCheckTracker::ToggleLineChecks()
{
	bIsTrackingEnabled = !bIsTrackingEnabled;
	UE_LOG(LogSpawn, Log, TEXT("Line tracing is now %s."), bIsTrackingEnabled ? TEXT("enabled") : TEXT("disabled"));
	
	CurrentCountForSpike = 0;
	if (LineCheckStackTracker == NULL)
	{
		FPlatformStackWalk::InitStackWalking();
		LineCheckStackTracker = new FStackTracker(LineCheckUpdateFn, LineCheckReportFn);
	}

	if (LineCheckScriptStackTracker == NULL)
	{
		LineCheckScriptStackTracker = new FScriptStackTracker();
	}

	LineCheckStackTracker->ToggleTracking();
	LineCheckScriptStackTracker->ToggleTracking();
}

/** Captures a single stack trace for a line check */
void LineCheckTracker::CaptureLineCheck(int32 LineCheckFlags, const FVector* Extent, const FFrame* ScriptStackFrame, const UObject* Object)
{
	if (LineCheckStackTracker == NULL || LineCheckScriptStackTracker == NULL)
	{
		return;
	}

	if (ScriptStackFrame)
	{
		int32 EntriesToIgnore = 0;
		LineCheckScriptStackTracker->CaptureStackTrace(ScriptStackFrame, EntriesToIgnore);
	}
	else
	{		   
		FLineCheckData* const LCData = static_cast<FLineCheckData*>(FMemory::Malloc(sizeof(FLineCheckData)));
		FMemory::Memset(LCData, 0, sizeof(FLineCheckData));
		LCData->Flags = LineCheckFlags;
		LCData->IsNonZeroExtent = (Extent && !Extent->IsZero()) ? true : false;
		FLineCheckData::LineCheckObj LCObj;
		if (Object)
		{
			LCObj = FLineCheckData::LineCheckObj(Object->GetFName(), 1, Object->GetDetailedInfo());
		}
		else
		{
			LCObj = FLineCheckData::LineCheckObj(NAME_None, 1, TEXT("Unknown"));
		}
		
		LCData->LineCheckObjsMap.Add(LCObj.ObjectName, LCObj);		

		int32 EntriesToIgnore = 3;
		LineCheckStackTracker->CaptureStackTrace(EntriesToIgnore, static_cast<void*>(LCData));
		//Only increment here because execTrace() will lead to here also
		CurrentCountForSpike++;
	}
}
#endif //LINE_CHECK_TRACING

/*-----------------------------------------------------------------------------
	Level actor management.
-----------------------------------------------------------------------------*/
// LOOKING_FOR_PERF_ISSUES
#define PERF_SHOW_MULTI_PAWN_SPAWN_FRAMES (!(UE_BUILD_SHIPPING || UE_BUILD_TEST)) && (LOOKING_FOR_PERF_ISSUES || !WITH_EDITORONLY_DATA)

#if PERF_SHOW_MULTI_PAWN_SPAWN_FRAMES
	/** Array showing names of pawns spawned this frame. */
	TArray<FString>	ThisFramePawnSpawns;
#endif	//PERF_SHOW_MULTI_PAWN_SPAWN_FRAMES

AActor* UWorld::SpawnActorAbsolute( UClass* Class, FTransform const& AbsoluteTransform, const FActorSpawnParameters& SpawnParameters)
{
	AActor* Template = SpawnParameters.Template;

	if(!Template)
	{
		// Use class's default actor as a template.
		Template = Class->GetDefaultObject<AActor>();
	}

	FTransform NewTransform = AbsoluteTransform;
	USceneComponent* TemplateRootComponent = (Template)? Template->GetRootComponent() : NULL;
	if(TemplateRootComponent)
	{
		TemplateRootComponent->UpdateComponentToWorld();
		NewTransform = TemplateRootComponent->GetComponentToWorld().Inverse() * NewTransform;
	}

	return SpawnActor(Class, &NewTransform, SpawnParameters);
}

AActor* UWorld::SpawnActor( UClass* Class, FVector const* Location, FRotator const* Rotation, const FActorSpawnParameters& SpawnParameters )
{
	FTransform Transform;
	if (Location)
	{
		Transform.SetLocation(*Location);
	}
	if (Rotation)
	{
		Transform.SetRotation(FQuat(*Rotation));
	}

	return SpawnActor(Class, &Transform, SpawnParameters);
}

#include "GameFramework/SpawnActorTimer.h"

AActor* UWorld::SpawnActor( UClass* Class, FTransform const* UserTransformPtr, const FActorSpawnParameters& SpawnParameters )
{
	SCOPE_CYCLE_COUNTER(STAT_SpawnActorTime);
	SCOPE_TIME_GUARD_NAMED_MS(TEXT("SpawnActor Of Type"), Class->GetFName(), 2);
	

	check( CurrentLevel ); 	
	check(GIsEditor || (CurrentLevel == PersistentLevel));

	// Make sure this class is spawnable.
	if( !Class )
	{
		UE_LOG(LogSpawn, Warning, TEXT("SpawnActor failed because no class was specified") );
		return NULL;
	}

#if ENABLE_SPAWNACTORTIMER
	FScopedSpawnActorTimer SpawnTimer(Class->GetFName(), SpawnParameters.bDeferConstruction ? ESpawnActorTimingType::SpawnActorDeferred : ESpawnActorTimingType::SpawnActorNonDeferred);
#endif

	if( Class->HasAnyClassFlags(CLASS_Deprecated) )
	{
		UE_LOG(LogSpawn, Warning, TEXT("SpawnActor failed because class %s is deprecated"), *Class->GetName() );
		return NULL;
	}
	if( Class->HasAnyClassFlags(CLASS_Abstract) )
	{
		UE_LOG(LogSpawn, Warning, TEXT("SpawnActor failed because class %s is abstract"), *Class->GetName() );
		return NULL;
	}
	else if( !Class->IsChildOf(AActor::StaticClass()) )
	{
		UE_LOG(LogSpawn, Warning, TEXT("SpawnActor failed because %s is not an actor class"), *Class->GetName() );
		return NULL;
	}
	else if (SpawnParameters.Template != NULL && SpawnParameters.Template->GetClass() != Class)
	{
		UE_LOG(LogSpawn, Warning, TEXT("SpawnActor failed because template class (%s) does not match spawn class (%s)"), *SpawnParameters.Template->GetClass()->GetName(), *Class->GetName());
		if (!SpawnParameters.bNoFail)
		{
			return NULL;
		}
	}
	else if (bIsRunningConstructionScript && !SpawnParameters.bAllowDuringConstructionScript)
	{
		UE_LOG(LogSpawn, Warning, TEXT("SpawnActor failed because we are running a ConstructionScript (%s)"), *Class->GetName() );
		return NULL;
	}
	else if (bIsTearingDown)
	{
		UE_LOG(LogSpawn, Warning, TEXT("SpawnActor failed because we are in the process of tearing down the world"));
		return NULL;
	}
	else if (UserTransformPtr && UserTransformPtr->ContainsNaN())
	{
		UE_LOG(LogSpawn, Warning, TEXT("SpawnActor failed because the given transform (%s) is invalid"), *(UserTransformPtr->ToString()));
		return NULL;
	}

	ULevel* LevelToSpawnIn = SpawnParameters.OverrideLevel;
	if (LevelToSpawnIn == NULL)
	{
		// Spawn in the same level as the owner if we have one. @warning: this relies on the outer of an actor being the level.
		LevelToSpawnIn = (SpawnParameters.Owner != NULL) ? CastChecked<ULevel>(SpawnParameters.Owner->GetOuter()) : CurrentLevel;
	}

	FName NewActorName = SpawnParameters.Name;
	AActor* Template = SpawnParameters.Template;

	if( !Template )
	{
		// Use class's default actor as a template.
		Template = Class->GetDefaultObject<AActor>();
	}
	else if (NewActorName.IsNone() && !Template->HasAnyFlags(RF_ClassDefaultObject))
	{
		NewActorName = MakeUniqueObjectName(LevelToSpawnIn, Template->GetClass(), *Template->GetFName().GetPlainNameString());
	}
	check(Template);

	// See if we can spawn on ded.server/client only etc (check NeedsLoadForClient & NeedsLoadForServer)
	if(!CanCreateInCurrentContext(Template))
	{
		UE_LOG(LogSpawn, Warning, TEXT("Unable to spawn class '%s' due to client/server context."), *Class->GetName() );
		return NULL;
	}

	FTransform const UserTransform = UserTransformPtr ? *UserTransformPtr : FTransform::Identity;

	ESpawnActorCollisionHandlingMethod CollisionHandlingOverride = SpawnParameters.SpawnCollisionHandlingOverride;

	// "no fail" take preedence over collision handling settings that include fails
	if (SpawnParameters.bNoFail)
	{
		// maybe upgrade to disallow fail
		if (CollisionHandlingOverride == ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding)
		{
			CollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		}
		else if (CollisionHandlingOverride == ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding)
		{
			CollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		}
	}

	// use override if set, else fall back to actor's preference
	ESpawnActorCollisionHandlingMethod const CollisionHandlingMethod = (CollisionHandlingOverride == ESpawnActorCollisionHandlingMethod::Undefined) ? Template->SpawnCollisionHandlingMethod : CollisionHandlingOverride;

	// see if we can avoid spawning altogether by checking native components
	// note: we can't handle all cases here, since we don't know the full component hierarchy until after the actor is spawned
	if (CollisionHandlingMethod == ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding)
	{
		USceneComponent* const TemplateRootComponent = Template->GetRootComponent();

		// Note that we respect any initial transformation the root component may have from the CDO, so the final transform
		// might necessarily be exactly the passed-in UserTransform.
		FTransform const FinalRootComponentTransform =
			TemplateRootComponent
			? FTransform(TemplateRootComponent->RelativeRotation, TemplateRootComponent->RelativeLocation, TemplateRootComponent->RelativeScale3D) * UserTransform
			: UserTransform;

		FVector const FinalRootLocation = FinalRootComponentTransform.GetLocation();
		FRotator const FinalRootRotation = FinalRootComponentTransform.Rotator();

		if (EncroachingBlockingGeometry(Template, FinalRootLocation, FinalRootRotation))
		{
			// a native component is colliding, that's enough to reject spawning
			UE_LOG(LogSpawn, Log, TEXT("SpawnActor failed because of collision at the spawn location [%s] for [%s]"), *FinalRootLocation.ToString(), *Class->GetName());
			return nullptr;
		}
	}

	// actually make the actor object
	AActor* const Actor = NewObject<AActor>(LevelToSpawnIn, Class, NewActorName, SpawnParameters.ObjectFlags, Template);
	check(Actor);

#if ENABLE_SPAWNACTORTIMER
	SpawnTimer.SetActorName(Actor->GetFName());
#endif

#if WITH_EDITOR
	Actor->ClearActorLabel(); // Clear label on newly spawned actors
#endif // WITH_EDITOR

	if ( GUndo )
	{
		ModifyLevel( LevelToSpawnIn );
	}
	LevelToSpawnIn->Actors.Add( Actor );
	LevelToSpawnIn->ActorsForGC.Add(Actor);

	// Add this newly spawned actor to the network actor list
	AddNetworkActor( Actor );

#if PERF_SHOW_MULTI_PAWN_SPAWN_FRAMES
	if( Cast<APawn>(Actor) )
	{
		FString PawnName = FString::Printf(TEXT("%d: %s"), ThisFramePawnSpawns.Num(), *Actor->GetPathName());
		ThisFramePawnSpawns.Add(PawnName);
	}
#endif

	// tell the actor what method to use, in case it was overridden
	Actor->SpawnCollisionHandlingMethod = CollisionHandlingMethod;

#if WITH_EDITOR
	Actor->bIsEditorPreviewActor = SpawnParameters.bTemporaryEditorActor;
#endif //WITH_EDITOR

	Actor->PostSpawnInitialize(UserTransform, SpawnParameters.Owner, SpawnParameters.Instigator, SpawnParameters.IsRemoteOwned(), SpawnParameters.bNoFail, SpawnParameters.bDeferConstruction);

	if (Actor->IsPendingKill() && !SpawnParameters.bNoFail)
	{
		UE_LOG(LogSpawn, Log, TEXT("SpawnActor failed because the spawned actor %s IsPendingKill"), *Actor->GetPathName());
		return NULL;
	}

	Actor->CheckDefaultSubobjects();

	// Broadcast notification of spawn
	OnActorSpawned.Broadcast(Actor);

#if WITH_EDITOR
	if (GIsEditor)
	{
		GEngine->BroadcastLevelActorAdded(Actor);
	}
#endif

	return Actor;
}


ABrush* UWorld::SpawnBrush()
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABrush* const Result = SpawnActor<ABrush>( SpawnInfo );
	check(Result);
	return Result;
}

/**
 * Wrapper for DestroyActor() that should be called in the editor.
 *
 * @param	bShouldModifyLevel		If true, Modify() the level before removing the actor.
 */
bool UWorld::EditorDestroyActor( AActor* ThisActor, bool bShouldModifyLevel )
{
	UNavigationSystem::OnActorUnregistered(ThisActor);

	bool bReturnValue = DestroyActor( ThisActor, false, bShouldModifyLevel );
	ThisActor->GetWorld()->BroadcastLevelsChanged();
	return bReturnValue;
}

/**
 * Removes the actor from its level's actor list and generally cleans up the engine's internal state.
 * What this function does not do, but is handled via garbage collection instead, is remove references
 * to this actor from all other actors, and kill the actor's resources.  This function is set up so that
 * no problems occur even if the actor is being destroyed inside its recursion stack.
 *
 * @param	ThisActor				Actor to remove.
 * @param	bNetForce				[opt] Ignored unless called during play.  Default is false.
 * @param	bShouldModifyLevel		[opt] If true, Modify() the level before removing the actor.  Default is true.
 * @return							true if destroy, false if actor couldn't be destroyed.
 */
bool UWorld::DestroyActor( AActor* ThisActor, bool bNetForce, bool bShouldModifyLevel )
{
	check(ThisActor);
	check(ThisActor->IsValidLowLevel());
	//UE_LOG(LogSpawn, Log,  "Destroy %s", *ThisActor->GetClass()->GetName() );

	if (ThisActor->GetWorld() == NULL)
	{
		UE_LOG(LogSpawn, Warning, TEXT("Destroying %s, which doesn't have a valid world pointer"), *ThisActor->GetPathName());
	}

	// If already on list to be deleted, pretend the call was successful.
	// We don't want recursive calls to trigger destruction notifications multiple times.
	if (ThisActor->IsPendingKillPending())
	{
		return true;
	}

	// In-game deletion rules.
	if( IsGameWorld() )
	{
		// Never destroy the world settings actor. This used to be enforced by bNoDelete and is actually needed for 
		// seamless travel and network games.
		if (GetWorldSettings() == ThisActor)
		{
			return false;
		}

		// Can't kill if wrong role.
		if( ThisActor->Role!=ROLE_Authority && !bNetForce && !ThisActor->bNetTemporary )
		{
			return false;
		}

		if (ThisActor->DestroyNetworkActorHandled())
		{
			// Network actor short circuited the destroy (network will cleanup properly)
			// Don't destroy PlayerControllers and BeaconClients
			return false;
		}
	}
	else
	{
		ThisActor->Modify();
	}

	// Prevent recursion
	FMarkActorIsBeingDestroyed MarkActorIsBeingDestroyed(ThisActor);

	// Notify the texture streaming manager about the destruction of this actor.
	IStreamingManager::Get().NotifyActorDestroyed( ThisActor );

	// Tell this actor it's about to be destroyed.
	ThisActor->Destroyed();

	// Detach this actor's children
	TArray<AActor*> AttachedActors;
	ThisActor->GetAttachedActors(AttachedActors);

	if (AttachedActors.Num() > 0)
	{
		TInlineComponentArray<USceneComponent*> SceneComponents;
		ThisActor->GetComponents(SceneComponents);

		for (TArray< AActor* >::TConstIterator AttachedActorIt(AttachedActors); AttachedActorIt; ++AttachedActorIt)
		{
			AActor* ChildActor = *AttachedActorIt;
			if (ChildActor != NULL)
			{
				for (USceneComponent* SceneComponent : SceneComponents)
				{
					ChildActor->DetachAllSceneComponents(SceneComponent, FDetachmentTransformRules::KeepWorldTransform);
				}
#if WITH_EDITOR
				if( GIsEditor )
				{
					GEngine->BroadcastLevelActorDetached(ChildActor, ThisActor);
				}
#endif
			}
		}
	}

	// Detach from anything we were attached to
	USceneComponent* RootComp = ThisActor->GetRootComponent();
	if( RootComp != nullptr && RootComp->GetAttachParent() != nullptr)
	{
		AActor* OldParentActor = RootComp->GetAttachParent()->GetOwner();
		if (OldParentActor)
		{
			OldParentActor->Modify();
		}

		ThisActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

#if WITH_EDITOR
		if( GIsEditor )
		{
			GEngine->BroadcastLevelActorDetached(ThisActor, OldParentActor);
		}
#endif
	}

	ThisActor->ClearComponentOverlaps();

	// If this actor has an owner, notify it that it has lost a child.
	if( ThisActor->GetOwner() )
	{
		ThisActor->SetOwner(NULL);
	}

	// Notify net drivers that this guy has been destroyed.
	if (GEngine->GetWorldContextFromWorld(this))
	{
		UNetDriver* ActorNetDriver = GEngine->FindNamedNetDriver(this,ThisActor->GetNetDriverName());
		if (ActorNetDriver)
		{
			ActorNetDriver->NotifyActorDestroyed(ThisActor);
		}
	}
	else if (WorldType != EWorldType::Inactive && !IsRunningCommandlet())
	{
		// Inactive worlds do not have a world context, otherwise only worlds in the middle of seamless travel should have no context,
		// and in that case, we shouldn't be destroying actors on them until they have become the current world (i.e. CopyWorldData has been called)
		UE_LOG(LogSpawn, Warning, TEXT("UWorld::DestroyActor: World has no context! World: %s, Actor: %s"), *GetName(), *ThisActor->GetPathName());
	}

	if ( DemoNetDriver )
	{
		DemoNetDriver->NotifyActorDestroyed( ThisActor );
	}

	// Remove the actor from the actor list.
	RemoveActor( ThisActor, bShouldModifyLevel );

	// Invalidate the lighting cache in the Editor.  We need to check for GIsEditor as play has not begun in network game and objects get destroyed on switching levels
	if ( GIsEditor )
	{
		if (!IsGameWorld())
		{
			ThisActor->InvalidateLightingCache();
		}
		
#if WITH_EDITOR
		GEngine->BroadcastLevelActorDeleted(ThisActor);
#endif
	}
		
	// Clean up the actor's components.
	ThisActor->UnregisterAllComponents();

	// Mark the actor and its direct components as pending kill.
	ThisActor->MarkPendingKill();
	ThisActor->MarkPackageDirty();
	ThisActor->MarkComponentsAsPendingKill();

	// Unregister the actor's tick function
	const bool bRegisterTickFunctions = false;
	const bool bIncludeComponents = true;
	ThisActor->RegisterAllActorTickFunctions(bRegisterTickFunctions, bIncludeComponents);

	// Return success.
	return true;
}

/*-----------------------------------------------------------------------------
	Player spawning.
-----------------------------------------------------------------------------*/

APlayerController* UWorld::SpawnPlayActor(UPlayer* NewPlayer, ENetRole RemoteRole, const FURL& InURL, const TSharedPtr<const FUniqueNetId>& UniqueId, FString& Error, uint8 InNetPlayerIndex)
{
	FUniqueNetIdRepl UniqueIdRepl(UniqueId);
	return SpawnPlayActor(NewPlayer, RemoteRole, InURL, UniqueIdRepl, Error, InNetPlayerIndex);
}

APlayerController* UWorld::SpawnPlayActor(UPlayer* NewPlayer, ENetRole RemoteRole, const FURL& InURL, const FUniqueNetIdRepl& UniqueId, FString& Error, uint8 InNetPlayerIndex)
{
	Error = TEXT("");

	// Make the option string.
	FString Options;
	for (int32 i = 0; i < InURL.Op.Num(); i++)
	{
		Options += TEXT('?');
		Options += InURL.Op[i];
	}

	AGameModeBase* GameMode = GetAuthGameMode();

	// Give the GameMode a chance to accept the login
	APlayerController* const NewPlayerController = GameMode->Login(NewPlayer, RemoteRole, *InURL.Portal, Options, UniqueId, Error);
	if (NewPlayerController == NULL)
	{
		UE_LOG(LogSpawn, Warning, TEXT("Login failed: %s"), *Error);
		return NULL;
	}

	UE_LOG(LogSpawn, Log, TEXT("%s got player %s [%s]"), *NewPlayerController->GetName(), *NewPlayer->GetName(), UniqueId.IsValid() ? *UniqueId->ToString() : TEXT("Invalid"));

	// Possess the newly-spawned player.
	NewPlayerController->NetPlayerIndex = InNetPlayerIndex;
	NewPlayerController->Role = ROLE_Authority;
	NewPlayerController->SetReplicates(RemoteRole != ROLE_None);
	if (RemoteRole == ROLE_AutonomousProxy)
	{
		NewPlayerController->SetAutonomousProxy(true);
	}
	NewPlayerController->SetPlayer(NewPlayer);
	GameMode->PostLogin(NewPlayerController);

	return NewPlayerController;
}

/*-----------------------------------------------------------------------------
	Level actor moving/placing.
-----------------------------------------------------------------------------*/

bool UWorld::FindTeleportSpot(AActor* TestActor, FVector& TestLocation, FRotator TestRotation)
{
	if( !TestActor || !TestActor->GetRootComponent() )
	{
		return true;
	}
	FVector Adjust(0.f);

	// check if fits at desired location
	if( !EncroachingBlockingGeometry(TestActor, TestLocation, TestRotation, &Adjust) )
	{
		return true;
	}

	// first do only Z
	if (!FMath::IsNearlyZero(Adjust.Z)) 
	{
		TestLocation.Z += Adjust.Z;
		if( !EncroachingBlockingGeometry(TestActor, TestLocation, TestRotation, &Adjust) )
		{
			return true;
		}
	}

	// now try just XY
	if (!FMath::IsNearlyZero(Adjust.X) || !FMath::IsNearlyZero(Adjust.Y))
	{
		const FVector OriginalTestLocation = TestLocation;
		const FVector OriginalAdjust = Adjust;
		// If initially spawning allow testing a few permutations (though this needs improvement).
		// During play only test the first adjustment, permuting axes could put the location on other sides of geometry.
		const int32 Iterations = (TestActor->HasActorBegunPlay() ? 1 : 8);
		for (int i = 0; i < Iterations; ++i)
		{
			TestLocation.X += (i < 4 ? Adjust.X : Adjust.Y) * (i % 2 == 0 ? 1 : -1);
			TestLocation.Y += (i < 4 ? Adjust.Y : Adjust.X) * (i % 4 < 2 ? 1 : -1);
			if (!EncroachingBlockingGeometry(TestActor, TestLocation, TestRotation, &Adjust))
			{
				return true;
			}

			// Restore original location and adjust, previous iterations should not affect the next test
			TestLocation = OriginalTestLocation;
			Adjust = OriginalAdjust;
		}
	}

	// now z again
	if (!FMath::IsNearlyZero(Adjust.Z))
	{
		TestLocation.Z += Adjust.Z;
		if( !EncroachingBlockingGeometry(TestActor, TestLocation, TestRotation, &Adjust) )
		{
			return true;
		}
	}

	if ( Adjust.IsNearlyZero() )
	{
		return false;
	}

	// Now try full adjustment
	TestLocation += Adjust;
	//DrawDebugSphere(this, TestLocation, 32, 10, FLinearColor::Blue, true);
	return !EncroachingBlockingGeometry(TestActor, TestLocation, TestRotation, &Adjust);
}

/** Tests shape components more efficiently than the with-adjustment case, but does less-efficient ppr-poly collision for meshes. */
static bool ComponentEncroachesBlockingGeometry_NoAdjustment(UWorld const* World, AActor const* TestActor, UPrimitiveComponent const* PrimComp, FTransform const& TestWorldTransform, const TArray<AActor*>& IgnoreActors)
{	
	float const Epsilon = CVarEncroachEpsilon.GetValueOnGameThread();
	
	if (World && PrimComp)
	{
		bool bFoundBlockingHit = false;
		
		ECollisionChannel const BlockingChannel = PrimComp->GetCollisionObjectType();
		FCollisionShape const CollisionShape = PrimComp->GetCollisionShape(-Epsilon);

		if (CollisionShape.IsBox() && (Cast<UBoxComponent>(PrimComp) == nullptr))
		{
			// we have a bounding box not for a box component, which means this was the fallback aabb
			// since we don't need the penetration info, go ahead and test the component itself for overlaps, which is more accurate
			if (PrimComp->IsRegistered())
			{
				// must be registered
				TArray<FOverlapResult> Overlaps;
				FComponentQueryParams Params(SCENE_QUERY_STAT(ComponentEncroachesBlockingGeometry_NoAdjustment), TestActor);
				FCollisionResponseParams ResponseParams;
				PrimComp->InitSweepCollisionParams(Params, ResponseParams);
				Params.AddIgnoredActors(IgnoreActors);
				return World->ComponentOverlapMultiByChannel(Overlaps, PrimComp, TestWorldTransform.GetLocation(), TestWorldTransform.GetRotation(), BlockingChannel, Params);
			}
			else
			{
				UE_LOG(LogPhysics, Log, TEXT("Components must be registered in order to be used in a ComponentOverlapMulti call. PriComp: %s TestActor: %s"), *PrimComp->GetName(), *TestActor->GetName());
				return false;
			}
		}
		else
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(ComponentEncroachesBlockingGeometry_NoAdjustment), false, TestActor);
			FCollisionResponseParams ResponseParams;
			PrimComp->InitSweepCollisionParams(Params, ResponseParams);
			Params.AddIgnoredActors(IgnoreActors);
			return World->OverlapBlockingTestByChannel(TestWorldTransform.GetLocation(), TestWorldTransform.GetRotation(), BlockingChannel, CollisionShape, Params, ResponseParams);
		}
	}

	return false;
}

/** Tests shape components less efficiently than the no-adjustment case, but does quicker aabb collision for meshes. */
static bool ComponentEncroachesBlockingGeometry_WithAdjustment(UWorld const* World, AActor const* TestActor, UPrimitiveComponent const* PrimComp, FTransform const& TestWorldTransform, FVector& OutProposedAdjustment, const TArray<AActor*>& IgnoreActors)
{
	// init our output
	OutProposedAdjustment = FVector::ZeroVector;

	float const Epsilon = CVarEncroachEpsilon.GetValueOnGameThread();

	if (World && PrimComp)
	{
		bool bFoundBlockingHit = false;
		bool bComputePenetrationAdjustment = true;
		
		TArray<FOverlapResult> Overlaps;
		ECollisionChannel const BlockingChannel = PrimComp->GetCollisionObjectType();
		FCollisionShape const CollisionShape = PrimComp->GetCollisionShape(-Epsilon);

		if (CollisionShape.IsBox() && (Cast<UBoxComponent>(PrimComp) == nullptr))
		{
			// we have a bounding box not for a box component, which means this was the fallback aabb
			// so lets test the actual component instead of it's aabb
			// note we won't get penetration adjustment but that's ok
			if (PrimComp->IsRegistered())
			{
				// must be registered
				FComponentQueryParams Params(SCENE_QUERY_STAT(ComponentEncroachesBlockingGeometry_WithAdjustment), TestActor);
				FCollisionResponseParams ResponseParams;
				PrimComp->InitSweepCollisionParams(Params, ResponseParams);
				Params.AddIgnoredActors(IgnoreActors);
				bFoundBlockingHit = World->ComponentOverlapMultiByChannel(Overlaps, PrimComp, TestWorldTransform.GetLocation(), TestWorldTransform.GetRotation(), BlockingChannel, Params);
				bComputePenetrationAdjustment = false;
			}
			else
			{
				UE_LOG(LogPhysics, Log, TEXT("Components must be registered in order to be used in a ComponentOverlapMulti call. PriComp: %s TestActor: %s"), *PrimComp->GetName(), *TestActor->GetName());
			}
		}
		else
		{
			// overlap our shape
			FCollisionQueryParams Params(SCENE_QUERY_STAT(ComponentEncroachesBlockingGeometry_WithAdjustment), false, TestActor);
			FCollisionResponseParams ResponseParams;
			PrimComp->InitSweepCollisionParams(Params, ResponseParams);
			Params.AddIgnoredActors(IgnoreActors);
			bFoundBlockingHit = World->OverlapMultiByChannel(Overlaps, TestWorldTransform.GetLocation(), TestWorldTransform.GetRotation(), BlockingChannel, CollisionShape, Params, ResponseParams);
		}

		// compute adjustment
		if (bFoundBlockingHit && bComputePenetrationAdjustment)
		{
			// if encroaching, add up all the MTDs of overlapping shapes
			FMTDResult MTDResult;
			uint32 NumBlockingHits = 0;
			for (int32 HitIdx = 0; HitIdx < Overlaps.Num(); HitIdx++)
			{
				UPrimitiveComponent* const OverlapComponent = Overlaps[HitIdx].Component.Get();
				// first determine closest impact point along each axis
				if (OverlapComponent && OverlapComponent->GetCollisionResponseToChannel(BlockingChannel) == ECR_Block)
				{
					NumBlockingHits++;
					FCollisionShape const NonShrunkenCollisionShape = PrimComp->GetCollisionShape();
					bool bSuccess = OverlapComponent->ComputePenetration(MTDResult, NonShrunkenCollisionShape, TestWorldTransform.GetLocation(), TestWorldTransform.GetRotation());
					if (bSuccess)
					{
						OutProposedAdjustment += MTDResult.Direction * MTDResult.Distance;
					}
					else
					{
						UE_LOG(LogPhysics, Log, TEXT("OverlapTest says we are overlapping, yet MTD says we're not. Something is wrong"));
						// It's not safe to use a partial result, that could push us out to an invalid location (like the other side of a wall).
						OutProposedAdjustment = FVector::ZeroVector;
						return true;
					}

					// #hack: sometimes for boxes, physx returns a 0 MTD even though it reports a contact (returns true)
					// to get around this, let's go ahead and test again with the epsilon-shrunken collision shape to see if we're really in 
					// the clear.
					if (bSuccess && FMath::IsNearlyZero(MTDResult.Distance))
					{
						FCollisionShape const ShrunkenCollisionShape = PrimComp->GetCollisionShape(-Epsilon);
						bSuccess = OverlapComponent->ComputePenetration(MTDResult, ShrunkenCollisionShape, TestWorldTransform.GetLocation(), TestWorldTransform.GetRotation());
						if (bSuccess)
						{
							OutProposedAdjustment += MTDResult.Direction * MTDResult.Distance;
						}
						else
						{
							// Ignore this overlap.
							UE_LOG(LogPhysics, Log, TEXT("OverlapTest says we are overlapping, yet MTD says we're not (with smaller shape). Ignoring this overlap."));
							NumBlockingHits--;
							continue;
						}
					}
				}
			}

			// See if we chose to invalidate all of our supposed "blocking hits".
			if (NumBlockingHits == 0)
			{
				OutProposedAdjustment = FVector::ZeroVector;
				bFoundBlockingHit = false;
			}
		}

		return bFoundBlockingHit;
	}

	return false;
}

/** Tests if the given component overlaps any blocking geometry if it were placed at the given world transform, optionally returns a suggested translation to get the component away from its overlaps. */
static bool ComponentEncroachesBlockingGeometry(UWorld const* World, AActor const* TestActor, UPrimitiveComponent const* PrimComp, FTransform const& TestWorldTransform, FVector* OutProposedAdjustment, const TArray<AActor*>& IgnoreActors)
{
	return OutProposedAdjustment
		? ComponentEncroachesBlockingGeometry_WithAdjustment(World, TestActor, PrimComp, TestWorldTransform, *OutProposedAdjustment, IgnoreActors)
		: ComponentEncroachesBlockingGeometry_NoAdjustment(World, TestActor, PrimComp, TestWorldTransform, IgnoreActors);
}


static FVector CombineAdjustments(FVector CurrentAdjustment, FVector AdjustmentToAdd)
{
	// remove the part of the new adjustment that's parallel to the current adjustment
	if (CurrentAdjustment.IsZero())
	{
		return AdjustmentToAdd;
	}

	FVector Projection = AdjustmentToAdd.ProjectOnTo(CurrentAdjustment);
	Projection = Projection.GetClampedToMaxSize(CurrentAdjustment.Size());

	FVector OrthogalAdjustmentToAdd = AdjustmentToAdd - Projection;
	return CurrentAdjustment + OrthogalAdjustmentToAdd;
}

// perf note: this is faster if ProposedAdjustment is null, since it can early out on first penetration
bool UWorld::EncroachingBlockingGeometry(AActor* TestActor, FVector TestLocation, FRotator TestRotation, FVector* ProposedAdjustment)
{
	if (TestActor == nullptr)
	{
		return false;
	}

	USceneComponent* const RootComponent = TestActor->GetRootComponent();
	if (RootComponent == nullptr)
	{
		return false;
	}

	bool bFoundEncroacher = false;

	FVector TotalAdjustment(0.f);
	FTransform const TestRootToWorld = FTransform(TestRotation, TestLocation);
	FTransform const WorldToOldRoot = RootComponent->GetComponentToWorld().Inverse();

	UMovementComponent* const MoveComponent = TestActor->FindComponentByClass<UMovementComponent>();
	if (MoveComponent && MoveComponent->UpdatedPrimitive)
	{
		// This actor has a movement component, which we interpret to mean that this actor has a primary component being swept around
		// the world, and that component is the only one we care about encroaching (since the movement code will happily embedding
		// other components in the world during movement updates)
		UPrimitiveComponent* const MovedPrimComp = MoveComponent->UpdatedPrimitive;
		if (MovedPrimComp->IsQueryCollisionEnabled())
		{
			// might not be the root, so we need to compute the transform
			FTransform const CompToRoot = MovedPrimComp->GetComponentToWorld() * WorldToOldRoot;
			FTransform const CompToNewWorld = CompToRoot * TestRootToWorld;

			TArray<AActor*> ChildActors;
			TestActor->GetAllChildActors(ChildActors);

			if (ComponentEncroachesBlockingGeometry(this, TestActor, MovedPrimComp, CompToNewWorld, ProposedAdjustment, ChildActors))
			{
				if (ProposedAdjustment == nullptr)
				{
					// don't need an adjustment and we know we are overlapping, so we can be done
					return true;
				}
				else
				{
					TotalAdjustment = *ProposedAdjustment;
				}

				bFoundEncroacher = true;
			}
		}
	}
	else
	{
		bool bFetchedChildActors = false;
		TArray<AActor*> ChildActors;

		// This actor does not have a movement component, so we'll assume all components are potentially important to keep out of the world
		UPrimitiveComponent* const RootPrimComp = Cast<UPrimitiveComponent>(RootComponent);
		if (RootPrimComp && RootPrimComp->IsQueryCollisionEnabled())
		{
			TestActor->GetAllChildActors(ChildActors);
			bFetchedChildActors = true;

			if (ComponentEncroachesBlockingGeometry(this, TestActor, RootPrimComp, TestRootToWorld, ProposedAdjustment, ChildActors))
			{
				if (ProposedAdjustment == nullptr)
				{
					// don't need an adjustment and we know we are overlapping, so we can be done
					return true;
				}
				else
				{
					TotalAdjustment = *ProposedAdjustment;
				}

				bFoundEncroacher = true;
			}
		}

		// now test all colliding children for encroachment
		TArray<USceneComponent*> Children;
		RootComponent->GetChildrenComponents(true, Children);

		for (USceneComponent* Child : Children)
		{
			if (Child->IsQueryCollisionEnabled())
			{
				UPrimitiveComponent* const PrimComp = Cast<UPrimitiveComponent>(Child);
				if (PrimComp)
				{
					FTransform const CompToRoot = Child->GetComponentToWorld() * WorldToOldRoot;
					FTransform const CompToNewWorld = CompToRoot * TestRootToWorld;

					if (!bFetchedChildActors)
					{
						TestActor->GetAllChildActors(ChildActors);
						bFetchedChildActors = true;
					}

					if (ComponentEncroachesBlockingGeometry(this, TestActor, PrimComp, CompToNewWorld, ProposedAdjustment, ChildActors))
					{
						if (ProposedAdjustment == nullptr)
						{
							// don't need an adjustment and we know we are overlapping, so we can be done
							return true;
						}

						TotalAdjustment = CombineAdjustments(TotalAdjustment, *ProposedAdjustment);
						bFoundEncroacher = true;
					}
				}
			}
		}
	}

	// copy over total adjustment
	if (ProposedAdjustment)
	{
		*ProposedAdjustment = TotalAdjustment;
	}

	return bFoundEncroacher;
}


void UWorld::LoadSecondaryLevels(bool bForce, TSet<FString>* CookedPackages)
{
	check( GIsEditor );

	// streamingServer
	// Only load secondary levels in the Editor, and not for commandlets.
	if( (!IsRunningCommandlet() || bForce)
	// Don't do any work for world info actors that are part of secondary levels being streamed in! 
	&& !IsAsyncLoading())
	{
		for( int32 LevelIndex=0; LevelIndex<StreamingLevels.Num(); LevelIndex++ )
		{
			ULevelStreaming* const StreamingLevel = StreamingLevels[LevelIndex];
			if( StreamingLevel )
			{
				bool bAlreadyCooked = false;
				// If we are cooking don't cook sub levels multiple times if they've already been cooked
				FString PackageFilename;
				const FString StreamingLevelWorldAssetPackageName = StreamingLevel->GetWorldAssetPackageName();
				if (CookedPackages)
				{
					if (FPackageName::DoesPackageExist(StreamingLevelWorldAssetPackageName, NULL, &PackageFilename))
					{
						PackageFilename = FPaths::ConvertRelativePathToFull(PackageFilename);
						bAlreadyCooked |= CookedPackages->Contains( PackageFilename );
					}
				}


				bool bAlreadyLoaded = false;
				UPackage* LevelPackage = FindObject<UPackage>(NULL, *StreamingLevelWorldAssetPackageName,true);
				// don't need to do any extra work if the level is already loaded
				if ( LevelPackage && LevelPackage->IsFullyLoaded() ) 
				{
					bAlreadyLoaded = true;
				}

				if ( !bAlreadyCooked && !bAlreadyLoaded )
				{
					bool bLoadedLevelPackage = false;
					const FName StreamingLevelWorldAssetPackageFName = StreamingLevel->GetWorldAssetPackageFName();
					// Load the package and find the world object.
					if( FPackageName::IsShortPackageName(StreamingLevelWorldAssetPackageFName) == false )
					{
						ULevel::StreamedLevelsOwningWorld.Add(StreamingLevelWorldAssetPackageFName, this);
						LevelPackage = LoadPackage( NULL, *StreamingLevelWorldAssetPackageName, LOAD_None );
						ULevel::StreamedLevelsOwningWorld.Remove(StreamingLevelWorldAssetPackageFName);

						if( LevelPackage )
						{
							bLoadedLevelPackage = true;

							// Find the world object in the loaded package.
							UWorld* LoadedWorld	= UWorld::FindWorldInPackage(LevelPackage);
							// If the world was not found, it could be a redirector to a world. If so, follow it to the destination world.
							if (!LoadedWorld)
							{
								LoadedWorld = UWorld::FollowWorldRedirectorInPackage(LevelPackage);
							}
							check(LoadedWorld);

							if ( !LevelPackage->IsFullyLoaded() )
							{
								// LoadedWorld won't be serialized as there's a BeginLoad on the stack so we manually serialize it here.
								check( LoadedWorld->GetLinker() );
								LoadedWorld->GetLinker()->Preload( LoadedWorld );
							}


							// Keep reference to prevent garbage collection.
							check( LoadedWorld->PersistentLevel );

							LoadedWorld->PersistentLevel->HandleLegacyMapBuildData();

							ULevel* NewLoadedLevel = LoadedWorld->PersistentLevel;
							NewLoadedLevel->OwningWorld = this;

							StreamingLevel->SetLoadedLevel(NewLoadedLevel);
						}
					}
					else
					{
						UE_LOG(LogSpawn, Warning, TEXT("Streaming level uses short package name (%s). Level will not be loaded."), *StreamingLevelWorldAssetPackageName);
					}

					// Remove this level object if the file couldn't be found.
					if ( !bLoadedLevelPackage )
					{		
						StreamingLevels.RemoveAt( LevelIndex-- );
						MarkPackageDirty();
					}
				}
			}
		}
	}
}

/** Utility for returning the ULevelStreaming object for a particular sub-level, specified by package name */
ULevelStreaming* UWorld::GetLevelStreamingForPackageName(FName InPackageName)
{
	// iterate over each level streaming object
	for( int32 LevelIndex=0; LevelIndex<StreamingLevels.Num(); LevelIndex++ )
	{
		ULevelStreaming* LevelStreaming = StreamingLevels[LevelIndex];
		// see if name matches
		if(LevelStreaming && LevelStreaming->GetWorldAssetPackageFName() == InPackageName)
		{
			// it doesn't, return this one
			return LevelStreaming;
		}
	}

	// failed to find one
	return NULL;
}

#if WITH_EDITOR


void UWorld::RefreshStreamingLevels( const TArray<class ULevelStreaming*>& InLevelsToRefresh )
{
	// Reassociate levels in case we changed streaming behavior. Editor-only!
	if( GIsEditor )
	{
		// Load and associate levels if necessary.
		FlushLevelStreaming();

		// Remove all currently visible levels.
		for( int32 LevelIndex=0; LevelIndex<InLevelsToRefresh.Num(); LevelIndex++ )
		{
			ULevelStreaming* StreamingLevel = InLevelsToRefresh[LevelIndex];
			ULevel* LoadedLevel = StreamingLevel ? StreamingLevel->GetLoadedLevel() : nullptr;

			if( LoadedLevel &&
				LoadedLevel->bIsVisible )
			{
				RemoveFromWorld( LoadedLevel );
			}
		}

		// Load and associate levels if necessary.
		FlushLevelStreaming();

		// Update the level browser so it always contains valid data
		FEditorSupportDelegates::WorldChange.Broadcast();
	}
}

void UWorld::RefreshStreamingLevels()
{
	RefreshStreamingLevels( StreamingLevels );
}

void UWorld::IssueEditorLoadWarnings()
{
	float TotalLoadTimeFromFixups = 0;

	for (int32 LevelIndex = 0; LevelIndex < Levels.Num(); LevelIndex++)
	{
		ULevel* Level = Levels[LevelIndex];

		if (Level->FixupOverrideVertexColorsCount > 0)
		{
			TotalLoadTimeFromFixups += Level->FixupOverrideVertexColorsTime;
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("LoadTime"), FText::FromString(FString::Printf(TEXT("%.1fs"), Level->FixupOverrideVertexColorsTime)));
			Arguments.Add(TEXT("NumComponents"), FText::FromString(FString::Printf(TEXT("%u"), Level->FixupOverrideVertexColorsCount)));
			Arguments.Add(TEXT("LevelName"), FText::FromString(Level->GetOutermost()->GetName()));
			
			FMessageLog("MapCheck").Info()
				->AddToken(FTextToken::Create(FText::Format( LOCTEXT( "MapCheck_Message_RepairedPaintedVertexColors", "Repaired painted vertex colors in {LoadTime} for {NumComponents} components in {LevelName}.  Resave map to fix." ), Arguments ) ))
				->AddToken(FMapErrorToken::Create(FMapErrors::RepairedPaintedVertexColors));
		}
	}

	if (TotalLoadTimeFromFixups > 0)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("LoadTime"), FText::FromString(FString::Printf(TEXT("%.1fs"), TotalLoadTimeFromFixups)));
			
		FMessageLog("MapCheck").Warning()
			->AddToken(FTextToken::Create(FText::Format( LOCTEXT( "MapCheck_Message_SpentXRepairingPaintedVertexColors", "Spent {LoadTime} repairing painted vertex colors due to static mesh re-imports!  This will happen every load until the maps are resaved." ), Arguments ) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::RepairedPaintedVertexColors));
	}
}

#endif // WITH_EDITOR


AAudioVolume* UWorld::GetAudioSettings( const FVector& ViewLocation, FReverbSettings* OutReverbSettings, FInteriorSettings* OutInteriorSettings )
{
	// Find the highest priority volume encompassing the current view location.
	for (AAudioVolume* Volume : AudioVolumes)
	{
		// Volume encompasses, break out of loop.
		if (Volume->GetEnabled() && Volume->EncompassesPoint(ViewLocation))
		{
			if( OutReverbSettings )
			{
				*OutReverbSettings = Volume->GetReverbSettings();
			}

			if( OutInteriorSettings )
			{
				*OutInteriorSettings = Volume->GetInteriorSettings();
			}
			return Volume;
		}
	}

	// If first level is a FakePersistentLevel (see CommitMapChange for more info)
	// then use its world info for reverb settings
	AWorldSettings* CurrentWorldSettings = GetWorldSettings(true);

	if( OutReverbSettings )
	{
		*OutReverbSettings = CurrentWorldSettings->DefaultReverbSettings;
	}

	if( OutInteriorSettings )
	{
		*OutInteriorSettings = CurrentWorldSettings->DefaultAmbientZoneSettings;
	}

	return nullptr;
}

void UWorld::SetAudioDeviceHandle(const uint32 InAudioDeviceHandle)
{
	AudioDeviceHandle = InAudioDeviceHandle;
}

FAudioDevice* UWorld::GetAudioDevice()
{
	FAudioDevice* AudioDevice = nullptr;
	if (GEngine)
	{
		class FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
		if (AudioDeviceManager != nullptr)
		{
			AudioDevice = AudioDeviceManager->GetAudioDevice(AudioDeviceHandle);
			if (AudioDevice == nullptr)
			{
				AudioDevice = GEngine->GetMainAudioDevice();
			}
		}
	}
	return AudioDevice;
}

/**
 * Sets bMapNeedsLightingFullyRebuild to the specified value.  Marks the worldsettings package dirty if the value changed.
 *
 * @param	bInMapNeedsLightingFullyRebuild			The new value.
 */
void UWorld::SetMapNeedsLightingFullyRebuilt(int32 InNumLightingUnbuiltObjects)
{
	static const TConsoleVariableData<int32>* AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnGameThread() != 0);

	AWorldSettings* WorldSettings = GetWorldSettings();
	if (bAllowStaticLighting && WorldSettings && !WorldSettings->bForceNoPrecomputedLighting)
	{
		check(IsInGameThread());
		if (NumLightingUnbuiltObjects != InNumLightingUnbuiltObjects && (NumLightingUnbuiltObjects == 0 || InNumLightingUnbuiltObjects == 0))
		{
			// Save the lighting invalidation for transactions.
			Modify(false);
		}

		NumLightingUnbuiltObjects = InNumLightingUnbuiltObjects;

		// Update last time unbuilt lighting was encountered.
		if (NumLightingUnbuiltObjects > 0)
		{
			LastTimeUnbuiltLightingWasEncountered = FApp::GetCurrentTime();
		}
	}
}

#undef LOCTEXT_NAMESPACE
