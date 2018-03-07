// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "WidgetBlueprintEditor.h"
#include "BlueprintEditorModes.h"

class UWidgetBlueprint;

/////////////////////////////////////////////////////
// FWidgetBlueprintApplicationMode

class FWidgetBlueprintApplicationMode : public FBlueprintEditorApplicationMode
{
public:
	FWidgetBlueprintApplicationMode(TSharedPtr<class FWidgetBlueprintEditor> InWidgetEditor, FName InModeName);

	// FApplicationMode interface
	// End of FApplicationMode interface

protected:
	UWidgetBlueprint* GetBlueprint() const;
	
	TSharedPtr<class FWidgetBlueprintEditor> GetBlueprintEditor() const;

	TWeakPtr<class FWidgetBlueprintEditor> MyWidgetBlueprintEditor;

	// Set of spawnable tabs in the mode
	FWorkflowAllowedTabSet TabFactories;
};
