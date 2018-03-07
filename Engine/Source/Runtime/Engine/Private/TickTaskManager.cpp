// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TickTaskManager.cpp: Manager for ticking tasks
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "Async/TaskGraphInterfaces.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "TickTaskManagerInterface.h"
#include "Async/ParallelFor.h"
#include "Misc/TimeGuard.h"

DEFINE_LOG_CATEGORY_STATIC(LogTick, Log, All);

DECLARE_CYCLE_STAT(TEXT("Queue Ticks"),STAT_QueueTicks,STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("Queue Ticks Wait"),STAT_QueueTicksWait,STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("Queue Tick Task"),STAT_QueueTickTask,STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("Post Queue Tick Task"),STAT_PostTickTask,STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("Cooldown Dequeuing"),STAT_DequeueCooldowns,STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("Gather Ticks for Parallel"),STAT_GatherTicksForParallel,STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("Finalize Parallel Queue"),STAT_FinalizeParallelQueue,STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("Schedule cooldowns"),STAT_ScheduleCooldowns,STATGROUP_Game);
DECLARE_DWORD_COUNTER_STAT(TEXT("Ticks Queued"),STAT_TicksQueued,STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("TG_NewlySpawned"), STAT_TG_NewlySpawned, STATGROUP_TickGroups);
DECLARE_CYCLE_STAT(TEXT("ReleaseTickGroup"), STAT_ReleaseTickGroup, STATGROUP_TickGroups);
DECLARE_CYCLE_STAT(TEXT("ReleaseTickGroup Block"), STAT_ReleaseTickGroup_Block, STATGROUP_TickGroups);
DECLARE_CYCLE_STAT(TEXT("CleanupTasksWait"), STAT_CleanupTasksWait, STATGROUP_TickGroups);

static TAutoConsoleVariable<float> CVarStallStartFrame(
	TEXT("CriticalPathStall.TickStartFrame"),
	0.0f,
	TEXT("Sleep for the given time in start frame. Time is given in ms. This is a debug option used for critical path analysis and forcing a change in the critical path."));

static TAutoConsoleVariable<int32> CVarLogTicks(
	TEXT("tick.LogTicks"),
	0,
	TEXT("Spew ticks for debugging."));

static TAutoConsoleVariable<int32> CVarLogTicksShowPrerequistes(
	TEXT("tick.ShowPrerequistes"),
	1,
	TEXT("When logging ticks, show the prerequistes; debugging."));

static TAutoConsoleVariable<int32> CVarAllowAsyncComponentTicks(
	TEXT("tick.AllowAsyncComponentTicks"),
	1,
	TEXT("Used to control async component ticks."));

static TAutoConsoleVariable<int32> CVarAllowConcurrentQueue(
	TEXT("tick.AllowConcurrentTickQueue"),
	1,
	TEXT("If true, queue ticks concurrently."));

static TAutoConsoleVariable<int32> CVarAllowAsyncTickDispatch(
	TEXT("tick.AllowAsyncTickDispatch"),
	0,
	TEXT("If true, ticks are dispatched in a task thread."));

static TAutoConsoleVariable<int32> CVarAllowAsyncTickCleanup(
	TEXT("tick.AllowAsyncTickCleanup"),
	1,
	TEXT("If true, ticks are cleaned up in a task thread."));

FAutoConsoleTaskPriority CPrio_DispatchTaskPriority(
	TEXT("TaskGraph.TaskPriorities.TickDispatchTaskPriority"),
	TEXT("Task and thread priority for tick tasks dispatch."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);

FAutoConsoleTaskPriority CPrio_CleanupTaskPriority(
	TEXT("TaskGraph.TaskPriorities.TickCleanupTaskPriority"),
	TEXT("Task and thread priority for tick cleanup."),
	ENamedThreads::BackgroundThreadPriority, // if we have background priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::NormalTaskPriority // if we don't have background threads, then use normal priority threads at normal task priority instead
	);

FAutoConsoleTaskPriority CPrio_NormalAsyncTickTaskPriority(
	TEXT("TaskGraph.TaskPriorities.NormalAsyncTickTaskPriority"),
	TEXT("Task and thread priority for async ticks that are not high priority."),
	ENamedThreads::NormalThreadPriority,
	ENamedThreads::NormalTaskPriority
	);

FAutoConsoleTaskPriority CPrio_HiPriAsyncTickTaskPriority(
	TEXT("TaskGraph.TaskPriorities.HiPriAsyncTickTaskPriority"),
	TEXT("Task and thread priority for async ticks that are high priority."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);


FORCEINLINE bool CanDemoteIntoTickGroup(ETickingGroup TickGroup)
{
	switch (TickGroup)
	{
		case TG_StartPhysics:
		case TG_EndPhysics:
			return false;
	}
	return true;
}

template<typename InElementType, typename InAllocator = FDefaultAllocator>
class TArrayWithThreadsafeAdd : public TArray<InElementType, InAllocator>
{
public:
	typedef InElementType ElementType;
	typedef InAllocator   Allocator;

	template <typename... ArgsType>
	int32 EmplaceThreadsafe(ArgsType&&... Args)
	{
		const int32 Index = AddUninitializedThreadsafe(1);
		new(this->GetData() + Index) ElementType(Forward<ArgsType>(Args)...);
		return Index;
	}


	/**
	 * Adds a given number of uninitialized elements into the array using an atomic increment on the array num
	 *
	 * Caution, the array must have sufficient slack or this will assert/crash. You must presize the array.
	 *
	 * Caution, AddUninitialized() will create elements without calling
	 * the constructor and this is not appropriate for element types that
	 * require a constructor to function properly.
	 *
	 * @param Count Number of elements to add.
	 *
	 * @returns Number of elements in array before addition.
	 */
	int32 AddUninitializedThreadsafe(int32 Count = 1)
	{
		checkSlow(Count >= 0);
		const int32 OldNum = FPlatformAtomics::InterlockedAdd(&this->ArrayNum, Count);
		check(OldNum + Count <= this->ArrayMax);
		return OldNum;
	}

	/**
	 * Adds a new item to the end of the array, using atomics to update the current size of the array
	 *
	 * Caution, the array must have sufficient slack or this will assert/crash. You must presize the array.
	 *
	 * @param Item	The item to add
	 * @return		Index to the new item
	 */
	FORCEINLINE int32 AddThreadsafe(const ElementType& Item)
	{
		return EmplaceThreadsafe(Item);
	}

};


struct FTickContext
{
	/** Delta time to tick **/
	float					DeltaSeconds;
	/** Tick type **/
	ELevelTick				TickType;
	/** Tick type **/
	ETickingGroup			TickGroup;
	/** Current or desired thread **/
	ENamedThreads::Type		Thread;
	/** The world in which the object being ticked is contained. **/
	UWorld*					World;

	FTickContext(float InDeltaSeconds = 0.0f, ELevelTick InTickType = LEVELTICK_All, ETickingGroup InTickGroup = TG_PrePhysics, ENamedThreads::Type InThread = ENamedThreads::GameThread)
		: DeltaSeconds(InDeltaSeconds)
		, TickType(InTickType)
		, TickGroup(InTickGroup)
		, Thread(InThread)
		, World(nullptr)
	{
	}

	FTickContext(const FTickContext& In)
		: DeltaSeconds(In.DeltaSeconds)
		, TickType(In.TickType)
		, TickGroup(In.TickGroup)
		, Thread(In.Thread)
		, World(In.World)
	{
	}
	void operator=(const FTickContext& In)
	{
		DeltaSeconds = In.DeltaSeconds;
		TickType = In.TickType;
		TickGroup = In.TickGroup;
		Thread = In.Thread;
		World = In.World;
	}
};

/**
 * Class that handles the actual tick tasks and starting and completing tick groups
 */
/** Helper class define the task of ticking a component **/
class FTickFunctionTask
{
	/** Actor to tick **/
	FTickFunction*			Target;
	/** tick context, here thread is desired execution thread **/
	FTickContext			Context;
	/** If true, log each tick **/
	bool					bLogTick;
	/** If true, log prereqs **/
	bool					bLogTicksShowPrerequistes;
public:
	/** Constructor
		* @param InTarget - Function to tick
		* @param InContext - context to tick in, here thread is desired execution thread
	**/
	FORCEINLINE FTickFunctionTask(FTickFunction* InTarget, const FTickContext* InContext, bool InbLogTick, bool bInLogTicksShowPrerequistes)
		: Target(InTarget)
		, Context(*InContext)
		, bLogTick(InbLogTick)
	, bLogTicksShowPrerequistes(bInLogTicksShowPrerequistes)
	{
	}
	static FORCEINLINE TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FTickFunctionTask, STATGROUP_TaskGraphTasks);
	}
	/** return the thread for this task **/
	FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return Context.Thread;
	}
	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::TrackSubsequents;
	}
	/**
		*	Actually execute the tick.
		*	@param	CurrentThread; the thread we are running on
		*	@param	MyCompletionGraphEvent; my completion event. Not always useful since at the end of DoWork, you can assume you are done and hence further tasks do not need you as a prerequisite.
		*	However, MyCompletionGraphEvent can be useful for passing to other routines or when it is handy to set up subsequents before you actually do work.
		**/
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		if (bLogTick)
		{
			UE_LOG(LogTick, Log, TEXT("tick %s [%1d, %1d] %6llu %2d %s"), Target->bHighPriority ? TEXT("*") : TEXT(" "), (int32)Target->GetActualTickGroup(), (int32)Target->GetActualEndTickGroup(), (uint64)GFrameCounter, (int32)CurrentThread, *Target->DiagnosticMessage());
			if (bLogTicksShowPrerequistes)
			{
				Target->ShowPrerequistes();
			}
		}
		if (Target->IsTickFunctionEnabled())
		{
#if DO_TIMEGUARD
			FTimerNameDelegate NameFunction = FTimerNameDelegate::CreateLambda( [&]{ return FString::Printf(TEXT("Slowtick %s "), *Target->DiagnosticMessage()); } );
			SCOPE_TIME_GUARD_DELEGATE_MS(NameFunction, 4);
#endif

			Target->ExecuteTick(Target->CalculateDeltaTime(Context), Context.TickType, CurrentThread, MyCompletionGraphEvent);
		}
		Target->TaskPointer = nullptr;  // This is stale and a good time to clear it for safety
	}
};


