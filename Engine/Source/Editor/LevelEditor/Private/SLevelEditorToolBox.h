// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateIcon.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Editor.h"
#include "ILevelEditor.h"
#include "Misc/NotifyHook.h"

class FExtender;
class SBorder;

/**
 * Tools for the level editor                   
 */
class SLevelEditorToolBox : public SCompoundWidget, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS( SLevelEditorToolBox ){}
	SLATE_END_ARGS()

	~SLevelEditorToolBox();

	void Construct( const FArguments& InArgs, const TSharedRef< class ILevelEditor >& OwningLevelEditor );

	/** Called by SLevelEditor to notify the toolbox about a new toolkit being hosted */
	void OnToolkitHostingStarted( const TSharedRef< class IToolkit >& Toolkit );

	/** Called by SLevelEditor to notify the toolbox about an existing toolkit no longer being hosted */
	void OnToolkitHostingFinished(const TSharedRef< class IToolkit >& Toolkit);

	/** Handles updating the mode toolbar when the registered mode commands change */
	void OnEditorModeCommandsChanged();

private:

	/** Gets the visibility for the SBorder showing toolbox editor mode inline content */
	EVisibility GetInlineContentHolderVisibility() const;

	/** Gets the visibility for the message suggesting the user select a tool */
	EVisibility GetNoToolSelectedTextVisibility() const;

	/** Updates the widget for showing toolbox editor mode inline content */
	void UpdateInlineContent(TSharedPtr<SWidget> InlineContent) const;

	/** Creates and sets the mode toolbar */
	void UpdateModeToolBar();

	/** Handles updating the mode toolbar when the user settings change */
	void HandleUserSettingsChange( FName PropertyName );

	/** Returns specified Editor modes icon, if that mode is active it adds ".Selected" onto the name */
	FSlateIcon GetEditorModeIcon(TSharedPtr< FUICommandInfo > EditorModeUICommand, FEditorModeID EditorMode);

private:

	/** Level editor that we're associated with */
	TWeakPtr<class ILevelEditor> LevelEditor;

	/** Inline content area for editor modes */
	TSharedPtr<SBorder> InlineContentHolder;

	/** The menu extenders to populate the toolbox*/
	TArray< TSharedPtr<FExtender> > ToolboxExtenders;

	/** The container holding the mode toolbar */
	TSharedPtr< SBorder > ModeToolBarContainer;
};
