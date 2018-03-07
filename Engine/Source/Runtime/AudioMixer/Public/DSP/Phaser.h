// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Delay.h"
#include "DSP/LFO.h"
#include "DSP/Filter.h"

namespace Audio
{

	class AUDIOMIXER_API FPhaser
	{
	public:
		FPhaser();
		~FPhaser();

		void Init(const float SampleRate);

		// Sets the phaser LFO rate
		void SetFrequency(const float InFreqHz);

		// Sets the wet level of the phaser
		void SetWetLevel(const float InWetLevel);

		// Sets the feedback of the phaser
		void SetFeedback(const float InFeedback);

		// Sets the phaser LFO type
		void SetLFOType(const ELFO::Type LFOType);

		// Sets whether or not to put the phaser in quadrature mode
		void SetQuadPhase(const bool bQuadPhase);

		// Generates the next stereo frame of audio
		void ProcessAudio(const float* InFrame, float* OutFrame);

	protected:
		void ComputeNewCoefficients(const int32 ChannelIndex, const float LFOValue);

		// First-order all-pass filters in series
		static const int32 NumApfs = 6;
		static const int32 NumChannels = 2;

		int32 ControlSampleCount;
		int32 ControlRate;
		float Frequency;
		float WetLevel;
		float Feedback;
		ELFO::Type LFOType;
		bool bIsBiquadPhase;

		// 6 APFs per channel
		FBiquadFilter APFs[NumChannels][NumApfs];
		FVector2D APFFrequencyRanges[NumApfs];

		// Feedback samples
		float FeedbackFrame[NumChannels];

		FLFO LFO;
	};

}