/**
 * Class that handles the actual tick tasks and starting and completing tick groups
 */
class FTickTaskSequencer
{
	/**
	 * Class that handles dispatching a tick group
	 */
	class FDipatchTickGroupTask
	{
		/** Sequencer to proxy to **/
		FTickTaskSequencer &TTS;
		/** Tick group to dispatch **/
		ETickingGroup WorldTickGroup;
	public:
		/** Constructor
			* @param InTarget - Function to tick
			* @param InContext - context to tick in, here thread is desired execution thread
		**/
		FORCEINLINE FDipatchTickGroupTask(FTickTaskSequencer &InTTS, ETickingGroup InWorldTickGroup)
			: TTS(InTTS)
			, WorldTickGroup(InWorldTickGroup)
		{
		}
		static FORCEINLINE TStatId GetStatId()
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FDipatchTickGroupTask, STATGROUP_TaskGraphTasks);
		}
		static FORCEINLINE ENamedThreads::Type GetDesiredThread()
		{
			return CPrio_DispatchTaskPriority.Get();
		}
		static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode()
		{
			return ESubsequentsMode::TrackSubsequents;
		}
		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
		{
			TTS.DispatchTickGroup(CurrentThread, WorldTickGroup);
		}
	};
	/**
	 * Class that handles Reset a tick group
	 */
	class FResetTickGroupTask
	{
		/** Sequencer to proxy to **/
		FTickTaskSequencer &TTS;
		/** Tick group to dispatch **/
		ETickingGroup WorldTickGroup;
	public:
		/** Constructor
			* @param InTarget - Function to tick
			* @param InContext - context to tick in, here thread is desired execution thread
		**/
		FORCEINLINE FResetTickGroupTask(FTickTaskSequencer &InTTS, ETickingGroup InWorldTickGroup)
			: TTS(InTTS)
			, WorldTickGroup(InWorldTickGroup)
		{
		}
		static FORCEINLINE TStatId GetStatId()
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FResetTickGroupTask, STATGROUP_TaskGraphTasks);
		}
		static FORCEINLINE ENamedThreads::Type GetDesiredThread()
		{
			return CPrio_CleanupTaskPriority.Get();
		}
		FORCEINLINE static ESubsequentsMode::Type GetSubsequentsMode()
		{
			return ESubsequentsMode::TrackSubsequents;
		}
		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
		{
			TTS.ResetTickGroup(WorldTickGroup);
		}
	};

	/** Completion handles for each phase of ticks */
	TArrayWithThreadsafeAdd<FGraphEventRef, TInlineAllocator<4> > TickCompletionEvents[TG_MAX];

	/** HiPri Held tasks for each tick group. */
	TArrayWithThreadsafeAdd<TGraphTask<FTickFunctionTask>*> HiPriTickTasks[TG_MAX][TG_MAX];

	/** LowPri Held tasks for each tick group. */
	TArrayWithThreadsafeAdd<TGraphTask<FTickFunctionTask>*> TickTasks[TG_MAX][TG_MAX];

	/** These are waited for at the end of the frame; they are not on the critical path, but they have to be done before we leave the frame. */
	FGraphEventArray CleanupTasks;

	/** we keep track of the last TG we have blocked for so when we do block, we know which TG's to wait for . */
	ETickingGroup WaitForTickGroup;

	/** If true, allow concurrent ticks **/
	bool				bAllowConcurrentTicks;

	/** If true, log each tick **/
	bool				bLogTicks;
	/** If true, log each tick **/
	bool				bLogTicksShowPrerequistes;

public:

	/**
	 * Singleton to retrieve the global tick task sequencer
	 * @return Reference to the global tick task sequencer
	**/
	static FTickTaskSequencer& Get()
	{
		static FTickTaskSequencer SingletonInstance;
		return SingletonInstance;
	}
	/**
	 * Return true if we should be running in single threaded mode, ala dedicated server
	**/
	FORCEINLINE static bool SingleThreadedMode()
	{
		if (!FApp::ShouldUseThreadingForPerformance() || IsRunningDedicatedServer() || FPlatformMisc::NumberOfCores() < 3 || !FPlatformProcess::SupportsMultithreading())
		{
			return true;
		}
		return false;
	}
	/**
	 * Start a component tick task
	 *
	 * @param	InPrerequisites - prerequisites that must be completed before this tick can begin
	 * @param	TickFunction - the tick function to queue
	 * @param	Context - tick context to tick in. Thread here is the current thread.
	 */
	FORCEINLINE void StartTickTask(const FGraphEventArray* Prerequisites, FTickFunction* TickFunction, const FTickContext& TickContext)
	{
		checkSlow(TickFunction->ActualStartTickGroup >=0 && TickFunction->ActualStartTickGroup < TG_MAX);

		FTickContext UseContext = TickContext;

		bool bIsOriginalTickGroup = (TickFunction->ActualStartTickGroup == TickFunction->TickGroup);

		if (TickFunction->bRunOnAnyThread && bAllowConcurrentTicks && bIsOriginalTickGroup)
		{
			if (TickFunction->bHighPriority)
			{
				UseContext.Thread = CPrio_HiPriAsyncTickTaskPriority.Get();
			}
			else
			{
				UseContext.Thread = CPrio_NormalAsyncTickTaskPriority.Get();
			}
		}
		else
		{
			UseContext.Thread = ENamedThreads::SetTaskPriority(ENamedThreads::GameThread, TickFunction->bHighPriority ? ENamedThreads::HighTaskPriority : ENamedThreads::NormalTaskPriority);
		}

		TickFunction->TaskPointer = TGraphTask<FTickFunctionTask>::CreateTask(Prerequisites, TickContext.Thread).ConstructAndHold(TickFunction, &UseContext, bLogTicks, bLogTicksShowPrerequistes);
	}

	/** Add a completion handle to a tick group **/
	FORCEINLINE void AddTickTaskCompletion(ETickingGroup StartTickGroup, ETickingGroup EndTickGroup, TGraphTask<FTickFunctionTask>* Task, bool bHiPri)
	{
		checkSlow(StartTickGroup >=0 && StartTickGroup < TG_MAX && EndTickGroup >=0 && EndTickGroup < TG_MAX && StartTickGroup <= EndTickGroup);
		if (bHiPri)
		{
			HiPriTickTasks[StartTickGroup][EndTickGroup].Add(Task);
		}
		else
		{
			TickTasks[StartTickGroup][EndTickGroup].Add(Task);
		}
		new (TickCompletionEvents[EndTickGroup]) FGraphEventRef(Task->GetCompletionEvent());
	}
	/** Add a completion handle to a tick group, parallel version **/
	FORCEINLINE void AddTickTaskCompletionParallel(ETickingGroup StartTickGroup, ETickingGroup EndTickGroup, TGraphTask<FTickFunctionTask>* Task, bool bHiPri)
	{
		check(StartTickGroup >= 0 && StartTickGroup < TG_NewlySpawned && EndTickGroup >= 0 && EndTickGroup < TG_NewlySpawned && StartTickGroup <= EndTickGroup);
		if (bHiPri)
		{
			HiPriTickTasks[StartTickGroup][EndTickGroup].AddThreadsafe(Task);
		}
		else
		{
			TickTasks[StartTickGroup][EndTickGroup].AddThreadsafe(Task);
		}
		TickCompletionEvents[EndTickGroup].AddThreadsafe(Task->GetCompletionEvent());
	}
	/** Set up the lists for AddTickTaskCompletionParallel, since we are using AddThreadsafe, we need to presize the arrays **/
	void SetupAddTickTaskCompletionParallel(int32 NumTicks)
	{
		for (int32 TickGroup = 0; TickGroup < TG_MAX; TickGroup++)
		{
			for (int32 EndTickGroup = 0; EndTickGroup < TG_MAX; EndTickGroup++)
			{
				HiPriTickTasks[TickGroup][EndTickGroup].Reserve(NumTicks);
				TickTasks[TickGroup][EndTickGroup].Reserve(NumTicks);
			}
			TickCompletionEvents[TickGroup].Reserve(NumTicks);
		}
	}
	/**
	 * Start a component tick task and add the completion handle
	 *
	 * @param	InPrerequisites - prerequisites that must be completed before this tick can begin
	 * @param	TickFunction - the tick function to queue
	 * @param	Context - tick context to tick in. Thread here is the current thread.
	 */
	FORCEINLINE void QueueTickTask(const FGraphEventArray* Prerequisites, FTickFunction* TickFunction, const FTickContext& TickContext)
	{
		checkSlow(TickContext.Thread == ENamedThreads::GameThread);
		StartTickTask(Prerequisites, TickFunction, TickContext);
		TGraphTask<FTickFunctionTask>* Task = (TGraphTask<FTickFunctionTask>*)TickFunction->TaskPointer;
		AddTickTaskCompletion(TickFunction->ActualStartTickGroup, TickFunction->ActualEndTickGroup, Task, TickFunction->bHighPriority);
	}

	/**
	 * Start a component tick task and add the completion handle
	 *
	 * @param	InPrerequisites - prerequisites that must be completed before this tick can begin
	 * @param	TickFunction - the tick function to queue
	 * @param	Context - tick context to tick in. Thread here is the current thread.
	 */
	FORCEINLINE void QueueTickTaskParallel(const FGraphEventArray* Prerequisites, FTickFunction* TickFunction, const FTickContext& TickContext)
	{
		checkSlow(TickContext.Thread == ENamedThreads::GameThread);
		StartTickTask(Prerequisites, TickFunction, TickContext);
		TGraphTask<FTickFunctionTask>* Task = (TGraphTask<FTickFunctionTask>*)TickFunction->TaskPointer;
		AddTickTaskCompletionParallel(TickFunction->ActualStartTickGroup, TickFunction->ActualEndTickGroup, Task, TickFunction->bHighPriority);
	}

	/**
	 * Release the queued ticks for a given tick group and process them.
	 * @param WorldTickGroup - tick group to release
	 * @param bBlockTillComplete - if true, do not return until all ticks are complete
	**/
	void ReleaseTickGroup(ETickingGroup WorldTickGroup, bool bBlockTillComplete)
	{
		if (bLogTicks)
		{
			UE_LOG(LogTick, Log, TEXT("tick %6llu ---------------------------------------- Release tick group %d"),(uint64)GFrameCounter, (int32)WorldTickGroup);
		}
		checkSlow(WorldTickGroup >= 0 && WorldTickGroup < TG_MAX);

		{
			SCOPE_CYCLE_COUNTER(STAT_ReleaseTickGroup);
			if (SingleThreadedMode() || CVarAllowAsyncTickDispatch.GetValueOnGameThread() == 0)
			{
				DispatchTickGroup(ENamedThreads::GameThread, WorldTickGroup);
			}
			else
			{
				// dispatch the tick group on another thread, that way, the game thread can be processing ticks while ticks are being queued by another thread
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(
					TGraphTask<FDipatchTickGroupTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(*this, WorldTickGroup));
			}
		}

		if (bBlockTillComplete || SingleThreadedMode())
		{
			SCOPE_CYCLE_COUNTER(STAT_ReleaseTickGroup_Block);
			for (ETickingGroup Block = WaitForTickGroup; Block <= WorldTickGroup; Block = ETickingGroup(Block + 1))
			{
				CA_SUPPRESS(6385);
				if (TickCompletionEvents[Block].Num())
				{
					FTaskGraphInterface::Get().WaitUntilTasksComplete(TickCompletionEvents[Block], ENamedThreads::GameThread);
					if (SingleThreadedMode() || Block == TG_NewlySpawned || CVarAllowAsyncTickCleanup.GetValueOnGameThread() == 0)
					{
						ResetTickGroup(Block);
					}
					else
					{
						DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.ResetTickGroup"), STAT_FDelegateGraphTask_ResetTickGroup, STATGROUP_TaskGraphTasks);
						CleanupTasks.Add(TGraphTask<FResetTickGroupTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(*this, Block));
					}
				}
			}
			WaitForTickGroup = ETickingGroup(WorldTickGroup + (WorldTickGroup == TG_NewlySpawned ? 0 : 1)); // don't advance for newly spawned
		}
		else
		{
			// since this is used to soak up some async time for another task (physics), we should process whatever we have now
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			check(WorldTickGroup + 1 < TG_MAX && WorldTickGroup != TG_NewlySpawned); // you must block on the last tick group! And we must block on newly spawned
		}
	}

	/**
	 * Resets the internal state of the object at the start of a frame
	 */
	void StartFrame()
	{
		bLogTicks = !!CVarLogTicks.GetValueOnGameThread();
		bLogTicksShowPrerequistes = !!CVarLogTicksShowPrerequistes.GetValueOnGameThread();

		if (bLogTicks)
		{
			UE_LOG(LogTick, Log, TEXT("tick %6llu ---------------------------------------- Start Frame"),(uint64)GFrameCounter);
		}

		if (SingleThreadedMode())
		{
			bAllowConcurrentTicks = false;
		}
		else
		{
			bAllowConcurrentTicks = !!CVarAllowAsyncComponentTicks.GetValueOnGameThread();
		}

		WaitForCleanup();

		for (int32 Index = 0; Index < TG_MAX; Index++)
		{
			check(!TickCompletionEvents[Index].Num());  // we should not be adding to these outside of a ticking proper and they were already cleared after they were ticked
			TickCompletionEvents[Index].Reset();
			for (int32 IndexInner = 0; IndexInner < TG_MAX; IndexInner++)
			{
				check(!TickTasks[Index][IndexInner].Num() && !HiPriTickTasks[Index][IndexInner].Num());  // we should not be adding to these outside of a ticking proper and they were already cleared after they were ticked
				TickTasks[Index][IndexInner].Reset();
				HiPriTickTasks[Index][IndexInner].Reset();
			}
		}
		WaitForTickGroup = (ETickingGroup)0;
	}
	/**
	 * Checks that everything is clean at the end of a frame
	 */
	void EndFrame()
	{
		if (bLogTicks)
		{
			UE_LOG(LogTick, Log, TEXT("tick %6llu ---------------------------------------- End Frame"),(uint64)GFrameCounter);
		}
	}
