// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AudioThread.cpp: Audio thread implementation.
=============================================================================*/

#include "AudioThread.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "HAL/ExceptionHandling.h"
#include "Misc/CoreStats.h"
#include "UObject/UObjectGlobals.h"
#include "Audio.h"
#include "HAL/LowLevelMemTracker.h"

//
// Globals
//

int32 GCVarSuspendAudioThread = 0;
TAutoConsoleVariable<int32> CVarSuspendAudioThread(TEXT("AudioThread.SuspendAudioThread"), GCVarSuspendAudioThread, TEXT("0=Resume, 1=Suspend"), ECVF_Cheat);

int32 GCVarAboveNormalAudioThreadPri = 0;
TAutoConsoleVariable<int32> CVarAboveNormalAudioThreadPri(TEXT("AudioThread.AboveNormalPriority"), GCVarAboveNormalAudioThreadPri, TEXT("0=Normal, 1=AboveNormal"), ECVF_Default);

struct FAudioThreadInteractor
{
	static void UseAudioThreadCVarSinkFunction()
	{
		static bool bLastSuspendAudioThread = false;
		const bool bSuspendAudioThread = (CVarSuspendAudioThread.GetValueOnGameThread() != 0);

		if (bLastSuspendAudioThread != bSuspendAudioThread)
		{
			bLastSuspendAudioThread = bSuspendAudioThread;
			if (GAudioThread)
			{
				if (bSuspendAudioThread)
				{
					FAudioThread::SuspendAudioThread();
				}
				else
				{
					FAudioThread::ResumeAudioThread();
				}
			}
			else if (GIsEditor)
			{
				UE_LOG(LogAudio, Warning, TEXT("Audio threading is disabled in the editor."));
			}
			else if (!FAudioThread::IsUsingThreadedAudio())
			{
				UE_LOG(LogAudio, Warning, TEXT("Cannot manipulate audio thread when disabled by platform or ini."));
			}
		}
	}
};

static FAutoConsoleVariableSink CVarUseAudioThreadSink(FConsoleCommandDelegate::CreateStatic(&FAudioThreadInteractor::UseAudioThreadCVarSinkFunction));

bool FAudioThread::bIsAudioThreadRunning = false;
bool FAudioThread::bIsAudioThreadSuspended = false;
bool FAudioThread::bUseThreadedAudio = false;
uint32 FAudioThread::CachedAudioThreadId = 0;
FRunnable* FAudioThread::AudioThreadRunnable = nullptr;


/** The audio thread main loop */
void AudioThreadMain( FEvent* TaskGraphBoundSyncEvent )
{
	FTaskGraphInterface::Get().AttachToThread(ENamedThreads::AudioThread);
	FPlatformMisc::MemoryBarrier();

	// Inform main thread that the audio thread has been attached to the taskgraph and is ready to receive tasks
	if( TaskGraphBoundSyncEvent != nullptr )
	{
		TaskGraphBoundSyncEvent->Trigger();
	}

	FTaskGraphInterface::Get().ProcessThreadUntilRequestReturn(ENamedThreads::AudioThread);
	FPlatformMisc::MemoryBarrier();
}

FAudioThread::FAudioThread()
{
	TaskGraphBoundSyncEvent	= FPlatformProcess::GetSynchEventFromPool(true);

	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(this, &FAudioThread::OnPreGarbageCollect);
	FCoreUObjectDelegates::GetPostGarbageCollect().AddRaw(this, &FAudioThread::OnPostGarbageCollect);
}

FAudioThread::~FAudioThread()
{
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().RemoveAll(this);
	FCoreUObjectDelegates::GetPostGarbageCollect().RemoveAll(this);

	FPlatformProcess::ReturnSynchEventToPool(TaskGraphBoundSyncEvent);
	TaskGraphBoundSyncEvent = nullptr;
}

void FAudioThread::SuspendAudioThread()
{
	check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);
	check(!bIsAudioThreadSuspended || CVarSuspendAudioThread.GetValueOnGameThread() != 0);
	if (IsAudioThreadRunning())
	{
		// Make GC wait on the audio thread finishing processing
		FAudioCommandFence AudioFence;
		AudioFence.BeginFence();
		AudioFence.Wait();

		CachedAudioThreadId = GAudioThreadId;
		GAudioThreadId = 0; // While we are suspended we will pretend we have no audio thread
		bIsAudioThreadSuspended = true;
		FPlatformMisc::MemoryBarrier();
		bIsAudioThreadRunning = false;
	}
	check(!bIsAudioThreadRunning);
}

void FAudioThread::ResumeAudioThread()
{
	check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);
	if (bIsAudioThreadSuspended && CVarSuspendAudioThread.GetValueOnGameThread() == 0)
	{
		GAudioThreadId = CachedAudioThreadId;
		CachedAudioThreadId = 0;
		bIsAudioThreadSuspended = false;
		FPlatformMisc::MemoryBarrier();
		bIsAudioThreadRunning = true;
	}
}

void FAudioThread::OnPreGarbageCollect()
{
	SuspendAudioThread();
}

void FAudioThread::OnPostGarbageCollect()
{
	ResumeAudioThread();
}

bool FAudioThread::Init()
{ 
	GAudioThreadId = FPlatformTLS::GetCurrentThreadId();
	return true; 
}

void FAudioThread::Exit()
{
	GAudioThreadId = 0;
}

uint32 FAudioThread::Run()
{
	LLM_SCOPE(ELLMTag::Audio);

	FPlatformProcess::SetupAudioThread();
	AudioThreadMain( TaskGraphBoundSyncEvent );
	return 0;
}

