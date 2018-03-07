// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Dsp.h"

namespace Audio
{
	// Simple 1-pole lowpass filter
	class AUDIOMIXER_API FOnePoleLPF
	{
	public:

		// Constructor 
		FOnePoleLPF()
			: CutoffFrequency(0.0f)
			, B1(0.0f)
			, A0(1.0f)
			, Z1(0.0f)
		{}

		// Set the LPF gain coefficient
		FORCEINLINE void SetG(float InG)
		{ 
			B1 = InG;
			A0 = 1.0f - B1;
		}

		// Resets the sample delay to 0
		void Reset()
		{
			B1 = 0.0f;
			A0 = 1.0f;
			Z1 = 0.0f;
		}

		/** Sets the filter frequency using normalized frequency (between 0.0 and 1.0f or 0.0 hz and Nyquist Frequency in Hz) */
		FORCEINLINE void SetFrequency(const float InFrequency)
		{
			if (InFrequency != CutoffFrequency)
			{
				CutoffFrequency = InFrequency;
				B1 = FMath::Exp(-PI * CutoffFrequency);
				A0 = 1.0f - B1;
			}
		}

		float ProcessAudio(const float InputSample)
		{
			float Yn = InputSample*A0 + B1*Z1;
			Yn = UnderflowClamp(Yn);
			Z1 = Yn;
			return Yn;
		}

		// Process audio
		void ProcessAudio(const float* InputSample, float* OutputSample)
		{
			*OutputSample = ProcessAudio(*InputSample);
		}

	protected:
		float CutoffFrequency;

		// Filter coefficients
		float B1;
		float A0;

		// 1-sample delay
		float Z1;
	};

	// one pole LPF filter for multiple channels
	class FOnePoleLPFBank
	{
	public:
		FOnePoleLPFBank()
			: DelayPtr(nullptr)
			, NumChannels(1)
			, CutoffFrequency(-1.0f)
			, B1(0.0f)
			, A0(1.0f)
		{
			Z1.Init(0.0f, NumChannels);
			DelayPtr = Z1.GetData();
		}

		void Init(int32 InSampleRate, int32 InNumChannels)
		{
			SampleRate = InSampleRate;
			NumChannels = InNumChannels;
			CutoffFrequency = -1.0f;
			Z1.Init(0.0f, NumChannels);
			DelayPtr = Z1.GetData();
			Reset();
		}

		// Set the LPF gain coefficient
		FORCEINLINE void SetG(float InG)
		{
			B1 = InG;
			A0 = 1.0f - B1;
		}

		// Resets the sample delay to 0
		void Reset()
		{
			B1 = 0.0f;
			A0 = 1.0f;
			Z1.SetNumZeroed(NumChannels);
		}

		/** Sets the filter frequency using normalized frequency (between 0.0 and 1.0f or 0.0 hz and Nyquist Frequency in Hz) */
		void SetFrequency(const float InFrequency)
		{
			if (InFrequency != CutoffFrequency)
			{
				CutoffFrequency = InFrequency;
				float NormalizedFreq = FMath::Clamp(0.5f * InFrequency / SampleRate, 0.0f, 1.0f);
				B1 = FMath::Exp(-PI * NormalizedFreq);
				A0 = 1.0f - B1;
			}
		}

		FORCEINLINE void ProcessAudio(float* InputFrame, float* OutputFrame)
		{			
			for (int32 i = 0; i < NumChannels; ++i)
			{
				float Yn = InputFrame[i] * A0 + B1*DelayPtr[i];
				Yn = UnderflowClamp(Yn);
				DelayPtr[i] = Yn;
				OutputFrame[i] = Yn;
			}
		}

	protected:

		TArray<float> Z1;
		float* DelayPtr;
		int32 NumChannels;
		float CutoffFrequency;
		int32 SampleRate;
		float B1;
		float A0;
	};

}
