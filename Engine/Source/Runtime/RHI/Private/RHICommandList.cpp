// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Async/TaskGraphInterfaces.h"
#include "RHI.h"
#include "ScopeLock.h"

DECLARE_CYCLE_STAT(TEXT("Nonimmed. Command List Execute"), STAT_NonImmedCmdListExecuteTime, STATGROUP_RHICMDLIST);
DECLARE_DWORD_COUNTER_STAT(TEXT("Nonimmed. Command List memory"), STAT_NonImmedCmdListMemory, STATGROUP_RHICMDLIST);
DECLARE_DWORD_COUNTER_STAT(TEXT("Nonimmed. Command count"), STAT_NonImmedCmdListCount, STATGROUP_RHICMDLIST);

DECLARE_CYCLE_STAT(TEXT("All Command List Execute"), STAT_ImmedCmdListExecuteTime, STATGROUP_RHICMDLIST);
DECLARE_DWORD_COUNTER_STAT(TEXT("Immed. Command List memory"), STAT_ImmedCmdListMemory, STATGROUP_RHICMDLIST);
DECLARE_DWORD_COUNTER_STAT(TEXT("Immed. Command count"), STAT_ImmedCmdListCount, STATGROUP_RHICMDLIST);




#if !PLATFORM_USES_FIXED_RHI_CLASS
#include "RHICommandListCommandExecutes.inl"
#endif

static TAutoConsoleVariable<int32> CVarRHICmdBypass(
	TEXT("r.RHICmdBypass"),
	FRHICommandListExecutor::DefaultBypass,
	TEXT("Whether to bypass the rhi command list and send the rhi commands immediately.\n")
	TEXT("0: Disable (required for the multithreaded renderer)\n")
	TEXT("1: Enable (convenient for debugging low level graphics API calls, can suppress artifacts from multithreaded renderer code)"));

static TAutoConsoleVariable<int32> CVarRHICmdUseParallelAlgorithms(
	TEXT("r.RHICmdUseParallelAlgorithms"),
	1,
	TEXT("True to use parallel algorithms. Ignored if r.RHICmdBypass is 1."));

TAutoConsoleVariable<int32> CVarRHICmdWidth(
	TEXT("r.RHICmdWidth"), 
	8,
	TEXT("Controls the task granularity of a great number of things in the parallel renderer."));

static TAutoConsoleVariable<int32> CVarRHICmdUseDeferredContexts(
	TEXT("r.RHICmdUseDeferredContexts"),
	1,
	TEXT("True to use deferred contexts to parallelize command list execution. Only available on some RHIs."));

TAutoConsoleVariable<int32> CVarRHICmdFlushRenderThreadTasks(
	TEXT("r.RHICmdFlushRenderThreadTasks"),
	0,
	TEXT("If true, then we flush the render thread tasks every pass. For issue diagnosis. This is a master switch for more granular cvars."));

TAutoConsoleVariable<int32> CVarRHICmdFlushUpdateTextureReference(
	TEXT("r.RHICmdFlushUpdateTextureReference"),
	0,
	TEXT("If true, then we flush the rhi thread when we do RHIUpdateTextureReference, otherwise this is deferred. For issue diagnosis."));

static TAutoConsoleVariable<int32> CVarRHICmdFlushOnQueueParallelSubmit(
	TEXT("r.RHICmdFlushOnQueueParallelSubmit"),
	0,
	TEXT("Wait for completion of parallel commandlists immediately after submitting. For issue diagnosis. Only available on some RHIs."));

static TAutoConsoleVariable<int32> CVarRHICmdMergeSmallDeferredContexts(
	TEXT("r.RHICmdMergeSmallDeferredContexts"),
	1,
	TEXT("When it can be determined, merge small parallel translate tasks based on r.RHICmdMinDrawsPerParallelCmdList."));

static TAutoConsoleVariable<int32> CVarRHICmdBufferWriteLocks(
	TEXT("r.RHICmdBufferWriteLocks"),
	1,
	TEXT("Only relevant with an RHI thread. Debugging option to diagnose problems with buffered locks."));

static TAutoConsoleVariable<int32> CVarRHICmdAsyncRHIThreadDispatch(
	TEXT("r.RHICmdAsyncRHIThreadDispatch"),
	1,
	TEXT("Experiemental option to do RHI dispatches async. This keeps data flowing to the RHI thread faster and avoid a block at the end of the frame."));

static TAutoConsoleVariable<int32> CVarRHICmdCollectRHIThreadStatsFromHighLevel(
	TEXT("r.RHICmdCollectRHIThreadStatsFromHighLevel"),
	1,
	TEXT("This pushes stats on the RHI thread executes so you can determine which high level pass they came from. This has an adverse effect on framerate. This is on by default."));

static TAutoConsoleVariable<int32> CVarRHICmdUseThread(
	TEXT("r.RHICmdUseThread"),
	1,
	TEXT("Uses the RHI thread. For issue diagnosis."));

static TAutoConsoleVariable<int32> CVarRHICmdForceRHIFlush(
	TEXT("r.RHICmdForceRHIFlush"),
	0,
	TEXT("Force a flush for every task sent to the RHI thread. For issue diagnosis."));

static TAutoConsoleVariable<int32> CVarRHICmdBalanceTranslatesAfterTasks(
	TEXT("r.RHICmdBalanceTranslatesAfterTasks"),
	0,
	TEXT("Experimental option to balance the parallel translates after the render tasks are complete. This minimizes the number of deferred contexts, but adds latency to starting the translates. r.RHICmdBalanceParallelLists overrides and disables this option"));

static TAutoConsoleVariable<int32> CVarRHICmdMinCmdlistForParallelTranslate(
	TEXT("r.RHICmdMinCmdlistForParallelTranslate"),
	2,
	TEXT("If there are fewer than this number of parallel translates, they just run on the RHI thread and immediate context. Only relevant if r.RHICmdBalanceTranslatesAfterTasks is on."));

static TAutoConsoleVariable<int32> CVarRHICmdMinCmdlistSizeForParallelTranslate(
	TEXT("r.RHICmdMinCmdlistSizeForParallelTranslate"),
	32,
	TEXT("In kilobytes. Cmdlists are merged into one parallel translate until we have at least this much memory to process. For a given pass, we won't do more translates than we have task threads. Only relevant if r.RHICmdBalanceTranslatesAfterTasks is on."));


bool GUseRHIThread_InternalUseOnly = false;
bool GUseRHITaskThreads_InternalUseOnly = false;
bool GIsRunningRHIInSeparateThread_InternalUseOnly = false;
bool GIsRunningRHIInDedicatedThread_InternalUseOnly = false;
bool GIsRunningRHIInTaskThread_InternalUseOnly = false;

RHI_API bool GEnableAsyncCompute = true;
RHI_API FRHICommandListExecutor GRHICommandList;

static FGraphEventArray AllOutstandingTasks;
static FGraphEventArray WaitOutstandingTasks;
static FGraphEventRef RHIThreadTask;
static FGraphEventRef RenderThreadSublistDispatchTask;
static FGraphEventRef RHIThreadBufferLockFence;

static FGraphEventRef GRHIThreadEndDrawingViewportFences[2];
static uint32 GRHIThreadEndDrawingViewportFenceIndex = 0;

// Used by AsyncCompute
RHI_API FRHICommandListFenceAllocator GRHIFenceAllocator;

DECLARE_CYCLE_STAT(TEXT("RHI Thread Execute"), STAT_RHIThreadExecute, STATGROUP_RHICMDLIST);

static TStatId GCurrentExecuteStat;

RHI_API FAutoConsoleTaskPriority CPrio_SceneRenderingTask(
	TEXT("TaskGraph.TaskPriorities.SceneRenderingTask"),
	TEXT("Task and thread priority for various scene rendering tasks."),
	ENamedThreads::NormalThreadPriority, 
	ENamedThreads::HighTaskPriority 
	);

struct FRHICommandStat : public FRHICommand<FRHICommandStat>
{
	TStatId CurrentExecuteStat;
	FORCEINLINE_DEBUGGABLE FRHICommandStat(TStatId InCurrentExecuteStat)
		: CurrentExecuteStat(InCurrentExecuteStat)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		GCurrentExecuteStat = CurrentExecuteStat;
	}
};

void FRHICommandListImmediate::SetCurrentStat(TStatId Stat)
{
	if (!Bypass())
	{
		new (AllocCommand<FRHICommandStat>()) FRHICommandStat(Stat);
	}
}

DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.RenderThreadTaskFence"), STAT_RenderThreadTaskFence, STATGROUP_TaskGraphTasks);
DECLARE_CYCLE_STAT(TEXT("Render thread task fence wait"), STAT_RenderThreadTaskFenceWait, STATGROUP_TaskGraphTasks);
FGraphEventRef FRHICommandListImmediate::RenderThreadTaskFence()
{
	FGraphEventRef Result;
	check(IsInRenderingThread());
	//@todo optimize, if there is only one outstanding, then return that instead
	if (WaitOutstandingTasks.Num())
	{
		Result = TGraphTask<FNullGraphTask>::CreateTask(&WaitOutstandingTasks, ENamedThreads::RenderThread).ConstructAndDispatchWhenReady(GET_STATID(STAT_RenderThreadTaskFence), ENamedThreads::RenderThread_Local);
	}
	return Result;
}
void FRHICommandListImmediate::WaitOnRenderThreadTaskFence(FGraphEventRef& Fence)
{
	if (Fence.GetReference() && !Fence->IsComplete())
	{
		SCOPE_CYCLE_COUNTER(STAT_RenderThreadTaskFenceWait);
		check(IsInRenderingThread() && !FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local));
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Fence, ENamedThreads::RenderThread_Local);
	}
}

bool FRHICommandListImmediate::AnyRenderThreadTasksOutstanding()
{
	return !!WaitOutstandingTasks.Num();
}


void FRHIAsyncComputeCommandListImmediate::ImmediateDispatch(FRHIAsyncComputeCommandListImmediate& RHIComputeCmdList)
{
	check(IsInRenderingThread());

	//queue a final command to submit all the async compute commands up to this point to the GPU.
	RHIComputeCmdList.SubmitCommandsHint();

	if (!RHIComputeCmdList.Bypass())
	{
		FRHIAsyncComputeCommandListImmediate* SwapCmdList;
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandListExecutor_SwapCmdLists);
			SwapCmdList = new FRHIAsyncComputeCommandListImmediate();

			//hack stolen from Gfx commandlist.  transfer
			static_assert(sizeof(FRHICommandList) == sizeof(FRHIAsyncComputeCommandListImmediate), "We are memswapping FRHICommandList and FRHICommandListImmediate; they need to be swappable.");
			check(RHIComputeCmdList.IsImmediateAsyncCompute());
			SwapCmdList->ExchangeCmdList(RHIComputeCmdList);
			RHIComputeCmdList.PSOContext = SwapCmdList->PSOContext;

			//queue the execution of this async commandlist amongst other commands in the immediate gfx list.
			//this guarantees resource update commands made on the gfx commandlist will be executed before the async compute.
			FRHICommandListImmediate& RHIImmCmdList = FRHICommandListExecutor::GetImmediateCommandList();
			RHIImmCmdList.QueueAsyncCompute(*SwapCmdList);

			//dispatch immediately to RHI Thread so we can get the async compute on the GPU ASAP.
			RHIImmCmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
		}
	}
}


FRHICommandBase* GCurrentCommand = nullptr;

DECLARE_CYCLE_STAT(TEXT("BigList"), STAT_BigList, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("SmallList"), STAT_SmallList, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("PTrans"), STAT_PTrans, STATGROUP_RHICMDLIST);

