// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Audio
{

	// A digital wave shaping effect to cause audio distortion
	// https://en.wikipedia.org/wiki/Waveshaper
	class AUDIOMIXER_API FWaveShaper
	{
	public:
		// Constructor
		FWaveShaper();

		// Destructor
		~FWaveShaper();

		// Initialize the equalizer
		void Init(const float InSampleRate);

		// Sets the amount of wave shapping. 0.0 is no effect.
		void SetAmount(const float InAmount);

		// Sets the output gain of the waveshaper
		void SetOutputGainDb(const float InGainDb);

		// Processes the audio frame (audio frame must have channels equal to that used during initialization)
		void ProcessAudio(const float InSample, float& OutSample);

	private:

		float Amount;
		float AtanAmount;
		float OutputGain;
	};

}
