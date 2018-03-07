// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Engine/World.h"
#include "Widgets/SWindow.h"
#include "SLevelViewport.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "Toolkits/IToolkit.h"
#include "AssetThumbnail.h"
#include "ILevelEditor.h"
#include "LevelViewportTabContent.h"

class IAssetEditorInstance;
class IDetailsView;
class SActorDetails;
class SBorder;
class SLevelEditorModeContent;
class SLevelEditorToolBox;

/**
 * Unreal editor level editor Slate widget
 */
class SLevelEditor
	: public ILevelEditor
{

public:
	SLATE_BEGIN_ARGS( SLevelEditor ){}

	SLATE_END_ARGS()

	/**
	 * Constructor
	 */
	SLevelEditor();

	~SLevelEditor();

	/**
	 * Constructs this widget
	 *
	 * @param InArgs    Declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs );

	/**
	 * Initialize the newly constructed level editor UI, needed because restoring the layout could trigger showing tabs
	 * that immediately try to get a reference to the current level editor.
	 */
	void Initialize( const TSharedRef<SDockTab>& OwnerTab, const TSharedRef<SWindow>& OwnerWindow );

	/**
	 * Gets the currently active viewport in the level editor
	 * @todo Slate: Needs a better implementation 
	 *
	 * @return The active viewport.  If multiple are active it returns the first one               
	 */
	TSharedPtr<class SLevelViewport> GetActiveViewport();

	/** ILevelEditor interface */
	virtual void SummonLevelViewportContextMenu() override;
	virtual void SummonLevelViewportViewOptionMenu(const ELevelViewportType ViewOption) override;
	virtual const TArray< TSharedPtr< class IToolkit > >& GetHostedToolkits() const override;
	virtual TArray< TSharedPtr< ILevelViewport > > GetViewports() const override;
	virtual TSharedPtr<ILevelViewport> GetActiveViewportInterface() override;
	virtual TSharedPtr< class FAssetThumbnailPool > GetThumbnailPool() const override;
	virtual void AppendCommands( const TSharedRef<FUICommandList>& InCommandsToAppend ) override;
	virtual void AddStandaloneLevelViewport( const TSharedRef<SLevelViewport>& LevelViewport ) override;

	/**
	 * Given a tab ID, summons a new tab in the position saved in the current layout, or in a default position.
	 * @return the invoked tab
	 */
	TSharedRef<SDockTab> InvokeTab( FName TabID );

	/**
	 * Sync the details panel to the current selection
	 * Will spawn a new details window if required (and possible) due to other details windows being locked
	 */
	void SyncDetailsToSelection();

	/**
	 * @return true if the level editor has a viewport currently being used for pie
	 */
	bool HasActivePlayInEditorViewport() const;

	/** @return	Returns the title to display in the level editor's tab label */
	FText GetTabTitle() const;

	/**
	 * Processes level editor keybindings using events made in a viewport
	 * 
	 * @param MyGeometry		Information about the size and position of the viewport widget
	 * @param InKeyEvent	The event which just occurred	
	 */
	virtual FReply OnKeyDownInViewport( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;

	bool CanCloseApp();

	/** Returns the full action list for this level editor instance */
	virtual const TSharedPtr< FUICommandList >& GetLevelEditorActions() const override
	{
		return LevelEditorCommands;
	}

	/** IToolKitHost interface */
	virtual TSharedRef< class SWidget > GetParentWidget() override;
	virtual void BringToFront() override;
	virtual TSharedRef< class SDockTabStack > GetTabSpot( const EToolkitTabSpot::Type TabSpot ) override;
	virtual TSharedPtr<FTabManager> GetTabManager() const override;
	virtual void OnToolkitHostingStarted( const TSharedRef< class IToolkit >& Toolkit ) override;
	virtual void OnToolkitHostingFinished( const TSharedRef< class IToolkit >& Toolkit ) override;
	virtual UWorld* GetWorld() const override;
	virtual TSharedRef<SWidget> CreateActorDetails( const FName TabIdentifier ) override;
	virtual TSharedRef<SWidget> CreateToolBox() override;

	/** SWidget overrides */
	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}
	
	/** Attaches a sequencer asset editor used to animate objects in the level to this level editor */
	void AttachSequencer( TSharedPtr<SWidget> SequencerWidget, TSharedPtr<IAssetEditorInstance> NewSequencerAssetEditor );
private:
	
	TSharedRef<SDockTab> SpawnLevelEditorTab(const FSpawnTabArgs& Args, FName TabIdentifier, FString InitializationPayload);
	//TSharedRef<SDockTab> SpawnLevelEditorModeTab(const FSpawnTabArgs& Args, FEdMode* EditorMode);
	TSharedRef<SDockTab> SummonDetailsPanel( FName Identifier );

	/**
	 * Binds UI commands to actions for the level editor                   
	 */
	void BindCommands();

	/**
	 * Fills the level editor with content, using the layout string, or the default if
	 * no layout string is passed in
	 */
	TSharedRef<SWidget> RestoreContentArea( const TSharedRef<SDockTab>& OwnerTab, const TSharedRef<SWindow>& OwnerWindow );

	/** Called when a property is changed */
	void HandleExperimentalSettingChanged(FName PropertyName);

	/** Rebuilds the command list for spawning editor modes, this is done when new modes are registered. */
	void RefreshEditorModeCommands();

	/** Gets the tabId mapping to an editor mode */
	static FName GetEditorModeTabId( FEditorModeID ModeID );

	/** Toggles the editor mode on and off, this is what the auto generated editor mode commands are mapped to. */
	static void ToggleEditorMode( FEditorModeID ModeID );

	/** Checks if the editor mode is active for the auto-generated editor mode command. */
	static bool IsModeActive( FEditorModeID ModeID );

	/**
	 * Processes keybindings on the level editor
	 * 
	 * @param MyGeometry		Information about the size and position of the level editor widget
	 * @param InKeyEvent	The event which just occurred	
	 */
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;

	/** Callback for when the property view changes */
	void OnPropertyObjectArrayChanged(const FString& NewTitle, const TArray< UObject* >& UObjects );

	/** Callback for when the level editor layout has changed */
	void OnLayoutHasChanged();
	
	/** Constructs the NotificationBar widgets */
	void ConstructNotificationBar();

	/** Builds a viewport tab. */
	TSharedRef<SDockTab> BuildViewportTab( const FText& Label, const FString LayoutId, const FString& InitializationPayload );

	/** Called when a viewport tab is closed */
	void OnViewportTabClosed(TSharedRef<SDockTab> ClosedTab);

	/** Save the information about the given viewport in the transient viewport information */
	void SaveViewportTabInfo(TSharedRef<const class FLevelViewportTabContent> ViewportTabContent);

	/** Restore the information about the given viewport from the transient viewport information */
	void RestoreViewportTabInfo(TSharedRef<FLevelViewportTabContent> ViewportTabContent) const;

	/** Reset the transient viewport information */
	void ResetViewportTabInfo();

	/** Handles Editor map changes */
	void HandleEditorMapChange( uint32 MapChangeFlags );

	/** Called when actors are selected or unselected */
	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh = false);
