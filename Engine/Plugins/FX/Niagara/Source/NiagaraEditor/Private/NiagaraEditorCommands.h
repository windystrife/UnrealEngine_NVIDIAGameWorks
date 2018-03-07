// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Commands.h"
#include "EditorStyleSet.h"
/**
* Defines commands for the niagara editor.
*/
class FNiagaraEditorCommands : public TCommands<FNiagaraEditorCommands>
{
public:
	FNiagaraEditorCommands()
		: TCommands<FNiagaraEditorCommands>
		(
			TEXT("NiagaraEditor"),
			NSLOCTEXT("Contexts", "NiagaraEditor", "Niagara Editor"),
			NAME_None,
			FEditorStyle::GetStyleSetName()
			)
	{
	}

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> Apply;
	TSharedPtr<FUICommandInfo> Compile;
	TSharedPtr<FUICommandInfo> RefreshNodes;
	TSharedPtr<FUICommandInfo> ResetSimulation;
	
	/** Toggles the preview pane's grid */
	TSharedPtr< FUICommandInfo > TogglePreviewGrid;
	
	/** Toggles the preview pane's background */
	TSharedPtr< FUICommandInfo > TogglePreviewBackground;

	/** Toggles the locking/unlocking of refreshing from changes*/
	TSharedPtr<FUICommandInfo> ToggleUnlockToChanges;
};