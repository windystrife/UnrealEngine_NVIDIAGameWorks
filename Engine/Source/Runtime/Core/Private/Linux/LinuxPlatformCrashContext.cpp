// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Linux/LinuxPlatformCrashContext.h"
#include "Containers/StringConv.h"
#include "HAL/PlatformStackWalk.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformOutputDevices.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"
#include "HAL/FileManager.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Delegates/IDelegateInstance.h"
#include "Misc/Guid.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/OutputDeviceError.h"
#include "Containers/Ticker.h"
#include "Misc/FeedbackContext.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "HAL/PlatformMallocCrash.h"
#include "Linux/LinuxPlatformRunnableThread.h"
#include "ExceptionHandling.h"

#include "HAL/ThreadHeartBeat.h"

extern CORE_API bool GIsGPUCrashed;

FString DescribeSignal(int32 Signal, siginfo_t* Info, ucontext_t *Context)
{
	FString ErrorString;

#define HANDLE_CASE(a,b) case a: ErrorString += TEXT(#a ": " b); break;

	switch (Signal)
	{
	case 0:
		// No signal - used for initialization stacktrace on non-fatal errors (ex: ensure)
		break;
	case SIGSEGV:
#ifdef __x86_64__	// architecture-specific
		ErrorString += FString::Printf(TEXT("SIGSEGV: invalid attempt to %s memory at address 0x%016x"),
			(Context != nullptr) ? ((Context->uc_mcontext.gregs[REG_ERR] & 0x2) ? TEXT("write") : TEXT("read")) : TEXT("access"), (uint64)Info->si_addr);
#else
		ErrorString += FString::Printf(TEXT("SIGSEGV: invalid attempt to access memory at address 0x%016x"), (uint64)Info->si_addr);
#endif // __x86_64__
		break;
	case SIGBUS:
		ErrorString += FString::Printf(TEXT("SIGBUS: invalid attempt to access memory at address 0x%016x"), (uint64)Info->si_addr);
		break;

		HANDLE_CASE(SIGINT, "program interrupted")
		HANDLE_CASE(SIGQUIT, "user-requested crash")
		HANDLE_CASE(SIGILL, "illegal instruction")
		HANDLE_CASE(SIGTRAP, "trace trap")
		HANDLE_CASE(SIGABRT, "abort() called")
		HANDLE_CASE(SIGFPE, "floating-point exception")
		HANDLE_CASE(SIGKILL, "program killed")
		HANDLE_CASE(SIGSYS, "non-existent system call invoked")
		HANDLE_CASE(SIGPIPE, "write on a pipe with no reader")
		HANDLE_CASE(SIGTERM, "software termination signal")
		HANDLE_CASE(SIGSTOP, "stop")

	default:
		ErrorString += FString::Printf(TEXT("Signal %d (unknown)"), Signal);
	}

	return ErrorString;
#undef HANDLE_CASE
}

__thread siginfo_t FLinuxCrashContext::FakeSiginfoForEnsures;

FLinuxCrashContext::~FLinuxCrashContext()
{
	if (BacktraceSymbols)
	{
		// glibc uses malloc() to allocate this, and we only need to free one pointer, see http://www.gnu.org/software/libc/manual/html_node/Backtraces.html
		free(BacktraceSymbols);
		BacktraceSymbols = NULL;
	}
}

void FLinuxCrashContext::InitFromSignal(int32 InSignal, siginfo_t* InInfo, void* InContext)
{
	Signal = InSignal;
	Info = InInfo;
	Context = reinterpret_cast< ucontext_t* >( InContext );

	FCString::Strcat(SignalDescription, ARRAY_COUNT( SignalDescription ) - 1, *DescribeSignal(Signal, Info, Context));
}

