// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	AndroidProperties.h - Basic static properties of a platform 
	These are shared between:
		the runtime platform - via FPlatformProperties
		the target platforms - via ITargetPlatform
==================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformProperties.h"


/**
 * Implements Android platform properties.
 */
struct FAndroidPlatformProperties
	: public FGenericPlatformProperties
{
	static FORCEINLINE const char* GetPhysicsFormat( )
	{
		return "PhysXGeneric";		//@todo android: physx format
	}

	static FORCEINLINE bool HasEditorOnlyData( )
	{
		return false;
	}

	static FORCEINLINE const char* PlatformName()
	{
		return "Android";
	}

	static FORCEINLINE const char* IniPlatformName()
	{
		return "Android";
	}

	static FORCEINLINE bool IsGameOnly( )
	{
		return true;
	}

	static FORCEINLINE bool RequiresCookedData( )
	{
		return true;
	}

	static FORCEINLINE bool SupportsBuildTarget( EBuildTargets::Type BuildTarget )
	{
		return (BuildTarget == EBuildTargets::Game);
	}

	static FORCEINLINE bool SupportsAutoSDK()
	{
		return true;
	}

	static FORCEINLINE bool SupportsHighQualityLightmaps()
	{
#if PLATFORM_ANDROIDESDEFERRED
		return true;
#else
		return false;
#endif
	}

	static FORCEINLINE bool SupportsLowQualityLightmaps()
	{
#if PLATFORM_ANDROIDESDEFERRED
		return false;
#else
		return true;
#endif
	}

	static FORCEINLINE bool SupportsDistanceFieldShadows()
	{
		return true;
	}

	static FORCEINLINE bool SupportsTextureStreaming()
	{
		return false;
	}

	static FORCEINLINE bool SupportsMinimize()
	{
		return true;
	}

	static FORCEINLINE bool SupportsQuit() 
	{
		return true;
	}

	static FORCEINLINE bool HasFixedResolution()
	{
		return true;
	}

	static FORCEINLINE bool AllowsFramerateSmoothing()
	{
		return true;
	}

	static FORCEINLINE bool AllowsCallStackDumpDuringAssert()
	{
		return true;
	}

	static FORCEINLINE bool SupportsAudioStreaming()
	{
		return true;
	}
};

struct FAndroid_PVRTCPlatformProperties : public FAndroidPlatformProperties
{
	static FORCEINLINE const char* PlatformName()
	{
		return "Android_PVRTC";
	}
};

struct FAndroid_ATCPlatformProperties : public FAndroidPlatformProperties
{
	static FORCEINLINE const char* PlatformName()
	{
		return "Android_ATC";
	}
};

struct FAndroid_DXTPlatformProperties : public FAndroidPlatformProperties
{
	static FORCEINLINE const char* PlatformName()
	{
		return "Android_DXT";
	}
};

struct FAndroid_ETC1PlatformProperties : public FAndroidPlatformProperties
{
	static FORCEINLINE const char* PlatformName()
	{
		return "Android_ETC1";
	}
};

struct FAndroid_ETC2PlatformProperties : public FAndroidPlatformProperties
{
	static FORCEINLINE const char* PlatformName()
	{
		return "Android_ETC2";
	}
};

struct FAndroid_ASTCPlatformProperties : public FAndroidPlatformProperties
{
	static FORCEINLINE const char* PlatformName()
	{
		return "Android_ASTC";
	}
};

struct FAndroid_MultiPlatformProperties : public FAndroidPlatformProperties
{
	static FORCEINLINE const char* PlatformName()
	{
		return "Android_Multi";
	}
};
