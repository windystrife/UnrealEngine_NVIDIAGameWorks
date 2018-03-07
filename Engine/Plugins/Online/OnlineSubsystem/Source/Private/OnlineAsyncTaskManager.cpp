// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncTaskManager.h"
#include "Misc/ScopeLock.h"
#include "Stats/Stats.h"
#include "HAL/Event.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "OnlineSubsystem.h"

int32 FOnlineAsyncTaskManager::InvocationCount = 0;

#if !UE_BUILD_SHIPPING
namespace OSSConsoleVariables
{
	/** Time to delay finalization of a task in the out queue */
	TAutoConsoleVariable<float> CVarDelayAsyncTaskOutQueue(
		TEXT("OSS.DelayAsyncTaskOutQueue"),
		0.0f,
		TEXT("Min total async task time\n")
		TEXT("Time in secs"),
		ECVF_Default);
}
#endif

/** The default value for the polling interval when not set by config */
#define POLLING_INTERVAL_MS 50

FOnlineAsyncTaskManager::FOnlineAsyncTaskManager() :
	ActiveTask(nullptr),
	WorkEvent(nullptr),
	PollingInterval(POLLING_INTERVAL_MS),
	bRequestingExit(false),
	OnlineThreadId(0)
{
}

bool FOnlineAsyncTaskManager::Init(void)
{
	WorkEvent = FPlatformProcess::GetSynchEventFromPool();
	int32 PollingConfig = POLLING_INTERVAL_MS;
	// Read the polling interval to use from the INI file
	if (GConfig->GetInt(TEXT("OnlineSubsystem"), TEXT("PollingIntervalInMs"), PollingConfig, GEngineIni))
	{
		PollingInterval = (uint32)PollingConfig;
	}

	return WorkEvent != nullptr;
}

uint32 FOnlineAsyncTaskManager::Run(void)
{
	InvocationCount++;
	// This should not be set yet
	check(OnlineThreadId == 0);
	FPlatformAtomics::InterlockedExchange((volatile int32*)&OnlineThreadId, FPlatformTLS::GetCurrentThreadId());

	do 
	{
		// Wait for a trigger event to start work
		WorkEvent->Wait(PollingInterval);
		if (!bRequestingExit)
		{
			Tick();
		}
	} 
	while (!bRequestingExit);

	return 0;
}

void FOnlineAsyncTaskManager::Stop(void)
{
	int32 NumInTasks = 0;
	{
		FScopeLock Lock(&InQueueLock);
		NumInTasks = InQueue.Num();
	}

	int32 NumOutTasks = 0;
	{
		FScopeLock Lock(&OutQueueLock);
		NumOutTasks = OutQueue.Num();
	}

	UE_LOG_ONLINE(Display, TEXT("FOnlineAsyncTaskManager::Stop() ActiveTask:%p Tasks[%d/%d]"), ActiveTask, NumInTasks, NumOutTasks);

	// Set the variable to requesting exit before we trigger the event
	bRequestingExit = true;
	WorkEvent->Trigger();
}

void FOnlineAsyncTaskManager::Exit(void)
{
	UE_LOG_ONLINE(Display, TEXT("FOnlineAsyncTaskManager::Exit() started"));

	FPlatformProcess::ReturnSynchEventToPool(WorkEvent);
	WorkEvent = nullptr;

	OnlineThreadId = 0;
	InvocationCount--;

	UE_LOG_ONLINE(Display, TEXT("FOnlineAsyncTaskManager::Exit() finished"));
}

void FOnlineAsyncTaskManager::AddToInQueue(FOnlineAsyncTask* NewTask)
{
	FScopeLock Lock(&InQueueLock);
	InQueue.Add(NewTask);
}

void FOnlineAsyncTaskManager::AddToOutQueue(FOnlineAsyncItem* CompletedItem)
{
	FScopeLock Lock(&OutQueueLock);
	OutQueue.Add(CompletedItem);
}

void FOnlineAsyncTaskManager::AddToParallelTasks(FOnlineAsyncTask* NewTask)
{
	NewTask->Initialize();

	FScopeLock LockParallelTasks(&ParallelTasksLock);

	ParallelTasks.Add( NewTask );
}

void FOnlineAsyncTaskManager::RemoveFromParallelTasks(FOnlineAsyncTask* OldTask)
{
	FScopeLock LockParallelTasks(&ParallelTasksLock);

	ParallelTasks.Remove( OldTask );
}

