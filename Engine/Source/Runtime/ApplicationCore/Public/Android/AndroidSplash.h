// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	AndroidSplash.h: Android platform splash screen...
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformSplash.h"

/**
 * Android splash implementation
 */
struct APPLICATIONCORE_API FAndroidSplash : public FGenericPlatformSplash
{
	// default implementation for now
};


typedef FAndroidSplash FPlatformSplash;
