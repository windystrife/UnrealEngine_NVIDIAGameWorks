// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Core includes.
#include "Misc/CoreMisc.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/ScopeLock.h"

/** For FConfigFile in appInit							*/
#include "Misc/ConfigCacheIni.h"

#include "Modules/ModuleManager.h"
#include "DerivedDataCacheInterface.h"
#include "Interfaces/ITargetPlatformManagerModule.h"

DEFINE_LOG_CATEGORY(LogSHA);
DEFINE_LOG_CATEGORY(LogStats);
DEFINE_LOG_CATEGORY(LogStreaming);
DEFINE_LOG_CATEGORY(LogInit);
DEFINE_LOG_CATEGORY(LogExit);
DEFINE_LOG_CATEGORY(LogExec);
DEFINE_LOG_CATEGORY(LogScript);
DEFINE_LOG_CATEGORY(LogLocalization);
DEFINE_LOG_CATEGORY(LogLongPackageNames);
DEFINE_LOG_CATEGORY(LogProcess);
DEFINE_LOG_CATEGORY(LogLoad);
DEFINE_LOG_CATEGORY(LogCore);


/*-----------------------------------------------------------------------------
	FSelfRegisteringExec implementation.
-----------------------------------------------------------------------------*/

/** Constructor, registering this instance. */
FSelfRegisteringExec::FSelfRegisteringExec()
{
	GetRegisteredExecs().Add( this );
}

/** Destructor, unregistering this instance. */
FSelfRegisteringExec::~FSelfRegisteringExec()
{
	verify( GetRegisteredExecs().Remove( this ) == 1 );
}

bool FSelfRegisteringExec::StaticExec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	const TArray<FSelfRegisteringExec*>& RegisteredExecs = GetRegisteredExecs();
	for(int32 ExecIndex = 0;ExecIndex < RegisteredExecs.Num();++ExecIndex)
	{
		if(RegisteredExecs[ExecIndex]->Exec( InWorld, Cmd,Ar ))
		{
			return true;
		}
	}

	return false;
}

TArray<FSelfRegisteringExec*>& FSelfRegisteringExec::GetRegisteredExecs()
{
	static TArray<FSelfRegisteringExec*>* RegisteredExecs = new TArray<FSelfRegisteringExec*>();
	return *RegisteredExecs;
}

FStaticSelfRegisteringExec::FStaticSelfRegisteringExec(bool (*InStaticExecFunc)(UWorld* Inworld, const TCHAR* Cmd,FOutputDevice& Ar))
:	StaticExecFunc(InStaticExecFunc)
{}

bool FStaticSelfRegisteringExec::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	return (*StaticExecFunc)( InWorld, Cmd,Ar);
}


			// Remove old UE4 crash contexts


/*-----------------------------------------------------------------------------
	Module singletons.
-----------------------------------------------------------------------------*/

class FDerivedDataCacheInterface* GetDerivedDataCache()
{
	static class FDerivedDataCacheInterface* SingletonInterface = NULL;
	if (!FPlatformProperties::RequiresCookedData())
	{
		static bool bInitialized = false;
		if (!bInitialized)
		{
			check(IsInGameThread());
			bInitialized = true;
			class IDerivedDataCacheModule* Module = FModuleManager::LoadModulePtr<IDerivedDataCacheModule>("DerivedDataCache");
			if (Module)
			{
				SingletonInterface = &Module->GetDDC();
			}
		}
	}
	return SingletonInterface;
}

class FDerivedDataCacheInterface& GetDerivedDataCacheRef()
{
	class FDerivedDataCacheInterface* SingletonInterface = GetDerivedDataCache();
	if (!SingletonInterface)
	{
		UE_LOG(LogInit, Fatal, TEXT("Derived Data Cache was requested, but not available."));
		CA_ASSUME( SingletonInterface != NULL );	// Suppress static analysis warning in unreachable code (fatal error)
	}
	return *SingletonInterface;
}

class ITargetPlatformManagerModule* GetTargetPlatformManager()
{
	static class ITargetPlatformManagerModule* SingletonInterface = NULL;
	if (!FPlatformProperties::RequiresCookedData())
	{
		static bool bInitialized = false;
		if (!bInitialized)
		{
			check(IsInGameThread());
			bInitialized = true;
			SingletonInterface = FModuleManager::LoadModulePtr<ITargetPlatformManagerModule>("TargetPlatform");
		}
	}
	return SingletonInterface;
}

