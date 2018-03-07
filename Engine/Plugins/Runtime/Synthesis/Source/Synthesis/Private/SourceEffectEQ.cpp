// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SourceEffects/SourceEffectEQ.h"
#include "Audio.h"
#include "AudioDevice.h"

FSourceEffectEQ::FSourceEffectEQ()
	: SampleRate(0)
{
	FMemory::Memzero((void*)InAudioFrame, sizeof(float)*2);
	FMemory::Memzero((void*)OutAudioFrame, sizeof(float)*2);
}

void FSourceEffectEQ::Init(const FSoundEffectSourceInitData& InSampleRate)
{
	SampleRate = InSampleRate.SampleRate;
}

void FSourceEffectEQ::OnPresetChanged() 
{
	GET_EFFECT_SETTINGS(SourceEffectEQ);

	// Remove the filter bands at the end of the Settings eq bands is less than what we already have
	const int32 NumSettingBands = Settings.EQBands.Num();
	if (Filters.Num() < NumSettingBands)
	{
		const int32 Delta = NumSettingBands - Filters.Num();
		
		// Add them to the array
		int32 i = Filters.AddDefaulted(Delta);

		// Now initialize the new filters
		for (; i < Filters.Num(); ++i)
		{
			Filters[i].Init(SampleRate, 2, Audio::EBiquadFilter::ParametricEQ);
		}
	}
	else
	{
		// Disable filters if they don't match
		for (int32 i = NumSettingBands; i < Filters.Num(); ++i)
		{
			Filters[i].SetEnabled(false);
		}
	}

	check(Settings.EQBands.Num() <= Filters.Num());

	// Now make sure the filters settings are the same as the eq settings
	for (int32 i = 0; i < NumSettingBands; ++i)
	{
		const FSourceEffectEQBand& EQBandSetting = Settings.EQBands[i];

		Filters[i].SetEnabled(EQBandSetting.bEnabled);
		Filters[i].SetParams(Audio::EBiquadFilter::ParametricEQ, FMath::Max(20.0f, EQBandSetting.Frequency), EQBandSetting.Bandwidth, EQBandSetting.GainDb);
	}

}

void FSourceEffectEQ::ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData)
{
	const int32 NumChannels = InData.AudioFrame.Num();
	if (Filters.Num() == 0)
	{
		for (int32 Channel = 0; Channel < NumChannels; ++Channel)
		{
			OutData.AudioFrame[Channel] = InData.AudioFrame[Channel];
		}
	}

	if (NumChannels == 2)
	{
		for (int32 Channel = 0; Channel < 2; ++Channel)
		{
			InAudioFrame[Channel] = InData.AudioFrame[Channel];
		}

		for (int32 i = 0; i < Filters.Num(); ++i)
		{
			Filters[i].ProcessAudioFrame(InAudioFrame, OutData.AudioFrame.GetData());

			for (int32 Channel = 0; Channel < 2; ++Channel)
			{
				InAudioFrame[Channel] = OutData.AudioFrame[Channel];
			}
		}
	}
	else
	{
		InAudioFrame[0] = 0.5f * InData.AudioFrame[0];
		InAudioFrame[1] = InAudioFrame[0];
		OutAudioFrame[0] = 0.0f;
		OutAudioFrame[1] = 0.0f;

		for (int32 i = 0; i < Filters.Num(); ++i)
		{
			Filters[i].ProcessAudioFrame(InAudioFrame, OutAudioFrame);

			for (int32 Channel = 0; Channel < 2; ++Channel)
			{
				InAudioFrame[Channel] = OutAudioFrame[Channel];
			}
		}

		OutData.AudioFrame[0] = OutAudioFrame[0];
	}


}

void USourceEffectEQPreset::SetSettings(const FSourceEffectEQSettings& InSettings)
{
	UpdateSettings(InSettings);
}