// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SProjectLauncherBuildPage.h"

#include "EditorStyleSet.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

#include "Widgets/Cook/SProjectLauncherCookedPlatforms.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Shared/SProjectLauncherBuildConfigurationSelector.h"
#include "Widgets/Shared/SProjectLauncherFormLabel.h"


#define LOCTEXT_NAMESPACE "SProjectLauncherBuildPage"


/* SProjectLauncherCookPage structors
 *****************************************************************************/

SProjectLauncherBuildPage::~SProjectLauncherBuildPage()
{
	if (Model.IsValid())
	{
		Model->OnProfileSelected().RemoveAll(this);
	}
}


/* SProjectLauncherCookPage interface
 *****************************************************************************/

void SProjectLauncherBuildPage::Construct(const FArguments& InArgs, const TSharedRef<FProjectLauncherModel>& InModel)
{
	Model = InModel;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.FillWidth(1.0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("BuildText", "Do you wish to build?"))
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(8.0, 0.0, 0.0, 0.0)
					[
						// build mode check box
						SNew(SCheckBox)
							.IsChecked(this, &SProjectLauncherBuildPage::HandleBuildIsChecked)
							.OnCheckStateChanged(this, &SProjectLauncherBuildPage::HandleBuildCheckedStateChanged)
					]
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 3, 0, 3)
			[
				SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Visibility(this, &SProjectLauncherBuildPage::ShowBuildConfiguration)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SProjectLauncherFormLabel)
									.ErrorToolTipText(NSLOCTEXT("SProjectLauncherBuildValidation", "NoBuildConfigurationSelectedError", "A Build Configuration must be selected."))
									.ErrorVisibility(this, &SProjectLauncherBuildPage::HandleValidationErrorIconVisibility, ELauncherProfileValidationErrors::NoBuildConfigurationSelected)
									.LabelText(LOCTEXT("ConfigurationComboBoxLabel", "Build Configuration:"))
							]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								// build configuration selector
								SNew(SProjectLauncherBuildConfigurationSelector)
									.Font(FCoreStyle::Get().GetFontStyle(TEXT("NormalFont")))
									.OnConfigurationSelected(this, &SProjectLauncherBuildPage::HandleBuildConfigurationSelectorConfigurationSelected)
									.Text(this, &SProjectLauncherBuildPage::HandleBuildConfigurationSelectorText)
							]
					]
			]

        + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f, 0.0f, 0.0f)
            [
                SNew(SExpandableArea)
					.AreaTitle(LOCTEXT("AdvancedAreaTitle", "Advanced Settings"))
					.InitiallyCollapsed(true)
					.Padding(8.0)
					.BodyContent()
					[
						SNew(SVerticalBox)

	/*                    + SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SButton)
							.Text(LOCTEXT("GenDSYMText", "Generate DSYM"))
							.IsEnabled(this, &SProjectLauncherBuildPage::HandleGenDSYMButtonEnabled)
							.OnClicked(this, &SProjectLauncherBuildPage::HandleGenDSYMClicked)
						]*/
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							// build mode check box
							SNew(SCheckBox)
								.IsChecked(this, &SProjectLauncherBuildPage::HandleUATIsChecked)
								.OnCheckStateChanged(this, &SProjectLauncherBuildPage::HandleUATCheckedStateChanged)
								.Padding(FMargin(4.0f, 0.0f))
								.ToolTipText(LOCTEXT("UATCheckBoxTooltip", "If checked, UAT will be built as part of the build."))
								.Content()
								[
									SNew(STextBlock)
										.Text(LOCTEXT("UATCheckBoxText", "Build UAT"))
								]
						]
				   ]
            ]

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SProjectLauncherCookedPlatforms, InModel)
					.Visibility(this, &SProjectLauncherBuildPage::HandleBuildPlatformVisibility)
			]
	];

	Model->OnProfileSelected().AddSP(this, &SProjectLauncherBuildPage::HandleProfileManagerProfileSelected);
}


/* SProjectLauncherBuildPage implementation
 *****************************************************************************/

bool SProjectLauncherBuildPage::GenerateDSYMForProject(const FString& ProjectName, const FString& Configuration)
{
    // UAT executable
    FString ExecutablePath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() + FString(TEXT("Build")) / TEXT("BatchFiles"));
