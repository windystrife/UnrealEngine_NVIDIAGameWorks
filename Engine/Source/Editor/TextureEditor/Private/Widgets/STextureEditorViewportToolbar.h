// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"

class FUICommandList;
class SMenuAnchor;

/**
 * Implements a tool bar for the texture editor viewport.
 */
class STextureEditorViewportToolbar
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STextureEditorViewportToolbar) { }
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<FUICommandList>& InToolkitCommands );

protected:

	/**
	 * Generates the view options menu widget.
	 */
	TSharedRef<SWidget> GenerateViewOptionsMenu( ) const;

private:

	// Callback for clicking the View Options menu button.
	FReply HandleViewOptionsMenuButtonClicked( );

private:

	// Holds a pointer to the toolkit command list.
	TSharedPtr<FUICommandList> ToolkitCommands;

	// Holds the anchor for the view options menu.
	TSharedPtr<SMenuAnchor> ViewOptionsMenuAnchor;
};
