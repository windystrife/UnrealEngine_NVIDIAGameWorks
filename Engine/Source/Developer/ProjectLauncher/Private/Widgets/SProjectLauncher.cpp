// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SProjectLauncher.h"

#include "EditorStyleSet.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SlateOptMacros.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"

#include "Models/ProjectLauncherCommands.h"
#include "Widgets/Deploy/SProjectLauncherSimpleDeviceListView.h"
#include "Widgets/Profile/SProjectLauncherProfileListView.h"
#include "Widgets/Progress/SProjectLauncherProgress.h"
#include "Widgets/Project/SProjectLauncherProjectPicker.h"
#include "Widgets/Settings/SProjectLauncherSettings.h"


#define LOCTEXT_NAMESPACE "SProjectLauncher"


/* SProjectLauncher structors
 *****************************************************************************/

SProjectLauncher::SProjectLauncher()
	:bAdvanced(false)
{
	if (GConfig != NULL)
	{
		GConfig->GetBool(TEXT("FProjectLauncher"), TEXT("AdvancedMode"), bAdvanced, GEngineIni);
	}
}


SProjectLauncher::~SProjectLauncher()
{
	if (GConfig != NULL)
	{
		GConfig->SetBool(TEXT("FProjectLauncher"), TEXT("AdvancedMode"), bAdvanced, GEngineIni);
	}

	if (LauncherWorker.IsValid())
	{
		LauncherWorker->Cancel();
		FPlatformProcess::Sleep(0.5f);
	}
}


