// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMovieScenePlayer.h"

struct FTimeAndDelta
{
	float Time;
	float Delta;
};

struct FSequencerTimingManager
{
	virtual ~FSequencerTimingManager(){}

public:
	void Update(EMovieScenePlayerStatus::Type InStatus, float InCurrentTime)
	{
		if (Status.IsSet() && InStatus != Status.GetValue())
		{
			OnStopPlaying(InCurrentTime);
			Status.Reset();
		}

		if (!Status.IsSet() && (InStatus == EMovieScenePlayerStatus::Playing || InStatus == EMovieScenePlayerStatus::Recording))
		{
			Status = InStatus;
			OnStartPlaying(InCurrentTime);
		}
	}

public:

	virtual void OnStartPlaying(float InStartTime) {}
	virtual void OnStopPlaying(float InStopTime) {}
	virtual FTimeAndDelta AdjustTime(float InCurrentTime, float InDelta, float InPlayRate, float InDilation) = 0;

private:
	TOptional<EMovieScenePlayerStatus::Type> Status;
};

/** Default timing manager that is forced to the audio device clock */
struct FSequencerDefaultTimingManager : FSequencerTimingManager
{
	virtual FTimeAndDelta AdjustTime(float InCurrentTime, float InDelta, float InPlayRate, float InDilation) override;
};

/**
 * Playback timing manager that is forced to the audio device clock for accurate syncing of animation to audio.
 * @note Does not respect slomo tracks, since they accrue innacuracies each delta.
 */
struct FSequencerAudioClockTimer : FSequencerDefaultTimingManager
{
	FSequencerAudioClockTimer()
	{
		PlaybackStartTime = PlaybackStartAudioClock = 0;
		bIsPlaying = false;
	}

	double PlaybackStartTime;
	double PlaybackStartAudioClock;
	TOptional<double> LastAudioClock;
	bool bIsPlaying;
	
	virtual void OnStartPlaying(float InStartTime) override;
	virtual void OnStopPlaying(float InStopTime) override;

	virtual FTimeAndDelta AdjustTime(float InCurrentTime, float InDelta, float InPlayRate, float InDilation) override;
};
