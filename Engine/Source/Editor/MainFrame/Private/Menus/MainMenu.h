// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Framework/Docking/TabManager.h"
#include "ILocalizationDashboardModule.h"

class FMenuBuilder;

/**
 * Unreal editor main frame Slate widget
 */
class FMainMenu
{
public:

	/**
	 * Static: Creates a widget for the main menu bar.
	 *
	 * @param TabManager Create the workspace menu based on this tab manager.
	 * @param Extender Extensibility support for the menu.
	 * @return New widget
	 */
	static TSharedRef<SWidget> MakeMainMenu( const TSharedPtr<FTabManager>& TabManager, const TSharedRef<FExtender> Extender );

	/**
	 * Static: Creates a widget for the main tab's menu bar.  This is just like the main menu bar, but also includes.
	 * some "project level" menu items that we don't want propagated to most normal menus.
	 *
	 * @param TabManager Create the workspace menu based on this tab manager.
	 * @param UserExtender Extensibility support for the menu.
	 * @return New widget
	 */
	static TSharedRef<SWidget> MakeMainTabMenu( const TSharedPtr<FTabManager>& TabManager, const TSharedRef<FExtender> UserExtender );

protected:

	/**
	 * Called to fill the file menu's content.
	 *
	 * @param MenuBuilder The multi-box builder that should be filled with content for this pull-down menu.
	 * @param Extender Extensibility support for this menu.
	 */
	static void FillFileMenu( FMenuBuilder& MenuBuilder, const TSharedRef<FExtender> Extender );

	/**
	 * Called to fill the edit menu's content.
	 *
	 * @param MenuBuilder The multi-box builder that should be filled with content for this pull-down menu.
	 * @param Extender Extensibility support for this menu.
	 * @param TabManager A Tab Manager from which to populate tab spawner menu items.
	 */
	static void FillEditMenu( FMenuBuilder& MenuBuilder, const TSharedRef<FExtender> Extender, const TSharedPtr<FTabManager> TabManager );

	/**
	 * Called to fill the app menu's content.
	 *
	 * @param MenuBuilder The multi-box builder that should be filled with content for this pull-down menu.
	 * @param Extender Extensibility support for this menu.
	 * @param TabManager A Tab Manager from which to populate tab spawner menu items.
	 */
	static void FillWindowMenu( FMenuBuilder& MenuBuilder, const TSharedRef<FExtender> Extender, const TSharedPtr<FTabManager> TabManager );

	/**
	 * Called to fill the help menu's content.
	 *
	 * @param MenuBuilder The multi-box builder that should be filled with content for this pull-down menu.
	 * @param Extender Extensibility support for this menu.
	 */
	static void FillHelpMenu( FMenuBuilder& MenuBuilder, const TSharedRef<FExtender> Extender );

private:
	/** 
	* Opens the experimental project launcher tab.
	* Remove this when it is is no longer experimental.
	*/
	static void OpenProjectLauncher()
	{
		FGlobalTabmanager::Get()->InvokeTab(FName(TEXT("ProjectLauncher")));
	}

	/**
	* Opens the experimental localization dashboard.
	* Remove this when it is no longer experimental.
	*/
	static void OpenLocalizationDashboard()
	{
		FModuleManager::LoadModuleChecked<ILocalizationDashboardModule>("LocalizationDashboard").Show();
	}

	/**
	* Opens the experimental blutility shelf tab.
	* Remove this when it is no longer experimental.
	*/
	static void OpenBlutilityShelf()
	{
		FGlobalTabmanager::Get()->InvokeTab(FName(TEXT("BlutilityShelfApp")));
	}

	/**
	* Opens the experimental Visual Logger tab.
	* Remove this when it is no longer experimental.
	*/
	static void OpenVisualLogger()
	{
		FModuleManager::Get().LoadModuleChecked<IModuleInterface>("LogVisualizer");
		FGlobalTabmanager::Get()->InvokeTab(FName(TEXT("VisualLogger")));
	}
		
	/**
	* Opens the 'Device Output Log' tab.
	* Remove this when it is no longer experimental.
	*/
	static void OpenDeviceOutputLog()
	{
		FGlobalTabmanager::Get()->InvokeTab(FName(TEXT("DeviceOutputLog")));
	}
};
