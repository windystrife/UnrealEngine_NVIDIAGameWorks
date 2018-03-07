// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SGameProjectDialog.h"
#include "Misc/MessageDialog.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Styling/CoreStyle.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "GameProjectUtils.h"
#include "SProjectBrowser.h"
#include "SNewProjectWizard.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

#define LOCTEXT_NAMESPACE "GameProjectGeneration"


/* SGameProjectDialog interface
*****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SGameProjectDialog::Construct( const FArguments& InArgs )
{
	bool bAtLeastOneVisibleRecentProject = false;

	if (InArgs._AllowProjectCreate)
	{
		NewProjectWizard = SNew(SNewProjectWizard);
	}

	if (InArgs._AllowProjectOpening)
	{
		ProjectBrowser = SNew(SProjectBrowser);
	}

	FadeAnimation.AddCurve( 0.f, 0.5f, ECurveEaseFunction::QuadOut );
	RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateSP( this, &SGameProjectDialog::TriggerFadeInPostConstruct ) );

	TSharedPtr<SWidget> Content;

	if (InArgs._AllowProjectCreate && InArgs._AllowProjectOpening)
	{
		// Create the Open Project tab button
		TSharedRef<SButton> ProjectsTabButton = SNew(SButton)
			.ForegroundColor(FCoreStyle::Get().GetSlateColor("Foreground"))
			.ButtonStyle(FEditorStyle::Get(), TEXT("NoBorder"))
			.OnClicked(this, &SGameProjectDialog::HandleProjectsTabButtonClicked)
			.ContentPadding(FMargin(40, 5))
			.Text(LOCTEXT("ProjectsTabTitle", "Projects"))
			.TextStyle(FEditorStyle::Get(), TEXT("ProjectBrowser.Tab.Text"));

		// Create the New Project tab button
		TSharedRef<SButton> NewProjectTabButton = SNew(SButton)
			.ForegroundColor(FCoreStyle::Get().GetSlateColor("Foreground"))
			.ButtonStyle(FEditorStyle::Get(), "NoBorder")
			.OnClicked(this, &SGameProjectDialog::HandleNewProjectTabButtonClicked)
			.ContentPadding(FMargin(20, 5))
			.TextStyle(FEditorStyle::Get(), "ProjectBrowser.Tab.Text")
			.Text(LOCTEXT("NewProjectTabTitle", "New Project"))
			.ToolTip(IDocumentation::Get()->CreateToolTip(LOCTEXT("NewProjectTabTitle", "New Project"), nullptr, "Shared/LevelEditor", "NewProjectTab"));

		// Allow creation and opening, so we need tabs here
		ChildSlot
		[
		
			SNew(SBorder)
			.ColorAndOpacity(this, &SGameProjectDialog::HandleCustomContentColorAndOpacity)
			.BorderImage(FEditorStyle::GetBrush("Docking.Tab.ContentAreaBrush"))
			.Padding(0)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(6.f, 0, 0, 0))
				[
					SNew(SHorizontalBox)

					// Open Project Tab
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew( SBorder )
						.BorderImage(this, &SGameProjectDialog::OnGetTabBorderImage, ProjectsTab)
						.Padding(0)
						[
							SNew(SOverlay)

							+ SOverlay::Slot()
							.VAlign(VAlign_Top)
							[
								SNew(SBox)
								.HeightOverride(2.0f)
								[
									SNew(SImage)
									.Image(this, &SGameProjectDialog::OnGetTabHeaderImage, ProjectsTab, ProjectsTabButton)
									.Visibility(EVisibility::HitTestInvisible)
								]
							]

							+ SOverlay::Slot()
							[
								ProjectsTabButton
							]
						]
					]

					+ SHorizontalBox::Slot()
					.Padding(FMargin(6.f, 0, 0, 0))
					.AutoWidth()
					[
						SNew(SBorder)
						.BorderImage(this, &SGameProjectDialog::OnGetTabBorderImage, NewProjectTab)
						.Padding(0)
						[
							SNew(SOverlay)

							+ SOverlay::Slot()
							.VAlign(VAlign_Top)
							[
								SNew(SBox)
								.HeightOverride(2.0f)
								[
									SNew(SImage)
									.Image(this, &SGameProjectDialog::OnGetTabHeaderImage, NewProjectTab, NewProjectTabButton)
									.Visibility(EVisibility::HitTestInvisible)
								]
							]

							+ SOverlay::Slot()
							[
								NewProjectTabButton
							]
						]
					]
				]

				+ SVerticalBox::Slot()
				[
					SAssignNew(ContentAreaSwitcher, SWidgetSwitcher)
					.WidgetIndex(0)

					+ SWidgetSwitcher::Slot()
					[
						ProjectBrowser.ToSharedRef()
					]

					+ SWidgetSwitcher::Slot()
					[
						NewProjectWizard.ToSharedRef()
					]
				]
			]
		];
	}
	else
	{
		TSharedPtr<SWidget> BorderContent;
		if (NewProjectWizard.IsValid())
		{
			BorderContent = NewProjectWizard;
		}
		else
		{
			BorderContent = ProjectBrowser;
		}

		ChildSlot
		[
			BorderContent.ToSharedRef()
		];
	}

	ActiveTab = InArgs._AllowProjectOpening ? ProjectsTab : NewProjectTab;
	if (ProjectBrowser.IsValid())
	{
		ActiveTab = !ProjectBrowser->HasProjects() && InArgs._AllowProjectCreate ? NewProjectTab : ActiveTab;
	}

	if (ActiveTab == ProjectsTab)
	{
		ShowProjectBrowser();
	}
	else if (ActiveTab == NewProjectTab)
	{
		ShowNewProjectTab();
	}
	else
	{
		check(false);
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


EActiveTimerReturnType SGameProjectDialog::TriggerFadeInPostConstruct(double InCurrentTime, float InDeltaTime)
{
	// Play the intro fade in the first frame after the widget is created.
	// We start it now instead of Construct because there is a lot of elapsed time between Construct and when we
	// see the dialog and the beginning of the animation is cut off.
	FadeAnimation.Play(this->AsShared());

	return EActiveTimerReturnType::Stop;
}

/* SGameProjectDialog implementation
*****************************************************************************/

