// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AudioThread.h: Rendering thread definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Async/TaskGraphInterfaces.h"
#include "HAL/Runnable.h"

////////////////////////////////////
// Audio thread API
////////////////////////////////////

DECLARE_STATS_GROUP(TEXT("Audio Thread Commands"), STATGROUP_AudioThreadCommands, STATCAT_Advanced);

class FAudioThread : public FRunnable
{
private:
	/**
	* Whether to run with an audio thread
	*/
	static bool bUseThreadedAudio;

	/** 
	* Sync event to make sure that audio thread is bound to the task graph before main thread queues work against it.
	*/
	FEvent* TaskGraphBoundSyncEvent;

	/**
	* Whether the audio thread is currently running
	* If this is false, then we have no audio thread and audio commands will be issued directly on the game thread
	*/
	static bool bIsAudioThreadRunning;

	static bool bIsAudioThreadSuspended;

	/** The audio thread itself. */
	static FRunnable* AudioThreadRunnable;

	/** The stashed value of the audio thread as we clear it during GC */
	static uint32 CachedAudioThreadId;

	void OnPreGarbageCollect();
	void OnPostGarbageCollect();

public:

	FAudioThread();
	virtual ~FAudioThread();

	// FRunnable interface.
	virtual bool Init() override;
	virtual void Exit() override;
	virtual uint32 Run() override;

	/** Starts the audio thread. */
	static ENGINE_API void StartAudioThread();

	/** Stops the audio thread. */
	static ENGINE_API void StopAudioThread();

	/** Execute a command on the audio thread. If GIsAudioThreadRunning is false the command will execute immediately */
	static ENGINE_API void RunCommandOnAudioThread(TFunction<void()> InFunction, const TStatId InStatId = TStatId());

	/** Execute a (presumably audio) command on the game thread. If GIsAudioThreadRunning is false the command will execute immediately */
	static ENGINE_API void RunCommandOnGameThread(TFunction<void()> InFunction, const TStatId InStatId = TStatId());

	static ENGINE_API void SetUseThreadedAudio(bool bInUseThreadedAudio);
	static ENGINE_API bool IsUsingThreadedAudio() { return bUseThreadedAudio; }

	static ENGINE_API bool IsAudioThreadRunning() { return bIsAudioThreadRunning; }

	static ENGINE_API void SuspendAudioThread();
	static ENGINE_API void ResumeAudioThread();

};

/** Suspends the audio thread for the duration of the lifetime of the object */
struct FAudioThreadSuspendContext
{
	FAudioThreadSuspendContext()
	{
		FAudioThread::SuspendAudioThread();
	}

	~FAudioThreadSuspendContext()
	{
		FAudioThread::ResumeAudioThread();
	}
};

////////////////////////////////////
// Audio fences
////////////////////////////////////

/**
* Used to track pending audio commands from the game thread.
*/
class ENGINE_API FAudioCommandFence
{
public:

	/**
	* Adds a fence command to the audio command queue.
	* Conceptually, the pending fence count is incremented to reflect the pending fence command.
	* Once the rendering thread has executed the fence command, it decrements the pending fence count.
	*/
	void BeginFence();

	/**
	* Waits for pending fence commands to retire.
	* @param bProcessGameThreadTasks, if true we are on a short callstack where it is safe to process arbitrary game thread tasks while we wait
	*/
	void Wait(bool bProcessGameThreadTasks = false) const;

	// return true if the fence is complete
	bool IsFenceComplete() const;

private:
	/** Graph event that represents completion of this fence **/
	mutable FGraphEventRef CompletionEvent;
};

