// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelEditorToolBar.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SSlider.h"
#include "EditorStyleSet.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "Settings/EditorExperimentalSettings.h"
#include "GameMapsSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/TextureStreamingTypes.h"
#include "Toolkits/AssetEditorManager.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "SourceCodeNavigation.h"
#include "Kismet2/DebuggerCommands.h"
#include "SceneOutlinerPublicTypes.h"
#include "SceneOutlinerModule.h"
#include "SScalabilitySettings.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Matinee/MatineeActor.h"
#include "LevelSequenceActor.h"
#include "Engine/LevelScriptBlueprint.h"
#include "ISettingsCategory.h"
#include "ISettingsContainer.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Widgets/Input/SVolumeControl.h"
#include "Features/IModularFeatures.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "Features/EditorFeatures.h"
#include "ConfigCacheIni.h"
#include "ILauncherPlatform.h"
#include "LauncherPlatformModule.h"

namespace LevelEditorActionHelpers
{
	/** Filters out any classes for the Class Picker when creating or selecting classes in the Blueprints dropdown */
	class FBlueprintParentFilter_MapModeSettings : public IClassViewerFilter
	{
	public:
		/** Classes to not allow any children of into the Class Viewer/Picker. */
		TSet< const UClass* > AllowedChildrenOfClasses;

		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) override
		{
			return InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) == EFilterReturn::Passed;
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			return InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) == EFilterReturn::Passed;
		}
	};

	/**
	 * Retrieves the GameMode class
	 *
	 * @param InLevelEditor				The editor to extract the world from
	 * @param bInIsProjectSettings		TRUE if retrieving the game mode from the project settings
	 * @return							The GameMode class in the Project Settings or World Settings
	 */
	static UClass* GetGameModeClass(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/** Callback for the label to display for the GameMode menu selection */
	static FText GetOpenGameModeBlueprintLabel(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/** Callback when selecting a GameMode class, assigns it to the world */
	static void OnSelectGameModeClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/** Callback when creating a new GameMode class, creates the Blueprint and assigns it to the world */
	static void OnCreateGameModeClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/**
	 * Retrieves the active GameState class from
	 *
	 * @param InLevelEditor		The editor to extract the world from
	 * @param bInIsProjectSettings		TRUE if retrieving the game mode from the project settings
	 * @return					The active GameState class in the World Settings
	 */
	static UClass* GetGameStateClass(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/** Callback for the label to display for the GameState menu selection */
	static FText GetOpenGameStateBlueprintLabel(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);
	
	/** Callback when selecting a GameState class, assigns it to the world */
	static void OnSelectGameStateClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/** Callback when creating a new GameState class, creates the Blueprint and assigns it to the world */
	static void OnCreateGameStateClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/**
	 * Retrieves the active Pawn class from
	 *
	 * @param InLevelEditor		The editor to extract the world from
	 * @return					The active Pawn class in the World Settings
	 */
	static UClass* GetPawnClass(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/** Callback for the label to display for the Pawn menu selection */
	static FText GetOpenPawnBlueprintLabel(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/** Callback for the tooltip to display for the Pawn menu selection */
	static FText GetOpenPawnBlueprintTooltip(TWeakPtr< SLevelEditor > InLevelEditor);

	/** Callback when selecting a Pawn class, assigns it to the world */
	static void OnSelectPawnClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/** Callback when creating a new Pawn class, creates the Blueprint and assigns it to the world */
	static void OnCreatePawnClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/**
	 * Retrieves the active HUD class from
	 *
	 * @param InLevelEditor		The editor to extract the world from
	 * @param bInIsProjectSettings		TRUE if retrieving the game mode from the project settings
	 * @return					The active HUD class in the World Settings
	 */
	static UClass* GetHUDClass(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/** Callback for the label to display for the HUD menu selection */
	static FText GetOpenHUDBlueprintLabel(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);
	
	/** Callback when selecting a HUD class, assigns it to the world */
	static void OnSelectHUDClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/** Callback when creating a new HUD class, creates the Blueprint and assigns it to the world */
	static void OnCreateHUDClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/**
	 * Retrieves the active PlayerController class from
	 *
	 * @param InLevelEditor		The editor to extract the world from
	 * @param bInIsProjectSettings		TRUE if retrieving the game mode from the project settings
	 * @return					The active PlayerController class in the World Settings
	 */
	static UClass* GetPlayerControllerClass(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/** Callback for the label to display for the PlayerController menu selection */
	static FText GetOpenPlayerControllerBlueprintLabel(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/** Callback when selecting a PlayerController class, assigns it to the world */
	static void OnSelectPlayerControllerClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/** Callback when creating a new PlayerController class, creates the Blueprint and assigns it to the world */
	static void OnCreatePlayerControllerClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings);

	/** Opens a native class's header file if the compiler is available. */
	static void OpenNativeClass(UClass* InClass)
	{
		if(InClass->HasAllClassFlags(CLASS_Native) && FSourceCodeNavigation::IsCompilerAvailable())
		{
			FString NativeParentClassHeaderPath;
			const bool bFileFound = FSourceCodeNavigation::FindClassHeaderPath(InClass, NativeParentClassHeaderPath) 
				&& (IFileManager::Get().FileSize(*NativeParentClassHeaderPath) != INDEX_NONE);
			if (bFileFound)
			{
				const FString AbsoluteHeaderPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*NativeParentClassHeaderPath);
				FSourceCodeNavigation::OpenSourceFile( AbsoluteHeaderPath );
			}
		}
	}

	/** Open the game mode blueprint, in the project settings or world settings */
	static void OpenGameModeBlueprint( TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings )
	{
		if(UClass* GameModeClass = GetGameModeClass(InLevelEditor, bInIsProjectSettings))
		{
			if(UBlueprint* BlueprintClass = Cast<UBlueprint>(GameModeClass->ClassGeneratedBy))
			{
				// @todo Re-enable once world centric works
				const bool bOpenWorldCentric = false;
				FAssetEditorManager::Get().OpenEditorForAsset(
					BlueprintClass,
					bOpenWorldCentric ? EToolkitMode::WorldCentric : EToolkitMode::Standalone,
					InLevelEditor.Pin() );
			}
			else
			{
				OpenNativeClass(GameModeClass);
			}
		}
	}

	/** Open the game state blueprint, in the project settings or world settings */
	static void OpenGameStateBlueprint( TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings )
	{
		if(UClass* GameStateClass = GetGameStateClass(InLevelEditor, bInIsProjectSettings))
		{
			if(UBlueprint* BlueprintClass = Cast<UBlueprint>(GameStateClass->ClassGeneratedBy))
			{
				// @todo Re-enable once world centric works
				const bool bOpenWorldCentric = false;
				FAssetEditorManager::Get().OpenEditorForAsset(
					BlueprintClass,
					bOpenWorldCentric ? EToolkitMode::WorldCentric : EToolkitMode::Standalone,
					InLevelEditor.Pin() );
			}
			else
			{
				OpenNativeClass(GameStateClass);
			}
		}
	}

	/** Open the default pawn blueprint, in the project settings or world settings */
	static void OpenDefaultPawnBlueprint( TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings )
	{
		if(UClass* DefaultPawnClass = GetPawnClass(InLevelEditor, bInIsProjectSettings))
		{
			if(UBlueprint* BlueprintClass = Cast<UBlueprint>(DefaultPawnClass->ClassGeneratedBy))
			{
				// @todo Re-enable once world centric works
				const bool bOpenWorldCentric = false;
				FAssetEditorManager::Get().OpenEditorForAsset(
					BlueprintClass,
					bOpenWorldCentric ? EToolkitMode::WorldCentric : EToolkitMode::Standalone,
					InLevelEditor.Pin() );
			}
			else
			{
				OpenNativeClass(DefaultPawnClass);
			}
		}
	}

	/** Open the HUD blueprint, in the project settings or world settings */
	static void OpenHUDBlueprint( TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings )
	{
		if(UClass* DefaultHUDClass = GetHUDClass(InLevelEditor, bInIsProjectSettings))
		{
			if(UBlueprint* BlueprintClass = Cast<UBlueprint>(DefaultHUDClass->ClassGeneratedBy))
			{
				// @todo Re-enable once world centric works
				const bool bOpenWorldCentric = false;
				FAssetEditorManager::Get().OpenEditorForAsset(
					BlueprintClass,
					bOpenWorldCentric ? EToolkitMode::WorldCentric : EToolkitMode::Standalone,
					InLevelEditor.Pin() );
			}
			else
			{
				OpenNativeClass(DefaultHUDClass);
			}
		}
	}

	/** Open the player controller blueprint, in the project settings or world settings */
	static void OpenPlayerControllerBlueprint( TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings )
	{
		if(UClass* PlayerControllerClass = GetPlayerControllerClass(InLevelEditor, bInIsProjectSettings))
		{
			if(UBlueprint* BlueprintClass = Cast<UBlueprint>(PlayerControllerClass->ClassGeneratedBy))
			{
				// @todo Re-enable once world centric works
				const bool bOpenWorldCentric = false;
				FAssetEditorManager::Get().OpenEditorForAsset(
					BlueprintClass,
					bOpenWorldCentric ? EToolkitMode::WorldCentric : EToolkitMode::Standalone,
					InLevelEditor.Pin() );
			}
			else
			{
				OpenNativeClass(PlayerControllerClass);
			}
		}
	}

	/**
	 * Builds a sub-menu for selecting a class
	 *
	 * @param InMenuBuilder		Object to append menu items/widgets to
	 * @param InRootClass		The root class to filter the Class Viewer by to only show children of
	 * @param InOnClassPicked	Callback delegate to fire when a class is picked
	 */
	void GetSelectSettingsClassSubMenu(FMenuBuilder& InMenuBuilder, UClass* InRootClass, FOnClassPicked InOnClassPicked)
	{
		FClassViewerInitializationOptions Options;
		Options.Mode = EClassViewerMode::ClassPicker;
		Options.DisplayMode = EClassViewerDisplayMode::ListView;
		Options.bShowObjectRootClass = true;
		Options.bShowNoneOption = true;

		// Only want blueprint actor base classes.
		Options.bIsBlueprintBaseOnly = true;

		// This will allow unloaded blueprints to be shown.
		Options.bShowUnloadedBlueprints = true;

		TSharedPtr< FBlueprintParentFilter_MapModeSettings > Filter = MakeShareable(new FBlueprintParentFilter_MapModeSettings);
		Filter->AllowedChildrenOfClasses.Add(InRootClass);
		Options.ClassFilter = Filter;

		FText RootClassName = FText::FromString(InRootClass->GetName());
		TSharedRef<SWidget> ClassViewer = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, InOnClassPicked);
		FFormatNamedArguments FormatArgs;
		FormatArgs.Add(TEXT("RootClass"), RootClassName);
		InMenuBuilder.BeginSection(NAME_None, FText::Format(NSLOCTEXT("LevelToolBarViewMenu", "SelectGameModeLabel", "Select {RootClass} class"), FormatArgs));
		InMenuBuilder.AddWidget(ClassViewer, FText::GetEmpty(), true);
		InMenuBuilder.EndSection();
	}

	/**
	 * Builds a sub-menu for creating a class
	 *
	 * @param InMenuBuilder		Object to append menu items/widgets to
	 * @param InRootClass		The root class to filter the Class Viewer by to only show children of
	 * @param InOnClassPicked	Callback delegate to fire when a class is picked
	 */
	void GetCreateSettingsClassSubMenu(FMenuBuilder& InMenuBuilder, UClass* InRootClass, FOnClassPicked InOnClassPicked)
	{
		FClassViewerInitializationOptions Options;
		Options.Mode = EClassViewerMode::ClassPicker;
		Options.DisplayMode = EClassViewerDisplayMode::ListView;
		Options.bShowObjectRootClass = true;

		// Only want blueprint actor base classes.
		Options.bIsBlueprintBaseOnly = true;

		// This will allow unloaded blueprints to be shown.
		Options.bShowUnloadedBlueprints = true;

		TSharedPtr< FBlueprintParentFilter_MapModeSettings > Filter = MakeShareable(new FBlueprintParentFilter_MapModeSettings);
		Filter->AllowedChildrenOfClasses.Add(InRootClass);
		Options.ClassFilter = Filter;

		FText RootClassName = FText::FromString(InRootClass->GetName());
		TSharedRef<SWidget> ClassViewer = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, InOnClassPicked);
		FFormatNamedArguments FormatArgs;
		FormatArgs.Add(TEXT("RootClass"), RootClassName);
		InMenuBuilder.BeginSection(NAME_None, FText::Format(NSLOCTEXT("LevelToolBarViewMenu", "CreateGameModeLabel", "Select {RootClass} parent class"), FormatArgs));
		InMenuBuilder.AddWidget(ClassViewer, FText::GetEmpty(), true);
		InMenuBuilder.EndSection();
	}

	/** Helper struct for passing all required data to the GetBlueprintSettingsSubMenu function */
	struct FBlueprintMenuSettings
	{
		/** The UI command for editing the Blueprint class associated with the menu */
		FUIAction EditCommand;

		/** Current class associated with the menu */
		UClass* CurrentClass;

		/** Root class that defines what class children can be set through the menu */
		UClass* RootClass;

		/** Callback when a class is picked, to assign the new class */
		FOnClassPicked OnSelectClassPicked;

		/** Callback when a class is picked, to create a new child class of and assign */
		FOnClassPicked OnCreateClassPicked;

		/** Level Editor these menu settings are for */
		TWeakPtr< SLevelEditor > LevelEditor;

		/** TRUE if these represent Project Settings, FALSE if they represent World Settings */
		bool bIsProjectSettings;
	};

	/** Returns the label of the "Check Out" option based on if source control is present or not */
	FText GetCheckOutLabel()
	{
#define LOCTEXT_NAMESPACE "LevelToolBarViewMenu"
		if(ISourceControlModule::Get().IsEnabled())
		{
			return LOCTEXT("CheckoutMenuLabel", "Check Out");
		}
		else
		{
			return LOCTEXT("MakeWritableLabel", "Make Writable");
		}
#undef LOCTEXT_NAMESPACE
	}

	/** Returns the tooltip of the "Check Out" option based on if source control is present or not */
	FText GetCheckOutTooltip()
	{
#define LOCTEXT_NAMESPACE "LevelToolBarViewMenu"
		if(ISourceControlModule::Get().IsEnabled())
		{
			return LOCTEXT("CheckoutMenuTooltip", "Checks out the project settings config file so the game mode can be set.");
		}
		else
		{
			return LOCTEXT("MakeWritableTooltip", "Forces the project settings config file to be writable so the game mode can be set.");
		}
#undef LOCTEXT_NAMESPACE
	}

	/**
	 * A sub-menu for the Blueprints dropdown, facilitates all the sub-menu actions such as creating, editing, and selecting classes for the world settings game mode.
	 *
	 * @param InMenuBuilder		Object to append menu items/widgets to
	 * @param InCommandList		Commandlist for menu items
	 * @param InSettingsData	All the data needed to create the menu actions
	 */
	void GetBlueprintSettingsSubMenu(FMenuBuilder& InMenuBuilder, const TSharedRef<FUICommandList> InCommandList, FBlueprintMenuSettings InSettingsData);

	/** Returns TRUE if the class can be edited, always TRUE for Blueprints and for native classes a compiler must be present */
	bool CanEditClass(UClass* InClass)
	{
		// For native classes, we can only edit them if a compiler is available
		if(InClass && InClass->HasAllClassFlags(CLASS_Native))
		{
			return FSourceCodeNavigation::IsCompilerAvailable();
		}
		return true;
	}

	/** Returns TRUE if the GameMode's sub-class can be created or selected */
	bool CanCreateSelectSubClass(UClass* InGameModeClass, bool bInIsProjectSettings)
	{
		// Can never create or select project settings sub-classes if the config file is not checked out
		if(bInIsProjectSettings && !FLevelEditorActionCallbacks::CanSelectGameModeBlueprint())
		{
			return false;
		}

		// If the game mode class is native, we cannot set the sub class
		if(!InGameModeClass || InGameModeClass->HasAllClassFlags(CLASS_Native))
		{
			return false;
		}
		return true;
	}

	/** Creates a tooltip for a submenu */
	FText GetSubMenuTooltip(UClass* InClass, UClass* InRootClass, bool bInIsProjectSettings)
	{
#define LOCTEXT_NAMESPACE "LevelToolBarViewMenu"
		FFormatNamedArguments Args;
		Args.Add(TEXT("Class"), FText::FromString(InRootClass->GetName()));
		Args.Add(TEXT("TargetLocation"), bInIsProjectSettings? LOCTEXT("Project", "project") : LOCTEXT("World", "world"));
		return FText::Format(LOCTEXT("ClassSubmenu_Tooltip", "Select, edit, or create a new {Class} blueprint for the {TargetLocation}"), Args);
#undef LOCTEXT_NAMESPACE
	}

	/** Creates a tooltip for the create class submenu */
	FText GetCreateMenuTooltip(UClass* InGameModeClass, UClass* InRootClass, bool bInIsProjectSettings)
	{
#define LOCTEXT_NAMESPACE "LevelToolBarViewMenu"
		FText ResultText;

		// Game modes can always be created and selected (providing the config is checked out, handled separately)
		if(InRootClass != AGameModeBase::StaticClass() && InGameModeClass->HasAllClassFlags(CLASS_Native))
		{
			ResultText = LOCTEXT("CannotCreateClasses", "Cannot create classes when the game mode is a native class!");
		}
		else if(bInIsProjectSettings && !FLevelEditorActionCallbacks::CanSelectGameModeBlueprint())
		{
			ResultText = LOCTEXT("CannotCreateClasses_NeedsCheckOut", "Cannot create classes when the config file is not writable!");
		}
		else
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("RootClass"), FText::FromString(InRootClass->GetName()));
			Args.Add(TEXT("TargetLocation"), bInIsProjectSettings? LOCTEXT("Project", "project") : LOCTEXT("World", "world"));
			ResultText = FText::Format( LOCTEXT("CreateClass_Tooltip", "Create a new {RootClass} based on a selected class and auto-assign it to the {TargetLocation}"), Args );
		}

		return ResultText;
#undef LOCTEXT_NAMESPACE
	}

	/** Creates a tooltip for the select class submenu */
	FText GetSelectMenuTooltip(UClass* InGameModeClass, UClass* InRootClass, bool bInIsProjectSettings)
	{
#define LOCTEXT_NAMESPACE "LevelToolBarViewMenu"
		FText ResultText;

		// Game modes can always be created and selected (providing the config is checked out, handled separately)
		if(InRootClass != AGameModeBase::StaticClass() && InGameModeClass->HasAllClassFlags(CLASS_Native))
		{
			ResultText = LOCTEXT("CannotSelectClasses", "Cannot select classes when the game mode is a native class!");
		}
		else if(bInIsProjectSettings && !FLevelEditorActionCallbacks::CanSelectGameModeBlueprint())
		{
			ResultText = LOCTEXT("CannotSelectClasses_NeedsCheckOut", "Cannot select classes when the config file is not writable!");
		}
		else
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("RootClass"), FText::FromString(InRootClass->GetName()));
			Args.Add(TEXT("TargetLocation"), bInIsProjectSettings? LOCTEXT("Project", "project") : LOCTEXT("World", "world"));
			ResultText = FText::Format( LOCTEXT("SelectClass_Tooltip", "Select a new {RootClass} based on a selected class and auto-assign it to the {TargetLocation}"), Args );
		}
		return ResultText;
#undef LOCTEXT_NAMESPACE
	}

	void CreateGameModeSubMenu(FMenuBuilder& InMenuBuilder, TSharedRef<FUICommandList> InCommandList, TWeakPtr< SLevelEditor > InLevelEditor, bool bInProjectSettings)
	{
#define LOCTEXT_NAMESPACE "LevelToolBarViewMenu"
		LevelEditorActionHelpers::FBlueprintMenuSettings GameModeMenuSettings;
		GameModeMenuSettings.EditCommand = 
			FUIAction(
				FExecuteAction::CreateStatic< TWeakPtr< SLevelEditor > >( &OpenGameModeBlueprint, InLevelEditor, bInProjectSettings )
			);
		GameModeMenuSettings.OnCreateClassPicked = FOnClassPicked::CreateStatic( &LevelEditorActionHelpers::OnCreateGameModeClassPicked, InLevelEditor, bInProjectSettings );
		GameModeMenuSettings.OnSelectClassPicked = FOnClassPicked::CreateStatic( &LevelEditorActionHelpers::OnSelectGameModeClassPicked, InLevelEditor, bInProjectSettings );
		GameModeMenuSettings.CurrentClass = LevelEditorActionHelpers::GetGameModeClass(InLevelEditor, bInProjectSettings);
		GameModeMenuSettings.RootClass = AGameModeBase::StaticClass();
		GameModeMenuSettings.LevelEditor = InLevelEditor;
		GameModeMenuSettings.bIsProjectSettings = bInProjectSettings;

		auto IsGameModeActive = [](TWeakPtr< SLevelEditor > InLevelEditorPtr, bool bInProjSettings)->bool
		{
			UClass* WorldSettingsGameMode = LevelEditorActionHelpers::GetGameModeClass(InLevelEditorPtr, false);
			if((WorldSettingsGameMode == nullptr) ^ bInProjSettings ) //(WorldSettingsGameMode && !bInProjectSettings) || (!WorldSettingsGameMode && bInProjectSettings) )
			{
				return false;
			}
			return true;
		};

		InMenuBuilder.AddSubMenu( LevelEditorActionHelpers::GetOpenGameModeBlueprintLabel(InLevelEditor, bInProjectSettings), 
									GetSubMenuTooltip(GameModeMenuSettings.CurrentClass, GameModeMenuSettings.RootClass, bInProjectSettings), 
									FNewMenuDelegate::CreateStatic( &LevelEditorActionHelpers::GetBlueprintSettingsSubMenu, InCommandList, GameModeMenuSettings), 
									FUIAction(FExecuteAction(), FCanExecuteAction(), FIsActionChecked::CreateStatic(IsGameModeActive, InLevelEditor, bInProjectSettings)),
									NAME_None, EUserInterfaceActionType::RadioButton );
#undef LOCTEXT_NAMESPACE
	}

	/**
	 * Builds the game mode's sub menu objects
	 *
	 * @param InMenuBuilder		Object to append menu items/widgets to
	 * @param InCommandList		Commandlist for menu items
	 * @param InSettingsData	All the data needed to create the menu actions
	 */
	void GetGameModeSubMenu(FMenuBuilder& InMenuBuilder, const TSharedRef<FUICommandList> InCommandList, const FBlueprintMenuSettings& InSettingsData)
	{
#define LOCTEXT_NAMESPACE "LevelToolBarViewMenu"
		// Game State
		LevelEditorActionHelpers::FBlueprintMenuSettings GameStateMenuSettings;
		GameStateMenuSettings.EditCommand = 
			FUIAction(
				FExecuteAction::CreateStatic< TWeakPtr< SLevelEditor > >( &OpenGameStateBlueprint, InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings )
			);
		GameStateMenuSettings.OnCreateClassPicked = FOnClassPicked::CreateStatic( &LevelEditorActionHelpers::OnCreateGameStateClassPicked, InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings );
		GameStateMenuSettings.OnSelectClassPicked = FOnClassPicked::CreateStatic( &LevelEditorActionHelpers::OnSelectGameStateClassPicked, InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings );
		GameStateMenuSettings.CurrentClass = LevelEditorActionHelpers::GetGameStateClass(InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings);
		GameStateMenuSettings.RootClass = AGameStateBase::StaticClass();
		GameStateMenuSettings.LevelEditor = InSettingsData.LevelEditor;
		GameStateMenuSettings.bIsProjectSettings = InSettingsData.bIsProjectSettings;

		InMenuBuilder.AddSubMenu( LevelEditorActionHelpers::GetOpenGameStateBlueprintLabel(InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings),
			GetSubMenuTooltip(GameStateMenuSettings.CurrentClass, GameStateMenuSettings.RootClass, InSettingsData.bIsProjectSettings),
			FNewMenuDelegate::CreateStatic( &LevelEditorActionHelpers::GetBlueprintSettingsSubMenu, InCommandList, GameStateMenuSettings )
		);

		// Pawn
		LevelEditorActionHelpers::FBlueprintMenuSettings PawnMenuSettings;
		PawnMenuSettings.EditCommand = 
			FUIAction(
				FExecuteAction::CreateStatic< TWeakPtr< SLevelEditor > >( &OpenDefaultPawnBlueprint, InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings )
			);
		PawnMenuSettings.OnCreateClassPicked = FOnClassPicked::CreateStatic( &LevelEditorActionHelpers::OnCreatePawnClassPicked, InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings );
		PawnMenuSettings.OnSelectClassPicked = FOnClassPicked::CreateStatic( &LevelEditorActionHelpers::OnSelectPawnClassPicked, InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings );
		PawnMenuSettings.CurrentClass = LevelEditorActionHelpers::GetPawnClass(InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings);
		PawnMenuSettings.RootClass = APawn::StaticClass();
		PawnMenuSettings.LevelEditor = InSettingsData.LevelEditor;
		PawnMenuSettings.bIsProjectSettings = InSettingsData.bIsProjectSettings;

		InMenuBuilder.AddSubMenu( LevelEditorActionHelpers::GetOpenPawnBlueprintLabel(InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings),
			GetSubMenuTooltip(PawnMenuSettings.CurrentClass, PawnMenuSettings.RootClass, InSettingsData.bIsProjectSettings),
			FNewMenuDelegate::CreateStatic( &LevelEditorActionHelpers::GetBlueprintSettingsSubMenu, InCommandList, PawnMenuSettings )
		);

		// HUD
		LevelEditorActionHelpers::FBlueprintMenuSettings HUDMenuSettings;
		HUDMenuSettings.EditCommand = 
			FUIAction(
				FExecuteAction::CreateStatic< TWeakPtr< SLevelEditor > >( &OpenHUDBlueprint, InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings )
			);
		HUDMenuSettings.OnCreateClassPicked = FOnClassPicked::CreateStatic( &LevelEditorActionHelpers::OnCreateHUDClassPicked, InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings );
		HUDMenuSettings.OnSelectClassPicked = FOnClassPicked::CreateStatic( &LevelEditorActionHelpers::OnSelectHUDClassPicked, InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings );
		HUDMenuSettings.CurrentClass = LevelEditorActionHelpers::GetHUDClass(InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings);
		HUDMenuSettings.RootClass = AHUD::StaticClass();
		HUDMenuSettings.LevelEditor = InSettingsData.LevelEditor;
		HUDMenuSettings.bIsProjectSettings = InSettingsData.bIsProjectSettings;

		InMenuBuilder.AddSubMenu( LevelEditorActionHelpers::GetOpenHUDBlueprintLabel(InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings),
			GetSubMenuTooltip(HUDMenuSettings.CurrentClass, HUDMenuSettings.RootClass, InSettingsData.bIsProjectSettings),
			FNewMenuDelegate::CreateStatic( &LevelEditorActionHelpers::GetBlueprintSettingsSubMenu, InCommandList, HUDMenuSettings )
		);

		// Player Controller
		LevelEditorActionHelpers::FBlueprintMenuSettings PlayerControllerMenuSettings;
		PlayerControllerMenuSettings.EditCommand = 
			FUIAction(
				FExecuteAction::CreateStatic< TWeakPtr< SLevelEditor > >( &OpenPlayerControllerBlueprint, InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings )
			);
		PlayerControllerMenuSettings.OnCreateClassPicked = FOnClassPicked::CreateStatic( &LevelEditorActionHelpers::OnCreatePlayerControllerClassPicked, InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings );
		PlayerControllerMenuSettings.OnSelectClassPicked = FOnClassPicked::CreateStatic( &LevelEditorActionHelpers::OnSelectPlayerControllerClassPicked, InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings );
		PlayerControllerMenuSettings.CurrentClass = LevelEditorActionHelpers::GetPlayerControllerClass(InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings);
		PlayerControllerMenuSettings.RootClass = APlayerController::StaticClass();
		PlayerControllerMenuSettings.LevelEditor = InSettingsData.LevelEditor;
		PlayerControllerMenuSettings.bIsProjectSettings = InSettingsData.bIsProjectSettings;

		InMenuBuilder.AddSubMenu( LevelEditorActionHelpers::GetOpenPlayerControllerBlueprintLabel(InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings),
			GetSubMenuTooltip(PlayerControllerMenuSettings.CurrentClass, PlayerControllerMenuSettings.RootClass, InSettingsData.bIsProjectSettings),
			FNewMenuDelegate::CreateStatic( &LevelEditorActionHelpers::GetBlueprintSettingsSubMenu, InCommandList, PlayerControllerMenuSettings )
		);
#undef LOCTEXT_NAMESPACE
	}
}

