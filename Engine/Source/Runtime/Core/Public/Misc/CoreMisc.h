// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Exec.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "Math/IntPoint.h"
#include "UObject/NameTypes.h"
#include "CoreGlobals.h"
#include "HAL/ThreadSingleton.h"

/**
 * Exec handler that registers itself and is being routed via StaticExec.
 * Note: Not intended for use with UObjects!
 */
class CORE_API FSelfRegisteringExec : public FExec
{
public:
	/** Constructor, registering this instance. */
	FSelfRegisteringExec();
	/** Destructor, unregistering this instance. */
	virtual ~FSelfRegisteringExec();

	/** Routes a command to the self-registered execs. */
	static bool StaticExec( UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar );

private:
	
	/** Array of registered exec's routed via StaticExec. */
	static TArray<FSelfRegisteringExec*>& GetRegisteredExecs();
};

/** Registers a static Exec function using FSelfRegisteringExec. */
class CORE_API FStaticSelfRegisteringExec : public FSelfRegisteringExec
{
public:

	/** Initialization constructor. */
	FStaticSelfRegisteringExec(bool (*InStaticExecFunc)(UWorld* Inworld, const TCHAR* Cmd,FOutputDevice& Ar));

	//~ Begin Exec Interface
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar );
	//~ End Exec Interface

private:

	bool (*StaticExecFunc)(UWorld* Inworld, const TCHAR* Cmd,FOutputDevice& Ar);
};

// Interface for returning a context string.
class FContextSupplier
{
public:
	virtual ~FContextSupplier() {}
	virtual FString GetContext()=0;
};


struct CORE_API FMaintenance
{
	/** deletes log files older than a number of days specified in the Engine ini file */
	static void DeleteOldLogs();
};

/*-----------------------------------------------------------------------------
	Module singletons.
-----------------------------------------------------------------------------*/

/** Return the DDC interface, if it is available, otherwise return NULL **/
CORE_API class FDerivedDataCacheInterface* GetDerivedDataCache();

/** Return the DDC interface, fatal error if it is not available. **/
CORE_API class FDerivedDataCacheInterface& GetDerivedDataCacheRef();

/** Return the Target Platform Manager interface, if it is available, otherwise return NULL **/
CORE_API class ITargetPlatformManagerModule* GetTargetPlatformManager();

/** Return the Target Platform Manager interface, fatal error if it is not available. **/
CORE_API class ITargetPlatformManagerModule& GetTargetPlatformManagerRef();

/*-----------------------------------------------------------------------------
	Runtime.
-----------------------------------------------------------------------------*/

/**
 * Check to see if this executable is running as dedicated server
 * Editor can run as dedicated with -server
 */
FORCEINLINE bool IsRunningDedicatedServer()
{
	if (FPlatformProperties::IsServerOnly())
	{
		return true;
	}

	if (FPlatformProperties::IsGameOnly())
	{
		return false;
	}

#if UE_EDITOR
	extern CORE_API int32 StaticDedicatedServerCheck();
	return (StaticDedicatedServerCheck() == 1);
#else
	return false;
#endif
}

/**
 * Check to see if this executable is running as "the game"
 * - contains all net code (WITH_SERVER_CODE=1)
 * Editor can run as a game with -game
 */
FORCEINLINE bool IsRunningGame()
{
	if (FPlatformProperties::IsGameOnly())
	{
		return true;
	}

	if (FPlatformProperties::IsServerOnly())
	{
		return false;
	}

#if UE_EDITOR
	extern CORE_API int32 StaticGameCheck();
	return (StaticGameCheck() == 1);
#else
	return false;
#endif
}

/**
 * Check to see if this executable is running as "the client"
 * - removes all net code (WITH_SERVER_CODE=0)
 * Editor can run as a game with -clientonly
 */
FORCEINLINE bool IsRunningClientOnly()
{
	if (FPlatformProperties::IsClientOnly())
	{
		return true;
	}

#if UE_EDITOR
	extern CORE_API int32 StaticClientOnlyCheck();
	return (StaticClientOnlyCheck() == 1);
#else
	return false;
#endif
}

/**
 * Helper for obtaining the default Url configuration
 */
struct CORE_API FUrlConfig
{
	FString DefaultProtocol;
	FString DefaultName;
	FString DefaultHost;
	FString DefaultPortal;
	FString DefaultSaveExt;
	int32 DefaultPort;

	/**
	 * Initialize with defaults from ini
	 */
	void Init();

	/**
	 * Reset state
	 */
	void Reset();
};

bool CORE_API StringHasBadDashes(const TCHAR* Str);

/**
 * Helper for script stack traces
 */
struct FScriptTraceStackNode
{
	FName	Scope;
	FName	FunctionName;

	FScriptTraceStackNode(FName InScope, FName InFunctionName)
		: Scope(InScope)
		, FunctionName(InFunctionName)
	{
	}

	FString GetStackDescription() const
	{
		return Scope.ToString() + TEXT(".") + FunctionName.ToString();
	}

private:
	FScriptTraceStackNode() {};
};

/** Helper structure for boolean values in config */
struct CORE_API FBoolConfigValueHelper
{
private:
	bool bValue;
public:
	FBoolConfigValueHelper(const TCHAR* Section, const TCHAR* Key, const FString& Filename = GEditorIni);

	operator bool() const
	{
		return bValue;
	}
};

#ifndef DO_BLUEPRINT_GUARD
	#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		#define DO_BLUEPRINT_GUARD 1
	#else
		#define DO_BLUEPRINT_GUARD 0
	#endif
#endif

#if DO_BLUEPRINT_GUARD
/** 
 * Helper struct for dealing with Blueprint exceptions 
 */
struct CORE_API FBlueprintExceptionTracker : TThreadSingleton<FBlueprintExceptionTracker>
{
	FBlueprintExceptionTracker()
		: Runaway(0)
		, Recurse(0)
		, bRanaway(false)
		, ScriptEntryTag(0)
	{}

	void ResetRunaway();

	static FBlueprintExceptionTracker& Get();
public:
	// map of currently displayed warnings in exception handler
	TMap<FName, int32> DisplayedWarningsMap;

	// runaway tracking
	int32 Runaway;
	int32 Recurse;
	bool bRanaway;

	// Script entry point tracking
	int32 ScriptEntryTag;

	// Stack names from the VM to be unrolled when we assert
	TArray<FScriptTraceStackNode> ScriptStack;
};

#endif // DO_BLUEPRINT_GUARD
