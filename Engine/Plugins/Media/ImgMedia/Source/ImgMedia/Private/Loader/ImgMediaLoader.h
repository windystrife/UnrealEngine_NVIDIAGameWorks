// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/LruCache.h"
#include "Containers/Queue.h"

#include "IImgMediaLoader.h"

class FImgMediaLoaderWork;
class FImgMediaTextureSample;
class IImageWrapperModule;
class IImgMediaReader;

struct FImgMediaFrame;


/**
 * Loads image sequence frames from disk.
 */
class FImgMediaLoader
	: protected IImgMediaLoader
{
public:

	/** Default constructor. */
	FImgMediaLoader();

	/** Virtual destructor. */
	virtual ~FImgMediaLoader();

public:

	/**
	 * Get the data bit rate of the video frames.
	 *
	 * @return Data rate (in bits per second).
	 */
	uint64 GetBitRate() const;

	/**
	 * Get the time ranges of frames that are being loaded right now.
	 *
	 * @param OutRangeSet Will contain the set of time ranges.
	 * @return true on success, false otherwise.
	 * @see GetCompletedTimeRanges, GetPendingTimeRanges
	 */
	void GetBusyTimeRanges(TRangeSet<FTimespan>& OutRangeSet) const;

	/**
	 * Get the time ranges of frames that are are already loaded.
	 *
	 * @param OutRangeSet Will contain the set of time ranges.
	 * @return true on success, false otherwise.
	 * @see GetBusyTimeRanges, GetPendingTimeRanges
	 */
	void GetCompletedTimeRanges(TRangeSet<FTimespan>& OutRangeSet) const;

	/**
	 * Get the image frame at the specified time.
	 *
	 * @param Time The time of the image frame to get (relative to the beginning of the sequence).
	 * @return The frame, or nullptr if the frame wasn't available yet.
	 */
	TSharedPtr<FImgMediaTextureSample, ESPMode::ThreadSafe> GetFrameSample(FTimespan Time);

	/**
	 * Get the information string for the currently loaded image sequence.
	 *
	 * @return Info string.
	 * @see GetSequenceDuration, GetSamples
	 */
	const FString& GetInfo() const
	{
		return Info;
	}

	/**
	 * Get the time ranges of frames that are pending to be loaded.
	 *
	 * @param OutRangeSet Will contain the set of time ranges.
	 * @return true on success, false otherwise.
	 * @see GetBusyTimeRanges, GetCompletedTimeRanges
	 */
	void GetPendingTimeRanges(TRangeSet<FTimespan>& OutRangeSet) const;

	/**
	 * Get the image reader object used by this loader.
	 *
	 * @return The image reader.
	 */
	TSharedRef<IImgMediaReader, ESPMode::ThreadSafe> GetReader() const
	{
		return Reader.ToSharedRef();
	}

	/**
	 * Get the width and height of the image sequence.
	 *
	 * The dimensions of the image sequence are determined by reading the attributes
	 * of the first image. The dimensions of individual image frames in the sequence
	 * are allowed to differ. However, this usually indicates a mistake in the content
	 * creation pipeline and will be logged out as such.
	 *
	 * @return Sequence dimensions.
	 * @see GetSequenceDuration, GetSequenceFps
	 */
	const FIntPoint& GetSequenceDim() const
	{
		return SequenceDim;
	}

	/**
	 * Get the total duration of the image sequence.
	 *
	 * @return Duration.
	 * @see GetSequenceDim, GetSequenceFps
	 */
	FTimespan GetSequenceDuration() const
	{
		return SequenceDuration;
	}

	/**
	 * Get the sequence's frames per second.
	 *
	 * The frame rate of the image sequence is determined by reading the attributes
	 * of the first image. Individual image frames may specify a different frame rate,
	 * but it will be ignored during playback.
	 *
	 * @return Frames per second.
	 * @see GetSequenceDuration, GetSequenceDim
	 */
	float GetSequenceFps() const
	{
		return SequenceFps;
	}

	/**
	 * Initialize the image sequence loader.
	 *
	 * @param SequencePath Path to the EXR image sequence.
	 * @param FpsOverride The frames per second to use (0.0 = do not override).
	 * @see IsInitialized
	 */
	void Initialize(const FString& SequencePath, const float FpsOverride);

	/**
	 * Whether this loader has been initialized yet.
	 *
	 * @return true if initialized, false otherwise.
	 * @see Initialize
	 */
	bool IsInitialized() const
	{
		return Initialized;
	}

	/**
	 * Asynchronously request the image frame at the specified time.
	 *
	 * @param Time The time of the image frame to request (relative to the beginning of the sequence).
	 * @param PlayRate The current play rate (used by the look-ahead logic).
	 * @return bool if the frame will be loaded, false otherwise.
	 */
	bool RequestFrame(FTimespan Time, float PlayRate);

protected:

	/**
	 * Convert a collection of frame numbers to corresponding time ranges.
	 *
	 * @param FrameNumbers The frame numbers to convert.
	 * @param OutRangeSet Will contain the time ranges.
	 */
	void FrameNumbersToTimeRanges(const TArray<int32>& FrameNumbers, TRangeSet<FTimespan>& OutRangeSet) const;

	/**
	 * Get the frame number corresponding to the specified play head time.
	 *
	 * @param Time The play head time.
	 * @return The corresponding frame number, or INDEX_NONE.
	 */
	uint32 TimeToFrame(FTimespan Time) const;

	/**
	 * Update the loader based on the current play position.
	 *
	 * @param PlayHeadFrame Current play head frame number.
	 * @param PlayRate Current play rate.
	 */
	void Update(int32 PlayHeadFrame, float PlayRate);

protected:

	//~ IImgMediaLoader interface

	virtual void NotifyWorkComplete(FImgMediaLoaderWork& CompletedWork, int32 FrameNumber, const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>& Frame) override;

private:

	/** Critical section for synchronizing access to Frames. */
	mutable FCriticalSection CriticalSection;

	/** The currently loaded image sequence frames. */
	TLruCache<int32, TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>> Frames;

	/** The image wrapper module to use. */
	IImageWrapperModule& ImageWrapperModule;

	/** Paths to each EXR image in the currently opened sequence. */
	TArray<FString> ImagePaths;

	/** Media information string. */
	FString Info;

	/** Whether this loader has been initialized yet. */
	bool Initialized;

	/** The number of frames to load ahead of the play head. */
	int32 NumLoadAhead;

	/** The number of frames to load behind the play head. */
	int32 NumLoadBehind;

	/** The image sequence reader to use. */
	TSharedPtr<IImgMediaReader, ESPMode::ThreadSafe> Reader;

	/** Width and height of the image sequence (in pixels) .*/
	FIntPoint SequenceDim;

	/** Total length of the image sequence. */
	FTimespan SequenceDuration;

	/** Number of frames per second. */
	float SequenceFps;

private:

	TArray<FImgMediaLoaderWork*> AbandonedWorks;

	/** Index of the previously requested frame. */
	int32 LastRequestedFrame;

	/** Collection of frame numbers that need to be read. */
	TArray<int32> PendingFrameNumbers;

	/** Collection of frame numbers that are being read. */
	TArray<int32> QueuedFrameNumbers;

	/** Maps queued frame numbers to work items. */
	TMap<int32, FImgMediaLoaderWork*> QueuedWorks;

	/** Object pool for reusable work items. */
	TArray<FImgMediaLoaderWork*> WorkPool;
};
