// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Math/Range.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"

class IMediaAudioSample;
class IMediaBinarySample;
class IMediaOverlaySample;
class IMediaTextureSample;


/**
 * Interface for access to a media player's sample queue.
 *
 * @see IMediaCache, IMediaControls, IMediaPlayer, IMediaTracks, IMediaView
 */
class IMediaSamples
{
public:

	//~ The following methods are optional

	/**
	 * Fetch the next audio sample.
	 *
	 * @param TimeRange The range of present times that the sample is allowed to have.
	 * @param OutSample Will contain the sample if the queue is not empty.
	 * @return true if the returned sample is valid, false otherwise.
	 * @see FetchCaption, FetchMetadata, FetchSubtitle, FetchVideo
	 */
	virtual bool FetchAudio(TRange<FTimespan> TimeRange, TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe>& OutSample)
	{
		return false; // override in child classes, if supported
	}

	/**
	 * Fetch the next caption sample.
	 *
	 * @param TimeRange The range of present times that the sample is allowed to have.
	 * @param OutSample Will contain the sample if the queue is not empty.
	 * @return true if the returned sample is valid, false otherwise.
	 * @see FetchAudio, FetchMetadata, FetchSubtitle, FetchVideo
	 */
	virtual bool FetchCaption(TRange<FTimespan> TimeRange, TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe>& OutSample)
	{
		return false; // override in child classes, if supported
	}

	/**
	 * Fetch the next metadata sample.
	 *
	 * @param TimeRange The range of present times that the sample is allowed to have.
	 * @param OutSample Will contain the sample if the queue is not empty.
	 * @return true if the returned sample is valid, false otherwise.
	 * @see FetchAudio, FetchCaption, FetchSubtitle, FetchVideo
	 */
	virtual bool FetchMetadata(TRange<FTimespan> TimeRange, TSharedPtr<IMediaBinarySample, ESPMode::ThreadSafe>& OutSample)
	{
		return false; // override in child classes, if supported
	}

	/**
	 * Fetch the next subtitle sample.
	 *
	 * @param TimeRange The range of present times that the sample is allowed to have.
	 * @param OutSample Will contain the sample if the queue is not empty.
	 * @return true if the returned sample is valid, false otherwise.
	 * @see FetchAudio, FetchCaption, FetchMetadata, FetchVideo
	 */
	virtual bool FetchSubtitle(TRange<FTimespan> TimeRange, TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe>& OutSample)
	{
		return false; // override in child classes, if supported
	}

	/**
	 * Fetch the next video sample.
	 *
	 * @param TimeRange The range of present times that the sample is allowed to have.
	 * @param OutSample Will contain the sample if the queue is not empty.
	 * @return true if the returned sample is valid, false otherwise.
	 * @see FetchAudio, FetchCaption, FetchMetadata, FetchSubtitle
	 */
	virtual bool FetchVideo(TRange<FTimespan> TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample)
	{
		return false; // override in child classes, if supported
	}

	/** Discard any outstanding media samples. */
	virtual void FlushSamples()
	{
		// override in child classes, if supported
	}

public:

	/** Virtual destructor. */
	virtual ~IMediaSamples() { }
};