void FRHICommandListExecutor::ExecuteInner_DoExecute(FRHICommandListBase& CmdList)
{
	FScopeCycleCounter ScopeOuter(CmdList.ExecuteStat);

	CmdList.bExecuting = true;
	check(CmdList.Context || CmdList.ComputeContext);


	FRHICommandListIterator Iter(CmdList);
#if STATS
	bool bDoStats =  CVarRHICmdCollectRHIThreadStatsFromHighLevel.GetValueOnRenderThread() > 0 && FThreadStats::IsCollectingData() && (IsInRenderingThread() || IsInRHIThread());
	if (bDoStats)
	{
		while (Iter.HasCommandsLeft())
		{
			TStatIdData const* Stat = GCurrentExecuteStat.GetRawPointer();
			FScopeCycleCounter Scope(GCurrentExecuteStat);
			while (Iter.HasCommandsLeft() && Stat == GCurrentExecuteStat.GetRawPointer())
			{
				FRHICommandBase* Cmd = Iter.NextCommand();
				//FPlatformMisc::Prefetch(Cmd->Next);
				Cmd->CallExecuteAndDestruct(CmdList);
			}
		}
	}
	else
#endif
	{
		while (Iter.HasCommandsLeft())
		{
			FRHICommandBase* Cmd = Iter.NextCommand();
			GCurrentCommand = Cmd;
			//FPlatformMisc::Prefetch(Cmd->Next);
			Cmd->CallExecuteAndDestruct(CmdList);
		}
	}
	CmdList.Reset();
}


static FAutoConsoleTaskPriority CPrio_RHIThreadOnTaskThreads(
	TEXT("TaskGraph.TaskPriorities.RHIThreadOnTaskThreads"),
	TEXT("Task and thread priority for when we are running 'RHI thread' tasks on any thread."),
	ENamedThreads::NormalThreadPriority,
	ENamedThreads::NormalTaskPriority
	);


static FCriticalSection GRHIThreadOnTasksCritical;


class FExecuteRHIThreadTask
{
	FRHICommandListBase* RHICmdList;

public:

	FExecuteRHIThreadTask(FRHICommandListBase* InRHICmdList)
		: RHICmdList(InRHICmdList)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FExecuteRHIThreadTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		check(IsRunningRHIInSeparateThread()); // this should never be used on a platform that doesn't support the RHI thread
		return IsRunningRHIInDedicatedThread() ? ENamedThreads::RHIThread : CPrio_RHIThreadOnTaskThreads.Get();
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		SCOPE_CYCLE_COUNTER(STAT_RHIThreadExecute);
		if (IsRunningRHIInTaskThread())
		{
			GRHIThreadId = FPlatformTLS::GetCurrentThreadId();
		}
		{
			FScopeLock Lock(&GRHIThreadOnTasksCritical);
		FRHICommandListExecutor::ExecuteInner_DoExecute(*RHICmdList);
		delete RHICmdList;
	}
		if (IsRunningRHIInTaskThread())
		{
			GRHIThreadId = 0;
		}
	}
};

class FDispatchRHIThreadTask
{
	FRHICommandListBase* RHICmdList;
	bool bRHIThread;

public:

	FDispatchRHIThreadTask(FRHICommandListBase* InRHICmdList, bool bInRHIThread)
		: RHICmdList(InRHICmdList)
		, bRHIThread(bInRHIThread)
	{		
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FDispatchRHIThreadTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		// If we are using async dispatch, this task is somewhat redundant, but it does allow things to wait for dispatch without waiting for execution. 
		// since in that case we will be queuing an rhithread task from an rhithread task, the overhead is minor.
		check(IsRunningRHIInSeparateThread()); // this should never be used on a platform that doesn't support the RHI thread
		return bRHIThread ? (IsRunningRHIInDedicatedThread() ? ENamedThreads::RHIThread : CPrio_RHIThreadOnTaskThreads.Get()) : ENamedThreads::RenderThread_Local;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		check(bRHIThread || IsInRenderingThread());
		FGraphEventArray Prereq;
		if (RHIThreadTask.GetReference())
		{
			Prereq.Add(RHIThreadTask);
		}
		RHIThreadTask = TGraphTask<FExecuteRHIThreadTask>::CreateTask(&Prereq, CurrentThread).ConstructAndDispatchWhenReady(RHICmdList);
	}
};

void FRHICommandListExecutor::ExecuteInner(FRHICommandListBase& CmdList)
{
	check(CmdList.HasCommands()); 

	bool bIsInRenderingThread = IsInRenderingThread();
	bool bIsInGameThread = IsInGameThread();
	if (IsRunningRHIInSeparateThread())
	{
		bool bAsyncSubmit = false;
		if (bIsInRenderingThread)
		{
			if (!bIsInGameThread && !FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandListExecutor_ExecuteInner_DoTasksBeforeDispatch);
				// move anything down the pipe that needs to go
				FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::RenderThread_Local);
			}
			bAsyncSubmit = CVarRHICmdAsyncRHIThreadDispatch.GetValueOnRenderThread() > 0;
			if (RenderThreadSublistDispatchTask.GetReference() && RenderThreadSublistDispatchTask->IsComplete())
			{
				RenderThreadSublistDispatchTask = nullptr;
				if (bAsyncSubmit && RHIThreadTask.GetReference() && RHIThreadTask->IsComplete())
			{
				RHIThreadTask = nullptr;
			}
			}
			if (!bAsyncSubmit && RHIThreadTask.GetReference() && RHIThreadTask->IsComplete())
			{
				RHIThreadTask = nullptr;
			}
		}
		if (CVarRHICmdUseThread.GetValueOnRenderThread() > 0 && bIsInRenderingThread && !bIsInGameThread)
		{
			FRHICommandList* SwapCmdList;
			FGraphEventArray Prereq;
			Exchange(Prereq, CmdList.RTTasks); 
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandListExecutor_SwapCmdLists);
				SwapCmdList = new FRHICommandList;

				// Super scary stuff here, but we just want the swap command list to inherit everything and leave the immediate command list wiped.
				// we should make command lists virtual and transfer ownership rather than this devious approach
				static_assert(sizeof(FRHICommandList) == sizeof(FRHICommandListImmediate), "We are memswapping FRHICommandList and FRHICommandListImmediate; they need to be swappable.");
				SwapCmdList->ExchangeCmdList(CmdList);
				CmdList.PSOContext = SwapCmdList->PSOContext;
			}
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandListExecutor_SubmitTasks);

			//if we use a FDispatchRHIThreadTask, we must have it pass an event along to the FExecuteRHIThreadTask it will spawn so that fences can know which event to wait on for execution completion
			//before the dispatch completes.
			//if we use a FExecuteRHIThreadTask directly we pass the same event just to keep things consistent.
			if (AllOutstandingTasks.Num() || RenderThreadSublistDispatchTask.GetReference())
			{
				Prereq.Append(AllOutstandingTasks);
				AllOutstandingTasks.Reset();
				if (RenderThreadSublistDispatchTask.GetReference())
				{
					Prereq.Add(RenderThreadSublistDispatchTask);
				}
				RenderThreadSublistDispatchTask = TGraphTask<FDispatchRHIThreadTask>::CreateTask(&Prereq, ENamedThreads::RenderThread).ConstructAndDispatchWhenReady(SwapCmdList, bAsyncSubmit);
			}
			else
			{
				check(!RenderThreadSublistDispatchTask.GetReference()); // if we are doing submits, there better not be any of these in flight since then the RHIThreadTask would get out of order.
				if (RHIThreadTask.GetReference())
				{
					Prereq.Add(RHIThreadTask);
				}
				RHIThreadTask = TGraphTask<FExecuteRHIThreadTask>::CreateTask(&Prereq, ENamedThreads::RenderThread).ConstructAndDispatchWhenReady(SwapCmdList);
			}
			if (CVarRHICmdForceRHIFlush.GetValueOnRenderThread() > 0 )
			{
				if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
				{
					// this is a deadlock. RT tasks must be done by now or they won't be done. We could add a third queue...
					UE_LOG(LogRHI, Fatal, TEXT("Deadlock in FRHICommandListExecutor::ExecuteInner 2."));
				}
				if (RenderThreadSublistDispatchTask.GetReference())
				{
					FTaskGraphInterface::Get().WaitUntilTaskCompletes(RenderThreadSublistDispatchTask, ENamedThreads::RenderThread_Local);
					RenderThreadSublistDispatchTask = nullptr;
				}
				while (RHIThreadTask.GetReference())
				{
					FTaskGraphInterface::Get().WaitUntilTaskCompletes(RHIThreadTask, ENamedThreads::RenderThread_Local);
					if (RHIThreadTask.GetReference() && RHIThreadTask->IsComplete())
					{
						RHIThreadTask = nullptr;
					}
				}
			}
			return;
		}
		if (bIsInRenderingThread)
		{
			if (CmdList.RTTasks.Num())
			{
				if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
				{
					// this is a deadlock. RT tasks must be done by now or they won't be done. We could add a third queue...
					UE_LOG(LogRHI, Fatal, TEXT("Deadlock in FRHICommandListExecutor::ExecuteInner (RTTasks)."));
				}
				FTaskGraphInterface::Get().WaitUntilTasksComplete(CmdList.RTTasks, ENamedThreads::RenderThread_Local);
				CmdList.RTTasks.Reset();

			}
			if (RenderThreadSublistDispatchTask.GetReference())
			{
				if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
				{
					// this is a deadlock. RT tasks must be done by now or they won't be done. We could add a third queue...
					UE_LOG(LogRHI, Fatal, TEXT("Deadlock in FRHICommandListExecutor::ExecuteInner (RenderThreadSublistDispatchTask)."));
				}
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(RenderThreadSublistDispatchTask, ENamedThreads::RenderThread_Local);
				RenderThreadSublistDispatchTask = nullptr;
			}
			while (RHIThreadTask.GetReference())
			{
				if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
				{
					// this is a deadlock. RT tasks must be done by now or they won't be done. We could add a third queue...
					UE_LOG(LogRHI, Fatal, TEXT("Deadlock in FRHICommandListExecutor::ExecuteInner (RHIThreadTask)."));
				}
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(RHIThreadTask, ENamedThreads::RenderThread_Local);
				if (RHIThreadTask.GetReference() && RHIThreadTask->IsComplete())
				{
					RHIThreadTask = nullptr;
				}
			}
		}
	}

	ExecuteInner_DoExecute(CmdList);
}


static FORCEINLINE bool IsInRenderingOrRHIThread()
{
	return IsInRenderingThread() || IsInRHIThread();
}

void FRHICommandListExecutor::ExecuteList(FRHICommandListBase& CmdList)
{
	LLM_SCOPE(ELLMTag::RHIMisc);

	check(&CmdList != &GetImmediateCommandList() && (GRHISupportsParallelRHIExecute || IsInRenderingOrRHIThread()));

	if (IsInRenderingThread() && !GetImmediateCommandList().IsExecuting()) // don't flush if this is a recursive call and we are already executing the immediate command list
	{
		GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
	}

	INC_MEMORY_STAT_BY(STAT_NonImmedCmdListMemory, CmdList.GetUsedMemory());
	INC_DWORD_STAT_BY(STAT_NonImmedCmdListCount, CmdList.NumCommands);

	SCOPE_CYCLE_COUNTER(STAT_NonImmedCmdListExecuteTime);
	ExecuteInner(CmdList);
}

void FRHICommandListExecutor::ExecuteList(FRHICommandListImmediate& CmdList)
{
	check(IsInRenderingOrRHIThread() && &CmdList == &GetImmediateCommandList());

	INC_MEMORY_STAT_BY(STAT_ImmedCmdListMemory, CmdList.GetUsedMemory());
	INC_DWORD_STAT_BY(STAT_ImmedCmdListCount, CmdList.NumCommands);
#if 0
	static TAutoConsoleVariable<int32> CVarRHICmdMemDump(
		TEXT("r.RHICmdMemDump"),
		0,
		TEXT("dumps callstacks and sizes of the immediate command lists to the console.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Cheat);
	if (CVarRHICmdMemDump.GetValueOnRenderThread() > 0)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Mem %d\n"), CmdList.GetUsedMemory());
		if (CmdList.GetUsedMemory() > 300)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("big\n"));
		}
	}
#endif
	{
		SCOPE_CYCLE_COUNTER(STAT_ImmedCmdListExecuteTime);
		ExecuteInner(CmdList);
	}
}


