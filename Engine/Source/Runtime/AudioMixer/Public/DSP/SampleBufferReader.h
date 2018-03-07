// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Dsp.h"
#include "DSP/SampleBuffer.h"

namespace Audio
{
	namespace ESeekType
	{
		enum Type
		{
			FromBeginning,
			FromCurrentPosition,
			FromEnd
		};
	}

	class AUDIOMIXER_API FSampleBufferReader
	{
	public:
		FSampleBufferReader();
		~FSampleBufferReader();

		void Init(const int32 InSampleRate);

		// This must be a completely loaded buffer. This buffer reader doesn't OWN the buffer memory.
		void SetBuffer(const int16** InBufferPtr, const int32 InNumBufferSamples, const int32 InNumChannels, const int32 InBufferSampleRate);

		// Seeks the buffer the given number of frames. Returns true if succeeded.
		void SeekFrame(const float InNumFrames, const ESeekType::Type InSeekType = ESeekType::FromBeginning, const bool bWrap = true);

		// Seeks the buffer the given time in seconds. Returns true if succeeded.
		void SeekTime(const float InTimeSec, const ESeekType::Type InSeekType = ESeekType::FromBeginning, const bool bWrap = true);

		// Sets the pitch of the buffer reader. Can be negative. Will linearly interpolate over the given time value.
		void SetPitch(const float InPitch, const float InterpolationTimeSec = 0.0f);

		// Puts the wave reader into scrub mode
		void SetScrubMode(const bool bInIsScrubMode);

		// Sets the scrub width. The sound will loop between the scrub width region and the current frame
		void SetScrubTimeWidth(const float InScrubTimeWidthSec);

		// Returns the number of channels of this buffer.
		int32 GetNumChannels() const { return BufferNumChannels; }

		// Returns the number of frames of the buffer.
		int32 GetNumFrames() const { return BufferNumFrames; }

		// Returns the current playback position in seconds
		float GetPlaybackProgress() const { return PlaybackProgress; }

		// Generates the next block of audio. Returns true if it's no longer playing (reached end of the buffer and not set to wrap)
		bool Generate(float* OutAudioBuffer, const int32 NumFrames, const int32 OutChannels, const bool bWrap = false);

		// Whether or not the buffer reader has a buffer
		bool HasBuffer() const { return BufferPtr != nullptr; }

		// Clears current buffer and resets state
		void ClearBuffer();

	protected:

		float GetSampleValueForChannel(const int32 Channel);
		void UpdateScrubMinAndMax();
		float GetSampleValue(const int16* InBuffer, const int32 SampleIndex);

		const int16* BufferPtr;
		int32 BufferNumSamples;
		int32 BufferNumFrames;
		int32 BufferSampleRate;
		int32 BufferNumChannels;

		float DeviceSampleRate;

		// The current frame alpha
		float BasePitch;
		float PitchScale;
		Audio::FLinearEase Pitch;

		int32 CurrentFrameIndex;
		int32 NextFrameIndex;
		float AlphaLerp;

		float CurrentBufferFrameIndexInterpolated;

		float PlaybackProgress;

		float ScrubAnchorFrame;
		float ScrubMinFrame;
		float ScrubMaxFrame;

		float ScrubWidthFrames;
		bool bIsScrubMode;
		bool bIsFinished;
	};

}