void FLinuxCrashContext::InitFromEnsureHandler(const TCHAR* EnsureMessage, const void* CrashAddress)
{
	Signal = SIGTRAP;

	FakeSiginfoForEnsures.si_signo = SIGTRAP;
	FakeSiginfoForEnsures.si_code = TRAP_TRACE;
	FakeSiginfoForEnsures.si_addr = const_cast<void *>(CrashAddress);
	Info = &FakeSiginfoForEnsures;

	Context = nullptr;

	// set signal description to a more human-readable one for ensures
	FCString::Strcpy(SignalDescription, ARRAY_COUNT(SignalDescription) - 1, EnsureMessage);

	// only need the first string
	for (int Idx = 0; Idx < ARRAY_COUNT(SignalDescription); ++Idx)
	{
		if (SignalDescription[Idx] == TEXT('\n'))
		{
			SignalDescription[Idx] = 0;
			break;
		}
	}
}

/**
 * Handles graceful termination. Gives time to exit gracefully, but second signal will quit immediately.
 */
void GracefulTerminationHandler(int32 Signal, siginfo_t* Info, void* Context)
{
	// do not flush logs at this point; this can result in a deadlock if the signal was received while we were holding lock in the malloc (flushing allocates memory)
	if( !GIsRequestingExit )
	{
		FPlatformMisc::RequestExitWithStatus(false, 128 + Signal);	// Keeping the established shell practice of returning 128 + signal for terminations by signal. Allows to distinguish SIGINT/SIGTERM/SIGHUP.
	}
	else
	{
		FPlatformMisc::RequestExit(true);
	}
}

void CreateExceptionInfoString(int32 Signal, siginfo_t* Info, ucontext_t *Context)
{
	FString ErrorString = TEXT("Unhandled Exception: ");
	ErrorString += DescribeSignal(Signal, Info, Context);
	FCString::Strncpy(GErrorExceptionDescription, *ErrorString, FMath::Min(ErrorString.Len() + 1, (int32)ARRAY_COUNT(GErrorExceptionDescription)));
}

namespace
{
	/** 
	 * Write a line of UTF-8 to a file
	 */
	void WriteLine(FArchive* ReportFile, const ANSICHAR* Line = NULL)
	{
		if( Line != NULL )
		{
			int64 StringBytes = FCStringAnsi::Strlen(Line);
			ReportFile->Serialize(( void* )Line, StringBytes);
		}

		// use Windows line terminator
		static ANSICHAR WindowsTerminator[] = "\r\n";
		ReportFile->Serialize(WindowsTerminator, 2);
	}

	/**
	 * Serializes UTF string to UTF-16
	 */
	void WriteUTF16String(FArchive* ReportFile, const TCHAR * UTFString4BytesChar, uint32 NumChars)
	{
		check(UTFString4BytesChar != NULL || NumChars == 0);
		static_assert(sizeof(TCHAR) == 4, "Platform TCHAR is not 4 bytes. Revisit this function.");

		for (uint32 Idx = 0; Idx < NumChars; ++Idx)
		{
			ReportFile->Serialize(const_cast< TCHAR* >( &UTFString4BytesChar[Idx] ), 2);
		}
	}

	/** 
	 * Writes UTF-16 line to a file
	 */
	void WriteLine(FArchive* ReportFile, const TCHAR* Line)
	{
		if( Line != NULL )
		{
			int64 NumChars = FCString::Strlen(Line);
			WriteUTF16String(ReportFile, Line, NumChars);
		}

		// use Windows line terminator
		static TCHAR WindowsTerminator[] = TEXT("\r\n");
		WriteUTF16String(ReportFile, WindowsTerminator, 2);
	}
}

/** 
 * Write all the data mined from the minidump to a text file
 */
