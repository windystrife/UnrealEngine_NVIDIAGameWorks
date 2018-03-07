// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Osc.h"
#include "DSP/ModulationMatrix.h"

namespace Audio
{
	// Envelope class generates ADSR style envelope
	class AUDIOMIXER_API FEnvelope
	{
	public:
		FEnvelope();
		virtual ~FEnvelope();

		// Initialize the envelope with the given sample rate
		void Init(const float InSampleRate, const int32 InVoiceId = 0, FModulationMatrix* InModMatrix = nullptr, const bool bInSimulateAnalog = true);

		// Sets the envelope mode 
		void SetSimulateAnalog(const bool bInSimulatingAnalog);

		// Sets whether the envelope is in legato mode. Legato mode doesn't restart the envelope if it's already playing
		void SetLegato(const bool bInLegatoMode) { bIsLegatoMode = bInLegatoMode; }

		// Sets whether or not the envelope is zero'd when reset
		void SetRetrigger(const bool bInRetrigger) { bIsRetriggerMode = bInRetrigger; }

		// Start the envelope, puts envelope in attack state
		virtual void Start();

		// Stop the envelope, puts in the release state. Can optionally force to off state.
		virtual void Stop();

		// Puts envelope into shutdown mode, which is a much faster cutoff than release, but avoids pops
		virtual void Shutdown();

		// Kills the envelope (will cause discontinuity)
		virtual void Kill();

		// Queries if the envelope has finished
		virtual bool IsDone() const;

		// Resets the envelope
		virtual void Reset();

		// Update the state of the envelope
		virtual void Update();

		// Generate the next output value of the envelope.
		// Optionally outputs the bias output (i.e. -1.0 to 1.0)
		virtual float Generate(float* BiasedOutput = nullptr);

		// Sets the envelope attack time in msec
		virtual void SetAttackTime(const float InAttackTimeMsec);

		// Sets the envelope decay time in msec
		virtual void SetDecayTime(const float InDecayTimeMsec);

		// Sets the envelope sustain gain in linear gain values
		virtual void SetSustainGain(const float InSustainGain);

		// Sets the envelope release time in msec
		virtual void SetReleaseTime(const float InReleaseTimeMsec);

		// Inverts the value of envelope output
		virtual void SetInvert(const bool bInInvert);

		// Inverts the value of the biased envelope output
		virtual void SetBiasInvert(const bool bInBiasInvert);

		// Sets the envelope depth.
		virtual void SetDepth(const float InDepth);

		// Sets the depth of the bias output. 
		virtual void SetBiasDepth(const float InDepth);

		// Get the envelope's patch nodes
		const FPatchSource GetModSourceEnv() const { return EnvSource; }
		const FPatchSource GetModSourceBiasEnv() const { return BiasedEnvSource; }

	protected:

		// States for the envelope state machine
		enum class EEnvelopeState
		{
			Off,
			Attack,
			Decay,
			Sustain,
			Release,
			Shutdown
		};

		struct FEnvData
		{
			float Coefficient;
			float Offset;
			float TCO;
			float TimeSamples;

			FEnvData()
				: Coefficient(0.0f)
				, Offset(0.0f)
				, TCO(0.0f)
				, TimeSamples(0.0f)
			{}
		};

		// The current envelope value, used to compute exponential envelope curves
		int32 VoiceId;
		float CurrentEnvelopeValue;
		float CurrentEnvelopeBiasValue;
		float SampleRate;
		float AttackTimeMSec;
		float DecayTimeMsec;
		float SustainGain;
		float ReleaseTimeMsec;
		float ShutdownTimeMsec;
		float ShutdownDelta;
		float Depth;
		float BiasDepth;

		FEnvData AttackData;
		FEnvData DecayData;
		FEnvData ReleaseData;

		EEnvelopeState CurrentState;

		// Mod matrix
		FModulationMatrix* ModMatrix;

		FPatchSource EnvSource;
		FPatchSource BiasedEnvSource;

		// Whether or not we want to simulate analog behavior
		bool bIsSimulatingAnalog;

		// Whether or not the envelope is reset back to attack when started
		bool bIsLegatoMode;

		// Whether or not the envelope value is set to zero when finished
		bool bIsRetriggerMode;

		// Whether or not this envelope has changed and needs to have values recomputed
		bool bChanged;

		// Inverts the output
		bool bInvert;

		// Bias output inversions
		bool bBiasInvert;
	};
}
