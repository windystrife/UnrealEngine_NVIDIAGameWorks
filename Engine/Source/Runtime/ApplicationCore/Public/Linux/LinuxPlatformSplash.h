// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	LinuxSplash.h: Linux platform splash screen.
==============================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformSplash.h"

/**
 * Linux splash implementation
 */
struct APPLICATIONCORE_API FLinuxPlatformSplash : public FGenericPlatformSplash
{
	/**
	 * Show the splash screen
	 */
	static void Show();
	/**
	 * Hide the splash screen
	 */
	static void Hide();

	/**
	 * Sets the text displayed on the splash screen (for startup/loading progress)
	 *
	 * @param	InType		Type of text to change
	 * @param	InText		Text to display
	 */
	static void SetSplashText(const SplashTextType::Type InType, const TCHAR* InText);
};

typedef FLinuxPlatformSplash FPlatformSplash;
