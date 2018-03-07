// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SourceEffects/SourceEffectBitCrusher.h"

void FSourceEffectBitCrusher::Init(const FSoundEffectSourceInitData& InitData)
{
	bIsActive = true;
	BitCrusher.Init(InitData.SampleRate);
}

void FSourceEffectBitCrusher::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SourceEffectBitCrusher);

	BitCrusher.SetBitDepthCrush(Settings.CrushedBits);
	BitCrusher.SetSampleRateCrush(Settings.CrushedSampleRate);
}

void FSourceEffectBitCrusher::ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData)
{
	if (InData.AudioFrame.Num() == 2)
	{
		BitCrusher.ProcessAudio(InData.AudioFrame[0], InData.AudioFrame[1], OutData.AudioFrame[0], OutData.AudioFrame[1]);
	}
	else
	{
		BitCrusher.ProcessAudio(InData.AudioFrame[0], OutData.AudioFrame[0]);
	}
}

void USourceEffectBitCrusherPreset::SetSettings(const FSourceEffectBitCrusherSettings& InSettings)
{
	UpdateSettings(InSettings);
}