private:

	FTickTaskSequencer()
		: bAllowConcurrentTicks(false)
		, bLogTicks(false)
		, bLogTicksShowPrerequistes(false)
	{
		TFunction<void()> ShutdownCallback([this](){WaitForCleanup();});
		FTaskGraphInterface::Get().AddShutdownCallback(ShutdownCallback);
	}

	~FTickTaskSequencer()
	{
		// Need to clean up oustanding tasks before we destroy this data structure.
		// Typically it is already gone because the task graph shutdown first.
		WaitForCleanup();
	}

	void WaitForCleanup()
	{
		if (CleanupTasks.Num() > 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_CleanupTasksWait);
			FTaskGraphInterface::Get().WaitUntilTasksComplete(CleanupTasks, ENamedThreads::GameThread);
			CleanupTasks.Reset();
		}
	}

	void ResetTickGroup(ETickingGroup WorldTickGroup)
	{
		TickCompletionEvents[WorldTickGroup].Reset();
	}

	void DispatchTickGroup(ENamedThreads::Type CurrentThread, ETickingGroup WorldTickGroup)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_DispatchTickGroup);
		for (int32 IndexInner = 0; IndexInner < TG_MAX; IndexInner++)
		{
			TArray<TGraphTask<FTickFunctionTask>*>& TickArray = HiPriTickTasks[WorldTickGroup][IndexInner];
			if (IndexInner < WorldTickGroup)
			{
				check(TickArray.Num() == 0); // makes no sense to have and end TG before the start TG
			}
			else
			{
				for (int32 Index = 0; Index < TickArray.Num(); Index++)
				{
					TickArray[Index]->Unlock(CurrentThread);
				}
			}
			TickArray.Reset();
		}
		for (int32 IndexInner = 0; IndexInner < TG_MAX; IndexInner++)
		{
			TArray<TGraphTask<FTickFunctionTask>*>& TickArray = TickTasks[WorldTickGroup][IndexInner];
			if (IndexInner < WorldTickGroup)
			{
				check(TickArray.Num() == 0); // makes no sense to have and end TG before the start TG
			}
			else
			{
				for (int32 Index = 0; Index < TickArray.Num(); Index++)
				{
					TickArray[Index]->Unlock(CurrentThread);
				}
			}
			TickArray.Reset();
		}
	}

};


class FTickTaskLevel
{
public:
	/** Constructor, grabs, the sequencer singleton **/
	FTickTaskLevel()
		: TickTaskSequencer(FTickTaskSequencer::Get())
		, bTickNewlySpawned(false)
	{
	}
	~FTickTaskLevel()
	{
		for (TSet<FTickFunction*>::TIterator It(AllEnabledTickFunctions); It; ++It)
		{
			(*It)->bRegistered = false;
		}
		for (TSet<FTickFunction*>::TIterator It(AllDisabledTickFunctions); It; ++It)
		{
			(*It)->bRegistered = false;
		}
		FTickFunction* CoolingDownNode = AllCoolingDownTickFunctions.Head;
		while (CoolingDownNode)
		{
			CoolingDownNode->bRegistered = false;
			CoolingDownNode = CoolingDownNode->Next;
		}
	}

	/**
	 * Queues the ticks for this level
	 * @param InContext - information about the tick
	 * @return the total number of ticks we will be ticking
	 */
	int32 StartFrame(const FTickContext& InContext)
	{
		check(!NewlySpawnedTickFunctions.Num()); // There shouldn't be any in here at this point in the frame
		Context.TickGroup = ETickingGroup(0); // reset this to the start tick group
		Context.DeltaSeconds = InContext.DeltaSeconds;
		Context.TickType = InContext.TickType;
		Context.Thread = ENamedThreads::GameThread;
		Context.World = InContext.World;
		bTickNewlySpawned = true;

		int32 CooldownTicksEnabled = 0;
		{
			SCOPE_CYCLE_COUNTER(STAT_DequeueCooldowns);
			// Determine which cooled down ticks will be enabled this frame
			float CumulativeCooldown = 0.f;
			FTickFunction* TickFunction = AllCoolingDownTickFunctions.Head;
			while (TickFunction)
			{
				if (CumulativeCooldown + TickFunction->RelativeTickCooldown >= Context.DeltaSeconds)
				{
					TickFunction->RelativeTickCooldown -= (Context.DeltaSeconds - CumulativeCooldown);
					break;
				}
				CumulativeCooldown += TickFunction->RelativeTickCooldown;

				TickFunction->TickState = FTickFunction::ETickState::Enabled;
				TickFunction = TickFunction->Next;
				++CooldownTicksEnabled;
			}
		}

		return AllEnabledTickFunctions.Num() + CooldownTicksEnabled;
	}

	/**
	 * Queues the ticks for this level
	 * @param InContext - information about the tick
	 */
	void StartFrameParallel(const FTickContext& InContext, TArray<FTickFunction*>& AllTickFunctions)
	{
		check(!NewlySpawnedTickFunctions.Num()); // There shouldn't be any in here at this point in the frame
		Context.TickGroup = ETickingGroup(0); // reset this to the start tick group
		Context.DeltaSeconds = InContext.DeltaSeconds;
		Context.TickType = InContext.TickType;
		Context.Thread = ENamedThreads::GameThread;
		Context.World = InContext.World;
		bTickNewlySpawned = true;

		{
			SCOPE_CYCLE_COUNTER(STAT_DequeueCooldowns);
			// Determine which cooled down ticks will be enabled this frame
			float CumulativeCooldown = 0.f;
			FTickFunction* TickFunction = AllCoolingDownTickFunctions.Head;
			while (TickFunction)
			{
				if (CumulativeCooldown + TickFunction->RelativeTickCooldown >= Context.DeltaSeconds)
				{
					TickFunction->RelativeTickCooldown -= (Context.DeltaSeconds - CumulativeCooldown);
					break;
				}
				CumulativeCooldown += TickFunction->RelativeTickCooldown;

				TickFunction->TickState = FTickFunction::ETickState::Enabled;
				TickFunction->bWasInterval = true;
				AllTickFunctions.Add(TickFunction);

				TickFunctionsToReschedule.Add(FTickScheduleDetails(TickFunction, TickFunction->TickInterval - (Context.DeltaSeconds - CumulativeCooldown))); // Give credit for any overrun

				AllCoolingDownTickFunctions.Head = TickFunction->Next;
				TickFunction = TickFunction->Next;
			}
		}
		{
			SCOPE_CYCLE_COUNTER(STAT_GatherTicksForParallel);
			for (TSet<FTickFunction*>::TIterator It(AllEnabledTickFunctions); It; ++It)
			{
				FTickFunction* TickFunction = *It;
				AllTickFunctions.Add(TickFunction);
			}
		}
	}