void FLinuxCrashContext::GenerateReport(const FString & DiagnosticsPath) const
{
	FArchive* ReportFile = IFileManager::Get().CreateFileWriter(*DiagnosticsPath);
	if (ReportFile != NULL)
	{
		FString Line;

		WriteLine(ReportFile, "Generating report for minidump");
		WriteLine(ReportFile);

		Line = FString::Printf(TEXT("Application version %d.%d.%d.0" ), FEngineVersion::Current().GetMajor(), FEngineVersion::Current().GetMinor(), FEngineVersion::Current().GetPatch());
		WriteLine(ReportFile, TCHAR_TO_UTF8(*Line));

		Line = FString::Printf(TEXT(" ... built from changelist %d"), FEngineVersion::Current().GetChangelist());
		WriteLine(ReportFile, TCHAR_TO_UTF8(*Line));
		WriteLine(ReportFile);

		utsname UnixName;
		if (uname(&UnixName) == 0)
		{
			Line = FString::Printf(TEXT( "OS version %s %s (network name: %s)" ), UTF8_TO_TCHAR(UnixName.sysname), UTF8_TO_TCHAR(UnixName.release), UTF8_TO_TCHAR(UnixName.nodename));
			WriteLine(ReportFile, TCHAR_TO_UTF8(*Line));	
			Line = FString::Printf( TEXT( "Running %d %s processors (%d logical cores)" ), FPlatformMisc::NumberOfCores(), UTF8_TO_TCHAR(UnixName.machine), FPlatformMisc::NumberOfCoresIncludingHyperthreads());
			WriteLine(ReportFile, TCHAR_TO_UTF8(*Line));
		}
		else
		{
			Line = FString::Printf(TEXT("OS version could not be determined (%d, %s)"), errno, UTF8_TO_TCHAR(strerror(errno)));
			WriteLine(ReportFile, TCHAR_TO_UTF8(*Line));	
			Line = FString::Printf( TEXT( "Running %d unknown processors" ), FPlatformMisc::NumberOfCores());
			WriteLine(ReportFile, TCHAR_TO_UTF8(*Line));
		}
		Line = FString::Printf(TEXT("Exception was \"%s\""), SignalDescription);
		WriteLine(ReportFile, TCHAR_TO_UTF8(*Line));
		WriteLine(ReportFile);

		WriteLine(ReportFile, "<SOURCE START>");
		WriteLine(ReportFile, "<SOURCE END>");
		WriteLine(ReportFile);

		WriteLine(ReportFile, "<CALLSTACK START>");
		WriteLine(ReportFile, MinidumpCallstackInfo);
		WriteLine(ReportFile, "<CALLSTACK END>");
		WriteLine(ReportFile);

		WriteLine(ReportFile, "0 loaded modules");

		WriteLine(ReportFile);

		Line = FString::Printf(TEXT("Report end!"));
		WriteLine(ReportFile, TCHAR_TO_UTF8(*Line));

		ReportFile->Close();
		delete ReportFile;
	}
}

/** 
 * Mimics Windows WER format
 */
