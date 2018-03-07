// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DSP/LFO.h"
#include "DSP/Dsp.h"

namespace Audio
{
	FLFO::FLFO()
		: LFOType(ELFO::Sine)
		, LFOMode(ELFOMode::Sync)
		, ExponentialFactor(3.5f)
		, RSHCounter(INDEX_NONE)
		, RSHValue(0.0f)
		, ModScale(1.0f)
		, ModAdd(0.0f)
		, LastOutput(0.0f)
		, QuadLastOutput(0.0f)
	{
	}

	FLFO::~FLFO()
	{
	}

	void FLFO::Init(const float InSampleRate, const int32 InVoiceId, FModulationMatrix* InMatrix, const int32 ModMatrixStage)
	{
		IOscBase::Init(InSampleRate, InVoiceId, InMatrix, ModMatrixStage);

		if (ModMatrix)
		{
			ModNormalPhase = ModMatrix->CreatePatchSource(InVoiceId);
			ModQuadPhase = ModMatrix->CreatePatchSource(InVoiceId);

#if MOD_MATRIX_DEBUG_NAMES
			ModNormalPhase.Name = TEXT("ModNormalPhase");
			ModQuadPhase.Name = TEXT("ModQuadPhase");
#endif
		}
	}

	void FLFO::Start()
	{
		if (LFOMode == ELFOMode::Sync || LFOMode == ELFOMode::OneShot)
		{
			Reset();
		}

		bIsPlaying = true;
	}

	void FLFO::Stop()
	{
		bIsPlaying = false;
	}

	void FLFO::Reset()
	{
		// reset base class first
		IOscBase::Reset();

		RSHValue = 0.0f;
		RSHCounter = INDEX_NONE;
	}
	
	float FLFO::Generate(float* QuadPhaseOutput)
	{
		// If the LFO isn't playing, return 0.0 for quad phase and for normal output
		if (!bIsPlaying)
		{
			if (QuadPhaseOutput)
			{
				*QuadPhaseOutput = QuadLastOutput;
			}

			return LastOutput;
		}

		const bool bWrapped = WrapPhase();

		// If we're in the oneshot mode, check if we've wrapped
		// and if so, turn the LFO off and return 0.0s
		if (LFOMode == ELFOMode::OneShot && bWrapped)
		{
			bIsPlaying = false;

			if (QuadPhaseOutput)
			{
				*QuadPhaseOutput = QuadLastOutput;
			}

			return LastOutput;
		}

		LastOutput = ComputeLFO(GetPhase(), QuadPhaseOutput);

		// Update the LFO phase after computing LFO values
		UpdatePhase();

		// Return the output
		return LastOutput;
	}

	float FLFO::ComputeLFO(const float InPhase, float* OutQuad)
	{
		float Output = 0.0f;
		float QuadOutput = 0.0f;

		float QuadPhase = InPhase + 0.25f;
		if (QuadPhase >= 1.0f)
		{
			QuadPhase -= 1.0f;
		}

		switch (LFOType)
		{
			case ELFO::Sine:
			{
				float Angle = 2.0f * InPhase * PI - PI;
				Output = Audio::FastSin(Angle);

				Angle = 2.0f * QuadPhase * PI - PI;
				QuadOutput = Audio::FastSin(Angle);
			}
			break;

			case ELFO::UpSaw:
			{
				Output = GetBipolar(InPhase);
				QuadOutput = GetBipolar(QuadPhase);
			}
			break;

			case ELFO::DownSaw:
			{
				Output = -1.0f * GetBipolar(InPhase);
				QuadOutput = -1.0f * GetBipolar(QuadPhase);
			}
			break;

			case ELFO::Square:
			{
				Output = InPhase > PulseWidth ? -1.0f : 1.0f;
				QuadOutput = QuadPhase > PulseWidth ? -1.0f : 1.0f;
			}
			break;

			case ELFO::Triangle:
			{
				Output = FMath::Abs(GetBipolar(InPhase));
				QuadOutput = FMath::Abs(GetBipolar(QuadPhase));;

				// If not one-shot, we need to convert to bipolar
				if (LFOMode != ELFOMode::OneShot)
				{
					Output = GetBipolar(Output);
					QuadOutput = GetBipolar(QuadOutput);
				}
			}
			break;

			case ELFO::Exponential:
			{
				Output = FMath::Pow(InPhase, ExponentialFactor);
				QuadOutput = FMath::Pow(QuadPhase, ExponentialFactor);
			}
			break;

			case ELFO::RandomSampleHold:
			{
				const float FrequencyThreshold = SampleRate / Freq;
				if (RSHCounter > (uint32)FrequencyThreshold)
				{
					RSHCounter = 0;
					RSHValue = FMath::FRandRange(-1.0f, 1.0f);
				}
				else
				{
					++RSHCounter;
				}

				Output = RSHValue;
				QuadOutput = RSHValue;
			}
			break;
		}

		const float MaxGain = Gain * ExternalGainMod;
		Output = Output * MaxGain;
		QuadOutput = QuadOutput * MaxGain;

		// If we have a mod matrix, then mix in the destination data
		// This allows LFO's (or envelopes, etc) to modulation this LFO
		if (ModMatrix)
		{
			ModMatrix->GetDestinationValue(VoiceId, ModScaleDest, ModAdd);
			ModMatrix->GetDestinationValue(VoiceId, ModAddDest, ModScale);

			Output = Output * ModScale + ModAdd;
			QuadOutput = QuadOutput * ModScale + ModAdd;

			// Write out the modulations
			ModMatrix->SetSourceValue(VoiceId, ModNormalPhase, Output);
			ModMatrix->SetSourceValue(VoiceId, ModQuadPhase, QuadOutput);
		}

		QuadLastOutput = QuadOutput;

		if (OutQuad)
		{
			*OutQuad = QuadOutput;
		}

		return Output;
	}
}
