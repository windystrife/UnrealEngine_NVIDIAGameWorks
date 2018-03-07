// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DSP/SinOsc.h"
#include "AudioMixer.h"

namespace Audio
{
	FSineOsc::FSineOsc()
		: SampleRate(0)
		, FrequencyHz(440.0f)
		, B1(0.0f)
		, B2(0.0f)
		, Yn_1(0.0f)
		, Yn_2(0.0f)
		, Scale(1.0f)
		, Add(0.0f)
	{
	}

	FSineOsc::FSineOsc(const int32 InSampleRate, const float InFrequencyHz, const float InScale, const float InAdd)
		: SampleRate(0)
		, FrequencyHz(440.0f)
		, B1(0.0f)
		, B2(0.0f)
		, Yn_1(0.0f)
		, Yn_2(0.0f)
		, Scale(InScale)
		, Add(InAdd)
	{
		Init(InSampleRate, InFrequencyHz);
	}

	FSineOsc::~FSineOsc()
	{
	}

	void FSineOsc::Init(const int32 InSampleRate, const float InFrequencyHz, const float InScale, const float InAdd)
	{
		AUDIO_MIXER_CHECK(InSampleRate > 0);
		AUDIO_MIXER_CHECK(InFrequencyHz > 0.0f);

		Scale = InScale;
		Add = InAdd;

		SampleRate = InSampleRate;

		SetFrequency(InFrequencyHz);
	}

	void FSineOsc::SetFrequency(const float InFrequencyHz)
	{
		AUDIO_MIXER_CHECK(SampleRate > 0);

		FrequencyHz = InFrequencyHz;

		// Find new wT value
		const float OmegaT = 2.0f * PI * FrequencyHz / SampleRate;

		// set the biquad feedback coefficients using filter design equations
		B1 = -2.0f * FMath::Cos(OmegaT);
		B2 = 1.0f;

		// Set up initial conditions based on current state of the oscillator to avoid pops when dynamically changing frequencies

		// Get previous outputs phase
		const float OmegaTPrev = FMath::Asin(Yn_1);

		// Get N by dividing prev phase over new current phase
		float N = OmegaTPrev / OmegaT;

		// If currently on rising edge (newer value is higher)
		if (Yn_1 > Yn_2)
		{
			// new y(n-2) value will be Sin((N - 1) * wT)
			N -= 1.0f;
		}
		// If on falling edge (new value is lower)
		else
		{
			// new y(n-2) value will be Sin((N + 1) * wT)
			N += 1.0f;
		}

		Yn_2 = FMath::Sin(N * OmegaT);
	}

	float FSineOsc::GetFrequency() const
	{ 
		return FrequencyHz; 
	}

	float FSineOsc::ProcessAudio()
	{
		// using direct-form difference equation
		// y(n) = -b1 * y(n - 1) - b2 * y(n - 2)
		const float Yn = -B1 * Yn_1 - B2 * Yn_2;

		// Move outputs down
		Yn_2 = Yn_1;
		Yn_1 = Yn;
		return Scale * Yn + Add;
	}
}
