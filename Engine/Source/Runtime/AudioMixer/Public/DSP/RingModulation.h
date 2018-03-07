// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Osc.h"

namespace Audio
{
	// Ring modulation effect
	// https://en.wikipedia.org/wiki/Ring_modulation
	class AUDIOMIXER_API FRingModulation
	{
	public:
		// Constructor
		FRingModulation();

		// Destructor
		~FRingModulation();

		// Initialize the equalizer
		void Init(const float InSampleRate);

		// The type of modulation
		void SetModulatorWaveType(const EOsc::Type InType);

		// Set the ring modulation frequency
		void SetModulationFrequency(const float InModulationFrequency);

		// Set the ring modulation depth
		void SetModulationDepth(const float InModulationDepth);

		// Processes the audio frame (audio frame must have channels equal to that used during initialization)
		void ProcessAudio(const float InLeftSample, const float InRightSample, float& OutLeftSample, float& OutRightSample);

	private:
		Audio::FOsc Osc;
		float ModulationFrequency;
		float ModulationDepth;
	};

}
