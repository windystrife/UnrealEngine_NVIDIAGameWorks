// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "SubmixEffects/SubmixEffectDelay.h"

FSubmixEffectDelay::FSubmixEffectDelay()
	: SampleRate(0.0f)
	, MaxDelayLineLength(10000.0f)
	, InterpolationTime(0.0f)
	, TargetDelayLineLength(5000.0f)
{
}

FSubmixEffectDelay::~FSubmixEffectDelay()
{
}

void FSubmixEffectDelay::Init(const FSoundEffectSubmixInitData& InData)
{
	SampleRate = InData.SampleRate;
	InterpolationInfo.Init(SampleRate);
}

void FSubmixEffectDelay::OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData)
{
	if (DelayLines.Num() != InData.NumChannels)
	{
		OnNumChannelsChanged(InData.NumChannels);
	}

	UpdateParameters();

	// Cache all pointers first to avoid bounds checks on dereferences:
	const float* InBuffer = InData.AudioBuffer->GetData();
	float* OutBuffer = OutData.AudioBuffer->GetData();

	Audio::FDelay* DelaysPtr = DelayLines.GetData();

	int32 NumDelays = DelayLines.Num();
	
	// If we have no taps to render, short circuit.
	if (!NumDelays)
	{
		return;
	}

	for (int32 OutputBufferIndex = 0; OutputBufferIndex < OutData.AudioBuffer->Num(); OutputBufferIndex += OutData.NumChannels)
	{
		for (int32 DelayIndex = 0; DelayIndex < NumDelays; DelayIndex++)
		{
			const int32 SampleIndex = OutputBufferIndex + DelayIndex;
			DelaysPtr[DelayIndex].ProcessAudio(&InBuffer[SampleIndex], &OutBuffer[SampleIndex]);
			DelayLines[DelayIndex].SetDelayMsec(InterpolationInfo.GetValue());
		}
	}
}

void FSubmixEffectDelay::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SubmixEffectDelay);

	FSubmixEffectDelaySettings NewSettings;
	NewSettings = Settings;

	SetEffectParameters(NewSettings);
}

void FSubmixEffectDelay::SetEffectParameters(const FSubmixEffectDelaySettings& InTapEffectParameters)
{
	Params.SetParams(InTapEffectParameters);
}

void FSubmixEffectDelay::SetInterpolationTime(float Time)
{
	InterpolationTime = Time / 1000.0f;
	InterpolationInfo.SetValue(TargetDelayLineLength, InterpolationTime);
}

void FSubmixEffectDelay::SetDelayLineLength(float Length)
{
	TargetDelayLineLength = FMath::Clamp(Length, 0.4f, MaxDelayLineLength);
	InterpolationInfo.SetValue(TargetDelayLineLength, InterpolationTime);
}

void FSubmixEffectDelay::UpdateParameters()
{
	FSubmixEffectDelaySettings NewSettings;

	if (Params.GetParams(&NewSettings))
	{
		Audio::FDelay* DelaysPtr = DelayLines.GetData();

		MaxDelayLineLength = NewSettings.MaximumDelayLength;
		InterpolationTime = NewSettings.InterpolationTime / 1000.0f;

		TargetDelayLineLength = FMath::Clamp(NewSettings.DelayLength, 0.4f, MaxDelayLineLength);
		InterpolationInfo.SetValue(TargetDelayLineLength, InterpolationTime);

		if (MaxDelayLineLength != NewSettings.MaximumDelayLength)
		{
			for (int32 DelayIndex = 0; DelayIndex < DelayLines.Num(); DelayIndex++)
			{
				DelaysPtr[DelayIndex].Init(SampleRate, MaxDelayLineLength / 1000.0f);
			}
		}
	}
}

void FSubmixEffectDelay::OnNumChannelsChanged(int32 NumChannels)
{
	const int32 PriorNumDelayLines = DelayLines.Num();
	int32 NumAdditionalChannels = NumChannels - PriorNumDelayLines;

	DelayLines.SetNum(NumChannels);

	//If we have an additional amount of delay lines, initialize them here:
	if (NumAdditionalChannels > 0)
	{
		Audio::FDelay* DelayLinesPtr = DelayLines.GetData();

		for (int32 DelayIndex = PriorNumDelayLines; DelayIndex < DelayLines.Num(); DelayIndex++)
		{
			DelayLinesPtr[DelayIndex].Init(SampleRate, MaxDelayLineLength);
		}
	}
}

void USubmixEffectDelayPreset::SetInterpolationTime(float Time)
{
	DynamicSettings.InterpolationTime = Time;
	// Dispatch to all effect instances:
	for (FSoundEffectBase* EffectBaseInstance : Instances)
	{
		FSubmixEffectDelay* TapDelay = (FSubmixEffectDelay*)EffectBaseInstance;
		EffectBaseInstance->EffectCommand([TapDelay, Time]()
		{
			TapDelay->SetInterpolationTime(Time);
		});
	}
}

void USubmixEffectDelayPreset::SetDelay(float Length)
{
	DynamicSettings.DelayLength = Length;
	// Dispatch to all effect instances:
	for (FSoundEffectBase* EffectBaseInstance : Instances)
	{
		FSubmixEffectDelay* Delay = (FSubmixEffectDelay*)EffectBaseInstance;
		EffectBaseInstance->EffectCommand([Delay, Length]()
		{
			Delay->SetDelayLineLength(Length);
		});
	}
}

void USubmixEffectDelayPreset::OnInit()
{
	// Copy the settings to our dynamic settings so we can modify
	DynamicSettings = Settings;
}

void USubmixEffectDelayPreset::SetSettings(const FSubmixEffectDelaySettings& InSettings)
{
	DynamicSettings = InSettings;
	UpdateSettings(InSettings);
}