void GenerateWindowsErrorReport(const FString & WERPath, bool bReportingNonCrash)
{
	FArchive* ReportFile = IFileManager::Get().CreateFileWriter(*WERPath);
	if (ReportFile != NULL)
	{
		// write BOM
		static uint16 ByteOrderMarker = 0xFEFF;
		ReportFile->Serialize(&ByteOrderMarker, sizeof(ByteOrderMarker));

		WriteLine(ReportFile, TEXT("<?xml version=\"1.0\" encoding=\"UTF-16\"?>"));
		WriteLine(ReportFile, TEXT("<WERReportMetadata>"));
		
		WriteLine(ReportFile, TEXT("\t<OSVersionInformation>"));
		WriteLine(ReportFile, TEXT("\t\t<WindowsNTVersion>0.0</WindowsNTVersion>"));
		WriteLine(ReportFile, TEXT("\t\t<Build>No Build</Build>"));
		WriteLine(ReportFile, TEXT("\t\t<Product>Linux</Product>"));
		WriteLine(ReportFile, TEXT("\t\t<Edition>No Edition</Edition>"));
		WriteLine(ReportFile, TEXT("\t\t<BuildString>No BuildString</BuildString>"));
		WriteLine(ReportFile, TEXT("\t\t<Revision>0</Revision>"));
		WriteLine(ReportFile, TEXT("\t\t<Flavor>No Flavor</Flavor>"));
		WriteLine(ReportFile, TEXT("\t\t<Architecture>Unknown Architecture</Architecture>"));
		WriteLine(ReportFile, TEXT("\t\t<LCID>0</LCID>"));
		WriteLine(ReportFile, TEXT("\t</OSVersionInformation>"));
		
		WriteLine(ReportFile, TEXT("\t<ParentProcessInformation>"));
		WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<ParentProcessId>%d</ParentProcessId>"), getppid()));
		WriteLine(ReportFile, TEXT("\t\t<ParentProcessPath>C:\\Windows\\explorer.exe</ParentProcessPath>"));			// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t\t<ParentProcessCmdLine>C:\\Windows\\Explorer.EXE</ParentProcessCmdLine>"));	// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t</ParentProcessInformation>"));

		WriteLine(ReportFile, TEXT("\t<ProblemSignatures>"));
		WriteLine(ReportFile, TEXT("\t\t<EventType>APPCRASH</EventType>"));
		WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<Parameter0>UE4-%s</Parameter0>"), FApp::GetProjectName()));
		WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<Parameter1>%d.%d.%d</Parameter1>"), FEngineVersion::Current().GetMajor(), FEngineVersion::Current().GetMinor(), FEngineVersion::Current().GetPatch()));
		WriteLine(ReportFile, TEXT("\t\t<Parameter2>0</Parameter2>"));													// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t\t<Parameter3>Unknown Fault Module</Parameter3>"));										// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t\t<Parameter4>0.0.0.0</Parameter4>"));													// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t\t<Parameter5>00000000</Parameter5>"));													// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t\t<Parameter6>00000000</Parameter6>"));													// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t\t<Parameter7>0000000000000000</Parameter7>"));											// FIXME: supply valid?
		WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<Parameter8>!%s!</Parameter8>"), FCommandLine::Get()));				// FIXME: supply valid? Only partially valid
		WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<Parameter9>%s!%s!%s!%d</Parameter9>"), *FApp::GetBranchName(), FPlatformProcess::BaseDir(), FPlatformMisc::GetEngineMode(), FEngineVersion::Current().GetChangelist()));
		WriteLine(ReportFile, TEXT("\t</ProblemSignatures>"));

		WriteLine(ReportFile, TEXT("\t<DynamicSignatures>"));
		WriteLine(ReportFile, TEXT("\t\t<Parameter1>6.1.7601.2.1.0.256.48</Parameter1>"));
		WriteLine(ReportFile, TEXT("\t\t<Parameter2>1033</Parameter2>"));
		WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<DeploymentName>%s</DeploymentName>"), FApp::GetDeploymentName()));
		WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<BuildVersion>%s</BuildVersion>"), FApp::GetBuildVersion()));
		WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<IsEnsure>%s</IsEnsure>"), bReportingNonCrash ? TEXT("1") : TEXT("0")));
		WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<IsAssert>%s</IsAssert>"), FDebug::bHasAsserted ? TEXT("1") : TEXT("0")));
		WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<CrashType>%s</CrashType>"), FGenericCrashContext::GetCrashTypeString(bReportingNonCrash, FDebug::bHasAsserted, GIsGPUCrashed)));
		WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<EngineModeEx>%s</EngineModeEx>"), FGenericCrashContext::EngineModeExString()));
		WriteLine(ReportFile, TEXT("\t</DynamicSignatures>"));

		WriteLine(ReportFile, TEXT("\t<SystemInformation>"));
		WriteLine(ReportFile, TEXT("\t\t<MID>11111111-2222-3333-4444-555555555555</MID>"));							// FIXME: supply valid?
		
		WriteLine(ReportFile, TEXT("\t\t<SystemManufacturer>Unknown.</SystemManufacturer>"));						// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t\t<SystemProductName>Linux machine</SystemProductName>"));					// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t\t<BIOSVersion>A02</BIOSVersion>"));											// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t</SystemInformation>"));

		WriteLine(ReportFile, TEXT("</WERReportMetadata>"));

		ReportFile->Close();
		delete ReportFile;
	}
}