	struct FTickScheduleDetails
	{
		FTickScheduleDetails(FTickFunction* InTickFunction, const float InCooldown, bool bInDeferredRemove = false)
			: TickFunction(InTickFunction)
			, Cooldown(InCooldown)
			, bDeferredRemove(bInDeferredRemove)
		{}

		FTickFunction* TickFunction;
		float Cooldown;
		bool bDeferredRemove;
	};

	void RemoveAndRescheduleForInterval(FTickFunction* TickFunction)
	{
		verify(AllEnabledTickFunctions.Remove(TickFunction) == 1);
		TickFunctionsToReschedule.Add(FTickScheduleDetails(TickFunction, TickFunction->TickInterval));
	}
	void RescheduleForIntervalParallel(FTickFunction* TickFunction)
	{
		// note we do the remove later!
		TickFunctionsToReschedule.AddThreadsafe(FTickScheduleDetails(TickFunction, TickFunction->TickInterval, true));
	}
	/* Puts a TickFunction in to the cooldown state*/
	void ReserveTickFunctionCooldowns(int32 NumToReserve)
	{
		TickFunctionsToReschedule.Reserve(NumToReserve);
	}
	/* Puts a TickFunction in to the cooldown state*/
	void ScheduleTickFunctionCooldowns()
	{
		if (TickFunctionsToReschedule.Num() > 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_ScheduleCooldowns);

			TickFunctionsToReschedule.Sort([](const FTickScheduleDetails& A, const FTickScheduleDetails& B)
			{
				return A.Cooldown < B.Cooldown;
			});

			int32 RescheduleIndex = 0;
			float CumulativeCooldown = 0.f;
			FTickFunction* PrevComparisonTickFunction = nullptr;
			FTickFunction* ComparisonTickFunction = AllCoolingDownTickFunctions.Head;
			while (ComparisonTickFunction && RescheduleIndex < TickFunctionsToReschedule.Num())
			{
				const float CooldownTime = TickFunctionsToReschedule[RescheduleIndex].Cooldown;
				if ((CumulativeCooldown + ComparisonTickFunction->RelativeTickCooldown) > CooldownTime)
				{
					FTickFunction* TickFunction = TickFunctionsToReschedule[RescheduleIndex].TickFunction;
					if (TickFunction->TickState != FTickFunction::ETickState::Disabled)
					{
						if (TickFunctionsToReschedule[RescheduleIndex].bDeferredRemove)
						{
							verify(AllEnabledTickFunctions.Remove(TickFunction) == 1);
						}
						TickFunction->TickState = FTickFunction::ETickState::CoolingDown;
						TickFunction->RelativeTickCooldown = CooldownTime - CumulativeCooldown;

						if (PrevComparisonTickFunction)
						{
							PrevComparisonTickFunction->Next = TickFunction;
						}
						else
						{
							check(ComparisonTickFunction == AllCoolingDownTickFunctions.Head);
							AllCoolingDownTickFunctions.Head = TickFunction;
						}
						TickFunction->Next = ComparisonTickFunction;
						PrevComparisonTickFunction = TickFunction;
						ComparisonTickFunction->RelativeTickCooldown -= TickFunction->RelativeTickCooldown;
						CumulativeCooldown += TickFunction->RelativeTickCooldown;
					}
					++RescheduleIndex;
				}
				else
				{
					CumulativeCooldown += ComparisonTickFunction->RelativeTickCooldown;
					PrevComparisonTickFunction = ComparisonTickFunction;
					ComparisonTickFunction = ComparisonTickFunction->Next;
				}
			}
			for ( ; RescheduleIndex < TickFunctionsToReschedule.Num(); ++RescheduleIndex)
			{
				FTickFunction* TickFunction = TickFunctionsToReschedule[RescheduleIndex].TickFunction;
				checkSlow(TickFunction);
				if (TickFunction->TickState != FTickFunction::ETickState::Disabled)
				{
					if (TickFunctionsToReschedule[RescheduleIndex].bDeferredRemove)
					{
						verify(AllEnabledTickFunctions.Remove(TickFunction) == 1);
					}
					const float CooldownTime = TickFunctionsToReschedule[RescheduleIndex].Cooldown;

					TickFunction->TickState = FTickFunction::ETickState::CoolingDown;
					TickFunction->RelativeTickCooldown = CooldownTime - CumulativeCooldown;

					TickFunction->Next = nullptr;
					if (PrevComparisonTickFunction)
					{
						PrevComparisonTickFunction->Next = TickFunction;
					}
					else
					{
						check(ComparisonTickFunction == AllCoolingDownTickFunctions.Head);
						AllCoolingDownTickFunctions.Head = TickFunction;
					}
					PrevComparisonTickFunction = TickFunction;

					CumulativeCooldown += TickFunction->RelativeTickCooldown;
				}
			}
			TickFunctionsToReschedule.Reset();
		}
	}

	/* Queue all tick functions for execution */
	void QueueAllTicks()
	{
		FTickTaskSequencer& TTS = FTickTaskSequencer::Get();
		for (TSet<FTickFunction*>::TIterator It(AllEnabledTickFunctions); It; ++It)
		{
			FTickFunction* TickFunction = *It;
			TickFunction->QueueTickFunction(TTS, Context);

			if (TickFunction->TickInterval > 0.f)
			{
				It.RemoveCurrent();
				TickFunctionsToReschedule.Add(FTickScheduleDetails(TickFunction, TickFunction->TickInterval));
			}
		}
		int32 EnabledCooldownTicks = 0;
		float CumulativeCooldown = 0.f;
		while (FTickFunction* TickFunction = AllCoolingDownTickFunctions.Head)
		{
			if (TickFunction->TickState == FTickFunction::ETickState::Enabled)
			{
				CumulativeCooldown += TickFunction->RelativeTickCooldown;
				TickFunction->QueueTickFunction(TTS, Context);
				TickFunctionsToReschedule.Add(FTickScheduleDetails(TickFunction, TickFunction->TickInterval - (Context.DeltaSeconds - CumulativeCooldown))); // Give credit for any overrun
				AllCoolingDownTickFunctions.Head = TickFunction->Next;
			}
			else
			{
				break;
			}
		}

		ScheduleTickFunctionCooldowns();
	}
	/**
	 * Queues the newly spawned ticks for this level
	 * @return - the number of items
	 */
	int32 QueueNewlySpawned(ETickingGroup CurrentTickGroup)
	{
		Context.TickGroup = CurrentTickGroup;
		int32 Num = 0;
		FTickTaskSequencer& TTS = FTickTaskSequencer::Get();
		for (TSet<FTickFunction*>::TIterator It(NewlySpawnedTickFunctions); It; ++It)
		{
			FTickFunction* TickFunction = *It;
			TickFunction->QueueTickFunction(TTS, Context);
			Num++;

			if (TickFunction->TickInterval > 0.f)
			{
				AllEnabledTickFunctions.Remove(TickFunction);
				TickFunctionsToReschedule.Add(FTickScheduleDetails(TickFunction, TickFunction->TickInterval));
			}
		}
		ScheduleTickFunctionCooldowns();
		NewlySpawnedTickFunctions.Empty();
		return Num;
	}
	/**
	 * If there is infinite recursive spawning, log that and discard them
	 */
	void LogAndDisardRunawayNewlySpawned(ETickingGroup CurrentTickGroup)
	{
		Context.TickGroup = CurrentTickGroup;
		FTickTaskSequencer& TTS = FTickTaskSequencer::Get();
		for (TSet<FTickFunction*>::TIterator It(NewlySpawnedTickFunctions); It; ++It)
		{
			FTickFunction* TickFunction = *It;
			UE_LOG(LogTick, Error, TEXT("Could not tick newly spawned in 100 iterations; runaway recursive spawing. Tick is %s."), *TickFunction->DiagnosticMessage());

			if (TickFunction->TickInterval > 0.f)
			{
				AllEnabledTickFunctions.Remove(TickFunction);
				TickFunctionsToReschedule.Add(FTickScheduleDetails(TickFunction, TickFunction->TickInterval));
			}
		}
		ScheduleTickFunctionCooldowns();
		NewlySpawnedTickFunctions.Empty();
	}

	/**
	 * Run all of the ticks for a pause frame synchronously on the game thread.
	 * The capability of pause ticks are very limited. There are no dependencies or ordering or tick groups.
	 * @param InContext - information about the tick
	 */
	void RunPauseFrame(const FTickContext& InContext)
	{
		check(!NewlySpawnedTickFunctions.Num()); // There shouldn't be any in here at this point in the frame

		float CumulativeCooldown = 0.f;
		FTickFunction* PrevTickFunction = nullptr;
		FTickFunction* TickFunction = AllCoolingDownTickFunctions.Head;
		while (TickFunction)
		{
			CumulativeCooldown += TickFunction->RelativeTickCooldown;
			if (TickFunction->bTickEvenWhenPaused)
			{
				TickFunction->TaskPointer = nullptr; // this is stale, clear it out now
				if (CumulativeCooldown < InContext.DeltaSeconds)
				{
					TickFunction->TickVisitedGFrameCounter = GFrameCounter;
					TickFunction->TickQueuedGFrameCounter = GFrameCounter;
					TickFunction->ExecuteTick(TickFunction->CalculateDeltaTime(InContext), InContext.TickType, ENamedThreads::GameThread, FGraphEventRef());

					TickFunctionsToReschedule.Add(FTickScheduleDetails(TickFunction, TickFunction->TickInterval - (InContext.DeltaSeconds - CumulativeCooldown))); // Give credit for any overrun
				}
				else
				{
					TickFunctionsToReschedule.Add(FTickScheduleDetails(TickFunction, CumulativeCooldown - InContext.DeltaSeconds));
				}
				if (PrevTickFunction)
				{
					PrevTickFunction->Next = TickFunction->Next;
				}
				else
				{
					check(TickFunction == AllCoolingDownTickFunctions.Head);
					AllCoolingDownTickFunctions.Head = TickFunction->Next;
				}
				if (TickFunction->Next)
				{
					TickFunction->Next->RelativeTickCooldown += TickFunction->RelativeTickCooldown;
					CumulativeCooldown -= TickFunction->RelativeTickCooldown; // Since the next object in the list will have this cooldown included take it back out of the cumulative
				}
			}
			else
			{
				PrevTickFunction = TickFunction;
			}
			TickFunction = TickFunction->Next;
		}

		for (TSet<FTickFunction*>::TIterator It(AllEnabledTickFunctions); It; ++It)
		{
			TickFunction = *It;
			TickFunction->TaskPointer = nullptr; // this is stale, clear it out now
			if (TickFunction->bTickEvenWhenPaused && TickFunction->TickState == FTickFunction::ETickState::Enabled)
			{
				TickFunction->TickVisitedGFrameCounter = GFrameCounter;
				TickFunction->TickQueuedGFrameCounter = GFrameCounter;
				TickFunction->ExecuteTick(TickFunction->CalculateDeltaTime(InContext), InContext.TickType, ENamedThreads::GameThread, FGraphEventRef());

				if (TickFunction->TickInterval > 0.f)
				{
					It.RemoveCurrent();
					TickFunctionsToReschedule.Add(FTickScheduleDetails(TickFunction, TickFunction->TickInterval));
				}
			}
		}

		ScheduleTickFunctionCooldowns();

		check(!NewlySpawnedTickFunctions.Num()); // We don't support new spawns during pause ticks
	}

	/** End a tick frame **/
	void EndFrame()
	{
		bTickNewlySpawned = false;
		check(!NewlySpawnedTickFunctions.Num()); // hmmm, this might be ok, but basically anything that was added this late cannot be ticked until the next frame
	}
	// Interface that is private to FTickFunction

	/** Return true if this tick function is in the master list **/
	bool HasTickFunction(FTickFunction* TickFunction)
	{
		return AllEnabledTickFunctions.Contains(TickFunction) || AllDisabledTickFunctions.Contains(TickFunction) || AllCoolingDownTickFunctions.Contains(TickFunction);
	}

	/** Add the tick function to the master list **/
	void AddTickFunction(FTickFunction* TickFunction)
	{
		check(!HasTickFunction(TickFunction));
		if (TickFunction->TickState == FTickFunction::ETickState::Enabled)
		{
			AllEnabledTickFunctions.Add(TickFunction);
			if (bTickNewlySpawned)
			{
				NewlySpawnedTickFunctions.Add(TickFunction);
			}
		}
		else
		{
			check(TickFunction->TickState == FTickFunction::ETickState::Disabled);
			AllDisabledTickFunctions.Add(TickFunction);
		}
	}

	/** Dumps info about a tick function to output device. */
	FORCEINLINE void DumpTickFunction(FOutputDevice& Ar, FTickFunction* Function, UEnum* TickGroupEnum, const float RemainingCooldown = 0.f)
	{
		// Info about the function.
		Ar.Logf(TEXT("%s, %s, ActualStartTickGroup: %s, Prerequesities: %d"),
			*Function->DiagnosticMessage(),
			Function->IsTickFunctionEnabled() ? (RemainingCooldown > 0.f ? *FString::Printf(TEXT("Cooling Down for %.4g seconds"),RemainingCooldown) : TEXT("Enabled")) : TEXT("Disabled"),
			*TickGroupEnum->GetNameStringByValue(Function->ActualStartTickGroup),
			Function->Prerequisites.Num());

		// List all prerequisities
		for (int32 Index = 0; Index < Function->Prerequisites.Num(); ++Index)
		{
			FTickPrerequisite& Prerequisite = Function->Prerequisites[Index];
			if (Prerequisite.PrerequisiteObject.IsValid())
			{
				Ar.Logf(TEXT("    %s, %s"), *Prerequisite.PrerequisiteObject->GetFullName(), *Prerequisite.PrerequisiteTickFunction->DiagnosticMessage());
			}
			else
			{
				Ar.Logf(TEXT("    Invalid Prerequisite"));
			}
		}
	}

	/** Dumps all tick functions to output device. */
	void DumpAllTickFunctions(FOutputDevice& Ar, int32& EnabledCount, int32& DisabledCount, bool bEnabled, bool bDisabled)
	{
		UEnum* TickGroupEnum = CastChecked<UEnum>(StaticFindObject(UEnum::StaticClass(), ANY_PACKAGE, TEXT("ETickingGroup"), true));
		if (bEnabled)
		{
			for (TSet<FTickFunction*>::TIterator It(AllEnabledTickFunctions); It; ++It)
			{
				DumpTickFunction(Ar, *It, TickGroupEnum);
			}
			float CumulativeCooldown = 0.f;
			FTickFunction* TickFunction = AllCoolingDownTickFunctions.Head;
			while (TickFunction)
			{
				CumulativeCooldown += TickFunction->RelativeTickCooldown;
				DumpTickFunction(Ar, TickFunction, TickGroupEnum, CumulativeCooldown);
				TickFunction = TickFunction->Next;
				++EnabledCount;
			}
		}
		else
		{
			FTickFunction* TickFunction = AllCoolingDownTickFunctions.Head;
			while (TickFunction)
			{
				TickFunction = TickFunction->Next;
				++EnabledCount;
			}
		}
		EnabledCount += AllEnabledTickFunctions.Num();
		if (bDisabled)
		{
			for (TSet<FTickFunction*>::TIterator It(AllDisabledTickFunctions); It; ++It)
			{
				DumpTickFunction(Ar, *It, TickGroupEnum);
			}
		}
		DisabledCount += AllDisabledTickFunctions.Num();
	}

	/** Remove the tick function from the master list **/
	void RemoveTickFunction(FTickFunction* TickFunction)
	{
		switch(TickFunction->TickState)
		{
		case FTickFunction::ETickState::Enabled:
			if (TickFunction->TickInterval > 0.f)
			{
				// An enabled function with a tick interval could be in either the enabled or cooling down list
				if (AllEnabledTickFunctions.Remove(TickFunction) == 0)
				{
					FTickFunction* PrevComparisionFunction = nullptr;
					FTickFunction* ComparisonFunction = AllCoolingDownTickFunctions.Head;
					bool bFound = false;
					while (ComparisonFunction && !bFound)
					{
						if (ComparisonFunction == TickFunction)
						{
							bFound = true;
							if (PrevComparisionFunction)
							{
								PrevComparisionFunction->Next = TickFunction->Next;
							}
							else
							{
								check(TickFunction == AllCoolingDownTickFunctions.Head);
								AllCoolingDownTickFunctions.Head = TickFunction->Next;
							}
							TickFunction->Next = nullptr;
						}
						else
						{
							PrevComparisionFunction = ComparisonFunction;
							ComparisonFunction = ComparisonFunction->Next;
						}
					}
					check(bFound); // otherwise you changed TickState while the tick function was registered. Call SetTickFunctionEnable instead.
				}
			}
			else
			{
				verify(AllEnabledTickFunctions.Remove(TickFunction) == 1); // otherwise you changed TickState while the tick function was registered. Call SetTickFunctionEnable instead.
			}
			break;

		case FTickFunction::ETickState::Disabled:
			verify(AllDisabledTickFunctions.Remove(TickFunction) == 1); // otherwise you changed TickState while the tick function was registered. Call SetTickFunctionEnable instead.
			break;

		case FTickFunction::ETickState::CoolingDown:
			FTickFunction* PrevComparisionFunction = nullptr;
			FTickFunction* ComparisonFunction = AllCoolingDownTickFunctions.Head;
			bool bFound = false;
			while (ComparisonFunction && !bFound)
			{
				if (ComparisonFunction == TickFunction)
				{
					bFound = true;
					if (PrevComparisionFunction)
					{
						PrevComparisionFunction->Next = TickFunction->Next;
					}
					else
					{
						check(TickFunction == AllCoolingDownTickFunctions.Head);
						AllCoolingDownTickFunctions.Head = TickFunction->Next;
					}
					if (TickFunction->Next)
					{
						TickFunction->Next->RelativeTickCooldown += TickFunction->RelativeTickCooldown;
						TickFunction->Next = nullptr;
					}
				}
				else
				{
					PrevComparisionFunction = ComparisonFunction;
					ComparisonFunction = ComparisonFunction->Next;
				}
			}
			check(bFound); // otherwise you changed TickState while the tick function was registered. Call SetTickFunctionEnable instead.
			break;
		}
		if (bTickNewlySpawned)
		{
			NewlySpawnedTickFunctions.Remove(TickFunction);
		}
	}

