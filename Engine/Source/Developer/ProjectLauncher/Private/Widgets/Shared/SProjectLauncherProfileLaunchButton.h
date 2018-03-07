// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "EditorStyleSet.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Shared/SProjectLauncherValidation.h"


#define LOCTEXT_NAMESPACE "SProjectLauncherProfileLaunchButton"


/**
 * Implements a build configuration selector widget.
 */
class SProjectLauncherProfileLaunchButton
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SProjectLauncherProfileLaunchButton) { }
	SLATE_EVENT(FOnClicked, OnClicked)
		SLATE_ATTRIBUTE(ILauncherProfilePtr, LaunchProfile)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param InModel The data model.
	 */
	void Construct(const FArguments& InArgs, bool InShowText)
	{
		LaunchProfileAttr = InArgs._LaunchProfile;

		TSharedPtr<SVerticalBox> VerticalBoxWidget;
		ChildSlot
		[	
			SNew(SOverlay)
					
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FCoreStyle::Get(), "Toolbar.Button")
				.ForegroundColor(FEditorStyle::GetSlateColor("DefaultForeground"))
				.OnClicked(InArgs._OnClicked)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ContentPadding(0)
				.IsEnabled(this, &SProjectLauncherProfileLaunchButton::ButtonEnabled)
				[
					SAssignNew(VerticalBoxWidget, SVerticalBox)

					// Icon
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(this, &SProjectLauncherProfileLaunchButton::GetLaunchIcon)
					]
				]
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(this, &SProjectLauncherProfileLaunchButton::GetErrorIcon)
				.Visibility(this, &SProjectLauncherProfileLaunchButton::GetErrorVisibility)
			]
		];

		// Add launch text is this was requested
		if (InShowText && VerticalBoxWidget.IsValid())
		{
			VerticalBoxWidget->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.TextStyle(FCoreStyle::Get(), "Toolbar.Label")
				.ShadowOffset(FVector2D::UnitVector)
				.Text(LOCTEXT("ProjectLauncherLaunch", "Launch"))
			];
		}

		// Tooltip when we have validation errors
		SAssignNew(ErrorToolTipWidget, SToolTip)
		[
			SNew(SProjectLauncherValidation)
			.LaunchProfile(InArgs._LaunchProfile)
		];

		// Otherwise we fall back on simple text
		SetToolTipText(LOCTEXT("ProjectLauncherLaunchToolTip", "Launch this profile"));
	}

	virtual TSharedPtr<IToolTip> GetToolTip() override
	{
		ILauncherProfilePtr LaunchProfile = LaunchProfileAttr.Get();
		if (LaunchProfile.IsValid() && LaunchProfile->HasValidationError())
		{
			ErrorToolTipWidget->SetContentWidget(
				SNew(SProjectLauncherValidation)
				.LaunchProfile(LaunchProfileAttr));

			return ErrorToolTipWidget;
		}
		return SWidget::GetToolTip();
	}

private:

	// @returns true if there is an error in the launch profile, no launch profile is considered an error
	bool HasError() const
	{
		ILauncherProfilePtr LaunchProfile = LaunchProfileAttr.Get();
		if (LaunchProfile.IsValid() && !LaunchProfile->HasValidationError())
		{
			return false;
		}
		return true;
	}

	// Callback to see if the launch button should be enabled
	bool ButtonEnabled() const
	{
		return !HasError();
	}

	// Get the SlateIcon for Launch Button
	const FSlateBrush* GetLaunchIcon() const
	{
		return FEditorStyle::GetBrush("Launcher.Run");
	}

	// Get the SlateIcon for Error Image
	const FSlateBrush* GetErrorIcon() const
	{
		return FEditorStyle::GetBrush(TEXT("Icons.Error"));
	}

	// Callback to see if the error icon should be displayed, based on validity of the launch profile
	EVisibility GetErrorVisibility() const
	{
		return HasError() ? EVisibility::Visible : EVisibility::Hidden;
	}

private:

	/** Attribute for the launch profile this widget launches. */
	TAttribute<ILauncherProfilePtr> LaunchProfileAttr;

	/** Holds a pointer to our custom tooltip. */
	TSharedPtr<SToolTip> ErrorToolTipWidget;

};


#undef LOCTEXT_NAMESPACE