void LevelEditorActionHelpers::GetBlueprintSettingsSubMenu(FMenuBuilder& InMenuBuilder, const TSharedRef<FUICommandList> InCommandList, FBlueprintMenuSettings InSettingsData)
{
#define LOCTEXT_NAMESPACE "LevelToolBarViewMenu"

	InMenuBuilder.PushCommandList(InCommandList);

	FSlateIcon EditBPIcon(FEditorStyle::Get().GetStyleSetName(), TEXT("PropertyWindow.Button_Edit"));
	FSlateIcon NewBPIcon(FEditorStyle::Get().GetStyleSetName(), TEXT("PropertyWindow.Button_AddToArray"));
	FText RootClassName = FText::FromString(InSettingsData.RootClass->GetName());

	// If there is currently a valid GameMode Blueprint, offer to edit the Blueprint
	if(InSettingsData.CurrentClass)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("RootClass"), RootClassName);
		Args.Add(TEXT("TargetLocation"), InSettingsData.bIsProjectSettings? LOCTEXT("Project", "project") : LOCTEXT("World", "world"));

		if(InSettingsData.CurrentClass->ClassGeneratedBy)
		{
			FText BlueprintName = FText::FromString(InSettingsData.CurrentClass->ClassGeneratedBy->GetName());
			Args.Add(TEXT("Blueprint"), BlueprintName);
			InMenuBuilder.AddMenuEntry( FText::Format( LOCTEXT("EditBlueprint", "Edit {Blueprint}"), Args), FText::Format( LOCTEXT("EditBlueprint_Tooltip", "Open the {TargetLocation}'s assigned {RootClass} blueprint"), Args), EditBPIcon, InSettingsData.EditCommand );
		}
		else
		{
			FText ClassName = FText::FromString(InSettingsData.CurrentClass->GetName());
			Args.Add(TEXT("Class"), ClassName);

			FText MenuDescription = FText::Format( LOCTEXT("EditNativeClass", "Edit {Class}.h"), Args);
			if(FSourceCodeNavigation::IsCompilerAvailable())
			{
				InMenuBuilder.AddMenuEntry( MenuDescription, FText::Format( LOCTEXT("EditNativeClass_Tooltip", "Open the {TargetLocation}'s assigned {RootClass} header"), Args), EditBPIcon, InSettingsData.EditCommand );
			}
			else
			{
				auto CannotEditClass = []() -> bool
				{
					return false;
				};

				// There is no compiler present, this is always disabled with a tooltip to explain why
				InMenuBuilder.AddMenuEntry( MenuDescription, FText::Format( LOCTEXT("CannotEditNativeClass_Tooltip", "Cannot edit the {TargetLocation}'s assigned {RootClass} header because no compiler is present!"), Args), EditBPIcon, FUIAction(FExecuteAction(), FCanExecuteAction::CreateStatic(CannotEditClass)) );
			}
		}
	}

	if(InSettingsData.bIsProjectSettings && InSettingsData.CurrentClass && InSettingsData.CurrentClass->IsChildOf(AGameModeBase::StaticClass()) && !FLevelEditorActionCallbacks::CanSelectGameModeBlueprint())
	{
		InMenuBuilder.BeginSection("CheckoutSection", LOCTEXT("CheckoutSection","Check Out Project Settings") );
		TAttribute<FText> CheckOutLabel;
		CheckOutLabel.BindStatic(&GetCheckOutLabel);

		TAttribute<FText> CheckOutTooltip;
		CheckOutTooltip.BindStatic(&GetCheckOutTooltip);
		InMenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().CheckOutProjectSettingsConfig, NAME_None, CheckOutLabel, CheckOutTooltip, FSlateIcon(FEditorStyle::Get().GetStyleSetName(), TEXT("Icons.Error")));
		InMenuBuilder.EndSection();
	}

	auto CannotCreateSelectNativeProjectGameMode = [](bool bInIsProjectSettings) -> bool
	{
		// For the project settings, we can only create/select the game mode class if the config is writable
		if(bInIsProjectSettings)
		{
			return FLevelEditorActionCallbacks::CanSelectGameModeBlueprint();
		}
		return true;
	};

	// Create a new GameMode, this is always available so the user can easily create a new one
	InMenuBuilder.AddSubMenu( LOCTEXT("CreateBlueprint", "Create..."),
		GetCreateMenuTooltip(GetGameModeClass(InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings), InSettingsData.RootClass, InSettingsData.bIsProjectSettings),
		FNewMenuDelegate::CreateStatic( &LevelEditorActionHelpers::GetCreateSettingsClassSubMenu, InSettingsData.RootClass, InSettingsData.OnCreateClassPicked ),
		FUIAction(
			FExecuteAction(), 
			InSettingsData.RootClass == AGameModeBase::StaticClass()? 
				FCanExecuteAction::CreateStatic(CannotCreateSelectNativeProjectGameMode, InSettingsData.bIsProjectSettings) 
				: FCanExecuteAction::CreateStatic( &CanCreateSelectSubClass, GetGameModeClass(InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings), InSettingsData.bIsProjectSettings )
		),
		NAME_None, EUserInterfaceActionType::Button, false, NewBPIcon
	);

	// Select a game mode, this is always available so the user can switch his selection
	FFormatNamedArguments Args;
	Args.Add(TEXT("RootClass"), RootClassName);
	InMenuBuilder.AddSubMenu(FText::Format(LOCTEXT("SelectGameModeClass", "Select {RootClass} Class"), Args),
		GetSelectMenuTooltip(GetGameModeClass(InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings), InSettingsData.RootClass, InSettingsData.bIsProjectSettings),
		FNewMenuDelegate::CreateStatic( &LevelEditorActionHelpers::GetSelectSettingsClassSubMenu, InSettingsData.RootClass, InSettingsData.OnSelectClassPicked ),
		FUIAction(
			FExecuteAction(), 
			InSettingsData.RootClass == AGameModeBase::StaticClass()?
				FCanExecuteAction::CreateStatic(CannotCreateSelectNativeProjectGameMode, InSettingsData.bIsProjectSettings) 
				: FCanExecuteAction::CreateStatic( &CanCreateSelectSubClass, GetGameModeClass(InSettingsData.LevelEditor, InSettingsData.bIsProjectSettings), InSettingsData.bIsProjectSettings )
		),
		NAME_None, EUserInterfaceActionType::Button
	);

	// For GameMode classes only, there are some sub-classes we need to add to the menu
	if(InSettingsData.RootClass == AGameModeBase::StaticClass())
	{
		InMenuBuilder.BeginSection(NAME_None, LOCTEXT("GameModeClasses", "Game Mode Classes"));

		if(InSettingsData.CurrentClass)
		{
			GetGameModeSubMenu(InMenuBuilder, InCommandList, InSettingsData);
		}

		InMenuBuilder.EndSection();
	}

	InMenuBuilder.PopCommandList();

