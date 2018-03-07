// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Osc.h"
#include "DSP/ModulationMatrix.h"

namespace Audio
{
	namespace ELFO
	{
		enum Type
		{
			Sine,
			UpSaw,
			DownSaw,
			Square,
			Triangle,
			Exponential,
			RandomSampleHold,

			NumLFOTypes
		};
	}

	namespace ELFOMode
	{
		enum Type
		{
			// Constantly oscillates
			Sync,

			// Performs the LFO only once, then stops
			OneShot,

			// Doesn't restart the phase of the LFO on subsequent calls to Start
			Free,

			NumLFOModes
		};
	}

	// Low frequency oscillator
	class AUDIOMIXER_API FLFO : public IOscBase
	{
	public:
		FLFO();
		virtual ~FLFO();

		//~ Begin FOscBase
		virtual void Init(const float InSampleRate, const int32 InVoiceId = 0, FModulationMatrix* InMatrix = nullptr, const int32 ModMatrixStage = 0) override;
		virtual void Start() override;
		virtual void Stop() override;
		virtual void Reset() override;
		virtual float Generate(float* QuadPhaseOutput = nullptr) override;
		//~ End FOscBase

		void SetType(const ELFO::Type InLFOType) { LFOType = InLFOType; }
		ELFO::Type GetType() const { return LFOType; }
		void SetMode(const ELFOMode::Type InLFOMode) { LFOMode = InLFOMode; }
		ELFOMode::Type GetMode() const { return LFOMode; }
		void SetExponentialFactor(const float InExpFactor) { ExponentialFactor = FMath::Min(InExpFactor, 1.0f); }
		FPatchSource GetModSourceNormalPhase() const { return ModNormalPhase; }
		FPatchSource GetModSourceQuadPhase() const { return ModQuadPhase; }

	protected:

		float ComputeLFO(const float InputPhase, float* OutQuad = nullptr);

		ELFO::Type LFOType;
		ELFOMode::Type LFOMode;
		float ExponentialFactor;
		uint32 RSHCounter;
		float RSHValue;
		float ModScale;
		float ModAdd;
		float LastOutput;
		float QuadLastOutput;

		FPatchSource ModNormalPhase;
		FPatchSource ModQuadPhase;
	};

}
