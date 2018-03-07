// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "TestPALLog.h"
#include "Parent.h"
#include "Stats/StatsMisc.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformApplicationMisc.h"
#include "GenericPlatform/GenericApplication.h"

#include "TestDirectoryWatcher.h"
#include "RequiredProgramMainCPPInclude.h"
#include "MallocPoisonProxy.h"

DEFINE_LOG_CATEGORY(LogTestPAL);

IMPLEMENT_APPLICATION(TestPAL, "TestPAL");

#define ARG_PROC_TEST						"proc"
#define ARG_PROC_TEST_CHILD					"proc-child"
#define ARG_CASE_SENSITIVITY_TEST			"case"
#define ARG_MESSAGEBOX_TEST					"messagebox"
#define ARG_DIRECTORY_WATCHER_TEST			"dirwatcher"
#define ARG_THREAD_SINGLETON_TEST			"threadsingleton"
#define ARG_SYSINFO_TEST					"sysinfo"
#define ARG_CRASH_TEST						"crash"
#define ARG_STRINGPRECISION_TEST			"stringprecision"
#define ARG_DSO_TEST						"dso"
#define ARG_GET_ALLOCATION_SIZE_TEST		"getallocationsize"
#define ARG_MALLOC_THREADING_TEST			"mallocthreadtest"
#define ARG_MALLOC_REPLAY					"mallocreplay"

namespace TestPAL
{
	FString CommandLine;
};

/**
 * FProcHandle test (child instance)
 */
int32 ProcRunAsChild(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);

	// set a random delay pretending to do some useful work up to a minute. 
	srand(FPlatformProcess::GetCurrentProcessId());
	double RandomWorkTime = FMath::FRandRange(0.0f, 6.0f);

	UE_LOG(LogTestPAL, Display, TEXT("Running proc test as child (pid %d), will be doing work for %f seconds."), FPlatformProcess::GetCurrentProcessId(), RandomWorkTime);

	double StartTime = FPlatformTime::Seconds();

	// Use all the CPU!
	for (;;)
	{
		double CurrentTime = FPlatformTime::Seconds();
		if (CurrentTime - StartTime >= RandomWorkTime)
		{
			break;
		}
	}

	UE_LOG(LogTestPAL, Display, TEXT("Child (pid %d) finished work."), FPlatformProcess::GetCurrentProcessId());

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * FProcHandle test (parent instance)
 */
int32 ProcRunAsParent(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running proc test as parent."));

	// Run slave instance continuously
	int NumChildrenToSpawn = 255, MaxAtOnce = 5;
	FParent Parent(NumChildrenToSpawn, MaxAtOnce);

	Parent.Run();

	UE_LOG(LogTestPAL, Display, TEXT("Parent quit."));

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Tests a single file.
 */
void TestCaseInsensitiveFile(const FString & Filename, const FString & WrongFilename)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	IFileHandle * CreationHandle = PlatformFile.OpenWrite(*Filename);
	checkf(CreationHandle, TEXT("Could not create a test file for '%s'"), *Filename);
	delete CreationHandle;
	
	IFileHandle* CheckGoodHandle = PlatformFile.OpenRead(*Filename);
	checkf(CheckGoodHandle, TEXT("Could not open a test file for '%s' (zero probe)"), *Filename);
	delete CheckGoodHandle;
	
	IFileHandle* CheckWrongCaseRelHandle = PlatformFile.OpenRead(*WrongFilename);
	checkf(CheckWrongCaseRelHandle, TEXT("Could not open a test file for '%s'"), *WrongFilename);
	delete CheckWrongCaseRelHandle;
	
	PlatformFile.DeleteFile(*Filename);
}

/**
 * Case-(in)sensitivity test/
 */
int32 CaseTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();
	
	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running case sensitivity test."));
	
	TestCaseInsensitiveFile(TEXT("Test.Test"), TEXT("teSt.teSt"));
	
	FString File(TEXT("Test^%!CaseInsens"));
	FString AbsFile = FPaths::ConvertRelativePathToFull(File);
	FString AbsFileUpper = AbsFile.ToUpper();

	TestCaseInsensitiveFile(AbsFile, AbsFileUpper);
	
	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Message box test/
 */
