// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SProjectLauncherPackagingSettings.h"

#include "DesktopPlatformModule.h"
#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateTypes.h"
#include "SlateOptMacros.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SProjectLauncherPackagingSettings"


/* SProjectLauncherPackagingSettings structors
 *****************************************************************************/

SProjectLauncherPackagingSettings::~SProjectLauncherPackagingSettings()
{
	if (Model.IsValid())
	{
		Model->OnProfileSelected().RemoveAll(this);
	}
}


/* SProjectLauncherPackagingSettings interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SProjectLauncherPackagingSettings::Construct(const FArguments& InArgs, const TSharedRef<FProjectLauncherModel>& InModel)
{
	Model = InModel;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.FillHeight(1.0)
			[
				SNew(SBorder)
					.Padding(8.0)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
									.Text(this, &SProjectLauncherPackagingSettings::HandleDirectoryTitleText)
							]

						+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0, 4.0, 0.0, 0.0)
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
									.FillWidth(1.0)
									.Padding(0.0, 0.0, 0.0, 3.0)
									[
										// repository path text box
										SAssignNew(DirectoryPathTextBox, SEditableTextBox)
										.IsEnabled(this, &SProjectLauncherPackagingSettings::IsEditable)
										.OnTextCommitted(this, &SProjectLauncherPackagingSettings::OnTextCommitted)
										.OnTextChanged(this, &SProjectLauncherPackagingSettings::OnTextChanged)
										.HintText(this, &SProjectLauncherPackagingSettings::HandleHintPathText)
									]

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.HAlign(HAlign_Right)
									.Padding(4.0, 0.0, 0.0, 0.0)
									[
										// browse button
										SNew(SButton)
											.ContentPadding(FMargin(6.0, 2.0))
											.IsEnabled(this, &SProjectLauncherPackagingSettings::IsEditable)
											.Text(LOCTEXT("BrowseButtonText", "Browse..."))
											.ToolTipText(LOCTEXT("BrowseButtonToolTip", "Browse for the directory"))
											.OnClicked(this, &SProjectLauncherPackagingSettings::HandleBrowseButtonClicked)
									]
							]

						// don't include editor content
						+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[

								SNew(SCheckBox)
									.IsEnabled(this, &SProjectLauncherPackagingSettings::IsEditable)
									.IsChecked(this, &SProjectLauncherPackagingSettings::HandleForDistributionCheckBoxIsChecked)
									.OnCheckStateChanged(this, &SProjectLauncherPackagingSettings::HandleForDistributionCheckBoxCheckStateChanged)
									.Padding(FMargin(4.0f, 0.0f))
									.ToolTipText(LOCTEXT("ForDistributionCheckBoxTooltip", "If checked the build will be marked as for release to the public (distribution)."))
									.Content()
									[
										SNew(STextBlock)
										.Text(LOCTEXT("ForDistributionCheckBoxText", "Is this build for distribution to the public"))
									]
							]
					]
			]
	];

	Model->OnProfileSelected().AddSP(this, &SProjectLauncherPackagingSettings::HandleProfileManagerProfileSelected);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


void SProjectLauncherPackagingSettings::UpdateDirectoryPathText()
{
	DirectoryPathTextBox->SetText(HandleDirectoryPathText());
}


/* SProjectLauncherPackagingSettings callbacks
 *****************************************************************************/

void SProjectLauncherPackagingSettings::HandleForDistributionCheckBoxCheckStateChanged(ECheckBoxState NewState)
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();
	if (SelectedProfile.IsValid())
	{
		SelectedProfile->SetForDistribution(NewState == ECheckBoxState::Checked);
	}
}


ECheckBoxState SProjectLauncherPackagingSettings::HandleForDistributionCheckBoxIsChecked() const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();
	if (SelectedProfile.IsValid())
	{
		if (SelectedProfile->IsForDistribution())
		{
			return ECheckBoxState::Checked;
		}
	}
	return ECheckBoxState::Unchecked;
}


FText SProjectLauncherPackagingSettings::HandleDirectoryTitleText() const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		ELauncherProfilePackagingModes::Type PackagingMode = SelectedProfile->GetPackagingMode();
		switch (PackagingMode)
		{
		case ELauncherProfilePackagingModes::Locally:
			return LOCTEXT("LocalDirectoryPathLabel", "Local Directory Path:");
		case ELauncherProfilePackagingModes::SharedRepository:
			return LOCTEXT("RepositoryPathLabel", "Repository Path:");
		}
	}

	return FText::GetEmpty();
}


FText SProjectLauncherPackagingSettings::HandleDirectoryPathText() const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		ELauncherProfilePackagingModes::Type PackagingMode = SelectedProfile->GetPackagingMode();
		if (PackagingMode == ELauncherProfilePackagingModes::Locally)
		{
			return FText::FromString(SelectedProfile->GetPackageDirectory());
		}
	}

	return FText::GetEmpty();
}


FText SProjectLauncherPackagingSettings::HandleHintPathText() const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid() && SelectedProfile->GetPackagingMode() == ELauncherProfilePackagingModes::Locally && !SelectedProfile->GetProjectBasePath().IsEmpty())
	{
		const FString ProjectPathWithoutExtension = FPaths::GetPath(SelectedProfile->GetProjectPath());
		return FText::FromString(ProjectPathWithoutExtension / "Saved" / "StagedBuilds");
	}
	
	return FText::GetEmpty();
}


void SProjectLauncherPackagingSettings::HandleProfileManagerProfileSelected(const ILauncherProfilePtr& SelectedProfile, const ILauncherProfilePtr& PreviousProfile)
{
	UpdateDirectoryPathText();
}


FReply SProjectLauncherPackagingSettings::HandleBrowseButtonClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

		FString FolderName;
		const bool bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
			ParentWindowHandle,
			LOCTEXT("FolderDialogTitle", "Choose a directory").ToString(),
			DirectoryPathTextBox->GetText().ToString(),
			FolderName
			);

		if (bFolderSelected)
		{
			if (!FolderName.EndsWith(TEXT("/")))
			{
				FolderName += TEXT("/");
			}

			DirectoryPathTextBox->SetText(FText::FromString(FolderName));
			ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

			if (SelectedProfile.IsValid())
			{
				SelectedProfile->SetPackageDirectory(FolderName);
			}
		}
	}

	return FReply::Handled();
}


bool SProjectLauncherPackagingSettings::IsEditable() const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		return SelectedProfile->GetPackagingMode() == ELauncherProfilePackagingModes::Locally;
	}
	return false;
}


void SProjectLauncherPackagingSettings::OnTextChanged(const FText& InText)
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		SelectedProfile->SetPackageDirectory(InText.ToString());
	}
}


void SProjectLauncherPackagingSettings::OnTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

		if (SelectedProfile.IsValid())
		{
			SelectedProfile->SetPackageDirectory(InText.ToString());
		}
	}
}


#undef LOCTEXT_NAMESPACE
