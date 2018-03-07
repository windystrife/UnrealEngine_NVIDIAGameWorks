// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SkeletalMeshEditorCommands.h"

#define LOCTEXT_NAMESPACE "SkeletalMeshEditorCommands"

void FSkeletalMeshEditorCommands::RegisterCommands()
{
	UI_COMMAND(ReimportMesh, "Reimport Mesh", "Reimport the current mesh.", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(MeshSectionSelection, "Section Selection", "Enables selecting Mesh Sections in the viewport (disables selecting bones using their physics shape).", EUserInterfaceActionType::ToggleButton, FInputChord());
}

#undef LOCTEXT_NAMESPACE
