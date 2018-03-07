// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PoseEditorCommands.h"

#define LOCTEXT_NAMESPACE "PoseEditorCommands"

void FPoseEditorCommands::RegisterCommands()
{
	UI_COMMAND(PasteAllNames, "Paste All Pose Names", "Paste all pose names from clipboard", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
