// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Delegates/Delegate.h"
#include "MediaSampleSource.h"
#include "Templates/SharedPointer.h"

class IMediaAudioSample;


/**
 *
 */
class MEDIAUTILS_API FMediaAudioResampler
{
public:

	/** Default constructor. */
	FMediaAudioResampler();

public:

	/** Flush the resampler. */
	void Flush();

	/**
	 * Generate the next frame of audio.
	 *
	 * @param Output The output sample buffer (will be incremented by the number of sample values written).
	 * @param FramesRequested The maximum number of frames to get.
	 * @param Rate The current play rate.
	 * @param Time The current play time.
	 * @param SampleSource The object that provides more audio samples if needed.
	 * @return The actual number of frames returned.
	 */
	uint32 Generate(float* Output, const uint32 FramesRequested, float Rate, FTimespan Time, FMediaAudioSampleSource& SampleSource);

	/**
	 * Initialize the resampler.
	 *
	 * @param InOutputSampleRate Desired output sample rate.
	 * @see SetAudioSamples
	 */
	void Initialize(const uint32 InOutputChannels, const uint32 InOutputSampleRate);

protected:

	/**
	 * Clear the input samples.
	 *
	 * @see SetInput
	 */
	void ClearInput();

	/**
	 * Set the audio sample to be resampled.
	 *
	 * @param Sample The input audio sample.
	 * @return true on success, false otherwise.
	 * @see ClearInput
	 */
	bool SetInput(const TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe>& Sample);

private:

	//~ Frame interpolation values

	/** Cached sample values of the current frame. */
	float CurrentFrame[8];

	/** Cached sample values of the next frame. */
	float NextFrame[8];

	/** Linear interpolation between current and next frame. */
	float FrameAlpha;

	/** Index of the current input read position. */
	int64 FrameIndex;

	/** Index of the previously generated frame (to avoid calculating it again). */
	int64 LastFrameIndex;

	/** The play rate of the previously generated frame. */
	float LastRate;

private:

	//~ Input and output specs

	/** The input buffer. */
	TArray<float> Input;

	/** Duration of the input buffer. */
	FTimespan InputDuration;

	/** Number of frames in input buffer. */
	uint32 InputFrames;

	/** Sample rate of input buffer. */
	uint32 InputSampleRate;

	/** Start time of the input buffer. */
	FTimespan InputTime;

	/** Number of channels in the output. */
	uint32 OutputChannels;

	/** Sample rate of the output. */
	uint32 OutputSampleRate;
};
