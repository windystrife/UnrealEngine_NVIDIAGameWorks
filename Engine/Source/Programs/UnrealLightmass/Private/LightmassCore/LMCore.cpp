// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LMCore.h"
#include "Misc/Guid.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Templates/ScopedPointer.h"
#include "UnrealLightmass.h"
#include "Misc/Paths.h"
#include "UniquePtr.h"

namespace Lightmass
{

/*-----------------------------------------------------------------------------
	Logging functionality
-----------------------------------------------------------------------------*/

FLightmassLog::FLightmassLog()
{
	Filename[0] = 0;

	// Create a Guid for this run.
	FGuid Guid = FGuid::NewGuid();

	// get the app name to base the log name off of
	FString ExeName = FPlatformProcess::ExecutableName();
	ExeName = ExeName.Replace(TEXT("\\"), TEXT("/"));
	// Extract filename part and add "-[guid].log"
	int32 ExeNameLen = ExeName.Len();
	int32 PathSeparatorPos = ExeNameLen - 1;
	while ( PathSeparatorPos >= 0 && ExeName[PathSeparatorPos] != TEXT('/') )
	{
		PathSeparatorPos--;
	}
#if PLATFORM_MAC
	FString LogName = FPaths::Combine( FPlatformProcess::UserLogsDir(), *FString( ExeNameLen - PathSeparatorPos, *ExeName + PathSeparatorPos + 1 ) );
#else
	FString LogName( ExeNameLen - PathSeparatorPos - 5, *ExeName + PathSeparatorPos + 1 );
#endif
	LogName += TEXT("_");
	LogName += FPlatformProcess::ComputerName();
	LogName += TEXT("_");
	LogName += Guid.ToString();
	LogName += TEXT(".log");
	FCString::Strncpy( Filename, *LogName, PLATFORM_MAX_FILEPATH_LENGTH );

	// open the file for writing
	File = IFileManager::Get().CreateFileWriter(*LogName);

	// mark the file to be unicode
	if (File != NULL)
	{
		uint16 UnicodeBOM = 0xfeff;
		(*File) << UnicodeBOM;
	}
	else
	{
		// print to the screen that we failed to open the file
		wprintf(TEXT("\nFailed to open the log file '%s' for writing\n\n"), *LogName);
	}
}

FLightmassLog::~FLightmassLog()
{
	delete File;
}


void FLightmassLog::Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	// write it out to disk
	if (File != NULL)
	{
		File->Serialize((void*)V, FCString::Strlen(V) * sizeof(TCHAR));
		File->Serialize((void*)TEXT("\r\n"), 2 * sizeof(TCHAR));
	}

	// also print it to the screen and debugger output
#if PLATFORM_USE_LS_SPEC_FOR_WIDECHAR
	printf("%ls\n", V);
#else
	wprintf(TEXT("%s\n"), V);
#endif
	fflush( stdout );

	if( FPlatformMisc::IsDebuggerPresent() )
	{
		FPlatformMisc::LowLevelOutputDebugString( V );
		FPlatformMisc::LowLevelOutputDebugString( TEXT("\n") );
		fflush( stderr );
	}
}


void FLightmassLog::Flush()
{
	File->Flush();
}

static TUniquePtr<FLightmassLog>	GScopedLog;

FLightmassLog* FLightmassLog::Get()
{
	static FLightmassLog LogInstance;
	return &LogInstance;
}

/** CPU frequency for stats, only used for inner loop timing with rdtsc. */
double GCPUFrequency = 3000000000.0;

/** Number of CPU clock cycles per second (as counted by __rdtsc). */
double GSecondPerCPUCycle = 1.0 / 3000000000.0;

struct FInitCPUFrequency
{
	uint64 StartCPUTime;
	uint64 EndCPUTime;
	double StartTime;
	double EndTime;
};
static FInitCPUFrequency GInitCPUFrequency;

/** Start initializing CPU frequency (as counted by __rdtsc). */
void StartInitCPUFrequency()
{
	GInitCPUFrequency.StartTime		= FPlatformTime::Seconds();
	GInitCPUFrequency.StartCPUTime	= __rdtsc();
}

/** Finish initializing CPU frequency (as counted by __rdtsc), and set up CPUFrequency and CPUCyclesPerSecond. */
void FinishInitCPUFrequency()
{
	GInitCPUFrequency.EndTime		= FPlatformTime::Seconds();
	GInitCPUFrequency.EndCPUTime	= __rdtsc();
	double NumSeconds				= GInitCPUFrequency.EndTime - GInitCPUFrequency.StartTime;
	GCPUFrequency					= double(GInitCPUFrequency.EndCPUTime - GInitCPUFrequency.StartCPUTime) / NumSeconds;
	GSecondPerCPUCycle				= NumSeconds / double(GInitCPUFrequency.EndCPUTime - GInitCPUFrequency.StartCPUTime);
	UE_LOG(LogLightmass, Log, TEXT("Measured CPU frequency: %.2f GHz"), GCPUFrequency/1000000000.0);
}

}


