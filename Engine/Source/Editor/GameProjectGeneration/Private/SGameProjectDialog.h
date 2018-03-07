// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Animation/CurveSequence.h"
#include "Widgets/SCompoundWidget.h"

class SButton;
class SNewProjectWizard;
class SProjectBrowser;
struct FSlateBrush;

/**
 * A dialog to create a new project or open an existing one
 */
class SGameProjectDialog
	: public SCompoundWidget
{
	// Enumerates available tabs.
	enum ETab
	{
		ProjectsTab,
		NewProjectTab
	};

public:

	SLATE_BEGIN_ARGS(SGameProjectDialog) { }
		SLATE_ARGUMENT(bool, AllowProjectOpening)
		SLATE_ARGUMENT(bool, AllowProjectCreate)
	SLATE_END_ARGS()

public:

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

protected:
	/**
	 * Opens the specified project.
	 *
	 * @param ProjectFile - The project file to open.
	 *
	 * @return true if the project was opened, false otherwise.
	 */
	bool OpenProject( const FString& ProjectFile );

	/**
	 * Shows the 'New Project' tab.
	 */
	void ShowNewProjectTab( );

	/**
	 * Shows the project browser tab.
	 */
	FReply ShowProjectBrowser( );

private:
	/** Ensures the fade-in animation is played post-construct */
	EActiveTimerReturnType TriggerFadeInPostConstruct( double InCurrentTime, float InDeltaTime );

	// Callback for getting the color of the custom content area.
	FLinearColor HandleCustomContentColorAndOpacity() const;

	// Callback for clicking the 'New Project' button.
	FReply HandleNewProjectTabButtonClicked( );

	// Callback for clicking the 'Projects' button.
	FReply HandleProjectsTabButtonClicked( );

	// Callback for getting the border image of the specified tab.
	const FSlateBrush* OnGetTabBorderImage( ETab InTab ) const;

	// Callback for getting the header stripe image for the specified tab.
	const FSlateBrush* OnGetTabHeaderImage( ETab InTab, TSharedRef<SButton> TabButton ) const;

private:

	// Holds the fading animation.
	FCurveSequence FadeAnimation;

	/** The switcher widget to control which screen is in view */
	TSharedPtr<SWidgetSwitcher> ContentAreaSwitcher;
	TSharedPtr<SProjectBrowser> ProjectBrowser;
	TSharedPtr<SNewProjectWizard> NewProjectWizard;

	ETab ActiveTab;
};
