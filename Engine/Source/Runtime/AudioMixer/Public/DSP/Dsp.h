// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScopeLock.h"

// Macros which can be enabled to cause DSP sample checking
#if 0
#define CHECK_SAMPLE(VALUE) 
#define CHECK_SAMPLE2(VALUE)
#else
#define CHECK_SAMPLE(VALUE)  Audio::CheckSample(VALUE)
#define CHECK_SAMPLE2(VALUE)  Audio::CheckSample(VALUE)
#endif

namespace Audio
{
	// Utility to check for sample clipping. Put breakpoint in conditional to find 
	// DSP code that's not behaving correctly
	static void CheckSample(float InSample, float Threshold = 0.001f)
	{	
		if (InSample > Threshold || InSample < -Threshold)
		{
			UE_LOG(LogTemp, Log, TEXT("SampleValue Was %.2f"), InSample);
		}
	}

	// Clamps floats to 0 if they are in sub-normal range
	static FORCEINLINE float UnderflowClamp(const float InValue)
	{
		if (InValue > -FLT_MIN && InValue < FLT_MIN)
		{
			return 0.0f;
		}
		return InValue;
	}

	// Function converts linear scale volume to decibels
	static FORCEINLINE float ConvertToDecibels(const float InLinear)
	{
		return 20.0f * FMath::LogX(10.0f, FMath::Max(InLinear, SMALL_NUMBER));
	}

	// Function converts decibel to linear scale
	static FORCEINLINE float ConvertToLinear(const float InDecibels)
	{
		return FMath::Pow(10.0f, InDecibels / 20.0f);
	}

	// Given a velocity value [0,127], return the linear gain
	static FORCEINLINE float GetGainFromVelocity(const float InVelocity)
	{
		if (InVelocity == 0.0f)
		{
			return 0.0f;
		}
		return (InVelocity * InVelocity) / (127.0f * 127.0f);
	}

	// Low precision, high performance approximation of sine using parabolic polynomial approx
	static FORCEINLINE float FastSin(const float X)
	{
		return (4.0f * X) / PI * (1.0f - FMath::Abs(X) / PI);
	}

	// Slightly higher precision, high performance approximation of sine using parabolic polynomial approx
	static FORCEINLINE float FastSin2(const float X)
	{
		float X2 = FastSin(X);
		X2 = 0.225f * (X2* FMath::Abs(X2) - X2) + X2;
		return X2;
	}

	// Sine approximation using Bhaskara I technique discovered in 7th century. 
	// https://en.wikipedia.org/wiki/Bh%C4%81skara_I
	static FORCEINLINE float FastSin3(const float X)
	{
		// Component used to get negative radians
		const float SafeX = X < 0.0f ? FMath::Min(X, -SMALL_NUMBER) : FMath::Max(X, SMALL_NUMBER);
		const float Temp = SafeX * SafeX / FMath::Abs(SafeX);
		const float Numerator = 16.0f * SafeX * (PI - Temp);
		const float Denominator = 5.0f * PI * PI - 4.0f * Temp * (PI - Temp);
		return Numerator / Denominator;
	}

	// Fast tanh based on pade approximation
	static FORCEINLINE float FastTanh(float X)
	{
		if (X < -3) return -1.0f;
		if (X > 3) return 1.0f;
		const float InputSquared = X*X;
		return X*(27.0f + InputSquared) / (27.0f + 9.0f * InputSquared);
	}

	// Based on sin parabolic approximation
	static FORCEINLINE float FastTan(float X)
	{
		const float Num = X * (1.0f - FMath::Abs(X) / PI);
		const float Den = (X + 0.5f * PI) * (1.0f - FMath::Abs(X + 0.5f * PI) / PI);
		return Num / Den;
	}

	// Gets polar value from unipolar
	static FORCEINLINE float GetBipolar(const float X)
	{
		return 2.0f * X - 1.0f;
	}

	// Converts bipolar value to unipolar
	static FORCEINLINE float GetUnipolar(const float X)
	{
		return 0.5f * X + 0.5f;
	}

	// Using midi tuning standard, compute frequency in hz from midi value
	static FORCEINLINE float GetFrequencyFromMidi(const float InMidiNote)
	{
		return 440.0f * FMath::Pow(2.0f, (InMidiNote - 69.0f) / 12.0f);
	}

