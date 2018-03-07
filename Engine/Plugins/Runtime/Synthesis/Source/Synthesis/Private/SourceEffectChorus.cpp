// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SourceEffects/SourceEffectChorus.h"

void FSourceEffectChorus::Init(const FSoundEffectSourceInitData& InitData)
{
	bIsActive = true;

	Chorus.Init(InitData.SampleRate, 2.0f, 64);
}

void FSourceEffectChorus::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SourceEffectChorus);

	Chorus.SetDepth(Audio::EChorusDelays::Left, Settings.Depth);
	Chorus.SetDepth(Audio::EChorusDelays::Center, Settings.Depth);
	Chorus.SetDepth(Audio::EChorusDelays::Right, Settings.Depth);

	Chorus.SetFeedback(Audio::EChorusDelays::Left, Settings.Feedback);
	Chorus.SetFeedback(Audio::EChorusDelays::Center, Settings.Feedback);
	Chorus.SetFeedback(Audio::EChorusDelays::Right, Settings.Feedback);

	Chorus.SetFrequency(Audio::EChorusDelays::Left, Settings.Frequency);
	Chorus.SetFrequency(Audio::EChorusDelays::Center, Settings.Frequency);
	Chorus.SetFrequency(Audio::EChorusDelays::Right, Settings.Frequency);

	Chorus.SetWetLevel(Settings.WetLevel);
	Chorus.SetSpread(Settings.Spread);
}

void FSourceEffectChorus::ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData)
{
	if (InData.AudioFrame.Num() == 2)
	{
		Chorus.ProcessAudio(InData.AudioFrame[0], InData.AudioFrame[1], OutData.AudioFrame[0], OutData.AudioFrame[1]);
	}
	else
	{
		float OutLeft = 0.0f;
		float OutRight = 0.0f;
		Chorus.ProcessAudio(InData.AudioFrame[0], InData.AudioFrame[0], OutLeft, OutRight);
		OutData.AudioFrame[0] = 0.5f * (OutLeft + OutRight);
	}
}

void USourceEffectChorusPreset::SetSettings(const FSourceEffectChorusSettings& InSettings)
{
	UpdateSettings(InSettings);
}