private:

	struct FCoolingDownTickFunctionList
	{
		FCoolingDownTickFunctionList()
			: Head(nullptr)
		{
		}

		bool Contains(FTickFunction* TickFunction) const
		{
			FTickFunction* Node = Head;
			while (Node)
			{
				if (Node == TickFunction)
				{
					return true;
				}
				Node = Node->Next;
			}
			return false;
		}

		FTickFunction* Head;
	};

	/** Global Sequencer														*/
	FTickTaskSequencer&							TickTaskSequencer;
	/** Master list of enabled tick functions **/
	TSet<FTickFunction*>						AllEnabledTickFunctions;
	/** Master list of enabled tick functions **/
	FCoolingDownTickFunctionList				AllCoolingDownTickFunctions;
	/** Master list of disabled tick functions **/
	TSet<FTickFunction*>						AllDisabledTickFunctions;
	/** Utility array to avoid memory reallocations when collecting functions to reschedule **/
	TArrayWithThreadsafeAdd<FTickScheduleDetails>				TickFunctionsToReschedule;
	/** List of tick functions added during a tick phase; these items are also duplicated in AllLiveTickFunctions for future frames **/
	TSet<FTickFunction*>						NewlySpawnedTickFunctions;
	/** tick context **/
	FTickContext								Context;
	/** true during the tick phase, when true, tick function adds also go to the newly spawned list. **/
	bool										bTickNewlySpawned;
};

