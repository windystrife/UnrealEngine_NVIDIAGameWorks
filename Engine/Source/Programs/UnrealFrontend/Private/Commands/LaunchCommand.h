// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FLaunchCommand
{
public:

	/**
	 * Executes the command.
	 *
	 * @return true on success, false otherwise.
	 */
	static bool Run( const FString& Params );
};
