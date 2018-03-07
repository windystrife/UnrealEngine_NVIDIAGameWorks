// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealAudioTypes.h"

#if ENABLE_UNREAL_AUDIO


namespace UAudio
{

	class FSampleRateConverter
	{
	public:
		FSampleRateConverter();

		void Init(float InRate, int32 InNumChannels);

		void SetRateRatio(float InRate);
		float GetRateRatio() const;
		void ProcessBlock(const float* InputBuffer, int32 NumInputSamples, TArray<float>& OutputBuffer);

	private:
		void ProcessForward(bool& bFinished, const float* InputBuffer, int32 InputFrames, TArray<float>& OutputBuffer);
		void ProcessBackward(bool& bFinished, const float* InputBuffer, int32 InputFrames, TArray<float>& OutputBuffer);

		TArray<float> PrevFrameValues;
		TArray<float> NextFrameValues;

		int32 NumChannels;

		int32 PrevFrameIndex;
		int32 NextFrameIndex;
		float CurrentRateRatio;
		float TargetRateRatio;
		float CurrentFrameFraction;
		bool bCachePrevValues;
	};


}

#endif // #if ENABLE_UNREAL_AUDIO

