// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "Engine/Engine.h"
#include "Misc/RuntimeErrors.h"

bool FMovieSceneSequencePlaybackSettings::SerializeFromMismatchedTag( const FPropertyTag& Tag, FArchive& Ar )
{
	if (Tag.Type == NAME_StructProperty && Tag.StructName == "LevelSequencePlaybackSettings")
	{
		StaticStruct()->SerializeItem(Ar, this, nullptr);
		return true;
	}

	return false;
}

UMovieSceneSequencePlayer::UMovieSceneSequencePlayer(const FObjectInitializer& Init)
	: Super(Init)
	, Status(EMovieScenePlayerStatus::Stopped)
	, bReversePlayback(false)
	, bIsEvaluating(false)
	, Sequence(nullptr)
	, TimeCursorPosition(0.0f)
	, StartTime(0.f)
	, EndTime(0.f)
	, CurrentNumLoops(0)
{
}

UMovieSceneSequencePlayer::~UMovieSceneSequencePlayer()
{
	if (GEngine && OldMaxTickRate.IsSet())
	{
		GEngine->SetMaxFPS(OldMaxTickRate.GetValue());
	}
}

EMovieScenePlayerStatus::Type UMovieSceneSequencePlayer::GetPlaybackStatus() const
{
	return Status;
}

FMovieSceneSpawnRegister& UMovieSceneSequencePlayer::GetSpawnRegister()
{
	return SpawnRegister.IsValid() ? *SpawnRegister : IMovieScenePlayer::GetSpawnRegister();
}

void UMovieSceneSequencePlayer::ResolveBoundObjects(const FGuid& InBindingId, FMovieSceneSequenceID SequenceID, UMovieSceneSequence& InSequence, UObject* ResolutionContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	bool bAllowDefault = PlaybackSettings.BindingOverrides ? PlaybackSettings.BindingOverrides->LocateBoundObjects(InBindingId, SequenceID, OutObjects) : true;

	if (bAllowDefault)
	{
		InSequence.LocateBoundObjects(InBindingId, ResolutionContext, OutObjects);
	}
}

void UMovieSceneSequencePlayer::Play()
{
	bReversePlayback = false;
	PlayInternal();
}

void UMovieSceneSequencePlayer::PlayReverse()
{
	bReversePlayback = true;
	PlayInternal();
}

void UMovieSceneSequencePlayer::ChangePlaybackDirection()
{
	bReversePlayback = !bReversePlayback;
	PlayInternal();
}

void UMovieSceneSequencePlayer::PlayLooping(int32 NumLoops)
{
	PlaybackSettings.LoopCount = NumLoops;
	PlayInternal();
}

void UMovieSceneSequencePlayer::PlayInternal()
{
	if (!IsPlaying())
	{
		// Start playing
		StartPlayingNextTick();

		// Update now
		bPendingFirstUpdate = false;
		
		if (PlaybackSettings.bRestoreState)
		{
			PreAnimatedState.EnableGlobalCapture();
		}

		UMovieSceneSequence* MovieSceneSequence = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
		const bool bHasValidMovieScene = (MovieSceneSequence && MovieSceneSequence->GetMovieScene());
		TOptional<float> FixedFrameInterval = bHasValidMovieScene ? MovieSceneSequence->GetMovieScene()->GetOptionalFixedFrameInterval() : TOptional<float>();

		if (FixedFrameInterval.IsSet() && bHasValidMovieScene && MovieSceneSequence->GetMovieScene()->GetForceFixedFrameIntervalPlayback())
		{
			OldMaxTickRate = GEngine->GetMaxFPS();
			GEngine->SetMaxFPS(1.f / FixedFrameInterval.GetValue());
		}

		if (!PlayPosition.GetPreviousPosition().IsSet() || PlayPosition.GetPreviousPosition().GetValue() != GetSequencePosition())
		{
			// Ensure we're at the current sequence position
			PlayPosition.JumpTo(GetSequencePosition(), FixedFrameInterval);

			// We pass the range of PlayTo here in order to correctly update the last evaluated time in the playposition
			UpdateMovieSceneInstance(PlayPosition.PlayTo(GetSequencePosition(), FixedFrameInterval));
		}

		if (bReversePlayback)
		{
			if (OnPlayReverse.IsBound())
			{
				OnPlayReverse.Broadcast();
			}
		}
		else
		{
			if (OnPlay.IsBound())
			{
				OnPlay.Broadcast();
			}
		}
	}
}

