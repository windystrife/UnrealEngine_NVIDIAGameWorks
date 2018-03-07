// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/Margin.h"
#include "Widgets/SCompoundWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "InstalledPlatformInfo.h"

#define LOCTEXT_NAMESPACE "SProjectLauncherBuildConfigurationSelector"


/**
 * Delegate type for build configuration selections.
 *
 * The first parameter is the selected build configuration.
 */
DECLARE_DELEGATE_OneParam(FOnSessionSProjectLauncherBuildConfigurationSelected, EBuildConfigurations::Type)


/**
 * Implements a build configuration selector widget.
 */
class SProjectLauncherBuildConfigurationSelector
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SProjectLauncherBuildConfigurationSelector) { }
		SLATE_EVENT(FOnSessionSProjectLauncherBuildConfigurationSelected, OnConfigurationSelected)
		SLATE_ATTRIBUTE(FText, Text)
		SLATE_ATTRIBUTE(FSlateFontInfo, Font)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param InModel The data model.
	 */
	void Construct(const FArguments& InArgs)
	{
		OnConfigurationSelected = InArgs._OnConfigurationSelected;

		struct FConfigInfo
		{
			FText ToolTip;
			EBuildConfigurations::Type Configuration;
		};

		const FConfigInfo Configurations[] =
		{
			{LOCTEXT("DebugActionHint", "Debug configuration."), EBuildConfigurations::Debug},
			{LOCTEXT("DebugGameActionHint", "DebugGame configuration."), EBuildConfigurations::DebugGame},
			{LOCTEXT("DevelopmentActionHint", "Development configuration."), EBuildConfigurations::Development},
			{LOCTEXT("ShippingActionHint", "Shipping configuration."), EBuildConfigurations::Shipping},
			{LOCTEXT("TestActionHint", "Test configuration."), EBuildConfigurations::Test}
		};

		// create build configurations menu
		FMenuBuilder MenuBuilder(true, NULL);
		{
			for (const FConfigInfo& ConfigInfo : Configurations)
			{
				if (FInstalledPlatformInfo::Get().IsValidConfiguration(ConfigInfo.Configuration))
				{
					FUIAction UIAction(FExecuteAction::CreateSP(this, &SProjectLauncherBuildConfigurationSelector::HandleMenuEntryClicked, ConfigInfo.Configuration));
					MenuBuilder.AddMenuEntry(EBuildConfigurations::ToText(ConfigInfo.Configuration), ConfigInfo.ToolTip, FSlateIcon(), UIAction);
				}
			}
		}

		FSlateFontInfo TextFont = InArgs._Font.IsSet() ? InArgs._Font.Get() : FCoreStyle::Get().GetFontStyle(TEXT("SmallFont"));
		
		ChildSlot
		[
			// build configuration menu
			SNew(SComboButton)
			.VAlign(VAlign_Center)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Font(TextFont)
				.Text(InArgs._Text)
			]
			.ContentPadding(FMargin(4.0f, 2.0f))
			.MenuContent()
			[
				MenuBuilder.MakeWidget()
			]
		];
	}

private:

	// Callback for clicking a menu entry.
	void HandleMenuEntryClicked( EBuildConfigurations::Type Configuration )
	{
		OnConfigurationSelected.ExecuteIfBound(Configuration);
	}

private:

	// Holds a delegate to be invoked when a build configuration has been selected.
	FOnSessionSProjectLauncherBuildConfigurationSelected OnConfigurationSelected;
};


#undef LOCTEXT_NAMESPACE
