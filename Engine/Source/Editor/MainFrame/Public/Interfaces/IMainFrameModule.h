// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWidget.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxExtender.h"

class FTabManager;
class SWindow;

/**
 * Interface for main frame modules.
 */
class IMainFrameModule
	: public IModuleInterface
{
public:

	/**
	 * Creates the default editor main frame
	 *
	 * @param bStartImmersive True to force a main frame viewport into immersive mode
	 * @param bStartPIE True to start a PIE session right away
	 */
	virtual void CreateDefaultMainFrame( const bool bStartImmersive, const bool bStartPIE ) = 0;

	/**
	 * Generates a menu that includes application global commands, such as "Save All", "Exit", etc.  If you're building
	 * a menu for your tab, you should call this function to create your menu, passing in an extender object to add your
	 * tab-specific menu items!
	 *
	 * @param	TabManager	The tab manager for the tab you're creating the menu for.  This is needed so we can populate the layout menus correctly.
	 * @param	Extender	Extender object used to customize the main frame menu
	 *
	 * @return	The newly-created menu widget
	 */
	virtual TSharedRef<SWidget> MakeMainMenu( const TSharedPtr<FTabManager>& TabManager, const TSharedRef< FExtender > Extender ) const = 0;

	/**
	 * Generates a menu that's just like the "main menu" widget above, except it also includes some infrequently used commands
	 * that are better off only shown in a single "main tab" within the application
	 *
	 * @param	TabManager	The tab manager for the tab you're creating the menu for.  This is needed so we can populate the layout menus correctly.
	 * @param	Extender	Extender object used to customize the main frame menu
	 *
	 * @return	The newly-created menu widget
	 */
	virtual TSharedRef<SWidget> MakeMainTabMenu( const TSharedPtr<FTabManager>& TabManager, const TSharedRef< FExtender > Extender ) const = 0;

	/**
	 * @todo Editor: add documentation for MakeDeveloperTools
	 */
	virtual TSharedRef<SWidget> MakeDeveloperTools( ) const = 0;

	/**
	 * Checks to see if the main frame window is currently initialized
	 *
	 * @return	True if initialized, otherwise false
	 */
	virtual bool IsWindowInitialized() const = 0;

	/**
	 * Gets the window the mainframe lives in.
	 *
	 * @return The window widget.
	 */
	virtual TSharedPtr<SWindow> GetParentWindow( ) const = 0;

	/**
	 * Sets the reference to the main tab.
	 *
	 * @param MainTab - The main tab.
	 */
	virtual void SetMainTab( const TSharedRef<SDockTab>& MainTab ) = 0;

	/**
	 * Enables the delegate responsible for shutting down the editor when the main tab is closed.
	 */
	virtual void EnableTabClosedDelegate( ) = 0;

	/**
	 * Disables the delegate responsible for shutting down the editor when the main tab is closed.
	 */
	virtual void DisableTabClosedDelegate( ) = 0;

	/**
	 * Requests that the editor be closed
	 * In some cases the editor may not be closed (like if a user cancels a save dialog)
	 */
	virtual void RequestCloseEditor( ) = 0;

	/**
	 * Updates the mainframe title on the Slate window and the native OS window underneath
	 *
	 * @param InLevelFileName  Full level filename from which the base name will be stripped and used to make the window title
	 */
	virtual void SetLevelNameForWindowTitle(const FString& InLevelFileName) = 0;

	/**
	 * Returns a friendly string name for the currently loaded persistent level.
	 *
	 * @return Name of the loaded level.
	 */
	virtual FString GetLoadedLevelName() const = 0;

	virtual const TSharedRef<FUICommandList>& GetMainFrameCommandBindings( ) = 0;

	/**
	 * Gets the MRU/Favorites list
	 *
	 * @return	MRU/Favorites list
	 */
	virtual class FMainMRUFavoritesList* GetMRUFavoritesList() const = 0;

	/**
	 * Gets the title string for the application, optionally including the current game name as part of the title
	 *
	 * @param	bIncludeGameName	True if the game name should be included as part of the returned title string
	 *
	 * @return	Returns the title of the application, to be displayed in tabs or window titles 
	 */
	virtual const FText GetApplicationTitle( const bool bIncludeGameName ) const = 0;

	/**
	 * Shows the 'About UnrealEd' window.
	 */
	virtual void ShowAboutWindow( ) const = 0;

	/**
	 * Delegate for binding functions to be called when the mainframe finishes up getting created.
	 */
	DECLARE_EVENT_TwoParams(IMainFrameModule, FMainFrameCreationFinishedEvent, TSharedPtr<SWindow>, bool);
	virtual FMainFrameCreationFinishedEvent& OnMainFrameCreationFinished( ) = 0;

	/**
	 * Delegate for when a platform SDK isn't installed corrected (takes the platform name and the documentation link to show)
	 */
	DECLARE_EVENT_TwoParams(IMainFrameModule, FMainFrameSDKNotInstalled, const FString&, const FString&);
	virtual FMainFrameSDKNotInstalled& OnMainFrameSDKNotInstalled( ) = 0;
	virtual void BroadcastMainFrameSDKNotInstalled(const FString& PlatformName, const FString& DocLink) = 0;

public:

	/**
	 * Gets a reference to the search module instance.
	 *
	 * @todo gmp: better implementation using dependency injection.
	 * @return A reference to the MainFrame module.
	 */
	static IMainFrameModule& Get( )
	{
		static const FName MainFrameModuleName = "MainFrame";
		return FModuleManager::LoadModuleChecked<IMainFrameModule>(MainFrameModuleName);
	}

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~IMainFrameModule( ) { }
};
