// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CrashDebugHelperWindows.h"
#include "CrashDebugHelperPrivate.h"
#include "WindowsPlatformStackWalkExt.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"

#include "EngineVersion.h"
#include "ISourceControlModule.h"

#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"
#include <DbgHelp.h>

bool FCrashDebugHelperWindows::CreateMinidumpDiagnosticReport( const FString& InCrashDumpFilename )
{
	const bool bSyncSymbols = FParse::Param( FCommandLine::Get(), TEXT( "SyncSymbols" ) );
	const bool bAnnotate = FParse::Param( FCommandLine::Get(), TEXT( "Annotate" ) );
	const bool bNoTrim = FParse::Param(FCommandLine::Get(), TEXT("NoTrimCallstack"));
	const bool bUseSCC = bSyncSymbols || bAnnotate;

	if( bUseSCC )
	{
		InitSourceControl( false );
	}

	FWindowsPlatformStackWalkExt WindowsStackWalkExt( CrashInfo );

	const bool bReady = WindowsStackWalkExt.InitStackWalking();

	bool bHasAtLeastThreeValidFunctions = false;
	if( bReady && WindowsStackWalkExt.OpenDumpFile( InCrashDumpFilename ) )
	{
		if (CrashInfo.BuiltFromCL != FCrashInfo::INVALID_CHANGELIST)
		{
			// Get the build version and modules paths.
			FCrashModuleInfo ExeFileVersion;
			WindowsStackWalkExt.GetExeFileVersionAndModuleList(ExeFileVersion);

			// Init Symbols
			bool bInitSymbols = false;
			if (CrashInfo.bMutexPDBCache && !CrashInfo.PDBCacheLockName.IsEmpty())
			{
				// Scoped lock
				UE_LOG(LogCrashDebugHelper, Log, TEXT("Locking for InitSymbols()"));
				FSystemWideCriticalSection PDBCacheLock(CrashInfo.PDBCacheLockName, FTimespan::FromMinutes(10.0));
				if (PDBCacheLock.IsValid())
				{
					bInitSymbols = InitSymbols(WindowsStackWalkExt, bSyncSymbols);
				}
				UE_LOG(LogCrashDebugHelper, Log, TEXT("Unlocking after InitSymbols()"));
			}
			else
			{
				bInitSymbols = InitSymbols(WindowsStackWalkExt, bSyncSymbols);
			}

			if (bInitSymbols)
			{
				// Get all the info we should ever need about the modules
				WindowsStackWalkExt.GetModuleInfoDetailed();

				// Get info about the system that created the minidump
				WindowsStackWalkExt.GetSystemInfo();

				// Get all the thread info
				WindowsStackWalkExt.GetThreadInfo();

				// Get exception info
				WindowsStackWalkExt.GetExceptionInfo();

				// Get the callstacks for each thread
				bHasAtLeastThreeValidFunctions = WindowsStackWalkExt.GetCallstacks(!bNoTrim) >= 3;

				// Sync the source file where the crash occurred
				if (CrashInfo.SourceFile.Len() > 0)
				{
					const bool bMutexSourceSync = FParse::Param(FCommandLine::Get(), TEXT("MutexSourceSync"));
					FString SourceSyncLockName;
					FParse::Value(FCommandLine::Get(), TEXT("SourceSyncLock="), SourceSyncLockName);

					if (bMutexSourceSync && !SourceSyncLockName.IsEmpty())
					{
						// Scoped lock
						UE_LOG(LogCrashDebugHelper, Log, TEXT("Locking for SyncAndReadSourceFile()"));
						const FTimespan GlobalLockWaitTimeout = FTimespan::FromSeconds(30.0);
						FSystemWideCriticalSection SyncSourceLock(SourceSyncLockName, GlobalLockWaitTimeout);
						if (SyncSourceLock.IsValid())
						{
							SyncAndReadSourceFile(bSyncSymbols, bAnnotate, CrashInfo.BuiltFromCL);
						}
						UE_LOG(LogCrashDebugHelper, Log, TEXT("Unlocking after SyncAndReadSourceFile()"));
					}
					else
					{
						SyncAndReadSourceFile(bSyncSymbols, bAnnotate, CrashInfo.BuiltFromCL);
					}
				}
			}
			else
			{
				UE_LOG(LogCrashDebugHelper, Warning, TEXT("InitSymbols failed"));
			}
		}
		else
		{
			UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Invalid built from changelist" ) );
		}
	}
	else
	{
		UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Failed to open crash dump file: %s" ), *InCrashDumpFilename );
	}

	if( bUseSCC )
	{
		ShutdownSourceControl();
	}

	return bHasAtLeastThreeValidFunctions;
}

bool FCrashDebugHelperWindows::InitSymbols(FWindowsPlatformStackWalkExt& WindowsStackWalkExt, bool bSyncSymbols)
{
	// CrashInfo now contains a changelist to lookup a label for
	if (bSyncSymbols)
	{
		FindSymbolsAndBinariesStorage();

		bool bPDBCacheEntryValid = false;
		const bool bSynced = SyncModules(bPDBCacheEntryValid);
		// Without symbols we can't decode the provided minidump.
		if (!bSynced)
		{
			return false;
		}

		if (!bPDBCacheEntryValid)
		{
			// early-out option
			const bool bForceUsePDBCache = FParse::Param(FCommandLine::Get(), TEXT("ForceUsePDBCache"));
			if (bForceUsePDBCache)
			{
				UE_LOG(LogCrashDebugHelper, Log, TEXT("No cached symbols available. Exiting due to -ForceUsePDBCache."));
				return false;
			}
		}
	}

	// Initialise the symbol options
	WindowsStackWalkExt.InitSymbols();

	// Set the symbol path based on the loaded modules
	WindowsStackWalkExt.SetSymbolPathsFromModules();

	return true;
}

void FCrashDebugHelperWindows::SyncAndReadSourceFile(bool bSyncSymbols, bool bAnnotate, int32 BuiltFromCL)
{
	if (bSyncSymbols && BuiltFromCL > 0)
	{
		UE_LOG(LogCrashDebugHelper, Log, TEXT("Using CL %i to sync crash source file"), BuiltFromCL);
		SyncSourceFile();
	}

	// Try to annotate the file if requested
	bool bAnnotationSuccessful = false;
	if (bAnnotate)
	{
		bAnnotationSuccessful = AddAnnotatedSourceToReport();
	}

	// If annotation is not requested, or failed, add the standard source context
	if (!bAnnotationSuccessful)
	{
		AddSourceToReport();
	}
}

#include "HideWindowsPlatformTypes.h"
