// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DUpdate.h: Helpers to stream in and out mips.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "Async/AsyncWork.h"
#include "Engine/Texture2D.h"
#include "Async/AsyncFileHandle.h"

#define TEXTURE2D_UPDATE_CALLBACK(FunctionName) [this](const FContext& C){ FunctionName(C); }
extern TAutoConsoleVariable<int32> CVarFlushRHIThreadOnSTreamingTextureLocks;

/**
 * This class provides a framework for loading and unloading the mips of 2D textures.
 * Each thread essentially calls Tick() until the job is done.
 * The object can be safely deleted when IsCompleted() returns true.
 */
class FTexture2DUpdate
{
public:
	/** A thread type used for doing a part of the update process.  */
	enum EThreadType
	{
		TT_None,	// No thread.
		TT_Render,	// The render thread.
		TT_Async	// An async work thread.
	};

	/**  The state of scheduled work for the update process. */
	enum ETaskState
	{
		TS_None,		// Nothing to do.
		TS_Pending,		// The next task (or update step) is configured, but a callback has not been scheduled yet.
		TS_Scheduled,	// The next task (or update step) is configured and a callback has been scheduled on either the renderthread or async thread.
		TS_Locked,		// The object is locked, and no one is allowed to process or look at the next task.
	};

	FTexture2DUpdate(UTexture2D* InTexture, int32 InRequestedMips);
	virtual ~FTexture2DUpdate();

	/**
	 * Do or schedule any pending work for a given texture.
	 *
	 * @param InTexture - the texture being updated, this must be the same texture as the texture used to create this object.
	 * @param InCurrentThread - the thread from which the tick is being called. Using TT_None ensures that no work will be immediately performed.
	 */
	void Tick(UTexture2D* InTexture, EThreadType InCurrentThread);

	/** Returns whether the task has finished executing and there is no other thread possibly accessing it. */
	bool IsCompleted() const { return ScheduledTaskCount <= 0 && TaskState == TS_None; }

	/** Cancel the current update. Will also attempt to cancel pending IO requests, see FTexture2DStreamIn_IO::Abort(). */
	virtual void Abort() { MarkAsCancelled(); }

#if WITH_EDITORONLY_DATA
	/** Returns whether DDC of this texture needs to be regenerated.  */
	virtual bool DDCIsInvalid() const { return false; }
#endif

	/** Returns whether the task was aborted through Abort() or cancelled.  */
	bool IsCancelled() const  { return bIsCancelled; }

	/** 
	 * Perform a lock on the object, preventing any other thread from processing a pending task in Tick().  
	 * The lock will stall if there was another thread already processing the task data.
	 */
	void DoLock();
	
	/** Release any lock on the object, allowing other thread to modify it. */
	void DoUnlock();

	/** Get the number of requested mips for this update, ignoring cancellation attempts. */
	int32 GetNumRequestedMips() const { return RequestedMips; }

protected:

	/** Set the task state as cancelled. This is internally called in Abort() and when any critical conditions are not met when performing the update. */
	void MarkAsCancelled() { bIsCancelled = true; }

	/**
	 * A context used to update or proceed with the next update step.
	 * The texture and resource references could be stored in the update object
	 * but are currently kept outside to avoid lifetime management within the object.
	 */
	struct FContext
	{
		FContext(UTexture2D* InTexture, EThreadType InCurrentThread);

		/** The texture to update, this must be the same one as the one used when creating the FTexture2DUpdate object. */
		UTexture2D* Texture;
		/** The current 2D resource of this texture. */
		FTexture2DResource* Resource;
		/** The thread on which the context was created. */
		EThreadType CurrentThread;
	};

	/** A callback used to perform a task in the update process. Each task must be executed on a specific thread. */
	typedef TFunction<void(const FContext& Context)> FCallback;

	/**
	 * Defines the next step to be executed. The next step will be executed by calling the callback on the specified thread.
	 * The callback (for both success and cancelation) will only be executed if TaskSynchronization reaches 0.
	 * If all requirements are immediately satisfied when calling the PushTask the relevant callback will be called immediately.
	 *
	 * @param Context - The context defining which texture is being updated and on which thread this is being called.
	 * @param InTaskThread - The thread on which to call the next step of the update, being TaskCallback.
	 * @param InTaskCallback - The callback that will perform the next step of the update.
	 * @param InCancelationThread - The thread on which to call the cancellation of the update (only if the update gets cancelled).
	 * @param InCancelationCallback - The callback handling the cancellation of the update (only if the update gets cancelled).
	 */
	void PushTask(const FContext& Context, EThreadType InTaskThread, const FCallback& InTaskCallback, EThreadType InCancelationThread, const FCallback& InCancelationCallback);

