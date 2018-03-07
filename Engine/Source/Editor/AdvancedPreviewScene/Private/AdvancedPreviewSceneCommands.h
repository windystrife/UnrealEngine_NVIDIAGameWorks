// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

class FAdvancedPreviewSceneCommands : public TCommands<FAdvancedPreviewSceneCommands>
{

public:
	FAdvancedPreviewSceneCommands() : TCommands<FAdvancedPreviewSceneCommands>
	(
		"AdvancedPreviewScene",
		NSLOCTEXT("Contexts", "AdvancedPreviewScene", "Advanced Preview Scene"),
		NAME_None,
		FEditorStyle::Get().GetStyleSetName()
	)
	{}
	
	/** Toggles sky sphere visibility */
	TSharedPtr< FUICommandInfo > ToggleSky;

	/** Toggles floor visibility */
	TSharedPtr< FUICommandInfo > ToggleFloor;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;
};
