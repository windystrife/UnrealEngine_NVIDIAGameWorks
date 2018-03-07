// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/OutputDeviceError.h"
#include "LaunchEngineLoop.h"
#include "PhysicsPublic.h"
#include "HAL/ExceptionHandling.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "Stats/StatsMisc.h"
#include "Misc/CoreDelegates.h"
#include "Misc/EngineVersion.h"
#include "Misc/ScopedSlowTask.h"
#if WITH_EDITOR
	#include "UnrealEdGlobals.h"
#endif
#if PLATFORM_WINDOWS
	#include "WindowsHWrapper.h"
#endif


IMPLEMENT_MODULE(FDefaultModuleImpl, Launch);

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

FEngineLoop	GEngineLoop;
bool GIsConsoleExecutable = false;


extern "C" int test_main(int argc, char ** argp)
{
	return 0;
}

/** 
 * PreInits the engine loop 
 */
int32 EnginePreInit( const TCHAR* CmdLine )
{
	int32 ErrorLevel = GEngineLoop.PreInit( CmdLine );

	return( ErrorLevel );
}

/** 
 * Inits the engine loop 
 */
int32 EngineInit()
{
	int32 ErrorLevel = GEngineLoop.Init();

	return( ErrorLevel );
}

/** 
 * Ticks the engine loop 
 */
void EngineTick( void )
{
	GEngineLoop.Tick();
}

/**
 * Shuts down the engine
 */
void EngineExit( void )
{
	// Make sure this is set
	GIsRequestingExit = true;

	GEngineLoop.Exit();
}

/**
 * Performs any required cleanup in the case of a fatal error.
 */
void LaunchStaticShutdownAfterError()
{
	// Make sure physics is correctly torn down.
	TermGamePhys();
}

#if WITH_EDITOR
extern UNREALED_API FSecondsCounterData BlueprintCompileAndLoadTimerData;
#endif

/**
 * Static guarded main function. Rolled into own function so we can have error handling for debug/ release builds depending
 * on whether a debugger is attached or not.
 */
#if PLATFORM_WINDOWS
int32 GuardedMain( const TCHAR* CmdLine, HINSTANCE hInInstance, HINSTANCE hPrevInstance, int32 nCmdShow )
#else
int32 GuardedMain( const TCHAR* CmdLine )
#endif
{
	// Super early init code. DO NOT MOVE THIS ANYWHERE ELSE!
	FCoreDelegates::GetPreMainInitDelegate().Broadcast();

	// make sure GEngineLoop::Exit() is always called.
	struct EngineLoopCleanupGuard 
	{ 
		~EngineLoopCleanupGuard()
		{
			EngineExit();
		}
	} CleanupGuard;

	// Set up minidump filename. We cannot do this directly inside main as we use an FString that requires 
	// destruction and main uses SEH.
	// These names will be updated as soon as the Filemanager is set up so we can write to the log file.
	// That will also use the user folder for installed builds so we don't write into program files or whatever.
#if PLATFORM_WINDOWS
	FCString::Strcpy(MiniDumpFilenameW, *FString::Printf(TEXT("unreal-v%i-%s.dmp"), FEngineVersion::Current().GetChangelist(), *FDateTime::Now().ToString()));

	const TCHAR* OrgCmdLine = CmdLine;

	CmdLine = FCommandLine::RemoveExeName(OrgCmdLine);

	const TCHAR* CmdOccurence = FCString::Stristr(OrgCmdLine, TEXT("-cmd"));
	GIsConsoleExecutable = CmdOccurence != NULL && CmdOccurence < CmdLine;
	
#endif

	int32 ErrorLevel = EnginePreInit( CmdLine );

	// exit if PreInit failed.
	if ( ErrorLevel != 0 || GIsRequestingExit )
	{
		return ErrorLevel;
	}

	{
		FScopedSlowTask SlowTask(100, NSLOCTEXT("EngineInit", "EngineInit_Loading", "Loading..."));

		// EnginePreInit leaves 20% unused in its slow task.
		// Here we consume 80% immediately so that the percentage value on the splash screen doesn't change from one slow task to the next.
		// (Note, we can't include the call to EnginePreInit in this ScopedSlowTask, because the engine isn't fully initialized at that point)
		SlowTask.EnterProgressFrame(80);

		SlowTask.EnterProgressFrame(20);

#if WITH_EDITOR
		if (GIsEditor)
		{
			ErrorLevel = EditorInit(GEngineLoop);
		}
		else
#endif
		{
			ErrorLevel = EngineInit();
		}
	}

	double EngineInitializationTime = FPlatformTime::Seconds() - GStartTime;
	UE_LOG(LogLoad, Log, TEXT("(Engine Initialization) Total time: %.2f seconds"), EngineInitializationTime);

#if WITH_EDITOR
	UE_LOG(LogLoad, Log, TEXT("(Engine Initialization) Total Blueprint compile time: %.2f seconds"), BlueprintCompileAndLoadTimerData.GetTime());
#endif

	ACCUM_LOADTIME(TEXT("EngineInitialization"), EngineInitializationTime);

	while( !GIsRequestingExit )
	{
		EngineTick();
	}

#if WITH_EDITOR
	if( GIsEditor )
	{
		EditorExit();
	}
#endif
	return ErrorLevel;
}

#endif