	// Returns the log frequency of the input value. Maps linear domain and range values to log output (good for linear slider controlling frequency)
	static FORCEINLINE float GetLogFrequencyClamped(const float InValue, const FVector2D& Domain, const FVector2D& Range)
	{
		const float InValueCopy = FMath::Clamp<float>(InValue, Domain.X, Domain.Y);
		const FVector2D RangeLog(FMath::Loge(Range.X), FMath::Loge(Range.Y));

		check(Domain.Y != Domain.X);
		const float Scale = (RangeLog.Y - RangeLog.X) / (Domain.Y - Domain.X);

		return FMath::Exp(RangeLog.X + Scale * (InValueCopy - Domain.X));
	}

	// Using midi tuning standard, compute midi from frequency in hz
	static FORCEINLINE float GetMidiFromFrequency(const float InFrequency)
	{
		return 69.0f + 12.0f * FMath::LogX(2.0f, InFrequency / 440.0f);
	}

	// Return a pitch scale factor based on the difference between a base midi note and a target midi note. Useful for samplers.
	static FORCEINLINE float GetPitchScaleFromMIDINote(int32 BaseMidiNote, int32 TargetMidiNote)
	{
		const float BaseFrequency = GetFrequencyFromMidi(FMath::Clamp((float)BaseMidiNote, 0.0f, 127.0f));
		const float TargetFrequency = 440.0f * FMath::Pow(2.0f, ((float)TargetMidiNote - 69.0f) / 12.0f);
		const float PitchScale = TargetFrequency / BaseFrequency;
		return PitchScale;
	}

	// Returns the frequency multipler to scale a base frequency given the input semitones
	static FORCEINLINE float GetFrequencyMultiplier(const float InPitchSemitones)
	{
		if (InPitchSemitones == 0.0f)
		{
			return 1.0f;

		}
		return FMath::Pow(2.0f, InPitchSemitones / 12.0f);
	}

	// Calculates equal power stereo pan using sinusoidal-panning law and cheap approximation for sin
	// InLinear pan is [-1.0, 1.0] so it can be modulated by a bipolar LFO
	static FORCEINLINE void GetStereoPan(const float InLinearPan, float& OutLeft, float& OutRight)
	{
		const float LeftPhase = 0.25f * PI * (InLinearPan + 1.0f);
		const float RightPhase = 0.5f * PI * (0.5f * (InLinearPan + 1.0f) + 1.0f);
		OutLeft = FMath::Clamp(FastSin(LeftPhase), 0.0f, 1.0f);
		OutRight = FMath::Clamp(FastSin(RightPhase), 0.0f, 1.0f);
	}

	// Helper function to get bandwidth from Q
	static FORCEINLINE float GetBandwidthFromQ(const float InQ)
	{
		// make sure Q is not 0.0f, clamp to slightly positive
		const float Q = FMath::Max(KINDA_SMALL_NUMBER, InQ);
		const float Arg = 0.5f * ((1.0f / Q) + FMath::Sqrt(1.0f / (Q*Q) + 4.0f));
		const float OutBandwidth = 2.0f * FMath::LogX(2.0f, Arg);
		return OutBandwidth;
	}

	// Helper function get Q from bandwidth
	static FORCEINLINE float GetQFromBandwidth(const float InBandwidth)
	{
		const float InBandwidthClamped = FMath::Max(KINDA_SMALL_NUMBER, InBandwidth);
		const float Temp = FMath::Pow(2.0f, InBandwidthClamped);
		const float OutQ = FMath::Sqrt(Temp) / (Temp - 1.0f);
		return OutQ;
	}

	// Polynomial interpolation using lagrange polynomials. 
	// https://en.wikipedia.org/wiki/Lagrange_polynomial
	static FORCEINLINE float LagrangianInterpolation(const TArray<FVector2D> Points, const float Alpha)
	{
		float Lagrangian = 1.0f;
		float Output = 0.0f;

		const int32 NumPoints = Points.Num();
		for (int32 i = 0; i < NumPoints; ++i)
		{
			Lagrangian = 1.0f;
			for (int32 j = 0; j < NumPoints; ++j)
			{
				if (i != j)
				{
					float Denom = Points[i].X - Points[j].X;
					if (FMath::Abs(Denom) < SMALL_NUMBER)
					{
						Denom = SMALL_NUMBER;
					}
					Lagrangian *= (Alpha - Points[j].X) / Denom;
				}
			}
			Output += Lagrangian * Points[i].Y;
		}
		return Output;
	}

