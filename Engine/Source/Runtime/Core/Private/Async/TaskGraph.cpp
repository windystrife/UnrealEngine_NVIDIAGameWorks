// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/UnrealMemory.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"
#include "Misc/ScopedEvent.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/SingleThreadRunnable.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/NoopCounter.h"
#include "Misc/ScopeLock.h"
#include "Containers/LockFreeList.h"
#include "Templates/Function.h"
#include "Stats/Stats.h"
#include "Misc/CoreStats.h"
#include "Math/RandomStream.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Containers/LockFreeFixedSizeAllocator.h"
#include "Async/TaskGraphInterfaces.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/ThreadHeartBeat.h"

DEFINE_LOG_CATEGORY_STATIC(LogTaskGraph, Log, All);

DEFINE_STAT(STAT_FReturnGraphTask);
DEFINE_STAT(STAT_FTriggerEventGraphTask);
DEFINE_STAT(STAT_ParallelFor);
DEFINE_STAT(STAT_ParallelForTask);

static int32 GNumWorkerThreadsToIgnore = 0;

#if (PLATFORM_XBOXONE || PLATFORM_PS4 || PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX) && !IS_PROGRAM && WITH_ENGINE && !UE_SERVER
	#define CREATE_HIPRI_TASK_THREADS (1)
	#define CREATE_BACKGROUND_TASK_THREADS (1)
#else
	#define CREATE_HIPRI_TASK_THREADS (0)
	#define CREATE_BACKGROUND_TASK_THREADS (0)
#endif

namespace ENamedThreads
{
	CORE_API Type RenderThread = ENamedThreads::GameThread; // defaults to game and is set and reset by the render thread itself
	CORE_API Type RenderThread_Local = ENamedThreads::GameThread_Local; // defaults to game local and is set and reset by the render thread itself
	CORE_API int32 bHasBackgroundThreads = CREATE_BACKGROUND_TASK_THREADS;
	CORE_API int32 bHasHighPriorityThreads = CREATE_HIPRI_TASK_THREADS;
}

#if CREATE_HIPRI_TASK_THREADS || CREATE_BACKGROUND_TASK_THREADS
	static void ThreadSwitchForABTest(const TArray<FString>& Args)
	{
		if (Args.Num() == 2)
		{
#if CREATE_HIPRI_TASK_THREADS
			ENamedThreads::bHasHighPriorityThreads = !!FCString::Atoi(*Args[0]);
#endif
#if CREATE_BACKGROUND_TASK_THREADS
			ENamedThreads::bHasBackgroundThreads = !!FCString::Atoi(*Args[1]);
#endif
		}
		else
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("This command requires two arguments, both 0 or 1 to control the use of high priority and background priority threads, respectively."));
		}
		UE_LOG(LogConsoleResponse, Display, TEXT("High priority task threads: %d    Bacxkground priority threads: %d"), ENamedThreads::bHasHighPriorityThreads, ENamedThreads::bHasBackgroundThreads);
	}

	static FAutoConsoleCommand ThreadSwitchForABTestCommand(
		TEXT("TaskGraph.ABTestThreads"),
		TEXT("Takes two 0/1 arguments. Equivalent to setting TaskGraph.UseHiPriThreads and TaskGraph.UseBackgroundThreads, respectively. Packages as one command for use with the abtest command."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ThreadSwitchForABTest)
		);

#endif 


#if CREATE_BACKGROUND_TASK_THREADS
static FAutoConsoleVariableRef CVarUseBackgroundThreads(
	TEXT("TaskGraph.UseBackgroundThreads"),
	ENamedThreads::bHasBackgroundThreads,
	TEXT("If > 0, then use background threads, otherwise run background tasks on normal priority task threads. Used for performance tuning."),
	ECVF_Cheat
	);
#endif

#if CREATE_HIPRI_TASK_THREADS
static FAutoConsoleVariableRef CVarUseHiPriThreads(
	TEXT("TaskGraph.UseHiPriThreads"),
	ENamedThreads::bHasHighPriorityThreads,
	TEXT("If > 0, then use hi priority task threads, otherwise run background tasks on normal priority task threads. Used for performance tuning."),
	ECVF_Cheat
	);
#endif

#define PROFILE_TASKGRAPH (0)
#if PROFILE_TASKGRAPH
	struct FProfileRec
	{
		const TCHAR* Name;
		FThreadSafeCounter NumSamplesStarted;
		FThreadSafeCounter NumSamplesFinished;
		uint32 Samples[1000];

		FProfileRec()
		{
			Name = nullptr;
		}
	};
	static FThreadSafeCounter NumProfileSamples;
	static void DumpProfile();
	struct FProfileRecScope
	{
		FProfileRec* Target;
		int32 SampleIndex;
		uint32 StartCycles;
		FProfileRecScope(FProfileRec* InTarget, const TCHAR* InName)
			: Target(InTarget)
			, SampleIndex(InTarget->NumSamplesStarted.Increment() - 1)
			, StartCycles(FPlatformTime::Cycles())
		{
			if (SampleIndex == 0 && !Target->Name)
			{
				Target->Name = InName;
			}
		}
		~FProfileRecScope()
		{
			if (SampleIndex < 1000)
			{
				Target->Samples[SampleIndex] = FPlatformTime::Cycles() - StartCycles;
				if (Target->NumSamplesFinished.Increment() == 1000)
				{
					Target->NumSamplesFinished.Reset();
					FPlatformMisc::MemoryBarrier();
					uint64 Total = 0;
					for (int32 Index = 0; Index < 1000; Index++)
					{
						Total += Target->Samples[Index];
					}
					float MsPer = FPlatformTime::GetSecondsPerCycle() * double(Total);
					UE_LOG(LogTemp, Display, TEXT("%6.4f ms / scope %s"),MsPer, Target->Name);

					Target->NumSamplesStarted.Reset();
				}
			}
		}
	};
	static FProfileRec ProfileRecs[10];
	static void DumpProfile()
	{

	}

	#define TASKGRAPH_SCOPE_CYCLE_COUNTER(Index, Name) \
		FProfileRecScope ProfileRecScope##Index(&ProfileRecs[Index], TEXT(#Name));


#else
	#define TASKGRAPH_SCOPE_CYCLE_COUNTER(Index, Name)
#endif



/** 
 *	Pointer to the task graph implementation singleton.
 *	Because of the multithreaded nature of this system an ordinary singleton cannot be used.
 *	FTaskGraphImplementation::Startup() creates the singleton and the constructor actually sets this value.
**/
class FTaskGraphImplementation;
struct FWorkerThread;

static FTaskGraphImplementation* TaskGraphImplementationSingleton = NULL;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

static struct FChaosMode 
{
	enum 
	{
		NumSamples = 45771
	};
	FThreadSafeCounter Current;
	float DelayTimes[NumSamples + 1]; 
	int32 Enabled;

	FChaosMode()
		: Enabled(0)
	{
		FRandomStream Stream((int32)FPlatformTime::Cycles());
		for (int32 Index = 0; Index < NumSamples; Index++)
		{
			DelayTimes[Index] = Stream.GetFraction();
		}
		// ave = .5
		for (int32 Cube = 0; Cube < 2; Cube++)
		{
			for (int32 Index = 0; Index < NumSamples; Index++)
			{
				DelayTimes[Index] *= Stream.GetFraction();
			}
		}
		// ave = 1/8
		for (int32 Index = 0; Index < NumSamples; Index++)
		{
			DelayTimes[Index] *= 0.00001f;
		}
		// ave = 0.00000125s
		for (int32 Zeros = 0; Zeros < NumSamples / 20; Zeros++)
		{
			int32 Index = Stream.RandHelper(NumSamples);
			DelayTimes[Index] = 0.0f;
		}
		// 95% the samples are now zero
		for (int32 Zeros = 0; Zeros < NumSamples / 100; Zeros++)
		{
			int32 Index = Stream.RandHelper(NumSamples);
			DelayTimes[Index] = .00005f;
		}
		// .001% of the samples are 5ms
	}
	FORCEINLINE void Delay()
	{
		if (Enabled)
		{
			uint32 MyIndex = (uint32)Current.Increment();
			MyIndex %= NumSamples;
			float DelayS = DelayTimes[MyIndex];
			if (DelayS > 0.0f)
			{
				FPlatformProcess::Sleep(DelayS);
			}
		}
	}
} GChaosMode;

static void EnableRandomizedThreads(const TArray<FString>& Args)
{
	GChaosMode.Enabled = !GChaosMode.Enabled;
	if (GChaosMode.Enabled)
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("Random sleeps are enabled."));
	}
	else
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("Random sleeps are disabled."));
	}
}

static FAutoConsoleCommand TestRandomizedThreadsCommand(
	TEXT("TaskGraph.Randomize"),
	TEXT("Useful for debugging, adds random sleeps throughout the task graph."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&EnableRandomizedThreads)
	);

FORCEINLINE void TestRandomizedThreads()
{
	GChaosMode.Delay();
}

#else

FORCEINLINE void TestRandomizedThreads()
{
}

#endif

static FString ThreadPriorityToName(ENamedThreads::Type Priority)
{
	if (Priority == ENamedThreads::NormalThreadPriority)
	{
		return FString(TEXT("Normal"));
	}
	if (Priority == ENamedThreads::HighThreadPriority)
	{
		return FString(TEXT("High"));
	}
	if (Priority == ENamedThreads::BackgroundThreadPriority)
	{
		return FString(TEXT("Background"));
	}
	return FString(TEXT("??Unknown??"));
}

static FString TaskPriorityToName(ENamedThreads::Type Priority)
{
	if (Priority == ENamedThreads::NormalTaskPriority)
	{
		return FString(TEXT("Normal"));
	}
	if (Priority == ENamedThreads::HighTaskPriority)
	{
		return FString(TEXT("High"));
	}
	return FString(TEXT("??Unknown??"));
}

void FAutoConsoleTaskPriority::CommandExecute(const TArray<FString>& Args)
{
	if (Args.Num() > 0)
	{
		if (Args[0].Compare(ThreadPriorityToName(ENamedThreads::NormalThreadPriority), ESearchCase::IgnoreCase) == 0)
		{
			ThreadPriority = ENamedThreads::NormalThreadPriority;
		}
		else if (Args[0].Compare(ThreadPriorityToName(ENamedThreads::HighThreadPriority), ESearchCase::IgnoreCase) == 0)
		{
			ThreadPriority = ENamedThreads::HighThreadPriority;
		}
		else if (Args[0].Compare(ThreadPriorityToName(ENamedThreads::BackgroundThreadPriority), ESearchCase::IgnoreCase) == 0)
		{
			ThreadPriority = ENamedThreads::BackgroundThreadPriority;
		}
		else
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("Could not parse thread priority %s"), *Args[0]);
		}
	}
	if (Args.Num() > 1)
	{
		if (Args[1].Compare(TaskPriorityToName(ENamedThreads::NormalTaskPriority), ESearchCase::IgnoreCase) == 0)
		{
			TaskPriority = ENamedThreads::NormalTaskPriority;
		}
		else if (Args[1].Compare(TaskPriorityToName(ENamedThreads::HighTaskPriority), ESearchCase::IgnoreCase) == 0)
		{
			TaskPriority = ENamedThreads::HighTaskPriority;
		}
		else
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("Could not parse task priority %s"), *Args[1]);
		}
	}
	if (Args.Num() > 2)
	{
		if (Args[2].Compare(TaskPriorityToName(ENamedThreads::NormalTaskPriority), ESearchCase::IgnoreCase) == 0)
		{
			TaskPriorityIfForcedToNormalThreadPriority = ENamedThreads::NormalTaskPriority;
		}
		else if (Args[2].Compare(TaskPriorityToName(ENamedThreads::HighTaskPriority), ESearchCase::IgnoreCase) == 0)
		{
			TaskPriorityIfForcedToNormalThreadPriority = ENamedThreads::HighTaskPriority;
		}
		else
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("Could not parse task priority %s"), *Args[2]);
		}
	}
	if (ThreadPriority == ENamedThreads::NormalThreadPriority)
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("%s - thread priority:%s   task priority:%s"), *CommandName, *ThreadPriorityToName(ThreadPriority), *TaskPriorityToName(TaskPriority));
	}
	else
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("%s - thread priority:%s   task priority:%s  %s (when forced to normal)"), *CommandName, *ThreadPriorityToName(ThreadPriority), *TaskPriorityToName(TaskPriority), *TaskPriorityToName(this->TaskPriorityIfForcedToNormalThreadPriority));
	}
}



