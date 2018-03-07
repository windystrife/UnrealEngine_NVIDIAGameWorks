// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DSP/SampleBufferReader.h"
#include "AudioMixer.h"
#include "DSP/SinOsc.h"

namespace Audio
{
	FSampleBufferReader::FSampleBufferReader()
		: BufferPtr(nullptr)
		, BufferNumSamples(0)
		, BufferNumFrames(0)
		, BufferSampleRate(0)
		, BufferNumChannels(0)
		, DeviceSampleRate(0.0f)
		, BasePitch(1.0f)
		, PitchScale(1.0f)
		, CurrentFrameIndex(0)
		, NextFrameIndex(0)
		, AlphaLerp(0.0f)
		, CurrentBufferFrameIndexInterpolated(0.0f)
		, PlaybackProgress(0.0f)
		, ScrubAnchorFrame(0.0f)
		, ScrubMinFrame(0.0f)
		, ScrubMaxFrame(0.0f)
		, ScrubWidthFrames(0.0f)
		, bIsScrubMode(false)
		, bIsFinished(false)
	{
	}

	FSampleBufferReader::~FSampleBufferReader()
	{
	}

	void FSampleBufferReader::Init(const int32 InSampleRate)
	{
		DeviceSampleRate = InSampleRate;

		BufferPtr = nullptr;
		BufferNumSamples = 0;
		BufferNumFrames = 0;
		BufferSampleRate = 0;
		BufferNumChannels = 0;

		CurrentFrameIndex = 0;
		NextFrameIndex = 0;
		AlphaLerp = 0.0f;

		Pitch.Init(DeviceSampleRate);
		Pitch.SetValue(1.0, 0.0f);

		BasePitch = 1.0f;

		bIsFinished = false;
		CurrentBufferFrameIndexInterpolated = 0.0f;
		ScrubAnchorFrame = 0.0f;
		ScrubMinFrame = 0.0f;
		ScrubMaxFrame = 0.0f;

		// Default the scrub width to 0.1 seconds
		bIsScrubMode = false;
		ScrubWidthFrames = 0.1f * DeviceSampleRate;
		PlaybackProgress = 0.0f;
	}

	void FSampleBufferReader::SetBuffer(const int16** InBufferPtr, const int32 InNumBufferSamples, const int32 InNumChannels, const int32 InBufferSampleRate)
	{	
		BufferPtr = *InBufferPtr;
		BufferNumSamples = InNumBufferSamples;
		BufferNumChannels = InNumChannels;
		BufferSampleRate = InBufferSampleRate;
		BufferNumFrames = BufferNumSamples / BufferNumChannels;

		// This is the base pitch to use play at the "correct" sample rate for the buffer to sound correct on the output device sample rate
		BasePitch = BufferSampleRate / DeviceSampleRate;

		// Set the pitch to the previous pitch scale.
		Pitch.SetValueInterrupt(PitchScale * BasePitch);

		bIsFinished = false;
	}

	void FSampleBufferReader::ClearBuffer()
	{
		BufferPtr = nullptr;
		BufferNumSamples = 0;
		BufferNumChannels = 0;
		BufferSampleRate = 0;
		BufferNumFrames = 0;
	}

	void FSampleBufferReader::SeekFrame(const float InNumFrames, const ESeekType::Type InSeekType, const bool bWrap)
	{
		if (BufferPtr)
		{	
			if (InSeekType == ESeekType::FromBeginning)
			{
				CurrentBufferFrameIndexInterpolated = InNumFrames;
			}
			else if (InSeekType == ESeekType::FromEnd)
			{
				CurrentBufferFrameIndexInterpolated = (float)BufferNumFrames - InNumFrames;
			}
			else
			{
				CurrentBufferFrameIndexInterpolated += (float) InNumFrames;
			}

			if (bWrap)
			{
				while (CurrentBufferFrameIndexInterpolated > (float)BufferNumFrames)
				{
					CurrentBufferFrameIndexInterpolated -= (float)BufferNumFrames;
				}

				while (CurrentBufferFrameIndexInterpolated < 0.0f)
				{
					CurrentBufferFrameIndexInterpolated += (float)BufferNumFrames;
				}

				check(CurrentBufferFrameIndexInterpolated >= 0.0f && CurrentBufferFrameIndexInterpolated < (float)BufferNumFrames);
			}
			else
			{
				CurrentBufferFrameIndexInterpolated = FMath::Clamp(CurrentBufferFrameIndexInterpolated, 0.0f, (float)BufferNumFrames);
			}
		}

		ScrubAnchorFrame = CurrentBufferFrameIndexInterpolated;
		UpdateScrubMinAndMax();
	}

