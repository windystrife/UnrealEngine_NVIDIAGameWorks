// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Filter.h"

namespace Audio
{
	// Equalizer filter
	// An equalizer is a cascaded (serial) band of parametric EQs
	// This filter allows for setting each band with variable Bandwidth/Q, Frequency, and Gain
	class AUDIOMIXER_API FEqualizer
	{
	public:
		// Constructor
		FEqualizer();

		// Destructor
		~FEqualizer();

		// Initialize the equalizer
		void Init(const float InSampleRate, const int32 InNumBands, const int32 InNumChannels);

		// Sets whether or not the band is enabled
		void SetBandEnabled(const int32 InBand, const bool bEnabled);

		// Sets all params of the band at once
		void SetBandParams(const int32 InBand, const float InFrequency, const float InBandwidth, const float InGainDB);

		// Sets the band frequency
		void SetBandFrequency(const int32 InBand, const float InFrequency);

		// Sets the band resonance (use alternatively to bandwidth)
		void SetBandBandwidth(const int32 InBand, const float InBandwidth);

		// Sets the band gain in decibels
		void SetBandGainDB(const int32 InBand, const float InGainDB);

		// Processes the audio frame (audio frame must have channels equal to that used during initialization)
		void ProcessAudioFrame(const float* InAudio, float* OutAudio);

	private:

		// The number of bands in the equalizer
		int32 NumBands;

		// The number of channels in the equalizer
		int32 NumChannels;

		// The array of biquad filters
		FBiquadFilter* FilterBands;
	};

}