void FRHICommandListExecutor::LatchBypass()
{
#if CAN_TOGGLE_COMMAND_LIST_BYPASS
	if (IsRunningRHIInSeparateThread())
	{
		if (bLatchedBypass)
		{
			check((GRHICommandList.OutstandingCmdListCount.GetValue() == 2 && !GRHICommandList.GetImmediateCommandList().HasCommands()) && !GRHICommandList.GetImmediateAsyncComputeCommandList().HasCommands());
			bLatchedBypass = false;
		}
	}
	else
	{
		GRHICommandList.GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);		

		static bool bOnce = false;
		if (!bOnce)
		{
			bOnce = true;
			if (FParse::Param(FCommandLine::Get(),TEXT("forcerhibypass")) && CVarRHICmdBypass.GetValueOnRenderThread() == 0)
			{
				IConsoleVariable* BypassVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RHICmdBypass"));
				BypassVar->Set(1, ECVF_SetByCommandline);
			}
			else if (FParse::Param(FCommandLine::Get(),TEXT("parallelrendering")) && CVarRHICmdBypass.GetValueOnRenderThread() >= 1)
			{
				IConsoleVariable* BypassVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RHICmdBypass"));
				BypassVar->Set(0, ECVF_SetByCommandline);
			}
		}

		check((GRHICommandList.OutstandingCmdListCount.GetValue() == 2 && !GRHICommandList.GetImmediateCommandList().HasCommands() && !GRHICommandList.GetImmediateAsyncComputeCommandList().HasCommands()));

		check(!GDynamicRHI || IsInRenderingThread());
		bool NewBypass = IsInGameThread() || (CVarRHICmdBypass.GetValueOnAnyThread() >= 1);

		if (NewBypass && !bLatchedBypass)
		{
			FRHIResource::FlushPendingDeletes();
		}
		bLatchedBypass = NewBypass;
	}
#endif
	if (!bLatchedBypass)
	{
		bLatchedUseParallelAlgorithms = FApp::ShouldUseThreadingForPerformance() 
#if CAN_TOGGLE_COMMAND_LIST_BYPASS
			&& (CVarRHICmdUseParallelAlgorithms.GetValueOnAnyThread() >= 1)
#endif
			;
	}
	else
	{
		bLatchedUseParallelAlgorithms = false;
	}
}

void FRHICommandListExecutor::CheckNoOutstandingCmdLists()
{
	// else we are attempting to delete resources while there is still a live cmdlist (other than the immediate cmd list) somewhere.
	checkf(GRHICommandList.OutstandingCmdListCount.GetValue() == 2, TEXT("Oustanding: %i"), GRHICommandList.OutstandingCmdListCount.GetValue());
}

bool FRHICommandListExecutor::IsRHIThreadActive()
{
	checkSlow(IsInRenderingThread());
	bool bAsyncSubmit = CVarRHICmdAsyncRHIThreadDispatch.GetValueOnRenderThread() > 0;
	if (bAsyncSubmit)
	{
		if (RenderThreadSublistDispatchTask.GetReference() && RenderThreadSublistDispatchTask->IsComplete())
		{
			RenderThreadSublistDispatchTask = nullptr;
		}
		if (RenderThreadSublistDispatchTask.GetReference())
		{
			return true; // it might become active at any time
		}
		// otherwise we can safely look at RHIThreadTask
	}

	if (RHIThreadTask.GetReference() && RHIThreadTask->IsComplete())
	{
		RHIThreadTask = nullptr;
	}
	return !!RHIThreadTask.GetReference();
}

bool FRHICommandListExecutor::IsRHIThreadCompletelyFlushed()
{
	if (IsRHIThreadActive() || GetImmediateCommandList().HasCommands())
	{
		return false;
	}
	if (RenderThreadSublistDispatchTask.GetReference() && RenderThreadSublistDispatchTask->IsComplete())
	{
		RenderThreadSublistDispatchTask = nullptr;
	}
	return !RenderThreadSublistDispatchTask;
}

struct FRHICommandRHIThreadFence : public FRHICommand<FRHICommandRHIThreadFence>
{
	FGraphEventRef Fence;
	FORCEINLINE_DEBUGGABLE FRHICommandRHIThreadFence()
		: Fence(FGraphEvent::CreateGraphEvent())
{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		check(IsInRHIThread());
		static TArray<FBaseGraphTask*> NewTasks;
		Fence->DispatchSubsequents(NewTasks, IsRunningRHIInDedicatedThread() ? ENamedThreads::RHIThread : ENamedThreads::AnyThread);
		Fence = nullptr;
	}
};


FGraphEventRef FRHICommandListImmediate::RHIThreadFence(bool bSetLockFence)
{
	check(IsInRenderingThread() && IsRunningRHIInSeparateThread());
	FRHICommandRHIThreadFence* Cmd = new (AllocCommand<FRHICommandRHIThreadFence>()) FRHICommandRHIThreadFence();
	if (bSetLockFence)
	{
		RHIThreadBufferLockFence = Cmd->Fence;
	}
	return Cmd->Fence;
}		

DECLARE_CYCLE_STAT(TEXT("Async Compute CmdList Execute"), STAT_AsyncComputeExecute, STATGROUP_RHICMDLIST);
struct FRHIAsyncComputeSubmitList : public FRHICommand<FRHIAsyncComputeSubmitList>
{
	FRHIAsyncComputeCommandList* RHICmdList;
	FORCEINLINE_DEBUGGABLE FRHIAsyncComputeSubmitList(FRHIAsyncComputeCommandList* InRHICmdList)
		: RHICmdList(InRHICmdList)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{		
		SCOPE_CYCLE_COUNTER(STAT_AsyncComputeExecute);
		delete RHICmdList;
	}
};

void FRHICommandListImmediate::QueueAsyncCompute(FRHIAsyncComputeCommandList& RHIComputeCmdList)
{
	if (Bypass())
	{
		SCOPE_CYCLE_COUNTER(STAT_AsyncComputeExecute);
		delete &RHIComputeCmdList;
		return;
	}
	new (AllocCommand<FRHIAsyncComputeSubmitList>()) FRHIAsyncComputeSubmitList(&RHIComputeCmdList);
}
	
void FRHICommandListExecutor::WaitOnRHIThreadFence(FGraphEventRef& Fence)
{
	check(IsInRenderingThread());
	if (Fence.GetReference() && !Fence->IsComplete())
	{
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_WaitOnRHIThreadFence_Dispatch);
			GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // necessary to prevent deadlock
		}
		check(IsRunningRHIInSeparateThread());
		QUICK_SCOPE_CYCLE_COUNTER(STAT_WaitOnRHIThreadFence_Wait);
		if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
		{
			// this is a deadlock. RT tasks must be done by now or they won't be done. We could add a third queue...
			UE_LOG(LogRHI, Fatal, TEXT("Deadlock in WaitOnRHIThreadFence."));
		}
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Fence, ENamedThreads::RenderThread_Local);
	}
}




FRHICommandListBase::FRHICommandListBase()
	: Root(nullptr)
	, CommandLink(nullptr)
	, bExecuting(false)
	, NumCommands(0)
	, UID(UINT32_MAX)
	, Context(nullptr)
	, ComputeContext(nullptr)
	, MemManager(0)
{
	GRHICommandList.OutstandingCmdListCount.Increment();
	Reset();
}

FRHICommandListBase::~FRHICommandListBase()
{
	Flush();
	GRHICommandList.OutstandingCmdListCount.Decrement();
}

const int32 FRHICommandListBase::GetUsedMemory() const
{
	return MemManager.GetByteCount();
}

void FRHICommandListBase::Reset()
{
	bExecuting = false;
	check(!RTTasks.Num());
	MemManager.Flush();
	NumCommands = 0;
	Root = nullptr;
	CommandLink = &Root;
	Context = GDynamicRHI ? RHIGetDefaultContext() : nullptr;

	if (GEnableAsyncCompute)
	{
		ComputeContext = GDynamicRHI ? RHIGetDefaultAsyncComputeContext() : nullptr;
	}
	else
	{
		ComputeContext = Context;
	}
	
	UID = GRHICommandList.UIDCounter.Increment();
	for (int32 Index = 0; ERenderThreadContext(Index) < ERenderThreadContext::Num; Index++)
	{
		RenderThreadContexts[Index] = nullptr;
	}
	ExecuteStat = TStatId();
}


DECLARE_CYCLE_STAT(TEXT("Parallel Async Chain Translate"), STAT_ParallelChainTranslate, STATGROUP_RHICMDLIST);

FAutoConsoleTaskPriority CPrio_FParallelTranslateCommandList(
	TEXT("TaskGraph.TaskPriorities.ParallelTranslateCommandList"),
	TEXT("Task and thread priority for FParallelTranslateCommandList."),
	ENamedThreads::NormalThreadPriority,
	ENamedThreads::NormalTaskPriority
	);

FAutoConsoleTaskPriority CPrio_FParallelTranslateCommandListPrepass(
	TEXT("TaskGraph.TaskPriorities.ParallelTranslateCommandListPrepass"),
	TEXT("Task and thread priority for FParallelTranslateCommandList for the prepass, which we would like to get to the GPU asap."),
	ENamedThreads::NormalThreadPriority,
	ENamedThreads::HighTaskPriority
	);

class FParallelTranslateCommandList
{
	FRHICommandListBase** RHICmdLists;
	int32 NumCommandLists;
	IRHICommandContextContainer* ContextContainer;
	bool bIsPrepass;
public:

	FParallelTranslateCommandList(FRHICommandListBase** InRHICmdLists, int32 InNumCommandLists, IRHICommandContextContainer* InContextContainer, bool bInIsPrepass)
		: RHICmdLists(InRHICmdLists)
		, NumCommandLists(InNumCommandLists)
		, ContextContainer(InContextContainer)
		, bIsPrepass(bInIsPrepass)
	{
		check(RHICmdLists && ContextContainer && NumCommandLists);
	}

	static FORCEINLINE TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FParallelTranslateCommandList, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return bIsPrepass ? CPrio_FParallelTranslateCommandListPrepass.Get() : CPrio_FParallelTranslateCommandList.Get();
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		SCOPE_CYCLE_COUNTER(STAT_ParallelChainTranslate);
		SCOPED_NAMED_EVENT(FParallelTranslateCommandList_DoTask, FColor::Magenta);
		check(ContextContainer && RHICmdLists);
		IRHICommandContext* Context = ContextContainer->GetContext();
		check(Context);
		for (int32 Index = 0; Index < NumCommandLists; Index++)
		{
			RHICmdLists[Index]->SetContext(Context);
			delete RHICmdLists[Index];
		}
		ContextContainer->FinishContext();
	}
};

DECLARE_DWORD_COUNTER_STAT(TEXT("Num Parallel Async Chains Links"), STAT_ParallelChainLinkCount, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Wait for Parallel Async CmdList"), STAT_ParallelChainWait, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Parallel Async Chain Execute"), STAT_ParallelChainExecute, STATGROUP_RHICMDLIST);

struct FRHICommandWaitForAndSubmitSubListParallel : public FRHICommand<FRHICommandWaitForAndSubmitSubListParallel>
{
	FGraphEventRef TranslateCompletionEvent;
	IRHICommandContextContainer* ContextContainer;
	int32 Num;
	int32 Index;

	FORCEINLINE_DEBUGGABLE FRHICommandWaitForAndSubmitSubListParallel(FGraphEventRef& InTranslateCompletionEvent, IRHICommandContextContainer* InContextContainer, int32 InNum, int32 InIndex)
		: TranslateCompletionEvent(InTranslateCompletionEvent)
		, ContextContainer(InContextContainer)
		, Num(InNum)
		, Index(InIndex)
	{
		check(ContextContainer && Num);
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		check(ContextContainer && Num && IsInRHIThread());
		INC_DWORD_STAT_BY(STAT_ParallelChainLinkCount, 1);

		if (TranslateCompletionEvent.GetReference() && !TranslateCompletionEvent->IsComplete())
		{
			SCOPE_CYCLE_COUNTER(STAT_ParallelChainWait);
			if (IsInRenderingThread())
			{
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(TranslateCompletionEvent, ENamedThreads::RenderThread_Local);
			}
			else if (IsInRHIThread())
			{
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(TranslateCompletionEvent, IsRunningRHIInDedicatedThread() ? ENamedThreads::RHIThread : ENamedThreads::AnyThread);
			}
			else
			{
				check(0);
			}
		}
		{
			SCOPE_CYCLE_COUNTER(STAT_ParallelChainExecute);
			ContextContainer->SubmitAndFreeContextContainer(Index, Num);
		}
	}
};



