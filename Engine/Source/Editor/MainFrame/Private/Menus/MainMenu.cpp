// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Menus/MainMenu.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Interfaces/IMainFrameModule.h"
#include "DesktopPlatformModule.h"
#include "ISourceControlModule.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/GlobalEditorCommonCommands.h"
#include "SourceCodeNavigation.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "EditorStyleSet.h"
#include "EditorStyleSettings.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/EditorExperimentalSettings.h"
#include "UnrealEdGlobals.h"
#include "Frame/MainFrameActions.h"
#include "Menus/PackageProjectMenu.h"
#include "Menus/RecentProjectsMenu.h"
#include "Menus/SettingsMenu.h"
#include "Menus/MainFrameTranslationEditorMenu.h"

#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Features/EditorFeatures.h"
#include "Features/IModularFeatures.h"
#include "UndoHistoryModule.h"
#include "Framework/Commands/GenericCommands.h"
#include "ToolboxModule.h"


#define LOCTEXT_NAMESPACE "MainFileMenu"


void FMainMenu::FillFileMenu( FMenuBuilder& MenuBuilder, const TSharedRef<FExtender> Extender )
{
	MenuBuilder.BeginSection("FileLoadAndSave", LOCTEXT("LoadSandSaveHeading", "Load and Save"));
	{
		// Open Asset...
		MenuBuilder.AddMenuEntry( FGlobalEditorCommonCommands::Get().SummonOpenAssetDialog );

		// Save All
		MenuBuilder.AddMenuEntry(FMainFrameCommands::Get().SaveAll, "SaveAll");

		// Choose specific files to save
		MenuBuilder.AddMenuEntry(FMainFrameCommands::Get().ChooseFilesToSave, "ChooseFilesToSave");

		if (ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable())
		{
			// Choose specific files to submit
			MenuBuilder.AddMenuEntry(FMainFrameCommands::Get().ChooseFilesToCheckIn, "ChooseFilesToCheckIn");
		}
		else
		{
			MenuBuilder.AddMenuEntry(FMainFrameCommands::Get().ConnectToSourceControl, "ConnectToSourceControl");
		}
	}
	MenuBuilder.EndSection();
}

#undef LOCTEXT_NAMESPACE


#define LOCTEXT_NAMESPACE "MainEditMenu"