/* SProjectLauncher interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SProjectLauncher::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow, const TSharedRef<FProjectLauncherModel>& InModel)
{
	FProjectLauncherCommands::Register();

	Model = InModel;

	// create & initialize main menu bar
	TSharedRef<FWorkspaceItem> RootMenuGroup = FWorkspaceItem::NewGroup(LOCTEXT("RootMenuGroup", "Root"));

	FMenuBarBuilder MenuBarBuilder = FMenuBarBuilder(TSharedPtr<FUICommandList>());
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("WindowMenuLabel", "Window"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateStatic(&SProjectLauncher::FillWindowMenu, RootMenuGroup),
		"Window"
	);

	ChildSlot
	[
		SAssignNew(WidgetSwitcher, SWidgetSwitcher)
		.WidgetIndex((int32)ELauncherPanels::Launch)
		
		// Empty Panel
		+ SWidgetSwitcher::Slot()
		[
			SNew(SBorder)
		]

		// SProjectLauncher Panel
		+ SWidgetSwitcher::Slot()
		[
			SNew(SSplitter)
			.Style(FEditorStyle::Get(), "ContentBrowser.Splitter")
			.Orientation(Orient_Vertical)

			// Simple SProjectLauncher
			+ SSplitter::Slot()
			.Value(0.7f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2)
				[
					SNew(SHorizontalBox)
					
					// Project Bar
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SProjectLauncherProjectPicker, InModel)
					]

					// Advanced Button
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					[
						SNew(SBorder)
						.Padding(2)
						.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
						[
							SNew(SCheckBox)
							.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
							.IsFocusable(true)
							.ToolTipText(LOCTEXT("ToggleAdvancedOptionsToolTipText", "Toggles Advanced Options"))
							.OnCheckStateChanged(this, &SProjectLauncher::OnAdvancedChanged)
							.IsChecked(this, &SProjectLauncher::OnIsAdvanced)
							[
								SNew(SHorizontalBox)

								// Icon
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								[
									SNew(SImage)
									.Image(this, &SProjectLauncher::GetAdvancedToggleBrush)
								]

								// Text
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(4,0,4,0)
								[
									SNew(STextBlock)
									.TextStyle(FCoreStyle::Get(), "Toolbar.Label")
									.ShadowOffset(FVector2D::UnitVector)
									.Text(LOCTEXT("AdvancedButton", "Advanced"))
								]
							]
						]
					]
				]

				+ SVerticalBox::Slot()
				.FillHeight(1)
				.Padding(2)
				[
					SNew(SBorder)
					[
						SAssignNew(LaunchList, SProjectLauncherSimpleDeviceListView, InModel)
						.OnProfileRun(this, &SProjectLauncher::OnProfileRun)
						.IsAdvanced(this, &SProjectLauncher::GetIsAdvanced)
					]
				]
			]

			+ SSplitter::Slot()
			.Value(0.3f)
			[
				SNew(SBorder)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2)
					[
						SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
						.Padding(4.0)
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								SNew(STextBlock)
								.TextStyle(FCoreStyle::Get(), "Toolbar.Label")
								.ShadowOffset(FVector2D::UnitVector)
								.Text(LOCTEXT("ProjectLauncherCustomProfilesTitle", "Custom Launch Profiles"))
							]

							+ SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							.AutoWidth()
							[
								SNew(SComboButton)
								.ComboButtonStyle(FEditorStyle::Get(), "GenericFilters.ComboButtonStyle")
								.ForegroundColor(FLinearColor::White)
								.ContentPadding(0)
								.ToolTipText(LOCTEXT("AddFilterToolTip", "Add a new custom launch profile using wizard"))
								.OnGetMenuContent(this, &SProjectLauncher::MakeProfileWizardsMenu)
								.HasDownArrow(true)
								.ContentPadding(FMargin(1, 0))
								.Visibility(this, &SProjectLauncher::GetProfileWizardsMenuVisibility)
							]

							+ SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							.AutoWidth()
							[
								SNew(SButton)
								.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
								.ToolTipText(LOCTEXT("ProjectLauncherCustomProfileAdd", "Add a new custom launch profile."))
								.ContentPadding(0)
								.OnClicked(this, &SProjectLauncher::OnAddCustomLaunchProfileClicked)
								[
									SNew(SImage)
									.Image(FCoreStyle::Get().GetBrush("EditableComboBox.Add"))
									.ColorAndOpacity(FSlateColor(FLinearColor::White))
								]
							]
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(1)
					.Padding(2)
					[
						SAssignNew(ProfileList, SBorder)
						.BorderImage(FEditorStyle::GetBrush("NoBorder"))
						.Padding(0)
						[
							// Simple Launch List
							SNew(SProjectLauncherProfileListView, InModel)
							.OnProfileEdit(this, &SProjectLauncher::OnProfileEdit)
							.OnProfileRun(this, &SProjectLauncher::OnProfileRun)
							.OnProfileDelete(this, &SProjectLauncher::OnProfileDelete)
						]
					]
				]
			]
		]
		
		// Launch Settings
		+ SWidgetSwitcher::Slot()
		[
			SAssignNew(ProfileSettingsPanel, SProjectLauncherSettings, InModel)
			.OnCloseClicked(this, &SProjectLauncher::OnProfileSettingsClose)
			.OnDeleteClicked(this, &SProjectLauncher::OnProfileDelete)
		]

		// Progress Panel
		+ SWidgetSwitcher::Slot()
		[
			SAssignNew(ProgressPanel, SProjectLauncherProgress)
			.OnCloseClicked(this, &SProjectLauncher::OnProgressClose)
			.OnRerunClicked(this, &SProjectLauncher::OnRerunClicked)
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


/* SProjectLauncher implementation
*****************************************************************************/

void SProjectLauncher::FillWindowMenu(FMenuBuilder& MenuBuilder, TSharedRef<FWorkspaceItem> RootMenuGroup)
{
#if !WITH_EDITOR
	MenuBuilder.BeginSection("WindowGlobalTabSpawners", LOCTEXT("UfeMenuGroup", "Unreal Frontend"));
	{
		FGlobalTabmanager::Get()->PopulateTabSpawnerMenu(MenuBuilder, RootMenuGroup);
	}
	MenuBuilder.EndSection();
#endif //!WITH_EDITOR
}


/* SProjectLauncher callbacks
*****************************************************************************/

