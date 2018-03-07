// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidEventManager.h"
#include "AndroidApplication.h"
#include "AudioDevice.h"
#include "CallbackDevice.h"
#include <android/native_window.h> 
#include <android/native_window_jni.h> 
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "RenderingThread.h"
#include "UnrealEngine.h"

DEFINE_LOG_CATEGORY(LogAndroidEvents);

FAppEventManager* FAppEventManager::sInstance = NULL;
// whether EventManager started doing ticks
static volatile bool bStartedTicking = false;

FAppEventManager* FAppEventManager::GetInstance()
{
	if(!sInstance)
	{
		sInstance = new FAppEventManager();
	}

	return sInstance;
}


void FAppEventManager::Tick()
{
	bStartedTicking = true;
	
	static const bool bIsDaydreamApp = FAndroidMisc::IsDaydreamApplication();
	bool bWindowCreatedThisTick = false;
	
	while (!Queue.IsEmpty())
	{
		bool bDestroyWindow = false;

		FAppEventData Event = DequeueAppEvent();

		switch (Event.State)
		{
		case APP_EVENT_STATE_WINDOW_CREATED:
			bCreateWindow = true;
			PendingWindow = (ANativeWindow*)Event.Data;

			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("APP_EVENT_STATE_WINDOW_CREATED, %d, %d, %d"), int(bRunning), int(bHaveWindow), int(bHaveGame));
			break;
		
		case APP_EVENT_STATE_WINDOW_RESIZED:
		case APP_EVENT_STATE_WINDOW_CHANGED:
			// React on device orientation/windowSize changes only when application has window
			// In case window was created this tick it should already has correct size
			if (bHaveWindow && !bWindowCreatedThisTick)
			{
				ExecWindowResized();
			}
			break;
		case APP_EVENT_STATE_SAVE_STATE:
			bSaveState = true; //todo android: handle save state.
			break;
		case APP_EVENT_STATE_WINDOW_DESTROYED:
			if (bIsDaydreamApp)
			{
				bCreateWindow = false;
			}
			else
			{
				if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice() && GEngine->XRSystem->GetHMDDevice()->IsHMDConnected())
				{
					// delay the destruction until after the renderer teardown on GearVR
					bDestroyWindow = true;
				}
				else
				{
					FAndroidAppEntry::DestroyWindow();
					FAndroidWindow::SetHardwareWindow(NULL);
				}
			}

			bHaveWindow = false;
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("APP_EVENT_STATE_WINDOW_DESTROYED, %d, %d, %d"), int(bRunning), int(bHaveWindow), int(bHaveGame));
			break;
		case APP_EVENT_STATE_ON_START:
			//doing nothing here
			break;
		case APP_EVENT_STATE_ON_DESTROY:
			if (FTaskGraphInterface::IsRunning())
			{
				FGraphEventRef WillTerminateTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
				{
					FCoreDelegates::ApplicationWillTerminateDelegate.Broadcast();
				}, TStatId(), NULL, ENamedThreads::GameThread);
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(WillTerminateTask);
			}
			GIsRequestingExit = true; //destroy immediately. Game will shutdown.
			FirstInitialized = false;
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("APP_EVENT_STATE_ON_DESTROY"));
			break;
		case APP_EVENT_STATE_ON_STOP:
			bHaveGame = false;
			break;
		case APP_EVENT_STATE_ON_PAUSE:
			bHaveGame = false;
			break;
		case APP_EVENT_STATE_ON_RESUME:
			bHaveGame = true;
			break;

		// window focus events that follow their own hierarchy, and might or might not respect App main events hierarchy

		case APP_EVENT_STATE_WINDOW_GAINED_FOCUS: 
			bWindowInFocus = true;
			break;
		case APP_EVENT_STATE_WINDOW_LOST_FOCUS:
			bWindowInFocus = false;
			break;

		default:
			UE_LOG(LogAndroidEvents, Display, TEXT("Application Event : %u  not handled. "), Event.State);
		}

		if (bCreateWindow)
		{
			// wait until activity is in focus.
			if (bWindowInFocus) 
			{
				ExecWindowCreated();
				bCreateWindow = false;
				bHaveWindow = true;
				bWindowCreatedThisTick = true;

				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("ExecWindowCreated, %d, %d, %d"), int(bRunning), int(bHaveWindow), int(bHaveGame));
			}
		}

		if (!bRunning && bHaveWindow && bHaveGame)
		{
			ResumeRendering();
			ResumeAudio();

			// broadcast events after the rendering thread has resumed
			if (FTaskGraphInterface::IsRunning())
			{
				FGraphEventRef EnterForegroundTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
				{
					FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
				}, TStatId(), NULL, ENamedThreads::GameThread);

				FGraphEventRef ReactivateTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
				{
					FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
				}, TStatId(), EnterForegroundTask, ENamedThreads::GameThread);
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(ReactivateTask);

				extern void AndroidThunkCpp_ShowHiddenAlertDialog();
				AndroidThunkCpp_ShowHiddenAlertDialog();
			}

			bRunning = true;
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Execution has been resumed!"));
		}
		else if (bRunning && (!bHaveWindow || !bHaveGame))
		{
			// broadcast events before rendering thread suspends
			if (FTaskGraphInterface::IsRunning())
			{
				FGraphEventRef DeactivateTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
				{
					FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
				}, TStatId(), NULL, ENamedThreads::GameThread);
				FGraphEventRef EnterBackgroundTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
				{
					FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
				}, TStatId(), DeactivateTask, ENamedThreads::GameThread);
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(EnterBackgroundTask);
			}

			PauseRendering();
			PauseAudio();

			bRunning = false;
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Execution has been paused..."));
		}

		if (bDestroyWindow)
		{
			FAndroidAppEntry::DestroyWindow();
			FAndroidWindow::SetHardwareWindow(NULL);
			bDestroyWindow = false;

			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAndroidAppEntry::DestroyWindow() called"));
		}
	}

	if (EmptyQueueHandlerEvent)
	{
		EmptyQueueHandlerEvent->Trigger();
	}
	if (bIsDaydreamApp)
	{
		if (!bRunning && FAndroidWindow::GetHardwareWindow() != NULL)
		{
			EventHandlerEvent->Wait();
		}
	}
	else
	{
		if (!bRunning && FirstInitialized)
		{
			EventHandlerEvent->Wait();
		}
	}
}