class ITargetPlatformManagerModule& GetTargetPlatformManagerRef()
{
	class ITargetPlatformManagerModule* SingletonInterface = GetTargetPlatformManager();
	if (!SingletonInterface)
	{
		UE_LOG(LogInit, Fatal, TEXT("Target platform manager was requested, but not available."));
		CA_ASSUME( SingletonInterface != NULL );	// Suppress static analysis warning in unreachable code (fatal error)
	}
	return *SingletonInterface;
}

//-----------------------------------------------------------------------------


FTicker& FTicker::GetCoreTicker()
{
	static FTicker Singleton;
	return Singleton;
}


/*----------------------------------------------------------------------------
	Runtime functions.
----------------------------------------------------------------------------*/

FQueryIsRunningServer GIsServerDelegate;

bool IsServerForOnlineSubsystems(FName WorldContextHandle)
{
	if (GIsServerDelegate.IsBound())
	{
		return GIsServerDelegate.Execute(WorldContextHandle);
	}
	else
	{
		return IsRunningDedicatedServer();
	}
}

void SetIsServerForOnlineSubsystemsDelegate(FQueryIsRunningServer NewDelegate)
{
	GIsServerDelegate = NewDelegate;
}

#if UE_EDITOR

/** Checks the command line for the presence of switches to indicate running as "dedicated server only" */
int32 CORE_API StaticDedicatedServerCheck()
{
	static int32 HasServerSwitch = -1;
	if (HasServerSwitch == -1)
	{
		const FString CmdLine = FString(FCommandLine::Get()).TrimStart();
		const TCHAR* TCmdLine = *CmdLine;

		TArray<FString> Tokens;
		TArray<FString> Switches;
		FCommandLine::Parse(TCmdLine, Tokens, Switches);

		HasServerSwitch = (Switches.Contains(TEXT("SERVER")) || Switches.Contains(TEXT("RUN=SERVER"))) ? 1 : 0;
	}
	return HasServerSwitch;
}

/** Checks the command line for the presence of switches to indicate running as "game only" */
int32 CORE_API StaticGameCheck()
{
	static int32 HasGameSwitch = -1;
	if (HasGameSwitch == -1)
	{
		const FString CmdLine = FString(FCommandLine::Get()).TrimStart();
		const TCHAR* TCmdLine = *CmdLine;

		TArray<FString> Tokens;
		TArray<FString> Switches;
		FCommandLine::Parse(TCmdLine, Tokens, Switches);

		if (Switches.Contains(TEXT("GAME")))
		{
			HasGameSwitch = 1;
		}
		else
		{
			HasGameSwitch = 0;
		}
	}
	return HasGameSwitch;
}

/** Checks the command line for the presence of switches to indicate running as "client only" */
int32 CORE_API StaticClientOnlyCheck()
{
	static int32 HasClientSwitch = -1;
	if (HasClientSwitch == -1)
	{
		const FString CmdLine = FString(FCommandLine::Get()).TrimStart();
		const TCHAR* TCmdLine = *CmdLine;

		TArray<FString> Tokens;
		TArray<FString> Switches;
		FCommandLine::Parse(TCmdLine, Tokens, Switches);

		HasClientSwitch = (StaticGameCheck() && Switches.Contains(TEXT("ClientOnly"))) ? 1 : 0;
	}
	return HasClientSwitch;
}

#endif //UE_EDITOR

void FUrlConfig::Init()
{
	DefaultProtocol = GConfig->GetStr( TEXT("URL"), TEXT("Protocol"), GEngineIni );
	DefaultName = GConfig->GetStr( TEXT("URL"), TEXT("Name"), GEngineIni );
	// strip off any file extensions from map names
	DefaultHost = GConfig->GetStr( TEXT("URL"), TEXT("Host"), GEngineIni );
	DefaultPortal = GConfig->GetStr( TEXT("URL"), TEXT("Portal"), GEngineIni );
	DefaultSaveExt = GConfig->GetStr( TEXT("URL"), TEXT("SaveExt"), GEngineIni );
	
	FString Port;
	// Allow the command line to override the default port
	if (FParse::Value(FCommandLine::Get(),TEXT("Port="),Port) == false)
	{
		Port = GConfig->GetStr( TEXT("URL"), TEXT("Port"), GEngineIni );
	}
	DefaultPort = FCString::Atoi( *Port );
}

void FUrlConfig::Reset()
{
	DefaultProtocol = TEXT("");
	DefaultName = TEXT("");
	DefaultHost = TEXT("");
	DefaultPortal = TEXT("");
	DefaultSaveExt = TEXT("");
}

