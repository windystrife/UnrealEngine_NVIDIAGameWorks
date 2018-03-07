// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigCommands.h"

#define LOCTEXT_NAMESPACE "ControlRigCommands"

void FControlRigCommands::RegisterCommands()
{
	UI_COMMAND(SetKey, "Set Key", "Sets a key on the selected nodes", EUserInterfaceActionType::Button, FInputChord(EKeys::Enter));
	UI_COMMAND(ToggleManipulators, "Toggle Manipulators", "Toggles visibility of manipulators in the viewport", EUserInterfaceActionType::Button, FInputChord(EKeys::R));
	UI_COMMAND(ToggleTrajectories, "Toggle Trajectories", "Toggles visibility of trajectories in the viewport", EUserInterfaceActionType::Button, FInputChord(EKeys::T));
	UI_COMMAND(ExportAnimSequence, "Export To Animation", "Export this sequence to an optimized skeleton-specific animation sequence", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReExportAnimSequence, "Re-export To Animation", "Re-export this sequence to an animation sequence using the previous export settings", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ImportFromRigSequence, "Import From Rig Sequence", "Import this animation sequence from a source rig sequence", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReImportFromRigSequence, "Re-import From Rig Sequence", "Re-import this animation sequence from it's source rig sequence", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
