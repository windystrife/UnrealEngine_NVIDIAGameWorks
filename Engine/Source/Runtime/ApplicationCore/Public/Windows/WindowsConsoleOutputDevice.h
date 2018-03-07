// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/OutputDeviceConsole.h"

/**
 * Windows implementation of console log window, utilizing the Win32 console API
 */
class APPLICATIONCORE_API FWindowsConsoleOutputDevice : public FOutputDeviceConsole
{
private:
	/** Handle to the console log window */
	Windows::HANDLE ConsoleHandle;

	/** true if the color is currently set by the caller */
	bool OverrideColorSet;

	/** true if this console device is attached to an existing console window. */
	bool bIsAttached;
	/**
	 * Saves the console window's position and size to the game .ini
	 */
	void SaveToINI();

public:

	/** 
	 * Constructor, setting console control handler.
	 */
	FWindowsConsoleOutputDevice();
	~FWindowsConsoleOutputDevice();

	/**
	 * Shows or hides the console window. 
	 *
	 * @param ShowWindow	Whether to show (true) or hide (false) the console window.
	 */
	virtual void Show( bool ShowWindow );

	/** 
	 * Returns whether console is currently shown or not
	 *
	 * @return true if console is shown, false otherwise
	 */
	virtual bool IsShown();

	virtual bool IsAttached();

	virtual bool CanBeUsedOnAnyThread() const override;

	/**
	 * Displays text on the console and scrolls if necessary.
	 *
	 * @param Data	Text to display
	 * @param Event	Event type, used for filtering/ suppression
	 */
	virtual void Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time ) override;
	virtual void Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category ) override;

	void SetColor( const TCHAR* Color );
};


