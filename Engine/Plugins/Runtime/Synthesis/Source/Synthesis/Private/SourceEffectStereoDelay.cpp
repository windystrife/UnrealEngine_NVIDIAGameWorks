// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SourceEffects/SourceEffectStereoDelay.h"
#include "Casts.h"

void FSourceEffectStereoDelay::Init(const FSoundEffectSourceInitData& InitData)
{
	bIsActive = true;
	DelayStereo.Init(InitData.SampleRate);
}

void FSourceEffectStereoDelay::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SourceEffectStereoDelay);

	DelayStereo.SetDelayTimeMsec(Settings.DelayTimeMsec);
	DelayStereo.SetFeedback(Settings.Feedback);
	DelayStereo.SetWetLevel(Settings.WetLevel);
	DelayStereo.SetDelayRatio(Settings.DelayRatio);
	DelayStereo.SetMode((Audio::EStereoDelayMode::Type)Settings.DelayMode);
}

void FSourceEffectStereoDelay::ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData)
{
	if (InData.AudioFrame.Num() == 2)
	{
		DelayStereo.ProcessAudio(InData.AudioFrame[0], InData.AudioFrame[1], OutData.AudioFrame[0], OutData.AudioFrame[1]);
	}
	else
	{
		float OutLeft = 0.0f;
		float OutRight = 0.0f;
		DelayStereo.ProcessAudio(InData.AudioFrame[0], InData.AudioFrame[0], OutLeft, OutRight);
		OutData.AudioFrame[0] = 0.5f * (OutLeft + OutRight);
	}
}

void USourceEffectStereoDelayPreset::SetSettings(const FSourceEffectStereoDelaySettings& InSettings)
{
	UpdateSettings(InSettings);
}