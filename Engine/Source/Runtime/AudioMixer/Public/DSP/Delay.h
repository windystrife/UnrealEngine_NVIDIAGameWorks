// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Dsp.h"
#include "DSP/WaveTableOsc.h"

namespace Audio
{
	// Circular Buffer Delay Line
	class AUDIOMIXER_API FDelay
	{
	public:
		// Constructor
		FDelay();

		// Virtual Destructor
		virtual ~FDelay();

	public:
		// Initialization of the delay with given sample rate and max buffer size in samples.
		void Init(const float InSampleRate, const float InBufferLengthSec = 2.0f);

		// Resets the delay line state, flushes buffer and resets read/write pointers.
		void Reset();

		// Sets the delay line length. Will clamp to within range of the max initialized delay line length (won't resize).
		void SetDelayMsec(const float InDelayMsec);

		// Same as SetDelayMsec, except in samples.
		void SetDelaySamples(const float InDelaySamples);

		// Sets the delay line length but using the internal easing function for smooth delay line interpolation.
		void SetEasedDelayMsec(const float InDelayMsec, const bool bIsInit = false);

		// Sets the output attenuation in DB
		void SetOutputAttenuationDB(const float InDelayAttenDB);

		// Returns the current delay line length (in samples).
		float GetDelayLengthSamples() const { return DelayInSamples; }

		// Reads the delay line at current read index without writing or incrementing read/write pointers.
		float Read() const;

		// Reads the delay line at an arbitrary time in Msec without writing or incrementing read/write pointers.
		float ReadDelayAt(const float InReadMsec) const;

		// Write the input and increment read/write pointers
		void WriteDelayAndInc(const float InDelayInput);

		// Process audio input and output buffer
		virtual void ProcessAudio(const float* InAudio, float* OutAudio);

	protected:

		// Updates delay line based on any recent changes to settings
		void Update(bool bForce = false);

		// Pointer to the circular buffer of audio.
		float* AudioBuffer;

		// Max length of buffer (in samples)
		int32 AudioBufferSize;

		// Read index for circular buffer.
		int32 ReadIndex;

		// Write index for circular buffer.
		int32 WriteIndex;

		// Sample rate
		float SampleRate;

		// Delay in samples; float supports fractional delay
		float DelayInSamples;

		// Eased delay in msec
		FExponentialEase EaseDelayMsec;

		// Output attenuation value.
		float OutputAttenuation;

		// Attenuation in decibel
		float OutputAttenuationDB;
	};

}