	// Simple exponential easing class. Useful for cheaply and smoothly interpolating parameters.
	class FExponentialEase
	{
	public:
		FExponentialEase(float InInitValue = 0.0f, float InEaseFactor = 0.001f, float InThreshold = KINDA_SMALL_NUMBER)
			: CurrentValue(InInitValue)
			, Threshold(InThreshold)
			, TargetValue(InInitValue)
			, EaseFactor(InEaseFactor)
		{
		}

		void Init(float InInitValue, float InEaseFactor = 0.001f)
		{
			CurrentValue = InInitValue;
			TargetValue = InInitValue;
		}

		bool IsDone() const
		{
			return FMath::Abs(TargetValue - CurrentValue) < Threshold;
		}

		float GetValue()
		{
			if (IsDone())
			{
				return CurrentValue;
			}

			return CurrentValue = CurrentValue + (TargetValue - CurrentValue) * EaseFactor;
		}

		void SetEaseFactor(const float InEaseFactor)
		{
			EaseFactor = InEaseFactor;
		}

		void operator=(const float& InValue)
		{
			SetValue(InValue);
		}

		void SetValue(const float InValue, const bool bIsInit = false)
		{
			TargetValue = InValue;
			if (bIsInit)
			{
				CurrentValue = TargetValue;
			}
		}

	private:

		// Current value of the exponential ease
		float CurrentValue;

		// Threshold to use to evaluate if the ease is done
		float Threshold;

		// Target value
		float TargetValue;

		// Percentage to move toward target value from current value each tick
		float EaseFactor;
	};
	
	// Simple easing function used to help interpolate params
	class FLinearEase 
	{
	public:
		FLinearEase()
			: StartValue(0.0f)
			, CurrentValue(0.0f)
			, DeltaValue(0.0f)
			, SampleRate(44100.0f)
			, DurationTicks(0)
			, DefaultDurationTicks(0)
			, CurrentTick(0)
			, bIsInit(true)
		{
		}

		~FLinearEase()
		{
		}

		bool IsDone() const
		{
			return CurrentTick >= DurationTicks;
		}

		void Init(float InSampleRate)
		{
			SampleRate = InSampleRate;
			bIsInit = true;
		}

		void SetValueRange(const float Start, const float End, const float InTimeSec)
		{
			StartValue = Start;
			CurrentValue = Start;
			SetValue(End, InTimeSec);
		}

		float GetValue()
		{
			if (IsDone())
			{
				return CurrentValue;
			}

			CurrentValue = DeltaValue * (float) CurrentTick / DurationTicks + StartValue;

			++CurrentTick;
			return CurrentValue;
		}
		 
		// Updates the target value without changing the duration or tick data.
		// Sets the state as if the new value was the target value all along
		void SetValueInterrupt(const float InValue)
		{
			if (IsDone())
			{
				CurrentValue = InValue;
			}
			else
			{
				DurationTicks = DurationTicks - CurrentTick;
				CurrentTick = 0;
				DeltaValue = InValue - CurrentValue;
				StartValue = CurrentValue;
			}
		}

		void SetValue(const float InValue, float InTimeSec = 0.0f)
		{
			if (bIsInit)
			{
				bIsInit = false;
				DurationTicks = 0;
			}
			else
			{
				DurationTicks = (int32)(SampleRate * InTimeSec);
			}
			CurrentTick = 0;

			if (DurationTicks == 0)
			{
				CurrentValue = InValue;			
			}
			else
			{
				DeltaValue = InValue - CurrentValue;
				StartValue = CurrentValue;
			}
		}

	private:
		float StartValue;
		float CurrentValue;
		float DeltaValue;
		float SampleRate;
		int32 DurationTicks;
		int32 DefaultDurationTicks;
		int32 CurrentTick;
		bool bIsInit;
	};

	// Simple parameter object which uses critical section to write to and read from data
	template<typename T>
	class TParams
	{
	public:
		TParams()
			: bChanged(false)
		{}

		// Sets the params
		void SetParams(const T& InParams)
		{
			FScopeLock Lock(&CritSect);
			bChanged = true;
			CurrentParams = InParams;
		}

		// Returns a copy of the params safely if they've changed since last time this was called
		bool GetParams(T* OutParamsCopy)
		{
			FScopeLock Lock(&CritSect);
			if (bChanged)
			{
				bChanged = false;
				*OutParamsCopy = CurrentParams;
				return true;
			}
			return false;
		}

		bool bChanged;
		T CurrentParams;
		FCriticalSection CritSect;
	};

}

