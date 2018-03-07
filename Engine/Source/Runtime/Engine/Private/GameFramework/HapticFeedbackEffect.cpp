// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
//

#include "CoreMinimal.h"
#include "AudioDevice.h"
#include "Haptics/HapticFeedbackEffect_Base.h"
#include "Haptics/HapticFeedbackEffect_Curve.h"
#include "Sound/SoundWave.h"
#include "Haptics/HapticFeedbackEffect_SoundWave.h"
#include "Haptics/HapticFeedbackEffect_Buffer.h"


bool FActiveHapticFeedbackEffect::Update(const float DeltaTime, FHapticFeedbackValues& Values)
{
	if (HapticEffect == nullptr)
	{
		return false;
	}

	const float Duration = HapticEffect->GetDuration();
	PlayTime += DeltaTime;

	if ((PlayTime > Duration) || (Duration == 0.f))
	{
		return false;
	}

	HapticEffect->GetValues(PlayTime, Values);
	Values.Amplitude *= Scale;
	if (Values.HapticBuffer)
	{
		Values.HapticBuffer->ScaleFactor = Scale;
		if (Values.HapticBuffer->bFinishedPlaying)
		{
			return false;
		}
	}
	
	return true;
}

//==========================================================================
// UHapticFeedbackEffect_Base
//==========================================================================

UHapticFeedbackEffect_Base::UHapticFeedbackEffect_Base(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

void UHapticFeedbackEffect_Base::GetValues(const float EvalTime, FHapticFeedbackValues& Values)
{
}

float UHapticFeedbackEffect_Base::GetDuration() const
{
	return 0.f;
}

//==========================================================================
// UHapticFeedbackEffect_Curve
//==========================================================================

UHapticFeedbackEffect_Curve::UHapticFeedbackEffect_Curve(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UHapticFeedbackEffect_Curve::GetValues(const float EvalTime, FHapticFeedbackValues& Values)
{
	Values.Amplitude = HapticDetails.Amplitude.GetRichCurveConst()->Eval(EvalTime);
	Values.Frequency = HapticDetails.Frequency.GetRichCurveConst()->Eval(EvalTime);
}

float UHapticFeedbackEffect_Curve::GetDuration() const
{
	float AmplitudeMinTime, AmplitudeMaxTime;
	float FrequencyMinTime, FrequencyMaxTime;

	HapticDetails.Amplitude.GetRichCurveConst()->GetTimeRange(AmplitudeMinTime, AmplitudeMaxTime);
	HapticDetails.Frequency.GetRichCurveConst()->GetTimeRange(FrequencyMinTime, FrequencyMaxTime);

	return FMath::Max(AmplitudeMaxTime, FrequencyMaxTime);
}

//==========================================================================
// UHapticFeedbackEffect_Buffer
//==========================================================================

UHapticFeedbackEffect_Buffer::UHapticFeedbackEffect_Buffer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HapticBuffer.RawData.AddUninitialized(Amplitudes.Num());
	HapticBuffer.CurrentPtr = 0;
	HapticBuffer.BufferLength = Amplitudes.Num();
	HapticBuffer.SamplesSent = 0;
	HapticBuffer.bFinishedPlaying = false;
	HapticBuffer.SamplingRate = SampleRate;
}

UHapticFeedbackEffect_Buffer::~UHapticFeedbackEffect_Buffer()
{
	HapticBuffer.RawData.Empty();
}


void UHapticFeedbackEffect_Buffer::Initialize()
{
	HapticBuffer.CurrentPtr = 0;
	HapticBuffer.SamplesSent = 0;
	HapticBuffer.bFinishedPlaying = false;
}

void UHapticFeedbackEffect_Buffer::GetValues(const float EvalTime, FHapticFeedbackValues& Values)
{
	int ampidx = EvalTime * SampleRate;

	Values.Frequency = 1.0;
	Values.Amplitude = ampidx < Amplitudes.Num() ? (float)Amplitudes[ampidx] / 255.f : 0.f;
	Values.HapticBuffer = &HapticBuffer;
}

float UHapticFeedbackEffect_Buffer::GetDuration() const
{
	return (float)Amplitudes.Num() / SampleRate;
}

//==========================================================================
// UHapticFeedbackEffect_SoundWave
//==========================================================================

UHapticFeedbackEffect_SoundWave::UHapticFeedbackEffect_SoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bPrepared = false;
}

UHapticFeedbackEffect_SoundWave::~UHapticFeedbackEffect_SoundWave()
{
	HapticBuffer.RawData.Empty();
}

void UHapticFeedbackEffect_SoundWave::Initialize()
{
	if (!bPrepared)
	{
		PrepareSoundWaveBuffer();
	}
	HapticBuffer.CurrentPtr = 0;
	HapticBuffer.SamplesSent = 0;
	HapticBuffer.bFinishedPlaying = false;
}

void UHapticFeedbackEffect_SoundWave::GetValues(const float EvalTime, FHapticFeedbackValues& Values)
{
	int ampidx = EvalTime * HapticBuffer.BufferLength/ SoundWave->GetDuration();
	Values.Frequency = 1.0;
	Values.Amplitude = ampidx < HapticBuffer.BufferLength ? (float)HapticBuffer.RawData[ampidx] / 255.f : 0.f;
	Values.HapticBuffer = &HapticBuffer;
}

float UHapticFeedbackEffect_SoundWave::GetDuration() const
{
	return SoundWave ? SoundWave->GetDuration() : 0.f;
}

void UHapticFeedbackEffect_SoundWave::PrepareSoundWaveBuffer()
{
	FAudioDevice *AD = GEngine->GetMainAudioDevice();
	if (!AD || !SoundWave)
	{
		return;
	}
	AD->Precache(SoundWave, true, false);
	SoundWave->InitAudioResource(AD->GetRuntimeFormat(SoundWave));
	uint8* PCMData = SoundWave->RawPCMData;
	int32 SampleRate = SoundWave->SampleRate;
	int TargetFrequency = 320;
	int TargetBufferSize = (SoundWave->RawPCMDataSize * TargetFrequency) / (SampleRate * 2) + 1; //2 because we're only using half of the 16bit source PCM buffer
	HapticBuffer.BufferLength = TargetBufferSize;
	HapticBuffer.RawData.AddUninitialized(TargetBufferSize);
	HapticBuffer.CurrentPtr = 0;
	HapticBuffer.SamplingRate = TargetFrequency;

	int previousTargetIndex = -1;
	int currentMin = 0;
	for (int i = 1; i < SoundWave->RawPCMDataSize; i += 2)
	{
		int targetIndex = i * TargetFrequency / (SampleRate * 2);
		int val = PCMData[i];
		if (val & 0x80)
		{
			val = ~val;
		}
		currentMin = FMath::Min(currentMin, val);

		if (targetIndex != previousTargetIndex)
		{

			HapticBuffer.RawData[targetIndex] = val * 2;// *Scale;
			previousTargetIndex = targetIndex;
			currentMin = 0;
		}
	}
	bPrepared = true;

}
