// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Audio
{
	// Different modes for the envelope follower
	namespace EPeakMode
	{
		enum Type
		{
			MeanSquared,
			RootMeanSquared,
			Peak,
			Count
		};
	}

	// A simple utility that returns a smoothed value given audio input using an RC circuit.
	// Used for following the envelope of an audio stream.
	class AUDIOMIXER_API FEnvelopeFollower
	{
	public:

		FEnvelopeFollower();
		FEnvelopeFollower(const float InSampleRate, const float InAttackTimeMsec = 10.0f, const float InReleaseTimeMSec = 100.0f, const EPeakMode::Type InMode = EPeakMode::Peak, const bool bInIsAnalg = true);

		virtual ~FEnvelopeFollower();

		// Initialize the envelope follower
		void Init(const float InSampleRate, const float InAttackTimeMsec = 10.0f, const float InReleaseTimeMSec = 100.0f, const EPeakMode::Type InMode = EPeakMode::Peak, const bool bInIsAnalg = true);

		// Resets the state of the envelope follower
		void Reset();

		// Sets whether or not to use analog or digital time constants
		void SetAnalog(const bool bInIsAnalog);

		// Sets the envelope follower attack time (how fast the envelope responds to input)
		void SetAttackTime(const float InAttackTimeMsec);

		// Sets the envelope follower release time (how slow the envelope dampens from input)
		void SetReleaseTime(const float InReleaseTimeMsec);

		// Sets the output mode of the envelope follower
		void SetMode(const EPeakMode::Type InMode);

		// Processes the input audio stream and returns the envelope value.
		float ProcessAudio(const float InAudioSample);

		// Gets the current envelope value
		float GetCurrentValue() const;

	protected:
		EPeakMode::Type EnvMode;
		float SampleRate;
		float AttackTimeMsec;
		float AttackTimeSamples;
		float ReleaseTimeMsec;
		float ReleaseTimeSamples;
		float CurrentEnvelopeValue;
		bool bIsAnalog;
	};


}
