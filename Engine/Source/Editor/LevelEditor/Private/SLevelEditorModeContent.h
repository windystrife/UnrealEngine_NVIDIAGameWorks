// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "ILevelEditor.h"
#include "Misc/NotifyHook.h"

class FEdMode;
class SBorder;

/**
 * Tools for the level editor                   
 */
class SLevelEditorModeContent : public SCompoundWidget, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS( SLevelEditorModeContent ){}
	SLATE_END_ARGS()

	~SLevelEditorModeContent();

	void Construct(
		const FArguments& InArgs, 
		const TSharedRef< class ILevelEditor >& OwningLevelEditor,
		const TSharedRef< SDockTab >& InOwningDocTab,
		FEdMode* EditorMode );

	/** Called by SLevelEditor to notify the toolbox about a new toolkit being hosted */
	void OnToolkitHostingStarted( const TSharedRef< class IToolkit >& Toolkit );

	/** Called by SLevelEditor to notify the toolbox about an existing toolkit no longer being hosted */
	void OnToolkitHostingFinished( const TSharedRef< class IToolkit >& Toolkit );

private:

	/**
	 * Handles being notified when any editor mode changes to see if this tab needs to close.
	 * @param Mode The mode that changed.
	 * @param IsEnabled true if the mode is enabled, otherwise false.
	 */
	void HandleEditorModeChanged(FEdMode* Mode, bool IsEnabled);

	/** Gets the visibility for the SBorder showing toolbox editor mode inline content */
	EVisibility GetInlineContentHolderVisibility() const;

	/** Updates the widget for showing toolbox editor mode inline content */
	void UpdateInlineContent(TSharedPtr<SWidget> InlineContent) const;

	/** Creates and sets the mode toolbar */
	void UpdateModeToolBar();

	/** Handles updating the mode toolbar when the user settings change */
	void HandleUserSettingsChange( FName PropertyName );

	/**
	 * Called when the tab is closed.
	 * @param TabBeingClosed The tab being closed
	 */
	void HandleParentClosed( TSharedRef<SDockTab> TabBeingClosed );

private:

	/** Level editor that we're associated with */
	TWeakPtr<class ILevelEditor> LevelEditor;

	/** Doc tab we're hosted in */
	TWeakPtr<class SDockTab> DocTab;

	FEdMode* EditorMode;

	/** Inline content area for editor modes */
	TSharedPtr<SBorder> InlineContentHolder;
};
