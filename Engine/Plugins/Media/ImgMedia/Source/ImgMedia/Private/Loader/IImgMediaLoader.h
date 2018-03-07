// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"

class FImgMediaLoaderWork;
struct FImgMediaFrame;


/**
 * Interface for the image sequence loader.
 */
class IImgMediaLoader
{
public:

	/**
	 * Notify the loader that a work item completed.
	 *
	 * @param CompletedWork The work item that completed.
	 * @param FrameNumber Number of the frame that was read.
	 * @param Frame The frame that was read, or nullptr if reading failed.
	 */
	virtual void NotifyWorkComplete(FImgMediaLoaderWork& CompletedWork, int32 FrameNumber, const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>& Frame) = 0;
};
