// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Toolkits/AssetEditorCommonCommands.h"

#define LOCTEXT_NAMESPACE "AssetEditorCommonCommands"

void FAssetEditorCommonCommands::RegisterCommands()
{
	UI_COMMAND( SaveAsset, "Save", "Saves this asset", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::S) );
	UI_COMMAND( SaveAssetAs, "Save As...", "Saves this asset under a different name", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Alt, EKeys::S) );
	UI_COMMAND( ReimportAsset, "Reimport", "Reimports the asset being edited", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND( SwitchToStandaloneEditor, "Switch to Standalone Editor", "Closes the level-centric asset editor and reopens it in 'standalone' mode", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( SwitchToWorldCentricEditor, "Switch to World-Centric Editor", "Closes the standalone asset editor and reopens it in 'world-centric' mode, docked within the level editor that it was originally opened in.", EUserInterfaceActionType::Button, FInputChord() );
}

#undef LOCTEXT_NAMESPACE
