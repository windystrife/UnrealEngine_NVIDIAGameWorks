// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/SlateDelegates.h"

class SButton;

/**
 * Implements the automation console command bar widget.
 */
class SAutomationWindowCommandBar
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SAutomationWindowCommandBar) { }

 		/** Called when the copy log button is clicked. */
 		SLATE_EVENT(FOnClicked, OnCopyLogClicked)

	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef< SNotificationList >& InNotificationList );

	/**
	 * Sets the number of messages selected in the log window.
	 *
	 * @param Count Number of selected messages.
	 */
	void SetNumLogMessages( int Count );

private:

	/** Handles clicking the copy log button. */
	FReply HandleCopyButtonClicked( );

private:

 	/** Holds the copy log button. */
 	TSharedPtr<SButton> CopyButton;

 	/** Holds a delegate that is executed when the copy log button is clicked. */
 	FOnClicked OnCopyLogClicked;
};