/** 
 *	FTaskThreadBase
 *	Base class for a thread that executes tasks
 *	This class implements the FRunnable API, but external threads don't use that because those threads are created elsewhere.
**/
class FTaskThreadBase : public FRunnable, FSingleThreadRunnable
{
public:
	// Calls meant to be called from a "main" or supervisor thread.

	/** Constructor, initializes everything to unusable values. Meant to be called from a "main" thread. **/
	FTaskThreadBase()
		: ThreadId(ENamedThreads::AnyThread)
		, PerThreadIDTLSSlot(0xffffffff)
		, OwnerWorker(nullptr)
	{
		NewTasks.Reset(128);
	}

	/** 
	 *	Sets up some basic information for a thread. Meant to be called from a "main" thread. Also creates the stall event.
	 *	@param InThreadId; Thread index for this thread.
	 *	@param InPerThreadIDTLSSlot; TLS slot to store the pointer to me into (later)
	 *	@param bInAllowsStealsFromMe; If true, this is a worker thread and any other thread can steal tasks from my incoming queue.
	 *	@param bInStealsFromOthers If true, this is a worker thread and I will attempt to steal tasks when I run out of work.
	**/
	void Setup(ENamedThreads::Type InThreadId, uint32 InPerThreadIDTLSSlot, FWorkerThread* InOwnerWorker)
	{
		ThreadId = InThreadId;
		check(ThreadId >= 0);
		PerThreadIDTLSSlot = InPerThreadIDTLSSlot;
		OwnerWorker = InOwnerWorker;
	}

	// Calls meant to be called from "this thread".

	/** A one-time call to set the TLS entry for this thread. **/
	void InitializeForCurrentThread()
	{
		FPlatformTLS::SetTlsValue(PerThreadIDTLSSlot,OwnerWorker);
	}

	/** Return the index of this thread. **/
	ENamedThreads::Type GetThreadId() const
	{
		checkThreadGraph(OwnerWorker); // make sure we are started up
		return ThreadId;
	}

	/** Used for named threads to start processing tasks until the thread is idle and RequestQuit has been called. **/
	virtual void ProcessTasksUntilQuit(int32 QueueIndex) = 0;

	/** Used for named threads to start processing tasks until the thread is idle and RequestQuit has been called. **/
	virtual void ProcessTasksUntilIdle(int32 QueueIndex)
	{
		check(0);
	}

	/** 
	 *	Queue a task, assuming that this thread is the same as the current thread.
	 *	For named threads, these go directly into the private queue.
	 *	@param QueueIndex, Queue to enqueue for
	 *	@param Task Task to queue.
	 **/
	virtual void EnqueueFromThisThread(int32 QueueIndex, FBaseGraphTask* Task)
	{
		check(0);
	}

	// Calls meant to be called from any thread.

	/** 
	 *	Will cause the thread to return to the caller when it becomes idle. Used to return from ProcessTasksUntilQuit for named threads or to shut down unnamed threads. 
	 *	CAUTION: This will not work under arbitrary circumstances. For example you should not attempt to stop unnamed threads unless they are known to be idle.
	 *	Return requests for named threads should be submitted from that named thread as FReturnGraphTask does.
	 *	@param QueueIndex, Queue to request quit from
	**/
	virtual void RequestQuit(int32 QueueIndex) = 0;

	/** 
	 *	Queue a task, assuming that this thread is not the same as the current thread.
	 *	@param QueueIndex, Queue to enqueue into
	 *	@param Task; Task to queue.
	 **/
	virtual bool EnqueueFromOtherThread(int32 QueueIndex, FBaseGraphTask* Task)
	{
		check(0);
		return false;
	}

	virtual void WakeUp()
	{
		check(0);
	}
	/** 
	 *Return true if this thread is processing tasks. This is only a "guess" if you ask for a thread other than yourself because that can change before the function returns.
	 *@param QueueIndex, Queue to request quit from
	 **/
	virtual bool IsProcessingTasks(int32 QueueIndex) = 0;

	// SingleThreaded API

	/** Tick single-threaded. */
	virtual void Tick() override
	{
		ProcessTasksUntilIdle(0);
	}


	// FRunnable API

	/**
	 * Allows per runnable object initialization. NOTE: This is called in the
	 * context of the thread object that aggregates this, not the thread that
	 * passes this runnable to a new thread.
	 *
	 * @return True if initialization was successful, false otherwise
	 */
	virtual bool Init() override
	{
		InitializeForCurrentThread();
		return true;
	}

	/**
	 * This is where all per object thread work is done. This is only called
	 * if the initialization was successful.
	 *
	 * @return The exit code of the runnable object
	 */
	virtual uint32 Run() override
	{
		check(OwnerWorker); // make sure we are started up
		ProcessTasksUntilQuit(0);
		FMemory::ClearAndDisableTLSCachesOnCurrentThread();
		return 0;
	}

	/**
	 * This is called if a thread is requested to terminate early
	 */
	virtual void Stop() override
	{
		RequestQuit(-1);
	}

	/**
	 * Called in the context of the aggregating thread to perform any cleanup.
	 */
	virtual void Exit() override
	{
	}

	/**
	 * Return single threaded interface when multithreading is disabled.
	 */
	virtual FSingleThreadRunnable* GetSingleThreadInterface() override
	{
		return this;
	}

protected:

	/** Id / Index of this thread. **/
	ENamedThreads::Type									ThreadId;
	/** TLS SLot that we store the FTaskThread* this pointer in. **/
	uint32												PerThreadIDTLSSlot;
	/** Used to signal stalling. Not safe for synchronization in most cases. **/
	FThreadSafeCounter									IsStalled;
	/** Array of tasks for this task thread. */
	TArray<FBaseGraphTask*> NewTasks;
	/** back pointer to the owning FWorkerThread **/
	FWorkerThread* OwnerWorker;
};

/** 
 *	FNamedTaskThread
 *	A class for managing a named thread. 
 */
class FNamedTaskThread : public FTaskThreadBase
{
public:

	virtual void ProcessTasksUntilQuit(int32 QueueIndex) override
	{
		check(Queue(QueueIndex).StallRestartEvent); // make sure we are started up

		Queue(QueueIndex).QuitForReturn = false;
		verify(++Queue(QueueIndex).RecursionGuard == 1);
		do
		{
			ProcessTasksNamedThread(QueueIndex, FPlatformProcess::SupportsMultithreading());
		} while (!Queue(QueueIndex).QuitForReturn && !Queue(QueueIndex).QuitForShutdown && FPlatformProcess::SupportsMultithreading()); // @Hack - quit now when running with only one thread.
		verify(!--Queue(QueueIndex).RecursionGuard);
	}

	virtual void ProcessTasksUntilIdle(int32 QueueIndex) override
	{
		check(Queue(QueueIndex).StallRestartEvent); // make sure we are started up

		Queue(QueueIndex).QuitForReturn = false;
		verify(++Queue(QueueIndex).RecursionGuard == 1);
		ProcessTasksNamedThread(QueueIndex, false);
		verify(!--Queue(QueueIndex).RecursionGuard);
	}


	void ProcessTasksNamedThread(int32 QueueIndex, bool bAllowStall)
	{
		TStatId StallStatId;
		bool bCountAsStall = false;
#if STATS
		TStatId StatName;
		FCycleCounter ProcessingTasks;
		if (ThreadId == ENamedThreads::GameThread)
		{
			StatName = GET_STATID(STAT_TaskGraph_GameTasks);
			StallStatId = GET_STATID(STAT_TaskGraph_GameStalls);
			bCountAsStall = true;
		}
		else if (ThreadId == ENamedThreads::RenderThread)
		{
			if (QueueIndex > 0)
			{
				StallStatId = GET_STATID(STAT_TaskGraph_RenderStalls);
				bCountAsStall = true;
			}
			// else StatName = none, we need to let the scope empty so that the render thread submits tasks in a timely manner. 
		}
		else if (ThreadId != ENamedThreads::StatsThread)
		{
			StatName = GET_STATID(STAT_TaskGraph_OtherTasks);
			StallStatId = GET_STATID(STAT_TaskGraph_OtherStalls);
			bCountAsStall = true;
		}
		bool bTasksOpen = false;
		if (FThreadStats::IsCollectingData(StatName))
		{
			bTasksOpen = true;
			ProcessingTasks.Start(StatName);
		}
#endif
		while (!Queue(QueueIndex).QuitForReturn)
		{
			FBaseGraphTask* Task = Queue(QueueIndex).StallQueue.Pop(0, bAllowStall);
			TestRandomizedThreads();
			if (!Task)
			{
#if STATS
				if (bTasksOpen)
				{
					ProcessingTasks.Stop();
					bTasksOpen = false;
				}
#endif
				if (bAllowStall)
				{
					{
						FScopeCycleCounter Scope(StallStatId);
						Queue(QueueIndex).StallRestartEvent->Wait(MAX_uint32, bCountAsStall);
						if (Queue(QueueIndex).QuitForShutdown)
						{
							return;
						}
						TestRandomizedThreads();
					}
#if STATS
					if (!bTasksOpen && FThreadStats::IsCollectingData(StatName))
					{
						bTasksOpen = true;
						ProcessingTasks.Start(StatName);
					}
#endif
					continue;
				}
				else
				{
					break; // we were asked to quit
				}
			}
			else
			{
				Task->Execute(NewTasks, ENamedThreads::Type(ThreadId | (QueueIndex << ENamedThreads::QueueIndexShift)));
				TestRandomizedThreads();
			}
		}
#if STATS
		if (bTasksOpen)
		{
			ProcessingTasks.Stop();
			bTasksOpen = false;
		}
#endif
	}
	virtual void EnqueueFromThisThread(int32 QueueIndex, FBaseGraphTask* Task) override
	{
		checkThreadGraph(Task && Queue(QueueIndex).StallRestartEvent); // make sure we are started up
		uint32 PriIndex = ENamedThreads::GetTaskPriority(Task->ThreadToExecuteOn) ? 0 : 1;
		int32 ThreadToStart = Queue(QueueIndex).StallQueue.Push(Task, PriIndex);
		check(ThreadToStart < 0); // if I am stalled, then how can I be queueing a task?
	}

