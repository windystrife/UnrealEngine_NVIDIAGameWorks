// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParllelFor.h: TaskGraph library
=============================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Math/UnrealMathUtility.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "HAL/ThreadSafeCounter.h"
#include "Stats/Stats.h"
#include "Async/TaskGraphInterfaces.h"
#include "Misc/App.h"

// struct to hold the working data; this outlives the ParallelFor call; lifetime is controlled by a shared pointer
struct FParallelForData
{
	int32 Num;
	int32 BlockSize;
	int32 LastBlockExtraNum;
	TFunctionRef<void(int32)> Body;
	FEvent* Event;
	FThreadSafeCounter IndexToDo;
	FThreadSafeCounter NumCompleted;
	bool bExited;
	bool bTriggered;
	bool bSaveLastBlockForMaster;
	FParallelForData(int32 InTotalNum, int32 InNumThreads, bool bInSaveLastBlockForMaster, TFunctionRef<void(int32)> InBody)
		: Body(InBody)
		, Event(FPlatformProcess::GetSynchEventFromPool(false))
		, bExited(false)
		, bTriggered(false)
		, bSaveLastBlockForMaster(bInSaveLastBlockForMaster)
	{
		check(InTotalNum >= InNumThreads);
		BlockSize = 0;
		Num = 0;
		for (int32 Div = 3; Div; Div--)
		{
			BlockSize = InTotalNum / (InNumThreads * Div);
			if (BlockSize)
			{
				Num = InTotalNum / BlockSize;
				if (Num >= InNumThreads + !!bSaveLastBlockForMaster)
				{
					break;
				}
			}
		}
		check(BlockSize && Num);
		LastBlockExtraNum = InTotalNum - Num * BlockSize;
		check(LastBlockExtraNum >= 0);
	}
	~FParallelForData()
	{
		check(IndexToDo.GetValue() >= Num);
		check(NumCompleted.GetValue() == Num);
		check(bExited);
		FPlatformProcess::ReturnSynchEventToPool(Event);
	}
	bool Process(int32 TasksToSpawn, TSharedRef<FParallelForData, ESPMode::ThreadSafe>& Data, bool bMaster);
};

class FParallelForTask
{
	TSharedRef<FParallelForData, ESPMode::ThreadSafe> Data;
	int32 TasksToSpawn;
public:
	FParallelForTask(TSharedRef<FParallelForData, ESPMode::ThreadSafe>& InData, int32 InTasksToSpawn = 0)
		: Data(InData) 
		, TasksToSpawn(InTasksToSpawn)
	{
	}
	static FORCEINLINE TStatId GetStatId()
	{
		return GET_STATID(STAT_ParallelForTask);
	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyHiPriThreadHiPriTask;
	}
	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode() 
	{ 
		return ESubsequentsMode::FireAndForget; 
	}
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		if (Data->Process(TasksToSpawn, Data, false))
		{
			checkSlow(!Data->bTriggered);
			Data->bTriggered = true;
			Data->Event->Trigger();
		}
	}
};

inline bool FParallelForData::Process(int32 TasksToSpawn, TSharedRef<FParallelForData, ESPMode::ThreadSafe>& Data, bool bMaster)
{
	int32 MaybeTasksLeft = Num - IndexToDo.GetValue();
	if (TasksToSpawn && MaybeTasksLeft > 0)
	{
		TasksToSpawn = FMath::Min<int32>(TasksToSpawn, MaybeTasksLeft);
		TGraphTask<FParallelForTask>::CreateTask().ConstructAndDispatchWhenReady(Data, TasksToSpawn - 1);		
	}
	int32 LocalBlockSize = BlockSize;
	int32 LocalNum = Num;
	bool bLocalSaveLastBlockForMaster = bSaveLastBlockForMaster;
	TFunctionRef<void(int32)> LocalBody(Body);
	while (true)
	{
		int32 MyIndex = IndexToDo.Increment() - 1;
		if (bLocalSaveLastBlockForMaster)
		{
			if (!bMaster && MyIndex >= LocalNum - 1)
			{
				break; // leave the last block for the master, hoping to avoid an event
			}
			else if (bMaster && MyIndex > LocalNum - 1)
			{
				MyIndex = LocalNum - 1; // I am the master, I need to take this block, hoping to avoid an event
			}
		}
		if (MyIndex < LocalNum)
		{
			int32 ThisBlockSize = LocalBlockSize;
			if (MyIndex == LocalNum - 1)
			{
				ThisBlockSize += LastBlockExtraNum;
			}
			for (int32 LocalIndex = 0; LocalIndex < ThisBlockSize; LocalIndex++)
			{
				LocalBody(MyIndex * LocalBlockSize + LocalIndex);
			}
			checkSlow(!bExited);
			int32 LocalNumCompleted = NumCompleted.Increment();
			if (LocalNumCompleted == LocalNum)
			{
				return true;
			}
			checkSlow(LocalNumCompleted < LocalNum);
		}
		if (MyIndex >= LocalNum - 1)
		{
			break;
		}
	}
	return false;
}

