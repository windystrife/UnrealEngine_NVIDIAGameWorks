// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	AndroidProcess.h: Android platform Process functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformProcess.h"

/** Dummy process handle for platforms that use generic implementation. */
struct FProcHandle : public TProcHandle<void*, nullptr>
{
public:
	/** Default constructor. */
	FORCEINLINE FProcHandle()
		: TProcHandle()
	{}

	/** Initialization constructor. */
	FORCEINLINE explicit FProcHandle( HandleType Other )
		: TProcHandle( Other )
	{}
};

/**
 * Android implementation of the Process OS functions
 **/
struct CORE_API FAndroidPlatformProcess : public FGenericPlatformProcess
{
	static const TCHAR* ComputerName();
	static void SetThreadAffinityMask( uint64 AffinityMask );
	static uint32 GetCurrentProcessId();
	static const TCHAR* BaseDir();
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);
	static class FRunnableThread* CreateRunnableThread();
	static void LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error);
	static FString GetGameBundleId();
};

typedef FAndroidPlatformProcess FPlatformProcess;