	virtual void RequestQuit(int32 QueueIndex) override
	{
		// this will not work under arbitrary circumstances. For example you should not attempt to stop threads unless they are known to be idle.
		if (!Queue(0).StallRestartEvent)
		{
			return;
		}
		if (QueueIndex == -1)
		{
			// we are shutting down
			checkThreadGraph(Queue(0).StallRestartEvent); // make sure we are started up
			checkThreadGraph(Queue(1).StallRestartEvent); // make sure we are started up
			Queue(0).QuitForShutdown = true;
			Queue(1).QuitForShutdown = true;
			Queue(0).StallRestartEvent->Trigger();
			Queue(1).StallRestartEvent->Trigger();
		}
		else
		{
			checkThreadGraph(Queue(QueueIndex).StallRestartEvent); // make sure we are started up
			Queue(QueueIndex).QuitForReturn = true;
		}
	}

	virtual bool EnqueueFromOtherThread(int32 QueueIndex, FBaseGraphTask* Task) override
	{
		TestRandomizedThreads();
		checkThreadGraph(Task && Queue(QueueIndex).StallRestartEvent); // make sure we are started up

		uint32 PriIndex = ENamedThreads::GetTaskPriority(Task->ThreadToExecuteOn) ? 0 : 1;
		int32 ThreadToStart = Queue(QueueIndex).StallQueue.Push(Task, PriIndex);

		if (ThreadToStart >= 0)
		{
			checkThreadGraph(ThreadToStart == 0);
			TASKGRAPH_SCOPE_CYCLE_COUNTER(1, STAT_TaskGraph_EnqueueFromOtherThread_Trigger);
			Queue(QueueIndex).StallRestartEvent->Trigger();
			return true;
		}
		return false;
	}

	virtual bool IsProcessingTasks(int32 QueueIndex) override
	{
		return !!Queue(QueueIndex).RecursionGuard;
	}

private:

	/** Grouping of the data for an individual queue. **/
	struct FThreadTaskQueue
	{
		FStallingTaskQueue<FBaseGraphTask, PLATFORM_CACHE_LINE_SIZE, 2> StallQueue;

		/** We need to disallow reentry of the processing loop **/
		uint32 RecursionGuard;

		/** Indicates we executed a return task, so break out of the processing loop. **/
		bool QuitForReturn;

		/** Indicates we executed a return task, so break out of the processing loop. **/
		bool QuitForShutdown;

		/** Event that this thread blocks on when it runs out of work. **/
		FEvent*	StallRestartEvent;

		FThreadTaskQueue()
			: RecursionGuard(0)
			, QuitForReturn(false)
			, QuitForShutdown(false)
			, StallRestartEvent(FPlatformProcess::GetSynchEventFromPool(false))
		{

		}
		~FThreadTaskQueue()
		{
			FPlatformProcess::ReturnSynchEventToPool(StallRestartEvent);
			StallRestartEvent = nullptr;
		}
	};

	FORCEINLINE FThreadTaskQueue& Queue(int32 QueueIndex)
	{
		checkThreadGraph(QueueIndex >= 0 && QueueIndex < ENamedThreads::NumQueues);
		return Queues[QueueIndex];
	}
	FORCEINLINE const FThreadTaskQueue& Queue(int32 QueueIndex) const
	{
		checkThreadGraph(QueueIndex >= 0 && QueueIndex < ENamedThreads::NumQueues);
		return Queues[QueueIndex];
	}

	FThreadTaskQueue Queues[ENamedThreads::NumQueues];
};

/**
*	FTaskThreadAnyThread
*	A class for managing a worker threads.
**/
class FTaskThreadAnyThread : public FTaskThreadBase
{
public:
	FTaskThreadAnyThread(int32 InPriorityIndex)
		: PriorityIndex(InPriorityIndex)
	{
	}
	virtual void ProcessTasksUntilQuit(int32 QueueIndex) override
	{
		if (PriorityIndex != (ENamedThreads::BackgroundThreadPriority >> ENamedThreads::ThreadPriorityShift))
		{
			FMemory::SetupTLSCachesOnCurrentThread();
		}
		check(!QueueIndex);
		do
		{
			ProcessTasks();			
		} while (!Queue.QuitForShutdown && FPlatformProcess::SupportsMultithreading()); // @Hack - quit now when running with only one thread.
	}

	virtual void ProcessTasksUntilIdle(int32 QueueIndex) override
	{
		if (!FPlatformProcess::SupportsMultithreading())
		{
			ProcessTasks();
		}
		else
		{
			check(0);
		}
	}

	// Calls meant to be called from any thread.

	/**
	*	Will cause the thread to return to the caller when it becomes idle. Used to return from ProcessTasksUntilQuit for named threads or to shut down unnamed threads.
	*	CAUTION: This will not work under arbitrary circumstances. For example you should not attempt to stop unnamed threads unless they are known to be idle.
	*	Return requests for named threads should be submitted from that named thread as FReturnGraphTask does.
	*	@param QueueIndex, Queue to request quit from
	**/
	virtual void RequestQuit(int32 QueueIndex) override
	{
		check(QueueIndex < 1);

		// this will not work under arbitrary circumstances. For example you should not attempt to stop threads unless they are known to be idle.
		checkThreadGraph(Queue.StallRestartEvent); // make sure we are started up
		Queue.QuitForShutdown = true;
		Queue.StallRestartEvent->Trigger();
	}

	virtual void WakeUp() final override
	{
		TASKGRAPH_SCOPE_CYCLE_COUNTER(1, STAT_TaskGraph_Wakeup_Trigger);
		Queue.StallRestartEvent->Trigger();
	}

	void StallForTuning(bool Stall)
	{
		if (Stall)
		{
			Queue.StallForTuning.Lock();
			Queue.bStallForTuning = true;
		}
		else
		{
			Queue.bStallForTuning = false;
			Queue.StallForTuning.Unlock();
		}
	}
	/**
	*Return true if this thread is processing tasks. This is only a "guess" if you ask for a thread other than yourself because that can change before the function returns.
	*@param QueueIndex, Queue to request quit from
	**/
	virtual bool IsProcessingTasks(int32 QueueIndex) override
	{
		check(!QueueIndex);
		return !!Queue.RecursionGuard;
	}

private:

	/**
	*	Process tasks until idle. May block if bAllowStall is true
	*	@param QueueIndex, Queue to process tasks from
	*	@param bAllowStall,  if true, the thread will block on the stall event when it runs out of tasks.
	**/
	void ProcessTasks()
	{
		LLM_SCOPE(ELLMTag::TaskGraphTasksMisc);

		TStatId StallStatId;
		bool bCountAsStall = true;
#if STATS
		TStatId StatName;
		FCycleCounter ProcessingTasks;
		StatName = GET_STATID(STAT_TaskGraph_OtherTasks);
		StallStatId = GET_STATID(STAT_TaskGraph_OtherStalls);
		bool bTasksOpen = false;
		if (FThreadStats::IsCollectingData(StatName))
		{
			bTasksOpen = true;
			ProcessingTasks.Start(StatName);
		}
#endif
		verify(++Queue.RecursionGuard == 1);
		while (1)
		{
			FBaseGraphTask* Task = FindWork();
			if (!Task)
			{
#if STATS
				if (bTasksOpen)
				{
					ProcessingTasks.Stop();
					bTasksOpen = false;
				}
#endif

				TestRandomizedThreads();
				if (FPlatformProcess::SupportsMultithreading())
				{
					FScopeCycleCounter Scope(StallStatId);
					Queue.StallRestartEvent->Wait(MAX_uint32, bCountAsStall);
				}
				if (Queue.QuitForShutdown || !FPlatformProcess::SupportsMultithreading())
				{
					break;
				}
				TestRandomizedThreads();

#if STATS
				if (FThreadStats::IsCollectingData(StatName))
				{
					bTasksOpen = true;
					ProcessingTasks.Start(StatName);
				}
#endif
				continue;
			}
			TestRandomizedThreads();
			Task->Execute(NewTasks, ENamedThreads::Type(ThreadId));
			TestRandomizedThreads();
			if (Queue.bStallForTuning)
			{
#if STATS
				if (bTasksOpen)
				{
					ProcessingTasks.Stop();
					bTasksOpen = false;
				}
#endif
				{
					FScopeLock Lock(&Queue.StallForTuning);
				}
#if STATS
				if (FThreadStats::IsCollectingData(StatName))
				{
					bTasksOpen = true;
					ProcessingTasks.Start(StatName);
				}
#endif
			}
		}
		verify(!--Queue.RecursionGuard);
	}

	/** Grouping of the data for an individual queue. **/
	struct FThreadTaskQueue
	{
		/** Event that this thread blocks on when it runs out of work. **/
		FEvent* StallRestartEvent;
		/** We need to disallow reentry of the processing loop **/
		uint32 RecursionGuard;
		/** Indicates we executed a return task, so break out of the processing loop. **/
		bool QuitForShutdown;
		/** Should we stall for tuning? **/
		bool bStallForTuning;
		FCriticalSection StallForTuning;

		FThreadTaskQueue()
			: StallRestartEvent(FPlatformProcess::GetSynchEventFromPool(false))
			, RecursionGuard(0)
			, QuitForShutdown(false)
			, bStallForTuning(false)
		{

		}
		~FThreadTaskQueue()
		{
			FPlatformProcess::ReturnSynchEventToPool(StallRestartEvent);
			StallRestartEvent = nullptr;
		}
	};

	/**
	*	Internal function to call the system looking for work. Called from this thread.
	*	@return New task to process.
	*/
	FBaseGraphTask* FindWork();

	/** Array of queues, only the first one is used for unnamed threads. **/
	FThreadTaskQueue Queue;

	int32 PriorityIndex;
};


/** 
	*	FWorkerThread
	*	Helper structure to aggregate a few items related to the individual threads.
**/
struct FWorkerThread
{
	/** The actual FTaskThread that manager this task **/
	FTaskThreadBase*	TaskGraphWorker;
	/** For internal threads, the is non-NULL and holds the information about the runable thread that was created. **/
	FRunnableThread*	RunnableThread;
	/** For external threads, this determines if they have been "attached" yet. Attachment is mostly setting up TLS for this individual thread. **/
	bool				bAttached;

	/** Constructor to set reasonable defaults. **/
	FWorkerThread()
		: TaskGraphWorker(nullptr)
		, RunnableThread(nullptr)
		, bAttached(false)
	{
	}
};

/**
*	FTaskGraphImplementation
*	Implementation of the centralized part of the task graph system.
*	These parts of the system have no knowledge of the dependency graph, they exclusively work on tasks.
**/

class FTaskGraphImplementation : public FTaskGraphInterface
{
public:

	// API related to life cycle of the system and singletons

	/** 
	 *	Singleton returning the one and only FTaskGraphImplementation.
	 *	Note that unlike most singletons, a manual call to FTaskGraphInterface::Startup is required before the singleton will return a valid reference.
	**/
	static FTaskGraphImplementation& Get()
	{		
		checkThreadGraph(TaskGraphImplementationSingleton);
		return *TaskGraphImplementationSingleton;
	}

