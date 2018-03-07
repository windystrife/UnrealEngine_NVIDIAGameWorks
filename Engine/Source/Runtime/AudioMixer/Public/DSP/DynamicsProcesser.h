// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/EnvelopeFollower.h"
#include "DSP/Delay.h"

namespace Audio
{
	// What mode the compressor is in
	namespace EDynamicsProcessingMode
	{
		enum Type
		{
			Compressor,
			Limiter,
			Expander,
			Gate,
			Count
		};
	}

	// Dynamic range compressor
	// https://en.wikipedia.org/wiki/Dynamic_range_compression
	class AUDIOMIXER_API FDynamicsProcessor
	{
	public:
		FDynamicsProcessor();
		~FDynamicsProcessor();

		void Init(const float SampleRate, const int32 NumChannels = 2);

		void SetLookaheadMsec(const float InLookAheadMsec);
		void SetAttackTime(const float InAttackTimeMsec);
		void SetReleaseTime(const float InReleaseTimeMsec);
		void SetThreshold(const float InThresholdDb);
		void SetRatio(const float InCompressionRatio);
		void SetKneeBandwidth(const float InKneeBandwidthDb);
		void SetInputGain(const float InInputGainDb);
		void SetOutputGain(const float InOutputGainDb);
		void SetChannelLinked(const bool bInIsChannelLinked);
		void SetAnalogMode(const bool bInIsAnalogMode);
		void SetPeakMode(const EPeakMode::Type InEnvelopeFollowerModeType);
		void SetProcessingMode(const EDynamicsProcessingMode::Type ProcessingMode);

		void ProcessAudio(const float* InputFrame, int32 NumChannels, float* OutputFrame);



	protected:

		float ComputeGain(const float InEnvFollowerDb);

		EDynamicsProcessingMode::Type ProcessingMode;

		// Lookahead delay lines
		TArray<FDelay> LookaheadDelay;

		// Envelope followers
		TArray<FEnvelopeFollower> EnvFollower;

		// Points in the knee used for lagrangian interpolation
		TArray<FVector2D> KneePoints;

		TArray<float> DetectorOuts;
		TArray<float> Gain;

		// How far ahead to look in the audio
		float LookaheedDelayMsec;

		// The period of which the compressor decreases gain to the level determined by the compression ratio
		float AttackTimeMsec;

		// The period of which the compressor increases gain to 0 dB once level has fallen below the threshold
		float ReleaseTimeMsec;

		// Amplitude threshold above which gain will be reduced
		float ThresholdDb;

		// Amount of gain reduction.
		float Ratio;

		// Defines how hard or soft the gain reduction blends from no gain reduction to gain reduction (determined by the ratio)
		float HalfKneeBandwidthDb;

		// Amount of input gain
		float InputGain;

		// Amount of output gain
		float OutputGain;

		// Whether or not the stereo channels are linked
		bool bIsChannelLinked;

		// Whether or not we're in analog mode
		bool bIsAnalogMode;

	};
}