void FMainMenu::FillEditMenu( FMenuBuilder& MenuBuilder, const TSharedRef< FExtender > Extender, const TSharedPtr<FTabManager> TabManager )
{
	MenuBuilder.BeginSection("EditHistory", LOCTEXT("HistoryHeading", "History"));
	{
		struct Local
		{
			/** @return Returns a dynamic text string for Undo that contains the name of the action */
			static FText GetUndoLabelText()
			{
				return FText::Format(LOCTEXT("DynamicUndoLabel", "Undo {0}"), GUnrealEd->Trans->GetUndoContext().Title);
			}

			/** @return Returns a dynamic text string for Redo that contains the name of the action */
			static FText GetRedoLabelText()
			{
				return FText::Format(LOCTEXT("DynamicRedoLabel", "Redo {0}"), GUnrealEd->Trans->GetRedoContext().Title);
			}
		};

		// Undo
		TAttribute<FText> DynamicUndoLabel;
		DynamicUndoLabel.BindStatic(&Local::GetUndoLabelText);
		MenuBuilder.AddMenuEntry( FGenericCommands::Get().Undo, "Undo", DynamicUndoLabel); // TAttribute< FString >::Create( &Local::GetUndoLabelText ) );

		// Redo
		TAttribute< FText > DynamicRedoLabel;
		DynamicRedoLabel.BindStatic( &Local::GetRedoLabelText );
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Redo, "Redo", DynamicRedoLabel); // TAttribute< FString >::Create( &Local::GetRedoLabelText ) );

		// Show undo history
		MenuBuilder.AddMenuEntry(
			LOCTEXT("UndoHistoryTabTitle", "Undo History"),
			LOCTEXT("UndoHistoryTooltipText", "View the entire undo history."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "UndoHistory.TabIcon"),
			FUIAction(FExecuteAction::CreateStatic(&FUndoHistoryModule::ExecuteOpenUndoHistory))
			);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("EditLocalTabSpawners", LOCTEXT("ConfigurationHeading", "Configuration"));
	{
		if (GetDefault<UEditorExperimentalSettings>()->bToolbarCustomization)
		{
			FUIAction ToggleMultiBoxEditMode(
				FExecuteAction::CreateStatic(&FMultiBoxSettings::ToggleToolbarEditing),
				FCanExecuteAction(),
				FIsActionChecked::CreateStatic(&FMultiBoxSettings::IsInToolbarEditMode)
			);
		
			MenuBuilder.AddMenuEntry(
				LOCTEXT("EditToolbarsLabel", "Edit Toolbars"),
				LOCTEXT("EditToolbarsToolTip", "Allows customization of each toolbar"),
				FSlateIcon(),
				ToggleMultiBoxEditMode,
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

			// Automatically populate tab spawners from TabManager
			if (TabManager.IsValid())
			{
				const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
				TabManager->PopulateTabSpawnerMenu(MenuBuilder, MenuStructure.GetEditOptions());
			}
		}

		if (GetDefault<UEditorStyleSettings>()->bExpandConfigurationMenus)
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("EditorPreferencesSubMenuLabel", "Editor Preferences"),
				LOCTEXT("EditorPreferencesSubMenuToolTip", "Configure the behavior and features of this Editor"),
				FNewMenuDelegate::CreateStatic(&FSettingsMenu::MakeMenu, FName("Editor")),
				false,
				FSlateIcon(FEditorStyle::GetStyleSetName(), "EditorPreferences.TabIcon")
			);

			MenuBuilder.AddSubMenu(
				LOCTEXT("ProjectSettingsSubMenuLabel", "Project Settings"),
				LOCTEXT("ProjectSettingsSubMenuToolTip", "Change the settings of the currently loaded project"),
				FNewMenuDelegate::CreateStatic(&FSettingsMenu::MakeMenu, FName("Project")),
				false,
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ProjectSettings.TabIcon")
			);
		}
		else
		{
#if !PLATFORM_MAC // Handled by app's menu in menu bar
			MenuBuilder.AddMenuEntry(
				LOCTEXT("EditorPreferencesMenuLabel", "Editor Preferences..."),
				LOCTEXT("EditorPreferencesMenuToolTip", "Configure the behavior and features of the Unreal Editor."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "EditorPreferences.TabIcon"),
				FUIAction(FExecuteAction::CreateStatic(&FSettingsMenu::OpenSettings, FName("Editor"), FName("General"), FName("Appearance")))
			);
#endif

			MenuBuilder.AddMenuEntry(
				LOCTEXT("ProjectSettingsMenuLabel", "Project Settings..."),
				LOCTEXT("ProjectSettingsMenuToolTip", "Change the settings of the currently loaded project."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ProjectSettings.TabIcon"),
				FUIAction(FExecuteAction::CreateStatic(&FSettingsMenu::OpenSettings, FName("Project"), FName("Project"), FName("General")))
			);
		}

		//@todo The tab system needs to be able to be extendable by plugins [9/3/2013 Justin.Sargent]
		if (IModularFeatures::Get().IsModularFeatureAvailable(EditorFeatures::PluginsEditor))
		{
			FGlobalTabmanager::Get()->PopulateTabSpawnerMenu(MenuBuilder, "PluginsEditor");
		}
	}
	MenuBuilder.EndSection();
}

#undef LOCTEXT_NAMESPACE

#define LOCTEXT_NAMESPACE "MainWindowMenu"