void FAppEventManager::TriggerEmptyQueue()
{
	if (EmptyQueueHandlerEvent)
	{
		EmptyQueueHandlerEvent->Trigger();
	}
}

FAppEventManager::FAppEventManager():
	EventHandlerEvent(nullptr)
	,EmptyQueueHandlerEvent(nullptr)
	,FirstInitialized(false)
	,bCreateWindow(false)
	,bWindowInFocus(true)
	,bSaveState(false)
	,bAudioPaused(false)
	,PendingWindow(NULL)
	,bHaveWindow(false)
	,bHaveGame(false)
	,bRunning(false)
{
	pthread_mutex_init(&MainMutex, NULL);
	pthread_mutex_init(&QueueMutex, NULL);

	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MobileContentScaleFactor"));
	check(CVar);
	CVar->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&FAppEventManager::OnScaleFactorChanged));
}

void FAppEventManager::OnScaleFactorChanged(IConsoleVariable* CVar)
{
	if (CVar->GetFlags() & ECVF_SetByConsole)
	{
		FAppEventManager::GetInstance()->ExecWindowResized();
	}
}

void FAppEventManager::HandleWindowCreated(void* InWindow)
{
	static const bool bIsDaydreamApp = FAndroidMisc::IsDaydreamApplication();
	if (bIsDaydreamApp)
	{
		// We must ALWAYS set the hardware window immediately,
		// Otherwise we will temporarily end up with an abandoned Window
		// when the application is pausing/resuming. This is likely
		// to happen in a Gvr app due to the DON flow pushing an activity
		// during initialization.

		int rc = pthread_mutex_lock(&MainMutex);
		check(rc == 0);

		// If we already have a window, destroy it
		ExecDestroyWindow();

		FAndroidWindow::SetHardwareWindow(InWindow);

		rc = pthread_mutex_unlock(&MainMutex);
		check(rc == 0);

		// Make sure window will not be deleted until event is processed
		// Window could be deleted by OS while event queue stuck at game start-up phase
		FAndroidWindow::AcquireWindowRef((ANativeWindow*)InWindow);

		EnqueueAppEvent(APP_EVENT_STATE_WINDOW_CREATED, InWindow);
		return;
	}

	// Make sure window will not be deleted until event is processed
	// Window could be deleted by OS while event queue stuck at game start-up phase
	FAndroidWindow::AcquireWindowRef((ANativeWindow*)InWindow);

	if (!bStartedTicking)
	{
		//This cannot wait until first tick. 
		int rc = pthread_mutex_lock(&MainMutex);
		check(rc == 0);

		check(FAndroidWindow::GetHardwareWindow() == NULL);
		FAndroidWindow::SetHardwareWindow(InWindow);
		FirstInitialized = true;

		rc = pthread_mutex_unlock(&MainMutex);
		check(rc == 0);
	}
	
	EnqueueAppEvent(APP_EVENT_STATE_WINDOW_CREATED, InWindow);
}

