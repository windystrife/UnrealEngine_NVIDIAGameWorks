// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelSequencePlayer.h"
#include "GameFramework/Actor.h"
#include "MovieScene.h"
#include "Misc/CoreDelegates.h"
#include "EngineGlobals.h"
#include "Camera/PlayerCameraManager.h"
#include "UObject/Package.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Tickable.h"
#include "Engine/LevelScriptActor.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieSceneSubSection.h"
#include "LevelSequenceSpawnRegister.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LocalPlayer.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "LevelSequenceActor.h"


/* ULevelSequencePlayer structors
 *****************************************************************************/

ULevelSequencePlayer::ULevelSequencePlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SpawnRegister = MakeShareable(new FLevelSequenceSpawnRegister);
}


/* ULevelSequencePlayer interface
 *****************************************************************************/

ULevelSequencePlayer* ULevelSequencePlayer::CreateLevelSequencePlayer(UObject* WorldContextObject, ULevelSequence* InLevelSequence, FMovieSceneSequencePlaybackSettings Settings, ALevelSequenceActor*& OutActor)
{
	if (InLevelSequence == nullptr)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.bAllowDuringConstructionScript = true;
	ALevelSequenceActor* Actor = World->SpawnActor<ALevelSequenceActor>(SpawnParams);

	Actor->PlaybackSettings = Settings;
	Actor->LevelSequence = InLevelSequence;

	Actor->InitializePlayer();
	OutActor = Actor;

	return Actor->SequencePlayer;
}

/* ULevelSequencePlayer implementation
 *****************************************************************************/

void ULevelSequencePlayer::Initialize(ULevelSequence* InLevelSequence, UWorld* InWorld, const FMovieSceneSequencePlaybackSettings& Settings)
{
	World = InWorld;
	UMovieSceneSequencePlayer::Initialize(InLevelSequence, Settings);
}

bool ULevelSequencePlayer::CanPlay() const
{
	return World.IsValid();
}

void ULevelSequencePlayer::OnStartedPlaying()
{
	EnableCinematicMode(true);
}

void ULevelSequencePlayer::OnStopped()
{
	EnableCinematicMode(false);

	AActor* LevelSequenceActor = Cast<AActor>(GetOuter());
	if (LevelSequenceActor == nullptr)
	{
		return;
	}

	for (FObjectKey WeakActor : PrerequisiteActors)
	{
		AActor* Actor = Cast<AActor>(WeakActor.ResolveObjectPtr());
		if (Actor)
		{
			for (UActorComponent* Component : Actor->GetComponents())
			{
				Component->PrimaryComponentTick.RemovePrerequisite(LevelSequenceActor, LevelSequenceActor->PrimaryActorTick);
			}

			Actor->PrimaryActorTick.RemovePrerequisite(LevelSequenceActor, LevelSequenceActor->PrimaryActorTick);
		}
	}
	PrerequisiteActors.Reset();
}

/* IMovieScenePlayer interface
 *****************************************************************************/

void ULevelSequencePlayer::UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut)
{
	if (World == nullptr || World->GetGameInstance() == nullptr)
	{
		return;
	}

	// skip missing player controller
	APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController();

	if (PC == nullptr)
	{
		return;
	}

	// skip same view target
	AActor* ViewTarget = PC->GetViewTarget();

	// save the last view target so that it can be restored when the camera object is null
	if (!LastViewTarget.IsValid())
	{
		LastViewTarget = ViewTarget;
		if (PC->GetLocalPlayer())
		{
			LastAspectRatioAxisConstraint = PC->GetLocalPlayer()->AspectRatioAxisConstraint;
		}
	}

	UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromRuntimeObject(CameraObject);
	
	CachedCameraComponent = CameraComponent;

	if (CameraObject == ViewTarget)
	{
		if ( bJumpCut )
		{
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->bGameCameraCutThisFrame = true;
			}

			if (CameraComponent)
			{
				CameraComponent->NotifyCameraCut();
			}
		}
		return;
	}

	// skip unlocking if the current view target differs
	AActor* UnlockIfCameraActor = Cast<AActor>(UnlockIfCameraObject);

	// if unlockIfCameraActor is valid, release lock if currently locked to object
	if (CameraObject == nullptr && UnlockIfCameraActor != nullptr && UnlockIfCameraActor != ViewTarget)
	{
		return;
	}

	// override the player controller's view target
	AActor* CameraActor = Cast<AActor>(CameraObject);

	// if the camera object is null, use the last view target so that it is restored to the state before the sequence takes control
	if (CameraActor == nullptr)
	{
		CameraActor = LastViewTarget.Get();
	}

	FViewTargetTransitionParams TransitionParams;
	PC->SetViewTarget(CameraActor, TransitionParams);

	if (PC->GetLocalPlayer())
	{
		PC->GetLocalPlayer()->AspectRatioAxisConstraint = EAspectRatioAxisConstraint::AspectRatio_MaintainXFOV;
	}

	if (CameraComponent)
	{
		CameraComponent->NotifyCameraCut();
	}

	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->bClientSimulatingViewTarget = (CameraActor != nullptr);
		PC->PlayerCameraManager->bGameCameraCutThisFrame = true;
	}

	if (OnCameraCut.IsBound())
	{
		OnCameraCut.Broadcast(CameraComponent);
	}
}

