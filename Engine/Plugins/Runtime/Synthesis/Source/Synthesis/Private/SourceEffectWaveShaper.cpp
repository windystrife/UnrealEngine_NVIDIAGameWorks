// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SourceEffects/SourceEffectWaveShaper.h"

void FSourceEffectWaveShaper::Init(const FSoundEffectSourceInitData& InitData)
{
	bIsActive = true;
	WaveShaper.Init(InitData.SampleRate);
}

void FSourceEffectWaveShaper::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SourceEffectWaveShaper);

	WaveShaper.SetAmount(Settings.Amount);
	WaveShaper.SetOutputGainDb(Settings.OutputGainDb);
}

void FSourceEffectWaveShaper::ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData)
{
	if (InData.AudioFrame.Num() == 2)
	{
		check(OutData.AudioFrame.Num() == 2);

		WaveShaper.ProcessAudio(InData.AudioFrame[0], OutData.AudioFrame[0]);
		WaveShaper.ProcessAudio(InData.AudioFrame[1], OutData.AudioFrame[1]);
	}
	else
	{
		WaveShaper.ProcessAudio(InData.AudioFrame[0], OutData.AudioFrame[0]);
	}
}

void USourceEffectWaveShaperPreset::SetSettings(const FSourceEffectWaveShaperSettings& InSettings)
{
	UpdateSettings(InSettings);
}