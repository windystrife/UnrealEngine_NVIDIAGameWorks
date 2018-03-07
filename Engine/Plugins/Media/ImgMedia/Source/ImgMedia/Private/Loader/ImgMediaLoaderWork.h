// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/IQueuedWork.h"

class FImgMediaLoader;
class IImgMediaLoader;
class IImgMediaReader;


/**
 * Loads a single image frame from disk.
 */
class FImgMediaLoaderWork
	: public IQueuedWork
{
public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InOwner The loader that created this work item.
	 * @param InReader The image reader to use.
	 */
	FImgMediaLoaderWork(IImgMediaLoader& InOwner, const TSharedRef<IImgMediaReader, ESPMode::ThreadSafe>& InReader);

public:

	/**
	 * Delete this work item when it is done.
	 *
	 * This method will mark the work item for shutdown. When the work is
	 * completed, the work item will automatically delete itself. No other
	 * methods may be called on this object after calling this one.
	 *
	 * @see Initialize
	 */
	void DeleteWhenDone();

	/**
	 * Initialize this work item.
	 *
	 * @param InFrameNumber The number of the image frame.
	 * @param InImagePath The file path to the image frame to read.
	 * @see Shutdown
	 */
	void Initialize(int32 InFrameNumber, const FString& InImagePath);

public:

	//~ IQueuedWork interface

	virtual void Abandon() override;
	virtual void DoThreadedWork() override;

private:

	/** Whether the work item should delete itself after it is complete. */
	bool AutoDelete;

	/** Whether the work was done. */
	bool Done;

	/** Critical section for synchronizing access to Owner. */
	FCriticalSection CriticalSection;

	/** The number of the image frame. */
	int32 FrameNumber;

	/** The file path to the image frame to read. */
	FString ImagePath;

	/** The loader that created this reader task. */
	IImgMediaLoader& Owner;

	/** The image sequence reader to use. */
	TSharedPtr<IImgMediaReader, ESPMode::ThreadSafe> Reader;
};