/** Helper struct to hold completion items from parallel task. They are moved into a separate place for cache coherency **/
struct FTickGroupCompletionItem
{
	/** Task created **/
	TGraphTask<FTickFunctionTask>* Task;
	/** Tick group to complete with **/
	TEnumAsByte<ETickingGroup>	ActualStartTickGroup;
	/** True if this was a misplaced interval tick that we need to deal with **/
	bool bNeedsToBeRemovedFromTickListsAndRescheduled;
	/** True if this is hi pri **/
	bool bHiPri;
};

/** Class that aggregates the individual levels and deals with parallel tick setup **/
class FTickTaskManager : public FTickTaskManagerInterface
{
public:
	/**
	 * Singleton to retrieve the global tick task manager
	 * @return Reference to the global tick task manager
	**/
	static FTickTaskManager& Get()
	{
		static FTickTaskManager SingletonInstance;
		return SingletonInstance;
	}

	/** Allocate a new ticking structure for a ULevel **/
	virtual FTickTaskLevel* AllocateTickTaskLevel() override
	{
		return new FTickTaskLevel;
	}

	/** Free a ticking structure for a ULevel **/
	virtual void FreeTickTaskLevel(FTickTaskLevel* TickTaskLevel) override
	{
		delete TickTaskLevel;
	}

	/**
	 * Ticks the dynamic actors in the given levels based upon their tick group. This function
	 * is called once for each ticking group
	 *
	 * @param World	- World currently ticking
	 * @param DeltaSeconds - time in seconds since last tick
	 * @param TickType - type of tick (viewports only, time only, etc)
	 * @param LevelsToTick - the levels to tick, may be a subset of InWorld->Levels
	 */
	virtual void StartFrame(UWorld* InWorld, float InDeltaSeconds, ELevelTick InTickType, const TArray<ULevel*>& LevelsToTick) override
	{
		SCOPE_CYCLE_COUNTER(STAT_QueueTicks);
#if !UE_BUILD_SHIPPING
		if (CVarStallStartFrame.GetValueOnGameThread() > 0.0f)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_Tick_Intentional_Stall);
			FPlatformProcess::Sleep(CVarStallStartFrame.GetValueOnGameThread() / 1000.0f);
		}
#endif
		Context.TickGroup = ETickingGroup(0); // reset this to the start tick group
		Context.DeltaSeconds = InDeltaSeconds;
		Context.TickType = InTickType;
		Context.Thread = ENamedThreads::GameThread;
		Context.World = InWorld;

		bTickNewlySpawned = true;
		TickTaskSequencer.StartFrame();
		FillLevelList(LevelsToTick);

		int32 NumWorkerThread = 0;
		bool bConcurrentQueue = false;
#if !PLATFORM_WINDOWS
		// the windows scheduler will hang for seconds trying to do this algorithm, threads starve even though other threads are calling sleep(0)
		if (!FTickTaskSequencer::SingleThreadedMode())
		{
			bConcurrentQueue = !!CVarAllowConcurrentQueue.GetValueOnGameThread();
		}
#endif

		if (!bConcurrentQueue)
		{
			int32 TotalTickFunctions = 0;
			for( int32 LevelIndex = 0; LevelIndex < LevelList.Num(); LevelIndex++ )
			{
				TotalTickFunctions += LevelList[LevelIndex]->StartFrame(Context);
			}
			INC_DWORD_STAT_BY(STAT_TicksQueued, TotalTickFunctions);
			for( int32 LevelIndex = 0; LevelIndex < LevelList.Num(); LevelIndex++ )
			{
				LevelList[LevelIndex]->QueueAllTicks();
			}
		}
		else
		{
			for( int32 LevelIndex = 0; LevelIndex < LevelList.Num(); LevelIndex++ )
			{
				LevelList[LevelIndex]->StartFrameParallel(Context, AllTickFunctions);
			}
			INC_DWORD_STAT_BY(STAT_TicksQueued, AllTickFunctions.Num());
			FTickTaskSequencer& TTS = FTickTaskSequencer::Get();
			TTS.SetupAddTickTaskCompletionParallel(AllTickFunctions.Num());
			for( int32 LevelIndex = 0; LevelIndex < LevelList.Num(); LevelIndex++ )
			{
				LevelList[LevelIndex]->ReserveTickFunctionCooldowns(AllTickFunctions.Num());
			}
			ParallelFor(AllTickFunctions.Num(),
				[this](int32 Index)
				{
					FTickFunction* TickFunction = AllTickFunctions[Index];

					TArray<FTickFunction*, TInlineAllocator<8> > StackForCycleDetection;
					TickFunction->QueueTickFunctionParallel(Context, StackForCycleDetection);
				}
			);
			for( int32 LevelIndex = 0; LevelIndex < LevelList.Num(); LevelIndex++ )
			{
				LevelList[LevelIndex]->ScheduleTickFunctionCooldowns();
			}
			AllTickFunctions.Reset();
		}
	}

	/**
	 * Run all of the ticks for a pause frame synchronously on the game thread.
	 * The capability of pause ticks are very limited. There are no dependencies or ordering or tick groups.
	 * @param World	- World currently ticking
	 * @param DeltaSeconds - time in seconds since last tick
	 * @param TickType - type of tick (viewports only, time only, etc)
	 */
	virtual void RunPauseFrame(UWorld* InWorld, float InDeltaSeconds, ELevelTick InTickType, const TArray<ULevel*>& LevelsToTick) override
	{
		bTickNewlySpawned = true; // we don't support new spawns, but lets at least catch them.
		Context.TickGroup = ETickingGroup(0); // reset this to the start tick group
		Context.DeltaSeconds = InDeltaSeconds;
		Context.TickType = InTickType;
		Context.Thread = ENamedThreads::GameThread;
		Context.World = InWorld;
		FillLevelList(LevelsToTick);
		for( int32 LevelIndex = 0; LevelIndex < LevelList.Num(); LevelIndex++ )
		{
			LevelList[LevelIndex]->RunPauseFrame(Context);
		}
		Context.World = nullptr;
		bTickNewlySpawned = false;
		LevelList.Reset();
	}
	/**
		* Run a tick group, ticking all actors and components
		* @param Group - Ticking group to run
		* @param bBlockTillComplete - if true, do not return until all ticks are complete
	*/
	virtual void RunTickGroup(ETickingGroup Group, bool bBlockTillComplete ) override
	{
		check(Context.TickGroup == Group); // this should already be at the correct value, but we want to make sure things are happening in the right order
		check(bTickNewlySpawned); // we should be in the middle of ticking
		TickTaskSequencer.ReleaseTickGroup(Group, bBlockTillComplete);
		Context.TickGroup = ETickingGroup(Context.TickGroup + 1); // new actors go into the next tick group because this one is already gone
		if (bBlockTillComplete) // we don't deal with newly spawned ticks within the async tick group, they wait until after the async stuff
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_TickTask_RunTickGroup_BlockTillComplete);

			bool bFinished = false;
			for (int32 Iterations = 0;Iterations < 101; Iterations++)
			{
				int32 Num = 0;
				for( int32 LevelIndex = 0; LevelIndex < LevelList.Num(); LevelIndex++ )
				{
					Num += LevelList[LevelIndex]->QueueNewlySpawned(Context.TickGroup);
				}
				if (Num && Context.TickGroup == TG_NewlySpawned)
				{
					SCOPE_CYCLE_COUNTER(STAT_TG_NewlySpawned);
					TickTaskSequencer.ReleaseTickGroup(TG_NewlySpawned, true);
				}
				else
				{
					bFinished = true;
					break;
				}
			}
			if (!bFinished)
			{
				// this is runaway recursive spawning.
				for( int32 LevelIndex = 0; LevelIndex < LevelList.Num(); LevelIndex++ )
				{
					LevelList[LevelIndex]->LogAndDisardRunawayNewlySpawned(Context.TickGroup);
				}
			}
		}
	}

	/** Finish a frame of ticks **/
	virtual void EndFrame() override
	{
		TickTaskSequencer.EndFrame();
		bTickNewlySpawned = false;
		for( int32 LevelIndex = 0; LevelIndex < LevelList.Num(); LevelIndex++ )
		{
			LevelList[LevelIndex]->EndFrame();
		}
		Context.World = nullptr;
		LevelList.Reset();
	}

	// Interface that is private to FTickFunction

	/** Return true if this tick function is in the master list **/
	bool HasTickFunction(ULevel* InLevel, FTickFunction* TickFunction)
	{
		FTickTaskLevel* Level = TickTaskLevelForLevel(InLevel);
		return Level->HasTickFunction(TickFunction);
	}
	/** Add the tick function to the master list **/
	void AddTickFunction(ULevel* InLevel, FTickFunction* TickFunction)
	{
		check(TickFunction->TickGroup >= 0 && TickFunction->TickGroup < TG_NewlySpawned); // You may not schedule a tick in the newly spawned group...they can only end up there if they are spawned late in a frame.
		FTickTaskLevel* Level = TickTaskLevelForLevel(InLevel);
		Level->AddTickFunction(TickFunction);
		TickFunction->TickTaskLevel = Level;
	}
	/** Remove the tick function from the master list **/
	void RemoveTickFunction(FTickFunction* TickFunction)
	{
		FTickTaskLevel* Level = TickFunction->TickTaskLevel;
		check(Level);
		Level->RemoveTickFunction(TickFunction);
	}

