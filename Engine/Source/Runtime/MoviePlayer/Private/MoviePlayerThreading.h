// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "SpinLock.h"

class FRunnable;
class FMoviePlayerWidgetRenderer;

/**
 * This class will handle all the nasty bits about running Slate on a separate thread
 * and then trying to sync it up with the game thread and the render thread simultaneously
 */
class FSlateLoadingSynchronizationMechanism
{
public:
	FSlateLoadingSynchronizationMechanism(TSharedPtr<FMoviePlayerWidgetRenderer, ESPMode::ThreadSafe> InWidgetRenderer);
	~FSlateLoadingSynchronizationMechanism();
	
	/** Sets up the locks in their proper initial state for running */
	void Initialize();

	/** Cleans up the slate thread */
	void DestroySlateThread();

	/** Handles the strict alternation of the slate drawing passes */
	bool IsSlateDrawPassEnqueued();
	void SetSlateDrawPassEnqueued();
	void ResetSlateDrawPassEnqueued();

	/** Handles the counter to determine if the slate thread should keep running */
	bool IsSlateMainLoopRunning();
	void SetSlateMainLoopRunning();
	void ResetSlateMainLoopRunning();

	/** The main loop to be run from the Slate thread */
	void SlateThreadRunMainLoop();

public:
	/**
	 * This spin lock blocks the game thread until the Slate thread main loop has finished spinning
	 */
	FSpinLock MainLoop;

private:
	/**
	 * This counter handles running the main loop of the slate thread
	 */
	FThreadSafeCounter IsRunningSlateMainLoop;
	/**
	 * This counter handles strict alternation between the slate thread and the render thread
	 * for passing Slate render draw passes between each other.
	 */
	FThreadSafeCounter IsSlateDrawEnqueued;

	/**
	* This counter is used to generate a unique id for each new instance of the loading thread
	*/
	static FThreadSafeCounter LoadingThreadInstanceCounter;

	/** The worker thread that will become the Slate thread */
	FRunnableThread* SlateLoadingThread;
	FRunnable* SlateRunnableTask;

	TSharedPtr<FMoviePlayerWidgetRenderer, ESPMode::ThreadSafe> WidgetRenderer;
};