void ULevelSequencePlayer::NotifyBindingUpdate(const FGuid& InGuid, FMovieSceneSequenceIDRef InSequenceID, TArrayView<TWeakObjectPtr<>> Objects)
{
	AActor* LevelSequenceActor = Cast<AActor>(GetOuter());
	if (LevelSequenceActor == nullptr)
	{
		return;
	}

	for (TWeakObjectPtr<> WeakObject : Objects)
	{
		if (AActor* Actor = Cast<AActor>(WeakObject.Get()))
		{
			for (UActorComponent* Component : Actor->GetComponents())
			{
				Component->PrimaryComponentTick.AddPrerequisite(LevelSequenceActor, LevelSequenceActor->PrimaryActorTick);
			}

			Actor->PrimaryActorTick.AddPrerequisite(LevelSequenceActor, LevelSequenceActor->PrimaryActorTick);
			PrerequisiteActors.Add(Actor);
		}
	}
}

UObject* ULevelSequencePlayer::GetPlaybackContext() const
{
	return World.Get();
}

TArray<UObject*> ULevelSequencePlayer::GetEventContexts() const
{
	TArray<UObject*> EventContexts;
	if (World.IsValid())
	{
		GetEventContexts(*World, EventContexts);
	}

	for (UObject* Object : AdditionalEventReceivers)
	{
		if (Object)
		{
			EventContexts.Add(Object);
		}
	}
	return EventContexts;
}

void ULevelSequencePlayer::GetEventContexts(UWorld& InWorld, TArray<UObject*>& OutContexts)
{
	if (InWorld.GetLevelScriptActor())
	{
		OutContexts.Add(InWorld.GetLevelScriptActor());
	}

	for (ULevelStreaming* StreamingLevel : InWorld.StreamingLevels)
	{
		if (StreamingLevel->GetLevelScriptActor())
		{
			OutContexts.Add(StreamingLevel->GetLevelScriptActor());
		}
	}
}

void ULevelSequencePlayer::TakeFrameSnapshot(FLevelSequencePlayerSnapshot& OutSnapshot) const
{
	if (!ensure(Sequence))
	{
		return;
	}

	const float StartTimeWithoutWarmupFrames = SnapshotOffsetTime.IsSet() ? StartTime + SnapshotOffsetTime.GetValue() : StartTime;

	// Use the actual last evaluation time as per the play position, which accounts for fixed time step offsetting
	const float CurrentTime = PlayPosition.GetLastPlayEvalPostition().Get(StartTimeWithoutWarmupFrames + TimeCursorPosition);

	OutSnapshot.Settings = SnapshotSettings;

	OutSnapshot.MasterTime = CurrentTime;
	OutSnapshot.MasterName = FText::FromString(Sequence->GetName());

	OutSnapshot.CurrentShotName = OutSnapshot.MasterName;
	OutSnapshot.CurrentShotLocalTime = CurrentTime;
	OutSnapshot.CameraComponent = CachedCameraComponent.IsValid() ? CachedCameraComponent.Get() : nullptr;
	OutSnapshot.ShotID = MovieSceneSequenceID::Invalid;

	UMovieSceneCinematicShotTrack* ShotTrack = Sequence->GetMovieScene()->FindMasterTrack<UMovieSceneCinematicShotTrack>();
	if (ShotTrack)
	{
		UMovieSceneCinematicShotSection* ActiveShot = nullptr;
		for (UMovieSceneSection* Section : ShotTrack->GetAllSections())
		{
			if (!ensure(Section))
			{
				continue;
			}

			// It's unfortunate that we have to copy the logic of UMovieSceneCinematicShotTrack::GetRowCompilerRules() to some degree here, but there's no better way atm
			bool bThisShotIsActive = Section->IsActive();

			TRange<float> SectionRange = Section->GetRange();
			bThisShotIsActive = bThisShotIsActive && SectionRange.Contains(CurrentTime);

			if (bThisShotIsActive && ActiveShot)
			{
				if (Section->GetRowIndex() < ActiveShot->GetRowIndex())
				{
					bThisShotIsActive = true;
				}
				else if (Section->GetRowIndex() == ActiveShot->GetRowIndex())
				{
					// On the same row - latest start wins
					bThisShotIsActive = TRangeBound<float>::MaxLower(SectionRange.GetLowerBound(), ActiveShot->GetRange().GetLowerBound()) == SectionRange.GetLowerBound();
				}
				else
				{
					bThisShotIsActive = false;
				}
			}

			if (bThisShotIsActive)
			{
				ActiveShot = Cast<UMovieSceneCinematicShotSection>(Section);
			}
		}

		if (ActiveShot)
		{
			// Assume that shots with no sequence start at 0.
			const float ShotLowerBound = ActiveShot->GetSequence() 
				? ActiveShot->GetSequence()->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue() 
				: 0;
			const float ShotOffset = ActiveShot->Parameters.StartOffset + ShotLowerBound - ActiveShot->GetPreRollTime();
			const float ShotPosition = ShotOffset + (CurrentTime - (ActiveShot->GetStartTime() - ActiveShot->GetPreRollTime())) / ActiveShot->Parameters.TimeScale;

			OutSnapshot.CurrentShotName = ActiveShot->GetShotDisplayName();
			OutSnapshot.CurrentShotLocalTime = ShotPosition;
			OutSnapshot.ShotID = ActiveShot->GetSequenceID();
		}
	}
}

void ULevelSequencePlayer::EnableCinematicMode(bool bEnable)
{
	// iterate through the controller list and set cinematic mode if necessary
	bool bNeedsCinematicMode = PlaybackSettings.bDisableMovementInput || PlaybackSettings.bDisableLookAtInput || PlaybackSettings.bHidePlayer || PlaybackSettings.bHideHud;

	if (bNeedsCinematicMode)
	{
		if (World.IsValid())
		{
			for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				APlayerController *PC = Iterator->Get();
				if (PC->IsLocalController())
				{
					PC->SetCinematicMode(bEnable, PlaybackSettings.bHidePlayer, PlaybackSettings.bHideHud, PlaybackSettings.bDisableMovementInput, PlaybackSettings.bDisableLookAtInput);
				}
			}
		}
	}
}