void FMainMenu::FillWindowMenu( FMenuBuilder& MenuBuilder, const TSharedRef< FExtender > Extender, const TSharedPtr<FTabManager> TabManager )
{
	// Automatically populate tab spawners from TabManager
	if (TabManager.IsValid())
	{
		// Local editor tabs
		TabManager->PopulateLocalTabSpawnerMenu(MenuBuilder);
	
		// General tabs
		const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
		TabManager->PopulateTabSpawnerMenu(MenuBuilder, MenuStructure.GetStructureRoot());
	}

	MenuBuilder.BeginSection("WindowGlobalTabSpawners");
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ProjectLauncherLabel", "Project Launcher"),
			LOCTEXT("ProjectLauncherToolTip", "The Project Launcher provides advanced workflows for packaging, deploying and launching your projects."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Launcher.TabIcon"),
			FUIAction(FExecuteAction::CreateStatic(&FMainMenu::OpenProjectLauncher))
			);
	}
	MenuBuilder.EndSection();

	{
		// This is a temporary home for the spawners of experimental features that must be explicitly enabled.
		// When the feature becomes permanent and need not check a flag, register a nomad spawner for it in the proper WorkspaceMenu category
		bool bBlutility = GetDefault<UEditorExperimentalSettings>()->bEnableEditorUtilityBlueprints;
		bool bLocalizationDashboard = GetDefault<UEditorExperimentalSettings>()->bEnableLocalizationDashboard;
		bool bTranslationPicker = GetDefault<UEditorExperimentalSettings>()->bEnableTranslationPicker;
		bool bDeviceOutputLog = GetDefault<UEditorExperimentalSettings>()->bDeviceOutputLog;

		// Make sure at least one is enabled before creating the section
		if (bBlutility || bLocalizationDashboard || bTranslationPicker || bDeviceOutputLog)
		{
			MenuBuilder.BeginSection("ExperimentalTabSpawners", LOCTEXT("ExperimentalTabSpawnersHeading", "Experimental"));
			{
				// Blutility
				if (bBlutility)
				{
					MenuBuilder.AddMenuEntry(
						LOCTEXT("BlutilityShelfLabel", "Blutility Shelf"),
						LOCTEXT("BlutilityShelfToolTip", "Open the blutility shelf."),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateStatic(&FMainMenu::OpenBlutilityShelf))
						);
				}

				// Localization Dashboard
				if (bLocalizationDashboard)
				{
					MenuBuilder.AddMenuEntry(
						LOCTEXT("LocalizationDashboardLabel", "Localization Dashboard"),
						LOCTEXT("LocalizationDashboardToolTip", "Open the Localization Dashboard for this Project."),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateStatic(&FMainMenu::OpenLocalizationDashboard))
						);
				}

				// Translation Picker
				if (bTranslationPicker)
				{
					MenuBuilder.AddMenuEntry(
						LOCTEXT("TranslationPickerMenuItem", "Translation Picker"),
						LOCTEXT("TranslationPickerMenuItemToolTip", "Launch the Translation Picker to Modify Editor Translations"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateStatic(&FMainFrameTranslationEditorMenu::HandleOpenTranslationPicker))
						);
				}
				
				// Device output log
				if (bDeviceOutputLog)
				{
					MenuBuilder.AddMenuEntry(
						LOCTEXT("DeviceOutputLogMenuLabel", "Device Output Log"),
						LOCTEXT("DeviceOutputLogToolTip", "Open the Device Output Log tab."),
						FSlateIcon(FEditorStyle::GetStyleSetName(), "Log.TabIcon"),
						FUIAction(FExecuteAction::CreateStatic(&FMainMenu::OpenDeviceOutputLog))
						);
				}

			}
			MenuBuilder.EndSection();
		}
	}

	MenuBuilder.BeginSection("WindowLayout", NSLOCTEXT("MainAppMenu", "LayoutManagementHeader", "Layout"));
	{
		MenuBuilder.AddMenuEntry(FMainFrameCommands::Get().ResetLayout);
		MenuBuilder.AddMenuEntry(FMainFrameCommands::Get().SaveLayout);
#if !PLATFORM_MAC // On Mac windowed fullscreen mode in the editor is currently unavailable
		MenuBuilder.AddMenuEntry(FMainFrameCommands::Get().ToggleFullscreen);
#endif
	}
	MenuBuilder.EndSection();
}

