// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DSP/Envelope.h"
#include "DSP/Dsp.h"

namespace Audio
{
	FEnvelope::FEnvelope()
		: VoiceId(0)
		, CurrentEnvelopeValue(0.0f)
		, CurrentEnvelopeBiasValue(0.0f)
		, SampleRate(44100.0f)
		, AttackTimeMSec(100.0f)
		, DecayTimeMsec(100.0f)
		, SustainGain(0.7f)
		, ReleaseTimeMsec(2000.0f)
		, ShutdownTimeMsec(10.0f)
		, ShutdownDelta(0.0f)
		, Depth(1.0f)
		, BiasDepth(1.0f)
		, CurrentState(EEnvelopeState::Off)
		, ModMatrix(nullptr)
		, bIsSimulatingAnalog(true)
		, bIsLegatoMode(false)
		, bIsRetriggerMode(false)
		, bChanged(true)
		, bInvert(false)
		, bBiasInvert(false)
	{
	}

	FEnvelope::~FEnvelope()
	{
	}

	void FEnvelope::Init(const float InSampleRate, const int32 InVoiceId, FModulationMatrix* InModMatrix, const bool bInSimulateAnalog)
	{
		VoiceId = InVoiceId;
		SampleRate = InSampleRate;
		SetSimulateAnalog(bInSimulateAnalog);
		bChanged = true;

		ModMatrix = InModMatrix;
		if (ModMatrix)
		{
			EnvSource = ModMatrix->CreatePatchSource(VoiceId);
			BiasedEnvSource = ModMatrix->CreatePatchSource(VoiceId);

#if MOD_MATRIX_DEBUG_NAMES
			EnvSource.Name = TEXT("EnvSource");
			BiasedEnvSource.Name = TEXT("BiasedEnvSource");
#endif
		}
	}

	void FEnvelope::SetSimulateAnalog(const bool bInSimulatingAnalog)
	{
		bIsSimulatingAnalog = bInSimulatingAnalog;
		bChanged = true;
	}

	void FEnvelope::Start()
	{
		// Don't reset the envelope if we're in legato mode and we're not in release or off
		if (bIsLegatoMode && CurrentState != EEnvelopeState::Off && CurrentState != EEnvelopeState::Release)
		{
			return;
		}

		// Reset the envelope data
		Reset();

		// Set the state back to attack no matter where it is
		CurrentState = EEnvelopeState::Attack;
	}

	void FEnvelope::Stop()
	{
		// If we're forcing off or if we're already off, then set state to off
		if (CurrentEnvelopeValue == 0.0f)
		{
			CurrentState = EEnvelopeState::Off;
		}
		else
		{
			// If we actually have an envelope value now, go to release
			CurrentState = EEnvelopeState::Release;
		}
	}

	void FEnvelope::Shutdown()
	{
		if (bIsLegatoMode)
		{
			return;
		}

		// If we're forcing off or if we're already off, then set state to off
		if (CurrentEnvelopeValue == 0.0f)
		{
			CurrentState = EEnvelopeState::Off;
		}
		else
		{
			// If we actually have an envelope value now, go to release
			CurrentState = EEnvelopeState::Shutdown;

			ShutdownDelta = -(1000.0f * CurrentEnvelopeValue) / ShutdownTimeMsec / SampleRate;
		}
	}

	void FEnvelope::Kill()
	{
		CurrentState = EEnvelopeState::Off;
	}

	bool FEnvelope::IsDone() const
	{
		return CurrentState == EEnvelopeState::Off;
	}

	void FEnvelope::Reset()
	{
		// Set the envelope state to off when reset
		CurrentState = EEnvelopeState::Off;

		// Recompute the envelopes if needed
		SetSimulateAnalog(bIsSimulatingAnalog);

		bChanged = true;

		// If set to reset the envelope value to 0.0, set the envelope back to 0
		// Otherwise the envelope will continue to the target value from where it currently is
		if (bIsRetriggerMode)
		{
			CurrentEnvelopeValue = 0.0f;
		}
	}

	void FEnvelope::Update()
	{
		if (bChanged)
		{
			bChanged = false;

			if (bIsSimulatingAnalog)
			{
				// If in analog mode, we're going to emulate capacitor charging
				// Q = 1 - e^(-t/RC) for charging (attack)
				// Q = e^(-t/RC) for discharging
				AttackData.TCO = FMath::Exp(-1.5f);
				DecayData.TCO = FMath::Exp(-4.95f);
			}
			else
			{
				AttackData.TCO = 0.99999f;
				DecayData.TCO = FMath::Exp(-11.05f);
			}

			ReleaseData.TCO = DecayData.TCO;

			AttackData.TimeSamples = 0.001f * SampleRate * AttackTimeMSec;;
			DecayData.TimeSamples = 0.001f * SampleRate * DecayTimeMsec;
			ReleaseData.TimeSamples = 0.001f * SampleRate * ReleaseTimeMsec;

			AttackData.Coefficient = FMath::Exp(-FMath::Loge((1.0f + AttackData.TCO) / AttackData.TCO) / AttackData.TimeSamples);
			AttackData.Offset = (1.0f + AttackData.TCO) * (1.0f - AttackData.Coefficient);

			DecayData.Coefficient = FMath::Exp(-FMath::Loge((1.0f + DecayData.TCO) / DecayData.TCO) / DecayData.TimeSamples);
			DecayData.Offset = (SustainGain - DecayData.TCO)*(1.0f - DecayData.Coefficient);

			ReleaseData.Coefficient = FMath::Exp(-FMath::Loge((1.0f + ReleaseData.TCO) / ReleaseData.TCO) / ReleaseData.TimeSamples);
			ReleaseData.Offset = -ReleaseData.TCO*(1.0f - ReleaseData.Coefficient);
		}
	}

