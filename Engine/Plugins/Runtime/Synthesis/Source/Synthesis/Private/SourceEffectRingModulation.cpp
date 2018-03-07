// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SourceEffects/SourceEffectRingModulation.h"

void FSourceEffectRingModulation::Init(const FSoundEffectSourceInitData& InitData)
{
	bIsActive = true;
	RingModulation.Init(InitData.SampleRate);
}

void FSourceEffectRingModulation::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SourceEffectRingModulation);

	switch (Settings.ModulatorType)
	{
		default:
		case ERingModulatorTypeSourceEffect::Sine:
			RingModulation.SetModulatorWaveType(Audio::EOsc::Sine);
			break;

		case ERingModulatorTypeSourceEffect::Saw:
			RingModulation.SetModulatorWaveType(Audio::EOsc::Saw);
			break;

		case ERingModulatorTypeSourceEffect::Triangle:
			RingModulation.SetModulatorWaveType(Audio::EOsc::Triangle);
			break;

		case ERingModulatorTypeSourceEffect::Square:
			RingModulation.SetModulatorWaveType(Audio::EOsc::Square);
			break;
	}

	RingModulation.SetModulationDepth(Settings.Depth);
	RingModulation.SetModulationFrequency(Settings.Frequency);
}

void FSourceEffectRingModulation::ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData)
{
	if (InData.AudioFrame.Num() == 2)
	{
		RingModulation.ProcessAudio(InData.AudioFrame[0], InData.AudioFrame[1], OutData.AudioFrame[0], OutData.AudioFrame[1]);
	}
	else
	{
		float OutLeft = 0.0f;
		float OutRight = 0.0f;
		RingModulation.ProcessAudio(InData.AudioFrame[0], 0.0f, OutLeft, OutRight);
		OutData.AudioFrame[0] = OutLeft;
	}
}

void USourceEffectRingModulationPreset::SetSettings(const FSourceEffectRingModulationSettings& InSettings)
{
	UpdateSettings(InSettings);
}