	/** The intermediate texture created in the update process. In the virtual path, this can exceptionally end update being the same as the original texture. */
	FTexture2DRHIRef IntermediateTextureRHI;
	/** The mip index within UTexture2D::GetPlatformMips() that will end as being the first mip of the intermediate (future) texture. */
	int32 PendingFirstMip;
	/** The total number of mips of the intermediate (future) texture. */
	int32 RequestedMips;

	/** Synchronization used for trigger the task next step execution. */
	FThreadSafeCounter	TaskSynchronization;

	/** The number of scheduled ticks (and exceptionally other work from FCancelIORequestsTask) from the renderthread and async thread. Used to prevent deleting the object while it could be accessed. */
	volatile int32 ScheduledTaskCount;

	// ****************************
	// ********* Helpers **********
	// ****************************

	/** Async Reallocate the texture to the requested size. */
	void DoAsyncReallocate(const FContext& Context);
	/** Transform the texture into a virtual texture. The virtual texture. */
	void DoConvertToVirtualWithNewMips(const FContext& Context);
	/** Transform the texture into a non virtual texture. The new texture will have the size of the requested mips to avoid later reallocs. */
	bool DoConvertToNonVirtual(const FContext& Context);
	/** Apply the new texture (if not cancelled) and finish the update process. When cancelled, the intermediate texture simply gets discarded. */
	void DoFinishUpdate(const FContext& Context);

private:

	/**
	 * Locks the object only if there is work to do. If the calling thread is has no capability to actually perform any work,
	 * the lock attempt will also fails if the object is already locked. This is used essentially to prevent the gamethread
	 * from being blocked when ticking the update while the update is already being ticked on another thread.
	 *
	 * @param InCurrentThread - the thread trying to lock the object. Passing TS_None may end up preventing to acquired the object.
	 * @return Whether the lock succeeded.
	 */
	bool DoConditionalLock(EThreadType InCurrentThread);

	/**
	 * Pushes a callback on either the gamethread or an async work thread.
	 *
	 * @param Context - the context from the caller thread.
	 * @param InThread - the thread on which to schedule a tick.
	 */
	void ScheduleTick(const FContext& Context, EThreadType InThread);

	/** Clears any pending work. */
	void ClearTask();

	/** An async task used to call tick on the pending update. */
	class FMipUpdateTask : public FNonAbandonableTask
	{
	public:
		FMipUpdateTask(UTexture2D* InTexture, FTexture2DUpdate* InPendingUpdate) : Texture(InTexture), CachedPendingUpdate(InPendingUpdate) {}
		void DoWork();
		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FMipUpdateTask, STATGROUP_ThreadPoolAsyncTasks);
		}
	protected:
		UTexture2D* Texture;
		FTexture2DUpdate* CachedPendingUpdate;
	};

	/** The async task to update this object, only one can be active at anytime. It just calls Tick(). */
	typedef FAsyncTask<FMipUpdateTask> FAsyncMipUpdateTask;
	TUniquePtr<FAsyncMipUpdateTask> AsyncMipUpdateTask;

	/** Whether the task has been cancelled because the update could not proceed or because the user called Abort(). */
	bool bIsCancelled;

	/** The state of the work yet to be performed to complete the update or cancelation. */
	volatile int32 TaskState;
	/** The pending state of future work. Used because the object is in locked state (TS_Locked) when being updated, until the call to Unlock() */
	ETaskState PendingTaskState;

	/** The thread on which to call the next step of the update, being TaskCallback. */
	EThreadType TaskThread;
	/** The callback that will perform the next step of the update. */
	FCallback TaskCallback;
	/** The thread on which to call the cancellation of the update (only if the update gets cancelled). */
	EThreadType CancelationThread; // The thread on which the callbacks should be called.
	/** The callback handling the cancellation of the update (only if the update gets cancelled). */
	FCallback CancelationCallback;

public:
	/** A counter defining whether render thread tasks should be postponed The callback handling the cancellation of the update (only if the update gets cancelled). */
	static volatile int32 GSuspendRenderThreadTasks;
};

void SuspendTextureStreamingRenderTasksInternal();
void ResumeTextureStreamingRenderTasksInternal();