DECLARE_DWORD_COUNTER_STAT(TEXT("Num Async Chains Links"), STAT_ChainLinkCount, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Wait for Async CmdList"), STAT_ChainWait, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Async Chain Execute"), STAT_ChainExecute, STATGROUP_RHICMDLIST);

FGraphEvent* GEventToWaitFor = nullptr;

struct FRHICommandWaitForAndSubmitSubList : public FRHICommand<FRHICommandWaitForAndSubmitSubList>
{
	FGraphEventRef EventToWaitFor;
	FRHICommandListBase* RHICmdList;
	FORCEINLINE_DEBUGGABLE FRHICommandWaitForAndSubmitSubList(FGraphEventRef& InEventToWaitFor, FRHICommandListBase* InRHICmdList)
		: EventToWaitFor(InEventToWaitFor)
		, RHICmdList(InRHICmdList)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		INC_DWORD_STAT_BY(STAT_ChainLinkCount, 1);
		if (EventToWaitFor.GetReference() && !EventToWaitFor->IsComplete() && !(!IsRunningRHIInSeparateThread() || !IsInRHIThread()))
		{
			GEventToWaitFor = EventToWaitFor.GetReference();
			FPlatformMisc::DebugBreak();
			check(EventToWaitFor->IsComplete());
		}
		if (EventToWaitFor.GetReference() && !EventToWaitFor->IsComplete())
		{
			check(!IsRunningRHIInSeparateThread() || !IsInRHIThread()); // things should not be dispatched if they can't complete without further waits
			SCOPE_CYCLE_COUNTER(STAT_ChainWait);
			if (IsInRenderingThread())
			{
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(EventToWaitFor, ENamedThreads::RenderThread_Local);
			}
			else
			{
				check(0);
			}
		}
		{
			SCOPE_CYCLE_COUNTER(STAT_ChainExecute);
			RHICmdList->CopyContext(CmdList);
			delete RHICmdList;
		}
	}
};


DECLARE_CYCLE_STAT(TEXT("Parallel Setup Translate"), STAT_ParallelSetupTranslate, STATGROUP_RHICMDLIST);

FAutoConsoleTaskPriority CPrio_FParallelTranslateSetupCommandList(
	TEXT("TaskGraph.TaskPriorities.ParallelTranslateSetupCommandList"),
	TEXT("Task and thread priority for FParallelTranslateSetupCommandList."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::HighTaskPriority, // .. at high task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);

class FParallelTranslateSetupCommandList
{
	FRHICommandList* RHICmdList;
	FRHICommandListBase** RHICmdLists;
	int32 NumCommandLists;
	bool bIsPrepass;
	int32 MinSize;
	int32 MinCount;
public:

	FParallelTranslateSetupCommandList(FRHICommandList* InRHICmdList, FRHICommandListBase** InRHICmdLists, int32 InNumCommandLists, bool bInIsPrepass)
		: RHICmdList(InRHICmdList)
		, RHICmdLists(InRHICmdLists)
		, NumCommandLists(InNumCommandLists)
		, bIsPrepass(bInIsPrepass)
	{
		check(RHICmdList && RHICmdLists && NumCommandLists);
		MinSize = CVarRHICmdMinCmdlistSizeForParallelTranslate.GetValueOnRenderThread() * 1024;
		MinCount = CVarRHICmdMinCmdlistForParallelTranslate.GetValueOnRenderThread();
	}

	static FORCEINLINE TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FParallelTranslateSetupCommandList, STATGROUP_TaskGraphTasks);
	}

	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_FParallelTranslateSetupCommandList.Get();
	}

	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		SCOPE_CYCLE_COUNTER(STAT_ParallelSetupTranslate);

		TArray<int32, TInlineAllocator<64> > Sizes;
		Sizes.Reserve(NumCommandLists);
		for (int32 Index = 0; Index < NumCommandLists; Index++)
		{
			Sizes.Add(RHICmdLists[Index]->GetUsedMemory());
		}

		int32 EffectiveThreads = 0;
		int32 Start = 0;
		// this is pretty silly but we need to know the number of jobs in advance, so we run the merge logic twice
		while (Start < NumCommandLists)
		{
			int32 Last = Start;
			int32 DrawCnt = Sizes[Start];

			while (Last < NumCommandLists - 1 && DrawCnt + Sizes[Last + 1] <= MinSize)
			{
				Last++;
				DrawCnt += Sizes[Last];
			}
			check(Last >= Start);
			Start = Last + 1;
			EffectiveThreads++;
		} 

		if (EffectiveThreads < MinCount)
		{
			FGraphEventRef Nothing;
			for (int32 Index = 0; Index < NumCommandLists; Index++)
			{
				FRHICommandListBase* CmdList = RHICmdLists[Index];
				new (RHICmdList->AllocCommand<FRHICommandWaitForAndSubmitSubList>()) FRHICommandWaitForAndSubmitSubList(Nothing, CmdList);
			}
		}
		else
		{
			Start = 0;
			int32 ThreadIndex = 0;

			while (Start < NumCommandLists)
			{
				int32 Last = Start;
				int32 DrawCnt = Sizes[Start];

				while (Last < NumCommandLists - 1 && DrawCnt + Sizes[Last + 1] <= MinSize)
				{
					Last++;
					DrawCnt += Sizes[Last];
				}
				check(Last >= Start);

				IRHICommandContextContainer* ContextContainer =  RHIGetCommandContextContainer(ThreadIndex, EffectiveThreads);
				check(ContextContainer);

				FGraphEventRef TranslateCompletionEvent = TGraphTask<FParallelTranslateCommandList>::CreateTask(nullptr, ENamedThreads::RenderThread).ConstructAndDispatchWhenReady(&RHICmdLists[Start], 1 + Last - Start, ContextContainer, bIsPrepass);
				MyCompletionGraphEvent->DontCompleteUntil(TranslateCompletionEvent);
				new (RHICmdList->AllocCommand<FRHICommandWaitForAndSubmitSubListParallel>()) FRHICommandWaitForAndSubmitSubListParallel(TranslateCompletionEvent, ContextContainer, EffectiveThreads, ThreadIndex++);
				Start = Last + 1;
			}
			check(EffectiveThreads == ThreadIndex);
		}
	}
};

void FRHICommandListBase::QueueParallelAsyncCommandListSubmit(FGraphEventRef* AnyThreadCompletionEvents, bool bIsPrepass, FRHICommandList** CmdLists, int32* NumDrawsIfKnown, int32 Num, int32 MinDrawsPerTranslate, bool bSpewMerge)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandListBase_QueueParallelAsyncCommandListSubmit);
	check(IsInRenderingThread() && IsImmediate() && Num);

	if (IsRunningRHIInSeparateThread())
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we should start on the stuff before this async list

		// as good a place as any to clear this
		if (RHIThreadBufferLockFence.GetReference() && RHIThreadBufferLockFence->IsComplete())
		{
			RHIThreadBufferLockFence = nullptr;
		}
	}
#if !UE_BUILD_SHIPPING
	// do a flush before hand so we can tell if it was this parallel set that broke something, or what came before.
	if (CVarRHICmdFlushOnQueueParallelSubmit.GetValueOnRenderThread())
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
#endif

	if (Num && IsRunningRHIInSeparateThread())
	{
		static const auto ICVarRHICmdBalanceParallelLists = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RHICmdBalanceParallelLists"));

		if (ICVarRHICmdBalanceParallelLists->GetValueOnRenderThread() == 0 && CVarRHICmdBalanceTranslatesAfterTasks.GetValueOnRenderThread() > 0 && GRHISupportsParallelRHIExecute && CVarRHICmdUseDeferredContexts.GetValueOnAnyThread() > 0)
		{
			FGraphEventArray Prereq;
			FRHICommandListBase** RHICmdLists = (FRHICommandListBase**)Alloc(sizeof(FRHICommandListBase*) * Num, alignof(FRHICommandListBase*));
			for (int32 Index = 0; Index < Num; Index++)
			{
				FGraphEventRef& AnyThreadCompletionEvent = AnyThreadCompletionEvents[Index];
				FRHICommandList* CmdList = CmdLists[Index];
				RHICmdLists[Index] = CmdList;
				if (AnyThreadCompletionEvent.GetReference())
				{
					Prereq.Add(AnyThreadCompletionEvent);
					WaitOutstandingTasks.Add(AnyThreadCompletionEvent);
				}
			}
			// this is used to ensure that any old buffer locks are completed before we start any parallel translates
			if (RHIThreadBufferLockFence.GetReference())
			{
				Prereq.Add(RHIThreadBufferLockFence);
			}
			FRHICommandList* CmdList = new FRHICommandList;
			CmdList->CopyRenderThreadContexts(*this);
			FGraphEventRef TranslateSetupCompletionEvent = TGraphTask<FParallelTranslateSetupCommandList>::CreateTask(&Prereq, ENamedThreads::RenderThread).ConstructAndDispatchWhenReady(CmdList, &RHICmdLists[0], Num, bIsPrepass);
			QueueCommandListSubmit(CmdList);
			AllOutstandingTasks.Add(TranslateSetupCompletionEvent);
			if (IsRunningRHIInSeparateThread())
			{
				FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we don't want stuff after the async cmd list to be bundled with it
			}
#if !UE_BUILD_SHIPPING
			if (CVarRHICmdFlushOnQueueParallelSubmit.GetValueOnRenderThread())
			{
				FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			}
#endif
			return;
		}
		IRHICommandContextContainer* ContextContainer = nullptr;
		bool bMerge = !!CVarRHICmdMergeSmallDeferredContexts.GetValueOnRenderThread();
		int32 EffectiveThreads = 0;
		int32 Start = 0;
		int32 ThreadIndex = 0;
		if (GRHISupportsParallelRHIExecute && CVarRHICmdUseDeferredContexts.GetValueOnAnyThread() > 0)
		{
			// this is pretty silly but we need to know the number of jobs in advance, so we run the merge logic twice
			while (Start < Num)
			{
				int32 Last = Start;
				int32 DrawCnt = NumDrawsIfKnown[Start];

				if (bMerge && DrawCnt >= 0)
				{
					while (Last < Num - 1 && NumDrawsIfKnown[Last + 1] >= 0 && DrawCnt + NumDrawsIfKnown[Last + 1] <= MinDrawsPerTranslate)
					{
						Last++;
						DrawCnt += NumDrawsIfKnown[Last];
					}
				}
				check(Last >= Start);
				Start = Last + 1;
				EffectiveThreads++;
			}

			Start = 0;
			ContextContainer = RHIGetCommandContextContainer(ThreadIndex, EffectiveThreads);
		}
		if (ContextContainer)
		{
			while (Start < Num)
			{
				int32 Last = Start;
				int32 DrawCnt = NumDrawsIfKnown[Start];
				int32 TotalMem = bSpewMerge ? CmdLists[Start]->GetUsedMemory() : 0;   // the memory is only accurate if we are spewing because otherwise it isn't done yet!

				if (bMerge && DrawCnt >= 0)
				{
					while (Last < Num - 1 && NumDrawsIfKnown[Last + 1] >= 0 && DrawCnt + NumDrawsIfKnown[Last + 1] <= MinDrawsPerTranslate)
					{
						Last++;
						DrawCnt += NumDrawsIfKnown[Last];
						TotalMem += bSpewMerge ? CmdLists[Start]->GetUsedMemory() : 0;   // the memory is only accurate if we are spewing because otherwise it isn't done yet!
					}
				}

				check(Last >= Start);

				if (!ContextContainer)
				{
					ContextContainer = RHIGetCommandContextContainer(ThreadIndex, EffectiveThreads);
				}
				check(ContextContainer);

				FGraphEventArray Prereq;
				FRHICommandListBase** RHICmdLists = (FRHICommandListBase**)Alloc(sizeof(FRHICommandListBase*) * (1 + Last - Start), alignof(FRHICommandListBase*));
				for (int32 Index = Start; Index <= Last; Index++)
				{
					FGraphEventRef& AnyThreadCompletionEvent = AnyThreadCompletionEvents[Index];
					FRHICommandList* CmdList = CmdLists[Index];
					RHICmdLists[Index - Start] = CmdList;
					if (AnyThreadCompletionEvent.GetReference())
					{
						Prereq.Add(AnyThreadCompletionEvent);
						AllOutstandingTasks.Add(AnyThreadCompletionEvent);
						WaitOutstandingTasks.Add(AnyThreadCompletionEvent);
					}
				}
				UE_CLOG(bSpewMerge, LogTemp, Display, TEXT("Parallel translate %d->%d    %dKB mem   %d draws (-1 = unknown)"), Start, Last, FMath::DivideAndRoundUp(TotalMem, 1024), DrawCnt);

				// this is used to ensure that any old buffer locks are completed before we start any parallel translates
				if (RHIThreadBufferLockFence.GetReference())
				{
					Prereq.Add(RHIThreadBufferLockFence);
				}

				FGraphEventRef TranslateCompletionEvent = TGraphTask<FParallelTranslateCommandList>::CreateTask(&Prereq, ENamedThreads::RenderThread).ConstructAndDispatchWhenReady(&RHICmdLists[0], 1 + Last - Start, ContextContainer, bIsPrepass);

				AllOutstandingTasks.Add(TranslateCompletionEvent);
				new (AllocCommand<FRHICommandWaitForAndSubmitSubListParallel>()) FRHICommandWaitForAndSubmitSubListParallel(TranslateCompletionEvent, ContextContainer, EffectiveThreads, ThreadIndex++);
				if (IsRunningRHIInSeparateThread())
				{
					FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we don't want stuff after the async cmd list to be bundled with it
				}

				ContextContainer = nullptr;
				Start = Last + 1;
			}
			check(EffectiveThreads == ThreadIndex);
#if !UE_BUILD_SHIPPING
			if (CVarRHICmdFlushOnQueueParallelSubmit.GetValueOnRenderThread())
			{
				FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			}
#endif
			return;
		}
	}
	for (int32 Index = 0; Index < Num; Index++)
	{
		FGraphEventRef& AnyThreadCompletionEvent = AnyThreadCompletionEvents[Index];
		FRHICommandList* CmdList = CmdLists[Index];
		if (AnyThreadCompletionEvent.GetReference())
		{
			if (IsRunningRHIInSeparateThread())
			{
				AllOutstandingTasks.Add(AnyThreadCompletionEvent);
			}
			WaitOutstandingTasks.Add(AnyThreadCompletionEvent);
		}
		new (AllocCommand<FRHICommandWaitForAndSubmitSubList>()) FRHICommandWaitForAndSubmitSubList(AnyThreadCompletionEvent, CmdList);
	}
	if (IsRunningRHIInSeparateThread())
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we don't want stuff after the async cmd list to be bundled with it
	}
}

