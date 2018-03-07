// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WindowsPlatformCrashContext.h"
#include "PlatformMallocCrash.h"
#include "ExceptionHandling.h"
#include "EngineVersion.h"
#include "EngineBuildSettings.h"
#include "HAL/ExceptionHandling.h"
#include "HAL/ThreadHeartBeat.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformOutputDevices.h"
#include "Internationalization/Internationalization.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h"
#include "Misc/MessageDialog.h"
#include "Misc/CoreDelegates.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Templates/ScopedPointer.h"
#include "WindowsPlatformStackWalk.h"
#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"
#include "UniquePtr.h"

#include <strsafe.h>
#include <dbghelp.h>
#include <Shlwapi.h>

#ifndef UE_LOG_CRASH_CALLSTACK
	#define UE_LOG_CRASH_CALLSTACK 1
#endif

#pragma comment( lib, "version.lib" )
#pragma comment( lib, "Shlwapi.lib" )

void FWindowsPlatformCrashContext::AddPlatformSpecificProperties()
{
	AddCrashProperty(TEXT("PlatformIsRunningWindows"), 1);
	// On windows track the crash type
	AddCrashProperty(TEXT("PlatformCallbackResult"), GetCrashType());
}

/** Platform specific constants. */
enum EConstants
{
	UE4_MINIDUMP_CRASHCONTEXT = LastReservedStream + 1,
};

namespace
{
static int32 ReportCrashCallCount = 0;

/**
 * Write a Windows minidump to disk
 * @param The Crash context with its data already serialized into its buffer
 * @param Path Full path of file to write (normally a .dmp file)
 * @param ExceptionInfo Pointer to structure containing the exception information
 * @return Success or failure
 */

// #CrashReport: 2014-10-08 Move to FWindowsPlatformCrashContext
bool WriteMinidump(FWindowsPlatformCrashContext& InContext, const TCHAR* Path, LPEXCEPTION_POINTERS ExceptionInfo, bool bIsEnsure )
{
	// Try to create file for minidump.
	HANDLE FileHandle = CreateFileW(Path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if (FileHandle == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	// Initialise structure required by MiniDumpWriteDumps
	MINIDUMP_EXCEPTION_INFORMATION DumpExceptionInfo = {};

	DumpExceptionInfo.ThreadId			= GetCurrentThreadId();
	DumpExceptionInfo.ExceptionPointers	= ExceptionInfo;
	DumpExceptionInfo.ClientPointers	= FALSE;

	// CrashContext.runtime-xml is now a part of the minidump file.
	MINIDUMP_USER_STREAM CrashContextStream ={0};
	CrashContextStream.Type = UE4_MINIDUMP_CRASHCONTEXT;
	CrashContextStream.BufferSize = InContext.GetBuffer().GetAllocatedSize();
	CrashContextStream.Buffer = (void*)*InContext.GetBuffer();

	MINIDUMP_USER_STREAM_INFORMATION CrashContextStreamInformation = {0};
	CrashContextStreamInformation.UserStreamCount = 1;
	CrashContextStreamInformation.UserStreamArray = &CrashContextStream;

	MINIDUMP_TYPE MinidumpType = MiniDumpNormal;//(MINIDUMP_TYPE)(MiniDumpWithPrivateReadWriteMemory|MiniDumpWithDataSegs|MiniDumpWithHandleData|MiniDumpWithFullMemoryInfo|MiniDumpWithThreadInfo|MiniDumpWithUnloadedModules);

	// For ensures by default we use minidump to avoid severe hitches when writing 3GB+ files.
	// However the crash dump mode will remain the same.
	bool bShouldBeFullCrashDump = bIsEnsure ? InContext.IsFullCrashDumpOnEnsure() : InContext.IsFullCrashDump();
	if (bShouldBeFullCrashDump)
	{
		MinidumpType = (MINIDUMP_TYPE)(MiniDumpWithFullMemory|MiniDumpWithFullMemoryInfo|MiniDumpWithHandleData|MiniDumpWithThreadInfo|MiniDumpWithUnloadedModules);
	}

	const BOOL Result = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), FileHandle, MinidumpType, &DumpExceptionInfo, &CrashContextStreamInformation, NULL);
	CloseHandle(FileHandle);

	return Result == TRUE;
}

/**
 * Enum indicating whether to run the crash reporter UI
 */
enum class EErrorReportUI
{
	/** Ask the user for a description */
	ShowDialog,