/** 
 * Creates (fake so far) minidump
 */
void GenerateMinidump(const FString & Path)
{
	FArchive* ReportFile = IFileManager::Get().CreateFileWriter(*Path);
	if (ReportFile != NULL)
	{
		// write BOM
		static uint32 Garbage = 0xDEADBEEF;
		ReportFile->Serialize(&Garbage, sizeof(Garbage));

		ReportFile->Close();
		delete ReportFile;
	}
}


void FLinuxCrashContext::CaptureStackTrace()
{
	// Only do work the first time this function is called - this is mainly a carry over from Windows where it can be called multiple times, left intact for extra safety.
	if (!bCapturedBacktrace)
	{
		const SIZE_T StackTraceSize = 65535;
		ANSICHAR* StackTrace = (ANSICHAR*) FMemory::Malloc( StackTraceSize );
		StackTrace[0] = 0;
		// Walk the stack and dump it to the allocated memory (do not ignore any stack frames to be consistent with check()/ensure() handling)
		FPlatformStackWalk::StackWalkAndDump( StackTrace, StackTraceSize, 0, this);

		FCString::Strncat( GErrorHist, UTF8_TO_TCHAR(StackTrace), ARRAY_COUNT(GErrorHist) - 1 );
		CreateExceptionInfoString(Signal, Info, Context);

		FMemory::Free( StackTrace );
		bCapturedBacktrace = true;
	}
}

namespace LinuxCrashReporterTracker
{
	FProcHandle CurrentlyRunningCrashReporter;
	FDelegateHandle CurrentTicker;

	bool Tick(float DeltaTime)
	{
		if (!FPlatformProcess::IsProcRunning(CurrentlyRunningCrashReporter))
		{
			FPlatformProcess::CloseProc(CurrentlyRunningCrashReporter);
			CurrentlyRunningCrashReporter = FProcHandle();

			FTicker::GetCoreTicker().RemoveTicker(CurrentTicker);
			CurrentTicker.Reset();

			UE_LOG(LogLinux, Log, TEXT("Done sending crash report for ensure()."));
			return false;
		}

		// tick again
		return true;
	}

	/**
	 * Waits for the proc with timeout (busy loop, workaround for platform abstraction layer not exposing this)
	 *
	 * @param Proc proc handle to wait for
	 * @param TimeoutInSec timeout in seconds
	 * @param SleepIntervalInSec sleep interval (the smaller the more CPU we will eat, but the faster we will detect the program exiting)
	 *
	 * @return true if exited cleanly, false if timeout has expired
	 */
	bool WaitForProcWithTimeout(FProcHandle Proc, const double TimeoutInSec, const double SleepIntervalInSec)
	{
		double StartSeconds = FPlatformTime::Seconds();
		for (;;)
		{
			if (!FPlatformProcess::IsProcRunning(Proc))
			{
				break;
			}

			if (FPlatformTime::Seconds() - StartSeconds > TimeoutInSec)
			{
				return false;
			}

			FPlatformProcess::Sleep(SleepIntervalInSec);
		};

		return true;
	}
}