#undef LOCTEXT_NAMESPACE


void FMainMenu::FillHelpMenu( FMenuBuilder& MenuBuilder, const TSharedRef< FExtender > Extender )
{
	MenuBuilder.BeginSection("HelpOnline", NSLOCTEXT("MainHelpMenu", "Online", "Online"));
	{
		MenuBuilder.AddMenuEntry(FMainFrameCommands::Get().VisitSupportWebSite);
		MenuBuilder.AddMenuEntry(FMainFrameCommands::Get().VisitForums);
		MenuBuilder.AddMenuEntry(FMainFrameCommands::Get().VisitSearchForAnswersPage);
		MenuBuilder.AddMenuEntry(FMainFrameCommands::Get().VisitWiki);


		const FText SupportWebSiteLabel = NSLOCTEXT("MainHelpMenu", "VisitUnrealEngineSupportWebSite", "Unreal Engine Support Web Site...");

		MenuBuilder.AddMenuSeparator("EpicGamesHelp");
		MenuBuilder.AddMenuEntry(FMainFrameCommands::Get().VisitEpicGamesDotCom, "VisitEpicGamesDotCom");

		MenuBuilder.AddMenuSeparator("Credits");
		MenuBuilder.AddMenuEntry(FMainFrameCommands::Get().CreditsUnrealEd);
	}
	MenuBuilder.EndSection();

#if !PLATFORM_MAC // Handled by app's menu in menu bar
	MenuBuilder.BeginSection("HelpApplication", NSLOCTEXT("MainHelpMenu", "Application", "Application"));
	{
		const FText AboutWindowTitle = NSLOCTEXT("MainHelpMenu", "AboutUnrealEditor", "About Unreal Editor...");

		MenuBuilder.AddMenuEntry(FMainFrameCommands::Get().AboutUnrealEd, "AboutUnrealEd", AboutWindowTitle);
	}
	MenuBuilder.EndSection();
#endif
}


TSharedRef<SWidget> FMainMenu::MakeMainMenu(const TSharedPtr<FTabManager>& TabManager, const TSharedRef< FExtender > Extender)
{
#define LOCTEXT_NAMESPACE "MainMenu"

	
	// Put the toolbox into our menus
	{
		const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
		IToolboxModule& ToolboxModule = FModuleManager::LoadModuleChecked<IToolboxModule>("Toolbox");
		ToolboxModule.RegisterSpawners(MenuStructure.GetDeveloperToolsDebugCategory(), MenuStructure.GetDeveloperToolsMiscCategory());
	}

	// Cache all project names once
	FMainFrameActionCallbacks::CacheProjectNames();

	FMenuBarBuilder MenuBuilder(FMainFrameCommands::ActionList, Extender);
	{
		// File
		MenuBuilder.AddPullDownMenu( 
			LOCTEXT("FileMenu", "File"),
			LOCTEXT("FileMenu_ToolTip", "Open the file menu"),
			FNewMenuDelegate::CreateStatic(&FMainMenu::FillFileMenu, Extender),
			"File",
			FName(TEXT("FileMenu"))
		);

		// Edit
		MenuBuilder.AddPullDownMenu( 
			LOCTEXT("EditMenu", "Edit"),
			LOCTEXT("EditMenu_ToolTip", "Open the edit menu"),
			FNewMenuDelegate::CreateStatic(&FMainMenu::FillEditMenu, Extender, TabManager),
			"Edit"
			,
			FName(TEXT("EditMenu"))
		);

		// Window
		MenuBuilder.AddPullDownMenu(
			LOCTEXT("WindowMenu", "Window"),
			LOCTEXT("WindowMenu_ToolTip", "Open new windows or tabs."),
			FNewMenuDelegate::CreateStatic(&FMainMenu::FillWindowMenu, Extender, TabManager),
			"Window"
		);

		// Help
		MenuBuilder.AddPullDownMenu( 
			LOCTEXT("HelpMenu", "Help"),
			LOCTEXT("HelpMenu_ToolTip", "Open the help menu"),
			FNewMenuDelegate::CreateStatic(&FMainMenu::FillHelpMenu, Extender),
			"Help"
		);
	}

	// Create the menu bar!
	TSharedRef<SWidget> MenuBarWidget = MenuBuilder.MakeWidget();

	// Tell tab-manager about the multi-box for platforms with a global menu bar
	TabManager->SetMenuMultiBox(MenuBuilder.GetMultiBox());
	
	return MenuBarWidget;
}

