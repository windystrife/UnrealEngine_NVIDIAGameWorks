// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CrashReportClient.h"

#if !CRASH_REPORT_UNATTENDED_ONLY

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

/**
 * UI for the crash report client app
 */
class SCrashReportClient : public SCompoundWidget
{
public:
	/**
	 * Slate arguments
	 */
	SLATE_BEGIN_ARGS(SCrashReportClient)
	{
	}

	SLATE_END_ARGS()

	/**
	 * Construct this Slate ui
	 * @param InArgs Slate arguments, not used
	 * @param Client Crash report client implementation object
	 */
	void Construct(const FArguments& InArgs, TSharedRef<FCrashReportClient> Client);

private:
	/**
	 * Keyboard short-cut handler
	 * @param InKeyEvent Which key was released, and which auxiliary keys were pressed
	 * @return Whether the event was handled
	 */
	FReply OnUnhandledKeyDown(const FKeyEvent& InKeyEvent);

	/** Called if the multi line widget text changes */
	void OnUserCommentTextChanged(const FText& NewText);

	/** Whether the hint text should be visible. */
	EVisibility IsHintTextVisible() const;

	/** Whether the send buttons are enabled. */
	bool IsSendEnabled() const;

	/** Crash report client implementation object */
	TSharedPtr<FCrashReportClient> CrashReportClient;

	TSharedPtr<SMultiLineEditableTextBox> CrashDetailsInformation;

	bool bHasUserCommentErrors;
};

#endif // !CRASH_REPORT_UNATTENDED_ONLY
