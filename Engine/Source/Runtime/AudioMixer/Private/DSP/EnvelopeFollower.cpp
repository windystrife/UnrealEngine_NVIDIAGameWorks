// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DSP/EnvelopeFollower.h"
#include "DSP/Dsp.h"

namespace Audio
{
	// see https://en.wikipedia.org/wiki/RC_time_constant
	// Time constants indicate how quickly the envelope follower responds to changes in input
	static const float AnalogTImeConstant = 1.00239343f;
	static const float DigitalTimeConstant = 4.60517019f;

	FEnvelopeFollower::FEnvelopeFollower()
		: EnvMode(EPeakMode::Peak)	
		, SampleRate(44100.0f)
		, AttackTimeMsec(0.0f)
		, AttackTimeSamples(0.0f)
		, ReleaseTimeMsec(0.0f)
		, ReleaseTimeSamples(0.0f)
		, CurrentEnvelopeValue(0.0f)
		, bIsAnalog(true)
	{
	}

	FEnvelopeFollower::FEnvelopeFollower(const float InSampleRate, const float InAttackTimeMsec, const float InReleaseTimeMSec, const EPeakMode::Type InMode, const bool bInIsAnalg)
	{
		Init(InSampleRate, InAttackTimeMsec, InReleaseTimeMSec, InMode, bIsAnalog);
	}

	FEnvelopeFollower::~FEnvelopeFollower()
	{
	}

	void FEnvelopeFollower::Init(const float InSampleRate, const float InAttackTimeMsec, const float InReleaseTimeMSec, const EPeakMode::Type InMode, const bool bInIsAnalog)
	{
		SampleRate = InSampleRate;

		bIsAnalog = bInIsAnalog;
		EnvMode = InMode;

		// Set the attack and release times using the default values
		SetAttackTime(InAttackTimeMsec);
		SetReleaseTime(InReleaseTimeMSec);
	}

	void FEnvelopeFollower::Reset()
	{
		CurrentEnvelopeValue = 0.0f;
	}

	void FEnvelopeFollower::SetAnalog(const bool bInIsAnalog)
	{
		bIsAnalog = bInIsAnalog;
		SetAttackTime(AttackTimeMsec);
		SetReleaseTime(ReleaseTimeMsec);
	}

	void FEnvelopeFollower::SetAttackTime(const float InAttackTimeMsec)
	{
		AttackTimeMsec = InAttackTimeMsec;
		const float TimeConstant = bIsAnalog ? AnalogTImeConstant : DigitalTimeConstant;
		AttackTimeSamples = FMath::Exp(-1000.0f * TimeConstant / (AttackTimeMsec * SampleRate));
	}

	void FEnvelopeFollower::SetReleaseTime(const float InReleaseTimeMsec)
	{
		ReleaseTimeMsec = InReleaseTimeMsec;
		const float TimeConstant = bIsAnalog ? AnalogTImeConstant : DigitalTimeConstant;
		ReleaseTimeSamples = FMath::Exp(-1000.0f * TimeConstant / (InReleaseTimeMsec * SampleRate));
	}

	void FEnvelopeFollower::SetMode(const EPeakMode::Type InMode)
	{
		EnvMode = InMode;
	}

	float FEnvelopeFollower::ProcessAudio(const float InAudioSample)
	{
		// Take the absolute value of the input sample
		float Sample = FMath::Abs(InAudioSample);

		// If we're not Peak detecting, then square the input
		if (EnvMode != EPeakMode::Peak)
		{
			Sample = Sample * Sample;
		}

		float TimeSamples = (Sample > CurrentEnvelopeValue) ? AttackTimeSamples : ReleaseTimeSamples;
		float NewEnvelopeValue = TimeSamples * (CurrentEnvelopeValue - Sample) + Sample;;
		NewEnvelopeValue = Audio::UnderflowClamp(NewEnvelopeValue);
		NewEnvelopeValue = FMath::Clamp(NewEnvelopeValue, 0.0f, 1.0f);

		// Update and return the envelope value
		return CurrentEnvelopeValue = NewEnvelopeValue;
	}

	float FEnvelopeFollower::GetCurrentValue() const
	{
		return CurrentEnvelopeValue;
	}

}