private:

	// Tracking the active viewports in this level editor.
	TArray< TWeakPtr<FLevelViewportTabContent> > ViewportTabs;

	// A list of any standalone editor viewports that aren't in tabs
	TArray< TWeakPtr<SLevelViewport> > StandaloneViewports;

	// Border that hosts the document content for the level editor.
	TSharedPtr< SBorder > DocumentsAreaBorder;
	
	// The list of commands with bound delegates for the level editor.
	TSharedPtr<FUICommandList> LevelEditorCommands;

	// Weak reference to all toolbox panels this level editor has spawned.  May contain invalid entries for tabs that were closed.
	TArray< TWeakPtr< class SLevelEditorToolBox > > ToolBoxTabs;

	TArray< TWeakPtr< class SLevelEditorModeContent > > ModesTabs;

	// List of all of the toolkits we're currently hosting.
	TArray< TSharedPtr< class IToolkit > > HostedToolkits;

	// The UWorld that this level editor is viewing and allowing the user to interact with through.
	UWorld* World;

	// The box that holds the notification bar.
	TSharedPtr< SHorizontalBox > NotificationBarBox;

	// Holds the world settings details view.
	TSharedPtr<IDetailsView> WorldSettingsView;

	// The thumbnail pool used to display asset thumbnails
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;

	/** Transient editor viewport states - one for each view type. Key is "LayoutId[ELevelViewportType]", eg) "Viewport 1[0]" */
	TMap<FString, FLevelViewportInfo> TransientEditorViews;

	/** List of all actor details panels to update when selection changes */
	TArray< TWeakPtr<class SActorDetails> > AllActorDetailPanels;

	/** Attached sequencer asset editor */
	TWeakPtr<IAssetEditorInstance> SequencerAssetEditor;

	/** Weak pointer to the level editor's Sequencer widget */
	TWeakPtr<SWidget> SequencerWidgetPtr;
};
