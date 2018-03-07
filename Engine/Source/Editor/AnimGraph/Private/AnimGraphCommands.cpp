// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphCommands.h"

#define LOCTEXT_NAMESPACE "AnimGraphCommands"

void FAnimGraphCommands::RegisterCommands()
{
	UI_COMMAND(TogglePoseWatch, "Toggle Pose Watch", "Toggle Pose Watching on this node", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
