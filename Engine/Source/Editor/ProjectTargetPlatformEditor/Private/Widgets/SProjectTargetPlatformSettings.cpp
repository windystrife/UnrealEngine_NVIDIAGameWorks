// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SProjectTargetPlatformSettings.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "PlatformInfo.h"
#include "GameProjectGenerationModule.h"
#include "Interfaces/IProjectManager.h"

#define LOCTEXT_NAMESPACE "SProjectTargetPlatformSettings"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SProjectTargetPlatformSettings::Construct(const FArguments& InArgs)
{
	// Create and sort a list of vanilla platforms that are game targets (sort by display name)
	// We show all of the platforms regardless of whether we have an SDK installed for them or not
	for(const PlatformInfo::FPlatformInfo& PlatformInfo : PlatformInfo::EnumeratePlatformInfoArray())
	{
		if(PlatformInfo.IsVanilla() && PlatformInfo.PlatformType == PlatformInfo::EPlatformType::Game)
		{
#if !PLATFORM_WINDOWS
			// @todo AllDesktop now only works on Windows (it can compile D3D shaders, and it can remote compile Metal shaders)
			if (PlatformInfo.PlatformInfoName != TEXT("AllDesktop"))
#endif
			{
				AvailablePlatforms.Add(&PlatformInfo);
			}
		}
	}

	AvailablePlatforms.Sort([](const PlatformInfo::FPlatformInfo& One, const PlatformInfo::FPlatformInfo& Two) -> bool
	{
		return One.DisplayName.CompareTo(Two.DisplayName) < 0;
	});

	// Generate a widget for each platform
	TSharedRef<SVerticalBox> PlatformsListBox = SNew(SVerticalBox);
	for(const PlatformInfo::FPlatformInfo* AvailablePlatform : AvailablePlatforms)
	{
		PlatformsListBox->AddSlot()
		.AutoHeight()
		[
			MakePlatformRow(AvailablePlatform->DisplayName, AvailablePlatform->PlatformInfoName, FEditorStyle::GetBrush(AvailablePlatform->GetIconStyleName(PlatformInfo::EPlatformIconSize::Normal)))
		];
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(5.0f)
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					MakePlatformRow(LOCTEXT("AllPlatforms", "All Platforms"), NAME_None, FEditorStyle::GetBrush("Launcher.Platform.AllPlatforms"))
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 1.0f))
				[
					SNew(SSeparator)
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					PlatformsListBox
				]
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 5.0f))
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(5.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(LOCTEXT("PlatformsListDescription", "Select the supported platforms for your project. Attempting to package, run, or cook your project on an unsupported platform will result in a warning."))
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> SProjectTargetPlatformSettings::MakePlatformRow(const FText& DisplayName, const FName PlatformName, const FSlateBrush* Icon) const
{
	return SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(5.0f, 0.0f))
		[
			SNew(SCheckBox)
			.IsChecked(this, &SProjectTargetPlatformSettings::HandlePlatformCheckBoxIsChecked, PlatformName)
			.IsEnabled(this, &SProjectTargetPlatformSettings::HandlePlatformCheckBoxIsEnabled, PlatformName)
			.OnCheckStateChanged(this, &SProjectTargetPlatformSettings::HandlePlatformCheckBoxStateChanged, PlatformName)
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(5.0f, 2.0f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(20)
			.HeightOverride(20)
			[
				SNew(SImage)
				.Image(Icon)
			]
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(5.0f, 0.0f))
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(DisplayName)
		];
}

ECheckBoxState SProjectTargetPlatformSettings::HandlePlatformCheckBoxIsChecked(const FName PlatformName) const
{
	FProjectStatus ProjectStatus;
	if(IProjectManager::Get().QueryStatusForCurrentProject(ProjectStatus))
	{
		if(PlatformName.IsNone())
		{
			// None is "All Platforms"
			return (ProjectStatus.SupportsAllPlatforms()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		else
		{
			return (ProjectStatus.IsTargetPlatformSupported(PlatformName)) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
	}

	return ECheckBoxState::Unchecked;
}

bool SProjectTargetPlatformSettings::HandlePlatformCheckBoxIsEnabled(const FName PlatformName) const
{
	FProjectStatus ProjectStatus;
	if(IProjectManager::Get().QueryStatusForCurrentProject(ProjectStatus))
	{
		if(PlatformName.IsNone())
		{
			// None is "All Platforms"
			return true;
		}
		else
		{
			// Individual platforms are only enabled when we're not supporting "All Platforms"
			return !ProjectStatus.SupportsAllPlatforms();
		}
	}

	return false;
}

void SProjectTargetPlatformSettings::HandlePlatformCheckBoxStateChanged(ECheckBoxState InState, const FName PlatformName) const
{
	if(PlatformName.IsNone())
	{
		// None is "All Platforms"
		if(InState == ECheckBoxState::Checked)
		{
			FGameProjectGenerationModule::Get().ClearSupportedTargetPlatforms();
		}
		else
		{
			// We've deselected "All Platforms", so manually select every available platform
			for(const PlatformInfo::FPlatformInfo* AvailablePlatform : AvailablePlatforms)
			{
				FGameProjectGenerationModule::Get().UpdateSupportedTargetPlatforms(AvailablePlatform->TargetPlatformName, true);
			}
		}
	}
	else
	{
		FGameProjectGenerationModule::Get().UpdateSupportedTargetPlatforms(PlatformName, InState == ECheckBoxState::Checked);
	}
}

#undef LOCTEXT_NAMESPACE