	/** Silently uploaded the report */
	ReportInUnattendedMode	
};

/** 
 * Create a crash report, add the user log and video, and save them a unique the crash folder
 * Launch CrashReportClient.exe to read the report and upload to our CR pipeline
 */
int32 ReportCrashUsingCrashReportClient(FWindowsPlatformCrashContext& InContext, EXCEPTION_POINTERS* ExceptionInfo, const TCHAR* ErrorMessage, EErrorReportUI ReportUI, bool bIsEnsure)
{
	// Prevent CrashReportClient from spawning another CrashReportClient.
	const TCHAR* ExecutableName = FPlatformProcess::ExecutableName();
	const bool bCanRunCrashReportClient = FCString::Stristr( ExecutableName, TEXT( "CrashReportClient" ) ) == nullptr;
	if( bCanRunCrashReportClient )
	{
		static const TCHAR CrashReportClientExeName[] = TEXT("CrashReportClient.exe");
		bool bCrashReporterRan = false;

		// Suppress the user input dialog if we're running in unattended mode
		bool bNoDialog = FApp::IsUnattended() || ReportUI == EErrorReportUI::ReportInUnattendedMode || IsRunningDedicatedServer();

		// Generate Crash GUID
		TCHAR CrashGUID[FGenericCrashContext::CrashGUIDLength];
		InContext.GetUniqueCrashName(CrashGUID, FGenericCrashContext::CrashGUIDLength);
		const FString AppName = FString::Printf(TEXT("UE4-%s"), FApp::GetProjectName());

		FString CrashFolder = FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("Crashes"), CrashGUID);
		FString CrashFolderAbsolute = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*CrashFolder);
		if (IFileManager::Get().MakeDirectory(*CrashFolderAbsolute, true))
		{
			// Save crash context
			const FString CrashContextXMLPath = FPaths::Combine(*CrashFolderAbsolute, FPlatformCrashContext::CrashContextRuntimeXMLNameW);
			InContext.SerializeAsXML(*CrashContextXMLPath);

			// Save mindump
			const FString MinidumpFileName = FPaths::Combine(*CrashFolderAbsolute, *FGenericCrashContext::UE4MinidumpName);
			WriteMinidump(InContext, *MinidumpFileName, ExceptionInfo, bIsEnsure);

			// Copy log
			const FString LogSrcAbsolute = FPlatformOutputDevices::GetAbsoluteLogFilename();

			// Flush out the log
			GLog->Flush();

			// If we have a memory only log, make sure it's dumped to file before we attach it to the report
			bool bHasLogFile = !FPlatformOutputDevices::GetLog()->IsMemoryOnly();
#if !NO_LOGGING
			if (!bHasLogFile)
			{
				FArchive* LogFile = IFileManager::Get().CreateFileWriter(*LogSrcAbsolute, FILEWRITE_AllowRead);
				if (LogFile)
				{
					FPlatformOutputDevices::GetLog()->Dump(*LogFile);
					LogFile->Flush();
					delete LogFile;
					bHasLogFile = true;
				}
			}
#endif
			if (bHasLogFile)
			{
				FString LogFilename = FPaths::GetCleanFilename(LogSrcAbsolute);
				const FString LogDstAbsolute = FPaths::Combine(*CrashFolderAbsolute, *LogFilename);
				const bool bReplace = true;
				const bool bEvenIfReadOnly = false;
				const bool bAttributes = false;
				FCopyProgress* const CopyProgress = nullptr;
				static_cast<void>(IFileManager::Get().Copy(*LogDstAbsolute, *LogSrcAbsolute, bReplace, bEvenIfReadOnly, bAttributes, CopyProgress, FILEREAD_AllowWrite, FILEWRITE_AllowRead));	// best effort, so don't care about result: couldn't copy -> tough, no log
			}

			// If present, include the crash report config file to pass config values to the CRC
			const TCHAR* CrashConfigSrcPath = FWindowsPlatformCrashContext::GetCrashConfigFilePath();
			if (IFileManager::Get().FileExists(CrashConfigSrcPath))
			{
				FString CrashConfigFilename = FPaths::GetCleanFilename(CrashConfigSrcPath);
				const FString CrashConfigDstAbsolute = FPaths::Combine(*CrashFolderAbsolute, *CrashConfigFilename);
				static_cast<void>(IFileManager::Get().Copy(*CrashConfigDstAbsolute, CrashConfigSrcPath));	// best effort, so don't care about result: couldn't copy -> tough, no config
			}

			// If present, include the crash video
			const FString CrashVideoPath = FPaths::ProjectLogDir() / TEXT("CrashVideo.avi");
			if (IFileManager::Get().FileExists(*CrashVideoPath))
			{
				FString CrashVideoFilename = FPaths::GetCleanFilename(CrashVideoPath);
				const FString CrashVideoDstAbsolute = FPaths::Combine(*CrashFolderAbsolute, *CrashVideoFilename);
				static_cast<void>(IFileManager::Get().Copy(*CrashVideoDstAbsolute, *CrashVideoPath));	// best effort, so don't care about result: couldn't copy -> tough, no video
			}

			// Build machines do not upload these automatically since it is not okay to have lingering processes after the build completes.
			if (GIsBuildMachine)
			{
				return EXCEPTION_CONTINUE_EXECUTION;
			}

			// Run Crash Report Client
			FString CrashReportClientArguments = FString::Printf(TEXT("\"%s\""), *CrashFolderAbsolute);

			// Pass nullrhi to CRC when the engine is in this mode to stop the CRC attempting to initialize RHI when the capability isn't available
			bool bNullRHI = !FApp::CanEverRender();

			if (bNoDialog || bNullRHI)
			{
				CrashReportClientArguments += TEXT(" -Unattended");
			}

			if (bNullRHI)
			{
				CrashReportClientArguments += TEXT(" -nullrhi");
			}

			CrashReportClientArguments += FString(TEXT(" -AppName=")) + AppName;
			CrashReportClientArguments += FString(TEXT(" -CrashGUID=")) + CrashGUID;

			const FString DownstreamStorage = FWindowsPlatformStackWalk::GetDownstreamStorage();
			if (!DownstreamStorage.IsEmpty())
			{
				CrashReportClientArguments += FString(TEXT(" -DebugSymbols=")) + DownstreamStorage;
			}

			FString CrashClientPath = FPaths::Combine(*FPaths::EngineDir(), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory(), CrashReportClientExeName);
			bCrashReporterRan = FPlatformProcess::CreateProc(*CrashClientPath, *CrashReportClientArguments, true, false, false, NULL, 0, NULL, NULL).IsValid();
		}

