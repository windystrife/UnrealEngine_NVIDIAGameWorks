// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DUpdate.cpp: Helpers to stream in and out mips.
=============================================================================*/

#include "Streaming/Texture2DUpdate.h"
#include "RenderUtils.h"
#include "Containers/ResourceArray.h"

volatile int32 FTexture2DUpdate::GSuspendRenderThreadTasks = 0;

FTexture2DUpdate::FContext::FContext(UTexture2D* InTexture, EThreadType InCurrentThread) 
	: Texture(InTexture)
	, CurrentThread(InCurrentThread)
{
	check(InTexture);
	checkSlow(InCurrentThread != TT_Render || IsInRenderingThread());
	Resource = static_cast<FTexture2DResource*>(Texture->Resource);
}

FTexture2DUpdate::FTexture2DUpdate(UTexture2D* InTexture, int32 InRequestedMips) 
	: PendingFirstMip(INDEX_NONE)
	, RequestedMips(INDEX_NONE)
	, ScheduledTaskCount(0)
	, bIsCancelled(false)
	, TaskState(TS_Locked) // The object is created in the locked state to follow the Tick path
	, PendingTaskState(TS_None)
	, TaskThread(TT_None)
	, CancelationThread(TT_None)
{
	check(InTexture);

	const int32 NonStreamingMipCount = InTexture->GetNumNonStreamingMips();
	const int32 MaxMipCount = InTexture->GetNumMips();
	InRequestedMips = FMath::Clamp<int32>(InRequestedMips, NonStreamingMipCount, MaxMipCount);

	if (InRequestedMips != InTexture->GetNumResidentMips() && InTexture->bIsStreamable && InTexture->Resource)
	{
		RequestedMips = InRequestedMips;
		PendingFirstMip	= InTexture->GetPlatformMips().Num() - RequestedMips;
	}
	else // This shouldn't happen but if it does, then the update is canceled
	{
		bIsCancelled = true;
	}
}

FTexture2DUpdate::~FTexture2DUpdate()
{
	// Work must be done here because derived destructors have been called now and so derived members are invalid.

	ensure(ScheduledTaskCount <= 0);
	ensure(!IntermediateTextureRHI);

	if (AsyncMipUpdateTask)
	{
		ensure(AsyncMipUpdateTask->IsWorkDone());
		AsyncMipUpdateTask->EnsureCompletion();
	}
}

void FTexture2DUpdate::Tick(UTexture2D* InTexture, EThreadType InCurrentThread)
{
	if (TaskState == TS_None || (TaskSynchronization.GetValue() > 0 && InCurrentThread == TT_None))
	{
		// Early exit if the task is not ready to execute and we are ticking from non executing thread.
		// Executing thread must not early exit in order to make sure that tasks are correctly scheduled.
		// Otherwise, this assumes that the game thread regularly ticks.
		return;
	}

	// Acquire the lock if there is work to do and if it is allowed to wait for the lock
	if (DoConditionalLock(InCurrentThread))
	{
		// Once locked, there shouldn't be anything pending now.
		ensure(PendingTaskState == TS_Scheduled || PendingTaskState == TS_Pending);

		// If the task is ready to execute. 
		// If this is the renderthread and we shouldn't run renderthread task, mark as pending.
		// This will require a tick from the game thread to reschedule the task.
		if (TaskSynchronization.GetValue() <= 0 && !(GSuspendRenderThreadTasks > 0 && InCurrentThread == TT_Render))
		{
			FContext Context(InTexture, InCurrentThread);

			// The task params can not change at this point but the bIsCancelled state could change.
			// To ensure coherency, the cancel state is cached as it affects which thread is relevant.
			const bool bCachedIsCancelled = bIsCancelled;
			const EThreadType RelevantThread = !bCachedIsCancelled ? TaskThread : CancelationThread;

			if (RelevantThread == TT_None)
			{
				ClearTask();
			}
			else if (InCurrentThread == RelevantThread)
			{
				FCallback CachedCallback = !bCachedIsCancelled ? TaskCallback : CancelationCallback;
				ClearTask();
				CachedCallback(Context); // Valid if the thread is valid.
			}
			else if (PendingTaskState != TS_Scheduled || InCurrentThread != TT_None)
			{
				// If the task was never scheduled (because synchro was not ready) schedule now.
				// We also reschedule if this is an executing thread and it end up not being the good thread.
				// This can happen when a task gets cancelled between scheduling and execution.
				// We enforce that executing threads either execute or reschedule to prevent a possible stalls
				// since game thread will not reschedule after the first time.
				// It's completely safe to schedule several time as task execution can only ever happen once.
				// we also keep track of how many callbacks are scheduled through ScheduledTaskCount to prevent
				// deleting the object while another thread is scheduled to access it.
				ScheduleTick(Context, RelevantThread);
			}
			else // Otherwise unlock the task for the executing thread to process it.
			{
				PendingTaskState = TS_Scheduled;
			}
		}
		else // If synchro is not ready, mark the task as pending to be scheduled.
		{
			PendingTaskState = TS_Pending;
		}

		DoUnlock();
	}
}