void FOnlineAsyncTaskManager::GameTick()
{
	// assert if not game thread
	check(IsInGameThread());

	FOnlineAsyncItem* Item = nullptr;
	int32 CurrentQueueSize = 0;

#if !UE_BUILD_SHIPPING
	const float TimeToWait = OSSConsoleVariables::CVarDelayAsyncTaskOutQueue.GetValueOnGameThread();
#endif

	do 
	{
		Item = nullptr;
		// Grab a completed task from the queue
		{
			FScopeLock LockOutQueue(&OutQueueLock);
			CurrentQueueSize = OutQueue.Num();
			if (CurrentQueueSize > 0)
			{
				Item = OutQueue[0];

#if !UE_BUILD_SHIPPING
				if (Item && Item->GetElapsedTime() >= TimeToWait)
				{
					OutQueue.RemoveAt(0);
				}
				else
				{
					Item = nullptr;
				}
#else
				OutQueue.RemoveAt(0);	
#endif
			}
		}

		if (Item)
		{
#if !UE_BUILD_SHIPPING
			if (TimeToWait > 0.0f)
			{
				UE_LOG(LogOnline, Verbose, TEXT("Async task '%s' finalizing after %f seconds"),
					*Item->ToString(),
					Item->GetElapsedTime());
			}
#endif

			// Finish work and trigger delegates
			Item->Finalize();
			Item->TriggerDelegates();
			delete Item;
			Item = nullptr;
		}
	}
	while (Item != nullptr);

	int32 QueueSize = 0;
	bool bHasActiveTask = false;
	{
		{
			FScopeLock LockInQueue(&InQueueLock);
			QueueSize = InQueue.Num();
		}
		{
			FScopeLock LockActiveTask(&ActiveTaskLock);
			if (ActiveTask != nullptr)
			{
				++QueueSize;
				bHasActiveTask = true;
			}
		}

		if (!bHasActiveTask && QueueSize > 0)
		{
			// Grab the current task from the queue
			FOnlineAsyncTask* Task = nullptr;
			{
				FScopeLock LockInQueue(&InQueueLock);
				Task = InQueue[0];
				InQueue.RemoveAt(0);
			}

			// Initialize the task before giving it away to the online thread
			Task->Initialize();
			{
				FScopeLock LockActiveTask(&ActiveTaskLock);
				ActiveTask = Task;
			}

			// Wake up the online thread
			WorkEvent->Trigger();
		}
	}

	SET_DWORD_STAT(STAT_Online_AsyncTasks, QueueSize);
}

void FOnlineAsyncTaskManager::Tick()
{
	SCOPE_CYCLE_COUNTER(STAT_Online_Async);

	// Tick Online services (possibly callbacks). 
	OnlineTick();

	{
		// Tick all the parallel tasks - Tick unrelated tasks together. 
		TArray<FOnlineAsyncTask*> CopyParallelTasks;

		// Grab a copy of the parallel list
		{
			FScopeLock LockParallelTasks(&ParallelTasksLock);
			CopyParallelTasks = ParallelTasks;
		}

		FOnlineAsyncTask* Task = nullptr;
		for (auto It = CopyParallelTasks.CreateIterator(); It; ++It)
		{
			Task = *It;
			Task->Tick();

			if (Task->IsDone())
			{
				if (Task->WasSuccessful())
				{
					UE_LOG(LogOnline, Verbose, TEXT("Async task '%s' succeeded in %f seconds (Parallel)"),
						*Task->ToString(),
						Task->GetElapsedTime());
				}
				else
				{
					UE_LOG(LogOnline, Warning, TEXT("Async task '%s' failed in %f seconds (Parallel)"),
						*Task->ToString(),
						Task->GetElapsedTime());
				}

				// Task is done, remove from the incoming queue and add to the outgoing queue, fixing up the original parallel task queue. 
				RemoveFromParallelTasks(Task);
				AddToOutQueue(Task);
			}
		}
	}

	{
		// Now process the serial "In" queue
		FOnlineAsyncTask* Task = nullptr;
		{
			FScopeLock LockActiveTask(&ActiveTaskLock);
			Task = ActiveTask;
		}

		if (Task)
		{
			Task->Tick();

			if (Task->IsDone())
			{
				if (Task->WasSuccessful())
				{
					UE_LOG(LogOnline, Verbose, TEXT("Async task '%s' succeeded in %f seconds"),
						*Task->ToString(),
						Task->GetElapsedTime());
				}
				else
				{
					UE_LOG(LogOnline, Warning, TEXT("Async task '%s' failed in %f seconds"),
						*Task->ToString(),
						Task->GetElapsedTime());
				}

				// Task is done, add to the outgoing queue
				AddToOutQueue(Task);

				{
					FScopeLock LockActiveTask(&ActiveTaskLock);
					ActiveTask = nullptr;
				}
			}
		}
	}
}