void FRHICommandListBase::QueueParallelAsyncCommandListSubmit(FGraphEventRef* AnyThreadCompletionEvents, bool bIsPrepass, FRHIRenderSubPassCommandList** CmdLists, int32* NumDrawsIfKnown, int32 Num, int32 MinDrawsPerTranslate, bool bSpewMerge)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandListBase_QueueParallelAsyncCommandListSubmit);
	check(IsInRenderingThread() && IsImmediate() && Num);
	
	if (IsRunningRHIInSeparateThread())
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we should start on the stuff before this async list

		// as good a place as any to clear this
		if (RHIThreadBufferLockFence.GetReference() && RHIThreadBufferLockFence->IsComplete())
		{
			RHIThreadBufferLockFence = nullptr;
		}
	}
#if !UE_BUILD_SHIPPING
	// do a flush before hand so we can tell if it was this parallel set that broke something, or what came before.
	if (CVarRHICmdFlushOnQueueParallelSubmit.GetValueOnRenderThread())
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
#endif

	if (Num && IsRunningRHIInSeparateThread())
	{
		static const auto ICVarRHICmdBalanceParallelLists = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RHICmdBalanceParallelLists"));

		if (ICVarRHICmdBalanceParallelLists->GetValueOnRenderThread() == 0 && CVarRHICmdBalanceTranslatesAfterTasks.GetValueOnRenderThread() > 0 && GRHISupportsParallelRHIExecute && CVarRHICmdUseDeferredContexts.GetValueOnAnyThread() > 0)
		{
			FGraphEventArray Prereq;
			FRHICommandListBase** RHICmdLists = (FRHICommandListBase**)Alloc(sizeof(FRHICommandListBase*) * Num, alignof(FRHICommandListBase*));
			for (int32 Index = 0; Index < Num; Index++)
			{
				FGraphEventRef& AnyThreadCompletionEvent = AnyThreadCompletionEvents[Index];
				FRHIRenderSubPassCommandList* CmdList = CmdLists[Index];
				RHICmdLists[Index] = CmdList;
				if (AnyThreadCompletionEvent.GetReference())
				{
					Prereq.Add(AnyThreadCompletionEvent);
					WaitOutstandingTasks.Add(AnyThreadCompletionEvent);
				}
			}
			// this is used to ensure that any old buffer locks are completed before we start any parallel translates
			if (RHIThreadBufferLockFence.GetReference())
			{
				Prereq.Add(RHIThreadBufferLockFence);
			}
			FRHICommandList* CmdList = new FRHICommandList;
			CmdList->CopyRenderThreadContexts(*this);
			FGraphEventRef TranslateSetupCompletionEvent = TGraphTask<FParallelTranslateSetupCommandList>::CreateTask(&Prereq, ENamedThreads::RenderThread).ConstructAndDispatchWhenReady(CmdList, &RHICmdLists[0], Num, bIsPrepass);
			QueueCommandListSubmit(CmdList);
			AllOutstandingTasks.Add(TranslateSetupCompletionEvent);
			if (IsRunningRHIInSeparateThread())
			{
				FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we don't want stuff after the async cmd list to be bundled with it
			}
#if !UE_BUILD_SHIPPING
			if (CVarRHICmdFlushOnQueueParallelSubmit.GetValueOnRenderThread())
			{
				FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			}
#endif
			return;
		}
		IRHICommandContextContainer* ContextContainer = nullptr;
			bool bMerge = !!CVarRHICmdMergeSmallDeferredContexts.GetValueOnRenderThread();
			int32 EffectiveThreads = 0;
			int32 Start = 0;
		int32 ThreadIndex = 0;
		if (GRHISupportsParallelRHIExecute && CVarRHICmdUseDeferredContexts.GetValueOnAnyThread() > 0)
		{
			// this is pretty silly but we need to know the number of jobs in advance, so we run the merge logic twice
			while (Start < Num)
			{
				int32 Last = Start;
				int32 DrawCnt = NumDrawsIfKnown[Start];
				
				if (bMerge && DrawCnt >= 0)
				{
					while (Last < Num - 1 && NumDrawsIfKnown[Last + 1] >= 0 && DrawCnt + NumDrawsIfKnown[Last + 1] <= MinDrawsPerTranslate)
					{
						Last++;
						DrawCnt += NumDrawsIfKnown[Last];
					}
				}
				check(Last >= Start);
				Start = Last + 1;
				EffectiveThreads++;
			}
			
			Start = 0;
			ContextContainer = RHIGetCommandContextContainer(ThreadIndex, EffectiveThreads);
		}
		if (ContextContainer)
		{
			while (Start < Num)
			{    
				int32 Last = Start;
				int32 DrawCnt = NumDrawsIfKnown[Start];
				int32 TotalMem = bSpewMerge ? CmdLists[Start]->GetUsedMemory() : 0;   // the memory is only accurate if we are spewing because otherwise it isn't done yet!

				if (bMerge && DrawCnt >= 0)
				{
					while (Last < Num - 1 && NumDrawsIfKnown[Last + 1] >= 0 && DrawCnt + NumDrawsIfKnown[Last + 1] <= MinDrawsPerTranslate)
					{
						Last++;
						DrawCnt += NumDrawsIfKnown[Last];
						TotalMem += bSpewMerge ? CmdLists[Start]->GetUsedMemory() : 0;   // the memory is only accurate if we are spewing because otherwise it isn't done yet!
					}
				}

				check(Last >= Start);

				if (!ContextContainer) 
				{
					ContextContainer = RHIGetCommandContextContainer(ThreadIndex, EffectiveThreads);
				}
				check(ContextContainer);

				FGraphEventArray Prereq;
				FRHICommandListBase** RHICmdLists = (FRHICommandListBase**)Alloc(sizeof(FRHICommandListBase*) * (1 + Last - Start), alignof(FRHICommandListBase*));
				for (int32 Index = Start; Index <= Last; Index++)
				{
					FGraphEventRef& AnyThreadCompletionEvent = AnyThreadCompletionEvents[Index];
					FRHIRenderSubPassCommandList* CmdList = CmdLists[Index];
					RHICmdLists[Index - Start] = CmdList;
					if (AnyThreadCompletionEvent.GetReference())
					{
						Prereq.Add(AnyThreadCompletionEvent);
						AllOutstandingTasks.Add(AnyThreadCompletionEvent);
						WaitOutstandingTasks.Add(AnyThreadCompletionEvent);
					}
				}
				UE_CLOG(bSpewMerge, LogTemp, Display, TEXT("Parallel translate %d->%d    %dKB mem   %d draws (-1 = unknown)"), Start, Last, FMath::DivideAndRoundUp(TotalMem, 1024), DrawCnt);

				// this is used to ensure that any old buffer locks are completed before we start any parallel translates
				if (RHIThreadBufferLockFence.GetReference())
				{
					Prereq.Add(RHIThreadBufferLockFence);
				}

				FGraphEventRef TranslateCompletionEvent = TGraphTask<FParallelTranslateCommandList>::CreateTask(&Prereq, ENamedThreads::RenderThread).ConstructAndDispatchWhenReady(&RHICmdLists[0], 1 + Last - Start, ContextContainer, bIsPrepass);

				AllOutstandingTasks.Add(TranslateCompletionEvent);
				new (AllocCommand<FRHICommandWaitForAndSubmitSubListParallel>()) FRHICommandWaitForAndSubmitSubListParallel(TranslateCompletionEvent, ContextContainer, EffectiveThreads, ThreadIndex++);
				if (IsRunningRHIInSeparateThread())
				{
					FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we don't want stuff after the async cmd list to be bundled with it
				}

				ContextContainer = nullptr;
				Start = Last + 1;
			}
			check(EffectiveThreads == ThreadIndex);
#if !UE_BUILD_SHIPPING
			if (CVarRHICmdFlushOnQueueParallelSubmit.GetValueOnRenderThread())
			{
				FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			}
#endif
			return;
		}
	}
	for (int32 Index = 0; Index < Num; Index++)
	{
		FGraphEventRef& AnyThreadCompletionEvent = AnyThreadCompletionEvents[Index];
		FRHIRenderSubPassCommandList* CmdList = CmdLists[Index];
		if (AnyThreadCompletionEvent.GetReference())
		{
			if (IsRunningRHIInSeparateThread())
			{
				AllOutstandingTasks.Add(AnyThreadCompletionEvent);
			}
			WaitOutstandingTasks.Add(AnyThreadCompletionEvent);
		}
		new (AllocCommand<FRHICommandWaitForAndSubmitSubList>()) FRHICommandWaitForAndSubmitSubList(AnyThreadCompletionEvent, CmdList);
	}
	if (IsRunningRHIInSeparateThread())
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we don't want stuff after the async cmd list to be bundled with it
	}
}