void FAudioThread::SetUseThreadedAudio(const bool bInUseThreadedAudio)
{
	if (bIsAudioThreadRunning && !bInUseThreadedAudio)
	{
		UE_LOG(LogAudio, Error, TEXT("You cannot disable using threaded audio once the thread has already begun running."));
	}
	else
	{
		bUseThreadedAudio = bInUseThreadedAudio;
	}
}

void FAudioThread::RunCommandOnAudioThread(TFunction<void()> InFunction, const TStatId InStatId)
{
	check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);
	if (bIsAudioThreadRunning)
	{
		FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(InFunction), InStatId, nullptr, ENamedThreads::AudioThread);
	}
	else
	{
		FScopeCycleCounter ScopeCycleCounter(InStatId);
		InFunction();
	}
}

void FAudioThread::RunCommandOnGameThread(TFunction<void()> InFunction, const TStatId InStatId)
{
	if (bIsAudioThreadRunning)
	{
		check(GAudioThreadId && FPlatformTLS::GetCurrentThreadId() == GAudioThreadId);
		FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(InFunction), InStatId, nullptr, ENamedThreads::GameThread);
	}
	else
	{
		check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);
		FScopeCycleCounter ScopeCycleCounter(InStatId);
		InFunction();
	}
}

void FAudioThread::StartAudioThread()
{
	check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);

	check(!bIsAudioThreadRunning);
	check(!bIsAudioThreadSuspended);
	if (bUseThreadedAudio)
	{
		check(GAudioThread == nullptr);

		static uint32 ThreadCount = 0;
		check(!ThreadCount); // we should not stop and restart the audio thread; it is complexity we don't need.

		bIsAudioThreadRunning = true;

		// Create the audio thread.
		AudioThreadRunnable = new FAudioThread();

		GAudioThread = FRunnableThread::Create(AudioThreadRunnable, *FName(NAME_AudioThread).GetPlainNameString(), 0, (CVarAboveNormalAudioThreadPri.GetValueOnGameThread() == 0) ? TPri_BelowNormal : TPri_AboveNormal, FPlatformAffinity::GetAudioThreadMask());

		// Wait for audio thread to have taskgraph bound before we dispatch any tasks for it.
		((FAudioThread*)AudioThreadRunnable)->TaskGraphBoundSyncEvent->Wait();

		// ensure the thread has actually started and is idling
		FAudioCommandFence Fence;
		Fence.BeginFence();
		Fence.Wait();

		ThreadCount++;

		if (CVarSuspendAudioThread.GetValueOnGameThread() != 0)
		{
			SuspendAudioThread();
		}
	}
}

void FAudioThread::StopAudioThread()
{
	check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);
	check(!bIsAudioThreadSuspended || CVarSuspendAudioThread.GetValueOnGameThread() != 0);

	if (!bIsAudioThreadRunning && CachedAudioThreadId == 0)
	{
		return;
	}

	// unregister
	IConsoleManager::Get().RegisterThreadPropagation();

	FGraphEventRef QuitTask = TGraphTask<FReturnGraphTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(ENamedThreads::AudioThread);

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_StopAudioThread);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(QuitTask, ENamedThreads::GameThread_Local);
	}

	// Wait for the audio thread to return.
	GAudioThread->WaitForCompletion();

	bIsAudioThreadRunning = false;

	// Destroy the audio thread objects.
	delete GAudioThread;
	GAudioThread = nullptr;
			
	delete AudioThreadRunnable;
	AudioThreadRunnable = nullptr;
}

void FAudioCommandFence::BeginFence()
{
	check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId); // this could be relaxed, but for now, we are going to require all fences are set from the GT
	if (FAudioThread::IsAudioThreadRunning())
	{
		DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.FenceAudioCommand"),
			STAT_FNullGraphTask_FenceAudioCommand,
			STATGROUP_TaskGraphTasks);

		CompletionEvent = TGraphTask<FNullGraphTask>::CreateTask(NULL, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(
			GET_STATID(STAT_FNullGraphTask_FenceAudioCommand), ENamedThreads::AudioThread);
	}
	else
	{
		CompletionEvent = nullptr;
	}
}

bool FAudioCommandFence::IsFenceComplete() const
{
	check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId); // this could be relaxed, but for now, we are going to require all fences are set from the GT
	if (!CompletionEvent.GetReference() || CompletionEvent->IsComplete())
	{
		CompletionEvent = nullptr; // this frees the handle for other uses, the NULL state is considered completed
		return true;
	}
	check(FAudioThread::IsAudioThreadRunning());
	return false;
}

/**
 * Waits for pending fence commands to retire.
 */
void FAudioCommandFence::Wait(bool bProcessGameThreadTasks) const
{
	if (!IsFenceComplete()) // this checks the current thread
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FAudioCommandFence_Wait);
		const double StartTime = FPlatformTime::Seconds();
		FEvent* Event = FPlatformProcess::GetSynchEventFromPool();
		FTaskGraphInterface::Get().TriggerEventWhenTaskCompletes(Event, CompletionEvent, ENamedThreads::GameThread);

		bool bDone;
		const uint32 WaitTime = 35;
		do
		{
			bDone = Event->Wait(WaitTime);
			float ThisTime = FPlatformTime::Seconds() - StartTime;
			UE_CLOG(ThisTime > .036f, LogAudio, Warning, TEXT("Waited %fms for audio thread."), ThisTime * 1000.0f);
		} while (!bDone);

		// Return the event to the pool and decrement the recursion counter.
		FPlatformProcess::ReturnSynchEventToPool(Event);
		Event = nullptr;
	}
}