#undef LOCTEXT_NAMESPACE
}

UClass* LevelEditorActionHelpers::GetGameModeClass(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
	UClass* GameModeClass = nullptr;
	if(bInIsProjectSettings)
	{
		UObject* GameModeObject = LoadObject<UObject>(nullptr, *UGameMapsSettings::GetGlobalDefaultGameMode());
		if(UBlueprint* GameModeAsBlueprint = Cast<UBlueprint>(GameModeObject))
		{
			GameModeClass = GameModeAsBlueprint->GeneratedClass;
		}
		else
		{
			GameModeClass = FindObject<UClass>(nullptr, *UGameMapsSettings::GetGlobalDefaultGameMode());
		}
	}
	else
	{
		AWorldSettings* WorldSettings = InLevelEditor.Pin()->GetWorld()->GetWorldSettings();
		if(WorldSettings->DefaultGameMode)
		{
			GameModeClass = WorldSettings->DefaultGameMode;
		}
	}
	return GameModeClass;
}

FText LevelEditorActionHelpers::GetOpenGameModeBlueprintLabel(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
#define LOCTEXT_NAMESPACE "LevelToolBarViewMenu"
	if(UClass* GameModeClass = GetGameModeClass(InLevelEditor, bInIsProjectSettings))
	{
		if(GameModeClass->ClassGeneratedBy)
		{
			return FText::Format( LOCTEXT("GameModeEditBlueprint", "GameMode: Edit {0}"), FText::FromString(GameModeClass->ClassGeneratedBy->GetName()));
		}

		return FText::Format( LOCTEXT("GameModeBlueprint", "GameMode: {0}"), FText::FromString(GameModeClass->GetName()));
	}

	if(bInIsProjectSettings)
	{
		return LOCTEXT("GameModeCreateBlueprint", "GameMode: New...");
	}

	// For World Settings, we want to inform the user that they are not overridding the Project Settings
	return LOCTEXT("GameModeNotOverridden", "GameMode: Not overridden!");

#undef LOCTEXT_NAMESPACE
}

