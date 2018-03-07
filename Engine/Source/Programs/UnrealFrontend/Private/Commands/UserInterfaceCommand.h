// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FUserInterfaceCommand
{
public:

	/** Executes the command. */
	static void Run( );

protected:

	/**
	 * Initializes the Slate application.
	 *
	 * @param LayoutInit The path to the layout configuration file.
	 */
	static void InitializeSlateApplication( const FString& LayoutIni );

	/**
	 * Shuts down the Slate application.
	 *
	 * @param LayoutInit The path to the layout configuration file.
	 */
	static void ShutdownSlateApplication( const FString& LayoutIni );
};
