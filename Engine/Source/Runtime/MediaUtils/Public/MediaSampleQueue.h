// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Queue.h"
#include "HAL/PlatformAtomics.h"
#include "IMediaSamples.h"
#include "Misc/App.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"

#include "MediaSampleSink.h"
#include "MediaSampleSource.h"


/**
 * Template for media sample queues.
 */
template<typename SampleType>
class TMediaSampleQueue
	: public TMediaSampleSink<SampleType>
	, public TMediaSampleSource<SampleType>
{
public:

	/** Default constructor. */
	TMediaSampleQueue()
		: NumSamples(0)
		, PendingFlushes(0)
	{ }

	/** Virtual destructor. */
	virtual ~TMediaSampleQueue() { }

public:

	/**
	 * Get the number of samples in the queue.
	 *
	 * Note: The value returned by this function is only eventually consistent. It
	 * can be called by both consumer and producer threads, but it should not be used
	 * to query the actual state of the queue. Always use Dequeue and Peek instead!
	 *
	 * @return Number of samples.
	 * @see Enqueue, Dequeue, Peek
	 */
	int32 Num() const
	{
		return NumSamples;
	}

public:

	//~ TMediaSampleSource interface (to be called only from consumer thread)

	virtual bool Dequeue(TSharedPtr<SampleType, ESPMode::ThreadSafe>& OutSample) override
	{
		DoPendingFlushes();

		TSharedPtr<SampleType, ESPMode::ThreadSafe> Sample;

		if (!Samples.Peek(Sample))
		{
			return false; // empty queue
		}
			
		if (!Sample.IsValid())
		{
			return false; // pending flush
		}

		Samples.Pop();

		FPlatformAtomics::InterlockedDecrement(&NumSamples);
		check(NumSamples >= 0);

		OutSample = Sample;

		return true;
	}

	virtual bool Peek(TSharedPtr<SampleType, ESPMode::ThreadSafe>& OutSample) override
	{
		DoPendingFlushes();

		TSharedPtr<SampleType, ESPMode::ThreadSafe> Sample;

		if (!Samples.Peek(Sample))
		{
			return false; // empty queue
		}

		if (!Sample.IsValid())
		{
			return false; // pending flush
		}

		OutSample = Sample;

		return true;
	}

	virtual bool Pop() override
	{
		TSharedPtr<SampleType, ESPMode::ThreadSafe> Sample;

		if (!Samples.Peek(Sample))
		{
			return false; // empty queue
		}

		if (!Sample.IsValid())
		{
			return false; // pending flush
		}

		Samples.Pop();

		FPlatformAtomics::InterlockedDecrement(&NumSamples);
		check(NumSamples >= 0);

		return true;
	}

public:

	//~ TMediaSampleSink interface (to be called only from producer thread)

	virtual bool Enqueue(const TSharedRef<SampleType, ESPMode::ThreadSafe>& Sample) override
	{
		FPlatformAtomics::InterlockedIncrement(&NumSamples); // avoid negative sample count in Dequeue

		if (!Samples.Enqueue(Sample))
		{
			FPlatformAtomics::InterlockedDecrement(&NumSamples);

			return false;
		}

		return true;
	}

	virtual void RequestFlush() override
	{
		Samples.Enqueue(nullptr); // insert flush marker
		FPlatformAtomics::InterlockedIncrement(&PendingFlushes);
	}

protected:

	/** Perform any pending flushes. */
	void DoPendingFlushes()
	{
		TSharedPtr<SampleType, ESPMode::ThreadSafe> Sample;

		while ((PendingFlushes > 0) && Samples.Dequeue(Sample))
		{
			if (Sample.IsValid())
			{
				FPlatformAtomics::InterlockedDecrement(&NumSamples);
				check(NumSamples >= 0);
			}
			else
			{
				FPlatformAtomics::InterlockedDecrement(&PendingFlushes);
			}
		}
	}

private:

	/** Number of samples in the queue. */
	int32 NumSamples;

	/** Number of pending flushes. */
	int32 PendingFlushes;

	/** Audio sample queue. */
	TQueue<TSharedPtr<SampleType, ESPMode::ThreadSafe>, EQueueMode::Mpsc> Samples;
};


/** Type definition for audio sample queue. */
typedef TMediaSampleQueue<class IMediaAudioSample> FMediaAudioSampleQueue;

/** Type definition for binary sample queue. */
typedef TMediaSampleQueue<class IMediaBinarySample> FMediaBinarySampleQueue;

/** Type definition for overlay sample queue. */
typedef TMediaSampleQueue<class IMediaOverlaySample> FMediaOverlaySampleQueue;

/** Type definition for texture sample queue. */
typedef TMediaSampleQueue<class IMediaTextureSample> FMediaTextureSampleQueue;
