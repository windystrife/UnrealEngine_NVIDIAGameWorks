// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioSampleRateConverter.h"
#include "UnrealAudioUtilities.h"
#include "UnrealAudioPrivate.h"

#if ENABLE_UNREAL_AUDIO

#define AUDIO_MIN_SAMPLE_RATE_RATIO_MAGNITUDE	(0.0001f)
#define AUDIO_TARGET_SAMPLE_RATE_SMOOTHNESS		(0.001f)

namespace UAudio
{

	FSampleRateConverter::FSampleRateConverter()
		: NumChannels(0)
		, PrevFrameIndex(0)
		, NextFrameIndex(1)
		, CurrentRateRatio(1.0f)
		, TargetRateRatio(1.0f)
		, CurrentFrameFraction(0.0f)
		, bCachePrevValues(true)
	{
	}

	void FSampleRateConverter::Init(float InRate, int32 InNumChannels)
	{
		NumChannels = InNumChannels;
		PrevFrameIndex = 0;
		NextFrameIndex = 0;
		CurrentRateRatio = InRate;
		TargetRateRatio = InRate;
		CurrentFrameFraction = 0.0f;
		bCachePrevValues = true;

		DEBUG_AUDIO_CHECK(NumChannels > 0);
		PrevFrameValues.Init(0.0f, NumChannels);
		NextFrameValues.Init(0.0f, NumChannels);
	}

	void FSampleRateConverter::SetRateRatio(float InRate)
	{
		// We can't have a 0.0f rate ratio...
		if (FMath::Abs(InRate) < AUDIO_MIN_SAMPLE_RATE_RATIO_MAGNITUDE)
		{
			if (InRate < 0.0f)
			{
				TargetRateRatio = -AUDIO_MIN_SAMPLE_RATE_RATIO_MAGNITUDE;
			}
			else
			{
				TargetRateRatio = AUDIO_MIN_SAMPLE_RATE_RATIO_MAGNITUDE;
			}
		}
		else
		{
			TargetRateRatio = InRate;
		}
	}

	float FSampleRateConverter::GetRateRatio() const
	{
		return CurrentRateRatio;
	}

	void FSampleRateConverter::ProcessBlock(const float* InputBuffer, int32 NumInputSamples, TArray<float>& OutputBuffer)
	{
		DEBUG_AUDIO_CHECK(TargetRateRatio != 0.0);

		// If our input buffer is empty, then we have nothing to do
		if (NumInputSamples == 0)
		{
			return;
		}

		// Our input buffer is interleaved samples, so frame is samples divided by channels
		int32 InputFrames = NumInputSamples / NumChannels;

		bool bFinished = false;
		while (!bFinished)
		{
			if (CurrentRateRatio > 0.0)
			{
				ProcessForward(bFinished, InputBuffer, InputFrames, OutputBuffer);
			}
			else
			{
				ProcessBackward(bFinished, InputBuffer, InputFrames, OutputBuffer);
			}
		}
	}

	void FSampleRateConverter::ProcessForward(bool& bFinished, const float* InputBuffer, int32 InputFrames, TArray<float>& OutputBuffer)
	{
		// Process the input block until there's no more frames to process
		while (CurrentRateRatio > 0.0f && PrevFrameIndex < InputFrames && NextFrameIndex < InputFrames)
		{
			DEBUG_AUDIO_CHECK(CurrentFrameFraction >= 0.0f && CurrentFrameFraction < 1.0f);

			int32 PrevSampleIndex = NumChannels * PrevFrameIndex;
			int32 NextSampleIndex = NumChannels * NextFrameIndex;

			for (int32 Channel = 0; Channel < NumChannels; ++Channel)
			{
				if (bCachePrevValues)
				{
					DEBUG_AUDIO_CHECK(PrevSampleIndex >= 0);
					PrevFrameValues[Channel] = InputBuffer[PrevSampleIndex + Channel];
				}
				DEBUG_AUDIO_CHECK(NextSampleIndex >= 0);
				NextFrameValues[Channel] = InputBuffer[NextSampleIndex + Channel];
			}

			bCachePrevValues = true;

			// while our current frame fraction is not another frame and our rate ratio is still positive
			while (CurrentFrameFraction < 1.0f && CurrentRateRatio > 0.0f)
			{
				// Perform a sample-rate conversion on the input block 
				for (int32 Channel = 0; Channel < NumChannels; ++Channel)
				{
					// linear interpolate
					float ChannelValue = PrevFrameValues[Channel] * (1.0 - CurrentFrameFraction) + CurrentFrameFraction * NextFrameValues[Channel];
					OutputBuffer.Add(ChannelValue);
				}
				CurrentFrameFraction += CurrentRateRatio;
				if (CurrentRateRatio != TargetRateRatio)
				{
					CurrentRateRatio = CurrentRateRatio + AUDIO_TARGET_SAMPLE_RATE_SMOOTHNESS * (TargetRateRatio - CurrentRateRatio);
				}
			}

			// Wrap the fraction to less than 1.0. This can be potentially multiples above 1.0
			// for sample rates that significantly less than the input file (e.g. 8k vs 48k)
			while (CurrentFrameFraction >= 1.0f)
			{
				++PrevFrameIndex;
				++NextFrameIndex;
				CurrentFrameFraction -= 1.0f;
			}
		}

		// If we didn't change directions in this process block
		if (CurrentRateRatio > 0.0f)
		{
			// we need to cache the last value of this buffer if we'll need it in the next block
			if (PrevFrameIndex < InputFrames)
			{
				bCachePrevValues = false;
				for (int32 Channel = 0; Channel < NumChannels; ++Channel)
				{
					PrevFrameValues[Channel] = NextFrameValues[Channel];
				}
			}

			// update our frame index relative to the last InputFrame count
			// Note: This could result in a negative PrevFrameIndex if PrevFrameIndex was less than InputFrames
			PrevFrameIndex -= InputFrames;
			NextFrameIndex = PrevFrameIndex + 1;

			bFinished = true;
		}
	}

	void FSampleRateConverter::ProcessBackward(bool& bFinished, const float* InputBuffer, int32 NumFrames, TArray<float>& OutputBuffer)
	{
		// TODO: do backward processing algorithm
		bFinished = true;
	}



}

#endif // #if ENABLE_UNREAL_AUDIO

