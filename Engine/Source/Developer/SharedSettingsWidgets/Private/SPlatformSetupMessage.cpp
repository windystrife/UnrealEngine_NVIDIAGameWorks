// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SPlatformSetupMessage.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "EditorStyleSet.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SPlatformSetupMessage"

/////////////////////////////////////////////////////
// SPlatformSetupMessage

TSharedRef<SWidget> SPlatformSetupMessage::MakeRow(FName IconName, FText Message, FText ButtonMessage)
{
	FText Tooltip = FText::Format(LOCTEXT("PlatformSetupTooltip", "Status of platform setup file\n'{0}'"), FText::FromString(TargetFilename));

	TSharedRef<SHorizontalBox> Result = SNew(SHorizontalBox)
		.ToolTipText(Tooltip)

		// Status icon
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(FEditorStyle::GetBrush(IconName))
		]

		// Notice
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(16.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::White)
			.ShadowColorAndOpacity(FLinearColor::Black)
			.ShadowOffset(FVector2D::UnitVector)
			.Text(Message)
		];


	if (!ButtonMessage.IsEmpty())
	{
		Result->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.OnClicked(this, &SPlatformSetupMessage::OnButtonPressed)
				.Text(ButtonMessage)
			];
	}

	return Result;
}

void SPlatformSetupMessage::Construct(const FArguments& InArgs, const FString& InTargetFilename)
{
	TargetFilename = InTargetFilename;

	OnSetupClicked = InArgs._OnSetupClicked;

	TSharedRef<SWidget> MissingFilesWidget = MakeRow(
		"SettingsEditor.WarningIcon",
		FText::Format(LOCTEXT("MissingFilesText", "Project is not configured for the {0} platform"), InArgs._PlatformName),
		LOCTEXT("MissingFilesButton", "Configure Now"));

	TSharedRef<SWidget> NeedsCheckoutWidget = MakeRow(
		"SettingsEditor.WarningIcon",
		LOCTEXT("NeedsCheckoutText", "Platform files are under source control"),
		LOCTEXT("NeedsCheckoutButton", "Check Out"));

	TSharedRef<SWidget> ReadOnlyFilesWidget = MakeRow(
		"SettingsEditor.WarningIcon",
		LOCTEXT("ReadOnlyText", "Platform files are read-only or locked"),
		LOCTEXT("ReadOnlyButton", "Make Writable"));

	TSharedRef<SWidget> ReadyToModifyWidget = MakeRow(
		"SettingsEditor.GoodIcon",
		LOCTEXT("ReadyToModifyText", "Platform files are writeable"),
		FText::GetEmpty());

	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor(this, &SPlatformSetupMessage::GetBorderColor)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.LightGroupBorder"))
		.Padding(8.0f)
		[
			SNew(SWidgetSwitcher)
			.WidgetIndex(this, &SPlatformSetupMessage::GetSetupStateAsInt)

			// Locked slot
			+SWidgetSwitcher::Slot()
			[
				MissingFilesWidget
			]
			+SWidgetSwitcher::Slot()
			[
				NeedsCheckoutWidget
			]
			+SWidgetSwitcher::Slot()
			[
				ReadOnlyFilesWidget
			]
			+SWidgetSwitcher::Slot()
			[
				ReadyToModifyWidget
			]
		]
	];

	UpdateCache(true);
}

void SPlatformSetupMessage::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	UpdateCache(false);
}

int32 SPlatformSetupMessage::GetSetupStateAsInt() const
{
	return (int32)CachedSetupState;
}

SPlatformSetupMessage::ESetupState SPlatformSetupMessage::GetSetupStateBasedOnFile(bool bForce) const
{
	if (!FPaths::FileExists(TargetFilename))
	{
		return MissingFiles;
	}
	else
	{
		ISourceControlModule& SCC = ISourceControlModule::Get();
		if (SCC.IsEnabled() && SCC.GetProvider().IsAvailable())
		{
			ISourceControlProvider& Provider = SCC.GetProvider();

			FSourceControlStatePtr SourceControlState = Provider.GetState(TargetFilename, bForce ? EStateCacheUsage::ForceUpdate : EStateCacheUsage::Use);
			if (SourceControlState.IsValid())
			{
				if (SourceControlState->IsSourceControlled())
				{
					if (SourceControlState->CanCheckout())
					{
						return NeedsCheckout;
					}
				}
				else
				{
					//@TODO: Should we instead try to add the file?
				}
			}
		}
		
		// SCC is disabled or unavailable
		const bool bIsReadOnly = FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*TargetFilename);
		return bIsReadOnly ? ReadOnlyFiles : ReadyToModify;
	}
}

void SPlatformSetupMessage::UpdateCache(bool bForceUpdate)
{
	CachedSetupState = GetSetupStateBasedOnFile(bForceUpdate);
}

FSlateColor SPlatformSetupMessage::GetBorderColor() const
{
	switch (CachedSetupState)
	{
	case MissingFiles:
		return FLinearColor(0.8f, 0, 0);
	case ReadyToModify:
		return FLinearColor::Green;
	case ReadOnlyFiles:
	case NeedsCheckout:
	default:
		return FLinearColor::Yellow;
	}
}

FReply SPlatformSetupMessage::OnButtonPressed()
{
	switch (CachedSetupState)
	{
	case MissingFiles:
		OnSetupClicked.Execute();
		break;

	case ReadOnlyFiles:
		{
			if (!FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*TargetFilename, false))
			{
				FText NotificationErrorText = FText::Format(LOCTEXT("FailedToMakeWritable", "Could not make {0} writable."), FText::FromString(TargetFilename));

				FNotificationInfo Info(NotificationErrorText);
				Info.ExpireDuration = 3.0f;

				FSlateNotificationManager::Get().AddNotification(Info);
			}
		}
		break;

	case NeedsCheckout:
		{
			FText ErrorMessage;

			if (!SourceControlHelpers::CheckoutOrMarkForAdd(TargetFilename, FText::FromString(TargetFilename), NULL, ErrorMessage))
			{
				FNotificationInfo Info(ErrorMessage);
				Info.ExpireDuration = 3.0f;
				FSlateNotificationManager::Get().AddNotification(Info);
			}
		}
		break;

	case ReadyToModify:
	default:
		break;
	}

	return FReply::Handled();
}

bool SPlatformSetupMessage::IsReadyToGo() const
{
	return CachedSetupState == ReadyToModify;
}

TAttribute<bool> SPlatformSetupMessage::GetReadyToGoAttribute() const
{
	return TAttribute<bool>(this, &SPlatformSetupMessage::IsReadyToGo);
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
