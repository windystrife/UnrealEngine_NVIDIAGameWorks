// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Osc.h"

namespace Audio
{
	// Bit crushing effect
	// https://en.wikipedia.org/wiki/Bitcrusher
	class AUDIOMIXER_API FBitCrusher
	{
	public:
		// Constructor
		FBitCrusher();

		// Destructor
		~FBitCrusher();

		// Initialize the equalizer
		void Init(const float InSampleRate);

		// The amount to reduce the sample rate of the audio stream.
		void SetSampleRateCrush(const float InFrequency);

		// The amount to reduce the bit depth of the audio stream.
		void SetBitDepthCrush(const float InBitDepth);

		// Processes a mono stream
		void ProcessAudio(const float InLeftSample, float& OutLeftSample);

		// Processes a stereo stream
		void ProcessAudio(const float InLeftSample, const float InRightSample, float& OutLeftSample, float& OutRightSample);

	private:
		// i.e. 8 bit, etc. But can be float!
		float SampleRate;
		float BitDepth;
		float BitDelta;

		// The current phase of the bit crusher
		float Phase;

		// The amount of phase to increment each sample
		float PhaseDelta;

		// Used to sample+hold the last output
		float LastOutputLeft;
		float LastOutputRight;

	};

}