void LevelEditorActionHelpers::OnCreateGameModeClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
	if(InChosenClass)
	{
		const FString NewBPName(TEXT("NewGameMode"));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromClass(NSLOCTEXT("LevelEditorCommands", "CreateGameModeBlueprint_Title", "Create GameMode Blueprint"), InChosenClass, NewBPName);

		if(Blueprint)
		{
			// @todo Re-enable once world centric works
			const bool bOpenWorldCentric = false;
			FAssetEditorManager::Get().OpenEditorForAsset(
				Blueprint,
				bOpenWorldCentric ? EToolkitMode::WorldCentric : EToolkitMode::Standalone,
				InLevelEditor.Pin()  );
			OnSelectGameModeClassPicked(Blueprint->GeneratedClass, InLevelEditor, bInIsProjectSettings);
		}
	}
	FSlateApplication::Get().DismissAllMenus();
}

void LevelEditorActionHelpers::OnSelectGameModeClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
	if(bInIsProjectSettings)
	{
		UGameMapsSettings::SetGlobalDefaultGameMode(InChosenClass? InChosenClass->GetPathName() : FString());

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			ISettingsContainerPtr SettingsContainer = SettingsModule->GetContainer("Project");

			if (SettingsContainer.IsValid())
			{
				ISettingsCategoryPtr SettingsCategory = SettingsContainer->GetCategory("Project");

				if(SettingsCategory.IsValid())
				{
					SettingsCategory->GetSection("Maps")->Save();
				}
			}
		}
	}
	else
	{
		const FScopedTransaction Transaction( NSLOCTEXT("LevelEditorCommands", "SelectGameModeClassAction", "Set Override Game Mode Class") );

		AWorldSettings* WorldSettings = InLevelEditor.Pin()->GetWorld()->GetWorldSettings();
		WorldSettings->Modify();
		WorldSettings->DefaultGameMode = InChosenClass;
	}
	FSlateApplication::Get().DismissAllMenus();
}

UClass* LevelEditorActionHelpers::GetGameStateClass(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
	if(UClass* GameModeClass = GetGameModeClass(InLevelEditor, bInIsProjectSettings))
	{
		AGameModeBase* ActiveGameMode = Cast<AGameModeBase>(GameModeClass->GetDefaultObject());
		if(ActiveGameMode)
		{
			return ActiveGameMode->GameStateClass;
		}
	}
	return NULL;
}

FText LevelEditorActionHelpers::GetOpenGameStateBlueprintLabel(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
#define LOCTEXT_NAMESPACE "LevelToolBarViewMenu"
	if(UClass* GameStateClass = GetGameStateClass(InLevelEditor, bInIsProjectSettings))
	{
		FFormatNamedArguments FormatArgs;
		if(GameStateClass->ClassGeneratedBy)
		{
			FormatArgs.Add(TEXT("GameStateName"), FText::FromString(GameStateClass->ClassGeneratedBy->GetName()));
			return FText::Format(LOCTEXT("GameStateEditBlueprint", "GameState: Edit {GameStateName}"), FormatArgs);
		}

		FormatArgs.Add(TEXT("GameStateName"), FText::FromString(GameStateClass->GetName()));
		return FText::Format(LOCTEXT("GameStateBlueprint", "GameState: {GameStateName}"), FormatArgs);
	}

	return LOCTEXT("GameStateCreateBlueprint", "GameState: New...");
#undef LOCTEXT_NAMESPACE
}

void LevelEditorActionHelpers::OnCreateGameStateClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
	if(InChosenClass)
	{
		const FString NewBPName(TEXT("NewGameState"));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromClass(NSLOCTEXT("LevelEditorCommands", "CreateGameStateBlueprint_Title", "Create GameState Blueprint"), InChosenClass, NewBPName);

		if(Blueprint)
		{
			// @todo Re-enable once world centric works
			const bool bOpenWorldCentric = false;
			FAssetEditorManager::Get().OpenEditorForAsset(
				Blueprint,
				bOpenWorldCentric ? EToolkitMode::WorldCentric : EToolkitMode::Standalone,
				InLevelEditor.Pin()  );

			OnSelectGameStateClassPicked(Blueprint->GeneratedClass, InLevelEditor, bInIsProjectSettings);
		}
	}
	FSlateApplication::Get().DismissAllMenus();
}

void LevelEditorActionHelpers::OnSelectGameStateClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
	if(UClass* GameModeClass = GetGameModeClass(InLevelEditor, bInIsProjectSettings))
	{
		const FScopedTransaction Transaction( NSLOCTEXT("LevelEditorCommands", "SelectGameStateClassAction", "Set Game State Class") );
		AGameModeBase* ActiveGameMode = Cast<AGameModeBase>(GameModeClass->GetDefaultObject());
		ActiveGameMode->GameStateClass = InChosenClass;

		UBlueprint* Blueprint = Cast<UBlueprint>(GameModeClass->ClassGeneratedBy);
		if (ensure(Blueprint))
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
	}

	FSlateApplication::Get().DismissAllMenus();
}

UClass* LevelEditorActionHelpers::GetPawnClass(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
	if(UClass* GameModeClass = GetGameModeClass(InLevelEditor, bInIsProjectSettings))
	{
		AGameModeBase* ActiveGameMode = Cast<AGameModeBase>(GameModeClass->GetDefaultObject());

		if(ActiveGameMode)
		{
			return ActiveGameMode->DefaultPawnClass;
		}
	}
	return NULL;
}

FText LevelEditorActionHelpers::GetOpenPawnBlueprintLabel(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
#define LOCTEXT_NAMESPACE "LevelToolBarViewMenu"
	if(UClass* PawnClass = GetPawnClass(InLevelEditor, bInIsProjectSettings))
	{
		FFormatNamedArguments FormatArgs;
		if(PawnClass->ClassGeneratedBy)
		{
			FormatArgs.Add(TEXT("PawnName"), FText::FromString(PawnClass->ClassGeneratedBy->GetName()));
			return FText::Format(LOCTEXT("PawnEditBlueprint", "Pawn: Edit {PawnName}"), FormatArgs);
		}

		FormatArgs.Add(TEXT("PawnName"), FText::FromString(PawnClass->GetName()));
		return FText::Format(LOCTEXT("PawnBlueprint", "Pawn: {PawnName}"), FormatArgs);
	}

	return LOCTEXT("PawnCreateBlueprint", "Pawn: New...");
#undef LOCTEXT_NAMESPACE
}

void LevelEditorActionHelpers::OnCreatePawnClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
	if(InChosenClass)
	{
		const FString NewBPName(TEXT("NewPawn"));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromClass(NSLOCTEXT("LevelEditorCommands", "CreatePawnBlueprint_Title", "Create Pawn Blueprint"), InChosenClass, NewBPName);

		if(Blueprint)
		{
			// @todo Re-enable once world centric works
			const bool bOpenWorldCentric = false;
			FAssetEditorManager::Get().OpenEditorForAsset(
				Blueprint,
				bOpenWorldCentric ? EToolkitMode::WorldCentric : EToolkitMode::Standalone,
				InLevelEditor.Pin()  );

			OnSelectPawnClassPicked(Blueprint->GeneratedClass, InLevelEditor, bInIsProjectSettings);
		}
	}
	FSlateApplication::Get().DismissAllMenus();
}

void LevelEditorActionHelpers::OnSelectPawnClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
	if(UClass* GameModeClass = GetGameModeClass(InLevelEditor, bInIsProjectSettings))
	{
		const FScopedTransaction Transaction( NSLOCTEXT("LevelEditorCommands", "SelectPawnClassAction", "Set Pawn Class") );

		AGameModeBase* ActiveGameMode = Cast<AGameModeBase>(GameModeClass->GetDefaultObject());
		ActiveGameMode->DefaultPawnClass = InChosenClass;

		UBlueprint* Blueprint = Cast<UBlueprint>(GameModeClass->ClassGeneratedBy);
		if (ensure(Blueprint))
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
	}

	FSlateApplication::Get().DismissAllMenus();
}

UClass* LevelEditorActionHelpers::GetHUDClass(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
	if(UClass* GameModeClass = GetGameModeClass(InLevelEditor, bInIsProjectSettings))
	{
		AGameModeBase* ActiveGameMode = Cast<AGameModeBase>(GameModeClass->GetDefaultObject());
		if(ActiveGameMode)
		{
			return ActiveGameMode->HUDClass;
		}
	}
	return NULL;
}

FText LevelEditorActionHelpers::GetOpenHUDBlueprintLabel(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
#define LOCTEXT_NAMESPACE "LevelToolBarViewMenu"
	if(UClass* HUDClass = GetHUDClass(InLevelEditor, bInIsProjectSettings))
	{
		FFormatNamedArguments FormatArgs;
		if (HUDClass->ClassGeneratedBy)
		{
			FormatArgs.Add(TEXT("HUDName"), FText::FromString(HUDClass->ClassGeneratedBy->GetName()));
			return FText::Format(LOCTEXT("HUDEditBlueprint", "HUD: Edit {HUDName}"), FormatArgs);
		}

		FormatArgs.Add(TEXT("HUDName"), FText::FromString(HUDClass->GetName()));
		return FText::Format(LOCTEXT("HUDBlueprint", "HUD: {HUDName}"), FormatArgs);
	}

	return LOCTEXT("HUDCreateBlueprint", "HUD: New...");
#undef LOCTEXT_NAMESPACE
}

void LevelEditorActionHelpers::OnCreateHUDClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
	if(InChosenClass)
	{
		const FString NewBPName(TEXT("NewHUD"));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromClass(NSLOCTEXT("LevelEditorCommands", "CreateHUDBlueprint_Title", "Create HUD Blueprint"), InChosenClass, NewBPName);

		if(Blueprint)
		{
			// @todo Re-enable once world centric works
			const bool bOpenWorldCentric = false;
			FAssetEditorManager::Get().OpenEditorForAsset(
				Blueprint,
				bOpenWorldCentric ? EToolkitMode::WorldCentric : EToolkitMode::Standalone,
				InLevelEditor.Pin()  );

			OnSelectHUDClassPicked(Blueprint->GeneratedClass, InLevelEditor, bInIsProjectSettings);
		}
	}
	FSlateApplication::Get().DismissAllMenus();
}

