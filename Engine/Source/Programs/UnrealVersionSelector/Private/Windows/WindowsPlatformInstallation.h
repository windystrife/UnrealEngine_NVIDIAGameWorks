// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../GenericPlatform/GenericPlatformInstallation.h"

struct FWindowsPlatformInstallation : FGenericPlatformInstallation
{
	// Launches the editor application
	static bool LaunchEditor(const FString &RootDirName, const FString &Arguments);

	// Select an engine installation
	static bool SelectEngineInstallation(FString &Identifier);

	// Shows an error dialog with log output
	static void ErrorDialog(const FString &Message, const FString &LogText);
};

