// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SubmixEffects/AudioMixerSubmixEffectDynamicsProcessor.h"

FSubmixEffectDynamicsProcessor::FSubmixEffectDynamicsProcessor()
{
}

void FSubmixEffectDynamicsProcessor::Init(const FSoundEffectSubmixInitData& InitData)
{
	DynamicsProcessor.Init(InitData.SampleRate, 8);

	AudioOutputFrame.Reset();
	AudioOutputFrame.AddZeroed(8);
}


void FSubmixEffectDynamicsProcessor::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SubmixEffectDynamicsProcessor);

	switch (Settings.DynamicsProcessorType)
	{
	default:
	case ESubmixEffectDynamicsProcessorType::Compressor:
		DynamicsProcessor.SetProcessingMode(Audio::EDynamicsProcessingMode::Compressor);
		break;

	case ESubmixEffectDynamicsProcessorType::Limiter:
		DynamicsProcessor.SetProcessingMode(Audio::EDynamicsProcessingMode::Limiter);
		break;

	case ESubmixEffectDynamicsProcessorType::Expander:
		DynamicsProcessor.SetProcessingMode(Audio::EDynamicsProcessingMode::Expander);
		break;

	case ESubmixEffectDynamicsProcessorType::Gate:
		DynamicsProcessor.SetProcessingMode(Audio::EDynamicsProcessingMode::Gate);
		break;
	}

	switch (Settings.PeakMode)
	{
	default:
	case ESubmixEffectDynamicsPeakMode::MeanSquared:
		DynamicsProcessor.SetPeakMode(Audio::EPeakMode::MeanSquared);
		break;

	case ESubmixEffectDynamicsPeakMode::RootMeanSquared:
		DynamicsProcessor.SetPeakMode(Audio::EPeakMode::RootMeanSquared);
		break;

	case ESubmixEffectDynamicsPeakMode::Peak:
		DynamicsProcessor.SetPeakMode(Audio::EPeakMode::Peak);
		break;
	}

	DynamicsProcessor.SetLookaheadMsec(Settings.LookAheadMsec);
	DynamicsProcessor.SetAttackTime(Settings.AttackTimeMsec);
	DynamicsProcessor.SetReleaseTime(Settings.ReleaseTimeMsec);
	DynamicsProcessor.SetThreshold(Settings.ThresholdDb);
	DynamicsProcessor.SetRatio(Settings.Ratio);
	DynamicsProcessor.SetKneeBandwidth(Settings.KneeBandwidthDb);
	DynamicsProcessor.SetInputGain(Settings.InputGainDb);
	DynamicsProcessor.SetOutputGain(Settings.OutputGainDb);
	DynamicsProcessor.SetChannelLinked(Settings.bChannelLinked);
	DynamicsProcessor.SetAnalogMode(Settings.bAnalogMode);
}

void FSubmixEffectDynamicsProcessor::OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData)
{
	const Audio::AlignedFloatBuffer& InBuffer = *InData.AudioBuffer;
	Audio::AlignedFloatBuffer& OutBuffer = *OutData.AudioBuffer;

	// Use a full multichannel frame as a scratch processing buffer
	AudioInputFrame.Reset();
	AudioInputFrame.AddZeroed(8);

	for (int32 Frame = 0; Frame < InData.NumFrames; ++Frame)
	{
		// Copy the data to the frame input
		const int32 SampleIndexOfFrame = Frame * InData.NumChannels;
		for (int32 Channel = 0; Channel < InData.NumChannels; ++Channel)
		{
			const int32 SampleIndex = SampleIndexOfFrame + Channel;
			AudioInputFrame[Channel] = InBuffer[SampleIndex];
		}

		// Process
		DynamicsProcessor.ProcessAudio(AudioInputFrame.GetData(), InData.NumChannels, AudioOutputFrame.GetData());

		// Copy the data to the frame output
		for (int32 Channel = 0; Channel < InData.NumChannels; ++Channel)
		{
			const int32 SampleIndex = SampleIndexOfFrame + Channel;
			OutBuffer[SampleIndex] = AudioOutputFrame[Channel];
		}
	}
}

void USubmixEffectDynamicsProcessorPreset::SetSettings(const FSubmixEffectDynamicsProcessorSettings& InSettings)
{
	UpdateSettings(InSettings);
}