void FTexture2DUpdate::PushTask(const FContext& Context, EThreadType InTaskThread, const FCallback& InTaskCallback, EThreadType InCancelationThread, const FCallback& InCancelationCallback)
{
	// PushTask can only be called by the one thread/callback that is doing the processing. 
	// This means we don't need to check whether other threads could be trying to push tasks.
	check(TaskState == TS_Locked);
	checkSlow((bool)InTaskCallback == (InTaskThread != TT_None));
	checkSlow((bool)InCancelationCallback == (InCancelationThread != TT_None));

	// To ensure coherency, the cancel state is cached as it affects which thread is relevant.
	const bool bCachedIsCancelled = bIsCancelled;
	const EThreadType RelevantThread = !bCachedIsCancelled ? InTaskThread : InCancelationThread;

	// TaskSynchronization is expected to be set before call this.
	const bool bCanExecuteNow = TaskSynchronization.GetValue() <= 0 && !(GSuspendRenderThreadTasks > 0 && RelevantThread == TT_Render);

	if (RelevantThread == TT_None)
	{
		// Nothing to do.
	}
	else if (bCanExecuteNow && Context.CurrentThread == RelevantThread)
	{
		FCallback CachedCallback = !bCachedIsCancelled ? InTaskCallback : InCancelationCallback;
		// Not null otherwise relevant thread would be TT_None.
		CachedCallback(Context);
	}
	else
	{
		TaskThread = InTaskThread;
		TaskCallback = InTaskCallback;
		CancelationThread = InCancelationThread;
		CancelationCallback = InCancelationCallback;

		if (bCanExecuteNow)
		{
			ScheduleTick(Context, RelevantThread);
		}
		else
		{
			PendingTaskState = TS_Pending;
		}
	}
}	

void FTexture2DUpdate::ScheduleTick(const FContext& Context, EThreadType InThread)
{
	// Task can only be scheduled once the synchronization is completed.
	check(TaskSynchronization.GetValue() <= 0);

	// The pending update needs to be cached because the scheduling can happen in the constructor, before the assignment.

	if (InThread == TT_Render)
	{
		FPlatformAtomics::InterlockedIncrement(&ScheduledTaskCount);
		PendingTaskState = TS_Scheduled;

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		Texture2DUpdateCommand,
		UTexture2D*, Texture, Context.Texture,
		FTexture2DUpdate*, CachedPendingUpdate, this,
		{
			check(Texture && CachedPendingUpdate);

			// Recompute the context has things might have changed!
			CachedPendingUpdate->Tick(Texture, TT_Render);

			FPlatformMisc::MemoryBarrier();
			FPlatformAtomics::InterlockedDecrement(&CachedPendingUpdate->ScheduledTaskCount);
		});
	}
	else // InThread == TT_Async
	{
		check(InThread == TT_Async);

		FPlatformAtomics::InterlockedIncrement(&ScheduledTaskCount);
		PendingTaskState = TS_Scheduled;

		// Shouldn't be pushing tasks if another one is pending.
		if (AsyncMipUpdateTask)
		{
			AsyncMipUpdateTask->EnsureCompletion();
		}
		AsyncMipUpdateTask = MakeUnique<FAsyncMipUpdateTask>(Context.Texture, this);
		AsyncMipUpdateTask->StartBackgroundTask();
	}
}

