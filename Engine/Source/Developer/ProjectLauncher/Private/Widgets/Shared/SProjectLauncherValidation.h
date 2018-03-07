// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ILauncherProfile.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "EditorStyleSet.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"


#define LOCTEXT_NAMESPACE "SProjectLauncherValidation"


/**
 * Implements the launcher's profile validation panel.
 */
class SProjectLauncherValidation
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SProjectLauncherValidation) { }
		SLATE_ATTRIBUTE(TSharedPtr<ILauncherProfile>, LaunchProfile)
	SLATE_END_ARGS()


public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param InModel The data model.
	 */
	void Construct( const FArguments& InArgs)
	{
		LaunchProfileAttr = InArgs._LaunchProfile;

		ChildSlot
		[
			SNew(SVerticalBox)
			
			// build settings
			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoBuildGameSelectedError", "A Project must be selected.").ToString(), ELauncherProfileValidationErrors::NoProjectSelected)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoBuildConfigurationSelectedError", "A Build Configuration must be selected.").ToString(), ELauncherProfileValidationErrors::NoBuildConfigurationSelected)
				]

			// cook settings
			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoCookedPlatformSelectedError", "At least one Platform must be selected when cooking by the book.").ToString(), ELauncherProfileValidationErrors::NoPlatformSelected)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoCookedCulturesSelectedError", "At least one Culture must be selected when cooking by the book.").ToString(), ELauncherProfileValidationErrors::NoCookedCulturesSelected)
				]

			// packaging settings

			// archive step
			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoArchiveDirectorySpecifiedError", "The archive step requires a valid directory.").ToString(), ELauncherProfileValidationErrors::NoArchiveDirectorySpecified)
				]

			// deployment settings
			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("CopyToDeviceRequiresCookByTheBookError", "Deployment by copying to device requires 'By The Book' cooking.").ToString(), ELauncherProfileValidationErrors::CopyToDeviceRequiresCookByTheBook)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("DeployedDeviceGroupRequired", "A device group must be selected when deploying builds.").ToString(), ELauncherProfileValidationErrors::DeployedDeviceGroupRequired)
				]

			// launch settings
			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("CustomRolesNotSupportedYet", "Custom launch roles are not supported yet.").ToString(), ELauncherProfileValidationErrors::CustomRolesNotSupportedYet)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("InitialCultureNotAvailable", "The Initial Culture selected for launch is not in the build.").ToString(), ELauncherProfileValidationErrors::InitialCultureNotAvailable)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("InitialMapNotAvailable", "The Initial Map selected for launch is not in the build.").ToString(), ELauncherProfileValidationErrors::InitialMapNotAvailable)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoLaunchRoleDeviceAssigned", "One or more launch roles do not have a device assigned.").ToString(), ELauncherProfileValidationErrors::NoLaunchRoleDeviceAssigned)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("GeneratingChunksRequiresUnrealPak", "UnrealPak must be selected to Generate Chunks.").ToString(), ELauncherProfileValidationErrors::GeneratingChunksRequiresUnrealPak)
				]
			
			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("GeneratingHttpChunkDataRequiresGeneratingChunks", "Generate Chunks must be selected to Generate Http Chunk Install Data.").ToString(), ELauncherProfileValidationErrors::GeneratingHttpChunkDataRequiresGeneratingChunks)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("GeneratingHttpChunkDataRequiresValidDirectoryAndName", "Generating Http Chunk Install Data requires a valid directory and release name.").ToString(), ELauncherProfileValidationErrors::GeneratingHttpChunkDataRequiresValidDirectoryAndName)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("ShippingDoesntSupportCommandlineOptionsCantUseCookOnTheFly", "Shipping doesn't support commandline options and can't use cook on the fly").ToString(), ELauncherProfileValidationErrors::ShippingDoesntSupportCommandlineOptionsCantUseCookOnTheFly)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("CookOnTheFlyDoesntSupportServer", "Cook on the fly doesn't support server target configurations").ToString(), ELauncherProfileValidationErrors::CookOnTheFlyDoesntSupportServer)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("UnversionedAndIncrimental", "Unversioned build cannot be incremental.").ToString(), ELauncherProfileValidationErrors::UnversionedAndIncrimental)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeCallbackMessage(TEXT("Icons.Error"), ELauncherProfileValidationErrors::NoPlatformSDKInstalled)
				]
		];
	}

protected:

	/**
	 * Creates a widget for a validation message.
	 *
	 * @param IconName The name of the message icon.
	 * @param MessageText The message text.
	 * @param MessageType The message type.
	 */
	TSharedRef<SWidget> MakeValidationMessage( const TCHAR* IconName, const FString& MessageText, ELauncherProfileValidationErrors::Type Message )
	{
		return SNew(SHorizontalBox)
			.Visibility(this, &SProjectLauncherValidation::HandleValidationMessageVisibility, Message)

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0)
			[
				SNew(SImage)
					.Image(FEditorStyle::GetBrush(IconName))
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(FText::FromString(MessageText))
			];
	}

	/**
	 * Creates a widget for a validation message.
	 *
	 * @param IconName The name of the message icon.
	 * @param MessageType The message type.
	 */
	TSharedRef<SWidget> MakeCallbackMessage( const TCHAR* IconName,ELauncherProfileValidationErrors::Type Message )
	{
		return SNew(SHorizontalBox)
			.Visibility(this, &SProjectLauncherValidation::HandleValidationMessageVisibility, Message)

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0)
			[
				SNew(SImage)
					.Image(FEditorStyle::GetBrush(IconName))
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(this, &SProjectLauncherValidation::HandleValidationMessage, Message)
			];
	}

private:

	// Callback for getting the visibility state of a validation message.
	EVisibility HandleValidationMessageVisibility( ELauncherProfileValidationErrors::Type Error ) const
	{
		TSharedPtr<ILauncherProfile> LaunchProfile = LaunchProfileAttr.Get();
		if (!LaunchProfile.IsValid() || LaunchProfile->HasValidationError(Error))
		{
			return EVisibility::Visible;
		}
		return EVisibility::Collapsed;
	}

	FText HandleValidationMessage( ELauncherProfileValidationErrors::Type Error ) const
	{
		TSharedPtr<ILauncherProfile> LaunchProfile = LaunchProfileAttr.Get();
		if (LaunchProfile.IsValid())
		{
			if (LaunchProfile->HasValidationError(Error))
			{
				return FText::Format(LOCTEXT("NoPlatformSDKInstalledFmt", "A required platform SDK is missing: {0}"), FText::FromString(LaunchProfile->GetInvalidPlatform()));
			}

			return FText::GetEmpty();
		}
		return LOCTEXT("InvalidLaunchProfile", "Invalid Launch Profile.");
	}

private:

	// Attribute for the launch profile this widget shows validation for. 
	TAttribute<TSharedPtr<ILauncherProfile>> LaunchProfileAttr;
};


#undef LOCTEXT_NAMESPACE