void FAppEventManager::HandleWindowClosed()
{
	static const bool bIsDaydreamApp = FAndroidMisc::IsDaydreamApplication();
	if (bIsDaydreamApp)
	{
		// We must ALWAYS destroy the hardware window immediately,
		// Otherwise we will temporarily end up with an abandoned Window
		// when the application is pausing/resuming. This is likely
		// to happen in a Gvr app due to the DON flow pushing an activity
		// during initialization.

		int rc = pthread_mutex_lock(&MainMutex);
		check(rc == 0);

		ExecDestroyWindow();

		rc = pthread_mutex_unlock(&MainMutex);
		check(rc == 0);

		EnqueueAppEvent(APP_EVENT_STATE_WINDOW_DESTROYED, NULL);
		return;
	}
	
	if (!bStartedTicking)
	{
		// If engine is not ticking yet, and window is being destroyed
		// 1. Immediately release current window
		// 2. Unwind to queue to APP_EVENT_STATE_WINDOW_CREATED event
		int rc = pthread_mutex_lock(&MainMutex);
		check(rc == 0);
		FAndroidWindow::SetHardwareWindow(nullptr);
		while (!Queue.IsEmpty())
		{
			FAppEventData Event = DequeueAppEvent();
			if (Event.State == APP_EVENT_STATE_WINDOW_CREATED)
			{
				ANativeWindow* DestroyedWindow = (ANativeWindow*)Event.Data;
				FAndroidWindow::ReleaseWindowRef(DestroyedWindow);
				break;
			}
		}
				
		rc = pthread_mutex_unlock(&MainMutex);
		check(rc == 0);
	}
	else
	{
		EnqueueAppEvent(APP_EVENT_STATE_WINDOW_DESTROYED, NULL);
	}
}


void FAppEventManager::SetEventHandlerEvent(FEvent* InEventHandlerEvent)
{
	EventHandlerEvent = InEventHandlerEvent;
}

void FAppEventManager::SetEmptyQueueHandlerEvent(FEvent* InEventHandlerEvent)
{
	EmptyQueueHandlerEvent = InEventHandlerEvent;
}

void FAppEventManager::PauseRendering()
{
	if(GUseThreadedRendering )
	{
		if (GIsThreadedRendering)
		{
			StopRenderingThread(); 
		}
	}
	else
	{
		RHIReleaseThreadOwnership();
	}
}


void FAppEventManager::ResumeRendering()
{
	if( GUseThreadedRendering )
	{
		if (!GIsThreadedRendering)
		{
			StartRenderingThread();
		}
	}
	else
	{
		RHIAcquireThreadOwnership();
	}
}


void FAppEventManager::ExecWindowCreated()
{
	UE_LOG(LogAndroidEvents, Display, TEXT("ExecWindowCreated"));

	static bool bIsDaydreamApp = FAndroidMisc::IsDaydreamApplication();
	if (!bIsDaydreamApp)
	{
		check(PendingWindow);
		FAndroidWindow::SetHardwareWindow(PendingWindow);
	}

	// When application launched while device is in sleep mode SystemResolution could be set to opposite orientation values
	// Force to update SystemResolution to current values whenever we create a new window
	FPlatformRect ScreenRect = FAndroidWindow::GetScreenRect();
	FSystemResolution::RequestResolutionChange(ScreenRect.Right, ScreenRect.Bottom, EWindowMode::Fullscreen);

	// ReInit with the new window handle, null for daydream case.
	FAndroidAppEntry::ReInitWindow(!bIsDaydreamApp ? PendingWindow : nullptr);

	if (!bIsDaydreamApp)
	{
		// We hold this reference to ensure that window will not be deleted while game starting up
		// release it when window is finally initialized
		FAndroidWindow::ReleaseWindowRef(PendingWindow);
		PendingWindow = nullptr;
	}

	FAndroidApplication::OnWindowSizeChanged();
}

void FAppEventManager::ExecWindowResized()
{
	if (bRunning)
	{
		FlushRenderingCommands();
	}
	FAndroidWindow::InvalidateCachedScreenRect();
	FAndroidAppEntry::ReInitWindow();
	FAndroidApplication::OnWindowSizeChanged();
}

