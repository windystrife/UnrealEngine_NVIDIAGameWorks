// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PLUGIN_NAMECommands.h"

#define LOCTEXT_NAMESPACE "FPLUGIN_NAMEModule"

void FPLUGIN_NAMECommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "PLUGIN_NAME", "Execute PLUGIN_NAME action", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