void LevelEditorActionHelpers::OnSelectHUDClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
	if(UClass* GameModeClass = GetGameModeClass(InLevelEditor, bInIsProjectSettings))
	{
		const FScopedTransaction Transaction( NSLOCTEXT("LevelEditorCommands", "SelectHUDClassAction", "Set HUD Class") );

		AGameModeBase* ActiveGameMode = Cast<AGameModeBase>(GameModeClass->GetDefaultObject());
		ActiveGameMode->HUDClass = InChosenClass;

		UBlueprint* Blueprint = Cast<UBlueprint>(GameModeClass->ClassGeneratedBy);
		if (ensure(Blueprint))
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
	}

	FSlateApplication::Get().DismissAllMenus();
}

UClass* LevelEditorActionHelpers::GetPlayerControllerClass(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
	if(UClass* GameModeClass = GetGameModeClass(InLevelEditor, bInIsProjectSettings))
	{
		AGameModeBase* ActiveGameMode = Cast<AGameModeBase>(GameModeClass->GetDefaultObject());
		if(ActiveGameMode)
		{
			return ActiveGameMode->PlayerControllerClass;
		}
	}
	return NULL;
}

FText LevelEditorActionHelpers::GetOpenPlayerControllerBlueprintLabel(TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
#define LOCTEXT_NAMESPACE "LevelToolBarViewMenu"
	if(UClass* PlayerControllerClass = GetPlayerControllerClass(InLevelEditor, bInIsProjectSettings))
	{
		FFormatNamedArguments FormatArgs;
		if (PlayerControllerClass->ClassGeneratedBy)
		{
			FormatArgs.Add(TEXT("PlayerControllerName"), FText::FromString(PlayerControllerClass->ClassGeneratedBy->GetName()));
			return FText::Format(LOCTEXT("PlayerControllerEditBlueprint", "PlayerController: Edit {PlayerControllerName}"), FormatArgs);
		}

		FormatArgs.Add(TEXT("PlayerControllerName"), FText::FromString(PlayerControllerClass->GetName()));
		return FText::Format(LOCTEXT("PlayerControllerBlueprint", "PlayerController: {PlayerControllerName}"), FormatArgs);
	}

	return LOCTEXT("PlayerControllerCreateBlueprint", "PlayerController: New...");
#undef LOCTEXT_NAMESPACE
}

void LevelEditorActionHelpers::OnCreatePlayerControllerClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
	if(InChosenClass)
	{
		const FString NewBPName(TEXT("NewPlayerController"));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromClass(NSLOCTEXT("LevelEditorCommands", "CreatePlayerControllerBlueprint_Title", "Create PlayerController Blueprint"), InChosenClass, NewBPName);

		if(Blueprint)
		{
			// @todo Re-enable once world centric works
			const bool bOpenWorldCentric = false;
			FAssetEditorManager::Get().OpenEditorForAsset(
				Blueprint,
				bOpenWorldCentric ? EToolkitMode::WorldCentric : EToolkitMode::Standalone,
				InLevelEditor.Pin()  );

			OnSelectPlayerControllerClassPicked(Blueprint->GeneratedClass, InLevelEditor, bInIsProjectSettings);
		}
	}
	FSlateApplication::Get().DismissAllMenus();
}

void LevelEditorActionHelpers::OnSelectPlayerControllerClassPicked(UClass* InChosenClass, TWeakPtr< SLevelEditor > InLevelEditor, bool bInIsProjectSettings)
{
	if(UClass* GameModeClass = GetGameModeClass(InLevelEditor, bInIsProjectSettings))
	{
		const FScopedTransaction Transaction( NSLOCTEXT("LevelEditorCommands", "SelectPlayerControllerClassAction", "Set Player Controller Class") );

		AGameModeBase* ActiveGameMode = Cast<AGameModeBase>(GameModeClass->GetDefaultObject());
		ActiveGameMode->PlayerControllerClass = InChosenClass;

		UBlueprint* Blueprint = Cast<UBlueprint>(GameModeClass->ClassGeneratedBy);
		if (ensure(Blueprint))
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
	}
	FSlateApplication::Get().DismissAllMenus();
}


/**
 * Static: Creates a widget for the level editor tool bar
 *
 * @return	New widget
 */
TSharedRef< SWidget > FLevelEditorToolBar::MakeLevelEditorToolBar( const TSharedRef<FUICommandList>& InCommandList, const TSharedRef<SLevelEditor> InLevelEditor )
{
#define LOCTEXT_NAMESPACE "LevelEditorToolBar"

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<FExtender> Extenders = LevelEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders();

	static const FName LevelEditorToolBarName = "LevelEditorToolBar";
	FToolBarBuilder ToolbarBuilder( InCommandList, FMultiBoxCustomization::AllowCustomization( LevelEditorToolBarName ), Extenders );

	ToolbarBuilder.BeginSection("File");
	{
		// Save All Levels
		ToolbarBuilder.AddToolBarButton( FLevelEditorCommands::Get().Save, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "AssetEditor.SaveAsset") );

		// Source control buttons
		{
			enum EQueryState
			{
				NotQueried,
				Querying,
				Queried,
			};

			static EQueryState QueryState = EQueryState::NotQueried;

			struct FSourceControlStatus
			{
				static void CheckSourceControlStatus()
				{
					ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
					if (SourceControlModule.IsEnabled())
					{
						SourceControlModule.GetProvider().Execute(ISourceControlOperation::Create<FConnect>(),
																  EConcurrency::Asynchronous,
																  FSourceControlOperationComplete::CreateStatic(&FSourceControlStatus::OnSourceControlOperationComplete));
						QueryState = EQueryState::Querying;
					}
				}

				static void OnSourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
				{
					QueryState = EQueryState::Queried;
				}

				static FText GetSourceControlTooltip()
				{
					if (QueryState == EQueryState::Querying)
					{
						return LOCTEXT("SourceControlUnknown", "Source control status is unknown");
					}
					else
					{
						return ISourceControlModule::Get().GetProvider().GetStatusText();
					}
				}

				static FSlateIcon GetSourceControlIcon()
				{
					if (QueryState == EQueryState::Querying)
					{
						return FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.SourceControl.Unknown");
					}
					else
					{
						ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
						if (SourceControlModule.IsEnabled())
						{
							if (!SourceControlModule.GetProvider().IsAvailable())
							{
								return FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.SourceControl.Error");
							}
							else
							{
								return FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.SourceControl.On");
							}
						}
						else
						{
							return FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.SourceControl.Off");
						}
					}
				}
			};

			FSourceControlStatus::CheckSourceControlStatus();

			ToolbarBuilder.AddComboButton(
				FUIAction(),
				FOnGetContent::CreateStatic(&FLevelEditorToolBar::GenerateSourceControlMenu, InCommandList),
				LOCTEXT("SourceControl_Label", "Source Control"),
				TAttribute<FText>::Create(&FSourceControlStatus::GetSourceControlTooltip),
				TAttribute<FSlateIcon>::Create(&FSourceControlStatus::GetSourceControlIcon),
				false
				);
		}
	}
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("Content");
	{
		ToolbarBuilder.AddToolBarButton( FLevelEditorCommands::Get().OpenContentBrowser, NAME_None, LOCTEXT( "ContentBrowser_Override", "Content" ), TAttribute<FText>(), TAttribute<FSlateIcon>(), "LevelToolbarContent" );
		if (FLauncherPlatformModule::Get()->CanOpenLauncher(true)) 
		{
			ToolbarBuilder.AddToolBarButton(FLevelEditorCommands::Get().OpenMarketplace, NAME_None, LOCTEXT("Marketplace_Override", "Marketplace"), TAttribute<FText>(), TAttribute<FSlateIcon>(), "LevelToolbarMarketplace");
		}
	}
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("Settings");
	{
		ToolbarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateStatic(&FLevelEditorToolBar::GenerateQuickSettingsMenu, InCommandList),
			LOCTEXT("QuickSettingsCombo", "Settings"),
			LOCTEXT("QuickSettingsCombo_ToolTip", "Project and Editor settings"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.GameSettings"),
			false,
			"LevelToolbarQuickSettings"
			);

	}
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection( NAME_None );
	{
		ToolbarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateStatic( &FLevelEditorToolBar::GenerateOpenBlueprintMenuContent, InCommandList, TWeakPtr<SLevelEditor>( InLevelEditor ) ),
			LOCTEXT( "OpenBlueprint_Label", "Blueprints" ),
			LOCTEXT( "OpenBlueprint_ToolTip", "List of world Blueprints available to the user for editing or creation." ),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.OpenLevelBlueprint")
			);

		ToolbarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateStatic( &FLevelEditorToolBar::GenerateCinematicsMenuContent, InCommandList, TWeakPtr<SLevelEditor>( InLevelEditor ) ),
			LOCTEXT( "EditCinematics_Label", "Cinematics" ),
			LOCTEXT( "EditCinematics_Tooltip", "Displays a list of Matinee and Level Sequence objects to open in their respective editors"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.EditMatinee") 
			);

		ToolbarBuilder.AddToolBarButton( FLevelEditorCommands::Get().ToggleVR, NAME_None, LOCTEXT("ToggleVR", "VR Mode") );
	}
	ToolbarBuilder.EndSection();
	
	ToolbarBuilder.BeginSection("Compile");
	{
		// Build			
		ToolbarBuilder.AddToolBarButton( FLevelEditorCommands::Get().Build, NAME_None, LOCTEXT("BuildAll", "Build") );

		// Build menu drop down
		ToolbarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateStatic( &FLevelEditorToolBar::GenerateBuildMenuContent, InCommandList ),
			LOCTEXT( "BuildCombo_Label", "Build Options" ),
			LOCTEXT( "BuildComboToolTip", "Build options menu" ),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Build"),
			true);

		// Only show the compile options on machines with the solution (assuming they can build it)
		if ( FSourceCodeNavigation::IsCompilerAvailable() )
		{
			// Since we can always add new code to the project, only hide these buttons if we haven't done so yet
			ToolbarBuilder.AddToolBarButton(
				FUIAction(
					FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::RecompileGameCode_Clicked),
					FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::Recompile_CanExecute),
					FIsActionChecked(),
					FIsActionButtonVisible::CreateStatic(FLevelEditorActionCallbacks::CanShowSourceCodeActions)),
				NAME_None,
				LOCTEXT( "CompileMenuButton", "Compile" ),
				FLevelEditorCommands::Get().RecompileGameCode->GetDescription(),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Recompile")
				);
		}
	}
	ToolbarBuilder.EndSection();
	
	ToolbarBuilder.BeginSection("Game");
	{
		// Add the shared play-world commands that will be shown on the Kismet toolbar as well
		FPlayWorldCommands::BuildToolbar(ToolbarBuilder, true);
	}
	ToolbarBuilder.EndSection();


	// Create the tool bar!
	return
		SNew( SBorder )
		.Padding(0)
		.BorderImage( FEditorStyle::GetBrush("NoBorder") )
		.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() )
		[
			ToolbarBuilder.MakeWidget()
		];
#undef LOCTEXT_NAMESPACE
}