bool CORE_API StringHasBadDashes(const TCHAR* Str)
{
	// Detect dashes (0x2013) and report an error if they're found
	while (TCHAR Ch = *Str++)
	{
		if ((UCS2CHAR)Ch == 0x2013)
			return true;
	}

	return false;
}

/*----------------------------------------------------------------------------
FBoolConfigValueHelper
----------------------------------------------------------------------------*/
FBoolConfigValueHelper::FBoolConfigValueHelper(const TCHAR* Section, const TCHAR* Key, const FString& Filename)
	: bValue(false)
{
	GConfig->GetBool(Section, Key, bValue, Filename);
}

/*----------------------------------------------------------------------------
FBlueprintExceptionTracker
----------------------------------------------------------------------------*/
#if DO_BLUEPRINT_GUARD
void FBlueprintExceptionTracker::ResetRunaway()
{
	Runaway = 0;
	Recurse = 0;
	bRanaway = false;
}

FBlueprintExceptionTracker& FBlueprintExceptionTracker::Get()
{
	return TThreadSingleton<FBlueprintExceptionTracker>::Get();
}
#endif // DO_BLUEPRINT_GUARD

bool GIsRetrievingVTablePtr = false;

void EnsureRetrievingVTablePtrDuringCtor(const TCHAR* CtorSignature)
{
	UE_CLOG(!GIsRetrievingVTablePtr, LogCore, Fatal, TEXT("The %s constructor is for internal usage only for hot-reload purposes. Please do NOT use it."), CtorSignature);
}

/*----------------------------------------------------------------------------
Boot timing
----------------------------------------------------------------------------*/

#if !UE_BUILD_SHIPPING

void NotifyLoadingStateChanged(bool bState, const TCHAR *Message)
{
	static bool bEnabled = FParse::Param(FCommandLine::Get(), TEXT("TrackBootLoading"));
	if (!bEnabled)
	{
		return;
	}

	static FCriticalSection Crit;
	FScopeLock Lock(&Crit);
	static double GLastTimeForNotifyAsyncLoadingStateHasMaybeChanged = FPlatformTime::Seconds();

	static double TotalActiveTime = 0.0;
	static double TotalInactiveTime = 0.0;
	static int32 LoadCount = 0;
	static int32 RecursiveCount = 0;

	double Now = FPlatformTime::Seconds();
	double Diff = Now - GLastTimeForNotifyAsyncLoadingStateHasMaybeChanged;

	if (bState)
	{
		RecursiveCount++;
		UE_LOG(LogStreaming, Display, TEXT("Loading Interval Starting %s"), Message);
	}
	else
	{
		RecursiveCount--;
		check(RecursiveCount >= 0);
		UE_LOG(LogStreaming, Display, TEXT("Loading Interval Ending   %s"), Message);
	}

	if (RecursiveCount == 1 && bState)
	{
		TotalInactiveTime += Diff;
	}
	else
	{
		TotalActiveTime += Diff;
	}

	if (!RecursiveCount)
	{
		LoadCount++;
		UE_LOG(LogStreaming, Display, TEXT("Loading Interval  %5d loading time intervals   %7.2fs spent loading    %7.2fs spent not loading"), LoadCount, TotalActiveTime, TotalInactiveTime);
	}
	GLastTimeForNotifyAsyncLoadingStateHasMaybeChanged = Now;
}

#endif


/*----------------------------------------------------------------------------
NAN Diagnostic Failure
----------------------------------------------------------------------------*/

int32 GEnsureOnNANDiagnostic = false;

#if ENABLE_NAN_DIAGNOSTIC
static FAutoConsoleVariableRef CVarGEnsureOnNANDiagnostic(
	TEXT( "EnsureOnNaNFail" ),
	GEnsureOnNANDiagnostic,
	TEXT( "If set to 1 NaN Diagnostic failures will result in ensures being emitted" )
	);
#endif

#if DO_CHECK
namespace UE4Asserts_Private
{
	void VARARGS InternalLogNANDiagnosticMessage(const TCHAR* FormattedMsg, ...)
	{		
		const int32 TempStrSize = 4096;
		TCHAR TempStr[TempStrSize];
		GET_VARARGS(TempStr, TempStrSize, TempStrSize - 1, FormattedMsg, FormattedMsg);
		UE_LOG(LogCore, Error, TempStr);
	}
}
#endif