	void FSampleBufferReader::SeekTime(const float InTimeSec, const ESeekType::Type InSeekType, const bool bWrap)
	{
		// use buffer sample rate to compute a seek frame
		const float NumSeekSamples = (float)BufferSampleRate * InTimeSec;
		const float NumSeekFrames = NumSeekSamples / BufferNumChannels;
		SeekFrame(NumSeekFrames, InSeekType, bWrap);
	}

	void FSampleBufferReader::SetScrubTimeWidth(const float InScrubTimeWidthSec)
	{
		ScrubWidthFrames = DeviceSampleRate * FMath::Max(InScrubTimeWidthSec, 0.001f);
		ScrubWidthFrames = FMath::Min((float)(BufferNumFrames - 1), ScrubWidthFrames);

		UpdateScrubMinAndMax();
	}

	void FSampleBufferReader::SetPitch(const float InPitch, const float InterpolationTimeSec)
	{
		PitchScale = InPitch;
		Pitch.SetValue(PitchScale * BasePitch, InterpolationTimeSec);
	}

	void FSampleBufferReader::SetScrubMode(const bool bInIsScrubMode)
	{
		bIsScrubMode = bInIsScrubMode;
		
		// Anchor the current frame index as the scrub anchor
		ScrubAnchorFrame = CurrentBufferFrameIndexInterpolated;
		UpdateScrubMinAndMax();
	}

	void FSampleBufferReader::UpdateScrubMinAndMax()
	{
		if (BufferNumFrames > 0)
		{
			ScrubMinFrame = ScrubAnchorFrame - 0.5f * ScrubWidthFrames;
			ScrubMaxFrame = ScrubAnchorFrame + 0.5f * ScrubWidthFrames;

			while (ScrubMinFrame < 0.0f)
			{
				ScrubMinFrame += (float)BufferNumFrames;
			}

			while (ScrubMaxFrame > (float)BufferNumFrames)
			{
				ScrubMaxFrame -= (float)BufferNumFrames;
			}
		}
	}

	float FSampleBufferReader::GetSampleValue(const int16* InBuffer, const int32 SampleIndex)
	{
		int16 PCMSampleValue = InBuffer[SampleIndex];
		return (float)PCMSampleValue / 32767.0f;
	}

