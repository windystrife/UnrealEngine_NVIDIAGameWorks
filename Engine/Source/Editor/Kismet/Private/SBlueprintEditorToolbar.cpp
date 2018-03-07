// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SBlueprintEditorToolbar.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/CoreMisc.h"
#include "Widgets/Layout/SBorder.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "BlueprintEditor.h"
#include "Widgets/Layout/SSpacer.h"
#include "ISourceControlModule.h"
#include "BlueprintEditorCommands.h"
#include "Kismet2/DebuggerCommands.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "GraphEditorActions.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "AssetToolsModule.h"
#include "BlueprintEditorModes.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "SBlueprintEditorSelectedDebugObjectWidget.h"
#include "DesktopPlatformModule.h"
#include "SBlueprintRevisionMenu.h"

#define LOCTEXT_NAMESPACE "KismetToolbar"

//////////////////////////////////////////////////////////////////////////
// SBlueprintModeSeparator

class SBlueprintModeSeparator : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SBlueprintModeSeparator) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArg)
	{
		SBorder::Construct(
			SBorder::FArguments()
			.BorderImage(FEditorStyle::GetBrush("BlueprintEditor.PipelineSeparator"))
			.Padding(0.0f)
			);
	}

	// SWidget interface
	virtual FVector2D ComputeDesiredSize(float) const override
	{
		const float Height = 20.0f;
		const float Thickness = 16.0f;
		return FVector2D(Thickness, Height);
	}
	// End of SWidget interface
};

//////////////////////////////////////////////////////////////////////////
// FKismet2Menu

void FKismet2Menu::FillFileMenuBlueprintSection( FMenuBuilder& MenuBuilder, FBlueprintEditor& Kismet )
{
	MenuBuilder.BeginSection("FileBlueprint", LOCTEXT("BlueprintHeading", "Blueprint"));
	{
		MenuBuilder.AddMenuEntry( FBlueprintEditorCommands::Get().CompileBlueprint );
		MenuBuilder.AddMenuEntry( FBlueprintEditorCommands::Get().RefreshAllNodes );
		MenuBuilder.AddMenuEntry( FBlueprintEditorCommands::Get().ReparentBlueprint );
		MenuBuilder.AddWrapperSubMenu(
			LOCTEXT("Diff", "Diff"),
			LOCTEXT("BlueprintEditorDiffToolTip", "Diff against previous revisions"),
			FOnGetContent::CreateStatic< FBlueprintEditor& >( &FKismet2Menu::MakeDiffMenu, Kismet),
			FSlateIcon());
		MenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().BeginBlueprintMerge);
	}
	MenuBuilder.EndSection();

	// Only show the developer menu on machines with the solution (assuming they can build it)
	FString SolutionPath;
	if(FDesktopPlatformModule::Get()->GetSolutionPath(SolutionPath))
	{
		MenuBuilder.BeginSection("FileDeveloper");
		{
			MenuBuilder.AddSubMenu( 
				LOCTEXT("DeveloperMenu", "Developer"),
				LOCTEXT("DeveloperMenu_ToolTip", "Open the developer menu"),
				FNewMenuDelegate::CreateStatic( &FKismet2Menu::FillDeveloperMenu ),
				true);
		}
		MenuBuilder.EndSection();
	}
}

void FKismet2Menu::FillDeveloperMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.BeginSection("FileDeveloperCompilerSettings", LOCTEXT("CompileOptionsHeading", "Compiler Settings"));
	{
		MenuBuilder.AddMenuEntry( FBlueprintEditorCommands::Get().SaveIntermediateBuildProducts );
	}
	MenuBuilder.EndSection();

	static const FBoolConfigValueHelper NativeCodeGenerationTool(TEXT("Kismet"), TEXT("bNativeCodeGenerationTool"), GEngineIni);
	if (NativeCodeGenerationTool)
	{
		MenuBuilder.BeginSection("GenerateNativeCode", LOCTEXT("Cpp", "C++"));
		{
			MenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().GenerateNativeCode);
		}
		MenuBuilder.EndSection();
	}

	if (false)
	{
		MenuBuilder.BeginSection("FileDeveloperFindReferences");
		{
			MenuBuilder.AddMenuEntry( FBlueprintEditorCommands::Get().FindReferencesFromClass );
			MenuBuilder.AddMenuEntry( FBlueprintEditorCommands::Get().FindReferencesFromBlueprint );
			MenuBuilder.AddMenuEntry( FBlueprintEditorCommands::Get().FindReferencesFromBlueprint );
		}
		MenuBuilder.EndSection();
	}

	MenuBuilder.BeginSection("SchemaDeveloperSettings", LOCTEXT("SchemaDevUtilsHeading", "Schema Utilities"));
	{
		MenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().ShowActionMenuItemSignatures);
	}
	MenuBuilder.EndSection();
}

