// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

/**
 * Class containing commands for viewport menu actions
 */
class FAnimViewportMenuCommands : public TCommands<FAnimViewportMenuCommands>
{
public:
	FAnimViewportMenuCommands() 
		: TCommands<FAnimViewportMenuCommands>
		(
			TEXT("AnimViewportMenu"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "AnimViewportMenu", "Animation Viewport Menu"), // Localized context name for displaying
			NAME_None, // Parent context name.  
			FEditorStyle::GetStyleSetName() // Icon Style Set
		)
	{
	}

	/** Open settings for the preview scene */
	TSharedPtr< FUICommandInfo > PreviewSceneSettings;
	
	/** Select camera follow */
	TSharedPtr< FUICommandInfo > CameraFollow;

	/** Show vertex normals */
	TSharedPtr< FUICommandInfo > SetCPUSkinning;

	/** Show vertex normals */
	TSharedPtr< FUICommandInfo > SetShowNormals;

	/** Show vertex tangents */
	TSharedPtr< FUICommandInfo > SetShowTangents;

	/** Show vertex binormals */
	TSharedPtr< FUICommandInfo > SetShowBinormals;

	/** Draw UV mapping to viewport */
	TSharedPtr< FUICommandInfo > AnimSetDrawUVs;

	/** Save current camera as default */
	TSharedPtr< FUICommandInfo > SaveCameraAsDefault;

	/** Clear default camera */
	TSharedPtr< FUICommandInfo > ClearDefaultCamera;

	/** Jump to default camera */
	TSharedPtr< FUICommandInfo > JumpToDefaultCamera;

public:
	/** Registers our commands with the binding system */
	virtual void RegisterCommands() override;
};
