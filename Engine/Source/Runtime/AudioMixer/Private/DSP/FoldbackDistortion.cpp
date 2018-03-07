// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DSP/FoldbackDistortion.h"
#include "DSP/Dsp.h"

namespace Audio
{
	FFoldbackDistortion::FFoldbackDistortion()
		: Threshold(0.5f)
		, Threshold2(2.0f * Threshold)
		, Threshold4(4.0f * Threshold)
		, OutputGain(1.0f)
	{
	}

	FFoldbackDistortion::~FFoldbackDistortion()
	{
	}

	void FFoldbackDistortion::Init(const float InSampleRate)
	{
		// nothing to do...
	}

	void FFoldbackDistortion::SetThresholdDb(const float InThresholdDb)
	{
		Threshold = Audio::ConvertToLinear(InThresholdDb);
		Threshold2 = 2.0f * Threshold;
		Threshold4 = 4.0f * Threshold;
	}

	void FFoldbackDistortion::SetInputGainDb(const float InInputGainDb)
	{
		InputGain = Audio::ConvertToLinear(InInputGainDb);
	}

	void FFoldbackDistortion::SetOutputGainDb(const float InOutputGainDb)
	{
		OutputGain = Audio::ConvertToLinear(InOutputGainDb);
	}

	void FFoldbackDistortion::ProcessAudio(const float InSample, float& OutSample)
	{
		float Sample = InputGain * InSample;

		if (Sample > Threshold || Sample < -Threshold)
		{
			OutSample = FMath::Abs(FMath::Abs(FMath::Fmod(Sample - Threshold, Threshold4)) - Threshold2) - Threshold;
		}
		else
		{
			OutSample = Sample;
		}

		OutSample *= OutputGain;
	}

	void FFoldbackDistortion::ProcessAudio(const float InLeftSample, const float InRightSample, float& OutLeftSample, float& OutRightSample)
	{
		ProcessAudio(InLeftSample, OutLeftSample);
		ProcessAudio(InRightSample, OutRightSample);
	}

}
