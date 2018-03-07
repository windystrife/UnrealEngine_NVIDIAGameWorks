// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/OutputDeviceConsole.h"

/**
* Linux implementation of console log window, just a printf to the terminal for now
*/
class APPLICATIONCORE_API FLinuxConsoleOutputDevice : public FOutputDeviceConsole
{

private:

	/** true if the color is currently set by the caller */
	bool bOverrideColorSet;

public:

	/**
	 * Constructor, setting console control handler.
	 */
	FLinuxConsoleOutputDevice();
	~FLinuxConsoleOutputDevice();

	/**
	 * Shows or hides the console window.
	 *
	 * @param ShowWindow	Whether to show (true) or hide (false) the console window.
	 */
	virtual void Show(bool bShowWindow);

	/**
	 * Returns whether console is currently shown or not
	 *
	 * @return true if console is shown, false otherwise
	 */
	virtual bool IsShown();

	virtual bool IsAttached() {return false;}

	/**
	 * Displays text on the console and scrolls if necessary.
	 *
	 * @param Data	Text to display
	 * @param Event	Event type, used for filtering/ suppression
	 */
	void Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category);

};