#if PLATFORM_MAC
    FString Executable = TEXT("RunUAT.command");
#elif PLATFORM_LINUX
    FString Executable = TEXT("RunUAT.sh");
#else
    FString Executable = TEXT("RunUAT.bat");
#endif

    // build UAT command line parameters
    FString CommandLine;
    CommandLine = FString::Printf(TEXT("GenerateDSYM -project=%s -config=%s"),
        *ProjectName,
        *Configuration);

    // launch the builder and monitor its process
    FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*(ExecutablePath / Executable), *CommandLine, false, false, false, NULL, 0, *ExecutablePath, NULL);
	if (ProcessHandle.IsValid())
	{
		FPlatformProcess::CloseProc(ProcessHandle);
		return true;
	}

	return false;
}


/* SProjectLauncherBuildPage callbacks
 *****************************************************************************/

void SProjectLauncherBuildPage::HandleBuildCheckedStateChanged(ECheckBoxState CheckState)
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		SelectedProfile->SetBuildGame(CheckState == ECheckBoxState::Checked);
	}
}


ECheckBoxState SProjectLauncherBuildPage::HandleBuildIsChecked() const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		if (SelectedProfile->IsBuilding())
		{
			return ECheckBoxState::Checked;
		}
	}

	return ECheckBoxState::Unchecked;
}


void SProjectLauncherBuildPage::HandleProfileManagerProfileSelected(const ILauncherProfilePtr& SelectedProfile, const ILauncherProfilePtr& PreviousProfile)
{
	// reload settings
}


EVisibility SProjectLauncherBuildPage::HandleBuildPlatformVisibility() const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		if (SelectedProfile->GetCookMode() == ELauncherProfileCookModes::DoNotCook && SelectedProfile->GetDeploymentMode() == ELauncherProfileDeploymentModes::DoNotDeploy)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}


FReply SProjectLauncherBuildPage::HandleGenDSYMClicked()
{
    ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

    if(SelectedProfile.IsValid())
    {
        if (!SelectedProfile->HasValidationError(ELauncherProfileValidationErrors::NoProjectSelected))
        {
            FString ProjectName = SelectedProfile->GetProjectName();
            EBuildConfigurations::Type ProjectConfig = SelectedProfile->GetBuildConfiguration();

            GenerateDSYMForProject(ProjectName, EBuildConfigurations::ToString(ProjectConfig));
        }
    }

    return FReply::Handled();
}


bool SProjectLauncherBuildPage::HandleGenDSYMButtonEnabled() const
{
    ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

    if (SelectedProfile.IsValid())
    {
        if (!SelectedProfile->HasValidationError(ELauncherProfileValidationErrors::NoProjectSelected))
        {
            return true;
        }
    }

    return false;
}


EVisibility SProjectLauncherBuildPage::ShowBuildConfiguration() const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid() && SelectedProfile->IsBuilding())
	{
		return EVisibility::Visible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}


void SProjectLauncherBuildPage::HandleBuildConfigurationSelectorConfigurationSelected(EBuildConfigurations::Type Configuration)
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		SelectedProfile->SetBuildConfiguration(Configuration);
	}
}


FText SProjectLauncherBuildPage::HandleBuildConfigurationSelectorText() const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		return FText::FromString(EBuildConfigurations::ToString(SelectedProfile->GetBuildConfiguration()));
	}

	return FText::GetEmpty();
}


EVisibility SProjectLauncherBuildPage::HandleValidationErrorIconVisibility(ELauncherProfileValidationErrors::Type Error) const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		if (SelectedProfile->HasValidationError(Error))
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Hidden;
}

void SProjectLauncherBuildPage::HandleUATCheckedStateChanged(ECheckBoxState CheckState)
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		SelectedProfile->SetBuildUAT(CheckState == ECheckBoxState::Checked);
	}
}


ECheckBoxState SProjectLauncherBuildPage::HandleUATIsChecked() const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		if (SelectedProfile->IsBuildingUAT())
		{
			return ECheckBoxState::Checked;
		}
	}

	return ECheckBoxState::Unchecked;
}


#undef LOCTEXT_NAMESPACE