void FLinuxCrashContext::GenerateCrashInfoAndLaunchReporter(bool bReportingNonCrash) const
{
	// do not report crashes for tools (particularly for crash reporter itself)
#if !IS_PROGRAM

	// create a crash-specific directory
	FString CrashGuid;
	if (!FParse::Value(FCommandLine::Get(), TEXT("CrashGUID="), CrashGuid) || CrashGuid.Len() <= 0)
	{
		CrashGuid = FGuid::NewGuid().ToString();
	}

	FString CrashInfoFolder = FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("Crashes"), *FString::Printf(TEXT("%sinfo-%s-pid-%d-%s"), bReportingNonCrash ? TEXT("ensure") : TEXT("crash"), FApp::GetProjectName(), getpid(), *CrashGuid));
	FString CrashInfoAbsolute = FPaths::ConvertRelativePathToFull(CrashInfoFolder);
	if (IFileManager::Get().MakeDirectory(*CrashInfoAbsolute, true))
	{
		// generate "minidump"
		GenerateReport(FPaths::Combine(*CrashInfoAbsolute, TEXT("Diagnostics.txt")));

		// generate "WER"
		GenerateWindowsErrorReport(FPaths::Combine(*CrashInfoAbsolute, TEXT("wermeta.xml")), bReportingNonCrash);

		// generate "minidump" (just >1 byte)
		GenerateMinidump(FPaths::Combine(*CrashInfoAbsolute, TEXT("minidump.dmp")));

		// Introduces a new runtime crash context. Will replace all Windows related crash reporting.
		//FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
		//FCStringAnsi::Strncat(FilePath, "/", PATH_MAX);
		//FCStringAnsi::Strncat(FilePath, FGenericCrashContext::CrashContextRuntimeXMLNameA, PATH_MAX);
		//SerializeAsXML( FilePath ); @todo uncomment after verification

		// copy log
		FString LogSrcAbsolute = FPlatformOutputDevices::GetAbsoluteLogFilename();
		FString LogFolder = FPaths::GetPath(LogSrcAbsolute);
		FString LogFilename = FPaths::GetCleanFilename(LogSrcAbsolute);
		FString LogBaseFilename = FPaths::GetBaseFilename(LogSrcAbsolute);
		FString LogExtension = FPaths::GetExtension(LogSrcAbsolute, true);
		FString LogDstAbsolute = FPaths::Combine(*CrashInfoAbsolute, *LogFilename);
		FPaths::NormalizeDirectoryName(LogDstAbsolute);
		static_cast<void>(IFileManager::Get().Copy(*LogDstAbsolute, *LogSrcAbsolute));	// best effort, so don't care about result: couldn't copy -> tough, no log

		// If present, include the crash report config file to pass config values to the CRC
		const TCHAR* CrashConfigFilePath = GetCrashConfigFilePath();
		if (IFileManager::Get().FileExists(CrashConfigFilePath))
		{
			FString CrashConfigFilename = FPaths::GetCleanFilename(CrashConfigFilePath);
			FString CrashConfigDstAbsolute = FPaths::Combine(*CrashInfoAbsolute, *CrashConfigFilename);
			static_cast<void>(IFileManager::Get().Copy(*CrashConfigDstAbsolute, CrashConfigFilePath));	// best effort, so don't care about result
		}

		// try launching the tool and wait for its exit, if at all
		const TCHAR * RelativePathToCrashReporter = TEXT("../../../Engine/Binaries/Linux/CrashReportClient");	// FIXME: painfully hard-coded
		if (!FPaths::FileExists(RelativePathToCrashReporter))
		{
			RelativePathToCrashReporter = TEXT("../../../engine/binaries/linux/crashreportclient");	// FIXME: even more painfully hard-coded
		}

		FString CrashReportLogFilename = LogBaseFilename + TEXT("-CRC") + LogExtension;
		FString CrashReportLogFilepath = FPaths::Combine(*LogFolder, *CrashReportLogFilename);
		FString CrashReportClientArguments = TEXT(" -Abslog=");
		CrashReportClientArguments += *CrashReportLogFilepath;
		CrashReportClientArguments += TEXT(" ");

		// Suppress the user input dialog if we're running in unattended mode
		bool bNoDialog = FApp::IsUnattended() || (!IsInteractiveEnsureMode() && bReportingNonCrash) || IsRunningDedicatedServer();
		if (bNoDialog)
		{
			CrashReportClientArguments += TEXT(" -Unattended ");
		}

		CrashReportClientArguments += CrashInfoAbsolute + TEXT("/");

		if (bReportingNonCrash)
		{
			// If we're reporting non-crash, try to avoid spinning here and instead do that in the tick.
			// However, if there was already a crash reporter running (i.e. we hit ensure() too quickly), take a hitch here
			if (FPlatformProcess::IsProcRunning(LinuxCrashReporterTracker::CurrentlyRunningCrashReporter))
			{
				// do not wait indefinitely, allow 45 second hitch (anticipating callstack parsing)
				const double kEnsureTimeOut = 45.0;
				const double kEnsureSleepInterval = 0.1;
				if (!LinuxCrashReporterTracker::WaitForProcWithTimeout(LinuxCrashReporterTracker::CurrentlyRunningCrashReporter, kEnsureTimeOut, kEnsureSleepInterval))
				{
					FPlatformProcess::TerminateProc(LinuxCrashReporterTracker::CurrentlyRunningCrashReporter);
				}

				LinuxCrashReporterTracker::Tick(0.001f);	// tick one more time to make it clean up after itself
			}

			LinuxCrashReporterTracker::CurrentlyRunningCrashReporter = FPlatformProcess::CreateProc(RelativePathToCrashReporter, *CrashReportClientArguments, true, false, false, NULL, 0, NULL, NULL);
			LinuxCrashReporterTracker::CurrentTicker = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&LinuxCrashReporterTracker::Tick), 1.f);
		}
		else
		{
			// spin here until CrashReporter exits
			FProcHandle RunningProc = FPlatformProcess::CreateProc(RelativePathToCrashReporter, *CrashReportClientArguments, true, false, false, NULL, 0, NULL, NULL);

			// do not wait indefinitely - can be more generous about the hitch than in ensure() case
			// NOTE: Chris.Wood - increased from 3 to 8 mins because server crashes were timing out and getting lost
			// NOTE: Do not increase above 8.5 mins without altering watchdog scripts to match
			const double kCrashTimeOut = 8 * 60.0;

			const double kCrashSleepInterval = 1.0;
			if (!LinuxCrashReporterTracker::WaitForProcWithTimeout(RunningProc, kCrashTimeOut, kCrashSleepInterval))
			{
				FPlatformProcess::TerminateProc(RunningProc);
			}

			FPlatformProcess::CloseProc(RunningProc);
		}
	}

