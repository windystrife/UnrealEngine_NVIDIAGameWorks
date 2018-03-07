// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Either a button to directly install or a hyperlink to a website to download the suggested IDE for the platform.
 * Only visible when no compiler is available.
 */
class SGetSuggestedIDEWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGetSuggestedIDEWidget)
	{}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	
private:
	/** Creates the appropriate widget to display for the platform */
	TSharedRef<SWidget> CreateGetSuggestedIDEWidget() const;

	/** Gets the visibility of the global error label IDE Link */
	EVisibility GetVisibility() const;

	/** Handler for when the error label IDE hyperlink was clicked */
	void OnDownloadIDEClicked(FString URL);

	/** Handler for when the install IDE button was clicked */
	FReply OnInstallIDEClicked();

	/** Handler for when the suggested IDE installer has finished downloading */
	static void OnIDEInstallerDownloadComplete(bool bWasSuccessful);

private:

	/** Handle to the notification displayed when downloading an IDE installer */
	static TSharedPtr<class SNotificationItem> IDEDownloadNotification;

};
