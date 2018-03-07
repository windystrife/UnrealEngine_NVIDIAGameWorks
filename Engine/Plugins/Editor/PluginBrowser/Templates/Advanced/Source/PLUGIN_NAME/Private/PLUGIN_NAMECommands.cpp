// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PLUGIN_NAMECommands.h"

#define LOCTEXT_NAMESPACE "FPLUGIN_NAMEModule"

void FPLUGIN_NAMECommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "PLUGIN_NAME", "Bring up PLUGIN_NAME window", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