#endif

	if (!bReportingNonCrash)
	{
		// remove the handler for this signal and re-raise it (which should generate the proper core dump)
		// print message to stdout directly, it may be too late for the log (doesn't seem to be printed during a crash in the thread) 
		printf("Engine crash handling finished; re-raising signal %d for the default handler. Good bye.\n", Signal);
		fflush(stdout);

		struct sigaction ResetToDefaultAction;
		FMemory::Memzero(ResetToDefaultAction);
		ResetToDefaultAction.sa_handler = SIG_DFL;
		sigfillset(&ResetToDefaultAction.sa_mask);
		sigaction(Signal, &ResetToDefaultAction, nullptr);

		raise(Signal);
	}
}

/**
 * Good enough default crash reporter.
 */
void DefaultCrashHandler(const FLinuxCrashContext & Context)
{
	printf("DefaultCrashHandler: Signal=%d\n", Context.Signal);

	// Stop the heartbeat thread so that it doesn't interfere with crashreporting
	FThreadHeartBeat::Get().Stop();

	// at this point we should already be using malloc crash handler (see PlatformCrashHandler)

	const_cast<FLinuxCrashContext&>(Context).CaptureStackTrace();
	if (GLog)
	{
		GLog->Flush();
	}
	if (GWarn)
	{
		GWarn->Flush();
	}
	if (GError)
	{
		GError->Flush();
		GError->HandleError();
	}

	return Context.GenerateCrashInfoAndLaunchReporter();
}