int32 MessageBoxTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running message box test."));
	
	FString Display(TEXT("I am a big big string in a big big game, it's not a big big thing if you print me. But I do do feel that I do do will be displayed wrong, displayed wrong...  or not."));
	FString Caption(TEXT("I am a big big caption in a big big game, it's not a big big thing if you print me. But I do do feel that I do do will be displayed wrong, displayed wrong... or not."));
	EAppReturnType::Type Result = FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *Display, *Caption);

	UE_LOG(LogTestPAL, Display, TEXT("MessageBoxExt result: %d."), static_cast<int32>(Result));
	
	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * ************  Thread singleton test *****************
 */

/**
 * Per-thread singleton
 */
struct FPerThreadTestSingleton : public TThreadSingleton<FPerThreadTestSingleton>
{
	FPerThreadTestSingleton()
	{
		UE_LOG(LogTestPAL, Log, TEXT("FPerThreadTestSingleton (this=%p) created for thread %d"),
			this,
			FPlatformTLS::GetCurrentThreadId());
	}

	void DoSomething()
	{
		UE_LOG(LogTestPAL, Log, TEXT("Thread %d is about to quit"), FPlatformTLS::GetCurrentThreadId());
	}

	virtual ~FPerThreadTestSingleton()
	{
		UE_LOG(LogTestPAL, Log, TEXT("FPerThreadTestSingleton (%p) destroyed for thread %d"),
			this,
			FPlatformTLS::GetCurrentThreadId());
	}
};

//DECLARE_THREAD_SINGLETON( FPerThreadTestSingleton );

/**
 * Thread runnable
 */
struct FSingletonTestingThread : public FRunnable
{
	virtual uint32 Run()
	{
		FPerThreadTestSingleton& Dummy = FPerThreadTestSingleton::Get();

		FPlatformProcess::Sleep(3.0f);

		Dummy.DoSomething();
		return 0;
	}
};

/**
 * Thread singleton test
 */