private:
	/** Default constructor **/
	FTickTaskManager()
		: TickTaskSequencer(FTickTaskSequencer::Get())
		, bTickNewlySpawned(false)
	{
		IConsoleManager::Get().RegisterConsoleCommand(TEXT("dumpticks"), TEXT("Dumps all tick functions registered with FTickTaskManager to log."));
	}

	/** Fill the level list **/
	void FillLevelList(const TArray<ULevel*>& Levels)
	{
		check(!LevelList.Num());
		if (!Context.World->GetActiveLevelCollection() || Context.World->GetActiveLevelCollection()->GetType() == ELevelCollectionType::DynamicSourceLevels)
		{
			check(Context.World->TickTaskLevel);
			LevelList.Add(Context.World->TickTaskLevel);
		}
		for( int32 LevelIndex = 0; LevelIndex < Levels.Num(); LevelIndex++ )
		{
			ULevel* Level = Levels[LevelIndex];
			if (Level->bIsVisible)
			{
				check(Level->TickTaskLevel);
				LevelList.Add(Level->TickTaskLevel);
			}
		}
	}

	/** Find the tick level for this actor **/
	FTickTaskLevel* TickTaskLevelForLevel(ULevel* Level)
	{
		check(Level);
		check(Level->TickTaskLevel);
		return Level->TickTaskLevel;
	}

	/** Dumps all tick functions to output device */
	virtual void DumpAllTickFunctions(FOutputDevice& Ar, UWorld* InWorld, bool bEnabled, bool bDisabled) override
	{
		int32 EnabledCount = 0, DisabledCount = 0;

		Ar.Logf(TEXT(""));
		Ar.Logf(TEXT("============================ Tick Functions (%s) ============================"), (bEnabled && bDisabled) ? TEXT("All") : (bEnabled ? TEXT("Enabled") : TEXT("Disabled")));

		check(InWorld);
		check(InWorld->TickTaskLevel);
		InWorld->TickTaskLevel->DumpAllTickFunctions(Ar, EnabledCount, DisabledCount, bEnabled, bDisabled);
		for( int32 LevelIndex = 0; LevelIndex < InWorld->GetNumLevels(); LevelIndex++ )
		{
			ULevel* Level = InWorld->GetLevel(LevelIndex);
			if (Level->bIsVisible)
			{
				check(Level->TickTaskLevel);
				Level->TickTaskLevel->DumpAllTickFunctions(Ar, EnabledCount, DisabledCount, bEnabled, bDisabled);
			}
		}

		Ar.Logf(TEXT(""));
		Ar.Logf(TEXT("Total registered tick functions: %d, enabled: %d, disabled: %d."), EnabledCount + DisabledCount, EnabledCount, DisabledCount);
		Ar.Logf(TEXT(""));
	}

	/** Global Sequencer														*/
	FTickTaskSequencer&							TickTaskSequencer;
	/** List of current levels **/
	TArray<FTickTaskLevel*>						LevelList;
	/** tick context **/
	FTickContext								Context;
	/** true during the tick phase, when true, tick function adds also go to the newly spawned list. **/
	bool										bTickNewlySpawned;

	/** true during the tick phase, when true, tick function adds also go to the newly spawned list. **/
	TArray<FTickFunction*> AllTickFunctions;
};


/** Default constructor, intitalizes to reasonable defaults **/
FTickFunction::FTickFunction()
	: TickGroup(TG_PrePhysics)
	, EndTickGroup(TG_PrePhysics)
	, ActualStartTickGroup(TG_PrePhysics)
	, ActualEndTickGroup(TG_PrePhysics)
	, bTickEvenWhenPaused(false)
	, bCanEverTick(false)
	, bAllowTickOnDedicatedServer(true)
	, bHighPriority(false)
	, bRunOnAnyThread(false)
	, bRegistered(false)
	, bWasInterval(false)
	, TickState(ETickState::Enabled)
	, TickVisitedGFrameCounter(0)
	, TickQueuedGFrameCounter(0)
	, RelativeTickCooldown(0.f)
	, LastTickGameTimeSeconds(-1.f)
	, TickInterval(0.f)
	, TickTaskLevel(nullptr)
{
}

/** Destructor, unregisters the tick function **/
FTickFunction::~FTickFunction()
{
	UnRegisterTickFunction();
}


/**
* Adds the tick function to the master list of tick functions.
* @param Level - level to place this tick function in
**/
void FTickFunction::RegisterTickFunction(ULevel* Level)
{
	if (!bRegistered)
	{
		// Only allow registration of tick if we are are allowed on dedicated server, or we are not a dedicated server
		if(bAllowTickOnDedicatedServer || !IsRunningDedicatedServer())
		{
			FTickTaskManager::Get().AddTickFunction(Level, this);
			bRegistered = true;
		}
	}
	else
	{
		check(FTickTaskManager::Get().HasTickFunction(Level, this));
	}
}

/** Removes the tick function from the master list of tick functions. **/
void FTickFunction::UnRegisterTickFunction()
{
	if (bRegistered)
	{
		FTickTaskManager::Get().RemoveTickFunction(this);
		bRegistered = false;
	}
}

/** Enables or disables this tick function. **/
void FTickFunction::SetTickFunctionEnable(bool bInEnabled)
{
	if (bRegistered && (bInEnabled == (TickState == ETickState::Disabled)))
	{
		check(TickTaskLevel);
		TickTaskLevel->RemoveTickFunction(this);
		TickState = (bInEnabled ? ETickState::Enabled : ETickState::Disabled);
		TickTaskLevel->AddTickFunction(this);
	}
	else
	{
		TickState = (bInEnabled ? ETickState::Enabled : ETickState::Disabled);
	}

	if (TickState == ETickState::Disabled)
	{
		LastTickGameTimeSeconds = -1.f;
	}
}

void FTickFunction::AddPrerequisite(UObject* TargetObject, struct FTickFunction& TargetTickFunction)
{
	const bool bThisCanTick = (bCanEverTick || IsTickFunctionRegistered());
	const bool bTargetCanTick = (TargetTickFunction.bCanEverTick || TargetTickFunction.IsTickFunctionRegistered());

	if (bThisCanTick && bTargetCanTick)
	{
		Prerequisites.AddUnique(FTickPrerequisite(TargetObject, TargetTickFunction));
	}
}

void FTickFunction::RemovePrerequisite(UObject* TargetObject, struct FTickFunction& TargetTickFunction)
{
	Prerequisites.RemoveSwap(FTickPrerequisite(TargetObject, TargetTickFunction));
}

void FTickFunction::SetPriorityIncludingPrerequisites(bool bInHighPriority)
{
	if (bHighPriority != bInHighPriority)
	{
		bHighPriority = bInHighPriority;
		for (auto& Prereq : Prerequisites)
		{
			if (Prereq.PrerequisiteObject.Get() && Prereq.PrerequisiteTickFunction && Prereq.PrerequisiteTickFunction->bHighPriority != bInHighPriority)
			{
				Prereq.PrerequisiteTickFunction->SetPriorityIncludingPrerequisites(bInHighPriority);
			}
		}
	}
}

void FTickFunction::ShowPrerequistes(int32 Indent)
{
	for (auto& Prereq : Prerequisites)
	{
		if (Prereq.PrerequisiteTickFunction)
		{
			UE_LOG(LogTick, Log, TEXT("%s prereq %s"), FCString::Spc(Indent * 2), *Prereq.PrerequisiteTickFunction->DiagnosticMessage());
			Prereq.PrerequisiteTickFunction->ShowPrerequistes(Indent + 1);
		}
	}
}

FGraphEventRef FTickFunction::GetCompletionHandle() const
{
	check(TaskPointer);
	TGraphTask<FTickFunctionTask>* Task = (TGraphTask<FTickFunctionTask>*)TaskPointer;
	return Task->GetCompletionEvent();
}


void FTickFunction::QueueTickFunction(FTickTaskSequencer& TTS, const struct FTickContext& TickContext)
{
	checkSlow(TickContext.Thread == ENamedThreads::GameThread); // we assume same thread here
	check(bRegistered);

	if (TickVisitedGFrameCounter != GFrameCounter)
	{
		TickVisitedGFrameCounter = GFrameCounter;
		if (TickState != FTickFunction::ETickState::Disabled)
		{
			ETickingGroup MaxPrerequisiteTickGroup =  ETickingGroup(0);

			FGraphEventArray TaskPrerequisites;
			for (int32 PrereqIndex = 0; PrereqIndex < Prerequisites.Num(); PrereqIndex++)
			{
				FTickFunction* Prereq = Prerequisites[PrereqIndex].Get();
				if (!Prereq)
				{
					// stale prereq, delete it
					Prerequisites.RemoveAtSwap(PrereqIndex--);
				}
				else if (Prereq->bRegistered)
				{
					// recursive call to make sure my prerequisite is set up so I can use its completion handle
					Prereq->QueueTickFunction(TTS, TickContext);
					if (Prereq->TickQueuedGFrameCounter != GFrameCounter)
					{
						// this must be up the call stack, therefore this is a cycle
						UE_LOG(LogTick, Warning, TEXT("While processing prerequisites for %s, could use %s because it would form a cycle."),*DiagnosticMessage(), *Prereq->DiagnosticMessage());
					}
					else if (!Prereq->TaskPointer)
					{
						//ok UE_LOG(LogTick, Warning, TEXT("While processing prerequisites for %s, could use %s because it is disabled."),*DiagnosticMessage(), *Prereq->DiagnosticMessage());
					}
					else
					{
						MaxPrerequisiteTickGroup =  FMath::Max<ETickingGroup>(MaxPrerequisiteTickGroup, Prereq->ActualStartTickGroup);
						TaskPrerequisites.Add(Prereq->GetCompletionHandle());
					}
				}
			}

			// tick group is the max of the prerequisites, the current tick group, and the desired tick group
			ETickingGroup MyActualTickGroup =  FMath::Max<ETickingGroup>(MaxPrerequisiteTickGroup, FMath::Max<ETickingGroup>(TickGroup,TickContext.TickGroup));
			if (MyActualTickGroup != TickGroup)
			{
				// if the tick was "demoted", make sure it ends up in an ordinary tick group.
				while (!CanDemoteIntoTickGroup(MyActualTickGroup))
				{
					MyActualTickGroup = ETickingGroup(MyActualTickGroup + 1);
				}
			}
			ActualStartTickGroup = MyActualTickGroup;
			ActualEndTickGroup = MyActualTickGroup;
			if (EndTickGroup > ActualStartTickGroup)
			{
				check(EndTickGroup <= TG_NewlySpawned);
				ETickingGroup TestTickGroup = ETickingGroup(ActualEndTickGroup + 1);
				while (TestTickGroup <= EndTickGroup)
			{
					if (CanDemoteIntoTickGroup(TestTickGroup))
					{
						ActualEndTickGroup = TestTickGroup;
					}
					TestTickGroup = ETickingGroup(TestTickGroup + 1);
				}
			}

			if (TickState == FTickFunction::ETickState::Enabled)
			{
				TTS.QueueTickTask(&TaskPrerequisites, this, TickContext);
			}
		}
		TickQueuedGFrameCounter = GFrameCounter;
	}
}