void FRHICommandListBase::QueueAsyncCommandListSubmit(FGraphEventRef& AnyThreadCompletionEvent, class FRHICommandList* CmdList)
{
	check(IsInRenderingThread() && IsImmediate());

	if (IsRunningRHIInSeparateThread())
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we should start on the stuff before this async list
	}
	if (AnyThreadCompletionEvent.GetReference())
	{
		if (IsRunningRHIInSeparateThread())
		{
			AllOutstandingTasks.Add(AnyThreadCompletionEvent);
		}
		WaitOutstandingTasks.Add(AnyThreadCompletionEvent);
	}
	new (AllocCommand<FRHICommandWaitForAndSubmitSubList>()) FRHICommandWaitForAndSubmitSubList(AnyThreadCompletionEvent, CmdList);
	if (IsRunningRHIInSeparateThread())
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we don't want stuff after the async cmd list to be bundled with it
	}
}

void FRHICommandListBase::QueueAsyncCommandListSubmit(FGraphEventRef& AnyThreadCompletionEvent, class FRHIRenderSubPassCommandList* CmdList)
{
	check(IsInRenderingThread() /*&& IsImmediate()*/);

	if (IsRunningRHIInSeparateThread())
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we should start on the stuff before this async list
	}
	if (AnyThreadCompletionEvent.GetReference())
	{
		if (IsRunningRHIInSeparateThread())
		{
			AllOutstandingTasks.Add(AnyThreadCompletionEvent);
		}
		WaitOutstandingTasks.Add(AnyThreadCompletionEvent);
	}
	new (AllocCommand<FRHICommandWaitForAndSubmitSubList>()) FRHICommandWaitForAndSubmitSubList(AnyThreadCompletionEvent, CmdList);
	if (IsRunningRHIInSeparateThread())
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we don't want stuff after the async cmd list to be bundled with it
	}
}

DECLARE_DWORD_COUNTER_STAT(TEXT("Num RT Chains Links"), STAT_RTChainLinkCount, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Wait for RT CmdList"), STAT_RTChainWait, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("RT Chain Execute"), STAT_RTChainExecute, STATGROUP_RHICMDLIST);

struct FRHICommandWaitForAndSubmitRTSubList : public FRHICommand<FRHICommandWaitForAndSubmitRTSubList>
{
	FGraphEventRef EventToWaitFor;
	FRHICommandList* RHICmdList;
	FORCEINLINE_DEBUGGABLE FRHICommandWaitForAndSubmitRTSubList(FGraphEventRef& InEventToWaitFor, FRHICommandList* InRHICmdList)
		: EventToWaitFor(InEventToWaitFor)
		, RHICmdList(InRHICmdList)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		INC_DWORD_STAT_BY(STAT_RTChainLinkCount, 1);
		{
			if (EventToWaitFor.GetReference() && !EventToWaitFor->IsComplete())
			{
			SCOPE_CYCLE_COUNTER(STAT_RTChainWait);
				check(!IsRunningRHIInSeparateThread() || !IsInRHIThread()); // things should not be dispatched if they can't complete without further waits
				if (IsInRenderingThread())
				{
					if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
					{
						// this is a deadlock. RT tasks must be done by now or they won't be done. We could add a third queue...
						UE_LOG(LogRHI, Fatal, TEXT("Deadlock in command list processing."));
					}
					FTaskGraphInterface::Get().WaitUntilTaskCompletes(EventToWaitFor, ENamedThreads::RenderThread_Local);
				}
				else
				{
					FTaskGraphInterface::Get().WaitUntilTaskCompletes(EventToWaitFor);
				}
			}
		}
		{
			SCOPE_CYCLE_COUNTER(STAT_RTChainExecute);
			RHICmdList->CopyContext(CmdList);
			delete RHICmdList;
		}
	}
};

void FRHICommandListBase::QueueRenderThreadCommandListSubmit(FGraphEventRef& RenderThreadCompletionEvent, class FRHICommandList* CmdList)
{
	check(!IsInRHIThread());

	if (RenderThreadCompletionEvent.GetReference())
	{
		check(!IsInActualRenderingThread() && !IsInGameThread() && !IsImmediate());
		RTTasks.Add(RenderThreadCompletionEvent);
	}
	new (AllocCommand<FRHICommandWaitForAndSubmitRTSubList>()) FRHICommandWaitForAndSubmitRTSubList(RenderThreadCompletionEvent, CmdList);
}

void FRHICommandListBase::QueueAsyncPipelineStateCompile(FGraphEventRef& AsyncCompileCompletionEvent)
{
	if (AsyncCompileCompletionEvent.GetReference())
	{
		RTTasks.AddUnique(AsyncCompileCompletionEvent);
	}
}

struct FRHICommandSubmitSubList : public FRHICommand<FRHICommandSubmitSubList>
{
	FRHICommandList* RHICmdList;
	FORCEINLINE_DEBUGGABLE FRHICommandSubmitSubList(FRHICommandList* InRHICmdList)
		: RHICmdList(InRHICmdList)
	{
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		INC_DWORD_STAT_BY(STAT_ChainLinkCount, 1);
		SCOPE_CYCLE_COUNTER(STAT_ChainExecute);
		RHICmdList->CopyContext(CmdList);
		delete RHICmdList;
	}
};

void FRHICommandListBase::QueueCommandListSubmit(class FRHICommandList* CmdList)
{
	new (AllocCommand<FRHICommandSubmitSubList>()) FRHICommandSubmitSubList(CmdList);
}


void FRHICommandList::BeginScene()
{
	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		CMD_CONTEXT(RHIBeginScene)();
		return;
	}
	new (AllocCommand<FRHICommandBeginScene>()) FRHICommandBeginScene();
	if (!IsRunningRHIInSeparateThread())
	{
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		QUICK_SCOPE_CYCLE_COUNTER(BeginScene_Flush);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
}
void FRHICommandList::EndScene()
{
	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		CMD_CONTEXT(RHIEndScene)();
		return;
	}
	new (AllocCommand<FRHICommandEndScene>()) FRHICommandEndScene();
	if (!IsRunningRHIInSeparateThread())
	{
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		QUICK_SCOPE_CYCLE_COUNTER(EndScene_Flush);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
}


void FRHICommandList::BeginDrawingViewport(FViewportRHIParamRef Viewport, FTextureRHIParamRef RenderTargetRHI)
{
	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		CMD_CONTEXT(RHIBeginDrawingViewport)(Viewport, RenderTargetRHI);
		return;
	}
	new (AllocCommand<FRHICommandBeginDrawingViewport>()) FRHICommandBeginDrawingViewport(Viewport, RenderTargetRHI);
	if (!IsRunningRHIInSeparateThread())
	{
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		QUICK_SCOPE_CYCLE_COUNTER(BeginDrawingViewport_Flush);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
}

void FRHICommandList::EndDrawingViewport(FViewportRHIParamRef Viewport, bool bPresent, bool bLockToVsync)
{
	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		CMD_CONTEXT(RHIEndDrawingViewport)(Viewport, bPresent, bLockToVsync);
	}
	else
	{
		new (AllocCommand<FRHICommandEndDrawingViewport>()) FRHICommandEndDrawingViewport(Viewport, bPresent, bLockToVsync);

		if (IsRunningRHIInSeparateThread())
		{
			// Insert a fence to prevent the renderthread getting more than a frame ahead of the RHIThread
			GRHIThreadEndDrawingViewportFences[GRHIThreadEndDrawingViewportFenceIndex] = static_cast<FRHICommandListImmediate*>(this)->RHIThreadFence();
		}
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_EndDrawingViewport_Dispatch);
			FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
		}
	}

	if (IsRunningRHIInSeparateThread())
	{
		// Wait on the previous frame's RHI thread fence (we never want the rendering thread to get more than a frame ahead)
		uint32 PreviousFrameFenceIndex = 1 - GRHIThreadEndDrawingViewportFenceIndex;
		FGraphEventRef& LastFrameFence = GRHIThreadEndDrawingViewportFences[PreviousFrameFenceIndex];
		FRHICommandListExecutor::WaitOnRHIThreadFence(LastFrameFence);
		GRHIThreadEndDrawingViewportFences[PreviousFrameFenceIndex] = nullptr;
		GRHIThreadEndDrawingViewportFenceIndex = PreviousFrameFenceIndex;
	}

	RHIAdvanceFrameForGetViewportBackBuffer(Viewport);
}

void FRHICommandList::BeginFrame()
{
	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		CMD_CONTEXT(RHIBeginFrame)();
		return;
	}
	new (AllocCommand<FRHICommandBeginFrame>()) FRHICommandBeginFrame();
	if (!IsRunningRHIInSeparateThread())
	{
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		QUICK_SCOPE_CYCLE_COUNTER(BeginFrame_Flush);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
}

void FRHICommandList::EndFrame()
{
	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		CMD_CONTEXT(RHIEndFrame)();
		return;
	}
	new (AllocCommand<FRHICommandEndFrame>()) FRHICommandEndFrame();
	if (!IsRunningRHIInSeparateThread())
	{
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		QUICK_SCOPE_CYCLE_COUNTER(EndFrame_Flush);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
}

DECLARE_CYCLE_STAT(TEXT("Explicit wait for tasks"), STAT_ExplicitWait, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Prewait dispatch"), STAT_PrewaitDispatch, STATGROUP_RHICMDLIST);
void FRHICommandListBase::WaitForTasks(bool bKnownToBeComplete)
{
	check(IsImmediate() && IsInRenderingThread());
	if (WaitOutstandingTasks.Num())
	{
		bool bAny = false;
		for (int32 Index = 0; Index < WaitOutstandingTasks.Num(); Index++)
		{
			if (!WaitOutstandingTasks[Index]->IsComplete())
			{
				ensure(!bKnownToBeComplete);
				bAny = true;
				break;
			}
		}
		if (bAny)
		{
			SCOPE_CYCLE_COUNTER(STAT_ExplicitWait);
			check(!FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local));
			FTaskGraphInterface::Get().WaitUntilTasksComplete(WaitOutstandingTasks, ENamedThreads::RenderThread_Local);
		}
		WaitOutstandingTasks.Reset();
	}
}