void SProjectLauncher::OnAdvancedChanged(const ECheckBoxState NewCheckedState)
{
	bAdvanced = (NewCheckedState == ECheckBoxState::Checked);
}


ECheckBoxState SProjectLauncher::OnIsAdvanced() const
{
	return (bAdvanced) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


const FSlateBrush* SProjectLauncher::GetAdvancedToggleBrush() const
{
	return FEditorStyle::GetBrush("LauncherCommand.AdvancedBuild.Medium");
}


bool SProjectLauncher::GetIsAdvanced() const
{
	return bAdvanced;
}


void SProjectLauncher::OnProfileEdit(const ILauncherProfileRef& Profile)
{
	Model->SelectProfile(Profile);
	WidgetSwitcher->SetActiveWidgetIndex(ELauncherPanels::ProfileEditor);
}


void SProjectLauncher::OnProfileRun(const ILauncherProfileRef& Profile)
{
	LauncherProfile = Profile;
	LauncherWorker = Model->GetSProjectLauncher()->Launch(Model->GetDeviceProxyManager(), Profile);
	
	if (LauncherWorker.IsValid())
	{
		ProgressPanel->SetLauncherWorker(LauncherWorker.ToSharedRef());
		WidgetSwitcher->SetActiveWidgetIndex(ELauncherPanels::Progress);
	}
}


void SProjectLauncher::OnProfileDelete(const ILauncherProfileRef& Profile)
{
	Model->GetProfileManager()->RemoveProfile(Profile);
}


FReply SProjectLauncher::OnAddCustomLaunchProfileClicked()
{
	ILauncherProfileRef Profile = Model->GetProfileManager()->AddNewProfile();
	
	OnProfileEdit(Profile);

	ProfileSettingsPanel->EnterEditMode();

	return FReply::Handled();
}


EVisibility SProjectLauncher::GetProfileWizardsMenuVisibility() const
{
	return (Model->GetProfileManager()->GetProfileWizards().Num() > 0) ? EVisibility::Visible : EVisibility::Collapsed;
}


TSharedRef<SWidget> SProjectLauncher::MakeProfileWizardsMenu()
{
	FMenuBuilder MenuBuilder(true, NULL);

	const TArray<ILauncherProfileWizardPtr>& Wizards = Model->GetProfileManager()->GetProfileWizards();
	for (const ILauncherProfileWizardPtr& Wizard : Wizards)
	{
		FText WizardName = Wizard->GetName();
		FText WizardDescription = Wizard->GetDescription();
				
		MenuBuilder.AddMenuEntry(
			WizardName,
			WizardDescription,
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SProjectLauncher::ExecProfileWizard, Wizard),
				FCanExecuteAction()
				)
			);
	}

	return MenuBuilder.MakeWidget();
}


void SProjectLauncher::ExecProfileWizard(ILauncherProfileWizardPtr InWizard)
{
	InWizard->HandleCreateLauncherProfile(Model->GetProfileManager());
}


FReply SProjectLauncher::OnProfileSettingsClose()
{
	if (LauncherWorker.IsValid())
	{
		LauncherWorker->Cancel();
	}
	LauncherProfile.Reset();

	WidgetSwitcher->SetActiveWidgetIndex(ELauncherPanels::Launch);

	return FReply::Handled();
}


FReply SProjectLauncher::OnProgressClose()
{
	if (LauncherWorker.IsValid())
	{
		LauncherWorker->Cancel();
	}
	LauncherProfile.Reset();

	WidgetSwitcher->SetActiveWidgetIndex(ELauncherPanels::Launch);

	return FReply::Handled();
}


FReply SProjectLauncher::OnRerunClicked()
{
	if (LauncherWorker.IsValid())
	{
		LauncherWorker->Cancel();
	}
	LauncherWorker = Model->GetSProjectLauncher()->Launch(Model->GetDeviceProxyManager(), LauncherProfile.ToSharedRef());

	if (LauncherWorker.IsValid())
	{
		ProgressPanel->SetLauncherWorker(LauncherWorker.ToSharedRef());
	}

	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE
