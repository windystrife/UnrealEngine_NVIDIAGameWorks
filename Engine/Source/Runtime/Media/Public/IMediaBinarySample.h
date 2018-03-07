// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Timespan.h"


/**
 * Interface for media binary data samples.
 */
class IMediaBinarySample
{
public:

	/**
	 * Get the sample data.
	 *
	 * @return Pointer to data buffer.
	 * @see GetDuration, GetSize, GetTime
	 */
	virtual const void* GetData() = 0;

	/**
	 * Get the amount of time for which the sample is valid.
	 *
	 * A duration of zero indicates that the sample is valid until the
	 * timecode of the next sample in the queue.
	 *
	 * @return Sample duration.
	 * @see GetData, GetSize, GetTime
	 */
	virtual FTimespan GetDuration() const = 0;

	/**
	 * Get the size of the binary data.
	 *
	 * @see GetData, GetDuration, GetTime
	 */
	virtual uint32 GetSize() const = 0;

	/**
	 * Get the sample time (in the player's local clock).
	 *
	 * This value is used primarily for debugging purposes.
	 *
	 * @return Sample time.
	 * @see GetData, GetDuration, GetSize, GetTime
	 */
	virtual FTimespan GetTime() const = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaBinarySample() { }
};
