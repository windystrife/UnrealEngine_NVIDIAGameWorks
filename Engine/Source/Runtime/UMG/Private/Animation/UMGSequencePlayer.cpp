// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/UMGSequencePlayer.h"
#include "MovieScene.h"
#include "UMGPrivate.h"
#include "Animation/WidgetAnimation.h"


UUMGSequencePlayer::UUMGSequencePlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerStatus = EMovieScenePlayerStatus::Stopped;
	TimeCursorPosition = 0.0f;
	AnimationStartOffset = 0;
	PlaybackSpeed = 1;
	Animation = nullptr;
	bIsEvaluating = false;
}

void UUMGSequencePlayer::InitSequencePlayer( UWidgetAnimation& InAnimation, UUserWidget& InUserWidget )
{
	Animation = &InAnimation;

	UMovieScene* MovieScene = Animation->GetMovieScene();

	// Cache the time range of the sequence to determine when we stop
	TimeRange = MovieScene->GetPlaybackRange();
	AnimationStartOffset = TimeRange.GetLowerBoundValue();

	UserWidget = &InUserWidget;
}

void UUMGSequencePlayer::Tick(float DeltaTime)
{
	if ( PlayerStatus == EMovieScenePlayerStatus::Playing )
	{
		const double AnimationLength = CurrentPlayRange.Size<double>();

		double LastTimePosition = TimeCursorPosition;
		TimeCursorPosition += bIsPlayingForward ? DeltaTime * PlaybackSpeed : -DeltaTime * PlaybackSpeed;

		// Check if we crossed over bounds
		const bool bCrossedLowerBound = TimeCursorPosition < CurrentPlayRange.GetLowerBoundValue();
		const bool bCrossedUpperBound = TimeCursorPosition > CurrentPlayRange.GetUpperBoundValue();
		const bool bCrossedEndTime = bIsPlayingForward
			? LastTimePosition < EndTime && EndTime <= TimeCursorPosition
			: LastTimePosition > EndTime && EndTime >= TimeCursorPosition;

		// Increment the num loops if we crossed any bounds.
		if (bCrossedLowerBound || bCrossedUpperBound || (bCrossedEndTime && NumLoopsCompleted >= NumLoopsToPlay - 1))
		{
			NumLoopsCompleted++;
		}

		// Did the animation complete
		const bool bCompleted = NumLoopsToPlay != 0 && NumLoopsCompleted >= NumLoopsToPlay;

		// Handle Times
		if (bCrossedLowerBound)
		{
			if (bCompleted)
			{
				TimeCursorPosition = CurrentPlayRange.GetLowerBoundValue();
			}
			else
			{
				if (PlayMode == EUMGSequencePlayMode::PingPong)
				{
					bIsPlayingForward = !bIsPlayingForward;
					TimeCursorPosition = FMath::Abs(TimeCursorPosition - CurrentPlayRange.GetLowerBoundValue()) + CurrentPlayRange.GetLowerBoundValue();
				}
				else
				{
					TimeCursorPosition += AnimationLength;
					LastTimePosition = TimeCursorPosition;
				}
			}
		}
		else if (bCrossedUpperBound)
		{
			if (bCompleted)
			{
				TimeCursorPosition = CurrentPlayRange.GetUpperBoundValue();
			}
			else
			{
				if (PlayMode == EUMGSequencePlayMode::PingPong)
				{
					bIsPlayingForward = !bIsPlayingForward;
					TimeCursorPosition = CurrentPlayRange.GetUpperBoundValue() - (TimeCursorPosition - CurrentPlayRange.GetUpperBoundValue());
				}
				else
				{
					TimeCursorPosition -= AnimationLength;
					LastTimePosition = TimeCursorPosition;
				}
			}
		}
		else if (bCrossedEndTime)
		{
			if (bCompleted)
			{
				TimeCursorPosition = EndTime;
			}
		}
		if (RootTemplateInstance.IsValid())
		{
			bIsEvaluating = true;

			const FMovieSceneContext Context(
				FMovieSceneEvaluationRange(TimeCursorPosition + AnimationStartOffset, LastTimePosition + AnimationStartOffset),
				PlayerStatus);
			RootTemplateInstance.Evaluate(Context, *this);

			bIsEvaluating = false;

			ApplyLatentActions();
		}

		if ( bCompleted )
		{
			PlayerStatus = EMovieScenePlayerStatus::Stopped;
			OnSequenceFinishedPlayingEvent.Broadcast(*this);
			Animation->OnAnimationFinished.Broadcast();
		}
	}
}

