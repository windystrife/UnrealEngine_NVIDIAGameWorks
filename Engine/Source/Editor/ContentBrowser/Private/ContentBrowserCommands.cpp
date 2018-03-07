// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserCommands.h"


#define LOCTEXT_NAMESPACE "ContentBrowser"


void FContentBrowserCommands::RegisterCommands()
{
	UI_COMMAND(OpenAssetsOrFolders, "Open Assets or Folders", "Opens the selected assets or folders, depending on the selection", EUserInterfaceActionType::Button, FInputChord(EKeys::Enter));
	UI_COMMAND(PreviewAssets, "Preview Assets", "Loads the selected assets and previews them if possible", EUserInterfaceActionType::Button, FInputChord(EKeys::SpaceBar));
	UI_COMMAND(DirectoryUp, "Up", "Up to parent directory", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::Up));
	UI_COMMAND(CreateNewFolder, "Create New Folder", "Creates new folder in selected path", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::N));
	UI_COMMAND(SaveSelectedAsset, "Save Selected Asset", "Save the selected asset", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::S));
	UI_COMMAND(SaveAllCurrentFolder, "Save All", "Save All in current folder", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::S));
	UI_COMMAND(ResaveAllCurrentFolder, "Resave All", "Resave all assets contained in the current folder", EUserInterfaceActionType::Button, FInputChord());
}


#undef LOCTEXT_NAMESPACE