FScopedCommandListWaitForTasks::~FScopedCommandListWaitForTasks()
{
	check(IsInRenderingThread());
	if (bWaitForTasks)
	{
		if (IsRunningRHIInSeparateThread())
		{
#if 0
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FScopedCommandListWaitForTasks_Dispatch);
				RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
			}
#endif
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FScopedCommandListWaitForTasks_WaitAsync);
				RHICmdList.ImmediateFlush(EImmediateFlushType::WaitForOutstandingTasksOnly);
			}
		}
		else
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FScopedCommandListWaitForTasks_Flush);
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("Explicit wait for dispatch"), STAT_ExplicitWaitDispatch, STATGROUP_RHICMDLIST);
void FRHICommandListBase::WaitForDispatch()
{
	check(IsImmediate() && IsInRenderingThread());
	check(!AllOutstandingTasks.Num()); // dispatch before you get here
	if (RenderThreadSublistDispatchTask.GetReference() && RenderThreadSublistDispatchTask->IsComplete())
	{
		RenderThreadSublistDispatchTask = nullptr;
	}
	while (RenderThreadSublistDispatchTask.GetReference())
	{
		SCOPE_CYCLE_COUNTER(STAT_ExplicitWaitDispatch);
		if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
		{
			// this is a deadlock. RT tasks must be done by now or they won't be done. We could add a third queue...
			UE_LOG(LogRHI, Fatal, TEXT("Deadlock in FRHICommandListBase::WaitForDispatch."));
		}
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(RenderThreadSublistDispatchTask, ENamedThreads::RenderThread_Local);
		if (RenderThreadSublistDispatchTask.GetReference() && RenderThreadSublistDispatchTask->IsComplete())
		{
			RenderThreadSublistDispatchTask = nullptr;
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("Explicit wait for RHI thread"), STAT_ExplicitWaitRHIThread, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Explicit wait for RHI thread async dispatch"), STAT_ExplicitWaitRHIThread_Dispatch, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Deep spin for stray resource init"), STAT_SpinWaitRHIThread, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Spin RHIThread wait for stall"), STAT_SpinWaitRHIThreadStall, STATGROUP_RHICMDLIST);


int32 StallCount = 0;
bool FRHICommandListImmediate::IsStalled()
{
	return StallCount > 0;
}

bool FRHICommandListImmediate::StallRHIThread()
{
	check(IsInRenderingThread() && IsRunningRHIInSeparateThread());
	bool bAsyncSubmit = CVarRHICmdAsyncRHIThreadDispatch.GetValueOnRenderThread() > 0;
	if (bAsyncSubmit)
	{
		if (RenderThreadSublistDispatchTask.GetReference() && RenderThreadSublistDispatchTask->IsComplete())
		{
			RenderThreadSublistDispatchTask = nullptr;
		}
		if (!RenderThreadSublistDispatchTask.GetReference())
		{
			if (RHIThreadTask.GetReference() && RHIThreadTask->IsComplete())
			{
				RHIThreadTask = nullptr;
			}
			if (!RHIThreadTask.GetReference())
			{
				return false;
			}
		}
		FPlatformAtomics::InterlockedIncrement(&StallCount);
		{
		SCOPE_CYCLE_COUNTER(STAT_SpinWaitRHIThreadStall);
			GRHIThreadOnTasksCritical.Lock();
		}
		return true;
	}
	else
	{
		WaitForRHIThreadTasks();
		return false;
	}
}

void FRHICommandListImmediate::UnStallRHIThread()
{
	check(IsInRenderingThread() && IsRunningRHIInSeparateThread());
	GRHIThreadOnTasksCritical.Unlock();
	FPlatformAtomics::InterlockedDecrement(&StallCount);
}

void FRHICommandListBase::WaitForRHIThreadTasks()
{
	check(IsImmediate() && IsInRenderingThread());
	bool bAsyncSubmit = CVarRHICmdAsyncRHIThreadDispatch.GetValueOnRenderThread() > 0;
	if (bAsyncSubmit)
	{
		if (RenderThreadSublistDispatchTask.GetReference() && RenderThreadSublistDispatchTask->IsComplete())
		{
			RenderThreadSublistDispatchTask = nullptr;
		}
		while (RenderThreadSublistDispatchTask.GetReference())
		{
			SCOPE_CYCLE_COUNTER(STAT_ExplicitWaitRHIThread_Dispatch);
			if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
			{
				// we have to spin here because all task threads might be stalled, meaning the fire event anythread task might not be hit.
				// todo, add a third queue
				SCOPE_CYCLE_COUNTER(STAT_SpinWaitRHIThread);
				while (!RenderThreadSublistDispatchTask->IsComplete())
				{
					FPlatformProcess::SleepNoStats(0);
				}
			}
			else
			{
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(RenderThreadSublistDispatchTask, ENamedThreads::RenderThread_Local);
			}
			if (RenderThreadSublistDispatchTask.GetReference() && RenderThreadSublistDispatchTask->IsComplete())
			{
				RenderThreadSublistDispatchTask = nullptr;
			}
		}
		// now we can safely look at RHIThreadTask
	}
	if (RHIThreadTask.GetReference() && RHIThreadTask->IsComplete())
	{
		RHIThreadTask = nullptr;
	}
	while (RHIThreadTask.GetReference())
	{
		SCOPE_CYCLE_COUNTER(STAT_ExplicitWaitRHIThread);
		if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
		{
			// we have to spin here because all task threads might be stalled, meaning the fire event anythread task might not be hit.
			// todo, add a third queue
			SCOPE_CYCLE_COUNTER(STAT_SpinWaitRHIThread);
			while (!RHIThreadTask->IsComplete())
			{
				FPlatformProcess::SleepNoStats(0);
			}
		}
		else
		{
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(RHIThreadTask, ENamedThreads::RenderThread_Local);
		}
		if (RHIThreadTask.GetReference() && RHIThreadTask->IsComplete())
		{
			RHIThreadTask = nullptr;
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("RTTask completion join"), STAT_HandleRTThreadTaskCompletion_Join, STATGROUP_RHICMDLIST);
void FRHICommandListBase::HandleRTThreadTaskCompletion(const FGraphEventRef& MyCompletionGraphEvent)
{
	check(!IsImmediate() && !IsInRHIThread());
	for (int32 Index = 0; Index < RTTasks.Num(); Index++)
	{
		if (!RTTasks[Index]->IsComplete())
		{
			MyCompletionGraphEvent->DontCompleteUntil(RTTasks[Index]);
		}
	}
	RTTasks.Empty();
}

void* FRHIRenderPassCommandList::operator new(size_t Size)
{
	static_assert(sizeof(FRHIRenderPassCommandList) == sizeof(FRHICommandList), "Must match!");
	static_assert(sizeof(FRHIParallelRenderPassCommandList) == sizeof(FRHICommandList), "Must match!");
	return FMemory::Malloc(Size);
}

void FRHIRenderPassCommandList::operator delete(void *RawMemory)
{
	FMemory::Free(RawMemory);
}

void* FRHIRenderSubPassCommandList::operator new(size_t Size)
{
	return FMemory::Malloc(Size);
}

void FRHIRenderSubPassCommandList::operator delete(void *RawMemory)
{
	FMemory::Free(RawMemory);
}

void* FRHIParallelRenderPassCommandList::operator new(size_t Size)
{
	return FMemory::Malloc(Size);
}

void FRHIParallelRenderPassCommandList::operator delete(void *RawMemory)
{
	FMemory::Free(RawMemory);
}

void* FRHICommandList::operator new(size_t Size)
{
	return FMemory::Malloc(Size);
}

void FRHICommandList::operator delete(void *RawMemory)
{
	FMemory::Free(RawMemory);
}

void* FRHIAsyncComputeCommandList::operator new(size_t Size)
{
	return FMemory::Malloc(Size);
}

void FRHIAsyncComputeCommandList::operator delete(void *RawMemory)
{
	FMemory::Free(RawMemory);
}

void* FRHICommandListBase::operator new(size_t Size)
{
	check(0); // you shouldn't be creating these
	return FMemory::Malloc(Size);
}

void FRHICommandListBase::operator delete(void *RawMemory)
{
	FMemory::Free(RawMemory);
}	

///////// Pass through functions that allow RHIs to optimize certain calls.

FVertexBufferRHIRef FDynamicRHI::CreateAndLockVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
{
	FVertexBufferRHIRef VertexBuffer = CreateVertexBuffer_RenderThread(RHICmdList, Size, InUsage, CreateInfo);
	OutDataBuffer = LockVertexBuffer_RenderThread(RHICmdList, VertexBuffer, 0, Size, RLM_WriteOnly);

	return VertexBuffer;
}

FIndexBufferRHIRef FDynamicRHI::CreateAndLockIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
{
	FIndexBufferRHIRef IndexBuffer = CreateIndexBuffer_RenderThread(RHICmdList, Stride, Size, InUsage, CreateInfo);
	OutDataBuffer = LockIndexBuffer_RenderThread(RHICmdList, IndexBuffer, 0, Size, RLM_WriteOnly);
	return IndexBuffer;
}


FVertexBufferRHIRef FDynamicRHI::CreateVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateVertexBuffer(Size, InUsage, CreateInfo);
}

FStructuredBufferRHIRef FDynamicRHI::CreateStructuredBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateStructuredBuffer(Stride, Size, InUsage, CreateInfo);
}

FIndexBufferRHIRef FDynamicRHI::CreateIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateIndexBuffer(Stride, Size, InUsage, CreateInfo);
}

FShaderResourceViewRHIRef FDynamicRHI::CreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateShaderResourceView(VertexBuffer, Stride, Format);
}

FShaderResourceViewRHIRef FDynamicRHI::CreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef Buffer)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateShaderResourceView(Buffer);
}

struct FRHICommandUpdateVertexBuffer : public FRHICommand<FRHICommandUpdateVertexBuffer>
{
	FVertexBufferRHIParamRef VertexBuffer;
	void* Buffer;
	uint32 BufferSize;
	uint32 Offset;

	FORCEINLINE_DEBUGGABLE FRHICommandUpdateVertexBuffer(FVertexBufferRHIParamRef InVertexBuffer, void* InBuffer, uint32 InOffset, uint32 InBufferSize)
		: VertexBuffer(InVertexBuffer)
		, Buffer(InBuffer)
		, BufferSize(InBufferSize)
		, Offset(InOffset)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandUpdateVertexBuffer_Execute);
		void* Data = GDynamicRHI->RHILockVertexBuffer(VertexBuffer, Offset, BufferSize, RLM_WriteOnly);
		FMemory::Memcpy(Data, Buffer, BufferSize);
		FMemory::Free(Buffer);
		GDynamicRHI->RHIUnlockVertexBuffer(VertexBuffer);
	}
};

struct FRHICommandUpdateIndexBuffer : public FRHICommand<FRHICommandUpdateIndexBuffer>
{
	FIndexBufferRHIParamRef IndexBuffer;
	void* Buffer;
	uint32 BufferSize;
	uint32 Offset;

	FORCEINLINE_DEBUGGABLE FRHICommandUpdateIndexBuffer(FIndexBufferRHIParamRef InIndexBuffer, void* InBuffer, uint32 InOffset, uint32 InBufferSize)
		: IndexBuffer(InIndexBuffer)
		, Buffer(InBuffer)
		, BufferSize(InBufferSize)
		, Offset(InOffset)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandUpdateIndexBuffer_Execute);
		void* Data = GDynamicRHI->RHILockIndexBuffer(IndexBuffer, Offset, BufferSize, RLM_WriteOnly);
		FMemory::Memcpy(Data, Buffer, BufferSize);
		FMemory::Free(Buffer);
		GDynamicRHI->RHIUnlockIndexBuffer(IndexBuffer);
	}
};

static struct FLockTracker
{
	struct FLockParams
	{
		void* RHIBuffer;
		void* Buffer;
		uint32 BufferSize;
		uint32 Offset;
		EResourceLockMode LockMode;

		FORCEINLINE_DEBUGGABLE FLockParams(void* InRHIBuffer, void* InBuffer, uint32 InOffset, uint32 InBufferSize, EResourceLockMode InLockMode)
			: RHIBuffer(InRHIBuffer)
			, Buffer(InBuffer)
			, BufferSize(InBufferSize)
			, Offset(InOffset)
			, LockMode(InLockMode)
		{
		}
	};
	TArray<FLockParams, TInlineAllocator<16> > OutstandingLocks;
	uint32 TotalMemoryOutstanding;

	FLockTracker()
	{
		TotalMemoryOutstanding = 0;
	}

	FORCEINLINE_DEBUGGABLE void Lock(void* RHIBuffer, void* Buffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
	{
#if DO_CHECK
		for (auto& Parms : OutstandingLocks)
		{
			check(Parms.RHIBuffer != RHIBuffer);
		}
#endif
		OutstandingLocks.Add(FLockParams(RHIBuffer, Buffer, Offset, SizeRHI, LockMode));
		TotalMemoryOutstanding += SizeRHI;
	}
	FORCEINLINE_DEBUGGABLE FLockParams Unlock(void* RHIBuffer)
	{
		for (int32 Index = 0; Index < OutstandingLocks.Num(); Index++)
		{
			if (OutstandingLocks[Index].RHIBuffer == RHIBuffer)
			{
				FLockParams Result = OutstandingLocks[Index];
				OutstandingLocks.RemoveAtSwap(Index, 1, false);
				return Result;
			}
		}
		check(!"Mismatched RHI buffer locks.");
		return FLockParams(nullptr, nullptr, 0, 0, RLM_WriteOnly);
	}
} GLockTracker;


void* FDynamicRHI::LockVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicRHI_LockVertexBuffer_RenderThread);
	check(IsInRenderingThread());
	bool bBuffer = CVarRHICmdBufferWriteLocks.GetValueOnRenderThread() > 0;
	void* Result;
	if (!bBuffer || LockMode != RLM_WriteOnly || RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockVertexBuffer_Flush);
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		Result = GDynamicRHI->RHILockVertexBuffer(VertexBuffer, Offset, SizeRHI, LockMode);
	}
	else
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockVertexBuffer_Malloc);
		Result = FMemory::Malloc(SizeRHI, 16);
	}
	check(Result);
	GLockTracker.Lock(VertexBuffer, Result, Offset, SizeRHI, LockMode);
	return Result;
}

