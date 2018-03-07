// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "EditorModes.h"
#include "Toolkits/BaseToolkit.h"
#include "EditorModeManager.h"
#include "Editor/PlacementMode/Private/SPlacementModeTools.h"

class FPlacementModeToolkit : public FModeToolkit
{
public:

	FPlacementModeToolkit()
	{
		SAssignNew( PlacementModeTools, SPlacementModeTools );
	}

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override { return FName("PlacementMode"); }
	virtual FText GetBaseToolkitName() const override { return NSLOCTEXT("BuilderModeToolkit", "DisplayName", "Builder"); }
	virtual class FEdMode* GetEditorMode() const override { return GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_Placement ); }
	virtual TSharedPtr<class SWidget> GetInlineContent() const override { return PlacementModeTools; }

private:

	TSharedPtr<SPlacementModeTools> PlacementModeTools;
};