TSharedRef< SWidget > FLevelEditorToolBar::GenerateBuildMenuContent( TSharedRef<FUICommandList> InCommandList )
{
#define LOCTEXT_NAMESPACE "LevelToolBarBuildMenu"

	// Get all menu extenders for this context menu from the level editor module
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
	TArray<FLevelEditorModule::FLevelEditorMenuExtender> MenuExtenderDelegates = LevelEditorModule.GetAllLevelEditorToolbarBuildMenuExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute(InCommandList));
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, InCommandList, MenuExtender );

	struct FLightingMenus
	{

		/** Generates a lighting quality sub-menu */
		static void MakeLightingQualityMenu( FMenuBuilder& InMenuBuilder )
		{
			InMenuBuilder.BeginSection("LevelEditorBuildLightingQuality", LOCTEXT( "LightingQualityHeading", "Quality Level" ) );
			{
				InMenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().LightingQuality_Production );
				InMenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().LightingQuality_High );
				InMenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().LightingQuality_Medium );
				InMenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().LightingQuality_Preview );
			}
			InMenuBuilder.EndSection();
		}

		/** Generates a lighting density sub-menu */
		static void MakeLightingDensityMenu( FMenuBuilder& InMenuBuilder )
		{
			InMenuBuilder.BeginSection("LevelEditorBuildLightingDensity", LOCTEXT( "LightingDensityHeading", "Density Rendering" ) );
			{
				TSharedRef<SWidget> Ideal =		SNew(SHorizontalBox)
												+SHorizontalBox::Slot()
												.Padding( FMargin( 27.0f, 0.0f, 0.0f, 0.0f ) )
												.FillWidth(1.0f)
												[
													SNew(SSpinBox<float>)
													.MinValue(0.f)
													.MaxValue(100.f)
													.Value(FLevelEditorActionCallbacks::GetLightingDensityIdeal())
													.OnValueChanged_Static(&FLevelEditorActionCallbacks::SetLightingDensityIdeal)
												];
				InMenuBuilder.AddWidget(Ideal, LOCTEXT("LightingDensity_Ideal","Ideal Density"));
				
				TSharedRef<SWidget> Maximum =	SNew(SHorizontalBox)
												+SHorizontalBox::Slot()
												.FillWidth(1.0f)
												[
													SNew(SSpinBox<float>)
													.MinValue(0.01f)
													.MaxValue(100.01f)
													.Value(FLevelEditorActionCallbacks::GetLightingDensityMaximum())
													.OnValueChanged_Static(&FLevelEditorActionCallbacks::SetLightingDensityMaximum)
												];
				InMenuBuilder.AddWidget(Maximum, LOCTEXT("LightingDensity_Maximum","Maximum Density"));

				TSharedRef<SWidget> ClrScale =	SNew(SHorizontalBox)
												+SHorizontalBox::Slot()
												.Padding( FMargin( 35.0f, 0.0f, 0.0f, 0.0f ) )
												.FillWidth(1.0f)
												[
													SNew(SSpinBox<float>)
													.MinValue(0.f)
													.MaxValue(10.f)
													.Value(FLevelEditorActionCallbacks::GetLightingDensityColorScale())
													.OnValueChanged_Static(&FLevelEditorActionCallbacks::SetLightingDensityColorScale)
												];
				InMenuBuilder.AddWidget(ClrScale, LOCTEXT("LightingDensity_ColorScale","Color Scale"));

				TSharedRef<SWidget> GrayScale =	SNew(SHorizontalBox)
												+SHorizontalBox::Slot()
												.Padding( FMargin( 11.0f, 0.0f, 0.0f, 0.0f ) )
												.FillWidth(1.0f)
												[
													SNew(SSpinBox<float>)
													.MinValue(0.f)
													.MaxValue(10.f)
													.Value(FLevelEditorActionCallbacks::GetLightingDensityGrayscaleScale())
													.OnValueChanged_Static(&FLevelEditorActionCallbacks::SetLightingDensityGrayscaleScale)
												];
				InMenuBuilder.AddWidget(GrayScale, LOCTEXT("LightingDensity_GrayscaleScale","Grayscale Scale"));

				InMenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().LightingDensity_RenderGrayscale );
			}
			InMenuBuilder.EndSection();
		}

		/** Generates a lighting resolution sub-menu */
		static void MakeLightingResolutionMenu( FMenuBuilder& InMenuBuilder )
		{
			InMenuBuilder.BeginSection("LevelEditorBuildLightingResolution1", LOCTEXT( "LightingResolutionHeading1", "Primitive Types" ) );
			{
				TSharedRef<SWidget> Meshes =	SNew(SHorizontalBox)
												+SHorizontalBox::Slot()
												.AutoWidth()
												[
													SNew( SCheckBox )
													.Style( FEditorStyle::Get(), "Menu.CheckBox" )
													.ToolTipText(LOCTEXT( "StaticMeshesToolTip", "Static Meshes will be adjusted if checked." ))
													.IsChecked_Static(&FLevelEditorActionCallbacks::IsLightingResolutionStaticMeshesChecked)
													.OnCheckStateChanged_Static(&FLevelEditorActionCallbacks::SetLightingResolutionStaticMeshes)
													.Content()
													[
														SNew( STextBlock )
														.Text( LOCTEXT("StaticMeshes", "Static Meshes") )
													]
												]
												+SHorizontalBox::Slot()
												.AutoWidth()
												.Padding( FMargin( 4.0f, 0.0f, 11.0f, 0.0f ) )
												[
													SNew(SSpinBox<float>)
													.MinValue(4.f)
													.MaxValue(4096.f)
													.ToolTipText(LOCTEXT( "LightingResolutionStaticMeshesMinToolTip", "The minimum lightmap resolution for static mesh adjustments. Anything outside of Min/Max range will not be touched when adjusting." ))
													.Value(FLevelEditorActionCallbacks::GetLightingResolutionMinSMs())
													.OnValueChanged_Static(&FLevelEditorActionCallbacks::SetLightingResolutionMinSMs)
												]
												+SHorizontalBox::Slot()
												.AutoWidth()
												[
													SNew(SSpinBox<float>)
													.MinValue(4.f)
													.MaxValue(4096.f)
													.ToolTipText(LOCTEXT( "LightingResolutionStaticMeshesMaxToolTip", "The maximum lightmap resolution for static mesh adjustments. Anything outside of Min/Max range will not be touched when adjusting." ))
													.Value(FLevelEditorActionCallbacks::GetLightingResolutionMaxSMs())
													.OnValueChanged_Static(&FLevelEditorActionCallbacks::SetLightingResolutionMaxSMs)
												];
				InMenuBuilder.AddWidget(Meshes, FText::GetEmpty(), true);
				
				TSharedRef<SWidget> BSPs =		SNew(SHorizontalBox)
												+SHorizontalBox::Slot()
												.AutoWidth()
												[
													SNew( SCheckBox )
													.Style(FEditorStyle::Get(), "Menu.CheckBox")
													.ToolTipText(LOCTEXT( "BSPSurfacesToolTip", "BSP Surfaces will be adjusted if checked." ))
													.IsChecked_Static(&FLevelEditorActionCallbacks::IsLightingResolutionBSPSurfacesChecked)
													.OnCheckStateChanged_Static(&FLevelEditorActionCallbacks::SetLightingResolutionBSPSurfaces)
													.Content()
													[
														SNew( STextBlock )
														.Text( LOCTEXT("BSPSurfaces", "BSP Surfaces") )
													]
												]
												+SHorizontalBox::Slot()
												.AutoWidth()
												.Padding( FMargin( 6.0f, 0.0f, 4.0f, 0.0f ) )
												[
													SNew(SSpinBox<float>)
													.MinValue(1.f)
													.MaxValue(63556.f)
													.ToolTipText(LOCTEXT( "LightingResolutionBSPsMinToolTip", "The minimum lightmap resolution of a BSP surface to adjust. When outside of the Min/Max range, the BSP surface will no be altered." ))
													.Value(FLevelEditorActionCallbacks::GetLightingResolutionMinBSPs())
													.OnValueChanged_Static(&FLevelEditorActionCallbacks::SetLightingResolutionMinBSPs)
												]
												+SHorizontalBox::Slot()
												.AutoWidth()
												[
													SNew(SSpinBox<float>)
													.MinValue(1.f)
													.MaxValue(63556.f)
													.ToolTipText(LOCTEXT( "LightingResolutionBSPsMaxToolTip", "The maximum lightmap resolution of a BSP surface to adjust. When outside of the Min/Max range, the BSP surface will no be altered." ))
													.Value(FLevelEditorActionCallbacks::GetLightingResolutionMaxBSPs())
													.OnValueChanged_Static(&FLevelEditorActionCallbacks::SetLightingResolutionMaxBSPs)
												];
				InMenuBuilder.AddWidget(BSPs, FText::GetEmpty(), true);
			}
			InMenuBuilder.EndSection(); //LevelEditorBuildLightingResolution1

			InMenuBuilder.BeginSection("LevelEditorBuildLightingResolution2", LOCTEXT( "LightingResolutionHeading2", "Select Options" ) );
			{
				InMenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().LightingResolution_CurrentLevel );
				InMenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().LightingResolution_SelectedLevels );
				InMenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().LightingResolution_AllLoadedLevels );
				InMenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().LightingResolution_SelectedObjectsOnly );
			}
			InMenuBuilder.EndSection();

			InMenuBuilder.BeginSection("LevelEditorBuildLightingResolution3", LOCTEXT( "LightingResolutionHeading3", "Ratio" ) );
			{
				TSharedRef<SWidget> Ratio =		SNew(SSpinBox<int32>)
												.MinValue(0)
												.MaxValue(400)
												.ToolTipText(LOCTEXT( "LightingResolutionRatioToolTip", "Ratio to apply (New Resolution = Ratio / 100.0f * CurrentResolution)." ))
												.Value(FLevelEditorActionCallbacks::GetLightingResolutionRatio())
												.OnEndSliderMovement_Static(&FLevelEditorActionCallbacks::SetLightingResolutionRatio)
												.OnValueCommitted_Static(&FLevelEditorActionCallbacks::SetLightingResolutionRatioCommit);
				InMenuBuilder.AddWidget(Ratio, LOCTEXT( "LightingResolutionRatio", "Ratio" ));
			}
			InMenuBuilder.EndSection();
		}

		/** Generates a lighting info dialogs sub-menu */
		static void MakeLightingInfoMenu( FMenuBuilder& InMenuBuilder )
		{
			InMenuBuilder.BeginSection("LevelEditorBuildLightingInfo", LOCTEXT( "LightingInfoHeading", "Lighting Info Dialogs" ) );
			{
				InMenuBuilder.AddSubMenu(
					LOCTEXT( "LightingDensityRenderingSubMenu", "LightMap Density Rendering Options" ),
					LOCTEXT( "LightingDensityRenderingSubMenu_ToolTip", "Shows the LightMap Density Rendering viewmode options." ),
					FNewMenuDelegate::CreateStatic( &FLightingMenus::MakeLightingDensityMenu ) );

				InMenuBuilder.AddSubMenu(
					LOCTEXT( "LightingResolutionAdjustmentSubMenu", "LightMap Resolution Adjustment" ),
					LOCTEXT( "LightingResolutionAdjustmentSubMenu_ToolTip", "Shows the LightMap Resolution Adjustment options." ),
					FNewMenuDelegate::CreateStatic( &FLightingMenus::MakeLightingResolutionMenu ) );

				InMenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().LightingStaticMeshInfo, NAME_None, LOCTEXT( "BuildLightingInfo_LightingStaticMeshInfo", "Lighting StaticMesh Info..." ) );
			}
			InMenuBuilder.EndSection();
		}
	};

	MenuBuilder.BeginSection("LevelEditorLighting", LOCTEXT( "LightingHeading", "Lighting" ) );
	{
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().BuildLightingOnly, NAME_None, LOCTEXT( "BuildLightingOnlyHeading", "Build Lighting Only" ) );

		MenuBuilder.AddSubMenu(
			LOCTEXT( "LightingQualitySubMenu", "Lighting Quality" ),
			LOCTEXT( "LightingQualitySubMenu_ToolTip", "Allows you to select the quality level for precomputed lighting" ),
			FNewMenuDelegate::CreateStatic( &FLightingMenus::MakeLightingQualityMenu ) );

		MenuBuilder.AddSubMenu(
			LOCTEXT( "BuildLightingInfoSubMenu", "Lighting Info" ),
			LOCTEXT( "BuildLightingInfoSubMenu_ToolTip", "Access the lighting info dialogs" ),
			FNewMenuDelegate::CreateStatic( &FLightingMenus::MakeLightingInfoMenu ) );

		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().LightingBuildOptions_UseErrorColoring );
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().LightingBuildOptions_ShowLightingStats );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelEditorReflections", LOCTEXT( "ReflectionHeading", "Reflections" ) );
	{
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().BuildReflectionCapturesOnly );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelEditorVisibility", LOCTEXT( "VisibilityHeading", "Visibility" ) );
	{
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().BuildLightingOnly_VisibilityOnly );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelEditorGeometry", LOCTEXT( "GeometryHeading", "Geometry" ) );
	{
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().BuildGeometryOnly );
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().BuildGeometryOnly_OnlyCurrentLevel );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelEditorNavigation", LOCTEXT( "NavigationHeading", "Navigation" ) );
	{
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().BuildPathsOnly );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelEditorLOD", LOCTEXT("LODHeading", "Hierarchical LOD"));
	{
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().BuildLODsOnly);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelEditorTextureStreaming", LOCTEXT("TextureStreamingHeading", "Texture Streaming"));
	if (CVarStreamingUseNewMetrics.GetValueOnAnyThread() != 0) // There is not point of in building texture streaming data with the old system.
	{
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().BuildTextureStreamingOnly);
	}
	MenuBuilder.EndSection();


	MenuBuilder.BeginSection("LevelEditorAutomation", LOCTEXT( "AutomationHeading", "Automation" ) );
	{
		MenuBuilder.AddMenuEntry( 
			FLevelEditorCommands::Get().BuildAndSubmitToSourceControl,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.BuildAndSubmit")
			);
	}
	MenuBuilder.EndSection();

	// Map Check
	MenuBuilder.BeginSection("LevelEditorVerification", LOCTEXT( "VerificationHeading", "Verification" ) );
	{
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().MapCheck, NAME_None, LOCTEXT("OpenMapCheck", "Map Check") );
	}
	MenuBuilder.EndSection();

