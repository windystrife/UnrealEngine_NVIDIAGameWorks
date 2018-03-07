// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AdvancedPreviewSceneCommands.h"

#define LOCTEXT_NAMESPACE "AdvancedPreviewSceneCommands"

void FAdvancedPreviewSceneCommands::RegisterCommands()
{
	UI_COMMAND(ToggleSky, "Toggle Sky", "Toggles sky sphere visibility", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::I));
	UI_COMMAND(ToggleFloor, "Toggle Floor", "Toggles floor visibility", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::O));
}

#undef LOCTEXT_NAMESPACE