void FTickFunction::QueueTickFunctionParallel(const struct FTickContext& TickContext, TArray<FTickFunction*, TInlineAllocator<8> >& StackForCycleDetection)
{

	bool bProcessTick;

	int32 OldValue = *(volatile int32*)&TickVisitedGFrameCounter;
	if (OldValue != GFrameCounter)
	{
		OldValue = FPlatformAtomics::InterlockedCompareExchange(&TickVisitedGFrameCounter , GFrameCounter, OldValue);
	}
	bProcessTick = OldValue != GFrameCounter;

	if (bProcessTick)
	{
		check(bRegistered);
		if (TickState != FTickFunction::ETickState::Disabled)
		{
			ETickingGroup MaxPrerequisiteTickGroup = ETickingGroup(0);

			FGraphEventArray TaskPrerequisites;
			if (Prerequisites.Num())
			{
				StackForCycleDetection.Push(this);
				for (int32 PrereqIndex = 0; PrereqIndex < Prerequisites.Num(); PrereqIndex++)
				{
					FTickFunction* Prereq = Prerequisites[PrereqIndex].Get();
#if USING_THREAD_SANITISER
					if (Prereq) { TSAN_AFTER(&Prereq->TickQueuedGFrameCounter); }
#endif
					if (!Prereq)
					{
						// stale prereq, delete it
						Prerequisites.RemoveAtSwap(PrereqIndex--);
					}
                    else if (StackForCycleDetection.Contains(Prereq))
                    {
                        UE_LOG(LogTick, Warning, TEXT("While processing prerequisites for %s, could use %s because it would form a cycle."), *DiagnosticMessage(), *Prereq->DiagnosticMessage());
                    }
					else if (Prereq->bRegistered)
					{
						// recursive call to make sure my prerequisite is set up so I can use its completion handle
						Prereq->QueueTickFunctionParallel(TickContext, StackForCycleDetection);
						if (!Prereq->TaskPointer)
						{
							//ok UE_LOG(LogTick, Warning, TEXT("While processing prerequisites for %s, could use %s because it is disabled."),*DiagnosticMessage(), *Prereq->DiagnosticMessage());
						}
						else
						{
							MaxPrerequisiteTickGroup = FMath::Max<ETickingGroup>(MaxPrerequisiteTickGroup, Prereq->ActualStartTickGroup);
							TaskPrerequisites.Add(Prereq->GetCompletionHandle());
						}
					}
				}
				StackForCycleDetection.Pop();
			}

			// tick group is the max of the prerequisites, the current tick group, and the desired tick group
			ETickingGroup MyActualTickGroup =  FMath::Max<ETickingGroup>(MaxPrerequisiteTickGroup, FMath::Max<ETickingGroup>(TickGroup,TickContext.TickGroup));
			if (MyActualTickGroup != TickGroup)
			{
				// if the tick was "demoted", make sure it ends up in an ordinary tick group.
				while (!CanDemoteIntoTickGroup(MyActualTickGroup))
				{
					MyActualTickGroup = ETickingGroup(MyActualTickGroup + 1);
				}
			}
			ActualStartTickGroup = MyActualTickGroup;
			ActualEndTickGroup = MyActualTickGroup;
			if (EndTickGroup > ActualStartTickGroup)
			{
				check(EndTickGroup <= TG_NewlySpawned);
				ETickingGroup TestTickGroup = ETickingGroup(ActualEndTickGroup + 1);
				while (TestTickGroup <= EndTickGroup)
				{
					if (CanDemoteIntoTickGroup(TestTickGroup))
					{
						ActualEndTickGroup = TestTickGroup;
					}
					TestTickGroup = ETickingGroup(TestTickGroup + 1);
				}
			}

			if (TickState == FTickFunction::ETickState::Enabled)
			{
				FTickTaskSequencer::Get().QueueTickTaskParallel(&TaskPrerequisites, this, TickContext);
				if (!bWasInterval && TickInterval > 0.f)
				{
					TickTaskLevel->RescheduleForIntervalParallel(this);
				}
			}
		}
		bWasInterval = false;
		
		TSAN_BEFORE(&TickQueuedGFrameCounter);
		FPlatformMisc::MemoryBarrier();
		
		// MSVC enforces acq/rel semantics on volatile values, but clang cannot (supports more backend architectures).
		// consequently on ARM64 you would end up racing
		FPlatformAtomics::InterlockedExchange(&TickQueuedGFrameCounter, GFrameCounter);
	}
	else
	{
		// if we are not going to process it, we need to at least wait until the other thread finishes it
		volatile int32* TickQueuedGFrameCounterPtr = &TickQueuedGFrameCounter;
		if (*TickQueuedGFrameCounterPtr != GFrameCounter)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FTickFunction_QueueTickFunctionParallel_Spin);
			while (*TickQueuedGFrameCounterPtr != GFrameCounter)
			{
				FPlatformProcess::SleepNoStats(0);
			}
		}
	}
}

float FTickFunction::CalculateDeltaTime(const FTickContext& TickContext)
{
	float DeltaTimeForFunction = TickContext.DeltaSeconds;

	if (TickInterval == 0.f)
	{
		// No tick interval. Return the world delta seconds, and make sure to mark that
		// we're not tracking last-tick-time for this object.
		LastTickGameTimeSeconds = -1.f;
	}
	else
	{
		// We've got a tick interval. Mark last-tick-time. If we already had last-tick-time, return
		// the time since then; otherwise, return the world delta seconds.
		const float CurrentWorldTime = (bTickEvenWhenPaused ? TickContext.World->GetUnpausedTimeSeconds() : TickContext.World->GetTimeSeconds());
		if (LastTickGameTimeSeconds >= 0.f)
		{
			DeltaTimeForFunction = CurrentWorldTime - LastTickGameTimeSeconds;
		}
		LastTickGameTimeSeconds = CurrentWorldTime;
	}

	return DeltaTimeForFunction;
}

/**
 * Singleton to retrieve the global tick task manager
 * @return Reference to the global tick task manager
**/
FTickTaskManagerInterface& FTickTaskManagerInterface::Get()
{
	return FTickTaskManager::Get();
}


struct FTestTickFunction : public FTickFunction
{
	FTestTickFunction()
	{
		TickGroup = TG_PrePhysics;
		bTickEvenWhenPaused = true;
	}
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_TestStatOverhead_FTestTickFunction);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_TestStatOverhead_FTestTickFunction_Inner);
	}
	/** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
	virtual FString DiagnosticMessage() override
	{
		return FString(TEXT("test"));
	}
};

template<>
struct TStructOpsTypeTraits<FTestTickFunction> : public TStructOpsTypeTraitsBase2<FTestTickFunction>
{
	enum
	{
		WithCopy = false
	};
};

static const int32 NumTestTickFunctions = 10000;
static TArray<FTestTickFunction> TestTickFunctions;
static TArray<FTestTickFunction*> IndirectTestTickFunctions;

static void RemoveTestTickFunctions(const TArray<FString>& Args)
{
	if (TestTickFunctions.Num() || IndirectTestTickFunctions.Num())
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("Removing Test Tick Functions."));
		TestTickFunctions.Empty(NumTestTickFunctions);
		for (int32 Index = 0; Index < IndirectTestTickFunctions.Num(); Index++)
		{
			delete IndirectTestTickFunctions[Index];
		}
		IndirectTestTickFunctions.Empty(NumTestTickFunctions);
	}
}

static void AddTestTickFunctions(const TArray<FString>& Args, UWorld* InWorld)
{
	RemoveTestTickFunctions(Args);
	ULevel* Level = InWorld->GetCurrentLevel();
	UE_LOG(LogConsoleResponse, Display, TEXT("Adding 1000 ticks in a cache coherent fashion."));


	TestTickFunctions.Reserve(NumTestTickFunctions);
	for (int32 Index = 0; Index < NumTestTickFunctions; Index++)
	{
		(new (TestTickFunctions) FTestTickFunction())->RegisterTickFunction(Level);
	}
}

static void AddIndirectTestTickFunctions(const TArray<FString>& Args, UWorld* InWorld)
{
	RemoveTestTickFunctions(Args);
	ULevel* Level = InWorld->GetCurrentLevel();
	UE_LOG(LogConsoleResponse, Display, TEXT("Adding 1000 ticks in a cache coherent fashion."));
	TArray<FTestTickFunction*> Junk;
	for (int32 Index = 0; Index < NumTestTickFunctions; Index++)
	{
		for (int32 JunkIndex = 0; JunkIndex < 8; JunkIndex++)
		{
			Junk.Add(new FTestTickFunction); // don't give the allocator an easy ride
		}
		FTestTickFunction* NewTick = new FTestTickFunction;
		NewTick->RegisterTickFunction(Level);
		IndirectTestTickFunctions.Add(NewTick);
	}
	for (int32 JunkIndex = 0; JunkIndex < 8; JunkIndex++)
	{
		delete Junk[JunkIndex];
	}
}

static FAutoConsoleCommand RemoveTestTickFunctionsCmd(
	TEXT("tick.RemoveTestTickFunctions"),
	TEXT("Remove no-op ticks to test performance of ticking infrastructure."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&RemoveTestTickFunctions)
	);

static FAutoConsoleCommandWithWorldAndArgs AddTestTickFunctionsCmd(
	TEXT("tick.AddTestTickFunctions"),
	TEXT("Add no-op ticks to test performance of ticking infrastructure."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AddTestTickFunctions)
	);

static FAutoConsoleCommandWithWorldAndArgs AddIndirectTestTickFunctionsCmd(
	TEXT("tick.AddIndirectTestTickFunctions"),
	TEXT("Add no-op ticks to test performance of ticking infrastructure."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AddIndirectTestTickFunctions)
	);