	/** 
	 *	Constructor - initializes the data structures, sets the singleton pointer and creates the internal threads.
	 *	@param InNumThreads; total number of threads in the system, including named threads, unnamed threads, internal threads and external threads. Must be at least 1 + the number of named threads.
	**/
	FTaskGraphImplementation(int32)
	{
		bCreatedHiPriorityThreads = !!ENamedThreads::bHasHighPriorityThreads;
		bCreatedBackgroundPriorityThreads = !!ENamedThreads::bHasBackgroundThreads;

		int32 MaxTaskThreads = MAX_THREADS;
		int32 NumTaskThreads = FPlatformMisc::NumberOfWorkerThreadsToSpawn();

		// if we don't want any performance-based threads, then force the task graph to not create any worker threads, and run in game thread
		if (!FPlatformProcess::SupportsMultithreading())
		{
			// this is the logic that used to be spread over a couple of places, that will make the rest of this function disable a worker thread
			// @todo: it could probably be made simpler/clearer
			// this - 1 tells the below code there is no rendering thread
			MaxTaskThreads = 1;
			NumTaskThreads = 1;
			LastExternalThread = (ENamedThreads::Type)(ENamedThreads::ActualRenderingThread - 1);
			bCreatedHiPriorityThreads = false;
			bCreatedBackgroundPriorityThreads = false;
			ENamedThreads::bHasBackgroundThreads = 0;
			ENamedThreads::bHasHighPriorityThreads = 0;
		}
		else
		{
			LastExternalThread = ENamedThreads::ActualRenderingThread;
		}
		
		NumNamedThreads = LastExternalThread + 1;

		NumTaskThreadSets = 1 + bCreatedHiPriorityThreads + bCreatedBackgroundPriorityThreads;

		// if we don't have enough threads to allow all of the sets asked for, then we can't create what was asked for.
		check(NumTaskThreadSets == 1 || FMath::Min<int32>(NumTaskThreads * NumTaskThreadSets + NumNamedThreads, MAX_THREADS) == NumTaskThreads * NumTaskThreadSets + NumNamedThreads);
		NumThreads = FMath::Max<int32>(FMath::Min<int32>(NumTaskThreads * NumTaskThreadSets + NumNamedThreads, MAX_THREADS), NumNamedThreads + 1);

		// Cap number of extra threads to the platform worker thread count
		// if we don't have enough threads to allow all of the sets asked for, then we can't create what was asked for.
		check(NumTaskThreadSets == 1 || FMath::Min(NumThreads, NumNamedThreads + NumTaskThreads * NumTaskThreadSets) == NumThreads);
		NumThreads = FMath::Min(NumThreads, NumNamedThreads + NumTaskThreads * NumTaskThreadSets);

		NumTaskThreadsPerSet = (NumThreads - NumNamedThreads) / NumTaskThreadSets;
		check((NumThreads - NumNamedThreads) % NumTaskThreadSets == 0); // should be equal numbers of threads per priority set

		UE_LOG(LogTaskGraph, Log, TEXT("Started task graph with %d named threads and %d total threads with %d sets of task threads."), NumNamedThreads, NumThreads, NumTaskThreadSets);
		check(NumThreads - NumNamedThreads >= 1);  // need at least one pure worker thread
		check(NumThreads <= MAX_THREADS);
		check(!ReentrancyCheck.GetValue()); // reentrant?
		ReentrancyCheck.Increment(); // just checking for reentrancy
		PerThreadIDTLSSlot = FPlatformTLS::AllocTlsSlot();

		for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ThreadIndex++)
		{
			check(!WorkerThreads[ThreadIndex].bAttached); // reentrant?
			bool bAnyTaskThread = ThreadIndex >= NumNamedThreads;
			if (bAnyTaskThread)
			{
				WorkerThreads[ThreadIndex].TaskGraphWorker = new FTaskThreadAnyThread(ThreadIndexToPriorityIndex(ThreadIndex));
			}
			else
			{
				WorkerThreads[ThreadIndex].TaskGraphWorker = new FNamedTaskThread;
			}
			WorkerThreads[ThreadIndex].TaskGraphWorker->Setup(ENamedThreads::Type(ThreadIndex), PerThreadIDTLSSlot, &WorkerThreads[ThreadIndex]);
		}

		TaskGraphImplementationSingleton = this; // now reentrancy is ok