/** 
	*	General purpose parallel for that uses the taskgraph
	*	@param Num; number of calls of Body; Body(0), Body(1)....Body(Num - 1)
	*	@param Body; Function to call from multiple threads
	*	@param bForceSingleThread; Mostly used for testing, if true, run single threaded instead.
	*	Notes: Please add stats around to calls to parallel for and within your lambda as appropriate. Do not clog the task graph with long running tasks or tasks that block.
**/
inline void ParallelFor(int32 Num, TFunctionRef<void(int32)> Body, bool bForceSingleThread = false)
{
	SCOPE_CYCLE_COUNTER(STAT_ParallelFor);
	check(Num >= 0);

	int32 AnyThreadTasks = 0;
	if (Num > 1 && !bForceSingleThread && FApp::ShouldUseThreadingForPerformance())
	{
		AnyThreadTasks = FMath::Min<int32>(FTaskGraphInterface::Get().GetNumWorkerThreads(), Num - 1);
	}
	if (!AnyThreadTasks)
	{
		// no threads, just do it and return
		for (int32 Index = 0; Index < Num; Index++)
		{
			Body(Index);
		}
		return;
	}
	FParallelForData* DataPtr = new FParallelForData(Num, AnyThreadTasks + 1, Num > AnyThreadTasks + 1, Body);
	TSharedRef<FParallelForData, ESPMode::ThreadSafe> Data = MakeShareable(DataPtr);
	TGraphTask<FParallelForTask>::CreateTask().ConstructAndDispatchWhenReady(Data, AnyThreadTasks - 1);		
	// this thread can help too and this is important to prevent deadlock on recursion 
	if (!Data->Process(0, Data, true))
	{
		Data->Event->Wait();
		check(Data->bTriggered);
	}
	else
	{
		check(!Data->bTriggered);
	}
	check(Data->NumCompleted.GetValue() == Data->Num);
	Data->bExited = true;
	// DoneEvent waits here if some other thread finishes the last item
	// Data must live on until all of the tasks are cleared which might be long after this function exits
}

/** 
	*	General purpose parallel for that uses the taskgraph
	*	@param Num; number of calls of Body; Body(0), Body(1)....Body(Num - 1)
	*	@param Body; Function to call from multiple threads
	*   @param CurrentThreadWorkToDoBeforeHelping; The work is performed on the main thread before it starts helping with the ParallelFor proper
	*	@param bForceSingleThread; Mostly used for testing, if true, run single threaded instead.
	*	Notes: Please add stats around to calls to parallel for and within your lambda as appropriate. Do not clog the task graph with long running tasks or tasks that block.
**/
inline void ParallelForWithPreWork(int32 Num, TFunctionRef<void(int32)> Body, TFunctionRef<void()> CurrentThreadWorkToDoBeforeHelping, bool bForceSingleThread = false)
{
	SCOPE_CYCLE_COUNTER(STAT_ParallelFor);

	int32 AnyThreadTasks = 0;
	if (!bForceSingleThread && FApp::ShouldUseThreadingForPerformance())
	{
		AnyThreadTasks = FMath::Min<int32>(FTaskGraphInterface::Get().GetNumWorkerThreads(), Num);
	}
	if (!AnyThreadTasks)
	{
		// do the prework
		CurrentThreadWorkToDoBeforeHelping();
		// no threads, just do it and return
		for (int32 Index = 0; Index < Num; Index++)
		{
			Body(Index);
		}
		return;
	}
	check(Num);
	FParallelForData* DataPtr = new FParallelForData(Num, AnyThreadTasks, false, Body);
	TSharedRef<FParallelForData, ESPMode::ThreadSafe> Data = MakeShareable(DataPtr);
	TGraphTask<FParallelForTask>::CreateTask().ConstructAndDispatchWhenReady(Data, AnyThreadTasks - 1);		
	// do the prework
	CurrentThreadWorkToDoBeforeHelping();
	// this thread can help too and this is important to prevent deadlock on recursion 
	if (!Data->Process(0, Data, true))
	{
		Data->Event->Wait();
		check(Data->bTriggered);
	}
	else
	{
		check(!Data->bTriggered);
	}
	check(Data->NumCompleted.GetValue() == Data->Num);
	Data->bExited = true;
	// Data must live on until all of the tasks are cleared which might be long after this function exits
}