#undef LOCTEXT_NAMESPACE

	return MenuBuilder.MakeWidget();
}

static void MakeES2PreviewPlatformOverrideMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("LevelEditorShaderModelPreview", NSLOCTEXT("LevelToolBarViewMenu", "ES2PreviewPlatformOverrideHeading", "Preview Platform"));
	{
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().PreviewPlatformOverride_DefaultES2);
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().PreviewPlatformOverride_AndroidGLES2);
	}
	MenuBuilder.EndSection();
}

static void MakeES31PreviewPlatformOverrideMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("LevelEditorShaderModelPreview", NSLOCTEXT("LevelToolBarViewMenu", "ES31PreviewPlatformOverrideHeading", "Preview Platform"));
	{
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().PreviewPlatformOverride_DefaultES31);

		bool bAndroidBuildForES31 = false;
		bool bAndroidSupportsVulkan = false;
		bool bIOSSupportsMetal = false;
		GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bBuildForES31"), bAndroidBuildForES31, GEngineIni);
		GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bSupportsVulkan"), bAndroidSupportsVulkan, GEngineIni);
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetal"), bIOSSupportsMetal, GEngineIni);

		if(bAndroidBuildForES31)
		{
			MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().PreviewPlatformOverride_AndroidGLES31);
		}
		if(bAndroidSupportsVulkan)
		{
			MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().PreviewPlatformOverride_AndroidVulkanES31);
		}
		if(bIOSSupportsMetal)
		{
			MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().PreviewPlatformOverride_IOSMetalES31);
		}
	}
	MenuBuilder.EndSection();
}

static void MakeMaterialQualityLevelMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.BeginSection("LevelEditorMaterialQualityLevel", NSLOCTEXT( "LevelToolBarViewMenu", "MaterialQualityLevelHeading", "Material Quality Level" ) );
	{
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().MaterialQualityLevel_Low);
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().MaterialQualityLevel_Medium);
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().MaterialQualityLevel_High);
	}
	MenuBuilder.EndSection();
}

static void MakeShaderModelPreviewMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("LevelEditorShaderModelPreview", NSLOCTEXT("LevelToolBarViewMenu", "FeatureLevelPreviewHeading", "Preview Rendering Level"));
	{
		for (int32 i = GMaxRHIFeatureLevel; i >= 0; --i)
		{
			switch (i)
			{
				case ERHIFeatureLevel::ES2:
					MenuBuilder.AddSubMenu(
						FLevelEditorCommands::Get().FeatureLevelPreview[i]->GetLabel(),
						FLevelEditorCommands::Get().FeatureLevelPreview[i]->GetDescription(),
						FNewMenuDelegate::CreateStatic(&MakeES2PreviewPlatformOverrideMenu));
					break;
				case ERHIFeatureLevel::ES3_1:
					MenuBuilder.AddSubMenu(
						FLevelEditorCommands::Get().FeatureLevelPreview[i]->GetLabel(),
						FLevelEditorCommands::Get().FeatureLevelPreview[i]->GetDescription(),
						FNewMenuDelegate::CreateStatic(&MakeES31PreviewPlatformOverrideMenu));
					break;
				default:
					MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().FeatureLevelPreview[i]);
			}
		}
	}
	MenuBuilder.EndSection();
}

static void MakeScalabilityMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.AddWidget(SNew(SScalabilitySettings), FText(), true);
}

static void MakePreviewSettingsMenu( FMenuBuilder& MenuBuilder )
{
#define LOCTEXT_NAMESPACE "LevelToolBarViewMenu"

	MenuBuilder.BeginSection("LevelEditorPreview", LOCTEXT("PreviewHeading", "Previewing"));
	{
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().DrawBrushMarkerPolys);
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().OnlyLoadVisibleInPIE);
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().ToggleParticleSystemLOD);
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().ToggleParticleSystemHelpers);
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().ToggleFreezeParticleSimulation);
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().ToggleLODViewLocking);
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().LevelStreamingVolumePrevis);
	}
	MenuBuilder.EndSection();
#undef LOCTEXT_NAMESPACE
}

TSharedRef< SWidget > FLevelEditorToolBar::GenerateQuickSettingsMenu( TSharedRef<FUICommandList> InCommandList )
{
#define LOCTEXT_NAMESPACE "LevelToolBarViewMenu"

	// Get all menu extenders for this context menu from the level editor module
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
	TArray<FLevelEditorModule::FLevelEditorMenuExtender> MenuExtenderDelegates = LevelEditorModule.GetAllLevelEditorToolbarViewMenuExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute(InCommandList));
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, InCommandList, MenuExtender );

	struct Local
	{
		static void OpenSettings(FName ContainerName, FName CategoryName, FName SectionName)
		{
			FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer(ContainerName, CategoryName, SectionName);
		}
	};

	MenuBuilder.BeginSection("ProjectSettingsSection", LOCTEXT("ProjectSettings","Game Specific Settings") );
	{

		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().WorldProperties);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ProjectSettingsMenuLabel", "Project Settings..."),
			LOCTEXT("ProjectSettingsMenuToolTip", "Change the settings of the currently loaded project"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "ProjectSettings.TabIcon"),
			FUIAction(FExecuteAction::CreateStatic(&Local::OpenSettings, FName("Project"), FName("Project"), FName("General")))
			);

		if (IModularFeatures::Get().IsModularFeatureAvailable(EditorFeatures::PluginsEditor))
		{
			FGlobalTabmanager::Get()->PopulateTabSpawnerMenu(MenuBuilder, "PluginsEditor");
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelEditorSelection", LOCTEXT("SelectionHeading","Selection") );
	{
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().AllowTranslucentSelection );
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().AllowGroupSelection );
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().StrictBoxSelect );
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().TransparentBoxSelect );
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().ShowTransformWidget );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelEditorScalability", LOCTEXT("ScalabilityHeading", "Scalability") );
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT( "ScalabilitySubMenu", "Engine Scalability Settings" ),
			LOCTEXT( "ScalabilitySubMenu_ToolTip", "Open the engine scalability settings" ),
			FNewMenuDelegate::CreateStatic( &MakeScalabilityMenu ) );

		MenuBuilder.AddSubMenu(
			LOCTEXT( "MaterialQualityLevelSubMenu", "Material Quality Level" ),
			LOCTEXT( "MaterialQualityLevelSubMenu_ToolTip", "Sets the value of the CVar \"r.MaterialQualityLevel\" (low=0, high=1, medium=2). This affects materials via the QualitySwitch material expression." ),
			FNewMenuDelegate::CreateStatic( &MakeMaterialQualityLevelMenu ) );

		MenuBuilder.AddSubMenu(
			LOCTEXT("FeatureLevelPreviewSubMenu", "Preview Rendering Level"),
			LOCTEXT("FeatureLevelPreviewSubMenu_ToolTip", "Sets the rendering level used by the main editor"),
			FNewMenuDelegate::CreateStatic(&MakeShaderModelPreviewMenu));
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelEditorAudio", LOCTEXT("AudioHeading", "Real Time Audio") );
	{
		TSharedRef<SWidget> VolumeItem = SNew(SHorizontalBox)
											+SHorizontalBox::Slot()
											.FillWidth(0.9f)
											.Padding( FMargin(2.0f, 0.0f, 0.0f, 0.0f) )
											[
												SNew(SVolumeControl)
												.ToolTipText_Static(&FLevelEditorActionCallbacks::GetAudioVolumeToolTip)
												.Volume_Static(&FLevelEditorActionCallbacks::GetAudioVolume)
												.OnVolumeChanged_Static(&FLevelEditorActionCallbacks::OnAudioVolumeChanged)
												.Muted_Static(&FLevelEditorActionCallbacks::GetAudioMuted)
												.OnMuteChanged_Static(&FLevelEditorActionCallbacks::OnAudioMutedChanged)
											]
											+SHorizontalBox::Slot()
											.FillWidth(0.1f);

		MenuBuilder.AddWidget(VolumeItem, LOCTEXT("VolumeControlLabel","Volume"));
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "Snapping", LOCTEXT("SnappingHeading","Snapping") );
	{
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().EnableActorSnap );
		TSharedRef<SWidget> SnapItem = 
		SNew(SHorizontalBox)
	          +SHorizontalBox::Slot()
	          .FillWidth(0.9f)
	          [
		          SNew(SSlider)
		          .ToolTipText_Static(&FLevelEditorActionCallbacks::GetActorSnapTooltip)
		          .Value_Static(&FLevelEditorActionCallbacks::GetActorSnapSetting)
		          .OnValueChanged_Static(&FLevelEditorActionCallbacks::SetActorSnapSetting)
	          ]
	          +SHorizontalBox::Slot()
	          .FillWidth(0.1f);
		MenuBuilder.AddWidget(SnapItem, LOCTEXT("ActorSnapLabel","Distance"));

		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().ToggleSocketSnapping );
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().EnableVertexSnap );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelEditorViewport", LOCTEXT("ViewportHeading", "Viewport") );
	{
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().ToggleHideViewportUI );

		MenuBuilder.AddSubMenu( LOCTEXT("PreviewMenu", "Previewing"), LOCTEXT("PreviewMenuTooltip","Game Preview Settings"), FNewMenuDelegate::CreateStatic( &MakePreviewSettingsMenu ) );
	}
	MenuBuilder.EndSection();

#undef LOCTEXT_NAMESPACE

	return MenuBuilder.MakeWidget();
}


TSharedRef< SWidget > FLevelEditorToolBar::GenerateSourceControlMenu(TSharedRef<FUICommandList> InCommandList)
{
#define LOCTEXT_NAMESPACE "LevelToolBarSourceControlMenu"

	// Get all menu extenders for this context menu from the level editor module
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TArray<FLevelEditorModule::FLevelEditorMenuExtender> MenuExtenderDelegates = LevelEditorModule.GetAllLevelEditorToolbarSourceControlMenuExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute(InCommandList));
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, InCommandList, MenuExtender);

	MenuBuilder.BeginSection("SourceControlActions", LOCTEXT("SourceControlMenuHeadingActions", "Actions"));

	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	if (ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable())
	{
		MenuBuilder.AddMenuEntry(
			FLevelEditorCommands::Get().ChangeSourceControlSettings, 
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.ChangeSettings")
			);
	}
	else
	{
		MenuBuilder.AddMenuEntry(
			FLevelEditorCommands::Get().ConnectToSourceControl,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Connect")
			);
	}

	MenuBuilder.AddMenuEntry(
		FLevelEditorCommands::Get().CheckOutModifiedFiles,
		NAME_None,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.CheckOut")
		);


	MenuBuilder.AddMenuEntry(
		FLevelEditorCommands::Get().SubmitToSourceControl,
		NAME_None,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Submit")
		);

	MenuBuilder.EndSection();

