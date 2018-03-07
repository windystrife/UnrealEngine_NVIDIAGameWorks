// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequencerTimingManager.h"
#include "AudioDevice.h"
#include "GameFramework/WorldSettings.h"

FTimeAndDelta FSequencerDefaultTimingManager::AdjustTime(float InCurrentTime, float InDelta, float InPlayRate, float InDilation)
{
	FTimeAndDelta RetVal;
	RetVal.Delta = InDelta;
	RetVal.Time = InCurrentTime + InDelta * InDilation * InPlayRate;

	return RetVal;
}

void FSequencerAudioClockTimer::OnStartPlaying(float InStartTime)
{
	PlaybackStartAudioClock = GEngine->GetMainAudioDevice()->GetAudioClock();
	PlaybackStartTime = InStartTime;
	bIsPlaying = true;
}

void FSequencerAudioClockTimer::OnStopPlaying(float InStopTime)
{
	bIsPlaying = false;
}

FTimeAndDelta FSequencerAudioClockTimer::AdjustTime(float InCurrentTime, float InDelta, float InPlayRate, float InDilation)
{
	if (!bIsPlaying)
	{
		// If we're not playing, we just use the default impl
		return FSequencerDefaultTimingManager::AdjustTime(InCurrentTime, InDelta, InPlayRate, InDilation);
	}

	const double Now = GEngine->GetMainAudioDevice()->GetAudioClock();
	const double AbsoluteAudioClockDelta = Now - PlaybackStartAudioClock;

	FTimeAndDelta RetVal;
	if (LastAudioClock.IsSet())
	{
		RetVal.Delta = float(Now - LastAudioClock.GetValue());
	}
	else
	{
		RetVal.Delta = AbsoluteAudioClockDelta;
	}
	RetVal.Time = PlaybackStartTime + float(AbsoluteAudioClockDelta);

	LastAudioClock = Now;
	return RetVal;
}
