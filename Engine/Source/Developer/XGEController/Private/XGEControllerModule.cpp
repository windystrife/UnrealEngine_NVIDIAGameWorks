// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XGEControllerInterface.h"
#include "Containers/Queue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformNamedPipe.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformFile.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeLock.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Serialization/MemoryWriter.h"

#if WITH_XGE_CONTROLLER

// Comma separated list of executable file names which should be intercepted by XGE.
// Update this list if adding new tasks.
#define XGE_INTERCEPT_EXE_NAMES TEXT("ShaderCompileWorker")

#if PLATFORM_WINDOWS

	#include "AllowWindowsPlatformTypes.h"
	#include <winreg.h>
	#include "HideWindowsPlatformTypes.h"

	#define XGE_CONTROL_WORKER_NAME		TEXT("XGEControlWorker")
	#define XGE_CONTROL_WORKER_FILENAME	TEXT("XGEControlWorker.exe")
	#define XGE_CONTROL_WORKER_REL_DIR	TEXT("../../../Engine/Binaries/Win64/")

#else
#error XGE Controller is not supported on non-Windows platforms.
#endif

DEFINE_LOG_CATEGORY_STATIC(LogXGEController, Log, Log);

namespace XGEControllerVariables
{
	int32 Enabled = 1;
	FAutoConsoleVariableRef CVarXGEControllerEnabled(
		TEXT("r.XGEController.Enabled"),
		Enabled,
		TEXT("Enables or disables the use of XGE for various build tasks in the engine.\n")
		TEXT("0: Local builds only. \n")
		TEXT("1: Distribute builds using XGE (default)."),
		ECVF_ReadOnly); // Must be set on start-up, e.g. via config ini

	float Timeout = 2.0f;
	FAutoConsoleVariableRef CVarXGEControllerTimeout(
		TEXT("r.XGEController.Timeout"),
		Timeout,
		TEXT("The time, in seconds, to wait after all tasks have been completed before shutting down the controller. (default: 2 seconds)."),
		ECVF_Default);
}

class FXGEControllerModule : public IXGEController
{
	struct FTask
	{
		uint32 ID;
		FString Command;
		FString CommandArgs;
		TPromise<FXGETaskResult> Promise;

		FTask(uint32 ID, const FString& Command, const FString& CommandArgs, TPromise<FXGETaskResult>&& Promise)
			: ID(ID)
			, Command(Command)
			, CommandArgs(CommandArgs)
			, Promise(MoveTemp(Promise))
		{}
	};

	struct FTaskResponse
	{
		uint32 ID;
		int32 ReturnCode;
	};

	bool bSupported;
	bool bInitialized;

	FThreadSafeCounter NextFileID;
	FThreadSafeCounter NextTaskID;

	FProcHandle BuildProcessHandle;

	const FString ControlWorkerDirectory;
	const FString RootWorkingDirectory;
	const FString WorkingDirectory;
	const FString PipeName;
	FString XGConsolePath;

	// Taken when accessing the PendingTasks and DispatchedTasks members.
	FCriticalSection* TasksCS;

	// Queue of tasks submitted by the engine, but not yet dispatched to the controller.
	TQueue<FTask*> PendingTasks;

	// Map of tasks dispatched to the controller and running within XGE, that have not yet finished.
	TMap<uint32, FTask*> DispatchedTasks;

	bool bShutdown;
	bool bRestartWorker;
	TFuture<void> WriteOutThreadFuture;
	TFuture<void> ReadBackThreadFuture;

	FEvent* WriteOutThreadEvent;

	// We need two pipes, as the named pipe API does not support simultaneous read/write on two threads.
	FPlatformNamedPipe InputNamedPipe, OutputNamedPipe;

	volatile uint32 LastEventTime;

	inline bool AreTasksPending()
	{
		FScopeLock Lock(TasksCS);
		return !PendingTasks.IsEmpty();
	}

	inline bool AreTasksDispatchedOrPending()
	{
		FScopeLock Lock(TasksCS);
		return DispatchedTasks.Num() > 0 || !PendingTasks.IsEmpty();
	}

public:
	FXGEControllerModule();
	virtual ~FXGEControllerModule();