void FKismet2Menu::FillEditMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.BeginSection("EditSearch", LOCTEXT("EditMenu_SearchHeading", "Search") );
	{
		MenuBuilder.AddMenuEntry( FBlueprintEditorCommands::Get().FindInBlueprint );
		MenuBuilder.AddMenuEntry( FBlueprintEditorCommands::Get().FindInBlueprints );
		MenuBuilder.AddMenuEntry( FBlueprintEditorCommands::Get().DeleteUnusedVariables );
	}
	MenuBuilder.EndSection();
}

void FKismet2Menu::FillViewMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.BeginSection("ViewPinVisibility", LOCTEXT("ViewMenu_PinVisibilityHeading", "Pin Visibility") );
	{
		MenuBuilder.AddMenuEntry(FGraphEditorCommands::Get().ShowAllPins);
		MenuBuilder.AddMenuEntry(FGraphEditorCommands::Get().HideNoConnectionNoDefaultPins);
		MenuBuilder.AddMenuEntry(FGraphEditorCommands::Get().HideNoConnectionPins);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("ViewZoom", LOCTEXT("ViewMenu_ZoomHeading", "Zoom") );
	{
		MenuBuilder.AddMenuEntry( FBlueprintEditorCommands::Get().ZoomToWindow );
		MenuBuilder.AddMenuEntry( FBlueprintEditorCommands::Get().ZoomToSelection );
	}
	MenuBuilder.EndSection();
}

void FKismet2Menu::FillDebugMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.BeginSection("DebugBreakpoints", LOCTEXT("DebugMenu_BreakpointHeading", "Breakpoints") );
	{
		MenuBuilder.AddMenuEntry( FBlueprintEditorCommands::Get().DisableAllBreakpoints );
		MenuBuilder.AddMenuEntry( FBlueprintEditorCommands::Get().EnableAllBreakpoints );
		MenuBuilder.AddMenuEntry( FBlueprintEditorCommands::Get().ClearAllBreakpoints );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("DebugWatches", LOCTEXT("DebugMenu_WatchHeading", "Watches") );
	{
		MenuBuilder.AddMenuEntry( FBlueprintEditorCommands::Get().ClearAllWatches );
	}
	MenuBuilder.EndSection();
}

void FKismet2Menu::SetupBlueprintEditorMenu( TSharedPtr< FExtender > Extender, FBlueprintEditor& BlueprintEditor)
{
	// Extend the File menu with asset actions
	Extender->AddMenuExtension(
		"FileLoadAndSave",
		EExtensionHook::After,
		BlueprintEditor.GetToolkitCommands(),
		FMenuExtensionDelegate::CreateStatic< FBlueprintEditor& >( &FKismet2Menu::FillFileMenuBlueprintSection, BlueprintEditor ) );

	// Extend the Edit menu
	Extender->AddMenuExtension(
		"EditHistory",
		EExtensionHook::After,
		BlueprintEditor.GetToolkitCommands(),
		FMenuExtensionDelegate::CreateStatic( &FKismet2Menu::FillEditMenu ) );

	// Add additional blueprint editor menus
	{
		struct Local
		{
			static void AddBlueprintEditorMenus( FMenuBarBuilder& MenuBarBuilder )
			{
				// View
				MenuBarBuilder.AddPullDownMenu( 
					LOCTEXT("ViewMenu", "View"),
					LOCTEXT("ViewMenu_ToolTip", "Open the View menu"),
					FNewMenuDelegate::CreateStatic( &FKismet2Menu::FillViewMenu ),
					"View");

				// Debug
				MenuBarBuilder.AddPullDownMenu( 
					LOCTEXT("DebugMenu", "Debug"),
					LOCTEXT("DebugMenu_ToolTip", "Open the debug menu"),
					FNewMenuDelegate::CreateStatic( &FKismet2Menu::FillDebugMenu ),
					"Debug");
			}
		};

		Extender->AddMenuBarExtension(
			"Edit",
			EExtensionHook::After,
			BlueprintEditor.GetToolkitCommands(), 
			FMenuBarExtensionDelegate::CreateStatic( &Local::AddBlueprintEditorMenus ) );
	}
}