		for (int32 ThreadIndex = LastExternalThread + 1; ThreadIndex < NumThreads; ThreadIndex++)
		{
			FString Name;
			int32 Priority = ThreadIndexToPriorityIndex(ThreadIndex);
			EThreadPriority ThreadPri;
			uint64 Affinity = FPlatformAffinity::GetTaskGraphThreadMask();
			if (Priority == 1)
			{
				Name = FString::Printf(TEXT("TaskGraphThreadHP %d"), ThreadIndex - (LastExternalThread + 1));
				ThreadPri = TPri_SlightlyBelowNormal; // we want even hi priority tasks below the normal threads
			}
			else if (Priority == 2)
			{
				Name = FString::Printf(TEXT("TaskGraphThreadBP %d"), ThreadIndex - (LastExternalThread + 1));
				ThreadPri = TPri_Lowest;
				if (PLATFORM_PS4)
				{
					// hack, use the audio affinity mask, since this might include the 7th core
					Affinity = FPlatformAffinity::GetTaskGraphBackgroundTaskMask();
				}
			}
			else
			{
				Name = FString::Printf(TEXT("TaskGraphThreadNP %d"), ThreadIndex - (LastExternalThread + 1));
				ThreadPri = TPri_BelowNormal; // we want normal tasks below normal threads like the game thread
			}

#if ( UE_BUILD_SHIPPING || UE_BUILD_TEST )
			uint32 StackSize = 384 * 1024;
#else
			uint32 StackSize = 512 * 1024;
#endif
			WorkerThreads[ThreadIndex].RunnableThread = FRunnableThread::Create(&Thread(ThreadIndex), *Name, StackSize, ThreadPri, Affinity); // these are below normal threads so that they sleep when the named threads are active
			WorkerThreads[ThreadIndex].bAttached = true;
		}
	}

	/** 
	 *	Destructor - probably only works reliably when the system is completely idle. The system has no idea if it is idle or not.
	**/
	virtual ~FTaskGraphImplementation()
	{
		for (auto& Callback : ShutdownCallbacks)
		{
			Callback();
		}
		ShutdownCallbacks.Empty();
		for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ThreadIndex++)
		{
			Thread(ThreadIndex).RequestQuit(-1);
		}
		for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ThreadIndex++)
		{
			if (ThreadIndex > LastExternalThread)
			{
				WorkerThreads[ThreadIndex].RunnableThread->WaitForCompletion();
				delete WorkerThreads[ThreadIndex].RunnableThread;
				WorkerThreads[ThreadIndex].RunnableThread = NULL;
			}
			WorkerThreads[ThreadIndex].bAttached = false;
		}
		TaskGraphImplementationSingleton = NULL;
		NumTaskThreadsPerSet = 0;
		FPlatformTLS::FreeTlsSlot(PerThreadIDTLSSlot);
	}

	// API inherited from FTaskGraphInterface

	/** 
	 *	Function to queue a task, called from a FBaseGraphTask
	 *	@param	Task; the task to queue
	 *	@param	ThreadToExecuteOn; Either a named thread for a threadlocked task or ENamedThreads::AnyThread for a task that is to run on a worker thread
	 *	@param	CurrentThreadIfKnown; This should be the current thread if it is known, or otherwise use ENamedThreads::AnyThread and the current thread will be determined.
	**/
	virtual void QueueTask(FBaseGraphTask* Task, ENamedThreads::Type ThreadToExecuteOn, ENamedThreads::Type InCurrentThreadIfKnown = ENamedThreads::AnyThread) final override
	{
		TASKGRAPH_SCOPE_CYCLE_COUNTER(2, STAT_TaskGraph_QueueTask);

		if (ENamedThreads::GetThreadIndex(ThreadToExecuteOn) == ENamedThreads::AnyThread)
		{
			TASKGRAPH_SCOPE_CYCLE_COUNTER(3, STAT_TaskGraph_QueueTask_AnyThread);
			if (FPlatformProcess::SupportsMultithreading())
			{
				uint32 TaskPriority = ENamedThreads::GetTaskPriority(Task->ThreadToExecuteOn);
				int32 Priority = ENamedThreads::GetThreadPriorityIndex(Task->ThreadToExecuteOn);
				if (Priority == (ENamedThreads::BackgroundThreadPriority >> ENamedThreads::ThreadPriorityShift) && (!bCreatedBackgroundPriorityThreads || !ENamedThreads::bHasBackgroundThreads))
				{
					Priority = ENamedThreads::NormalThreadPriority >> ENamedThreads::ThreadPriorityShift; // we don't have background threads, promote to normal
					TaskPriority = ENamedThreads::NormalTaskPriority >> ENamedThreads::TaskPriorityShift; // demote to normal task pri
				}
				else if (Priority == (ENamedThreads::HighThreadPriority >> ENamedThreads::ThreadPriorityShift) && (!bCreatedHiPriorityThreads || !ENamedThreads::bHasHighPriorityThreads))
				{
					Priority = ENamedThreads::NormalThreadPriority >> ENamedThreads::ThreadPriorityShift; // we don't have hi priority threads, demote to normal
					TaskPriority = ENamedThreads::HighTaskPriority >> ENamedThreads::TaskPriorityShift; // promote to hi task pri
				}
				check(Priority >= 0 && Priority < MAX_THREAD_PRIORITIES);
				{
					TASKGRAPH_SCOPE_CYCLE_COUNTER(4, STAT_TaskGraph_QueueTask_IncomingAnyThreadTasks_Push);
					int32 IndexToStart = IncomingAnyThreadTasks[Priority].Push(Task, TaskPriority);
					if (IndexToStart >= 0)
					{
						StartTaskThread(Priority, IndexToStart);
					}
				}
				return;
			}
			else
			{
				ThreadToExecuteOn = ENamedThreads::GameThread;
			}
		}
		ENamedThreads::Type CurrentThreadIfKnown;
		if (ENamedThreads::GetThreadIndex(InCurrentThreadIfKnown) == ENamedThreads::AnyThread)
		{
			CurrentThreadIfKnown = GetCurrentThread();
		}
		else
		{
			CurrentThreadIfKnown = ENamedThreads::GetThreadIndex(InCurrentThreadIfKnown);
			checkThreadGraph(CurrentThreadIfKnown == ENamedThreads::GetThreadIndex(GetCurrentThread()));
		}
		{
			int32 QueueToExecuteOn = ENamedThreads::GetQueueIndex(ThreadToExecuteOn);
			ThreadToExecuteOn = ENamedThreads::GetThreadIndex(ThreadToExecuteOn);
			FTaskThreadBase* Target = &Thread(ThreadToExecuteOn);
			if (ThreadToExecuteOn == ENamedThreads::GetThreadIndex(CurrentThreadIfKnown))
			{
				Target->EnqueueFromThisThread(QueueToExecuteOn, Task);
			}
			else
			{
				Target->EnqueueFromOtherThread(QueueToExecuteOn, Task);
			}
		}
	}


	virtual	int32 GetNumWorkerThreads() final override
	{
		int32 Result = (NumThreads - NumNamedThreads) / NumTaskThreadSets - GNumWorkerThreadsToIgnore;
		check(Result > 0); // can't tune it to zero task threads
		return Result;
	}

	virtual ENamedThreads::Type GetCurrentThreadIfKnown(bool bLocalQueue) final override
	{
		ENamedThreads::Type Result = GetCurrentThread();
		if (bLocalQueue && ENamedThreads::GetThreadIndex(Result) >= 0 && ENamedThreads::GetThreadIndex(Result) < NumNamedThreads)
		{
			Result = ENamedThreads::Type(int32(Result) | int32(ENamedThreads::LocalQueue));
		}
		return Result;
	}

	virtual bool IsThreadProcessingTasks(ENamedThreads::Type ThreadToCheck) final override
	{
		int32 QueueIndex = ENamedThreads::GetQueueIndex(ThreadToCheck);
		ThreadToCheck = ENamedThreads::GetThreadIndex(ThreadToCheck);
		check(ThreadToCheck >= 0 && ThreadToCheck < NumNamedThreads);
		return Thread(ThreadToCheck).IsProcessingTasks(QueueIndex);
	}

	// External Thread API

	virtual void AttachToThread(ENamedThreads::Type CurrentThread) final override
	{
		CurrentThread = ENamedThreads::GetThreadIndex(CurrentThread);
		check(NumTaskThreadsPerSet);
		check(CurrentThread >= 0 && CurrentThread < NumNamedThreads);
		check(!WorkerThreads[CurrentThread].bAttached);
		Thread(CurrentThread).InitializeForCurrentThread();
	}

	virtual void ProcessThreadUntilIdle(ENamedThreads::Type CurrentThread) final override
	{
		int32 QueueIndex = ENamedThreads::GetQueueIndex(CurrentThread);
		CurrentThread = ENamedThreads::GetThreadIndex(CurrentThread);
		check(CurrentThread >= 0 && CurrentThread < NumNamedThreads);
		check(CurrentThread == GetCurrentThread());
		Thread(CurrentThread).ProcessTasksUntilIdle(QueueIndex);
	}

	virtual void ProcessThreadUntilRequestReturn(ENamedThreads::Type CurrentThread) final override
	{
		int32 QueueIndex = ENamedThreads::GetQueueIndex(CurrentThread);
		CurrentThread = ENamedThreads::GetThreadIndex(CurrentThread);
		check(CurrentThread >= 0 && CurrentThread < NumNamedThreads);
		check(CurrentThread == GetCurrentThread());
		Thread(CurrentThread).ProcessTasksUntilQuit(QueueIndex);
	}

	virtual void RequestReturn(ENamedThreads::Type CurrentThread) final override
	{
		int32 QueueIndex = ENamedThreads::GetQueueIndex(CurrentThread);
		CurrentThread = ENamedThreads::GetThreadIndex(CurrentThread);
		check(CurrentThread != ENamedThreads::AnyThread);
		Thread(CurrentThread).RequestQuit(QueueIndex);
	}

	virtual void WaitUntilTasksComplete(const FGraphEventArray& Tasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread) final override
	{
		ENamedThreads::Type CurrentThread = CurrentThreadIfKnown;
		if (ENamedThreads::GetThreadIndex(CurrentThreadIfKnown) == ENamedThreads::AnyThread)
		{
			bool bIsHiPri = !!ENamedThreads::GetTaskPriority(CurrentThreadIfKnown);
			int32 Priority = ENamedThreads::GetThreadPriorityIndex(CurrentThreadIfKnown);
			check(!ENamedThreads::GetQueueIndex(CurrentThreadIfKnown));
			CurrentThreadIfKnown = ENamedThreads::GetThreadIndex(GetCurrentThread());
			CurrentThread = ENamedThreads::SetPriorities(CurrentThreadIfKnown, Priority, bIsHiPri);
		}
		else
		{
			CurrentThreadIfKnown = ENamedThreads::GetThreadIndex(CurrentThreadIfKnown);
			check(CurrentThreadIfKnown == ENamedThreads::GetThreadIndex(GetCurrentThread()));
			// we don't modify CurrentThread here because it might be a local queue
		}

		if (CurrentThreadIfKnown != ENamedThreads::AnyThread && CurrentThreadIfKnown < NumNamedThreads && !IsThreadProcessingTasks(CurrentThread))
		{
			if (Tasks.Num() > 8) // don't bother to check for completion if there are lots of prereqs...too expensive to check
			{
			bool bAnyPending = false;
			for (int32 Index = 0; Index < Tasks.Num(); Index++)
			{
				if (!Tasks[Index]->IsComplete())
				{
					bAnyPending = true;
					break;
				}
			}
			if (!bAnyPending)
			{
				return;
			}
			}
			// named thread process tasks while we wait
			TGraphTask<FReturnGraphTask>::CreateTask(&Tasks, CurrentThread).ConstructAndDispatchWhenReady(CurrentThread);
			ProcessThreadUntilRequestReturn(CurrentThread);
		}
		else
		{
			// We will just stall this thread on an event while we wait
			FScopedEvent Event;
			TriggerEventWhenTasksComplete(Event.Get(), Tasks, CurrentThreadIfKnown);
		}
	}

	virtual void TriggerEventWhenTasksComplete(FEvent* InEvent, const FGraphEventArray& Tasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread) final override
	{
		check(InEvent);
		bool bAnyPending = true;
		if (Tasks.Num() > 8) // don't bother to check for completion if there are lots of prereqs...too expensive to check
		{
			bAnyPending = false;
			for (int32 Index = 0; Index < Tasks.Num(); Index++)
			{
				if (!Tasks[Index]->IsComplete())
				{
					bAnyPending = true;
					break;
				}
			}
		}
		if (!bAnyPending)
		{
			TestRandomizedThreads();
			InEvent->Trigger();
			return;
		}
		TGraphTask<FTriggerEventGraphTask>::CreateTask(&Tasks, CurrentThreadIfKnown).ConstructAndDispatchWhenReady(InEvent);
	}

	virtual void AddShutdownCallback(TFunction<void()>& Callback)
	{
		ShutdownCallbacks.Emplace(Callback);
	}


	// Scheduling utilities

	void StartTaskThread(int32 Priority, int32 IndexToStart)
	{
		ENamedThreads::Type ThreadToWake = ENamedThreads::Type(IndexToStart + Priority * NumTaskThreadsPerSet + NumNamedThreads);
		((FTaskThreadAnyThread&)Thread(ThreadToWake)).WakeUp();
	}
	void StartAllTaskThreads(bool bDoBackgroundThreads)
	{
		for (int32 Index = 0; Index < GetNumWorkerThreads(); Index++)
		{
			for (int32 Priority = 0; Priority < ENamedThreads::NumThreadPriorities; Priority++)
			{
				if (Priority == (ENamedThreads::NormalThreadPriority >> ENamedThreads::ThreadPriorityShift) ||
					(Priority == (ENamedThreads::HighThreadPriority >> ENamedThreads::ThreadPriorityShift) && bCreatedHiPriorityThreads) ||
					(Priority == (ENamedThreads::BackgroundThreadPriority >> ENamedThreads::ThreadPriorityShift) && bCreatedBackgroundPriorityThreads && bDoBackgroundThreads)
					)
				{
					StartTaskThread(Priority, Index);
				}
			}
		}
	}

	FBaseGraphTask* FindWork(ENamedThreads::Type ThreadInNeed)
	{
		int32 LocalNumWorkingThread = GetNumWorkerThreads() + GNumWorkerThreadsToIgnore;
		int32 MyIndex = int32((uint32(ThreadInNeed) - NumNamedThreads) % NumTaskThreadsPerSet);
		int32 Priority = int32((uint32(ThreadInNeed) - NumNamedThreads) / NumTaskThreadsPerSet);
		check(MyIndex >= 0 && MyIndex < LocalNumWorkingThread &&
			MyIndex < (PLATFORM_64BITS ? 63 : 32) &&
			Priority >= 0 && Priority < ENamedThreads::NumThreadPriorities);

		return IncomingAnyThreadTasks[Priority].Pop(MyIndex, true);
	}

	void StallForTuning(int32 Index, bool Stall)
	{
		for (int32 Priority = 0; Priority < ENamedThreads::NumThreadPriorities; Priority++)
		{
			ENamedThreads::Type ThreadToWake = ENamedThreads::Type(Index + Priority * NumTaskThreadsPerSet + NumNamedThreads);
			((FTaskThreadAnyThread&)Thread(ThreadToWake)).StallForTuning(Stall);
		}
	}
	void SetTaskThreadPriorities(EThreadPriority Pri)
	{
		check(NumTaskThreadSets == 1); // otherwise tuning this doesn't make a lot of sense
		for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ThreadIndex++)
		{
			if (ThreadIndex > LastExternalThread)
			{
				WorkerThreads[ThreadIndex].RunnableThread->SetThreadPriority(Pri);
			}
		}
	}

private:

	// Internals

	/** 
	 *	Internal function to verify an index and return the corresponding FTaskThread
	 *	@param	Index; Id of the thread to retrieve.
	 *	@return	Reference to the corresponding thread.
	**/
	FTaskThreadBase& Thread(int32 Index)
	{
		checkThreadGraph(Index >= 0 && Index < NumThreads);
		checkThreadGraph(WorkerThreads[Index].TaskGraphWorker->GetThreadId() == Index);
		return *WorkerThreads[Index].TaskGraphWorker;
	}

	/** 
	 *	Examines the TLS to determine the identity of the current thread.
	 *	@return	Id of the thread that is this thread or ENamedThreads::AnyThread if this thread is unknown or is a named thread that has not attached yet.
	**/
	ENamedThreads::Type GetCurrentThread()
	{
		ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread;
		FWorkerThread* TLSPointer = (FWorkerThread*)FPlatformTLS::GetTlsValue(PerThreadIDTLSSlot);
		if (TLSPointer)
		{
			checkThreadGraph(TLSPointer - WorkerThreads >= 0 && TLSPointer - WorkerThreads < NumThreads);
			int32 ThreadIndex = TLSPointer - WorkerThreads;
			checkThreadGraph(Thread(ThreadIndex).GetThreadId() == ThreadIndex);
			if (ThreadIndex < NumNamedThreads)
			{
				CurrentThreadIfKnown = ENamedThreads::Type(ThreadIndex);
			}
			else
			{
				int32 Priority = (ThreadIndex - NumNamedThreads) / NumTaskThreadsPerSet;
				CurrentThreadIfKnown = ENamedThreads::SetPriorities(ENamedThreads::Type(ThreadIndex), Priority, false);
			}
		}
		return CurrentThreadIfKnown;
	}

	int32 ThreadIndexToPriorityIndex(int32 ThreadIndex)
	{
		check(ThreadIndex >= NumNamedThreads && ThreadIndex < NumThreads);
		int32 Result = (ThreadIndex - NumNamedThreads) / NumTaskThreadsPerSet;
		check(Result >= 0 && Result < NumTaskThreadSets);
		return Result;
	}



	enum
	{
		/** Compile time maximum number of threads. Didn't really need to be a compile time constant, but task thread are limited by MAX_LOCK_FREE_LINKS_AS_BITS **/
		MAX_THREADS = 22 * (CREATE_HIPRI_TASK_THREADS + CREATE_BACKGROUND_TASK_THREADS + 1) + ENamedThreads::ActualRenderingThread + 1,
		MAX_THREAD_PRIORITIES = 3
	};

	/** Per thread data. **/
	FWorkerThread		WorkerThreads[MAX_THREADS];
	/** Number of threads actually in use. **/
	int32				NumThreads;
	/** Number of named threads actually in use. **/
	int32				NumNamedThreads;
	/** Number of tasks thread sets for priority **/
	int32				NumTaskThreadSets;
	/** Number of tasks threads per priority set **/
	int32				NumTaskThreadsPerSet;
	bool				bCreatedHiPriorityThreads;
	bool				bCreatedBackgroundPriorityThreads;
	/**
	 * "External Threads" are not created, the thread is created elsewhere and makes an explicit call to run 
	 * Here all of the named threads are external but that need not be the case.
	 * All unnamed threads must be internal
	**/
	ENamedThreads::Type LastExternalThread;
	FThreadSafeCounter	ReentrancyCheck;
	/** Index of TLS slot for FWorkerThread* pointer. **/
	uint32				PerThreadIDTLSSlot;

	/** Array of callbacks to call before shutdown. **/
	TArray<TFunction<void()> > ShutdownCallbacks;

	FStallingTaskQueue<FBaseGraphTask, PLATFORM_CACHE_LINE_SIZE, 2>	IncomingAnyThreadTasks[MAX_THREAD_PRIORITIES];
};