bool SGameProjectDialog::OpenProject( const FString& ProjectFile )
{
	FText FailReason;

	if (GameProjectUtils::OpenProject(ProjectFile, FailReason))
	{
		return true;
	}

	FMessageDialog::Open(EAppMsgType::Ok, FailReason);

	return false;
}


void SGameProjectDialog::ShowNewProjectTab( )
{
	if (ContentAreaSwitcher.IsValid() && NewProjectWizard.IsValid())
	{
		ContentAreaSwitcher->SetActiveWidget( NewProjectWizard.ToSharedRef());
		ActiveTab = NewProjectTab;
	}
}


FReply SGameProjectDialog::ShowProjectBrowser( )
{
	if (ContentAreaSwitcher.IsValid() && ProjectBrowser.IsValid())
	{
		ContentAreaSwitcher->SetActiveWidget(ProjectBrowser.ToSharedRef());
		ActiveTab = ProjectsTab;
	}

	return FReply::Handled();
}


/* SGameProjectDialog callbacks
*****************************************************************************/

FLinearColor SGameProjectDialog::HandleCustomContentColorAndOpacity( ) const
{
	return FLinearColor(1,1,1, FadeAnimation.GetLerp());
}


FReply SGameProjectDialog::HandleNewProjectTabButtonClicked( )
{
	ShowNewProjectTab();

	return FReply::Handled();
}


const FSlateBrush* SGameProjectDialog::OnGetTabHeaderImage( ETab InTab, TSharedRef<SButton> TabButton ) const
{
	if (TabButton->IsPressed())
	{
		return FEditorStyle::GetBrush("ProjectBrowser.Tab.PressedHighlight");
	}

	if ((ActiveTab == InTab) || TabButton->IsHovered())
	{
		return FEditorStyle::GetBrush("ProjectBrowser.Tab.ActiveHighlight");
	}
	
	return FEditorStyle::GetBrush("ProjectBrowser.Tab.Highlight");
}


FReply SGameProjectDialog::HandleProjectsTabButtonClicked( )
{
	ShowProjectBrowser();

	return FReply::Handled();
}

const FSlateBrush* SGameProjectDialog::OnGetTabBorderImage( ETab InTab ) const
{
	if (ActiveTab == InTab)
	{
		return FEditorStyle::GetBrush("ProjectBrowser.Tab.ActiveBackground");
	}
	
	return FEditorStyle::GetBrush("ProjectBrowser.Tab.Background");
}


#undef LOCTEXT_NAMESPACE
