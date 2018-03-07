// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands.h"
#include "UICommandInfo.h"
#include "EditorStyleSet.h"

class FClothingAssetListCommands : public TCommands<FClothingAssetListCommands>
{
public:
	FClothingAssetListCommands()
		: TCommands<FClothingAssetListCommands>(
			TEXT("ClothAssetList"), 
			NSLOCTEXT("Contexts", "ClothAssetList", "Clothing Asset List"), 
			NAME_None, 
			FEditorStyle::GetStyleSetName())
	{}

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> DeleteAsset;

	TSharedPtr<FUICommandInfo> ReimportAsset;

	TSharedPtr<FUICommandInfo> RebuildAssetParams;

};