// Implementations of FTaskThread function that require knowledge of FTaskGraphImplementation

FBaseGraphTask* FTaskThreadAnyThread::FindWork()
{
	return FTaskGraphImplementation::Get().FindWork(ThreadId);
}


// Statics in FTaskGraphInterface

void FTaskGraphInterface::Startup(int32 NumThreads)
{
	// TaskGraphImplementationSingleton is actually set in the constructor because find work will be called before this returns.
	new FTaskGraphImplementation(NumThreads); 
}

void FTaskGraphInterface::Shutdown()
{
	delete TaskGraphImplementationSingleton;
	TaskGraphImplementationSingleton = NULL;
}

bool FTaskGraphInterface::IsRunning()
{
    return TaskGraphImplementationSingleton != NULL;
}

FTaskGraphInterface& FTaskGraphInterface::Get()
{
	checkThreadGraph(TaskGraphImplementationSingleton);
	return *TaskGraphImplementationSingleton;
}


// Statics and some implementations from FBaseGraphTask and FGraphEvent

static FBaseGraphTask::TSmallTaskAllocator TheSmallTaskAllocator;
FBaseGraphTask::TSmallTaskAllocator& FBaseGraphTask::GetSmallTaskAllocator()
{
	return TheSmallTaskAllocator;
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void FBaseGraphTask::LogPossiblyInvalidSubsequentsTask(const TCHAR* TaskName)
{
	UE_LOG(LogTaskGraph, Warning, TEXT("Subsequents of %s look like they contain invalid pointer(s)."), TaskName);
}
#endif

static TLockFreeClassAllocator_TLSCache<FGraphEvent, PLATFORM_CACHE_LINE_SIZE> TheGraphEventAllocator;

FGraphEventRef FGraphEvent::CreateGraphEvent()
{
	return TheGraphEventAllocator.New();
}

void FGraphEvent::Recycle(FGraphEvent* ToRecycle)
{
	TheGraphEventAllocator.Free(ToRecycle);
}

void FGraphEvent::DispatchSubsequents(TArray<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThreadIfKnown)
{
	if (EventsToWaitFor.Num())
	{
		// need to save this first and empty the actual tail of the task might be recycled faster than it is cleared.
		FGraphEventArray TempEventsToWaitFor;
		Exchange(EventsToWaitFor, TempEventsToWaitFor);
		// create the Gather...this uses a special version of private CreateTask that "assumes" the subsequent list (which other threads might still be adding too).
		DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.DontCompleteUntil"),
			STAT_FNullGraphTask_DontCompleteUntil,
			STATGROUP_TaskGraphTasks);

		TGraphTask<FNullGraphTask>::CreateTask(FGraphEventRef(this), &TempEventsToWaitFor, CurrentThreadIfKnown).ConstructAndDispatchWhenReady(GET_STATID(STAT_FNullGraphTask_DontCompleteUntil), ENamedThreads::AnyHiPriThreadHiPriTask);
		return;
	}

	SubsequentList.PopAllAndClose(NewTasks);
	for (int32 Index = NewTasks.Num() - 1; Index >= 0 ; Index--) // reverse the order since PopAll is implicitly backwards
	{
		FBaseGraphTask* NewTask = NewTasks[Index];
		checkThreadGraph(NewTask);
		NewTask->ConditionalQueueTask(CurrentThreadIfKnown);
	}
	NewTasks.Reset();
}

FGraphEvent::~FGraphEvent()
{
#if DO_CHECK
	if (!IsComplete())
	{
		check(SubsequentList.IsClosed());
	}
#endif
	CheckDontCompleteUntilIsEmpty(); // We should not have any wait untils outstanding
}

DECLARE_CYCLE_STAT(TEXT("FBroadcastTask"), STAT_FBroadcastTask, STATGROUP_TaskGraphTasks);

class FBroadcastTask
{
public:
	FBroadcastTask(TFunction<void(ENamedThreads::Type CurrentThread)>& InFunction, ENamedThreads::Type InDesiredThread, FThreadSafeCounter* InStallForTaskThread, FEvent* InTaskEvent, FEvent* InCallerEvent)
		: Function(InFunction)
		, DesiredThread(InDesiredThread)
		, StallForTaskThread(InStallForTaskThread)
		, TaskEvent(InTaskEvent)
		, CallerEvent(InCallerEvent)
	{
	}
	ENamedThreads::Type GetDesiredThread()
	{
		return DesiredThread;
	}

	FORCEINLINE TStatId GetStatId() const
	{
		return GET_STATID(STAT_FBroadcastTask);
	}

	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	void FORCEINLINE DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Function(CurrentThread);
		if (StallForTaskThread)
		{
			if (StallForTaskThread->Decrement())
			{
				TaskEvent->Wait();
			}
			else
				{
				CallerEvent->Trigger();
			}
		}
	}
private:
	TFunction<void(ENamedThreads::Type CurrentThread)> Function;
	const ENamedThreads::Type DesiredThread;
	FThreadSafeCounter* StallForTaskThread;
	FEvent* TaskEvent;
	FEvent* CallerEvent;
};

void FTaskGraphInterface::BroadcastSlow_OnlyUseForSpecialPurposes(bool bDoTaskThreads, bool bDoBackgroundThreads, TFunction<void(ENamedThreads::Type CurrentThread)>& Callback)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FTaskGraphInterface_BroadcastSlow_OnlyUseForSpecialPurposes);
	check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);
	if (!TaskGraphImplementationSingleton)
	{
		// we aren't going yet
		Callback(ENamedThreads::GameThread);
		return;
	}


	TArray<FEvent*> TaskEvents;

	FEvent* MyEvent = nullptr;
	FGraphEventArray TaskThreadTasks;
	FThreadSafeCounter StallForTaskThread;
	if (bDoTaskThreads)
	{
		MyEvent = FPlatformProcess::GetSynchEventFromPool(false);

		int32 Workers = FTaskGraphInterface::Get().GetNumWorkerThreads();
		StallForTaskThread.Add(Workers * (1 + (bDoBackgroundThreads && ENamedThreads::bHasBackgroundThreads) + !!(ENamedThreads::bHasHighPriorityThreads)));

		TaskEvents.Reserve(StallForTaskThread.GetValue());
		{

			for (int32 Index = 0; Index < Workers; Index++)
			{
				FEvent* TaskEvent = FPlatformProcess::GetSynchEventFromPool(false);
				TaskEvents.Add(TaskEvent);
				TaskThreadTasks.Add(TGraphTask<FBroadcastTask>::CreateTask().ConstructAndDispatchWhenReady(Callback, ENamedThreads::AnyNormalThreadHiPriTask, &StallForTaskThread, TaskEvent, MyEvent));
			}

		}
		if (ENamedThreads::bHasHighPriorityThreads)
		{
			for (int32 Index = 0; Index < Workers; Index++)
			{
				FEvent* TaskEvent = FPlatformProcess::GetSynchEventFromPool(false);
				TaskEvents.Add(TaskEvent);
				TaskThreadTasks.Add(TGraphTask<FBroadcastTask>::CreateTask().ConstructAndDispatchWhenReady(Callback, ENamedThreads::AnyHiPriThreadHiPriTask, &StallForTaskThread, TaskEvent, MyEvent));
			}
		}
		if (bDoBackgroundThreads && ENamedThreads::bHasBackgroundThreads)
		{
			for (int32 Index = 0; Index < Workers; Index++)
			{
				FEvent* TaskEvent = FPlatformProcess::GetSynchEventFromPool(false);
				TaskEvents.Add(TaskEvent);
				TaskThreadTasks.Add(TGraphTask<FBroadcastTask>::CreateTask().ConstructAndDispatchWhenReady(Callback, ENamedThreads::AnyBackgroundHiPriTask, &StallForTaskThread, TaskEvent, MyEvent));
			}
		}
		check(TaskGraphImplementationSingleton);
	}


	FGraphEventArray Tasks;
	STAT(Tasks.Add(TGraphTask<FBroadcastTask>::CreateTask().ConstructAndDispatchWhenReady(Callback, ENamedThreads::SetTaskPriority(ENamedThreads::StatsThread, ENamedThreads::HighTaskPriority), nullptr, nullptr, nullptr)););
	if (GRHIThread_InternalUseOnly)
	{
		Tasks.Add(TGraphTask<FBroadcastTask>::CreateTask().ConstructAndDispatchWhenReady(Callback, ENamedThreads::SetTaskPriority(ENamedThreads::RHIThread, ENamedThreads::HighTaskPriority), nullptr, nullptr, nullptr));
	}
	if (ENamedThreads::RenderThread != ENamedThreads::GameThread)
	{
		Tasks.Add(TGraphTask<FBroadcastTask>::CreateTask().ConstructAndDispatchWhenReady(Callback, ENamedThreads::SetTaskPriority(ENamedThreads::RenderThread, ENamedThreads::HighTaskPriority), nullptr, nullptr, nullptr));
	}
	Tasks.Add(TGraphTask<FBroadcastTask>::CreateTask().ConstructAndDispatchWhenReady(Callback, ENamedThreads::GameThread_Local, nullptr, nullptr, nullptr));
	if (bDoTaskThreads)
	{
		check(MyEvent);
		if (MyEvent && !MyEvent->Wait(3000))
		{
			UE_LOG(LogTaskGraph, Log, TEXT("FTaskGraphInterface::BroadcastSlow_OnlyUseForSpecialPurposes Broadcast failed after three seconds. Ok during automated tests."));
		}
		for (FEvent* TaskEvent : TaskEvents)
		{
			TaskEvent->Trigger();
		}
		FTaskGraphInterface::Get().WaitUntilTasksComplete(TaskThreadTasks, ENamedThreads::GameThread_Local);
	}
	FTaskGraphInterface::Get().WaitUntilTasksComplete(Tasks, ENamedThreads::GameThread_Local);
	for (FEvent* TaskEvent : TaskEvents)
	{
		FPlatformProcess::ReturnSynchEventToPool(TaskEvent);
	}
	if (MyEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(MyEvent);
	}
}

