// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/ContainersFwd.h"

/**
 * SplashTextType defines the types of text on the splash screen
 */
namespace SplashTextType
{
	enum Type
	{
		/** Startup progress text */
		StartupProgress	= 0,

		/** Version information text line 1 */
		VersionInfo1,

		/** Copyright information text */
		CopyrightInfo,

		/** Game Name */
		GameName,

		// ...

		/** Number of text types (must be final enum value) */
		NumTextTypes
	};
}

class FString;

/**
 * Generic implementation for most platforms
 */
struct APPLICATIONCORE_API FGenericPlatformSplash
{
	/** Show the splash screen. */
	FORCEINLINE static void Show() { }

	/** Hide the splash screen. */
	FORCEINLINE static void Hide() { }

	/**
	 * Sets the text displayed on the splash screen (for startup/loading progress)
	 *
	 * @param	InType		Type of text to change
	 * @param	InText		Text to display
	 */
	FORCEINLINE static void SetSplashText( const SplashTextType::Type InType, const TCHAR* InText )
	{

	}

	/**
	 * Return whether the splash screen is being shown or not
	 */
	FORCEINLINE static bool IsShown()
	{
		return true;
	}

protected:
	/**
	* Finds a usable splash pathname for the given filename
	*
	* @param SplashFilename Name of the desired splash name("Splash")
	* @param IconFilename Name of the desired icon name("Splash")
	* @param OutPath String containing the path to the file, if this function returns true
	* @param OutIconPath String containing the path to the icon, if this function returns true
	*
	* @return true if a splash screen was found
	*/
	static bool GetSplashPath(const TCHAR* SplashFilename, FString& OutPath, bool& OutIsCustom);
	static bool GetSplashPath(const TCHAR* SplashFilename, const TCHAR* IconFilename, FString& OutPath, FString& OutIconPath, bool& OutIsCustom);
};