void UMovieSceneSequencePlayer::StartPlayingNextTick()
{
	if (IsPlaying() || !Sequence || !CanPlay())
	{
		return;
	}

	// @todo Sequencer playback: Should we recreate the instance every time?
	// We must not recreate the instance since it holds stateful information (such as which objects it has spawned). Recreating the instance would break any 
	// @todo: Is this still the case now that eval state is stored (correctly) in the player?
	if (!RootTemplateInstance.IsValid())
	{
		RootTemplateInstance.Initialize(*Sequence, *this);
	}

	OnStartedPlaying();

	bPendingFirstUpdate = true;
	Status = EMovieScenePlayerStatus::Playing;
}

void UMovieSceneSequencePlayer::Pause()
{
	if (IsPlaying())
	{
		if (bIsEvaluating)
		{
			LatentActions.Add(ELatentAction::Pause);
			return;
		}

		Status = EMovieScenePlayerStatus::Paused;

		// Evaluate the sequence at its current time, with a status of 'stopped' to ensure that animated state pauses correctly
		{
			bIsEvaluating = true;

			UMovieSceneSequence* MovieSceneSequence = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
			TOptional<float> FixedFrameInterval = MovieSceneSequence->GetMovieScene() ? MovieSceneSequence->GetMovieScene()->GetOptionalFixedFrameInterval() : TOptional<float>();

			if (!PlayPosition.GetPreviousPosition().IsSet() || PlayPosition.GetPreviousPosition().GetValue() != GetSequencePosition())
			{
				FMovieSceneEvaluationRange Range = PlayPosition.JumpTo(GetSequencePosition(), FixedFrameInterval);

				const FMovieSceneContext Context(Range, EMovieScenePlayerStatus::Stopped);
				RootTemplateInstance.Evaluate(Context, *this);
			}

			bIsEvaluating = false;
		}

		ApplyLatentActions();

		if (OnPause.IsBound())
		{
			OnPause.Broadcast();
		}
	}
}

void UMovieSceneSequencePlayer::Scrub()
{
	// @todo Sequencer playback: Should we recreate the instance every time?
	// We must not recreate the instance since it holds stateful information (such as which objects it has spawned). Recreating the instance would break any 
	// @todo: Is this still the case now that eval state is stored (correctly) in the player?
	if (ensureAsRuntimeWarning(Sequence != nullptr))
	{
		if (!RootTemplateInstance.IsValid())
		{
			RootTemplateInstance.Initialize(*Sequence, *this);
		}
	}

	Status = EMovieScenePlayerStatus::Scrubbing;
}

void UMovieSceneSequencePlayer::Stop()
{
	if (IsPlaying() || IsPaused())
	{
		if (bIsEvaluating)
		{
			LatentActions.Add(ELatentAction::Stop);
			return;
		}

		Status = EMovieScenePlayerStatus::Stopped;
		TimeCursorPosition = bReversePlayback ? GetLength() : 0.f;
		CurrentNumLoops = 0;

		if (PlaybackSettings.bRestoreState)
		{
			RestorePreAnimatedState();
		}

		RootTemplateInstance.Finish(*this);

		if (OldMaxTickRate.IsSet())
		{
			GEngine->SetMaxFPS(OldMaxTickRate.GetValue());
		}

		OnStopped();

		if (OnStop.IsBound())
		{
			OnStop.Broadcast();
		}
	}
}

