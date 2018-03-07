// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "EditorModes.h"
#include "Toolkits/BaseToolkit.h"
#include "EditorModeManager.h"
#include "SControlRigEditModeTools.h"

class FControlRigEditModeToolkit : public FModeToolkit
{
public:

	FControlRigEditModeToolkit()
	{
		SAssignNew(ModeTools, SControlRigEditModeTools);
	}

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override { return FName("AnimationMode"); }
	virtual FText GetBaseToolkitName() const override { return NSLOCTEXT("AnimationModeToolkit", "DisplayName", "Animation"); }
	virtual class FEdMode* GetEditorMode() const override { return GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName); }
	virtual TSharedPtr<class SWidget> GetInlineContent() const override { return ModeTools; }

private:

	TSharedPtr<SControlRigEditModeTools> ModeTools;
};
