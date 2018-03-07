// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformSplash.h"


/**
 * Windows splash implementation.
 */
struct APPLICATIONCORE_API FWindowsPlatformSplash
	: public FGenericPlatformSplash
{
	/** Show the splash screen. */
	static void Show();

	/** Hide the splash screen. */
	static void Hide();

	/**
	 * Sets the text displayed on the splash screen (for startup/loading progress).
	 *
	 * @param InType Type of text to change.
	 * @param InText Text to display.
	 */
	static void SetSplashText( const SplashTextType::Type InType, const TCHAR* InText );

	/**
	 * Return whether the splash screen is being shown or not
	 */
	static bool IsShown();
};


typedef FWindowsPlatformSplash FPlatformSplash;