/** Global pointer to crash handler */
void (* GCrashHandlerPointer)(const FGenericCrashContext & Context) = NULL;

/** True system-specific crash handler that gets called first */
void PlatformCrashHandler(int32 Signal, siginfo_t* Info, void* Context)
{
	fprintf(stderr, "Signal %d caught.\n", Signal);

	// Stop the heartbeat thread
	FThreadHeartBeat::Get().Stop();

	// Switch to malloc crash.
	FPlatformMallocCrash::Get().SetAsGMalloc();

	FLinuxCrashContext CrashContext;
	CrashContext.InitFromSignal(Signal, Info, Context);

	if (GCrashHandlerPointer)
	{
		GCrashHandlerPointer(CrashContext);
	}
	else
	{
		// call default one
		DefaultCrashHandler(CrashContext);
	}
}

void FLinuxPlatformMisc::SetGracefulTerminationHandler()
{
	struct sigaction Action;
	FMemory::Memzero(Action);
	Action.sa_sigaction = GracefulTerminationHandler;
	sigfillset(&Action.sa_mask);
	Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
	sigaction(SIGINT, &Action, nullptr);
	sigaction(SIGTERM, &Action, nullptr);
	sigaction(SIGHUP, &Action, nullptr);	//  this should actually cause the server to just re-read configs (restart?)
}

// reserve stack for the main thread in BSS
char FRunnableThreadLinux::MainThreadSignalHandlerStack[FRunnableThreadLinux::EConstants::CrashHandlerStackSize];

void FLinuxPlatformMisc::SetCrashHandler(void (* CrashHandler)(const FGenericCrashContext & Context))
{
	GCrashHandlerPointer = CrashHandler;

	// This table lists all signals that we handle. 0 is not a valid signal, it is used as a separator: everything 
	// before is considered a crash and handled by the crash handler; everything above it is handled elsewhere 
	// and also omitted from setting to ignore
	int HandledSignals[] = 
	{
		// signals we consider crashes
		SIGQUIT,
		SIGABRT,
		SIGILL, 
		SIGFPE, 
		SIGBUS, 
		SIGSEGV, 
		SIGSYS,
		SIGTRAP,
		0,	// marks the end of crash signals
		SIGINT,
		SIGTERM,
		SIGHUP,
		SIGCHLD
	};

	struct sigaction Action;
	FMemory::Memzero(Action);
	sigfillset(&Action.sa_mask);
	Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
	Action.sa_sigaction = PlatformCrashHandler;

	// install this handler for all the "crash" signals
	for (int Signal : HandledSignals)
	{
		if (!Signal)
		{
			// hit the end of crash signals, the rest is already handled elsewhere
			break;
		}
		sigaction(Signal, &Action, nullptr);
	}

	// reinitialize the structure, since assigning to both sa_handler and sa_sigacton is ill-advised
	FMemory::Memzero(Action);
	sigfillset(&Action.sa_mask);
	Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
	Action.sa_handler = SIG_IGN;

	// set all the signals except ones we know we are handling to be ignored
	for (int Signal = 1; Signal < NSIG; ++Signal)
	{
		bool bSignalShouldBeIgnored = true;
		for (int HandledSignal : HandledSignals)
		{
			if (Signal == HandledSignal)
			{
				bSignalShouldBeIgnored = false;
				break;
			}
		}

		if (bSignalShouldBeIgnored)
		{
			sigaction(Signal, &Action, nullptr);
		}
	}

	checkf(IsInGameThread(), TEXT("Crash handler for the game thread should be set from the game thread only."));

	FRunnableThreadLinux::SetupSignalHandlerStack(FRunnableThreadLinux::MainThreadSignalHandlerStack, sizeof(FRunnableThreadLinux::MainThreadSignalHandlerStack), nullptr);
}