/** Delegate called to diff a specific revision with the current */
static void OnDiffRevisionPicked(FRevisionInfo const& RevisionInfo, TWeakObjectPtr<UBlueprint> BlueprintObj)
{
	if (BlueprintObj.IsValid())
	{
		bool const bIsLevelScriptBlueprint = FBlueprintEditorUtils::IsLevelScriptBlueprint(BlueprintObj.Get());
		FString const Filename = SourceControlHelpers::PackageFilename(bIsLevelScriptBlueprint ? BlueprintObj.Get()->GetOuter()->GetPathName() : BlueprintObj.Get()->GetPathName());

		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

		// Get the SCC state
		FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(Filename, EStateCacheUsage::Use);
		if (SourceControlState.IsValid())
		{
			for (int32 HistoryIndex = 0; HistoryIndex < SourceControlState->GetHistorySize(); HistoryIndex++)
			{
				TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Revision = SourceControlState->GetHistoryItem(HistoryIndex);
				check(Revision.IsValid());
				if (Revision->GetRevision() == RevisionInfo.Revision)
				{
					// Get the revision of this package from source control
					FString PreviousTempPkgName;
					if (Revision->Get(PreviousTempPkgName))
					{
						// Try and load that package
						UPackage* PreviousTempPkg = LoadPackage(NULL, *PreviousTempPkgName, LOAD_DisableCompileOnLoad);

						if (PreviousTempPkg != NULL)
						{
							UObject* PreviousAsset = NULL;

							// If its a levelscript blueprint, find the previous levelscript blueprint in the map
							if (bIsLevelScriptBlueprint)
							{
								TArray<UObject *> ObjectsInOuter;
								GetObjectsWithOuter(PreviousTempPkg, ObjectsInOuter);

								// Look for the level script blueprint for this package
								for (int32 Index = 0; Index < ObjectsInOuter.Num(); Index++)
								{
									UObject* Obj = ObjectsInOuter[Index];
									if (ULevelScriptBlueprint* ObjAsBlueprint = Cast<ULevelScriptBlueprint>(Obj))
									{
										PreviousAsset = ObjAsBlueprint;
										break;
									}
								}
							}
							// otherwise its a normal Blueprint
							else
							{
								FString PreviousAssetName = FPaths::GetBaseFilename(Filename, true);
								PreviousAsset = FindObject<UObject>(PreviousTempPkg, *PreviousAssetName);
							}

							if (PreviousAsset != NULL)
							{
								FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
								FRevisionInfo OldRevision = { Revision->GetRevision(), Revision->GetCheckInIdentifier(), Revision->GetDate() };
								FRevisionInfo CurrentRevision = { TEXT(""), Revision->GetCheckInIdentifier(), Revision->GetDate() };
								AssetToolsModule.Get().DiffAssets(PreviousAsset, BlueprintObj.Get(), OldRevision, CurrentRevision);
							}
						}
						else
						{
							FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("SourceControl.HistoryWindow", "UnableToLoadAssets", "Unable to load assets to diff. Content may no longer be supported?"));
						}
					}
					break;
				}
			}
		}
	}
}