void FTexture2DUpdate::ClearTask()
{
	// Reset everything so that the callback setup a new callback
	PendingTaskState = TS_None;
	TaskThread = TT_None;
	TaskCallback = nullptr;
	CancelationThread = TT_None;
	CancelationCallback = nullptr;
	TaskSynchronization.Set(0);
}

void FTexture2DUpdate::FMipUpdateTask::DoWork()
{
	check(Texture && CachedPendingUpdate);

	// Recompute the context has things might have changed!
	CachedPendingUpdate->Tick(Texture, FTexture2DUpdate::TT_Async);

	FPlatformMisc::MemoryBarrier();
	FPlatformAtomics::InterlockedDecrement(&CachedPendingUpdate->ScheduledTaskCount);
}


// ****************************
// ********* Helpers **********
// ****************************

void FTexture2DUpdate::DoAsyncReallocate(const FContext& Context)
{
	check(Context.CurrentThread == TT_Render);

	if (!IsCancelled() && Context.Texture && Context.Resource)
	{
		const FTexture2DMipMap& RequestedMipMap = Context.Texture->GetPlatformMips()[PendingFirstMip];

		TaskSynchronization.Set(1);

		ensure(!IntermediateTextureRHI);

		IntermediateTextureRHI = RHIAsyncReallocateTexture2D(
			Context.Resource->GetTexture2DRHI(),
			RequestedMips,
			RequestedMipMap.SizeX,
			RequestedMipMap.SizeY,
			&TaskSynchronization);
	}
}


//  Transform the texture into a virtual texture.
void FTexture2DUpdate::DoConvertToVirtualWithNewMips(const FContext& Context)
{
	check(Context.CurrentThread == TT_Render);

	if (!IsCancelled() && Context.Texture && Context.Resource)
	{
		// If the texture is not virtual, then make it virtual immediately.
		const FTexture2DRHIRef Texture2DRHI = Context.Resource->Texture2DRHI;
		if ((Texture2DRHI->GetFlags() & TexCreate_Virtual) != TexCreate_Virtual)
		{
			const TIndirectArray<FTexture2DMipMap>& OwnerMips = Context.Texture->GetPlatformMips();
			const uint32 TexCreateFlags = Texture2DRHI->GetFlags() | TexCreate_Virtual;

			ensure(!IntermediateTextureRHI);

			// Create a copy of the texture that is a virtual texture.
			FRHIResourceCreateInfo CreateInfo(Context.Resource->ResourceMem);
			IntermediateTextureRHI = RHICreateTexture2D(OwnerMips[0].SizeX, OwnerMips[0].SizeY, Texture2DRHI->GetFormat(), OwnerMips.Num(), 1, TexCreateFlags, CreateInfo);
			RHIVirtualTextureSetFirstMipInMemory(IntermediateTextureRHI, Context.Resource->GetCurrentFirstMip());
			RHIVirtualTextureSetFirstMipVisible(IntermediateTextureRHI, Context.Resource->GetCurrentFirstMip());
			RHICopySharedMips(IntermediateTextureRHI, Texture2DRHI);
		}
		else
		{
			// Otherwise the current texture is already virtual and we can update it directly.
			IntermediateTextureRHI = Context.Resource->GetTexture2DRHI();
		}
		RHIVirtualTextureSetFirstMipInMemory(IntermediateTextureRHI, PendingFirstMip);
	}
}