	virtual void StartupModule() override final;
	virtual void ShutdownModule() override final;

	virtual bool IsSupported() override final;

	virtual FString CreateUniqueFilePath() override final;
	virtual TFuture<FXGETaskResult> EnqueueTask(const FString& Command, const FString& CommandArgs) override final;

	void WriteOutThreadProc();
	void ReadBackThreadProc();

	void CleanWorkingDirectory();
};

FXGEControllerModule::FXGEControllerModule()
	: bShutdown(false)
	, bSupported(false)
	, bInitialized(false)
	, bRestartWorker(false)
	, ControlWorkerDirectory(FPaths::ConvertRelativePathToFull(XGE_CONTROL_WORKER_REL_DIR))
	, RootWorkingDirectory(FString::Printf(TEXT("%sUnrealXGEWorkingDir/"), FPlatformProcess::UserTempDir()))
	, WorkingDirectory(RootWorkingDirectory + FGuid::NewGuid().ToString(EGuidFormats::Digits))
	, PipeName(FString::Printf(TEXT("UnrealEngine-XGE-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)))
	, WriteOutThreadEvent(FPlatformProcess::CreateSynchEvent(false))
	, TasksCS(new FCriticalSection)
	, LastEventTime(0)
{}

FXGEControllerModule::~FXGEControllerModule()
{
	if (WriteOutThreadEvent)
	{
		delete WriteOutThreadEvent;
		WriteOutThreadEvent = nullptr;
	}

	if (TasksCS)
	{
		delete TasksCS;
		TasksCS = nullptr;
	}
}

bool FXGEControllerModule::IsSupported()
{
	if (bInitialized)
		return bSupported;

#if PLATFORM_WINDOWS

	// Check the command line to see if the XGE controller has been enabled/disabled.
	// This overrides the value of the console variable.
	if (FParse::Param(FCommandLine::Get(), TEXT("xgecontroller")))
	{
		XGEControllerVariables::Enabled = 1;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("noxgecontroller")))
	{
		XGEControllerVariables::Enabled = 0;
	}

	// Check for a valid installation of Incredibuild by seeing if xgconsole.exe exists.
	if (XGEControllerVariables::Enabled == 1)
	{
		// Try to find xgConsole.exe from the PATH environment variable
		FString PathString;
		{
			TCHAR EnvVariable[32 * 1024];
			FPlatformMisc::GetEnvironmentVariable(TEXT("Path"), EnvVariable, ARRAY_COUNT(EnvVariable));
			FString EnvString(EnvVariable);
			int32 PathStart = EnvString.Find(TEXT("Xoreax\\IncrediBuild"), ESearchCase::IgnoreCase, ESearchDir::FromStart);
			if (PathStart != INDEX_NONE)
			{
				// Move to the front of this string. The +1 is to change the return value from -1 to 0 (signifying our path was at the start of the string) or to move us past the ";"
				PathStart = EnvString.Find(TEXT(";"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, PathStart) + 1;
				int32 PathEnd = EnvString.Find(TEXT(";"), ESearchCase::CaseSensitive, ESearchDir::FromStart, PathStart);
				if (PathEnd == INDEX_NONE)
				{
					PathEnd = EnvString.Len();
				}

				FString Directory = EnvString.Mid(PathStart, PathEnd - PathStart);
				PathString = FPaths::Combine(Directory, TEXT("xgConsole.exe"));
			}
		}

		// List of possible paths to xgconsole.exe
		const FString Paths[] =
		{
			TEXT("C:\\Program Files\\Xoreax\\IncrediBuild\\xgConsole.exe"),
			TEXT("C:\\Program Files (x86)\\Xoreax\\IncrediBuild\\xgConsole.exe"),
			*PathString
		};

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		bool bFound = false;
		for (const FString& Path : Paths)
		{
			if (!Path.IsEmpty() && PlatformFile.FileExists(*Path))
			{
				bFound = true;
				XGConsolePath = Path;
				break;
			}
		}

		if (!bFound)
		{
			UE_LOG(LogXGEController, Warning, TEXT("Cannot use XGE Controller as Incredibuild is not installed on this machine."));
			XGEControllerVariables::Enabled = 0;
		}
		else
		{
			// xgConsole.exe has been found.
			// Check we have a compatible version of XGE by finding the version registry key.
			int32 Version = 0;

			HKEY RegistryKey;
			if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Xoreax\\IncrediBuild\\Builder"),              0, KEY_READ, &RegistryKey) == ERROR_SUCCESS ||
				RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\WOW6432Node\\Xoreax\\IncrediBuild\\Builder"), 0, KEY_READ, &RegistryKey) == ERROR_SUCCESS)
			{
				DWORD Type;
				BYTE Buffer[512] = {};
				DWORD Size = sizeof(Buffer);
				if (RegQueryValueEx(RegistryKey, TEXT("Version"), nullptr, &Type, Buffer, &Size) == ERROR_SUCCESS)
				{
					if (Type == REG_SZ && Size > 0 && FCString::IsNumeric((TCHAR*)Buffer))
					{
						Version = FCString::Atoi((TCHAR*)Buffer);
					}
				}

				RegCloseKey(RegistryKey);
			}

			if (Version == 0)
			{
				UE_LOG(LogXGEController, Warning, TEXT("Cannot determine XGE version. XGE Shader compilation with the interception interface may fail."));
			}
			else if (Version < 1002867)
			{
				UE_LOG(LogXGEController, Error, TEXT("XGE version 8.01 (build 1867) or higher is required for XGE shader compilation with the interception interface."));
				XGEControllerVariables::Enabled = 0;
			}
		}
	}

	return (bSupported = (XGEControllerVariables::Enabled == 1));

#else

	// Not supported on other platforms.
	return false;

#endif
}

void FXGEControllerModule::CleanWorkingDirectory()
{
	// Only clean the directory if we are the only instance running,
	// and we're not running in multi-process mode.
	if ((GIsFirstInstance) && !FParse::Param(FCommandLine::Get(), TEXT("Multiprocess")))
	{
		UE_LOG(LogXGEController, Log, TEXT("Cleaning working directory: %s"), *RootWorkingDirectory);
		IFileManager::Get().DeleteDirectory(*RootWorkingDirectory, false, true);
	}
}

void FXGEControllerModule::StartupModule()
{
	check(!bInitialized);

	CleanWorkingDirectory();

	bShutdown = false;
	if (IsSupported())
	{
		WriteOutThreadFuture = Async<void>(EAsyncExecution::Thread, [this]() { WriteOutThreadProc(); });
	}

	bInitialized = true;
}

void FXGEControllerModule::ShutdownModule()
{
	check(bInitialized);

	if (bSupported)
	{
		bShutdown = true;
		WriteOutThreadEvent->Trigger();

		// Wait for worker threads to exit
		if (WriteOutThreadFuture.IsValid())
		{
			WriteOutThreadFuture.Wait();
			WriteOutThreadFuture = TFuture<void>();
		}

		// Cancel any remaining tasks
		for (auto Iter = DispatchedTasks.CreateIterator(); Iter; ++Iter)
		{
			FXGETaskResult Result;
			Result.ReturnCode = 0;
			Result.bCompleted = false;

			FTask* Task = Iter.Value();
			Task->Promise.SetValue(Result);
			delete Task;
		}

		FTask* Task;
		while (PendingTasks.Dequeue(Task))
		{
			FXGETaskResult Result;
			Result.ReturnCode = 0;
			Result.bCompleted = false;
			Task->Promise.SetValue(Result);
			delete Task;
		}

		PendingTasks.Empty();
		DispatchedTasks.Empty();
	}

	CleanWorkingDirectory();
	bInitialized = false;
}

void FXGEControllerModule::WriteOutThreadProc()
{
	do
	{
		bRestartWorker = false;

		while (!AreTasksPending() && !bShutdown)
			WriteOutThreadEvent->Wait();

		if (bShutdown)
			return;

		// To handle spaces in the engine path, we just pass the XGEController.exe filename to xgConsole,
		// and set the working directory of xgConsole.exe to the engine binaries folder below.
		FString XGConsoleArgs = FString::Printf(TEXT("/VIRTUALIZEDIRECTX /allowremote=\"%s\" /allowintercept=\"%s\" /title=\"Unreal Engine XGE Tasks\" /monitordirs=\"%s\" /command=\"%s -xgecontroller %s\""),
			XGE_INTERCEPT_EXE_NAMES,
			XGE_CONTROL_WORKER_NAME,
			*WorkingDirectory,
			XGE_CONTROL_WORKER_FILENAME,
			*PipeName);

		// Create the output pipe as a server...
		if (!OutputNamedPipe.Create(FString::Printf(TEXT("\\\\.\\pipe\\%s-A"), *PipeName), true, false))
		{
			UE_LOG(LogXGEController, Fatal, TEXT("Failed to create the output XGE named pipe."));
		}

		// Start the controller process
		uint32 XGConsoleProcID = 0;
		BuildProcessHandle = FPlatformProcess::CreateProc(*XGConsolePath, *XGConsoleArgs, false, false, true, &XGConsoleProcID, 0, *ControlWorkerDirectory, nullptr);
		if (!BuildProcessHandle.IsValid())
		{
			UE_LOG(LogXGEController, Fatal, TEXT("Failed to launch the XGE control worker process."));
		}

		// Wait for the controller to connect to the output pipe
		if (!OutputNamedPipe.OpenConnection())
		{
			UE_LOG(LogXGEController, Fatal, TEXT("Failed to open a connection on the output XGE named pipe."));
		}

		// Connect the input pipe (controller is the server)...
		if (!InputNamedPipe.Create(FString::Printf(TEXT("\\\\.\\pipe\\%s-B"), *PipeName), false, false))
		{
			UE_LOG(LogXGEController, Fatal, TEXT("Failed to connect the input XGE named pipe."));
		}

		// Pass the xgConsole process ID to the XGE control worker, so it can terminate the build on exit
		if (!OutputNamedPipe.WriteBytes(sizeof(XGConsoleProcID), &XGConsoleProcID))
		{
			UE_LOG(LogXGEController, Fatal, TEXT("Failed to pass xgConsole process ID to XGE control worker."));
		}

		LastEventTime = FPlatformTime::Cycles();

		// Launch the output thread
		ReadBackThreadFuture = Async<void>(EAsyncExecution::Thread, [this]() { ReadBackThreadProc(); });

		// Main Tasks Loop
		TArray<uint8> WriteBuffer;
		while (true)
		{
			WriteBuffer.Reset();

			// Wait for new tasks to arrive, with a timeout...
			while (!bShutdown && !bRestartWorker && !AreTasksPending())
			{
				uint32 LastTime = (uint32)FPlatformAtomics::InterlockedAdd(reinterpret_cast<volatile int32*>(&LastEventTime), 0);
				float ElapsedSeconds = (FPlatformTime::Cycles() - LastTime) * FPlatformTime::GetSecondsPerCycle();
				float SecondsToWait = XGEControllerVariables::Timeout - ElapsedSeconds;

				if (!WriteOutThreadEvent->Wait(FMath::CeilToInt(SecondsToWait * 1000.0f)) && !AreTasksDispatchedOrPending())
				{
					// Timed out, and no more pending or dispatched tasks. End the current build.
					bRestartWorker = true;
					break;
				}
			}

			if (bShutdown || bRestartWorker)
				break;

			// Take one task from the pending queue.
			FTask* Task = nullptr;
			{
				FScopeLock Lock(TasksCS);
				PendingTasks.Dequeue(Task);
			}

			if (Task)
			{
				WriteBuffer.Reset();
				WriteBuffer.AddUninitialized(sizeof(uint32));

				FMemoryWriter Writer(WriteBuffer, false, true);

				Writer << Task->ID;
				Writer << Task->Command;
				Writer << Task->CommandArgs;
				*reinterpret_cast<uint32*>(WriteBuffer.GetData()) = WriteBuffer.Num() - sizeof(uint32);

				// Move the tasks to the dispatched tasks map before launching it
				{
					FScopeLock Lock(TasksCS);
					DispatchedTasks.Add(Task->ID, Task);
				}

				if (!OutputNamedPipe.WriteBytes(WriteBuffer.Num(), WriteBuffer.GetData()))
				{
					// Error occurred whilst writing task args to the named pipe.
					// It's likely the controller process was terminated.
					bRestartWorker = true;
				}

				// Update the last event time.
				FPlatformAtomics::InterlockedExchange(reinterpret_cast<volatile int32*>(&LastEventTime), FPlatformTime::Cycles());
			}
		}

		// Destroy the output named pipe. This signals the worker to exit, if it hasn't already.
		OutputNamedPipe.Destroy();

		// Wait for the read back thread to exit. This will happen when the input pipe is closed by the worker.
		if (ReadBackThreadFuture.IsValid())
		{
			ReadBackThreadFuture.Wait();
			ReadBackThreadFuture = TFuture<void>();
		}

		// Wait for the build process
		if (BuildProcessHandle.IsValid())
		{
			if (FPlatformProcess::IsProcRunning(BuildProcessHandle))
				FPlatformProcess::WaitForProc(BuildProcessHandle);

			FPlatformProcess::CloseProc(BuildProcessHandle);
		}

		// Reclaim dispatched (incomplete) tasks 
		for (auto Iter = DispatchedTasks.CreateIterator(); Iter; ++Iter)
			PendingTasks.Enqueue(Iter.Value());

		DispatchedTasks.Reset();

	} while (!bShutdown);
}

void FXGEControllerModule::ReadBackThreadProc()
{
	while (!bShutdown && !bRestartWorker)
	{
		FTaskResponse CompletedTaskResponse;
		if (!InputNamedPipe.ReadBytes(sizeof(CompletedTaskResponse), &CompletedTaskResponse))
		{
			// The named pipe was closed or had an error.
			// Instruct the write-out thread to restart the worker, then exit.
			bRestartWorker = true;
		}
		else
		{
			// Update the last event time.
			FPlatformAtomics::InterlockedExchange(reinterpret_cast<volatile int32*>(&LastEventTime), FPlatformTime::Cycles());

			// We've read a completed task response from the controller.
			// Find the task in the map and complete the promise.
			FTask* Task;
			{
				FScopeLock Lock(TasksCS);
				Task = DispatchedTasks.FindAndRemoveChecked(CompletedTaskResponse.ID);
			}

			FXGETaskResult Result;
			Result.ReturnCode = CompletedTaskResponse.ReturnCode;
			Result.bCompleted = true;

			Task->Promise.SetValue(Result);
			delete Task;
		}

		WriteOutThreadEvent->Trigger();
	}

	InputNamedPipe.Destroy();
}

FString FXGEControllerModule::CreateUniqueFilePath()
{
	check(bSupported);
	return FString::Printf(TEXT("%s/%d.xge"), *WorkingDirectory, NextFileID.Increment());
}

TFuture<FXGETaskResult> FXGEControllerModule::EnqueueTask(const FString& Command, const FString& CommandArgs)
{
	check(bSupported);

	TPromise<FXGETaskResult> Promise;
	TFuture<FXGETaskResult> Future = Promise.GetFuture();

	// Enqueue the new task
	FTask* Task = new FTask(NextTaskID.Increment(), Command, CommandArgs, MoveTemp(Promise));
	{
		FScopeLock Lock(TasksCS);
		PendingTasks.Enqueue(Task);
	}

	WriteOutThreadEvent->Trigger();

	return MoveTemp(Future);
}

XGECONTROLLER_API IXGEController& IXGEController::Get()
{
	static IXGEController& Ref = FModuleManager::LoadModuleChecked<IXGEController>(TEXT("XGEController"));
	return Ref;
}

IMPLEMENT_MODULE(FXGEControllerModule, XGEControllerModule);

#endif // WITH_XGE_CONTROLLER