TSharedRef<SWidget> FKismet2Menu::MakeDiffMenu(FBlueprintEditor& Kismet)
{
	if (ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable())
	{
		UBlueprint* BlueprintObj = Kismet.GetBlueprintObj();
		if(BlueprintObj)
		{
			TWeakObjectPtr<UBlueprint> BlueprintPtr = BlueprintObj;
			// Add our async SCC task widget
			return SNew(SBlueprintRevisionMenu, BlueprintObj)
				.OnRevisionSelected_Static(&OnDiffRevisionPicked, BlueprintPtr);
		}
		else
		{
			// if BlueprintObj is null then this means that multiple blueprints are selected
			FMenuBuilder MenuBuilder(true, NULL);
			MenuBuilder.AddMenuEntry( LOCTEXT("NoRevisionsForMultipleBlueprints", "Multiple blueprints selected"), 
				FText(), FSlateIcon(), FUIAction() );
			return MenuBuilder.MakeWidget();
		}
	}

	FMenuBuilder MenuBuilder(true, NULL);
	MenuBuilder.AddMenuEntry( LOCTEXT("SourceControlDisabled", "Source control is disabled"), 
		FText(), FSlateIcon(), FUIAction() );
	return MenuBuilder.MakeWidget();
}



//////////////////////////////////////////////////////////////////////////
// FFullBlueprintEditorCommands

void FFullBlueprintEditorCommands::RegisterCommands() 
{
	UI_COMMAND(Compile, "Compile", "Compile the blueprint", EUserInterfaceActionType::Button, FInputChord());
	
	UI_COMMAND(SaveOnCompile_Never, "Never", "Sets the save-on-compile option to 'Never', meaning that your Blueprints will not be saved when they are compiled", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SaveOnCompile_SuccessOnly, "On Success Only", "Sets the save-on-compile option to 'Success Only', meaning that your Blueprints will be saved whenever they are successfully compiled", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SaveOnCompile_Always, "Always", "Sets the save-on-compile option to 'Always', meaning that your Blueprints will be saved whenever they are compiled (even if there were errors)", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(SwitchToScriptingMode, "Graph", "Switches to Graph Editing Mode", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(SwitchToBlueprintDefaultsMode, "Defaults", "Switches to Class Defaults Mode", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(SwitchToComponentsMode, "Components", "Switches to Components Mode", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(EditGlobalOptions, "Class Settings", "Edit Class Settings (Previously known as Blueprint Props)", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(EditClassDefaults, "Class Defaults", "Edit the initial values of your class.", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(JumpToErrorNode, "Jump to Error Node", "When enabled, then the Blueprint will snap focus to nodes producing an error during compilation", EUserInterfaceActionType::ToggleButton, FInputChord());
}

//////////////////////////////////////////////////////////////////////////
// Static FBlueprintEditorToolbar Helpers

namespace BlueprintEditorToolbarImpl
{
	static TSharedRef<SWidget> GenerateCompileOptionsWidget(TSharedRef<FUICommandList> CommandList);
	static void MakeSaveOnCompileSubMenu(FMenuBuilder& InMenuBuilder);
	static void MakeCompileDeveloperSubMenu(FMenuBuilder& InMenuBuilder);
};

static TSharedRef<SWidget> BlueprintEditorToolbarImpl::GenerateCompileOptionsWidget(TSharedRef<FUICommandList> CommandList)
{
	FMenuBuilder MenuBuilder(/*bShouldCloseWindowAfterMenuSelection =*/true, CommandList);

	const FFullBlueprintEditorCommands& Commands = FFullBlueprintEditorCommands::Get();

	// @TODO: disable the menu and change up the tooltip when all sub items are disabled
	MenuBuilder.AddSubMenu(
		LOCTEXT("SaveOnCompileSubMenu", "Save on Compile"),
		LOCTEXT("SaveOnCompileSubMenu_ToolTip", "Determines how the Blueprint is saved whenever you compile it."),
		FNewMenuDelegate::CreateStatic(&BlueprintEditorToolbarImpl::MakeSaveOnCompileSubMenu));

	MenuBuilder.AddMenuEntry(Commands.JumpToErrorNode);

// 	MenuBuilder.AddSubMenu(
// 		LOCTEXT("DevCompileSubMenu", "Developer"),
// 		LOCTEXT("DevCompileSubMenu_ToolTip", "Advanced settings that aid in devlopment/debugging of the Blueprint system as a whole."),
// 		FNewMenuDelegate::CreateStatic(&BlueprintEditorToolbarImpl::MakeCompileDeveloperSubMenu));

	return MenuBuilder.MakeWidget();
}

static void BlueprintEditorToolbarImpl::MakeSaveOnCompileSubMenu(FMenuBuilder& InMenuBuilder)
{
	const FFullBlueprintEditorCommands& Commands = FFullBlueprintEditorCommands::Get();
	InMenuBuilder.AddMenuEntry(Commands.SaveOnCompile_Never);
	InMenuBuilder.AddMenuEntry(Commands.SaveOnCompile_SuccessOnly);
	InMenuBuilder.AddMenuEntry(Commands.SaveOnCompile_Always);
}

static void BlueprintEditorToolbarImpl::MakeCompileDeveloperSubMenu(FMenuBuilder& InMenuBuilder)
{
	const FBlueprintEditorCommands& EditorCommands = FBlueprintEditorCommands::Get();
	InMenuBuilder.AddMenuEntry(EditorCommands.SaveIntermediateBuildProducts);
	InMenuBuilder.AddMenuEntry(EditorCommands.ShowActionMenuItemSignatures);
}


//////////////////////////////////////////////////////////////////////////
// FBlueprintEditorToolbar

void FBlueprintEditorToolbar::AddBlueprintEditorModesToolbar(TSharedPtr<FExtender> Extender)
{
	
}

void FBlueprintEditorToolbar::AddBlueprintGlobalOptionsToolbar(TSharedPtr<FExtender> Extender)
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = BlueprintEditor.Pin();

	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		BlueprintEditorPtr->GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP( this, &FBlueprintEditorToolbar::FillBlueprintGlobalOptionsToolbar ) );
}

void FBlueprintEditorToolbar::AddCompileToolbar(TSharedPtr<FExtender> Extender)
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = BlueprintEditor.Pin();

	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::Before,
		BlueprintEditorPtr->GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP( this, &FBlueprintEditorToolbar::FillCompileToolbar ) );
}

void FBlueprintEditorToolbar::AddNewToolbar(TSharedPtr<FExtender> Extender)
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = BlueprintEditor.Pin();

	Extender->AddToolBarExtension(
		"MyBlueprint",
		EExtensionHook::After,
		BlueprintEditorPtr->GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP( this, &FBlueprintEditorToolbar::FillNewToolbar ) );
}

void FBlueprintEditorToolbar::AddScriptingToolbar(TSharedPtr<FExtender> Extender)
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = BlueprintEditor.Pin();

	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		BlueprintEditorPtr->GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP( this, &FBlueprintEditorToolbar::FillScriptingToolbar ) );
}

