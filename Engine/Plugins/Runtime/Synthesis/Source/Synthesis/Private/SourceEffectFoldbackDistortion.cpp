// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SourceEffects/SourceEffectFoldbackDistortion.h"

void FSourceEffectFoldbackDistortion::Init(const FSoundEffectSourceInitData& InitData)
{
	bIsActive = true;
	FoldbackDistortion.Init(InitData.SampleRate);
}

void FSourceEffectFoldbackDistortion::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SourceEffectFoldbackDistortion);

	FoldbackDistortion.SetInputGainDb(Settings.InputGainDb);
	FoldbackDistortion.SetThresholdDb(Settings.ThresholdDb);
	FoldbackDistortion.SetOutputGainDb(Settings.OutputGainDb);
}

void FSourceEffectFoldbackDistortion::ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData)
{
	if (InData.AudioFrame.Num() == 2)
	{
		FoldbackDistortion.ProcessAudio(InData.AudioFrame[0], InData.AudioFrame[1], OutData.AudioFrame[0], OutData.AudioFrame[1]);
	}
	else
	{
		FoldbackDistortion.ProcessAudio(InData.AudioFrame[0], OutData.AudioFrame[0]);
	}
}

void USourceEffectFoldbackDistortionPreset::SetSettings(const FSourceEffectFoldbackDistortionSettings& InSettings)
{
	UpdateSettings(InSettings);
}