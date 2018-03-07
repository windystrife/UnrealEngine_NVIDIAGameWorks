// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimViewportMenuCommands.h"

#define LOCTEXT_NAMESPACE "AnimViewportMenuCommands"

void FAnimViewportMenuCommands::RegisterCommands()
{
	UI_COMMAND( PreviewSceneSettings, "Preview Scene Settings...", "The Advanced Preview Settings tab will let you alter the preview scene's settings.", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND( CameraFollow, "Camera Follow", "Follow the bound of the mesh ", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND( SetCPUSkinning, "CPU Skinning", "Toggles display of CPU skinning in the Preview Pane.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND( SetShowNormals, "Normals", "Toggles display of vertex normals in the Preview Pane.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( SetShowTangents, "Tangents", "Toggles display of vertex tangents in the Preview Pane.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( SetShowBinormals, "Binormals", "Toggles display of vertex binormals (orthogonal vector to normal and tangent) in the Preview Pane.", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND( AnimSetDrawUVs, "UV", "Toggles display of the mesh's UVs for the specified channel.", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND(SaveCameraAsDefault, "Save Camera As Default", "Save the current camera as default for this mesh.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ClearDefaultCamera, "Clear Default Camera", "Clear default camera for this mesh.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(JumpToDefaultCamera, "Jump To Default Camera", "Jump to the default camera (if set).", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::F));
}

#undef LOCTEXT_NAMESPACE