void UUMGSequencePlayer::PlayInternal(double StartAtTime, double EndAtTime, double SubAnimStartTime, double SubAnimEndTime, int32 InNumLoopsToPlay, EUMGSequencePlayMode::Type InPlayMode, float InPlaybackSpeed)
{
	RootTemplateInstance.Initialize(*Animation, *this);

	PlaybackSpeed = FMath::Abs(InPlaybackSpeed);
	PlayMode = InPlayMode;

	// Set the temporary range for this play of the animation
	CurrentPlayRange = TRange<double>(SubAnimStartTime, TRangeBound<double>::Inclusive(SubAnimEndTime));

	if (PlayMode == EUMGSequencePlayMode::Reverse)
	{
		// When playing in reverse count subtract the start time from the end.
		TimeCursorPosition = CurrentPlayRange.GetUpperBoundValue() - StartAtTime;
	}
	else
	{
		TimeCursorPosition = StartAtTime;
	}
	
	// Clamp the start time and end time to be within the bounds
	TimeCursorPosition = FMath::Clamp(TimeCursorPosition, CurrentPlayRange.GetLowerBoundValue(), CurrentPlayRange.GetUpperBoundValue());
	EndTime = FMath::Clamp(EndAtTime, CurrentPlayRange.GetLowerBoundValue(), CurrentPlayRange.GetUpperBoundValue());

	if ( PlayMode == EUMGSequencePlayMode::PingPong )
	{
		// When animating in ping-pong mode double the number of loops to play so that a loop is a complete forward/reverse cycle.
		NumLoopsToPlay = 2 * InNumLoopsToPlay;
	}
	else
	{
		NumLoopsToPlay = InNumLoopsToPlay;
	}

	NumLoopsCompleted = 0;
	bIsPlayingForward = InPlayMode != EUMGSequencePlayMode::Reverse;

	// Immediately evaluate the first frame of the animation so that if tick has already occurred, the widget is setup correctly and ready to be
	// rendered using the first frames data, otherwise you may see a *pop* due to a widget being constructed with a default different than the
	// first frame of the animation.
	if (RootTemplateInstance.IsValid())
	{			
		const FMovieSceneContext Context(FMovieSceneEvaluationRange(TimeCursorPosition, TimeCursorPosition), PlayerStatus);
		RootTemplateInstance.Evaluate(Context, *this);
	}

	PlayerStatus = EMovieScenePlayerStatus::Playing;
	Animation->OnAnimationStarted.Broadcast();
}

void UUMGSequencePlayer::Play(float StartAtTime, int32 InNumLoopsToPlay, EUMGSequencePlayMode::Type InPlayMode, float InPlaybackSpeed)
{
	double SubAnimStartTime = 0.0;
	double SubAnimEndTime = TimeRange.Size<float>();

	PlayInternal(StartAtTime, 0.0, SubAnimStartTime, SubAnimEndTime, InNumLoopsToPlay, InPlayMode, InPlaybackSpeed);
}

void UUMGSequencePlayer::PlayTo(float StartAtTime, float EndAtTime, int32 InNumLoopsToPlay, EUMGSequencePlayMode::Type InPlayMode, float InPlaybackSpeed)
{
	double SubAnimStartTime = 0.0;
	double SubAnimEndTime = TimeRange.Size<float>();

	PlayInternal(StartAtTime, EndAtTime, SubAnimStartTime, SubAnimEndTime, InNumLoopsToPlay, InPlayMode, InPlaybackSpeed);
}

void UUMGSequencePlayer::Pause()
{
	if (bIsEvaluating)
	{
		LatentActions.Add(ELatentAction::Pause);
		return;
	}

	// Purposely don't trigger any OnFinished events
	PlayerStatus = EMovieScenePlayerStatus::Stopped;

	RootTemplateInstance.Finish(*this);

	ApplyLatentActions();
}

void UUMGSequencePlayer::Reverse()
{
	if (PlayerStatus == EMovieScenePlayerStatus::Playing)
	{
		bIsPlayingForward = !bIsPlayingForward;
	}
}

void UUMGSequencePlayer::Stop()
{
	if (bIsEvaluating)
	{
		LatentActions.Add(ELatentAction::Stop);
		return;
	}

	PlayerStatus = EMovieScenePlayerStatus::Stopped;

	if (RootTemplateInstance.IsValid())
	{
		const FMovieSceneContext Context(FMovieSceneEvaluationRange(0), PlayerStatus);
		RootTemplateInstance.Evaluate(Context, *this);
		RootTemplateInstance.Finish(*this);
	}

	OnSequenceFinishedPlayingEvent.Broadcast(*this);
	Animation->OnAnimationFinished.Broadcast();

	TimeCursorPosition = 0;
}

void UUMGSequencePlayer::SetNumLoopsToPlay(int32 InNumLoopsToPlay)
{
	if (PlayMode == EUMGSequencePlayMode::PingPong)
	{
		NumLoopsToPlay = (2 * InNumLoopsToPlay);
	}
	else
	{
		NumLoopsToPlay = InNumLoopsToPlay;
	}
}

void UUMGSequencePlayer::SetPlaybackSpeed(float InPlaybackSpeed)
{
	PlaybackSpeed = InPlaybackSpeed;
}

EMovieScenePlayerStatus::Type UUMGSequencePlayer::GetPlaybackStatus() const
{
	return PlayerStatus;
}

UObject* UUMGSequencePlayer::GetPlaybackContext() const
{
	return UserWidget.Get();
}

TArray<UObject*> UUMGSequencePlayer::GetEventContexts() const
{
	TArray<UObject*> EventContexts;
	if (UserWidget.IsValid())
	{
		EventContexts.Add(UserWidget.Get());
	}
	return EventContexts;
}

void UUMGSequencePlayer::SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus)
{
	PlayerStatus = InPlaybackStatus;
}

void UUMGSequencePlayer::ApplyLatentActions()
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
