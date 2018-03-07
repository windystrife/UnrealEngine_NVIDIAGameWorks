// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DSP/WaveShaper.h"
#include "DSP/Dsp.h"

namespace Audio
{
	FWaveShaper::FWaveShaper()
		: Amount(1.0f)
		, AtanAmount(FMath::Atan(Amount))
		, OutputGain(1.0f)
	{
	}

	FWaveShaper::~FWaveShaper()
	{
	}

	void FWaveShaper::Init(const float InSampleRate)
	{
	}

	void FWaveShaper::SetAmount(const float InAmount)
	{		
		Amount = FMath::Max(InAmount, SMALL_NUMBER);
		AtanAmount = FMath::Atan(Amount);
	}

	void FWaveShaper::SetOutputGainDb(const float InGainDb)
	{
		OutputGain = Audio::ConvertToLinear(InGainDb);
	}

	void FWaveShaper::ProcessAudio(const float InSample, float& OutSample)
	{
		OutSample = OutputGain * FMath::Atan(InSample * Amount) / AtanAmount;
	}

}
