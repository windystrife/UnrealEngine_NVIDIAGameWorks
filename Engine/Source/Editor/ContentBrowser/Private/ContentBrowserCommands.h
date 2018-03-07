// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"

class FContentBrowserCommands
	: public TCommands<FContentBrowserCommands>
{
public:

	/** Default constructor. */
	FContentBrowserCommands()
		: TCommands<FContentBrowserCommands>(TEXT("ContentBrowser"), NSLOCTEXT( "ContentBrowser", "ContentBrowser", "Content Browser" ), NAME_None, FEditorStyle::GetStyleSetName() )
	{ }

public:

	//~ TCommands interface

	virtual void RegisterCommands() override;

public:

	TSharedPtr<FUICommandInfo> CreateNewFolder;
	TSharedPtr<FUICommandInfo> DirectoryUp;
	TSharedPtr<FUICommandInfo> OpenAssetsOrFolders;
	TSharedPtr<FUICommandInfo> PreviewAssets;
	TSharedPtr<FUICommandInfo> SaveSelectedAsset;
	TSharedPtr<FUICommandInfo> SaveAllCurrentFolder;
	TSharedPtr<FUICommandInfo> ResaveAllCurrentFolder;
};
