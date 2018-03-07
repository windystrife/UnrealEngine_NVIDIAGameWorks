// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SourceEffects/SourceEffectFilter.h"


FSourceEffectFilter::FSourceEffectFilter()
	: CurrentFilter(nullptr)
	, CutoffFrequency(8000.0f)
	, FilterQ(2.0f)
	, CircuitType(ESourceEffectFilterCircuit::StateVariable)
	, FilterType(ESourceEffectFilterType::LowPass)
{
	FMemory::Memzero(AudioInput, 2 * sizeof(float));
	FMemory::Memzero(AudioOutput, 2 * sizeof(float));
}

void FSourceEffectFilter::Init(const FSoundEffectSourceInitData& InitData)
{
	bIsActive = true;

	StateVariableFilter.Init(InitData.SampleRate, 2);
	LadderFilter.Init(InitData.SampleRate, 2);
	OnePoleFilter.Init(InitData.SampleRate, 2);

	UpdateFilter();
}

void FSourceEffectFilter::UpdateFilter()
{
	switch (CircuitType)
	{
		default:
		case ESourceEffectFilterCircuit::OnePole:
		{
			CurrentFilter = &OnePoleFilter;
		}
		break;

		case ESourceEffectFilterCircuit::StateVariable:
		{
			CurrentFilter = &StateVariableFilter;
		}
		break;

		case ESourceEffectFilterCircuit::Ladder:
		{
			CurrentFilter = &LadderFilter;
		}
		break;
	}

	switch (FilterType)
	{
		default:
		case ESourceEffectFilterType::LowPass:
			CurrentFilter->SetFilterType(Audio::EFilter::LowPass);
			break;

		case ESourceEffectFilterType::HighPass:
			CurrentFilter->SetFilterType(Audio::EFilter::HighPass);
			break;

		case ESourceEffectFilterType::BandPass:
			CurrentFilter->SetFilterType(Audio::EFilter::BandPass);
			break;

		case ESourceEffectFilterType::BandStop:
			CurrentFilter->SetFilterType(Audio::EFilter::BandStop);
			break;
	}

	CurrentFilter->SetFrequency(CutoffFrequency);
	CurrentFilter->SetQ(FilterQ);
	CurrentFilter->Update();
}

void FSourceEffectFilter::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SourceEffectFilter);

	CircuitType = Settings.FilterCircuit;
	FilterType = Settings.FilterType;
	CutoffFrequency = Settings.CutoffFrequency;
	FilterQ = Settings.FilterQ;

	UpdateFilter();
}

void FSourceEffectFilter::ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData)
{
	if (InData.AudioFrame.Num() == 2)
	{
		CurrentFilter->ProcessAudio(InData.AudioFrame.GetData(), OutData.AudioFrame.GetData());
	}
	else
	{
		AudioInput[0] = InData.AudioFrame[0];
		CurrentFilter->ProcessAudio(AudioInput, AudioOutput);
		OutData.AudioFrame[0] = AudioOutput[0];
	}
}

void USourceEffectFilterPreset::SetSettings(const FSourceEffectFilterSettings& InSettings)
{
	UpdateSettings(InSettings);
}