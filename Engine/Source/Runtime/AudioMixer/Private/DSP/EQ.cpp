// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DSP/EQ.h"
#include "DSP/Dsp.h"

namespace Audio
{
	FEqualizer::FEqualizer()
		: NumBands(0)
		, NumChannels(0)
		, FilterBands(nullptr)
	{
	}

	FEqualizer::~FEqualizer()
	{
		if (FilterBands)
		{
			delete[] FilterBands;
			FilterBands = nullptr;
		}
	}

	void FEqualizer::Init(const float InSampleRate, const int32 InNumBands, const int32 InNumChannels)
	{
		if (FilterBands)
		{
			delete[] FilterBands;
			FilterBands = nullptr;
		}

		NumBands = InNumBands;
		NumChannels = InNumChannels;
		FilterBands = new FBiquadFilter[NumBands];

		for (int32 Band = 0; Band < NumBands; ++Band)
		{
			FilterBands[Band].Init(InSampleRate, InNumChannels, EBiquadFilter::ParametricEQ, 500.0f, 1.0f, 0.0f);
		}

	}

	void FEqualizer::SetBandEnabled(const int32 InBand, const bool bInEnabled)
	{
		if (!FilterBands || InBand >= NumBands)
		{
			return;
		}

		FilterBands[InBand].SetEnabled(bInEnabled);
	}

	void FEqualizer::SetBandParams(const int32 InBand, const float InFrequency, const float InBandwidth, const float InGainDB)
	{
		if (!FilterBands || InBand >= NumBands)
		{
			return;
		}

		FilterBands[InBand].SetParams(EBiquadFilter::ParametricEQ, InFrequency, InBandwidth, InGainDB);
	}

	void FEqualizer::SetBandFrequency(const int32 InBand, const float InFrequency)
	{
		if (!FilterBands || InBand >= NumBands)
		{
			return;
		}


		FilterBands[InBand].SetFrequency(InFrequency);
	}

	void FEqualizer::SetBandBandwidth(const int32 InBand, const float InBandwidth)
	{
		if (!FilterBands || InBand >= NumBands)
		{
			return;
		}

		FilterBands[InBand].SetBandwidth(InBandwidth);
	}

	void FEqualizer::SetBandGainDB(const int32 InBand, const float InGainDB)
	{
		if (!FilterBands || InBand >= NumBands)
		{
			return;
		}

		FilterBands[InBand].SetGainDB(InGainDB);
	}

	void FEqualizer::ProcessAudioFrame(const float* InAudio, float* OutAudio)
	{
		float AudioBufferScratchInput[8];

		for (int32 Channel = 0; Channel < NumChannels; ++Channel)
		{
			AudioBufferScratchInput[Channel] = InAudio[Channel];
		}

		// Process each band in parallel
		for (int32 Band = 0; Band < NumBands; ++Band)
		{
			FilterBands[Band].ProcessAudioFrame(AudioBufferScratchInput, OutAudio);

			for (int32 Channel = 0; Channel < NumChannels; ++Channel)
			{
				AudioBufferScratchInput[Channel] = OutAudio[Channel];
			}
		}
	}

}