int32 ThreadSingletonTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running thread singleton test."));

	const int kNumTestThreads = 10;

	FSingletonTestingThread * RunnableArray[kNumTestThreads] = { nullptr };
	FRunnableThread * ThreadArray[kNumTestThreads] = { nullptr };

	// start all threads
	for (int Idx = 0; Idx < kNumTestThreads; ++Idx)
	{
		RunnableArray[Idx] = new FSingletonTestingThread();
		ThreadArray[Idx] = FRunnableThread::Create(RunnableArray[Idx],
			*FString::Printf(TEXT("TestThread%d"), Idx));
	}

	GLog->FlushThreadedLogs();
	GLog->Flush();

	// join all threads
	for (int Idx = 0; Idx < kNumTestThreads; ++Idx)
	{
		ThreadArray[Idx]->WaitForCompletion();
		delete ThreadArray[Idx];
		ThreadArray[Idx] = nullptr;
		delete RunnableArray[Idx];
		RunnableArray[Idx] = nullptr;
	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Sysinfo test
 */
int32 SysInfoTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running system info test."));

	bool bIsRunningOnBattery = FPlatformMisc::IsRunningOnBattery();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformMisc::IsRunningOnBattery() = %s"), bIsRunningOnBattery ? TEXT("true") : TEXT("false"));

	GenericApplication * PlatformApplication = FPlatformApplicationMisc::CreateApplication();
	checkf(PlatformApplication, TEXT("Could not create platform application!"));
	bool bIsMouseAttached = PlatformApplication->IsMouseAttached();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformMisc::IsMouseAttached() = %s"), bIsMouseAttached ? TEXT("true") : TEXT("false"));

	FString OSInstanceGuid = FPlatformMisc::GetOperatingSystemId();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformMisc::GetOperatingSystemId() = '%s'"), *OSInstanceGuid);

	FString UserDir = FPlatformProcess::UserDir();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformMisc::UserDir() = '%s'"), *UserDir);

	FString ApplicationSettingsDir = FPlatformProcess::ApplicationSettingsDir();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformMisc::ApplicationSettingsDir() = '%s'"), *ApplicationSettingsDir);

	FPlatformMemory::DumpStats(*GLog);

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Crash test
 */
int32 CrashTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running crash test (this should not exit)."));

	// try ensures first (each ensure fires just once)
	{
		for (int IdxEnsure = 0; IdxEnsure < 5; ++IdxEnsure)
		{
			FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled FIRST ensure() #%d times"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
			ensure(false);
		}
	}
	{
		for (int IdxEnsure = 0; IdxEnsure < 5; ++IdxEnsure)
		{
			FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled SECOND ensure() #%d times"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
			ensure(false);
		}
	}

	if (FParse::Param(CommandLine, TEXT("logfatal")))
	{
		UE_LOG(LogTestPAL, Fatal, TEXT("  LogFatal!"));
	}
	else if (FParse::Param(CommandLine, TEXT("check")))
	{
		checkf(false, TEXT("  checkf!"));
	}
	else
	{
		*(int *)0x10 = 0x11;
	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * String Precision test
 */
int32 StringPrecisionTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running string precision test."));

	const FString TestString(TEXT("TestString"));
	int32 Indent = 15;
	UE_LOG(LogTestPAL, Display, TEXT("%*s"), Indent, *TestString);
	UE_LOG(LogTestPAL, Display, TEXT("Begining of the line %*s"), Indent, *TestString);
	UE_LOG(LogTestPAL, Display, TEXT("%*s end of the line"), Indent, *TestString);

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Test Push/PopDll
 */
int32 DynamicLibraryTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Attempting to load Steam library"));

	FString RootSteamPath;
	FString LibraryName;

	if (PLATFORM_LINUX)
	{
		RootSteamPath = FPaths::EngineDir() / FString(TEXT("Binaries/ThirdParty/Steamworks/Steamv139/x86_64-unknown-linux-gnu/"));
		LibraryName = TEXT("libsteam_api.so");
	}
	else
	{
		UE_LOG(LogTestPAL, Fatal, TEXT("This test is not implemented for this platform."))
	}

	FPlatformProcess::PushDllDirectory(*RootSteamPath);
	void* SteamDLLHandle = FPlatformProcess::GetDllHandle(*LibraryName);
	FPlatformProcess::PopDllDirectory(*RootSteamPath);

	if (SteamDLLHandle == nullptr)
	{
		// try bundled one
		UE_LOG(LogTestPAL, Error, TEXT("Could not load via Push/PopDll, loading directly."));
		SteamDLLHandle = FPlatformProcess::GetDllHandle(*(RootSteamPath + LibraryName));

		if (SteamDLLHandle == nullptr)
		{
			UE_LOG(LogTestPAL, Fatal, TEXT("Could not load Steam library!"))
		}
	}

	if (SteamDLLHandle)
	{
		UE_LOG(LogTestPAL, Log, TEXT("Loaded Steam library at %p"), SteamDLLHandle);
		FPlatformProcess::FreeDllHandle(SteamDLLHandle);
		SteamDLLHandle = nullptr;
	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}



/**
 * FMalloc::GetAllocationSize() test
 */
int32 GetAllocationSizeTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running GMalloc->GetAllocationSize() test."));

	struct Allocation
	{
		void *		Memory;
		SIZE_T		RequestedSize;
		SIZE_T		Alignment;
		SIZE_T		ActualSize;
	};

	TArray<Allocation> Allocs;
	SIZE_T TotalMemoryRequested = 0, TotalMemoryAllocated = 0;

	// force proxy malloc
	FMalloc* OldGMalloc = GMalloc;
	GMalloc = new FMallocPoisonProxy(GMalloc);

	// allocate all the memory and initialize with 0
	for (uint32 Size = 16; Size < 4096; Size += 16)
	{
		for (SIZE_T AlignmentPower = 4; AlignmentPower <= 7; ++AlignmentPower)
		{
			SIZE_T Alignment = ((SIZE_T)1 << AlignmentPower);

			Allocation New;
			New.RequestedSize = Size;
			New.Alignment = Alignment;
			New.Memory = GMalloc->Malloc(New.RequestedSize, New.Alignment);
			if (!GMalloc->GetAllocationSize(New.Memory, New.ActualSize))
			{
				UE_LOG(LogTestPAL, Fatal, TEXT("Could not get allocation size for %p"), New.Memory);
			}
			FMemory::Memzero(New.Memory, New.RequestedSize);

			TotalMemoryRequested += New.RequestedSize;
			TotalMemoryAllocated += New.ActualSize;

			Allocs.Add(New);
		}
	}

	UE_LOG(LogTestPAL, Log, TEXT("Allocated %llu memory (%llu requested) in %d chunks"), TotalMemoryAllocated, TotalMemoryRequested, Allocs.Num());

	if (FParse::Param(CommandLine, TEXT("realloc")))
	{
		for (Allocation & Alloc : Allocs)
		{
			// resize
			Alloc.RequestedSize += 16;
			Alloc.Memory = GMalloc->Realloc(Alloc.Memory, Alloc.RequestedSize, Alloc.Alignment);
			FMemory::Memzero(Alloc.Memory, Alloc.RequestedSize);
		}
	}
	else
	{
		for (Allocation & Alloc : Allocs)
		{
			// only fill the difference, if any
			if (Alloc.ActualSize > Alloc.RequestedSize)
			{
				SIZE_T Difference = Alloc.ActualSize - Alloc.RequestedSize;
				FMemory::Memset((void *)((SIZE_T)Alloc.Memory + Alloc.RequestedSize), 0xAA, Difference);
			}
		}
	}

	// check if any alloc got stomped
	for (Allocation & Alloc : Allocs)
	{
		for (SIZE_T Idx = 0; Idx < Alloc.RequestedSize; ++Idx)
		{
			if (((const uint8 *)Alloc.Memory)[Idx] != 0)
			{
				UE_LOG(LogTestPAL, Fatal, TEXT("Allocation at %p (offset %llu) got stomped with 0x%x"),
					Alloc.Memory, Idx, ((const uint8 *)Alloc.Memory)[Idx]
					);
			}
		}
	}

	UE_LOG(LogTestPAL, Log, TEXT("No memory stomping detected"));

	for (Allocation & Alloc : Allocs)
	{
		GMalloc->Free(Alloc.Memory);
	}
	GMalloc = OldGMalloc;

	Allocs.Empty();

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/** An ugly way to pass a parameter to FRunnable; shouldn't matter for this test code. */
int32 GMallocTestNumAllocs = 500000;

/**
 * Thread runnable
 * bUseSystemMalloc Whether to use system malloc for speed comparison.
 */
template<bool bUseSystemMalloc>
struct FMemoryAllocatingThread : public FRunnable
{
	virtual uint32 Run()
	{
		for (int32 IdxAlloc = 0; IdxAlloc < GMallocTestNumAllocs; ++IdxAlloc)
		{
			// allocate up to 4MB at a time
			const SIZE_T ChunkSize = 65536 + (4096 * 1024 - 65536) * FMath::FRand();

			if (bUseSystemMalloc)
			{
				void* Ptr = malloc(ChunkSize);
				if (LIKELY(Ptr))
				{
					free(Ptr);
				}
			}
			else
			{
				void* Ptr = FMemory::Malloc(ChunkSize);
				if (LIKELY(Ptr))
				{
					FMemory::Free(Ptr);
				}
			}
		}
		return 0;
	}
};

/**
 * Malloc threading test
 */
int32 MallocThreadingTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);

	TArray<FRunnable*> RunnableArray;
	TArray<FRunnableThread*> ThreadArray;

	FString Test(CommandLine);

	bool bUseSystemMalloc = FParse::Param(CommandLine, TEXT("systemmalloc"));
	int32 NumTestThreads = 32, InNumTestThreads = 0, InNumAllocs = 0;
	if (FParse::Value(CommandLine, TEXT("numthreads="), InNumTestThreads))
	{
		NumTestThreads = FMath::Max(1, InNumTestThreads);
	}
	if (FParse::Value(CommandLine, TEXT("numallocs="), InNumAllocs))
	{
		GMallocTestNumAllocs = FMath::Max(1000, InNumAllocs * 1000);
	}

	UE_LOG(LogTestPAL, Display, TEXT("Running malloc threading test using %s malloc and %d threads, each doing %d allocations."),
		bUseSystemMalloc ? TEXT("libc") : GMalloc->GetDescriptiveName(),
		NumTestThreads,
		GMallocTestNumAllocs
		);

	// start all threads
	double WallTimeDuration = 0.0;
	{
		FSimpleScopeSecondsCounter Duration(WallTimeDuration);
		for (int Idx = 0; Idx < NumTestThreads; ++Idx)
		{
			RunnableArray.Add( bUseSystemMalloc ? static_cast<FRunnable*>(new FMemoryAllocatingThread<true>()) : static_cast<FRunnable*>(new FMemoryAllocatingThread<false>()) );
			ThreadArray.Add( FRunnableThread::Create(RunnableArray[Idx],
				*FString::Printf(TEXT("MallocTest%d"), Idx)) );
		}

		GLog->FlushThreadedLogs();
		GLog->Flush();

		// join all threads
		for (int Idx = 0; Idx < NumTestThreads; ++Idx)
		{
			ThreadArray[Idx]->WaitForCompletion();
			delete ThreadArray[Idx];
			ThreadArray[Idx] = nullptr;
			delete RunnableArray[Idx];
			RunnableArray[Idx] = nullptr;
		}
	}
	UE_LOG(LogTestPAL, Display, TEXT("Test took %f seconds."), WallTimeDuration);

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Replays a malloc save file, streaming it from the disk. Waits for Ctrl-C until exiting.
 * 
 * @param ReplayFileName file name to read. Must be reachable from cwd (current working directory) or absolute
 * @param OperationToStopAfter number of operation to stop on - no further reads will be done. Useful to compare peak usage in the middle of file.
 * @param bSuppressErrors Whether to print errors
 */
void ReplayMallocFile(const FString& ReplayFileName, uint64 OperationToStopAfter, bool bSuppressErrors)
{
#if PLATFORM_WINDOWS
	FILE* ReplayFile = nullptr;
	if (fopen_s(&ReplayFile, TCHAR_TO_UTF8(*ReplayFileName), "rb") != 0 || ReplayFile == nullptr)
#else
	FILE* ReplayFile = fopen(TCHAR_TO_UTF8(*ReplayFileName), "rb");
	if (ReplayFile == nullptr)
#endif // PLATFORM_WINDOWS
	{
		return;
	}

	// For file format, see FMallocReplayProxy

	// skip the first line since it contains column headers
	{
		char DummyBuffer[4096];
		fgets(DummyBuffer, sizeof(DummyBuffer), ReplayFile);
	}

	double WallTimeDuration = 0.0;

	uint64 OperationNumber = 0;
	TMap<uint64, void *> FilePointerToRamPointers;
	for(;;)
	{
		FSimpleScopeSecondsCounter Duration(WallTimeDuration);

		char OpBuffer[128] = {0};
		uint64 PtrOut, PtrIn, Size, Ordinal;
		uint32 Alignment;
#if PLATFORM_WINDOWS
		if (fscanf_s(ReplayFile, "%s %llu %llu %llu %u\t# %llu\n", OpBuffer, static_cast<unsigned int>(sizeof(OpBuffer)), &PtrOut, &PtrIn, &Size, &Alignment, &Ordinal) != 6)
#else
		if (fscanf(ReplayFile, "%s %llu %llu %llu %u\t# %llu\n", OpBuffer, &PtrOut, &PtrIn, &Size, &Alignment, &Ordinal) != 6)
#endif // PLATFORM_WINDOWS
		{
			UE_LOG(LogTestPAL, Display, TEXT("Hit end of the replay file on %llu-th operation."), OperationNumber);
			break;
		}

		if (!FCStringAnsi::Strcmp(OpBuffer, "Malloc"))
		{
			if (FilePointerToRamPointers.Find(PtrOut) == nullptr)
			{
				void* Result = FMemory::Malloc(Size, Alignment);
				FilePointerToRamPointers.Add(PtrOut, Result);
			}
			else if (!bSuppressErrors)
			{
				UE_LOG(LogTestPAL, Error, TEXT("Replay file contains operation # %llu that returned pointer %llu, which was already allocated at that moment. Skipping."), Ordinal, PtrOut);
			}
		}
		else if (!FCStringAnsi::Strcmp(OpBuffer, "Realloc"))
		{
			void* PtrToRealloc = nullptr;
			if (PtrIn != 0)
			{
				void** RamPointer = FilePointerToRamPointers.Find(PtrIn);
				if (RamPointer != nullptr)
				{
					PtrToRealloc = *RamPointer;
				}
				else if (!bSuppressErrors)
				{
					UE_LOG(LogTestPAL, Error, TEXT("Replay file contains operation # %llu to reallocate pointer %llu, which was not allocated at that moment. Substituting with nullptr."), Ordinal, PtrIn);
				}
			}

			void* Result = FMemory::Realloc(PtrToRealloc, Size, Alignment);
			FilePointerToRamPointers.Remove(PtrIn);
			FilePointerToRamPointers.Add(PtrOut, Result);
		}
		else if (!FCStringAnsi::Strcmp(OpBuffer, "Free"))
		{
			void* PtrToFree = nullptr;
			if (PtrIn != 0)
			{
				void** RamPointer = FilePointerToRamPointers.Find(PtrIn);
				if (RamPointer != nullptr)
				{
					PtrToFree = *RamPointer;
				}
				else if (!bSuppressErrors)
				{
					UE_LOG(LogTestPAL, Error, TEXT("Replay file contains operation # %llu to free pointer %llu, which was not allocated at that moment. Substituting with nullptr."), Ordinal, PtrIn);
				}
			}

			FMemory::Free(PtrToFree);
			FilePointerToRamPointers.Remove(PtrIn);
		}
		else if (!bSuppressErrors)
		{
			UE_LOG(LogTestPAL, Error, TEXT("Replay file contains unknown operation '%s', skipping."), ANSI_TO_TCHAR(OpBuffer));
		}

		++OperationNumber;
		if (OperationNumber >= OperationToStopAfter)
		{
			UE_LOG(LogTestPAL, Display, TEXT("Stopping after %llu-th operation."), OperationNumber);
			break;
		}
	}

	UE_LOG(LogTestPAL, Display, TEXT("Replayed %llu operations in %f seconds, waiting for Ctrl-C to proceed further. You can now examine heap/process state."), OperationNumber, WallTimeDuration);

	while(!GIsRequestingExit)
	{
		FPlatformProcess::Sleep(1.0);
	}
}

/**
 * Malloc replaying test
 */
int32 MallocReplayTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);

	FString ReplayFileName;
	if (FParse::Value(CommandLine, TEXT("replayfile="), ReplayFileName))
	{
		uint64 OperationToStopAfter = (uint64)(-1);
		if (!FParse::Value(CommandLine, TEXT("stopafter="), OperationToStopAfter))
		{
			UE_LOG(LogTestPAL, Display, TEXT("You can pass -stopafter=N to stop after Nth operation."));
		}
		
		bool bSuppressErrors = FParse::Param(CommandLine, TEXT("suppresserrors"));

		ReplayMallocFile(ReplayFileName, OperationToStopAfter, bSuppressErrors);
	}
	else
	{
		UE_LOG(LogTestPAL, Error, TEXT("No file to replay. Pass -replayfile=PathToFile.txt"));
	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}


