// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	MacPlatformProperties.h - Basic static properties of a platform 
	These are shared between:
		the runtime platform - via FPlatformProperties
		the target platforms - via ITargetPlatform
==================================================================================*/

#pragma once

#include "GenericPlatformProperties.h"


/**
 * Implements Mac platform properties.
 */
template<bool HAS_EDITOR_DATA, bool IS_DEDICATED_SERVER, bool IS_CLIENT_ONLY>
struct FMacPlatformProperties
	: public FGenericPlatformProperties
{
	static FORCEINLINE bool HasEditorOnlyData( )
	{
		return HAS_EDITOR_DATA;
	}

	static FORCEINLINE const char* IniPlatformName( )
	{
		return "Mac";
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
			return "MacServer";
		}
		
		if (HAS_EDITOR_DATA)
		{
			return "Mac";
		}

		if (IS_CLIENT_ONLY)
		{
			return "MacClient";
		}

		return "MacNoEditor";
	}

	static FORCEINLINE bool RequiresCookedData( )
	{
		return !HAS_EDITOR_DATA;
	}

	static FORCEINLINE bool SupportsMultipleGameInstances( )
	{
		return false;
	}
	static FORCEINLINE bool SupportsWindowedMode( )
	{
		return true;
	}

	static FORCEINLINE bool AllowsFramerateSmoothing()
	{
		return true;
	}

	static FORCEINLINE bool HasFixedResolution()
	{
		return false;
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
	
	static FORCEINLINE bool SupportsTessellation()
	{
		return true;
	}

	static FORCEINLINE bool SupportsAudioStreaming()
	{
		return !IsServerOnly();
	}
};
