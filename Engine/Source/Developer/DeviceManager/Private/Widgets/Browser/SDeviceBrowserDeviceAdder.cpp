// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SDeviceBrowserDeviceAdder.h"

#include "Internationalization/Text.h"
#include "ITargetDeviceServiceManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/CoreMisc.h"
#include "SlateOptMacros.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "EditorStyleSet.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "PlatformInfo.h"


#define LOCTEXT_NAMESPACE "SDeviceBrowserDeviceAdder"


/* SDeviceBrowserDeviceAdder interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SDeviceBrowserDeviceAdder::Construct(const FArguments& InArgs, const TSharedRef<ITargetDeviceServiceManager>& InDeviceServiceManager)
{
	DeviceServiceManager = InDeviceServiceManager;

	// callback for clicking of the Add button
	auto AddButtonClicked = [this]() -> FReply {
		ITargetPlatform* TargetPlatform = GetTargetPlatformManager()->FindTargetPlatform(*PlatformComboBox->GetSelectedItem());

		FString DeviceIdString = DeviceIdTextBox->GetText().ToString();
		bool bAdded = TargetPlatform->AddDevice(DeviceIdString, false);
		if (bAdded)
		{
			// pass credentials to the newly added device
			if (TargetPlatform->RequiresUserCredentials())
			{
				// We cannot guess the device id, so we have to look it up by name
				TArray<ITargetDevicePtr> Devices;
				TargetPlatform->GetAllDevices(Devices);
				for (ITargetDevicePtr Device : Devices)
				{
					if (Device.IsValid() && Device->GetId().GetDeviceName() == DeviceIdString)
					{
						FString UserNameString = UserNameTextBox->GetText().ToString();
						FString UserPassString = UserPasswordTextBox->GetText().ToString();

						Device->SetUserCredentials(UserNameString, UserPassString);
					}
				}
			}

			DeviceIdTextBox->SetText(FText::GetEmpty());
			DeviceNameTextBox->SetText(FText::GetEmpty());
			UserNameTextBox->SetText(FText::GetEmpty());
			UserPasswordTextBox->SetText(FText::GetEmpty());
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DeviceAdderFailedToAddDeviceMessage", "Failed to add the device!"));
		}

		return FReply::Handled();
	};

	// callback for determining the enabled state of the 'Add' button
	auto AddButtonIsEnabled = [this]() -> bool {
		TSharedPtr<FString> PlatformName = PlatformComboBox->GetSelectedItem();

		if (PlatformName.IsValid())
		{
			FString TextCheck = DeviceNameTextBox->GetText().ToString();
			TextCheck.TrimStartAndEndInline();

			if (!TextCheck.IsEmpty())
			{
				ITargetPlatform* Platform = GetTargetPlatformManager()->FindTargetPlatform(*PlatformName);

				if (Platform != nullptr)
				{
					if (!Platform->RequiresUserCredentials())
					{
						return true;
					}

					// check user/password as well
					TextCheck = UserNameTextBox->GetText().ToString();
					TextCheck.TrimStartAndEndInline();

					if (!TextCheck.IsEmpty())
					{
						// do not trim the password
						return !UserPasswordTextBox->GetText().ToString().IsEmpty();
					}
				}
			}
		}

		return false;
	};

	// callback for determining the visibility of the credentials box
	auto CredentialsBoxVisibility = [this]() -> EVisibility {
		TSharedPtr<FString> PlatformName = PlatformComboBox->GetSelectedItem();

		if (PlatformName.IsValid())
		{
			ITargetPlatform* Platform = GetTargetPlatformManager()->FindTargetPlatform(*PlatformName);

			if ((Platform != nullptr) && Platform->RequiresUserCredentials())
			{
				return EVisibility::Visible;
			}
		}

		return EVisibility::Collapsed;
	};

	// callback for changes in the device name text box
	auto DeviceNameTextBoxTextChanged = [this](const FString& Text) {
		DetermineAddUnlistedButtonVisibility();
	};

	// callback for getting the name of the selected platform
	auto PlatformComboBoxContentText = [this]() -> FText {
		TSharedPtr<FString> SelectedPlatform = PlatformComboBox->GetSelectedItem();
		return SelectedPlatform.IsValid() ? FText::FromString(*SelectedPlatform) : LOCTEXT("SelectAPlatform", "Select a Platform");
	};

	// callback for generating widgets for the platforms combo box
	auto PlatformComboBoxGenerateWidget = [this](TSharedPtr<FString> Item) -> TSharedRef<SWidget> {
		const PlatformInfo::FPlatformInfo* const PlatformInfo = PlatformInfo::FindPlatformInfo(**Item);

		return
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				[
					SNew(SBox)
						.WidthOverride(24)
						.HeightOverride(24)
						[
							SNew(SImage)
								.Image((PlatformInfo) ? FEditorStyle::GetBrush(PlatformInfo->GetIconStyleName(PlatformInfo::EPlatformIconSize::Normal)) : FStyleDefaults::GetNoBrush())
						]
				]

			+ SHorizontalBox::Slot()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::FromString(*Item))
				];
	};

	// callback for handling platform selection changes
	auto PlatformComboBoxSelectionChanged = [this](TSharedPtr<FString> StringItem, ESelectInfo::Type SelectInfo) {
		// @todo
	};

	// construct children
	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)

				// platform selector
				+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Left)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("PlatformLabel", "Platform:"))
							]

						+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Left)
							.Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SAssignNew(PlatformComboBox, SComboBox<TSharedPtr<FString>>)
									.ContentPadding(FMargin(6.0f, 2.0f))
									.OptionsSource(&PlatformList)
									.OnGenerateWidget_Lambda(PlatformComboBoxGenerateWidget)
									.OnSelectionChanged_Lambda(PlatformComboBoxSelectionChanged)
									[
										SNew(STextBlock)
											.Text_Lambda(PlatformComboBoxContentText)
									]
							]
					]

				// device identifier input
				+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SVerticalBox)
							.ToolTipText(LOCTEXT("DeviceIdToolTip", "The device's unique identifier. Depending on the selected Platform, this can be a host name, an IP address, a MAC address or some other platform specific unique identifier."))

						+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Left)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("DeviceIdLabel", "Device Identifier:"))
							]

						+ SVerticalBox::Slot()
							.FillHeight(1.0)
							.Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SAssignNew(DeviceIdTextBox, SEditableTextBox)
							]
					]

				// device name input
				+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SVerticalBox)
							.ToolTipText(LOCTEXT("DeviceNameToolTip", "A display name for this device. Once the device is connected, this will be replaced with the device's actual name."))

						+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Left)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("DisplayNameLabel", "Display Name:"))
							]

						+ SVerticalBox::Slot()
							.FillHeight(1.0)
							.Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SAssignNew(DeviceNameTextBox, SEditableTextBox)
							]
					]

				// add button
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Bottom)
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SAssignNew(AddButton, SButton)
							.ContentPadding(FMargin(9.0, 2.0))
							.IsEnabled_Lambda(AddButtonIsEnabled)
							.Text(LOCTEXT("AddButtonText", "Add"))
							.OnClicked_Lambda(AddButtonClicked)
					]
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
					.Visibility_Lambda(CredentialsBoxVisibility)

				// user name input
				+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Left)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("UserNameLabel", "User:"))
							]

						+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							.Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SAssignNew(UserNameTextBox, SEditableTextBox)
							]
					]

				// user password input
				+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Left)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("UserPasswordLabel", "Password:"))
							]

						+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							.Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SAssignNew(UserPasswordTextBox, SEditableTextBox)
									.IsPassword(true)
							]
					]
			]
	];

	RefreshPlatformList();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


/* SDeviceBrowserDeviceAdder implementation
 *****************************************************************************/

void SDeviceBrowserDeviceAdder::DetermineAddUnlistedButtonVisibility()
{
	if (PlatformComboBox->GetSelectedItem().IsValid())
	{
		FString DeviceIdText = DeviceIdTextBox->GetText().ToString();
		DeviceIdText.TrimStartInline();

		AddButton->SetEnabled(!DeviceIdText.IsEmpty());
	}
}


void SDeviceBrowserDeviceAdder::RefreshPlatformList()
{
	TArray<ITargetPlatform*> Platforms = GetTargetPlatformManager()->GetTargetPlatforms();

	PlatformList.Reset();

	for (int32 Index = 0; Index < Platforms.Num(); ++Index)
	{
		PlatformList.Add(MakeShareable(new FString(Platforms[Index]->PlatformName())));
	}

	PlatformComboBox->RefreshOptions();
}


#undef LOCTEXT_NAMESPACE
