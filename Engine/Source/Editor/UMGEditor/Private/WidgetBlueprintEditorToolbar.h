// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FExtender;
class FToolBarBuilder;
class FWidgetBlueprintEditor;

/**
 * Handles all of the toolbar related construction for the widget blueprint editor.
 */
class FWidgetBlueprintEditorToolbar : public TSharedFromThis<FWidgetBlueprintEditorToolbar>
{

public:
	/** Constructor */
	FWidgetBlueprintEditorToolbar(TSharedPtr<FWidgetBlueprintEditor>& InWidgetEditor);
	
	/**
	 * Builds the modes toolbar for the widget blueprint editor.
	 */
	void AddWidgetBlueprintEditorModesToolbar(TSharedPtr<FExtender> Extender);

	void AddWidgetReflector(TSharedPtr<FExtender> Extender);

public:
	/**  */
	void FillWidgetBlueprintEditorModesToolbar(FToolBarBuilder& ToolbarBuilder);

	/**  */
	void FillWidgetReflectorToolbar(FToolBarBuilder& ToolbarBuilder);

	TWeakPtr<FWidgetBlueprintEditor> WidgetEditor;
};
