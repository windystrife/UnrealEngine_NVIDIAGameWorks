// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FGenericPlatformInstallation
{
	// Validate and normalize an engine root directory name
	static bool NormalizeEngineRootDir(FString &RootDir);

	// Launches the editor application
	static bool LaunchEditor(const FString &RootDirName, const FString &Arguments);

	// Select an engine installation
	static bool SelectEngineInstallation(FString &Identifier);

	// Shows an error dialog with log output
	static void ErrorDialog(const FString &Message, const FString &LogText);
};
