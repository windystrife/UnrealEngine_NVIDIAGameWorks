// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

/**
 * Class containing commands for persona viewport LOD actions
 */
class FAnimViewportLODCommands : public TCommands<FAnimViewportLODCommands>
{
public:
	FAnimViewportLODCommands() 
		: TCommands<FAnimViewportLODCommands>
		(
			TEXT("AnimViewportLODCmd"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "AnimViewportLODCmd", "Animation Viewport LOD Command"), // Localized context name for displaying
			NAME_None, // Parent context name. 
			FEditorStyle::GetStyleSetName() // Icon Style Set
		)
	{
	}

	/** LOD Auto */
	TSharedPtr< FUICommandInfo > LODAuto;

	/** LOD 0 */
	TSharedPtr< FUICommandInfo > LOD0;

public:
	/** Registers our commands with the binding system */
	virtual void RegisterCommands() override;
};