		if (!bCrashReporterRan && !bNoDialog)
		{
			UE_LOG(LogWindows, Log, TEXT("Could not start %s"), CrashReportClientExeName);
			FPlatformMemory::DumpStats(*GWarn);
			FText MessageTitle(FText::Format(
				NSLOCTEXT("MessageDialog", "AppHasCrashed", "The {0} {1} has crashed and will close"),
				FText::FromString(AppName),
				FText::FromString(FPlatformMisc::GetEngineMode())
				));
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(GErrorHist), &MessageTitle);
		}
	}

	// Let the system take back over (return value only used by NewReportEnsure)
	return EXCEPTION_CONTINUE_EXECUTION;
}

} // end anonymous namespace

static FCriticalSection EnsureLock;
static bool bReentranceGuard = false;

#if WINVER > 0x502	// Windows Error Reporting is not supported on Windows XP
/**
 * A wrapper for ReportCrashUsingCrashReportClient that creates a new ensure crash context
 */
int32 ReportEnsureUsingCrashReportClient(EXCEPTION_POINTERS* ExceptionInfo, const TCHAR* ErrorMessage, EErrorReportUI ReportUI)
{
	const bool bIsEnsure = true;
	FWindowsPlatformCrashContext CrashContext(bIsEnsure);

	return ReportCrashUsingCrashReportClient(CrashContext, ExceptionInfo, ErrorMessage, ReportUI, bIsEnsure);
}
#endif

