// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Templates/SharedPointer.h"

#include "MediaSampleSink.h"


/**
 * Collection of media sample sinks.
 *
 * @param SampleType The type of media samples that the sinks process.
 */
template<typename SampleType>
class TMediaSampleSinks
{
public:

	/**
	 * Add the given media sample sink to the collection.
	 *
	 * @param SampleSink The sink to add.
	 * @see Remove
	 */
	void Add(const TSharedRef<TMediaSampleSink<SampleType>, ESPMode::ThreadSafe>& SampleSink)
	{
		Sinks.AddUnique(SampleSink);
	}

	/**
	 * Enqueue the given media samples to the registered sinks.
	 *
	 * This method will also remove expired sinks that haven't been removed yet.
	 *
	 * @param Sample The media sample to enqueue.
	 * @param MaxQueueDepth The maximum depth of the sink queues before overflow.
	 * @return true if the sample was enqueued to all sinks, false if one or more sinks overflowed.
	 * @see Flush
	 */
	bool Enqueue(const TSharedRef<SampleType, ESPMode::ThreadSafe>& Sample, int32 MaxDepth)
	{
		bool Overflowed = false;

		for (int32 SinkIndex = Sinks.Num() - 1; SinkIndex >= 0; --SinkIndex)
		{
			TSharedPtr<TMediaSampleSink<SampleType>, ESPMode::ThreadSafe> Sink = Sinks[SinkIndex].Pin();

			if (Sink.IsValid())
			{
				if (Sink->Num() >= MaxDepth)
				{
					Overflowed = true;
				}
				else
				{
					Sink->Enqueue(Sample);
				}
			}
			else
			{
				Sinks.RemoveAtSwap(SinkIndex);
			}
		}

		return !Overflowed;
	}

	/**
	 * Flush all registered sinks.
	 *
	 * This method will also remove expired sinks that haven't been removed yet.
	 *
	 * @see Enqueue
	 */
	void Flush()
	{
		for (int32 SinkIndex = Sinks.Num() - 1; SinkIndex >= 0; --SinkIndex)
		{
			TSharedPtr<TMediaSampleSink<SampleType>, ESPMode::ThreadSafe> Sink = Sinks[SinkIndex].Pin();

			if (Sink.IsValid())
			{
				Sink->RequestFlush();
			}
			else
			{
				Sinks.RemoveAtSwap(SinkIndex);
			}
		}
	}

	/**
	 * Remove the given media sample sink from the collection.
	 *
	 * @param SampleSink The sink to remove.
	 * @see Add
	 */
	void Remove(const TSharedRef<TMediaSampleSink<SampleType>, ESPMode::ThreadSafe>& SampleSink)
	{
		Sinks.Remove(SampleSink);
	}

private:

	/** The collection of registered sinks. */
	TArray<TWeakPtr<TMediaSampleSink<SampleType>, ESPMode::ThreadSafe>> Sinks;
};