bool FTexture2DUpdate::DoConvertToNonVirtual(const FContext& Context)
{
	check(Context.CurrentThread == TT_Render);

	// If the texture is virtual, then create a new copy of the texture.
	if (!IsCancelled() && !IntermediateTextureRHI && Context.Texture && Context.Resource)
	{
		const FTexture2DRHIRef Texture2DRHI = Context.Resource->Texture2DRHI;
		if ((Texture2DRHI->GetFlags() & TexCreate_Virtual) == TexCreate_Virtual)
		{
			const TIndirectArray<FTexture2DMipMap>& OwnerMips = Context.Texture->GetPlatformMips();
			const uint32 TexCreateFlags = Context.Resource->Texture2DRHI->GetFlags() & ~TexCreate_Virtual;

			ensure(!IntermediateTextureRHI);
			FRHIResourceCreateInfo CreateInfo(Context.Resource->ResourceMem);
			IntermediateTextureRHI = RHICreateTexture2D(OwnerMips[PendingFirstMip].SizeX, OwnerMips[PendingFirstMip].SizeY, Texture2DRHI->GetFormat(), OwnerMips.Num() - PendingFirstMip, 1, TexCreateFlags, CreateInfo);
			RHICopySharedMips(IntermediateTextureRHI, Texture2DRHI);

			return true;
		}
	}
	return false;
}

void FTexture2DUpdate::DoFinishUpdate(const FContext& Context)
{
	check(Context.CurrentThread == TT_Render);

	if (IntermediateTextureRHI && Context.Resource)
	{
		if (!IsCancelled())
		{
			Context.Resource->UpdateTexture(IntermediateTextureRHI, PendingFirstMip);
		}
		IntermediateTextureRHI.SafeRelease();

	}
}

bool FTexture2DUpdate::DoConditionalLock(EThreadType InCurrentThread)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DUpdate::DoConditionalLock"), STAT_FTexture2DUpdate_DoConditionalLock, STATGROUP_StreamingDetails);

	// Acquire the lock if there is work to do and if it is allowed to wait for the lock
	int32 CachedTaskState = TS_None;
	do
	{
		// Sleep in between retries.
		if (CachedTaskState != TS_None)
		{
			FPlatformProcess::Sleep(0);
		}
		// Cache the task state.
		CachedTaskState = TaskState;

		//  Return immediately if there is no work to do or if it is locked and we are not on an executing thread.
		if (CachedTaskState == TS_None || (CachedTaskState == TS_Locked && InCurrentThread == TT_None))
		{
			return false;
		}
	}
	while (CachedTaskState == TS_Locked || FPlatformAtomics::InterlockedCompareExchange(&TaskState, TS_Locked, CachedTaskState) != CachedTaskState);

	ensure(PendingTaskState == TS_None);
	PendingTaskState = (ETaskState)CachedTaskState;

	return true;
}


void FTexture2DUpdate::DoLock()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DUpdate::DoLock"), STAT_FTexture2DUpdate_DoLock, STATGROUP_StreamingDetails);

	// Acquire the lock
	int32 CachedTaskState = TS_None;
	do
	{
		// Sleep in between retries.
		if (CachedTaskState != TS_None)
		{
			FPlatformProcess::Sleep(0);
		}
		// Cache the task state.
		CachedTaskState = TaskState;

	}
	while (CachedTaskState == TS_Locked || FPlatformAtomics::InterlockedCompareExchange(&TaskState, TS_Locked, CachedTaskState) != CachedTaskState);
	
	// If we just acquired the lock, nothing should be in the process.
	ensure(PendingTaskState == TS_None);
	PendingTaskState = (ETaskState)CachedTaskState;
}

void FTexture2DUpdate::DoUnlock()
{
	ensure(TaskState == TS_Locked && PendingTaskState != TS_Locked);

	ETaskState CachedPendingTaskState = PendingTaskState;

	// Reset the pending task state first to prevent racing condition that could fail ensure(PendingTaskState == TS_None) in DoLock()
	PendingTaskState = TS_None;
	TaskState = CachedPendingTaskState;
}

void SuspendTextureStreamingRenderTasksInternal()
{
	// This doesn't prevent from having a task pushed immediately after as some threads could already be deep in FTexture2DUpdate::PushTask.
	// This is why GSuspendRenderThreadTasks also gets checked in FTexture2DUpdate::Tick. The goal being to avoid accessing the RHI more
	// than avoiding pushing new render commands. The reason behind is that some code paths access the RHI outside the render thread.
	FPlatformAtomics::InterlockedIncrement(&FTexture2DUpdate::GSuspendRenderThreadTasks);
}

void ResumeTextureStreamingRenderTasksInternal()
{
	FPlatformAtomics::InterlockedDecrement(&FTexture2DUpdate::GSuspendRenderThreadTasks);
}
