// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Osc.h"

namespace Audio
{
	// Foldback distortion effect
	// https://en.wikipedia.org/wiki/Foldback_(power_supply_design)
	class AUDIOMIXER_API FFoldbackDistortion
	{
	public:
		// Constructor
		FFoldbackDistortion();

		// Destructor
		~FFoldbackDistortion();

		// Initialize the equalizer
		void Init(const float InSampleRate);

		// Sets the foldback distortion threshold
		void SetThresholdDb(const float InThresholdDb);

		// Sets the input gain
		void SetInputGainDb(const float InInputGainDb);

		// Sets the output gain
		void SetOutputGainDb(const float InOutputGainDb);

		// Processes a mono stream
		void ProcessAudio(const float InLeftSample, float& OutLeftSample);

		// Processes a stereo stream
		void ProcessAudio(const float InLeftSample, const float InRightSample, float& OutLeftSample, float& OutRightSample);

	private:
		// Threshold to check before folding audio back on itself
		float Threshold;

		// Threshold times 2
		float Threshold2;

		// Threshold time 4
		float Threshold4;

		// Input gain used to force hitting the threshold
		float InputGain;

		// A final gain scaler to apply to the output
		float OutputGain;
	};

}