#undef LOCTEXT_NAMESPACE

	return MenuBuilder.MakeWidget();
}

TSharedRef< SWidget > FLevelEditorToolBar::GenerateOpenBlueprintMenuContent( TSharedRef<FUICommandList> InCommandList, TWeakPtr< SLevelEditor > InLevelEditor )
{
#define LOCTEXT_NAMESPACE "LevelToolBarViewMenu"

	struct FBlueprintMenus
	{
		/** Generates a sub-level Blueprints sub-menu */
		static void MakeSubLevelsMenu(FMenuBuilder& InMenuBuilder, TWeakPtr< SLevelEditor > InLvlEditor)
		{
			FSlateIcon EditBP(FEditorStyle::Get().GetStyleSetName(), TEXT("LevelEditor.OpenLevelBlueprint"));

			InMenuBuilder.BeginSection(NAME_None, LOCTEXT("SubLevelsHeading", "Sub-Level Blueprints"));
			{
				UWorld* World = InLvlEditor.Pin()->GetWorld();
				for (int32 iLevel = 0; iLevel < World->GetNumLevels(); iLevel++)
				{
					ULevel* Level = World->GetLevel(iLevel);
					if (Level != NULL && Level->GetOutermost() != NULL)
					{
						if (!Level->IsPersistentLevel())
						{
							FUIAction UIAction
								(
								FExecuteAction::CreateStatic(&FLevelEditorToolBar::OnOpenSubLevelBlueprint, Level)
								);

							FText DisplayName = FText::Format(LOCTEXT("SubLevelBlueprintItem", "Edit {0}"), FText::FromString(FPaths::GetCleanFilename(Level->GetOutermost()->GetName())));
							InMenuBuilder.AddMenuEntry(DisplayName, FText::GetEmpty(), EditBP, UIAction);
						}
					}
				}
			}
			InMenuBuilder.EndSection();
		}

		/** Handle BP being selected from popup picker */
		static void OnBPSelected(const struct FAssetData& AssetData)
		{
			UBlueprint* SelectedBP = Cast<UBlueprint>(AssetData.GetAsset());
			if(SelectedBP)
			{
				FAssetEditorManager::Get().OpenEditorForAsset(SelectedBP);
			}
		}


		/** Generates 'open blueprint' sub-menu */
		static void MakeOpenBPClassMenu(FMenuBuilder& InMenuBuilder)
		{
			FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

			// Configure filter for asset picker
			FAssetPickerConfig Config;
			Config.Filter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());
			Config.InitialAssetViewType = EAssetViewType::List;
			Config.OnAssetSelected = FOnAssetSelected::CreateStatic(&FBlueprintMenus::OnBPSelected);
			Config.bAllowDragging = false;
			// Don't show stuff in Engine
			Config.Filter.PackagePaths.Add("/Game");
			Config.Filter.bRecursivePaths = true;

			TSharedRef<SWidget> Widget = 
				SNew(SBox)
				.WidthOverride(300.f)
				.HeightOverride(300.f)
				[
					ContentBrowserModule.Get().CreateAssetPicker(Config)
				];
		

			InMenuBuilder.BeginSection(NAME_None, LOCTEXT("BrowseHeader", "Browse"));
			{
				InMenuBuilder.AddWidget(Widget, FText::GetEmpty());
			}
			InMenuBuilder.EndSection();
		}
	};


	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, InCommandList );

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("BlueprintClass", "Blueprint Class"));
	{
		// Create a blank BP
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().CreateBlankBlueprintClass);

		// Convert selection to BP
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().ConvertSelectionToBlueprintViaHarvest);
		MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().ConvertSelectionToBlueprintViaSubclass);

		// Open an existing Blueprint Class...
		FSlateIcon OpenBPIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.OpenClassBlueprint");
		MenuBuilder.AddSubMenu(
			LOCTEXT("OpenBlueprintClassSubMenu", "Open Blueprint Class..."),
			LOCTEXT("OpenBlueprintClassSubMenu_ToolTip", "Open an existing Blueprint Class in this project"),
			FNewMenuDelegate::CreateStatic(&FBlueprintMenus::MakeOpenBPClassMenu),
			false,
			OpenBPIcon);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("LevelScriptBlueprints", "Level Blueprints"));
	{
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().OpenLevelBlueprint );

		// If there are any sub-levels, display the sub-menu. A single level means there is only the persistent level
		UWorld* World = InLevelEditor.Pin()->GetWorld();
		if(World->GetNumLevels() > 1)
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT( "SubLevelsSubMenu", "Sub-Levels" ),
				LOCTEXT( "SubLevelsSubMenu_ToolTip", "Shows available sub-level Blueprints that can be edited." ),
				FNewMenuDelegate::CreateStatic( &FBlueprintMenus::MakeSubLevelsMenu, InLevelEditor ), 
				FUIAction(), NAME_None, EUserInterfaceActionType::Button, false, FSlateIcon(FEditorStyle::Get().GetStyleSetName(), TEXT("LevelEditor.OpenLevelBlueprint")) );
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ProjectSettingsClasses", "Project Settings"));
	{
		// If source control is enabled, queue up a query to the status of the config file so it is (hopefully) ready before we get to the sub-menu
		if(ISourceControlModule::Get().IsEnabled())
		{
			FString ConfigFilePath = FPaths::ConvertRelativePathToFull(FString::Printf(TEXT("%sDefaultEngine.ini"), *FPaths::SourceConfigDir()));

			// note: calling QueueStatusUpdate often does not spam status updates as an internal timer prevents this
			ISourceControlModule::Get().QueueStatusUpdate(ConfigFilePath);
		}
		LevelEditorActionHelpers::CreateGameModeSubMenu(MenuBuilder, InCommandList, InLevelEditor, true);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("WorldSettingsClasses", "World Override"));
	{
		LevelEditorActionHelpers::CreateGameModeSubMenu(MenuBuilder, InCommandList, InLevelEditor, false);
	}
	MenuBuilder.EndSection();

#undef LOCTEXT_NAMESPACE

	return MenuBuilder.MakeWidget();
}

void FLevelEditorToolBar::OnOpenSubLevelBlueprint( ULevel* InLevel )
{
	ULevelScriptBlueprint* LevelScriptBlueprint = InLevel->GetLevelScriptBlueprint();

	if( LevelScriptBlueprint )
	{
		FAssetEditorManager::Get().OpenEditorForAsset(LevelScriptBlueprint);
	}
	else
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "UnableToCreateLevelScript", "Unable to find or create a level blueprint for this level.") );
	}
}

TSharedRef< SWidget > FLevelEditorToolBar::GenerateCinematicsMenuContent( TSharedRef<FUICommandList> InCommandList, TWeakPtr<SLevelEditor> LevelEditorWeakPtr )
{
#define LOCTEXT_NAMESPACE "LevelToolBarCinematicsMenu"

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<FExtender> Extender = FExtender::Combine(LevelEditorModule.GetAllLevelEditorToolbarCinematicsMenuExtenders());

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, InCommandList, Extender );

	using namespace SceneOutliner;

	// We can't build a list of Matinees and LevelSequenceActors while the current World is a PIE world.
	FInitializationOptions InitOptions;
	{
		InitOptions.Mode = ESceneOutlinerMode::ActorPicker;

		// We hide the header row to keep the UI compact.
		// @todo: Might be useful to have this sometimes, actually.  Ideally the user could summon it.
		InitOptions.bShowHeaderRow = false;
		InitOptions.bShowSearchBox = false;
		InitOptions.bShowCreateNewFolder = false;

		InitOptions.ColumnMap.Add(FBuiltInColumnTypes::Label(), FColumnInfo(EColumnVisibility::Visible, 0));
		InitOptions.ColumnMap.Add(FBuiltInColumnTypes::ActorInfo(), FColumnInfo(EColumnVisibility::Visible, 10));

		// Only display Matinee and MovieScene actors
		auto ActorFilter = [](const AActor* Actor){
			return Actor->IsA( AMatineeActor::StaticClass() ) || Actor->IsA( ALevelSequenceActor::StaticClass() );
		};
		InitOptions.Filters->AddFilterPredicate( FActorFilterPredicate::CreateLambda( ActorFilter ) );
	}

	// actor selector to allow the user to choose an actor
	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>( "SceneOutliner" );
	TSharedRef< SWidget > MiniSceneOutliner =
		SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(400.0f)
		[
			SceneOutlinerModule.CreateSceneOutliner(
				InitOptions,
				FOnActorPicked::CreateStatic( &FLevelEditorToolBar::OnCinematicsActorPicked ) )
		];

	static const FName DefaultForegroundName("DefaultForeground");

	// Give the scene outliner a border and background
	const FSlateBrush* BackgroundBrush = FEditorStyle::GetBrush( "Menu.Background" );
	TSharedRef< SBorder > RootBorder =
		SNew( SBorder )
		.Padding(3)
		.BorderImage( BackgroundBrush )
		.ForegroundColor( FEditorStyle::GetSlateColor(DefaultForegroundName) )

		// Assign the box panel as the child
		[
			SNew( SVerticalBox )
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 5 )
			.HAlign( HAlign_Center )
			[
				SNew( STextBlock )
				.Text( LOCTEXT( "SelectCinematicsActorToEdit", "Select an actor" ) )
			]

			+SVerticalBox::Slot()
				.AutoHeight()
				.Padding( 2 )
				[
					MiniSceneOutliner
				]
		]
	;

	MenuBuilder.BeginSection("LevelEditorNewMatinee", LOCTEXT("MatineeMenuCombo_NewHeading", "New"));
	{
		MenuBuilder.AddMenuEntry( FLevelEditorCommands::Get().AddMatinee, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.MatineeActor") );
	}
	MenuBuilder.EndSection();

	UWorld* World = LevelEditorWeakPtr.Pin()->GetWorld();
	const bool bHasAnyCinematicsActors = !!TActorIterator<AMatineeActor>(World) || !!TActorIterator<ALevelSequenceActor>(World);

	//Add a heading to separate the existing cinematics from the 'Add New Cinematic Actor' button
	MenuBuilder.BeginSection("LevelEditorExistingCinematic", LOCTEXT( "CinematicMenuCombo_ExistingHeading", "Edit Existing Cinematic" ) );
	{
		if( bHasAnyCinematicsActors )
		{
			MenuBuilder.AddWidget(MiniSceneOutliner, FText::GetEmpty(), true);
		}
	}
	MenuBuilder.EndSection();
#undef LOCTEXT_NAMESPACE

	return MenuBuilder.MakeWidget();
}

void FLevelEditorToolBar::OnCinematicsActorPicked( AActor* Actor )
{
	//The matinee editor will not tick unless the editor viewport is in realtime mode.
	//the scene outliner eats input, so we must close any popups manually.
	FSlateApplication::Get().DismissAllMenus();

	// Make sure we dismiss the menus before we open this
	if (AMatineeActor* MatineeActor = Cast<AMatineeActor>(Actor))
	{
		// Open Matinee for editing!
		GEditor->OpenMatinee( MatineeActor );
	}
	else if (ALevelSequenceActor* LevelSequenceActor = Cast<ALevelSequenceActor>(Actor))
	{
		UObject* Asset = LevelSequenceActor->LevelSequence.TryLoad();

		if (Asset != nullptr)
		{
			FAssetEditorManager::Get().OpenEditorForAsset(Asset);
		}
	}
}
