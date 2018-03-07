// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DSP/BitCrusher.h"
#include "Audio.h"

namespace Audio
{
	FBitCrusher::FBitCrusher()
		: SampleRate(0)
		, BitDepth(16.0f)
		, BitDelta(0.0f)
		, Phase(1.0f)
		, PhaseDelta(1.0f)
		, LastOutputLeft(0.0f)
		, LastOutputRight(0.0f)
	{
		BitDelta = 1.0f / FMath::Pow(2.0f, BitDepth);
	}

	FBitCrusher::~FBitCrusher()
	{
	}

	void FBitCrusher::Init(const float InSampleRate)
	{
		SampleRate = InSampleRate;
		Phase = 1.0f;
	}

	void FBitCrusher::SetSampleRateCrush(const float InFrequency)
	{
		PhaseDelta = FMath::Clamp(InFrequency, 1.0f, SampleRate) / SampleRate;
	}

	void FBitCrusher::SetBitDepthCrush(const float InBitDepth)
	{
		BitDepth = FMath::Clamp(InBitDepth, 1.0f, 32.0f);
		BitDelta = 1.0f / FMath::Pow(2.0f, BitDepth);
	}

	void FBitCrusher::ProcessAudio(const float InLeftSample, float& OutLeftSample)
	{
		Phase += PhaseDelta;

		// Only output audio at the sample rate set, otherwise sample and hold the last output value
		// This filters out every N samples, etc.
		if (Phase >= 1.0f)
		{
			Phase -= 1.0f;

			// This quantizes the input audio to the set bit depth
			LastOutputLeft = BitDelta * (float)FMath::FloorToInt(InLeftSample / BitDelta + 0.5f);
		}

		OutLeftSample = LastOutputLeft;
	}

	void FBitCrusher::ProcessAudio(const float InLeftSample, const float InRightSample, float& OutLeftSample, float& OutRightSample)
	{
		Phase += PhaseDelta;

		// Only output audio at the sample rate set, otherwise sample and hold the last output value
		// This filters out every N samples, etc.
		if (Phase >= 1.0f)
		{
			Phase -= 1.0f;

			// This quantizes the input audio to the set bit depth
			LastOutputLeft = BitDelta * (float)FMath::FloorToInt(InLeftSample / BitDelta + 0.5f);
			LastOutputRight = BitDelta * (float)FMath::FloorToInt(InRightSample / BitDelta + 0.5f);
		}

		OutLeftSample = LastOutputLeft;
		OutRightSample = LastOutputRight;
	}

}
