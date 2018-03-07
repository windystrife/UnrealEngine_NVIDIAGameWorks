// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	LinuxPlatformProperties.h - Basic static properties of a platform 
	These are shared between:
		the runtime platform - via FPlatformProperties
		the target platforms - via ITargetPlatform
==================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformProperties.h"


/**
 * Implements Linux platform properties.
 */
template<bool HAS_EDITOR_DATA, bool IS_DEDICATED_SERVER, bool IS_CLIENT_ONLY>
struct FLinuxPlatformProperties
	: public FGenericPlatformProperties
{
	static FORCEINLINE bool HasEditorOnlyData( )
	{
		return HAS_EDITOR_DATA;
	}

	static FORCEINLINE const char* IniPlatformName( )
	{
		return "Linux";
	}

	static FORCEINLINE bool IsGameOnly( )
	{
		return UE_GAME;
	}

	static FORCEINLINE bool IsServerOnly( )
	{
		return IS_DEDICATED_SERVER;
	}

	static FORCEINLINE bool IsClientOnly()
	{
		return IS_CLIENT_ONLY;
	}

	static FORCEINLINE const char* PlatformName( )
	{
		if (IS_DEDICATED_SERVER)
		{
			return "LinuxServer";
		}

		if (HAS_EDITOR_DATA)
		{
			return "Linux";
		}

		if (IS_CLIENT_ONLY)
		{
			return "LinuxClient";
		}

		return "LinuxNoEditor";
	}

	static FORCEINLINE bool RequiresCookedData( )
	{
		return !HAS_EDITOR_DATA;
	}

	static FORCEINLINE bool RequiresUserCredentials()
	{
		return true;
	}

	static FORCEINLINE bool SupportsAutoSDK()
	{
// linux cross-compiling / cross-building from windows supports AutoSDK.  But hosted linux doesn't yet.
#if PLATFORM_WINDOWS
		return true;
#else
		return false;
#endif
	}

	static FORCEINLINE bool SupportsMultipleGameInstances( )
	{
		return true;
	}

	static FORCEINLINE bool HasFixedResolution()
	{
		return false;
	}

	static FORCEINLINE bool SupportsTessellation()
	{
		return true;
	}

	static FORCEINLINE bool SupportsWindowedMode()
	{
		return !IS_DEDICATED_SERVER;
	}

	static FORCEINLINE bool AllowsFramerateSmoothing()
	{
		return true;
	}

	static FORCEINLINE bool SupportsQuit()
	{
		return true;
	}

	static FORCEINLINE float GetVariantPriority()
	{
		if (IS_DEDICATED_SERVER)
		{
			return 0.0f;
		}

		if (HAS_EDITOR_DATA)
		{
			return 0.0f;
		}

		if (IS_CLIENT_ONLY)
		{
			return 0.0f;
		}

		return 1.0f;
	}

	static FORCEINLINE bool AllowsCallStackDumpDuringAssert()
	{
		return true;
	}
};