// #CrashReport: 2015-05-28 This should be named EngineEnsureHandler
/** 
 * Report an ensure to the crash reporting system
 */
void NewReportEnsure( const TCHAR* ErrorMessage )
{
	if (ReportCrashCallCount > 0)
	{
		// Don't report ensures after we've crashed. They simply may be a result of the crash as
		// the engine is already in a bad state.
		return;
	}

	// Simple re-entrance guard.
	EnsureLock.Lock();

	if( bReentranceGuard )
	{
		EnsureLock.Unlock();
		return;
	}

	// Stop checking heartbeat for this thread. Ensure can take a lot of time
	// Thread heartbeat will be resumed the next time this thread calls FThreadHeartBeat::Get().HeartBeat();
	// The reason why we don't call HeartBeat() at the end of this function is that maybe this thread
	// Never had a heartbeat checked and may not be sending heartbeats at all which would later lead to a false positives when detecting hangs.
	FThreadHeartBeat::Get().KillHeartBeat();

	bReentranceGuard = true;

#if WINVER > 0x502	// Windows Error Reporting is not supported on Windows XP
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
	__try
#endif
	{
		FPlatformMisc::RaiseException( 1 );
	}
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
	__except(ReportEnsureUsingCrashReportClient(GetExceptionInformation(), ErrorMessage, IsInteractiveEnsureMode() ? EErrorReportUI::ShowDialog : EErrorReportUI::ReportInUnattendedMode))
	CA_SUPPRESS(6322)
	{
	}
#endif
#endif	// WINVER

	bReentranceGuard = false;
	EnsureLock.Unlock();
}

#include "HideWindowsPlatformTypes.h"

// Original code below

#include "AllowWindowsPlatformTypes.h"
	#include <ErrorRep.h>
	#include <DbgHelp.h>
#include "HideWindowsPlatformTypes.h"

#pragma comment(lib, "Faultrep.lib")

/** 
 * Creates an info string describing the given exception record.
 * See MSDN docs on EXCEPTION_RECORD.
 */
#include "AllowWindowsPlatformTypes.h"
void CreateExceptionInfoString(EXCEPTION_RECORD* ExceptionRecord)
{
	// #CrashReport: 2014-08-18 Fix FString usage?
	FString ErrorString = TEXT("Unhandled Exception: ");

#define HANDLE_CASE(x) case x: ErrorString += TEXT(#x); break;

	switch (ExceptionRecord->ExceptionCode)
	{
	case EXCEPTION_ACCESS_VIOLATION:
		ErrorString += TEXT("EXCEPTION_ACCESS_VIOLATION ");
		if (ExceptionRecord->ExceptionInformation[0] == 0)
		{
			ErrorString += TEXT("reading address ");
		}
		else if (ExceptionRecord->ExceptionInformation[0] == 1)
		{
			ErrorString += TEXT("writing address ");
		}
		ErrorString += FString::Printf(TEXT("0x%08x"), (uint32)ExceptionRecord->ExceptionInformation[1]);
		break;
	HANDLE_CASE(EXCEPTION_ARRAY_BOUNDS_EXCEEDED)
	HANDLE_CASE(EXCEPTION_DATATYPE_MISALIGNMENT)
	HANDLE_CASE(EXCEPTION_FLT_DENORMAL_OPERAND)
	HANDLE_CASE(EXCEPTION_FLT_DIVIDE_BY_ZERO)
	HANDLE_CASE(EXCEPTION_FLT_INVALID_OPERATION)
	HANDLE_CASE(EXCEPTION_ILLEGAL_INSTRUCTION)
	HANDLE_CASE(EXCEPTION_INT_DIVIDE_BY_ZERO)
	HANDLE_CASE(EXCEPTION_PRIV_INSTRUCTION)
	HANDLE_CASE(EXCEPTION_STACK_OVERFLOW)
	default:
		ErrorString += FString::Printf(TEXT("0x%08x"), (uint32)ExceptionRecord->ExceptionCode);
	}

	FCString::Strncpy(GErrorExceptionDescription, *ErrorString, ARRAY_COUNT(GErrorExceptionDescription));

#undef HANDLE_CASE
}