static void HandleNumWorkerThreadsToIgnore(const TArray<FString>& Args)
{
	if (Args.Num() > 0)
	{
		int32 Arg = FCString::Atoi(*Args[0]);
		int32 MaxNumPerBank = FTaskGraphInterface::Get().GetNumWorkerThreads() + GNumWorkerThreadsToIgnore;
		if (Arg < MaxNumPerBank && Arg >= 0 && Arg != GNumWorkerThreadsToIgnore)
		{
			if (Arg > GNumWorkerThreadsToIgnore)
			{
				for (int32 Index = MaxNumPerBank - GNumWorkerThreadsToIgnore - 1; Index >= MaxNumPerBank - Arg; Index--)
				{
					FTaskGraphImplementation::Get().StallForTuning(Index, true);
				}
			}
			else
			{
				for (int32 Index = MaxNumPerBank - Arg - 1; Index >= MaxNumPerBank - GNumWorkerThreadsToIgnore; Index--)
				{
					FTaskGraphImplementation::Get().StallForTuning(Index, false);
				}
			}
			GNumWorkerThreadsToIgnore = Arg;
		}
	}
	UE_LOG(LogConsoleResponse, Display, TEXT("Currently ignoring %d threads per priority bank"), GNumWorkerThreadsToIgnore);
}

static FAutoConsoleCommand CVarNumWorkerThreadsToIgnore(
	TEXT("TaskGraph.NumWorkerThreadsToIgnore"),
	TEXT("Used to tune the number of task threads. Generally once you have found the right value, PlatformMisc::NumberOfWorkerThreadsToSpawn() should be hardcoded."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&HandleNumWorkerThreadsToIgnore)
	);

// Benchmark

#include "Async/ParallelFor.h"

static FORCEINLINE void DoWork(void* Hash, FThreadSafeCounter& Counter, FThreadSafeCounter& Cycles, int32 Work)
{
	if (Work > 0)
	{
		uint32 CyclesStart = FPlatformTime::Cycles();
		Counter.Increment();
		int32 Sum = 0;
		for (int32 Index = 0; Index < Work; Index++)
		{
			Sum += PointerHash(((uint64*)Hash) + Index);
		}
		Cycles.Add(FPlatformTime::Cycles() - CyclesStart + (Sum & 1));
	}
	else if (Work == 0)
	{
		Counter.Increment();
	}
}

class FIncGraphTask : public FCustomStatIDGraphTaskBase
{
public:
	FORCEINLINE FIncGraphTask(FThreadSafeCounter& InCounter, FThreadSafeCounter& InCycles, int32 InWork)
		: FCustomStatIDGraphTaskBase(TStatId())
		, Counter(InCounter)
		, Cycles(InCycles)
		, Work(InWork)
	{
	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyThread;
	}

	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::FireAndForget; }
	void FORCEINLINE DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		DoWork(this, Counter, Cycles, Work);
	}
private:
	FThreadSafeCounter& Counter;
	FThreadSafeCounter& Cycles;
	int32 Work;
};

class FIncGraphTaskSub : public FCustomStatIDGraphTaskBase
{
public:
	FORCEINLINE FIncGraphTaskSub(FThreadSafeCounter& InCounter, FThreadSafeCounter& InCycles, int32 InWork)
		: FCustomStatIDGraphTaskBase(TStatId())
		, Counter(InCounter)
		, Cycles(InCycles)
		, Work(InWork)
	{
	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyThread;
	}

	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	void FORCEINLINE DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		DoWork(this, Counter, Cycles, Work);
	}
private:
	FThreadSafeCounter& Counter;
	FThreadSafeCounter& Cycles;
	int32 Work;
};

class FIncGraphTaskGT : public FIncGraphTask
{
public:
	FORCEINLINE FIncGraphTaskGT(FThreadSafeCounter& InCounter, FThreadSafeCounter& InCycles, int32 InWork)
		: FIncGraphTask(InCounter, InCycles, InWork)
	{
	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::GameThread_Local;
	}
};

class FBoolGraphTask : public FCustomStatIDGraphTaskBase
{
public:
	FORCEINLINE FBoolGraphTask(bool *InOut)
		: FCustomStatIDGraphTaskBase(TStatId())
		, Out(InOut)
	{
	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyThread;
	}
	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::FireAndForget; }
	void FORCEINLINE DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		*Out = true;
	}
private:
	bool *Out;
};


void PrintResult(double& StartTime, double& QueueTime, double& EndTime, double& JoinTime, FThreadSafeCounter& Counter, FThreadSafeCounter& Cycles, const TCHAR* Message)
{
	UE_LOG(LogConsoleResponse, Display, TEXT("Total %6.3fms   %6.3fms queue   %6.3fms join   %6.3fms wait   %6.3fms work   : %s")
		, float(1000.0 * (EndTime-StartTime)), float(1000.0 * (QueueTime-StartTime)), float(1000.0 * (JoinTime-QueueTime)), float(1000.0 * (EndTime-JoinTime))
		, float(FPlatformTime::GetSecondsPerCycle() * double(Cycles.GetValue()) * 1000.0)
		, Message
		);

	Counter.Reset();
	Cycles.Reset();
	StartTime = 0.0;
	QueueTime = 0.0;
	EndTime = 0.0;
	JoinTime = 0.0;
}

