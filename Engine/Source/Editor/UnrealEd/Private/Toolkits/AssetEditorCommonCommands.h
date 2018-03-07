// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

/** Asset editor common commands */
class FAssetEditorCommonCommands : public TCommands< FAssetEditorCommonCommands >
{

public:

	FAssetEditorCommonCommands()
		: TCommands< FAssetEditorCommonCommands >( TEXT("AssetEditor"), NSLOCTEXT("Contexts", "AssetEditor", "Asset Editor"), TEXT("EditorViewport"), FEditorStyle::GetStyleSetName() )
	{
	}	

	virtual void RegisterCommands() override;

	TSharedPtr< FUICommandInfo > SaveAsset;
	TSharedPtr< FUICommandInfo > SaveAssetAs;
	TSharedPtr< FUICommandInfo > ReimportAsset;
	TSharedPtr< FUICommandInfo > SwitchToStandaloneEditor;
	TSharedPtr< FUICommandInfo > SwitchToWorldCentricEditor;
};