void FBlueprintEditorToolbar::AddDebuggingToolbar(TSharedPtr<FExtender> Extender)
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = BlueprintEditor.Pin();

	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		BlueprintEditorPtr->GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP( this, &FBlueprintEditorToolbar::FillDebuggingToolbar ) );
}

void FBlueprintEditorToolbar::AddComponentsToolbar(TSharedPtr<FExtender> Extender)
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = BlueprintEditor.Pin();

	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		BlueprintEditorPtr->GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP( this, &FBlueprintEditorToolbar::FillComponentsToolbar ) );
}

void FBlueprintEditorToolbar::FillBlueprintEditorModesToolbar(FToolBarBuilder& ToolbarBuilder)
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = BlueprintEditor.Pin();
	UBlueprint* BlueprintObj = BlueprintEditorPtr->GetBlueprintObj();

	TAttribute<FName> GetActiveMode(BlueprintEditorPtr.ToSharedRef(), &FBlueprintEditor::GetCurrentMode);
	FOnModeChangeRequested SetActiveMode = FOnModeChangeRequested::CreateSP(BlueprintEditorPtr.ToSharedRef(), &FBlueprintEditor::SetCurrentMode);

	TArray< TSharedPtr< SWidget > > ToolbarWidgets = GenerateToolbarWidgets( BlueprintObj, GetActiveMode, SetActiveMode );
	
	for( const auto& Widget : ToolbarWidgets )
	{
		BlueprintEditorPtr->AddToolbarWidget( Widget.ToSharedRef() );
	}
}

