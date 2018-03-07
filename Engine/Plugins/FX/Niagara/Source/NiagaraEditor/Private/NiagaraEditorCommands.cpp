// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorCommands.h"

#include "Commands.h"

#define LOCTEXT_NAMESPACE "NiagaraEditorCommands"

void FNiagaraEditorCommands::RegisterCommands()
{
	UI_COMMAND(Apply, "Apply", "Push the currently compiled script to the world.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(Compile, "Compile", "Compile the current scripts", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RefreshNodes, "Refresh", "Refreshes the current graph nodes, and updates pins due to external changes.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ResetSimulation, "Reset", "Resets the current simulation", EUserInterfaceActionType::Button, FInputChord(EKeys::SpaceBar));
	UI_COMMAND(TogglePreviewGrid, "Grid", "Toggles the preview pane's grid.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND(TogglePreviewBackground, "Background", "Toggles the preview pane's background.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleUnlockToChanges, "Lock/Unlock To Changes", "Toggles whether or not changes in the source asset get pulled into this asset automatically.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE // NiagaraEditorCommands
