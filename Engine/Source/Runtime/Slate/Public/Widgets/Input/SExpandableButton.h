// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/Layout/SBorder.h"

/**
 * A button that can either be collapsed or expanded, containing different content in each state.
 */
class SLATE_API SExpandableButton
	: public SBorder
{
public:

	SLATE_BEGIN_ARGS( SExpandableButton )
		: _IsExpanded( true )
		{}

		/** The text to display in this button in it's collapsed state (if nothing is specified for CollapsedButtonContent) */
		SLATE_ATTRIBUTE( FText, CollapsedText )

		/** The text to display in this button in it's expanded state (if nothing is specified for ExpandedButtonContent) */
		SLATE_ATTRIBUTE( FText, ExpandedText )

		/** Slot for this button's collapsed content (optional) */
		SLATE_NAMED_SLOT( FArguments, CollapsedButtonContent )

		/** Slot for this button's expanded content (optional) */
		SLATE_NAMED_SLOT( FArguments, ExpandedButtonContent )

		/** Slot for this button's expanded body */
		SLATE_NAMED_SLOT( FArguments, ExpandedChildContent )

		/** Called when the expansion button is clicked */
		SLATE_EVENT( FOnClicked, OnExpansionClicked )

		/** Called when the close button is clicked */
		SLATE_EVENT( FOnClicked, OnCloseClicked )

		/** Current expansion state */
		SLATE_ATTRIBUTE( bool, IsExpanded )

	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct(const FArguments& InArgs);

protected:

	/** Callbacks to determine visibility of parts that should be shown when the button state is collapsed or expanded */
	EVisibility GetCollapsedVisibility() const;
	EVisibility GetExpandedVisibility() const;

protected:

	/** The attribute of the current expansion state */
	TAttribute<bool> IsExpanded;
};
