// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	IOSPlatformProperties.h - Basic static properties of a platform 
	These are shared between:
		the runtime platform - via FPlatformProperties
		the target platforms - via ITargetPlatform
==================================================================================*/

#pragma once

#include "GenericPlatformProperties.h"


/**
 * Implements iOS platform properties.
 */
struct FIOSPlatformProperties
	: public FGenericPlatformProperties
{
	static FORCEINLINE bool HasEditorOnlyData( )
	{
		return false;
	}

	static FORCEINLINE const char* PlatformName( )
	{
		return "IOS";
	}

	static FORCEINLINE const char* IniPlatformName( )
	{
		return "IOS";
	}

	static FORCEINLINE bool IsGameOnly()
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

	static FORCEINLINE bool SupportsLowQualityLightmaps()
	{
		return true;
	}

	static FORCEINLINE bool SupportsHighQualityLightmaps()
	{
		return true;
	}

	static FORCEINLINE bool SupportsTextureStreaming()
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

	static FORCEINLINE bool SupportsAudioStreaming()
	{
		return true;
	}
};