#undef LOCTEXT_NAMESPACE


#define LOCTEXT_NAMESPACE "MainTabMenu"

TSharedRef< SWidget > FMainMenu::MakeMainTabMenu( const TSharedPtr<FTabManager>& TabManager, const TSharedRef< FExtender > UserExtender )
{	struct Local
	{
		static void FillProjectMenuItems( FMenuBuilder& MenuBuilder )
		{
			MenuBuilder.BeginSection( "FileProject", LOCTEXT("ProjectHeading", "Project") );
			{
				MenuBuilder.AddMenuEntry( FMainFrameCommands::Get().NewProject );
				MenuBuilder.AddMenuEntry( FMainFrameCommands::Get().OpenProject );

				FText ShortIDEName = FSourceCodeNavigation::GetSelectedSourceCodeIDE();

				MenuBuilder.AddMenuEntry( FMainFrameCommands::Get().AddCodeToProject,
					NAME_None,
					TAttribute<FText>(),
					FText::Format(LOCTEXT("AddCodeToProjectTooltip", "Adds C++ code to the project. The code can only be compiled if you have {0} installed."), ShortIDEName)
				);

				MenuBuilder.AddSubMenu(
					LOCTEXT("PackageProjectSubMenuLabel", "Package Project"),
					LOCTEXT("PackageProjectSubMenuToolTip", "Compile, cook and package your project and its content for distribution."),
					FNewMenuDelegate::CreateStatic( &FPackageProjectMenu::MakeMenu ), false, FSlateIcon(FEditorStyle::GetStyleSetName(), "MainFrame.PackageProject")
				);
				/*
				MenuBuilder.AddMenuEntry( FMainFrameCommands::Get().LocalizeProject,
					NAME_None,
					TAttribute<FText>(),
					LOCTEXT("LocalizeProjectToolTip", "Gather text from your project and import/export translations.")
					);
					*/
				/*
				MenuBuilder.AddSubMenu(
					LOCTEXT("CookProjectSubMenuLabel", "Cook Project"),
					LOCTEXT("CookProjectSubMenuToolTip", "Cook your project content for debugging"),
					FNewMenuDelegate::CreateStatic( &FCookContentMenu::MakeMenu ), false, FSlateIcon()
				);
				*/

				FString SolutionPath;
				if (FSourceCodeNavigation::DoesModuleSolutionExist())
				{
					MenuBuilder.AddMenuEntry( FMainFrameCommands::Get().RefreshCodeProject,
						NAME_None,
						FText::Format(LOCTEXT("RefreshCodeProjectLabel", "Refresh {0} Project"), ShortIDEName),
						FText::Format(LOCTEXT("RefreshCodeProjectTooltip", "Refreshes your C++ code project in {0}."), ShortIDEName)
					);

					MenuBuilder.AddMenuEntry( FMainFrameCommands::Get().OpenIDE,
						NAME_None,
						FText::Format(LOCTEXT("OpenIDELabel", "Open {0}"), ShortIDEName),
						FText::Format(LOCTEXT("OpenIDETooltip", "Opens your C++ code in {0}."), ShortIDEName)
					);
				}
				else
				{
					MenuBuilder.AddMenuEntry( FMainFrameCommands::Get().RefreshCodeProject,
						NAME_None,
						FText::Format(LOCTEXT("GenerateCodeProjectLabel", "Generate {0} Project"), ShortIDEName),
						FText::Format(LOCTEXT("GenerateCodeProjectTooltip", "Generates your C++ code project in {0}."), ShortIDEName)
					);
				}

				// @hack GDC: this should be moved somewhere else and be less hacky
				ITargetPlatform* RunningTargetPlatform = GetTargetPlatformManager()->GetRunningTargetPlatform();

				if (RunningTargetPlatform != nullptr)
				{
					const FName CookedPlatformName = *(RunningTargetPlatform->PlatformName() + TEXT("NoEditor"));
					const FText CookedPlatformText = FText::FromString(RunningTargetPlatform->PlatformName());

					FUIAction Action(
						FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::CookContent, CookedPlatformName),
						FCanExecuteAction::CreateStatic(&FMainFrameActionCallbacks::CookContentCanExecute, CookedPlatformName)
					);

					MenuBuilder.AddMenuEntry(
						FText::Format(LOCTEXT("CookContentForPlatform", "Cook Content for {0}"), CookedPlatformText),
						FText::Format(LOCTEXT("CookContentForPlatformTooltip", "Cook your game content for debugging on the {0} platform"), CookedPlatformText),
						FSlateIcon(),
						Action
					);
				}
			}
			MenuBuilder.EndSection();
		}

		static void FillRecentFileAndExitMenuItems( FMenuBuilder& MenuBuilder )
		{
			MenuBuilder.BeginSection("FileRecentFiles");
			{
				if (GetDefault<UEditorStyleSettings>()->bShowProjectMenus && FMainFrameActionCallbacks::ProjectNames.Num() > 0)
				{
					MenuBuilder.AddSubMenu(
						LOCTEXT("SwitchProjectSubMenu", "Recent Projects"),
						LOCTEXT("SwitchProjectSubMenu_ToolTip", "Select a project to switch to"),
						FNewMenuDelegate::CreateStatic(&FRecentProjectsMenu::MakeMenu), false, FSlateIcon(FEditorStyle::GetStyleSetName(), "MainFrame.RecentProjects")
						);
				}
			}
			MenuBuilder.EndSection();

#if !PLATFORM_MAC // Handled by app's menu in menu bar
			MenuBuilder.AddMenuSeparator();
			MenuBuilder.AddMenuEntry( FMainFrameCommands::Get().Exit, "Exit" );
#endif
		}
	};

	FExtensibilityManager ExtensibilityManager;

	ExtensibilityManager.AddExtender( UserExtender );
	{
		TSharedRef< FExtender > Extender( new FExtender() );
	
		IMainFrameModule& MainFrameModule = FModuleManager::GetModuleChecked<IMainFrameModule>("MainFrame");

		if (GetDefault<UEditorStyleSettings>()->bShowProjectMenus)
		{
			Extender->AddMenuExtension(
				"FileLoadAndSave",
				EExtensionHook::After,
				MainFrameModule.GetMainFrameCommandBindings(),
				FMenuExtensionDelegate::CreateStatic(&Local::FillProjectMenuItems));
		}

		Extender->AddMenuExtension(
			"FileLoadAndSave",
			EExtensionHook::After,
			MainFrameModule.GetMainFrameCommandBindings(),
			FMenuExtensionDelegate::CreateStatic( &Local::FillRecentFileAndExitMenuItems ) );

		ExtensibilityManager.AddExtender( Extender );
	}


	TSharedRef< SWidget > MenuBarWidget = FMainMenu::MakeMainMenu( TabManager, ExtensibilityManager.GetAllExtenders().ToSharedRef() );

	return MenuBarWidget;
}


#undef LOCTEXT_NAMESPACE