void UMovieSceneSequencePlayer::GoToEndAndStop()
{
	SetPlaybackPosition(GetLength());

	Stop();
}

float UMovieSceneSequencePlayer::GetPlaybackPosition() const
{
	return TimeCursorPosition;
}

void UMovieSceneSequencePlayer::SetPlaybackPosition(float NewPlaybackPosition)
{
	UpdateTimeCursorPosition(NewPlaybackPosition);
}

void UMovieSceneSequencePlayer::JumpToPosition(float NewPlaybackPosition)
{
	UpdateTimeCursorPosition(NewPlaybackPosition, EMovieScenePlayerStatus::Scrubbing);
}

bool UMovieSceneSequencePlayer::IsPlaying() const
{
	return Status == EMovieScenePlayerStatus::Playing;
}

bool UMovieSceneSequencePlayer::IsPaused() const
{
	return Status == EMovieScenePlayerStatus::Paused;
}

float UMovieSceneSequencePlayer::GetLength() const
{
	return EndTime - StartTime;
}

float UMovieSceneSequencePlayer::GetPlayRate() const
{
	return PlaybackSettings.PlayRate;
}

void UMovieSceneSequencePlayer::SetPlayRate(float PlayRate)
{
	PlaybackSettings.PlayRate = PlayRate;
}

void UMovieSceneSequencePlayer::SetPlaybackRange( const float NewStartTime, const float NewEndTime )
{
	StartTime = NewStartTime;
	EndTime = FMath::Max(NewEndTime, StartTime);

	TimeCursorPosition = FMath::Clamp(TimeCursorPosition, 0.f, GetLength());
}

bool UMovieSceneSequencePlayer::ShouldStopOrLoop(float NewPosition) const
{
	bool bShouldStopOrLoop = false;
	if (IsPlaying())
	{
		if (!bReversePlayback)
		{
			bShouldStopOrLoop = NewPosition >= GetLength();
		}
		else
		{
			bShouldStopOrLoop = NewPosition < 0.f;
		}
	}

	return bShouldStopOrLoop;
}

void UMovieSceneSequencePlayer::Initialize(UMovieSceneSequence* InSequence, const FMovieSceneSequencePlaybackSettings& InSettings)
{
	check(InSequence);
	
	Sequence = InSequence;
	PlaybackSettings = InSettings;

	if (UMovieScene* MovieScene = Sequence->GetMovieScene())
	{
		TRange<float> PlaybackRange = MovieScene->GetPlaybackRange();
		SetPlaybackRange(PlaybackRange.GetLowerBoundValue(), PlaybackRange.GetUpperBoundValue());
	}

	TimeCursorPosition = PlaybackSettings.bRandomStartTime ? FMath::FRand() * 0.99f * GetLength() : FMath::Clamp(PlaybackSettings.StartTime, 0.f, GetLength());

	RootTemplateInstance.Initialize(*Sequence, *this);

	// Ensure everything is set up, ready for playback
	Stop();
}

void UMovieSceneSequencePlayer::Update(const float DeltaSeconds)
{
	if (IsPlaying())
	{
		float PlayRate = bReversePlayback ? -PlaybackSettings.PlayRate : PlaybackSettings.PlayRate;
		UpdateTimeCursorPosition(TimeCursorPosition + DeltaSeconds * PlayRate);
	}
}