void FBlueprintEditorToolbar::FillBlueprintGlobalOptionsToolbar(FToolBarBuilder& ToolbarBuilder)
{
	const FFullBlueprintEditorCommands& Commands = FFullBlueprintEditorCommands::Get();
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = BlueprintEditor.Pin();
	UBlueprint* BlueprintObj = BlueprintEditorPtr->GetBlueprintObj();
	
	ToolbarBuilder.BeginSection("Settings");
	
	if(BlueprintObj != NULL)
	{
		ToolbarBuilder.AddToolBarButton(Commands.EditGlobalOptions);
		ToolbarBuilder.AddToolBarButton(Commands.EditClassDefaults);
	}
	
	ToolbarBuilder.EndSection();
}

void FBlueprintEditorToolbar::FillCompileToolbar(FToolBarBuilder& ToolbarBuilder)
{
	const FFullBlueprintEditorCommands& Commands = FFullBlueprintEditorCommands::Get();
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = BlueprintEditor.Pin();
	UBlueprint* BlueprintObj = BlueprintEditorPtr->GetBlueprintObj();

	ToolbarBuilder.BeginSection("Compile");
	if (BlueprintObj != NULL)
	{
		ToolbarBuilder.AddToolBarButton( Commands.Compile,
									 NAME_None, 
									 TAttribute<FText>(),
									 TAttribute<FText>(this, &FBlueprintEditorToolbar::GetStatusTooltip),
									 TAttribute<FSlateIcon>(this, &FBlueprintEditorToolbar::GetStatusImage),
									 FName(TEXT("CompileBlueprint")));

		FUIAction TempCompileOptionsCommand;
		ToolbarBuilder.AddComboButton(
			TempCompileOptionsCommand,
			FOnGetContent::CreateStatic(&BlueprintEditorToolbarImpl::GenerateCompileOptionsWidget, BlueprintEditorPtr->GetToolkitCommands()),
			LOCTEXT("BlupeintCompileOptions_ToolbarName",    "Compile Options"),
			LOCTEXT("BlupeintCompileOptions_ToolbarTooltip", "Options to customize how Blueprints compile"),
			TAttribute<FSlateIcon>(),
			/*bSimpleComboBox =*/true
		);
	}
	ToolbarBuilder.EndSection();
}

void FBlueprintEditorToolbar::FillNewToolbar(FToolBarBuilder& ToolbarBuilder)
{
	const FBlueprintEditorCommands& Commands = FBlueprintEditorCommands::Get();

	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = BlueprintEditor.Pin();
	UBlueprint* BlueprintObj = BlueprintEditorPtr->GetBlueprintObj();

	ToolbarBuilder.BeginSection("AddNew");
	if (BlueprintObj != NULL)
	{
		ToolbarBuilder.AddToolBarButton(Commands.AddNewVariable, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), FName(TEXT("BPEAddNewVariable")));
		ToolbarBuilder.AddToolBarButton(Commands.AddNewFunction, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), FName(TEXT("BPEAddNewFunction")));
		ToolbarBuilder.AddToolBarButton(Commands.AddNewMacroDeclaration, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), FName(TEXT("BPEAddNewMacro")));
		// Add New Animation Graph isn't supported right now.
		ToolbarBuilder.AddToolBarButton(Commands.AddNewEventGraph, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), FName(TEXT("BPEAddNewEventGraph")));
		ToolbarBuilder.AddToolBarButton(Commands.AddNewDelegate, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), FName(TEXT("BPEAddNewDelegate")));
	}
	ToolbarBuilder.EndSection();
}

void FBlueprintEditorToolbar::FillScriptingToolbar(FToolBarBuilder& ToolbarBuilder)
{
	const FBlueprintEditorCommands& Commands = FBlueprintEditorCommands::Get();

	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = BlueprintEditor.Pin();
	UBlueprint* BlueprintObj = BlueprintEditorPtr->GetBlueprintObj();

	ToolbarBuilder.BeginSection("Script");

	ToolbarBuilder.AddToolBarButton(FBlueprintEditorCommands::Get().FindInBlueprint);

	ToolbarBuilder.EndSection();
}