void FAppEventManager::ExecDestroyWindow()
{
	if (FAndroidWindow::GetHardwareWindow() != NULL)
	{
		FAndroidWindow::ReleaseWindowRef((ANativeWindow*)FAndroidWindow::GetHardwareWindow());

		FAndroidAppEntry::DestroyWindow();
		FAndroidWindow::SetHardwareWindow(NULL);
	}
}

void FAppEventManager::PauseAudio()
{
	bAudioPaused = true;

	UE_LOG(LogTemp, Log, TEXT("Android pause audio"));

	FAudioDevice* AudioDevice = GEngine->GetMainAudioDevice();
	if (AudioDevice)
	{
		if (AudioDevice->IsAudioMixerEnabled())
		{
			AudioDevice->SuspendContext();
		}
		else
	{
		GEngine->GetMainAudioDevice()->Suspend(false);
	}
	}
}


void FAppEventManager::ResumeAudio()
{
	bAudioPaused = false;

	UE_LOG(LogTemp, Log, TEXT("Android resume audio"));

	FAudioDevice* AudioDevice = GEngine->GetMainAudioDevice();
	if (AudioDevice)
	{
		if (AudioDevice->IsAudioMixerEnabled())
		{
			AudioDevice->ResumeContext();
		}
		else
	{
		GEngine->GetMainAudioDevice()->Suspend(true);
	}
	}
}


void FAppEventManager::EnqueueAppEvent(EAppEventState InState, void* InData)
{
	FAppEventData Event;
	Event.State = InState;
	Event.Data = InData;

	int rc = pthread_mutex_lock(&QueueMutex);
	check(rc == 0);
	Queue.Enqueue(Event);

	if (EmptyQueueHandlerEvent)
	{
		EmptyQueueHandlerEvent->Reset();
	}

	rc = pthread_mutex_unlock(&QueueMutex);
	check(rc == 0);

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("LogAndroidEvents: EnqueueAppEvent : %u, %u, tid = %d"), InState, (uintptr_t)InData, gettid());
}


FAppEventData FAppEventManager::DequeueAppEvent()
{
	int rc = pthread_mutex_lock(&QueueMutex);
	check(rc == 0);

	FAppEventData OutData;
	Queue.Dequeue( OutData );

	rc = pthread_mutex_unlock(&QueueMutex);
	check(rc == 0);

	UE_LOG(LogAndroidEvents, Display, TEXT("DequeueAppEvent : %u, %u"), OutData.State, (uintptr_t)OutData.Data)

	return OutData;
}


bool FAppEventManager::IsGamePaused()
{
	return !bRunning;
}


bool FAppEventManager::IsGameInFocus()
{
	return (bWindowInFocus && bHaveWindow);
}


bool FAppEventManager::WaitForEventInQueue(EAppEventState InState, double TimeoutSeconds)
{
	bool FoundEvent = false;
	double StopTime = FPlatformTime::Seconds() + TimeoutSeconds;

	TQueue<FAppEventData, EQueueMode::Spsc> HoldingQueue;
	while (!FoundEvent)
	{
		int rc = pthread_mutex_lock(&QueueMutex);
		check(rc == 0);

		// Copy the existing queue (and check for our event)
		while (!Queue.IsEmpty())
		{
			FAppEventData OutData;
			Queue.Dequeue(OutData);

			if (OutData.State == InState)
				FoundEvent = true;

			HoldingQueue.Enqueue(OutData);
		}

		if (FoundEvent)
			break;

		// Time expired?
		if (FPlatformTime::Seconds() > StopTime)
			break;

		// Unlock for new events and wait a bit before trying again
		rc = pthread_mutex_unlock(&QueueMutex);
		check(rc == 0);
		FPlatformProcess::Sleep(0.01f);
	}

	// Add events back to queue from holding
	while (!HoldingQueue.IsEmpty())
	{
		FAppEventData OutData;
		HoldingQueue.Dequeue(OutData);
		Queue.Enqueue(OutData);
	}

	int rc = pthread_mutex_unlock(&QueueMutex);
	check(rc == 0);

	return FoundEvent;
}

extern volatile bool GEventHandlerInitialized;

void FAppEventManager::WaitForEmptyQueue()
{
	if (EmptyQueueHandlerEvent && GEventHandlerInitialized && !GIsRequestingExit)
	{
		EmptyQueueHandlerEvent->Wait();
	}
}