	bool FSampleBufferReader::Generate(float* OutAudioBuffer, const int32 NumFrames, const int32 OutChannels, const bool bWrap)
	{
		// Don't have a buffer yet, so fill in zeros, say we're not done
		if (!HasBuffer() || bIsFinished)
		{
			int32 NumSamples = NumFrames * OutChannels;
			for (int32 i = 0; i < NumSamples; ++i)
			{
				OutAudioBuffer[i] = 0.0f;
			}
			return false;
		}	

#if 0
		static FSineOsc SineOsc(48000.0f, 440.0f, 0.2f);

		int32 SampleIndex = 0;
		for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
		{
			const float Value = 0.1f * SineOsc.ProcessAudio();
			for (int32 Channel = 0; Channel < OutChannels; ++Channel)
			{
				OutAudioBuffer[SampleIndex++] = Value;
			}
		}
#else

		// We always want to wrap if we're in scrub mode
		const bool bDoWrap = bWrap || bIsScrubMode;

		int32 OutSampleIndex = 0;
		for (int32 i = 0; i < NumFrames && !bIsFinished; ++i)
		{
			float CurrentPitch = Pitch.GetValue();

			// Don't let the pitch go to 0.
			if (FMath::IsNearlyZero(CurrentPitch))
			{
				CurrentPitch = SMALL_NUMBER;
			}

			// We're going forward in the buffer
			if (CurrentPitch > 0.0f)
			{
				CurrentFrameIndex = FMath::FloorToInt(CurrentBufferFrameIndexInterpolated);
				NextFrameIndex = CurrentFrameIndex + 1;
				AlphaLerp = CurrentBufferFrameIndexInterpolated - (float)CurrentFrameIndex;

				// Check for boundary condition
				if (NextFrameIndex >= BufferNumFrames)
				{
					if (bDoWrap)
					{
						NextFrameIndex = 0;
					}
					else
					{
						bIsFinished = true;
					}
				}
			}
			else
			{
				CurrentFrameIndex = FMath::CeilToInt(CurrentBufferFrameIndexInterpolated);
				NextFrameIndex = CurrentFrameIndex - 1;
				AlphaLerp = (float)CurrentFrameIndex - CurrentBufferFrameIndexInterpolated;

				if (NextFrameIndex < 0)
				{
					if (bDoWrap)
					{
						NextFrameIndex = BufferNumFrames - 1;
					}
				}
			}

			if (!bIsFinished)
			{
				// Check for scrub boundaries and wrap. Note that we've already wrapped on the buffer boundary at this point.
				if (bIsScrubMode)
				{
					if (CurrentPitch > 0.0f && NextFrameIndex >= ScrubMaxFrame)
					{
						NextFrameIndex = ScrubMinFrame;
						CurrentFrameIndex = (int32)(ScrubMaxFrame - 1.0f);
						CurrentBufferFrameIndexInterpolated -= ScrubWidthFrames;
					}
					else if (NextFrameIndex < ScrubMinFrame)
					{
						NextFrameIndex = (int32)(ScrubMaxFrame - 1.0f);
						CurrentFrameIndex = (int32)ScrubMinFrame;
						CurrentBufferFrameIndexInterpolated += ScrubWidthFrames;
					}
				}

				if (OutChannels == BufferNumChannels)
				{
					for (int32 Channel = 0; Channel < BufferNumChannels; ++Channel)
					{
						OutAudioBuffer[OutSampleIndex++] = GetSampleValueForChannel(Channel);
					}
				}
				else if (OutChannels == 1 && BufferNumChannels == 2)
				{
					float LeftChannel = GetSampleValueForChannel(0);
					float RightChannel = GetSampleValueForChannel(1);
					OutAudioBuffer[OutSampleIndex++] = 0.5f * (LeftChannel + RightChannel);
				}
				else if (OutChannels == 2 && BufferNumChannels == 1)
				{
					float Sample = GetSampleValueForChannel(0);
					OutAudioBuffer[OutSampleIndex++] = 0.5f * Sample;
					OutAudioBuffer[OutSampleIndex++] = 0.5f * Sample;
				}

				CurrentBufferFrameIndexInterpolated += CurrentPitch;

				// Perform wrapping... if we're not finished but these hit, that means we're wrapping.
				if (CurrentBufferFrameIndexInterpolated >= (float)BufferNumFrames)
				{
					CurrentBufferFrameIndexInterpolated -= (float)BufferNumFrames;
				}
				else if (CurrentBufferFrameIndexInterpolated < 0.0f)
				{
					CurrentBufferFrameIndexInterpolated += (float)BufferNumFrames;
				}
			}
		}

		// Update the current playback time
		PlaybackProgress = CurrentBufferFrameIndexInterpolated / BufferSampleRate;

#endif
		return bIsFinished;
	}

	float FSampleBufferReader::GetSampleValueForChannel(const int32 Channel)
	{
		const int32 CurrentBufferSampleIndex = BufferNumChannels * CurrentFrameIndex + Channel;
		const int32 NextBufferSampleIndex = BufferNumChannels * NextFrameIndex + Channel;
		const float CurrentSampleValue = GetSampleValue(BufferPtr, CurrentBufferSampleIndex);
		const float NextSampleValue = GetSampleValue(BufferPtr, NextBufferSampleIndex);
		return FMath::Lerp(CurrentSampleValue, NextSampleValue, AlphaLerp);
	}

}


