// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformMisc.h"

/** 
 * FMessageDialog
 * These functions open a message dialog and display the specified informations
 * these.
 **/
struct CORE_API FMessageDialog
{
	/** Pops up a message dialog box containing the input string.
	 * @param Message Text of message to show
	 * @param OptTitle Optional title to use (defaults to "Message")
	*/
	static void Debugf( const FText& Message, const FText* OptTitle = nullptr );

	/** Pops up a message dialog box containing the last system error code in string form. */
	static void ShowLastError();

	/**
	 * Open a modal message box dialog
	 * @param MessageType Controls buttons dialog should have
	 * @param Message Text of message to show
	 * @param OptTitle Optional title to use (defaults to "Message")
	*/
	static EAppReturnType::Type Open( EAppMsgType::Type MessageType, const FText& Message, const FText* OptTitle = nullptr );
};