void FDynamicRHI::UnlockVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicRHI_UnlockVertexBuffer_RenderThread);
	check(IsInRenderingThread());
	bool bBuffer = CVarRHICmdBufferWriteLocks.GetValueOnRenderThread() > 0;
	FLockTracker::FLockParams Params = GLockTracker.Unlock(VertexBuffer);
	if (!bBuffer || Params.LockMode != RLM_WriteOnly || RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockVertexBuffer_Flush);
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		GDynamicRHI->RHIUnlockVertexBuffer(VertexBuffer);
		GLockTracker.TotalMemoryOutstanding = 0;
	}	
	else
	{
		new (RHICmdList.AllocCommand<FRHICommandUpdateVertexBuffer>()) FRHICommandUpdateVertexBuffer(VertexBuffer, Params.Buffer, Params.Offset, Params.BufferSize);
		RHICmdList.RHIThreadFence(true);
		if (GLockTracker.TotalMemoryOutstanding > 256 * 1024)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockVertexBuffer_FlushForMem);
			// we could be loading a level or something, lets get this stuff going
			RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); 
			GLockTracker.TotalMemoryOutstanding = 0;
		}
	}
}

void* FDynamicRHI::LockIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef IndexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicRHI_LockIndexBuffer_RenderThread);
	check(IsInRenderingThread());
	bool bBuffer = CVarRHICmdBufferWriteLocks.GetValueOnRenderThread() > 0;
	void* Result;
	if (!bBuffer || LockMode != RLM_WriteOnly || RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockIndexBuffer_Flush);
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		Result = GDynamicRHI->RHILockIndexBuffer(IndexBuffer, Offset, SizeRHI, LockMode);
	}
	else
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockIndexBuffer_Malloc);
		Result = FMemory::Malloc(SizeRHI, 16);
	}
	check(Result);
	GLockTracker.Lock(IndexBuffer, Result, Offset, SizeRHI, LockMode);
	return Result;
}

void FDynamicRHI::UnlockIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef IndexBuffer)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicRHI_UnlockIndexBuffer_RenderThread);
	check(IsInRenderingThread());
	bool bBuffer = CVarRHICmdBufferWriteLocks.GetValueOnRenderThread() > 0;
	FLockTracker::FLockParams Params = GLockTracker.Unlock(IndexBuffer);
	if (!bBuffer || Params.LockMode != RLM_WriteOnly || RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockIndexBuffer_Flush);
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		GDynamicRHI->RHIUnlockIndexBuffer(IndexBuffer);
		GLockTracker.TotalMemoryOutstanding = 0;
	}	
	else
	{
		new (RHICmdList.AllocCommand<FRHICommandUpdateIndexBuffer>()) FRHICommandUpdateIndexBuffer(IndexBuffer, Params.Buffer, Params.Offset, Params.BufferSize);
		RHICmdList.RHIThreadFence(true);
		if (GLockTracker.TotalMemoryOutstanding > 256 * 1024)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockIndexBuffer_FlushForMem);
			// we could be loading a level or something, lets get this stuff going
			RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); 
			GLockTracker.TotalMemoryOutstanding = 0;
		}
	}
}

FTexture2DRHIRef FDynamicRHI::AsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_AsyncReallocateTexture2D_Flush);
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
	return GDynamicRHI->RHIAsyncReallocateTexture2D(Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
}

ETextureReallocationStatus FDynamicRHI::FinalizeAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHIFinalizeAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

ETextureReallocationStatus FDynamicRHI::CancelAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICancelAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

FVertexDeclarationRHIRef FDynamicRHI::CreateVertexDeclaration_RenderThread(class FRHICommandListImmediate& RHICmdList, const FVertexDeclarationElementList& Elements)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateVertexDeclaration(Elements);
}

FVertexShaderRHIRef FDynamicRHI::CreateVertexShader_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateVertexShader(Code);
}

FVertexShaderRHIRef FDynamicRHI::CreateVertexShader_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateVertexShader(Library, Hash);
}

FPixelShaderRHIRef FDynamicRHI::CreatePixelShader_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreatePixelShader(Code);
}

FPixelShaderRHIRef FDynamicRHI::CreatePixelShader_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreatePixelShader(Library, Hash);
}

FGeometryShaderRHIRef FDynamicRHI::CreateGeometryShader_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateGeometryShader(Code);
}

FGeometryShaderRHIRef FDynamicRHI::CreateGeometryShader_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateGeometryShader(Library, Hash);
}

FGeometryShaderRHIRef FDynamicRHI::CreateGeometryShaderWithStreamOutput_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateGeometryShaderWithStreamOutput(Code, ElementList, NumStrides, Strides, RasterizedStream);
}

FGeometryShaderRHIRef FDynamicRHI::CreateGeometryShaderWithStreamOutput_RenderThread(class FRHICommandListImmediate& RHICmdList, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateGeometryShaderWithStreamOutput(ElementList, NumStrides, Strides, RasterizedStream, Library, Hash);
}

FComputeShaderRHIRef FDynamicRHI::CreateComputeShader_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateComputeShader(Code);
}

FComputeShaderRHIRef FDynamicRHI::CreateComputeShader_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateComputeShader(Library, Hash);
}

FHullShaderRHIRef FDynamicRHI::CreateHullShader_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateHullShader(Code);
}

FHullShaderRHIRef FDynamicRHI::CreateHullShader_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateHullShader(Library, Hash);
}

FDomainShaderRHIRef FDynamicRHI::CreateDomainShader_RenderThread(class FRHICommandListImmediate& RHICmdList, const TArray<uint8>& Code)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateDomainShader(Code);
}

FDomainShaderRHIRef FDynamicRHI::CreateDomainShader_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateDomainShader(Library, Hash);
}

void FDynamicRHI::UpdateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHIUpdateTexture2D(Texture, MipIndex, UpdateRegion, SourcePitch, SourceData);
}

FUpdateTexture3DData FDynamicRHI::BeginUpdateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture3DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion)
{
	check(IsInRenderingThread());

	const int32 FormatSize = PixelFormatBlockBytes[Texture->GetFormat()];
	const int32 RowPitch = UpdateRegion.Width * FormatSize;
	const int32 DepthPitch = UpdateRegion.Width * UpdateRegion.Height * FormatSize;

	SIZE_T MemorySize = DepthPitch * UpdateRegion.Depth;
	uint8* Data = (uint8*)FMemory::Malloc(MemorySize);	

	return FUpdateTexture3DData(Texture, MipIndex, UpdateRegion, RowPitch, DepthPitch, Data, MemorySize, GFrameNumberRenderThread);
}

void FDynamicRHI::EndUpdateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, FUpdateTexture3DData& UpdateData)
{
	check(IsInRenderingThread());
	check(GFrameNumberRenderThread == UpdateData.FrameNumber); 
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);	
	GDynamicRHI->RHIUpdateTexture3D(UpdateData.Texture, UpdateData.MipIndex, UpdateData.UpdateRegion, UpdateData.RowPitch, UpdateData.DepthPitch, UpdateData.Data);
	FMemory::Free(UpdateData.Data);
	UpdateData.Data = nullptr;
}

void FDynamicRHI::UpdateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture3DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	GDynamicRHI->RHIUpdateTexture3D(Texture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData);
}

void* FDynamicRHI::LockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush)
{
	if (bNeedsDefaultRHIFlush) 
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockTexture2D_Flush);
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		return GDynamicRHI->RHILockTexture2D(Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail);
	}
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHILockTexture2D(Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail);
}

void FDynamicRHI::UnlockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush)
{
	if (bNeedsDefaultRHIFlush)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockTexture2D_Flush);
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		GDynamicRHI->RHIUnlockTexture2D(Texture, MipIndex, bLockWithinMiptail);
		return;
	}
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	GDynamicRHI->RHIUnlockTexture2D(Texture, MipIndex, bLockWithinMiptail);
}

FTexture2DRHIRef FDynamicRHI::RHICreateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateTexture2D(SizeX, SizeY, Format, NumMips, NumSamples, Flags, CreateInfo);
}

FTexture2DRHIRef FDynamicRHI::RHICreateTextureExternal2D_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateTextureExternal2D(SizeX, SizeY, Format, NumMips, NumSamples, Flags, CreateInfo);
}

FTexture2DArrayRHIRef FDynamicRHI::RHICreateTexture2DArray_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateTexture2DArray(SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
}

FTexture3DRHIRef FDynamicRHI::RHICreateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateTexture3D(SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
}

FUnorderedAccessViewRHIRef FDynamicRHI::RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FStructuredBufferRHIParamRef StructuredBuffer, bool bUseUAVCounter, bool bAppendBuffer)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateUnorderedAccessView(StructuredBuffer, bUseUAVCounter, bAppendBuffer);
}

FUnorderedAccessViewRHIRef FDynamicRHI::RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTextureRHIParamRef Texture, uint32 MipLevel)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateUnorderedAccessView(Texture, MipLevel);
}

FUnorderedAccessViewRHIRef FDynamicRHI::RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint8 Format)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateUnorderedAccessView(VertexBuffer, Format);
}

FShaderResourceViewRHIRef FDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateShaderResourceView(Texture2DRHI, MipLevel);
}

FShaderResourceViewRHIRef FDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateShaderResourceView(Texture2DRHI, MipLevel, NumMipLevels, Format);
}

FShaderResourceViewRHIRef FDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateShaderResourceView(Texture3DRHI, MipLevel);
}

FShaderResourceViewRHIRef FDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateShaderResourceView(Texture2DArrayRHI, MipLevel);
}

FShaderResourceViewRHIRef FDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateShaderResourceView(TextureCubeRHI, MipLevel);
}

FShaderResourceViewRHIRef FDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateShaderResourceView(VertexBuffer, Stride, Format);
}

FShaderResourceViewRHIRef FDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef Buffer)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateShaderResourceView(Buffer);
}

FShaderResourceViewRHIRef FDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FStructuredBufferRHIParamRef StructuredBuffer)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateShaderResourceView(StructuredBuffer);
}

FTextureCubeRHIRef FDynamicRHI::RHICreateTextureCube_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateTextureCube(Size, Format, NumMips, Flags, CreateInfo);
}

FTextureCubeRHIRef FDynamicRHI::RHICreateTextureCubeArray_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateTextureCubeArray(Size, ArraySize, Format, NumMips, Flags, CreateInfo);
}

FRenderQueryRHIRef FDynamicRHI::RHICreateRenderQuery_RenderThread(class FRHICommandListImmediate& RHICmdList, ERenderQueryType QueryType)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateRenderQuery(QueryType);
}



void FRHICommandListImmediate::UpdateTextureReference(FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture)
{
	if (Bypass() || !IsRunningRHIInSeparateThread() || CVarRHICmdFlushUpdateTextureReference.GetValueOnRenderThread() > 0)
	{
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UpdateTextureReference_FlushRHI);
			ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		}
		CMD_CONTEXT(RHIUpdateTextureReference)(TextureRef, NewTexture);
		return;
	}
	new (AllocCommand<FRHICommandUpdateTextureReference>()) FRHICommandUpdateTextureReference(TextureRef, NewTexture);
	RHIThreadFence(true);
	if (GetUsedMemory() > 256 * 1024)
	{
		// we could be loading a level or something, lets get this stuff going
		ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); 
	}
}

void FDynamicRHI::RHICopySubTextureRegion_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef SourceTexture, FTexture2DRHIParamRef DestinationTexture, FBox2D SourceBox, FBox2D DestinationBox)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICopySubTextureRegion(SourceTexture, DestinationTexture, SourceBox, DestinationBox);
}

