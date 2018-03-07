// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SourceEffects/SourceEffectPhaser.h"

void FSourceEffectPhaser::Init(const FSoundEffectSourceInitData& InitData)
{
	bIsActive = true;

	Phaser.Init(InitData.SampleRate);
}

void FSourceEffectPhaser::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SourceEffectPhaser);

	Phaser.SetFrequency(Settings.Frequency);
	Phaser.SetWetLevel(Settings.WetLevel);
	Phaser.SetQuadPhase(Settings.UseQuadraturePhase);
	Phaser.SetFeedback(Settings.Feedback);

	Phaser.SetLFOType((Audio::ELFO::Type)Settings.LFOType);
}

void FSourceEffectPhaser::ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData)
{
	if (InData.AudioFrame.Num() == 2)
	{
		Phaser.ProcessAudio(InData.AudioFrame.GetData(), OutData.AudioFrame.GetData());
	}
	else
	{
		float InFrame[2];
		InFrame[0] = 0.5f * InData.AudioFrame[0];
		InFrame[1] = InFrame[0];

		float OutFrame[2];
		Phaser.ProcessAudio(InFrame, OutFrame);
		OutData.AudioFrame[0] = 0.5f * (OutFrame[0] + OutFrame[1]);
	}
}

void USourceEffectPhaserPreset::SetSettings(const FSourceEffectPhaserSettings& InSettings)
{
	UpdateSettings(InSettings);
}