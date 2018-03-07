// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"


/**
 * Interface for media sample sinks.
 *
 * This interface declares the write side of media sample queues.
 *
 * @see TMediaSampleQueue
 */
template<typename SampleType>
class TMediaSampleSink
{
public:

	/**
	 * Add a sample to the head of the queue.
	 *
	 * @param Sample The sample to add.
	 * @return true if the item was added, false otherwise.
	 * @see Num, RequestFlush
	 */
	virtual bool Enqueue(const TSharedRef<SampleType, ESPMode::ThreadSafe>& Sample) = 0;

	/**
	 * Get the number of samples in the queue.
	 *
	 * @return Number of samples.
	 * @see Enqueue, RequestFlush
	 */
	virtual int32 Num() const = 0;

	/**
	 * Request to flush the queue.
	 *
	 * @note To be called only from producer thread.
	 * @see Enqueue, Num
	 */
	virtual void RequestFlush() = 0;

public:

	/** Virtual destructor. */
	virtual ~TMediaSampleSink() { }
};


/** Type definition for audio sample sink. */
typedef TMediaSampleSink<class IMediaAudioSample> FMediaAudioSampleSink;

/** Type definition for binary sample sink. */
typedef TMediaSampleSink<class IMediaBinarySample> FMediaBinarySampleSink;

/** Type definition for overlay sample sink. */
typedef TMediaSampleSink<class IMediaOverlaySample> FMediaOverlaySampleSink;

/** Type definition for texture sample sink. */
typedef TMediaSampleSink<class IMediaTextureSample> FMediaTextureSampleSink;
