// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ClothingAssetListCommands.h"

#define LOCTEXT_NAMESPACE "ClothingAssetListCommands"

void FClothingAssetListCommands::RegisterCommands()
{
	UI_COMMAND(DeleteAsset, "Delete Asset", "Deletes a clothing asset from the mesh.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReimportAsset, "Reimport Asset", "If an asset was originally imported from an external file, reimport from that file.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RebuildAssetParams, "Rebuild Asset Parameter Masks", "Takes the parameter masks in LOD0 and creates masks in all lower LODs to match, casting those parameter masks to the new mesh.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