/**
 * Selects and runs one of test cases.
 *
 * @param ArgC Number of commandline arguments.
 * @param ArgV Commandline arguments.
 * @return Test case's return code.
 */
int32 MultiplexedMain(int32 ArgC, char* ArgV[])
{
	for (int32 IdxArg = 0; IdxArg < ArgC; ++IdxArg)
	{
		if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_PROC_TEST_CHILD))
		{
			return ProcRunAsChild(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_PROC_TEST))
		{
			return ProcRunAsParent(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_CASE_SENSITIVITY_TEST))
		{
			return CaseTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_MESSAGEBOX_TEST))
		{
			return MessageBoxTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_DIRECTORY_WATCHER_TEST))
		{
			return DirectoryWatcherTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_THREAD_SINGLETON_TEST))
		{
			return ThreadSingletonTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_SYSINFO_TEST))
		{
			return SysInfoTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_CRASH_TEST))
		{
			return CrashTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_STRINGPRECISION_TEST))
		{
			return StringPrecisionTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_DSO_TEST))
		{
			return DynamicLibraryTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_GET_ALLOCATION_SIZE_TEST))
		{
			return GetAllocationSizeTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_MALLOC_THREADING_TEST))
		{
			return MallocThreadingTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_MALLOC_REPLAY))
		{
			return MallocReplayTest(*TestPAL::CommandLine);
		}
	}

	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();
	
	GEngineLoop.PreInit(*TestPAL::CommandLine);
	UE_LOG(LogTestPAL, Warning, TEXT("Unable to find any known test name, no test started."));

	UE_LOG(LogTestPAL, Warning, TEXT(""));
	UE_LOG(LogTestPAL, Warning, TEXT("Available test cases:"));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test process handling API"), ANSI_TO_TCHAR( ARG_PROC_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test case-insensitive file operations"), ANSI_TO_TCHAR( ARG_CASE_SENSITIVITY_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test message box bug (too long strings)"), ANSI_TO_TCHAR( ARG_MESSAGEBOX_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test directory watcher"), ANSI_TO_TCHAR( ARG_DIRECTORY_WATCHER_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test per-thread singletons"), ANSI_TO_TCHAR( ARG_THREAD_SINGLETON_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test (some) system information"), ANSI_TO_TCHAR( ARG_SYSINFO_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test crash handling (pass '-logfatal' for testing Fatal logs)"), ANSI_TO_TCHAR( ARG_CRASH_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test passing %%*s in a format string"), ANSI_TO_TCHAR( ARG_STRINGPRECISION_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test APIs for dealing with dynamic libraries"), ANSI_TO_TCHAR( ARG_DSO_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test GMalloc->GetAllocationSize()"), UTF8_TO_TCHAR(ARG_GET_ALLOCATION_SIZE_TEST));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test malloc for thread-safety and performance. Pass -systemmalloc to use system malloc, -numthreads=N and -numallocs=M (in thousands)."), UTF8_TO_TCHAR(ARG_MALLOC_THREADING_TEST));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test by replaying a saved malloc history saved by -mallocsavereplay. Possible options: -replayfile=File, -stopafter=N (operation), -suppresserrors"), UTF8_TO_TCHAR(ARG_MALLOC_REPLAY));
	UE_LOG(LogTestPAL, Warning, TEXT(""));
	UE_LOG(LogTestPAL, Warning, TEXT("Pass one of those to run an appropriate test."));

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 1;
}

int32 main(int32 ArgC, char* ArgV[])
{
	TestPAL::CommandLine = TEXT("");

	for (int32 Option = 1; Option < ArgC; Option++)
	{
		TestPAL::CommandLine += TEXT(" ");
		FString Argument(ANSI_TO_TCHAR(ArgV[Option]));
		if (Argument.Contains(TEXT(" ")))
		{
			if (Argument.Contains(TEXT("=")))
			{
				FString ArgName;
				FString ArgValue;
				Argument.Split( TEXT("="), &ArgName, &ArgValue );
				Argument = FString::Printf( TEXT("%s=\"%s\""), *ArgName, *ArgValue );
			}
			else
			{
				Argument = FString::Printf(TEXT("\"%s\""), *Argument);
			}
		}
		TestPAL::CommandLine += Argument;
	}

	return MultiplexedMain(ArgC, ArgV);
}