void UMovieSceneSequencePlayer::UpdateTimeCursorPosition(float NewPosition, TOptional<EMovieScenePlayerStatus::Type> OptionalStatus)
{
	float Length = GetLength();

	UMovieSceneSequence* MovieSceneSequence = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
	TOptional<float> FixedFrameInterval = (MovieSceneSequence && MovieSceneSequence->GetMovieScene()) ? MovieSceneSequence->GetMovieScene()->GetOptionalFixedFrameInterval() : TOptional<float>();

	if (bPendingFirstUpdate)
	{
		NewPosition = TimeCursorPosition;
		bPendingFirstUpdate = false;
	}

	if (ShouldStopOrLoop(NewPosition))
	{
		// loop playback
		if (PlaybackSettings.LoopCount < 0 || CurrentNumLoops < PlaybackSettings.LoopCount)
		{
			++CurrentNumLoops;
			const float Overplay = FMath::Fmod(NewPosition, Length);
			TimeCursorPosition = Overplay < 0 ? Length + Overplay : Overplay;
			
			PlayPosition.Reset(Overplay < 0 ? Length : 0.f);
			FMovieSceneEvaluationRange Range = PlayPosition.PlayTo(GetSequencePosition(), FixedFrameInterval);

			if (SpawnRegister.IsValid())
			{
				SpawnRegister->ForgetExternallyOwnedSpawnedObjects(State, *this);
			}

			const bool bHasJumped = true;

			UpdateMovieSceneInstance(Range, OptionalStatus, bHasJumped);

			OnLooped();
		}

		// stop playback
		else
		{
			FMovieSceneEvaluationRange Range = PlayPosition.PlayTo(NewPosition + StartTime, FixedFrameInterval);
			UpdateMovieSceneInstance(Range, OptionalStatus);

			Stop();

			// When playback stops naturally, the time cursor is put at the boundary that was crossed to make ping-pong playback easy
			TimeCursorPosition = bReversePlayback ? 0.f : GetLength();
			PlayPosition.Reset(TimeCursorPosition);

			if (OnFinished.IsBound())
			{
				OnFinished.Broadcast();
			}
		}
	}
	else
	{
		// Just update the time and sequence
		TimeCursorPosition = NewPosition;

		FMovieSceneEvaluationRange Range = PlayPosition.PlayTo(NewPosition + StartTime, FixedFrameInterval);
		UpdateMovieSceneInstance(Range, OptionalStatus);
	}
}

void UMovieSceneSequencePlayer::UpdateMovieSceneInstance(FMovieSceneEvaluationRange InRange, TOptional<EMovieScenePlayerStatus::Type> OptionalStatus, bool bHasJumped)
{
	bIsEvaluating = true;

	FMovieSceneContext Context(InRange, OptionalStatus.Get(GetPlaybackStatus()));
	Context.SetHasJumped(bHasJumped);

	RootTemplateInstance.Evaluate(Context, *this);

#if WITH_EDITOR
	OnMovieSceneSequencePlayerUpdate.Broadcast(*this, Context.GetTime(), Context.GetPreviousTime());
#endif
	bIsEvaluating = false;

	ApplyLatentActions();
}

void UMovieSceneSequencePlayer::ApplyLatentActions()
{
	// Swap to a stack array to ensure no reentrancy if we evaluate during a pause, for instance
	TArray<ELatentAction> TheseActions;
	Swap(TheseActions, LatentActions);

	for (ELatentAction LatentAction : TheseActions)
	{
		switch(LatentAction)
		{
		case ELatentAction::Stop:	Stop(); break;
		case ELatentAction::Pause:	Pause(); break;
		}
	}
}

TArray<UObject*> UMovieSceneSequencePlayer::GetBoundObjects(FMovieSceneObjectBindingID ObjectBinding)
{
	TArray<UObject*> Objects;
	for (TWeakObjectPtr<> WeakObject : FindBoundObjects(ObjectBinding.GetGuid(), ObjectBinding.GetSequenceID()))
	{
		if (UObject* Object = WeakObject.Get())
		{
			Objects.Add(Object);
		}
	}
	return Objects;
}

void UMovieSceneSequencePlayer::BeginDestroy()
{
	Stop();

	if (GEngine && OldMaxTickRate.IsSet())
	{
		GEngine->SetMaxFPS(OldMaxTickRate.GetValue());
	}

	Super::BeginDestroy();
}