static void TaskGraphBenchmark(const TArray<FString>& Args)
{
	FSlowHeartBeatScope SuspendHeartBeat;
	double StartTime, QueueTime, EndTime, JoinTime;
	FThreadSafeCounter Counter;
	FThreadSafeCounter Cycles;

	if (!FPlatformProcess::SupportsMultithreading())
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("WARNING: TaskGraphBenchmark disabled for non multi-threading platforms"));
		return;
	}

	if (Args.Num() == 1 && Args[0] == TEXT("infinite"))
	{
		while (true)
		{
			{
				StartTime = FPlatformTime::Seconds();

				ParallelFor(1000, 
					[&Counter, &Cycles](int32 Index)
				{
					TGraphTask<FIncGraphTaskGT>::CreateTask().ConstructAndDispatchWhenReady(Counter, Cycles, -1);
				}
				);
				QueueTime = FPlatformTime::Seconds();
				JoinTime = QueueTime;
				FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread_Local);
				EndTime = FPlatformTime::Seconds();
			}
		}
	}
	{
		StartTime = FPlatformTime::Seconds();
		FGraphEventArray Tasks;
		Tasks.Reserve(1000);
		for (int32 Index = 0; Index < 1000; Index++)
		{
			Tasks.Emplace(TGraphTask<FNullGraphTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread));
		}
		QueueTime = FPlatformTime::Seconds();
		FGraphEventRef Join = TGraphTask<FNullGraphTask>::CreateTask(&Tasks, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
		JoinTime = FPlatformTime::Seconds();
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Join, ENamedThreads::GameThread_Local);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, ordinary GT start"));
	{
		StartTime = FPlatformTime::Seconds();
		FGraphEventArray Tasks;
		Tasks.Reserve(1000);
		for (int32 Index = 0; Index < 1000; Index++)
		{
			Tasks.Emplace(TGraphTask<FIncGraphTaskSub>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(Counter, Cycles, 1000));
		}
		QueueTime = FPlatformTime::Seconds();
		FGraphEventRef Join = TGraphTask<FNullGraphTask>::CreateTask(&Tasks, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
		JoinTime = FPlatformTime::Seconds();
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Join, ENamedThreads::GameThread_Local);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, ordinary GT start, with work"));
	{
		StartTime = FPlatformTime::Seconds();
		FGraphEventArray Tasks;
		Tasks.AddZeroed(1000);

		ParallelFor(1000, 
			[&Tasks](int32 Index)
			{
				Tasks[Index] = TGraphTask<FNullGraphTask>::CreateTask().ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
			}
		);
		QueueTime = FPlatformTime::Seconds();
		FGraphEventRef Join = TGraphTask<FNullGraphTask>::CreateTask(&Tasks, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
		JoinTime = FPlatformTime::Seconds();
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Join, ENamedThreads::GameThread_Local);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, ParallelFor start"));
	{
		StartTime = FPlatformTime::Seconds();
		FGraphEventArray Tasks;
		Tasks.AddZeroed(10);

		ParallelFor(10, 
			[&Tasks](int32 Index)
		{
			FGraphEventArray InnerTasks;
			InnerTasks.AddZeroed(100);
			ENamedThreads::Type CurrentThread = FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
			for (int32 InnerIndex = 0; InnerIndex < 100; InnerIndex++)
			{
				InnerTasks[InnerIndex] = TGraphTask<FNullGraphTask>::CreateTask(nullptr, CurrentThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
			}
			// join the above tasks
			Tasks[Index] = TGraphTask<FNullGraphTask>::CreateTask(&InnerTasks, CurrentThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
		}
		);
		QueueTime = FPlatformTime::Seconds();
		FGraphEventRef Join = TGraphTask<FNullGraphTask>::CreateTask(&Tasks, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
		JoinTime = FPlatformTime::Seconds();
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Join, ENamedThreads::GameThread_Local);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, ParallelFor start, batched completion 10x100"));
	{
		StartTime = FPlatformTime::Seconds();
		FGraphEventArray Tasks;
		Tasks.AddZeroed(100);

		ParallelFor(100, 
			[&Tasks](int32 Index)
		{
			FGraphEventArray InnerTasks;
			InnerTasks.AddZeroed(10);
			ENamedThreads::Type CurrentThread = FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
			for (int32 InnerIndex = 0; InnerIndex < 10; InnerIndex++)
			{
				InnerTasks[InnerIndex] = TGraphTask<FNullGraphTask>::CreateTask(nullptr, CurrentThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
			}
			// join the above tasks
			Tasks[Index] = TGraphTask<FNullGraphTask>::CreateTask(&InnerTasks, CurrentThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
		}
		);
		QueueTime = FPlatformTime::Seconds();
		FGraphEventRef Join = TGraphTask<FNullGraphTask>::CreateTask(&Tasks, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
		JoinTime = FPlatformTime::Seconds();
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Join, ENamedThreads::GameThread_Local);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, ParallelFor start, batched completion 100x10"));

	{
		StartTime = FPlatformTime::Seconds();

		ParallelFor(1000, 
			[&Counter, &Cycles](int32 Index)
			{
				TGraphTask<FIncGraphTask>::CreateTask().ConstructAndDispatchWhenReady(Counter, Cycles, 0);
			}
		);
		QueueTime = FPlatformTime::Seconds();
		JoinTime = QueueTime;
		while (Counter.GetValue() < 1000)
		{
			FPlatformMisc::MemoryBarrier();
		}
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, ParallelFor, counter tracking"));

	{
		StartTime = FPlatformTime::Seconds();

		static bool Output[1000];
		FPlatformMemory::Memzero(Output, 1000);

		ParallelFor(1000, 
			[](int32 Index)
		{
			TGraphTask<FBoolGraphTask>::CreateTask().ConstructAndDispatchWhenReady(Output + Index);
		}
		);
		QueueTime = FPlatformTime::Seconds();
		JoinTime = QueueTime;
		for (int32 Index = 0; Index < 1000; Index++)
		{
			while (!Output[Index])
			{
				FPlatformProcess::Sleep(0.0f);
			}
		}
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, ParallelFor, bool* tracking"));

	{
		StartTime = FPlatformTime::Seconds();

		ParallelFor(1000, 
			[&Counter, &Cycles](int32 Index)
		{
			TGraphTask<FIncGraphTask>::CreateTask().ConstructAndDispatchWhenReady(Counter, Cycles, 1000);
		}
		);
		QueueTime = FPlatformTime::Seconds();
		JoinTime = QueueTime;
		while (Counter.GetValue() < 1000)
		{
			FPlatformProcess::Sleep(0.0f);
		}
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, ParallelFor, counter tracking, with work"));
	{
		StartTime = FPlatformTime::Seconds();
		for (int32 Index = 0; Index < 1000; Index++)
		{
			TGraphTask<FIncGraphTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(Counter, Cycles, 1000);
		}
		QueueTime = FPlatformTime::Seconds();
		JoinTime = QueueTime;
		while (Counter.GetValue() < 1000)
		{
			FPlatformProcess::Sleep(0.0f);
		}
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, GT submit, counter tracking, with work"));
	{
		StartTime = FPlatformTime::Seconds();

		ParallelFor(1000, 
			[&Counter, &Cycles](int32 Index)
		{
			TGraphTask<FIncGraphTaskGT>::CreateTask().ConstructAndDispatchWhenReady(Counter, Cycles, -1);
		}
		);
		QueueTime = FPlatformTime::Seconds();
		JoinTime = QueueTime;
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread_Local);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 GT tasks, ParallelFor, no tracking (none needed)"));


	{
		StartTime = FPlatformTime::Seconds();
		QueueTime = StartTime;
		JoinTime = QueueTime;
		ParallelFor(1000, 
			[&Counter, &Cycles](int32 Index)
			{
				DoWork(&Counter, Counter, Cycles, -1);
			}
		);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 element do-nothing ParallelFor"));
	{
		StartTime = FPlatformTime::Seconds();
		QueueTime = StartTime;
		JoinTime = QueueTime;
		ParallelFor(1000, 
			[&Counter, &Cycles](int32 Index)
			{
				DoWork(&Counter, Counter, Cycles, 1000);
			}
		);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 element ParallelFor, with work"));

	{
		StartTime = FPlatformTime::Seconds();
		QueueTime = StartTime;
		JoinTime = QueueTime;
		ParallelFor(1000, 
			[&Counter, &Cycles](int32 Index)
			{
				DoWork(&Counter, Counter, Cycles, 1000);
			},
			true
		);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 element ParallelFor, single threaded, with work"));
}

static FAutoConsoleCommand TaskGraphBenchmarkCmd(
	TEXT("TaskGraph.Benchmark"),
	TEXT("Prints the time to run 1000 no-op tasks."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&TaskGraphBenchmark)
	);


struct FTestStruct
{
	int32 Index;
	int32 Constant;
	FTestStruct(int32 InIndex)
		: Index(InIndex)
		, Constant(0xfe05abcd)
	{
	}
};

struct FTestRigFIFO
{
	FLockFreePointerFIFOBase<FTestStruct, PLATFORM_CACHE_LINE_SIZE> Test1;
	FLockFreePointerFIFOBase<FTestStruct, 1> Test2;
	FLockFreePointerFIFOBase<FTestStruct, 1, 1 << 4> Test3;
};

struct FTestRigLIFO
{
	FLockFreePointerListLIFOBase<FTestStruct, PLATFORM_CACHE_LINE_SIZE> Test1;
	FLockFreePointerListLIFOBase<FTestStruct, 1> Test2;
	FLockFreePointerListLIFOBase<FTestStruct, 1, 1 << 4> Test3;
};

static void TestLockFree(int32 OuterIters = 3)
{
	FSlowHeartBeatScope SuspendHeartBeat;

	if (!FPlatformProcess::SupportsMultithreading())
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("WARNING: TestLockFree disabled for non multi-threading platforms"));
		return;
	}

	for (int32 Iter = 0; Iter < OuterIters; Iter++)
	{
		{
			UE_LOG(LogTemp, Display, TEXT("******************************* Iter FIFO %d"), Iter);
			FTestRigFIFO Rig;
			for (int32 Index = 0; Index < 1000; Index++)
			{
				Rig.Test1.Push(new FTestStruct(Index));
			}
			TFunction<void(ENamedThreads::Type CurrentThread)> Broadcast =
				[&Rig](ENamedThreads::Type MyThread)
			{
				FRandomStream Stream(((int32)MyThread) * 7 + 13);
				for (int32 Index = 0; Index < 1000000; Index++)
				{
					if (Index % 200000 == 1)
					{
						//UE_LOG(LogTemp, Log, TEXT("%8d iters thread=%d"), Index, int32(MyThread));
					}
					if (Stream.FRand() < .03f)
					{
						TArray<FTestStruct*> Items;
						{
							float r = Stream.FRand();
							if (r < .33f)
							{
								Rig.Test1.PopAll(Items);
							}
							else if (r < .66f)
							{
								Rig.Test2.PopAll(Items);
							}
							else
							{
								Rig.Test3.PopAll(Items);
							}
						}
						for (FTestStruct* Item : Items)
						{
							float r = Stream.FRand();
							if (r < .33f)
							{
								Rig.Test1.Push(Item);
							}
							else if (r < .66f)
							{
								Rig.Test2.Push(Item);
							}
							else
							{
								Rig.Test3.Push(Item);
							}
						}
					}
					else
					{
						FTestStruct* Item;
						{
							float r = Stream.FRand();
							if (r < .33f)
							{
								Item = Rig.Test1.Pop();
							}
							else if (r < .66f)
							{
								Item = Rig.Test2.Pop();
							}
							else
							{
								Item = Rig.Test3.Pop();
							}
						}
						if (Item)
						{
							float r = Stream.FRand();
							if (r < .33f)
							{
								Rig.Test1.Push(Item);
							}
							else if (r < .66f)
							{
								Rig.Test2.Push(Item);
							}
							else
							{
								Rig.Test3.Push(Item);
							}
						}
					}
				}
			};
			FTaskGraphInterface::BroadcastSlow_OnlyUseForSpecialPurposes(true, false, Broadcast);

			TArray<FTestStruct*> Items;
			Rig.Test1.PopAll(Items);
			Rig.Test2.PopAll(Items);
			Rig.Test3.PopAll(Items);

			checkf(Items.Num() == 1000, TEXT("Items %d"), Items.Num());

			for (int32 LookFor = 0; LookFor < 1000; LookFor++)
			{
				bool bFound = false;
				for (int32 Index = 0; Index < 1000; Index++)
				{
					if (Items[Index]->Index == LookFor && Items[Index]->Constant == 0xfe05abcd)
					{
						check(!bFound);
						bFound = true;
					}
				}
				check(bFound);
			}
			for (FTestStruct* Item : Items)
			{
				delete Item;
			}

			UE_LOG(LogTemp, Display, TEXT("******************************* Pass FTestRigFIFO"));

		}
		{
			UE_LOG(LogTemp, Display, TEXT("******************************* Iter LIFO %d"), Iter);
			FTestRigLIFO Rig;
			for (int32 Index = 0; Index < 1000; Index++)
			{
				Rig.Test1.Push(new FTestStruct(Index));
			}
			TFunction<void(ENamedThreads::Type CurrentThread)> Broadcast =
				[&Rig](ENamedThreads::Type MyThread)
			{
				FRandomStream Stream(((int32)MyThread) * 7 + 13);
				for (int32 Index = 0; Index < 1000000; Index++)
				{
					if (Index % 200000 == 1)
					{
						//UE_LOG(LogTemp, Log, TEXT("%8d iters thread=%d"), Index, int32(MyThread));
					}
					if (Stream.FRand() < .03f)
					{
						TArray<FTestStruct*> Items;
						{
							float r = Stream.FRand();
							if (r < .33f)
							{
								Rig.Test1.PopAll(Items);
							}
							else if (r < .66f)
							{
								Rig.Test2.PopAll(Items);
							}
							else
							{
								Rig.Test3.PopAll(Items);
							}
						}
						for (FTestStruct* Item : Items)
						{
							float r = Stream.FRand();
							if (r < .33f)
							{
								Rig.Test1.Push(Item);
							}
							else if (r < .66f)
							{
								Rig.Test2.Push(Item);
							}
							else
							{
								Rig.Test3.Push(Item);
							}
						}
					}
					else
					{
						FTestStruct* Item;
						{
							float r = Stream.FRand();
							if (r < .33f)
							{
								Item = Rig.Test1.Pop();
							}
							else if (r < .66f)
							{
								Item = Rig.Test2.Pop();
							}
							else
							{
								Item = Rig.Test3.Pop();
							}
						}
						if (Item)
						{
							float r = Stream.FRand();
							if (r < .33f)
							{
								Rig.Test1.Push(Item);
							}
							else if (r < .66f)
							{
								Rig.Test2.Push(Item);
							}
							else
							{
								Rig.Test3.Push(Item);
							}
						}
					}
				}
			};
			FTaskGraphInterface::BroadcastSlow_OnlyUseForSpecialPurposes(true, false, Broadcast);

			TArray<FTestStruct*> Items;
			Rig.Test1.PopAll(Items);
			Rig.Test2.PopAll(Items);
			Rig.Test3.PopAll(Items);

			checkf(Items.Num() == 1000, TEXT("Items %d"), Items.Num());

			for (int32 LookFor = 0; LookFor < 1000; LookFor++)
			{
				bool bFound = false;
				for (int32 Index = 0; Index < 1000; Index++)
				{
					if (Items[Index]->Index == LookFor && Items[Index]->Constant == 0xfe05abcd)
					{
						check(!bFound);
						bFound = true;
					}
				}
				check(bFound);
			}
			for (FTestStruct* Item : Items)
			{
				delete Item;
			}

			UE_LOG(LogTemp, Display, TEXT("******************************* Pass FTestRigLIFO"));

		}
	}
}

static void TestLockFree(const TArray<FString>& Args)
{
	TestLockFree(10);
}

static FAutoConsoleCommand TestLockFreeCmd(
	TEXT("TaskGraph.TestLockFree"),
	TEXT("Test lock free lists"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&TestLockFree)
	);

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMyTaskGraphTest, "System.Core.Misc.TaskGraph", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FMyTaskGraphTest::RunTest(const FString& Parameters)
{
	TArray<FString> Args;
	TaskGraphBenchmark(Args);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLockFreeTest, "System.Core.Misc.LockFree", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FLockFreeTest::RunTest(const FString& Parameters)
{
	TestLockFree(3);
	return true;
}

#endif

static void SetTaskThreadPriority(const TArray<FString>& Args)
{
	EThreadPriority Pri = TPri_Normal;
	if (Args.Num() && Args[0] == TEXT("abovenormal"))
	{
		Pri = TPri_AboveNormal;
		UE_LOG(LogConsoleResponse, Display, TEXT("Setting task thread priority to above normal."));
	}
	else if (Args.Num() && Args[0] == TEXT("belownormal"))
	{
		Pri = TPri_BelowNormal;
		UE_LOG(LogConsoleResponse, Display, TEXT("Setting task thread priority to below normal."));
	}
	else
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("Setting task thread priority to normal."));
	}
	FTaskGraphImplementation::Get().SetTaskThreadPriorities(Pri);
}

static FAutoConsoleCommand TaskThreadPriorityCmd(
	TEXT("TaskGraph.TaskThreadPriority"),
	TEXT("Sets the priority of the task threads. Argument is one of belownormal, normal or abovenormal."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&SetTaskThreadPriority)
	);
