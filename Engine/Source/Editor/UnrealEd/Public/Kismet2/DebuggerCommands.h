// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"

class FToolBarBuilder;

/** This class acts as a generic widget that listens to and process global play world actions */
class UNREALED_API SGlobalPlayWorldActions : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SGlobalPlayWorldActions) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	virtual bool SupportsKeyboardFocus() const override;
};

//////////////////////////////////////////////////////////////////////////
// FPlayWorldCommands

class FPlayWorldCommands : public TCommands<FPlayWorldCommands>
{
private:

	friend class TCommands<FPlayWorldCommands>;

	FPlayWorldCommands();

public:

	// TCommands interface
	virtual void RegisterCommands() override;
	// End of TCommands interface

	/**
	 * Binds all global kismet commands to delegates
	 */
	static void BindGlobalPlayWorldCommands();

	/** Populates a toolbar with the menu commands for play-world control (pause/resume/stop/possess/eject/step/show current loc) */
	UNREALED_API static void BuildToolbar( FToolBarBuilder& ToolBarBuilder, bool bIncludeLaunchButtonAndOptions = false );

	/**
	* Return the active widget that processes play world actions for PIE
	*
	*/
	static TWeakPtr<SGlobalPlayWorldActions> GetActiveGlobalPlayWorldActionsWidget();

	/**
	* Set the active widget that processes play world actions for PIE
	*
	*/
	static void SetActiveGlobalPlayWorldActionsWidget(TWeakPtr<SGlobalPlayWorldActions> ActiveWidget);

public:

	/** 
	 * A command list that can be passed around and isn't bound to an instance of any tool or editor. 
	 */
	UNREALED_API static TSharedPtr<FUICommandList> GlobalPlayWorldActions;

public:

	/** Start Simulating (SIE) */
	TSharedPtr<FUICommandInfo> Simulate;

	/** Play in editor (PIE) */
	TSharedPtr<FUICommandInfo> RepeatLastPlay;
	TSharedPtr<FUICommandInfo> PlayInViewport;
	TSharedPtr<FUICommandInfo> PlayInEditorFloating;
	TSharedPtr<FUICommandInfo> PlayInVR;
	TSharedPtr<FUICommandInfo> PlayInMobilePreview;
	TSharedPtr<FUICommandInfo> PlayInVulkanPreview;
	TSharedPtr<FUICommandInfo> PlayInNewProcess;
	TSharedPtr<FUICommandInfo> PlayInCameraLocation;
	TSharedPtr<FUICommandInfo> PlayInDefaultPlayerStart;
	TSharedPtr<FUICommandInfo> PlayInSettings;
	TSharedPtr<FUICommandInfo> PlayInNetworkSettings;
	TSharedPtr<FUICommandInfo> PlayInNetworkDedicatedServer;

	TArray< TSharedPtr< FUICommandInfo > > PlayInTargetedMobilePreviewDevices;

	/** SIE & PIE controls */
	TSharedPtr<FUICommandInfo> ResumePlaySession;
	TSharedPtr<FUICommandInfo> PausePlaySession;
	TSharedPtr<FUICommandInfo> SingleFrameAdvance;
	TSharedPtr<FUICommandInfo> TogglePlayPauseOfPlaySession;
	TSharedPtr<FUICommandInfo> StopPlaySession;
	TSharedPtr<FUICommandInfo> LateJoinSession;
	TSharedPtr<FUICommandInfo> PossessEjectPlayer;
	TSharedPtr<FUICommandInfo> ShowCurrentStatement;
	TSharedPtr<FUICommandInfo> StepInto;
	TSharedPtr<FUICommandInfo> StepOver;
	TSharedPtr<FUICommandInfo> StepOut;
	TSharedPtr<FUICommandInfo> GetMouseControl;

	/** Launch on device */
	TSharedPtr<FUICommandInfo> RepeatLastLaunch;
	TSharedPtr<FUICommandInfo> OpenProjectLauncher;
	TSharedPtr<FUICommandInfo> OpenDeviceManager;

protected:

	/** A weak pointer to the current active widget that processes PIE actions */
	static TWeakPtr<SGlobalPlayWorldActions> ActiveGlobalPlayWorldActionsWidget;

	/**
	 * Generates menu content for the PIE combo button drop down menu
	 *
	 * @return	Menu content widget
	 */
	static TSharedRef< SWidget > GeneratePlayMenuContent( TSharedRef<FUICommandList> InCommandList );

	/**
	 * Generates menu content for the Play On combo button drop down menu
	 *
	 * @return	Menu content widget
	 */
	static TSharedRef< SWidget > GenerateLaunchMenuContent( TSharedRef<FUICommandList> InCommandList );

	// Add mobile PIE preview device commands
	void AddPIEPreviewDeviceCommands();

	// Add mobile PIE preview device actions
	static void AddPIEPreviewDeviceActions(const FPlayWorldCommands &Commands, FUICommandList &ActionList);
};


//////////////////////////////////////////////////////////////////////////
// FPlayWorldCommandCallbacks

class UNREALED_API FPlayWorldCommandCallbacks
{
public:
	/**
	 * Called from the context menu to start previewing the game at the clicked location                   
	 */
	static void StartPlayFromHere();

	static void ResumePlaySession_Clicked();
	static void PausePlaySession_Clicked();

	static bool IsInSIE();
	static bool IsInPIE();

	static bool IsInSIE_AndRunning();
	static bool IsInPIE_AndRunning();

	static bool HasPlayWorld();
	static bool HasPlayWorldAndPaused();
	static bool HasPlayWorldAndRunning();
};