	float FEnvelope::Generate(float* BiasedOutput)
	{
		// Update the envelope if it changed
		Update();

		// Evaluate the finite state machine
		switch (CurrentState)
		{
			case EEnvelopeState::Off:
			{
				if (bIsRetriggerMode)
				{
					CurrentEnvelopeValue = 0.0f;
				}
			}
			break;

			case EEnvelopeState::Attack:
			{
				CurrentEnvelopeValue = AttackData.Offset + CurrentEnvelopeValue * AttackData.Coefficient;
				if (CurrentEnvelopeValue >= 1.0f || AttackTimeMSec <= 0.0f)
				{
					CurrentEnvelopeValue = 1.0f;
					CurrentState = EEnvelopeState::Decay;
					break;
				}
			}
			break;

			case EEnvelopeState::Decay:
			{
				// --- render value
				CurrentEnvelopeValue = DecayData.Offset + CurrentEnvelopeValue * DecayData.Coefficient;
				if (CurrentEnvelopeValue <= SustainGain || DecayTimeMsec <= 0.0f)
				{
					CurrentEnvelopeValue = SustainGain;
					CurrentState = EEnvelopeState::Sustain;
					break;
				}
			}
			break;

			case EEnvelopeState::Sustain:
				// Don't do anything in sustain state
				break;

			case EEnvelopeState::Release:
			{
				CurrentEnvelopeValue = ReleaseData.Offset + CurrentEnvelopeValue * ReleaseData.Coefficient;
				if (CurrentEnvelopeValue <= 0.0f || ReleaseTimeMsec <= 0.0f)
				{
					CurrentEnvelopeValue = 0.0f;
					CurrentState = EEnvelopeState::Off;
					break;
				}
			}
			break;

			case EEnvelopeState::Shutdown:
			{
				if (bIsRetriggerMode)
				{
					CurrentEnvelopeValue += ShutdownDelta;
					if (CurrentEnvelopeValue <= 0)
					{
						CurrentState = EEnvelopeState::Off;
						CurrentEnvelopeValue = 0.0f;
						break;
					}
				}
				else
				{
					CurrentState = EEnvelopeState::Off;
				}
			}
			break;
		}

		// Send the bias output (i.e. scale envelope by offset by sustain gain)
		float CurrentBiasedOutput = bBiasInvert ? 1.0f - CurrentEnvelopeValue : CurrentEnvelopeValue;
		CurrentBiasedOutput -= SustainGain;
		CurrentBiasedOutput *= BiasDepth;

		const float OutputEnvValue = bInvert ? 1.0f - CurrentEnvelopeValue : CurrentEnvelopeValue;
		CurrentEnvelopeValue *= Depth;

		if (BiasedOutput)
		{
			*BiasedOutput = CurrentBiasedOutput;
		}

		if (ModMatrix)
		{
			ModMatrix->SetSourceValue(VoiceId, EnvSource, CurrentEnvelopeValue);
			ModMatrix->SetSourceValue(VoiceId, BiasedEnvSource, CurrentBiasedOutput);
		}

		return CurrentEnvelopeValue;
	}

	void FEnvelope::SetAttackTime(const float InAttackTimeMsec)
	{
		AttackTimeMSec = InAttackTimeMsec;
		bChanged = true;
	}

	void FEnvelope::SetDecayTime(const float InDecayTimeMsec)
	{
		DecayTimeMsec = InDecayTimeMsec;
		bChanged = true;
	}

	void FEnvelope::SetSustainGain(const float InSustainGain)
	{
		SustainGain = InSustainGain;
		bChanged = true;
	}

	void FEnvelope::SetReleaseTime(const float InReleaseTimeMsec)
	{
		ReleaseTimeMsec = InReleaseTimeMsec;
		bChanged = true;
	}

	void FEnvelope::SetInvert(const bool bInInvert)
	{
		bInvert = bInInvert;
	}

	void FEnvelope::SetBiasInvert(const bool bInBiasInvert)
	{
		bBiasInvert = bInBiasInvert;
	}

	void FEnvelope::SetDepth(const float InDepth)
	{
		Depth = InDepth;
	}

	void FEnvelope::SetBiasDepth(const float InDepth)
	{
		BiasDepth = InDepth;
	}
}