void FBlueprintEditorToolbar::FillDebuggingToolbar(FToolBarBuilder& ToolbarBuilder)
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = BlueprintEditor.Pin();
	UBlueprint* BlueprintObj = BlueprintEditorPtr->GetBlueprintObj();

	ToolbarBuilder.BeginSection("Debugging");
	if (BlueprintObj)
	{
		FPlayWorldCommands::BuildToolbar(ToolbarBuilder);

		if (BlueprintObj->BlueprintType != BPTYPE_MacroLibrary)
		{
			// Selected debug actor button
			ToolbarBuilder.AddWidget(SNew(SBlueprintEditorSelectedDebugObjectWidget, BlueprintEditorPtr));
		}
	}
	ToolbarBuilder.EndSection();
}

void FBlueprintEditorToolbar::FillComponentsToolbar(FToolBarBuilder& ToolbarBuilder)
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = BlueprintEditor.Pin();
	UBlueprint* BlueprintObj = BlueprintEditorPtr->GetBlueprintObj();

#if 0 // restore this if we ever need the ability to toggle component editing on/off
	ToolbarBuilder.BeginSection("Components");
		ToolbarBuilder.AddToolBarButton(FSCSCommands::Get().ToggleComponentEditing);
	ToolbarBuilder.EndSection();
#endif

	ToolbarBuilder.BeginSection("ComponentsViewport");
		ToolbarBuilder.AddToolBarButton(FBlueprintEditorCommands::Get().EnableSimulation);
	ToolbarBuilder.EndSection();
}

FSlateIcon FBlueprintEditorToolbar::GetStatusImage() const
{
	UBlueprint* BlueprintObj = BlueprintEditor.Pin()->GetBlueprintObj();
	EBlueprintStatus Status = BlueprintObj->Status;

	// For macro types, always show as up-to-date, since we don't compile them
	if (BlueprintObj->BlueprintType == BPTYPE_MacroLibrary)
	{
		Status = BS_UpToDate;
	}

	switch (Status)
	{
	default:
	case BS_Unknown:
	case BS_Dirty:
		return FSlateIcon(FEditorStyle::GetStyleSetName(), "Kismet.Status.Unknown");
	case BS_Error:
		return FSlateIcon(FEditorStyle::GetStyleSetName(), "Kismet.Status.Error");
	case BS_UpToDate:
		return FSlateIcon(FEditorStyle::GetStyleSetName(), "Kismet.Status.Good");
	case BS_UpToDateWithWarnings:
		return FSlateIcon(FEditorStyle::GetStyleSetName(), "Kismet.Status.Warning");
	}
}

FText FBlueprintEditorToolbar::GetStatusTooltip() const
{
	UBlueprint* BlueprintObj = BlueprintEditor.Pin()->GetBlueprintObj();
	EBlueprintStatus Status = BlueprintObj->Status;

	// For macro types, always show as up-to-date, since we don't compile them
	if (BlueprintObj->BlueprintType == BPTYPE_MacroLibrary)
	{
		Status = BS_UpToDate;
	}

	switch (Status)
	{
	default:
	case BS_Unknown:
		return LOCTEXT("Recompile_Status", "Unknown status; should recompile");
	case BS_Dirty:
		return LOCTEXT("Dirty_Status", "Dirty; needs to be recompiled");
	case BS_Error:
		return LOCTEXT("CompileError_Status", "There was an error during compilation, see the log for details");
	case BS_UpToDate:
		return LOCTEXT("GoodToGo_Status", "Good to go");
	case BS_UpToDateWithWarnings:
		return LOCTEXT("GoodToGoWarning_Status", "There was a warning during compilation, see the log for details");
	}
}