/** 
 * Crash reporting thread. 
 * We process all the crashes on a separate thread in case the original thread's stack is corrupted (stack overflow etc).
 * We're using low level API functions here because at the time we initialize the thread, nothing in the engine exists yet.
 **/
class FCrashReportingThread
{
	/** Thread Id */
	DWORD ThreadId;
	/** Thread handle */
	HANDLE Thread;

	/** Stops this thread */
	FThreadSafeCounter StopTaskCounter;
	/** Signals that the game has crashed */
	HANDLE CrashEvent;
	/** Exception information */
	LPEXCEPTION_POINTERS ExceptionInfo;
	/** Event that signals the crash reporting thread has finished processing the crash */
	HANDLE CrashHandledEvent;

	/** Thread main proc */
	static DWORD STDCALL CrashReportingThreadProc(LPVOID pThis)
	{
		FCrashReportingThread* This = (FCrashReportingThread*)pThis;
		return This->Run();
	}

	/** Main loop that waits for a crash to trigger the report generation */
	FORCENOINLINE uint32 Run()
	{
		while (StopTaskCounter.GetValue() == 0)
		{
			if (WaitForSingleObject(CrashEvent, 500) == WAIT_OBJECT_0)
			{
				ResetEvent(CrashHandledEvent);
				HandleCrashInternal();
				ResetEvent(CrashEvent);
				// Let the thread that crashed know we're done.				
				SetEvent(CrashHandledEvent);
				break;
			}
		}
		return 0;
	}

	/** Called by the destructor to terminate the thread */
	void Stop()
	{
		StopTaskCounter.Increment();
	}

public:
		
	FCrashReportingThread()
		: Thread(nullptr)
		, CrashEvent(nullptr)
		, ExceptionInfo(nullptr)
		, CrashHandledEvent(nullptr)
	{
		// Create a background thread that will process the crash and generate crash reports
		Thread = CreateThread(NULL, 0, CrashReportingThreadProc, this, 0, &ThreadId);
		if (Thread)
		{
			SetThreadPriority(Thread, THREAD_PRIORITY_BELOW_NORMAL);
			// Synchronization objects
			CrashEvent = CreateEvent(nullptr, true, 0, nullptr);
			CrashHandledEvent = CreateEvent(nullptr, true, 0, nullptr);
		}
	}

	FORCENOINLINE ~FCrashReportingThread()
	{
		if (Thread)
		{
			// Stop the crash reporting thread
			Stop();
			// 1s should be enough for the thread to exit, otherwise don't bother with cleanup
			if (WaitForSingleObject(Thread, 1000) == WAIT_OBJECT_0)
			{
				CloseHandle(Thread);
				CloseHandle(CrashEvent);
				CloseHandle(CrashHandledEvent);
			}
			Thread = nullptr;
			CrashEvent = nullptr;
			CrashHandledEvent = nullptr;
		}
	}

	/** The thread that crashed calls this function which will trigger the CR thread to report the crash */
	FORCEINLINE void OnCrashed(LPEXCEPTION_POINTERS InExceptionInfo)
	{
		ExceptionInfo = InExceptionInfo;
		SetEvent(CrashEvent);
	}

	/** The thread that crashed calls this function to wait for the report to be generated */
	FORCEINLINE bool WaitUntilCrashIsHandled()
	{
		// Wait 60s, it's more than enough to generate crash report. We don't want to stall forever otherwise.
		return WaitForSingleObject(CrashHandledEvent, 60000) == WAIT_OBJECT_0;
	}

private:

	/** Handles the crash */
	FORCENOINLINE void HandleCrashInternal()
	{
		// Stop the heartbeat thread so that it doesn't interfere with crashreporting
		FThreadHeartBeat::Get().Stop();

		GLog->PanicFlushThreadedLogs();

		// Not super safe due to dynamic memory allocations, but at least enables new functionality.
		// Introduces a new runtime crash context. Will replace all Windows related crash reporting.
		const bool bIsEnsure = false;
		FWindowsPlatformCrashContext CrashContext(bIsEnsure);

		// First launch the crash reporter client.
#if WINVER > 0x502	// Windows Error Reporting is not supported on Windows XP
		if (GUseCrashReportClient)
		{
			ReportCrashUsingCrashReportClient(CrashContext, ExceptionInfo, GErrorMessage, EErrorReportUI::ShowDialog, bIsEnsure);
		}
		else
#endif		// WINVER
		{
			CrashContext.SerializeContentToBuffer();
			WriteMinidump(CrashContext, MiniDumpFilenameW, ExceptionInfo, bIsEnsure);

#if UE_BUILD_SHIPPING && WITH_EDITOR
			uint32 dwOpt = 0;
			EFaultRepRetVal repret = ReportFault(ExceptionInfo, dwOpt);
#endif
		}

		// Then try run time crash processing and broadcast information about a crash.
		FCoreDelegates::OnHandleSystemError.Broadcast();

		const bool bGenerateRuntimeCallstack =
#if UE_LOG_CRASH_CALLSTACK
			true;
#else
			FParse::Param(FCommandLine::Get(), TEXT("ForceLogCallstacks")) || FEngineBuildSettings::IsInternalBuild() || FEngineBuildSettings::IsPerforceBuild() || FEngineBuildSettings::IsSourceDistribution();
#endif // UE_LOG_CRASH_CALLSTACK
		if (bGenerateRuntimeCallstack)
		{
			const SIZE_T StackTraceSize = 65535;
			ANSICHAR* StackTrace = (ANSICHAR*)GMalloc->Malloc(StackTraceSize);
			StackTrace[0] = 0;
			// Walk the stack and dump it to the allocated memory. This process usually allocates a lot of memory.
			FPlatformStackWalk::StackWalkAndDump(StackTrace, StackTraceSize, 0, ExceptionInfo->ContextRecord);

			if (ExceptionInfo->ExceptionRecord->ExceptionCode != 1)
			{
				CreateExceptionInfoString(ExceptionInfo->ExceptionRecord);
				FCString::Strncat(GErrorHist, GErrorExceptionDescription, ARRAY_COUNT(GErrorHist));
				FCString::Strncat(GErrorHist, TEXT("\r\n\r\n"), ARRAY_COUNT(GErrorHist));
			}

			FCString::Strncat(GErrorHist, ANSI_TO_TCHAR(StackTrace), ARRAY_COUNT(GErrorHist));

			GMalloc->Free(StackTrace);
		}

#if !UE_BUILD_SHIPPING
		FPlatformStackWalk::UploadLocalSymbols();
#endif
	}

};

#include "HideWindowsPlatformTypes.h"

TUniquePtr<FCrashReportingThread> GCrashReportingThread = MakeUnique<FCrashReportingThread>();

// #CrashReport: 2015-05-28 This should be named EngineCrashHandler
int32 ReportCrash( LPEXCEPTION_POINTERS ExceptionInfo )
{
	// Only create a minidump the first time this function is called.
	// (Can be called the first time from the RenderThread, then a second time from the MainThread.)
	if (GCrashReportingThread)
	{
		if (FPlatformAtomics::InterlockedIncrement(&ReportCrashCallCount) == 1)
		{
			GCrashReportingThread->OnCrashed(ExceptionInfo);
		}

		// Wait 60s for the crash reporting thread to process the message
		GCrashReportingThread->WaitUntilCrashIsHandled();
	}

	return EXCEPTION_EXECUTE_HANDLER;
}