TArray< TSharedPtr< SWidget> > FBlueprintEditorToolbar::GenerateToolbarWidgets(const UBlueprint* BlueprintObj, TAttribute<FName> ActiveModeGetter, FOnModeChangeRequested ActiveModeSetter)
{
	TArray< TSharedPtr< SWidget> > Ret;
	if (!BlueprintObj ||
		(!FBlueprintEditorUtils::IsLevelScriptBlueprint(BlueprintObj)
		&& !FBlueprintEditorUtils::IsInterfaceBlueprint(BlueprintObj)
		&& !BlueprintObj->bIsNewlyCreated)
		)
	{
		// Left side padding
		Ret.Add(SNew(SSpacer).Size(FVector2D(4.0f, 1.0f)));

		Ret.Add(
			SNew(SModeWidget, FBlueprintEditorApplicationModes::GetLocalizedMode(FBlueprintEditorApplicationModes::BlueprintDefaultsMode), FBlueprintEditorApplicationModes::BlueprintDefaultsMode)
			.OnGetActiveMode(ActiveModeGetter)
			.OnSetActiveMode(ActiveModeSetter)
			.CanBeSelected(BlueprintObj ? FBlueprintEditorUtils::DoesSupportDefaults(BlueprintObj) : false)
			.ToolTip(IDocumentation::Get()->CreateToolTip(
				LOCTEXT("BlueprintDefaultsModeButtonTooltip", "Switch to Class Defaults Mode"),
				NULL,
				TEXT("Shared/Editors/BlueprintEditor"),
				TEXT("DefaultsMode")))
			.IconImage(FEditorStyle::GetBrush("FullBlueprintEditor.SwitchToBlueprintDefaultsMode"))
			.SmallIconImage(FEditorStyle::GetBrush("FullBlueprintEditor.SwitchToBlueprintDefaultsMode.Small"))
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("DefaultsMode")))
		);

		Ret.Add(SNew(SBlueprintModeSeparator));

		Ret.Add(
			SNew(SModeWidget, FBlueprintEditorApplicationModes::GetLocalizedMode(FBlueprintEditorApplicationModes::BlueprintComponentsMode), FBlueprintEditorApplicationModes::BlueprintComponentsMode)
			.OnGetActiveMode(ActiveModeGetter)
			.OnSetActiveMode(ActiveModeSetter)
			.CanBeSelected(BlueprintObj ? FBlueprintEditorUtils::DoesSupportComponents(BlueprintObj) : false)
			.ToolTip(IDocumentation::Get()->CreateToolTip(
				LOCTEXT("ComponentsModeButtonTooltip", "Switch to Components Mode"),
				NULL,
				TEXT("Shared/Editors/BlueprintEditor"),
				TEXT("ComponentsMode")))
			.IconImage(FEditorStyle::GetBrush("FullBlueprintEditor.SwitchToComponentsMode"))
			.SmallIconImage(FEditorStyle::GetBrush("FullBlueprintEditor.SwitchToComponentsMode.Small"))
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ComponentsMode")))
		);

		Ret.Add(SNew(SBlueprintModeSeparator));

		Ret.Add(
			SNew(SModeWidget, FBlueprintEditorApplicationModes::GetLocalizedMode(FBlueprintEditorApplicationModes::StandardBlueprintEditorMode), FBlueprintEditorApplicationModes::StandardBlueprintEditorMode)
			.OnGetActiveMode(ActiveModeGetter)
			.OnSetActiveMode(ActiveModeSetter)
			.CanBeSelected(BlueprintObj != NULL)
			.ToolTip(IDocumentation::Get()->CreateToolTip(
				LOCTEXT("GraphModeButtonTooltip", "Switch to Graph Editing Mode"),
				NULL,
				TEXT("Shared/Editors/BlueprintEditor"),
				TEXT("GraphMode")))
			.ToolTipText(LOCTEXT("GraphModeButtonTooltip", "Switch to Graph Editing Mode"))
			.IconImage(FEditorStyle::GetBrush("FullBlueprintEditor.SwitchToScriptingMode"))
			.SmallIconImage(FEditorStyle::GetBrush("FullBlueprintEditor.SwitchToScriptingMode.Small"))
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("GraphMode")))
		);

		// Right side padding
		Ret.Add(SNew(SSpacer).Size(FVector2D(4.0f, 1.0f)));
	}
	return Ret;
}

#undef LOCTEXT_NAMESPACE
