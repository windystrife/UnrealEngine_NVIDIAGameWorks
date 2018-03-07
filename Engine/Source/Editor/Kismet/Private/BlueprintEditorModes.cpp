// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorModes.h"
#include "Settings/EditorExperimentalSettings.h"


// Core kismet tabs
#include "SSCSEditor.h"
#include "SSCSEditorViewport.h"
#include "SKismetInspector.h"
#include "SMyBlueprint.h"
// End of core kismet tabs

// Debugging
// End of debugging

#include "LayoutExtender.h"
#include "SBlueprintEditorToolbar.h"
#include "BlueprintEditorTabs.h"
#include "BlueprintEditorTabFactories.h"
#include "BlueprintEditorSharedTabFactories.h"

#define LOCTEXT_NAMESPACE "BlueprintEditor"


const FName FBlueprintEditorApplicationModes::StandardBlueprintEditorMode( TEXT("GraphName") );
const FName FBlueprintEditorApplicationModes::BlueprintDefaultsMode( TEXT("DefaultsName") );
const FName FBlueprintEditorApplicationModes::BlueprintComponentsMode( TEXT("ComponentsName") );
const FName FBlueprintEditorApplicationModes::BlueprintInterfaceMode( TEXT("InterfaceName") );
const FName FBlueprintEditorApplicationModes::BlueprintMacroMode( TEXT("MacroName") );

TSharedPtr<FTabManager::FLayout> GetDefaltEditorLayout(TSharedPtr<class FBlueprintEditor> InBlueprintEditor)
{
	return FTabManager::NewLayout( "Standalone_BlueprintEditor_Layout_v6" )
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient( 0.186721f )
			->SetHideTabWell(true)
			->AddTab( InBlueprintEditor->GetToolbarTabId(), ETabState::OpenedTab )
		)
		->Split
		(
			FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewSplitter() ->SetOrientation( Orient_Vertical )
				->SetSizeCoefficient(0.70f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient( 0.80f )
					->AddTab( "Document", ETabState::ClosedTab )
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient( 0.20f )
					->AddTab( FBlueprintEditorTabs::CompilerResultsID, ETabState::ClosedTab )
					->AddTab( FBlueprintEditorTabs::FindResultsID, ETabState::ClosedTab )
				)
			)
			->Split
			(
				FTabManager::NewSplitter() ->SetOrientation( Orient_Vertical )
				->SetSizeCoefficient(0.15f)
				->Split
				(
					FTabManager::NewStack() ->SetSizeCoefficient(0.5f)
					->AddTab( FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab )
				)
				->Split
				(
					FTabManager::NewStack() ->SetSizeCoefficient(0.5f)
					->AddTab( FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab )
					->AddTab( FBlueprintEditorTabs::PaletteID, ETabState::ClosedTab )
					->AddTab( FBlueprintEditorTabs::DefaultEditorID, ETabState::ClosedTab )
				)
			)
		)
	);
}

FBlueprintEditorApplicationMode::FBlueprintEditorApplicationMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor, FName InModeName, FText(*GetLocalizedMode)(const FName), const bool bRegisterViewport, const bool bRegisterDefaultsTab)
	: FApplicationMode(InModeName, GetLocalizedMode)
{
	MyBlueprintEditor = InBlueprintEditor;

	// Create the tab factories
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FDebugInfoSummoner(InBlueprintEditor)));
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FPaletteSummoner(InBlueprintEditor)));
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FMyBlueprintSummoner(InBlueprintEditor)));
	if (GetDefault<UEditorExperimentalSettings>()->bEnableFindAndReplaceReferences)
	{
		BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FReplaceNodeReferencesSummoner(InBlueprintEditor)));
	}
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FCompilerResultsSummoner(InBlueprintEditor)));
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FFindResultsSummoner(InBlueprintEditor)));
	
	if( bRegisterViewport )
	{
		BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FSCSViewportSummoner(InBlueprintEditor)));
	}
	if( bRegisterDefaultsTab )
	{
		BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FDefaultsEditorSummoner(InBlueprintEditor)));
	}
	CoreTabFactories.RegisterFactory(MakeShareable(new FSelectionDetailsSummoner(InBlueprintEditor)));

	TabLayout = GetDefaltEditorLayout(InBlueprintEditor);
	
	// setup toolbar
	//@TODO: Keep this in sync with AnimBlueprintMode.cpp
	InBlueprintEditor->GetToolbarBuilder()->AddBlueprintEditorModesToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddCompileToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddScriptingToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddDebuggingToolbar(ToolbarExtender);

	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
	BlueprintEditorModule.OnRegisterTabsForEditor().Broadcast(BlueprintEditorTabFactories, InModeName, InBlueprintEditor);

	LayoutExtender = MakeShared<FLayoutExtender>();
	BlueprintEditorModule.OnRegisterLayoutExtensions().Broadcast(*LayoutExtender);
}

void FBlueprintEditorApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(CoreTabFactories);
	BP->PushTabFactories(BlueprintEditorOnlyTabFactories);
	BP->PushTabFactories(BlueprintEditorTabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FBlueprintEditorApplicationMode::PreDeactivateMode()
{
	FApplicationMode::PreDeactivateMode();

	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->SaveEditedObjectState();
	BP->GetMyBlueprintWidget()->ClearGraphActionMenuSelection();
}

void FBlueprintEditorApplicationMode::PostActivateMode()
{
	// Reopen any documents that were open when the blueprint was last saved
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	BP->RestoreEditedObjectState();
	BP->SetupViewForBlueprintEditingMode();

	FApplicationMode::PostActivateMode();
}


FBlueprintDefaultsApplicationMode::FBlueprintDefaultsApplicationMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor)
	: FApplicationMode(FBlueprintEditorApplicationModes::BlueprintDefaultsMode, FBlueprintEditorApplicationModes::GetLocalizedMode)
{
	MyBlueprintEditor = InBlueprintEditor;
	
	BlueprintDefaultsTabFactories.RegisterFactory(MakeShareable(new FDefaultsEditorSummoner(InBlueprintEditor)));
	BlueprintDefaultsTabFactories.RegisterFactory(MakeShareable(new FFindResultsSummoner(InBlueprintEditor)));

	TabLayout = FTabManager::NewLayout( "Standalone_BlueprintDefaults_Layout_v6" )
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient( 0.2f )
				->SetHideTabWell(true)
				->AddTab( InBlueprintEditor->GetToolbarTabId(), ETabState::OpenedTab )
			)
			->Split
			(
				FTabManager::NewStack()
				->AddTab( FBlueprintEditorTabs::DefaultEditorID, ETabState::OpenedTab )
			)
		);

	// setup toolbar
	InBlueprintEditor->GetToolbarBuilder()->AddCompileToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddBlueprintEditorModesToolbar(ToolbarExtender);
}

void FBlueprintDefaultsApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(BlueprintDefaultsTabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FBlueprintDefaultsApplicationMode::PostActivateMode()
{
	// Reopen any documents that were open when the blueprint was last saved
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	BP->StartEditingDefaults();

	FApplicationMode::PostActivateMode();
}


FBlueprintComponentsApplicationMode::FBlueprintComponentsApplicationMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor)
	: FApplicationMode(FBlueprintEditorApplicationModes::BlueprintComponentsMode, FBlueprintEditorApplicationModes::GetLocalizedMode)
{
	MyBlueprintEditor = InBlueprintEditor;
	
	BlueprintComponentsTabFactories.RegisterFactory(MakeShareable(new FConstructionScriptEditorSummoner(InBlueprintEditor)));
	BlueprintComponentsTabFactories.RegisterFactory(MakeShareable(new FSCSViewportSummoner(InBlueprintEditor)));
	BlueprintComponentsTabFactories.RegisterFactory(MakeShareable(new FSelectionDetailsSummoner(InBlueprintEditor)));
	BlueprintComponentsTabFactories.RegisterFactory(MakeShareable(new FDefaultsEditorSummoner(InBlueprintEditor)));
	BlueprintComponentsTabFactories.RegisterFactory(MakeShareable(new FFindResultsSummoner(InBlueprintEditor)));

	TabLayout = FTabManager::NewLayout( "Standalone_BlueprintComponents_Layout_v5" )
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient( 0.2f )
				->SetHideTabWell(true)
				->AddTab( InBlueprintEditor->GetToolbarTabId(), ETabState::OpenedTab )
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()
					->SetSizeCoefficient( 0.15f )
					->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.35f )
						->AddTab( FBlueprintEditorTabs::ConstructionScriptEditorID, ETabState::OpenedTab )
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.65f )
						->AddTab( FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab )
					)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient( 0.85f )
					->SetHideTabWell(true)
					->AddTab( FBlueprintEditorTabs::SCSViewportID, ETabState::OpenedTab )
				)
			)
		);

	// setup toolbar
	InBlueprintEditor->GetToolbarBuilder()->AddBlueprintEditorModesToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddCompileToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddComponentsToolbar(ToolbarExtender);
}

void FBlueprintComponentsApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(BlueprintComponentsTabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FBlueprintComponentsApplicationMode::PreDeactivateMode()
{
	FApplicationMode::PreDeactivateMode();

	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	BP->GetSCSEditor()->SetEnabled(true);
	BP->GetSCSEditor()->UpdateTree();
	BP->GetInspector()->SetEnabled(true);
	BP->GetInspector()->EnableComponentDetailsCustomization(false);
	BP->EnableSCSPreview(false);

	// Cache component selection before clearing so it can be restored
	for( auto& SCSNode : BP->GetSCSEditor()->GetSelectedNodes() )
	{
		CachedComponentSelection.AddUnique(SCSNode->GetComponentTemplate());
	}
	BP->GetSCSEditor()->ClearSelection();
}

void FBlueprintComponentsApplicationMode::PostActivateMode()
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	if (BP.IsValid())
	{
		auto SCSEditor = BP->GetSCSEditor();
		SCSEditor->UpdateTree();
		BP->EnableSCSPreview(true);
		BP->UpdateSCSPreview();
		BP->GetInspector()->EnableComponentDetailsCustomization(true);

		// Reselect the cached components
		TArray<TSharedPtr<FSCSEditorTreeNode>> Selection;
		for (auto Component : CachedComponentSelection)
		{
			if (Component.IsValid())
			{
				SCSEditor->SCSTreeWidget->SetItemSelection(SCSEditor->GetNodeFromActorComponent(Component.Get()), true);
			}
		}

		if (BP->GetSCSViewport()->GetIsSimulateEnabled())
		{
			SCSEditor->SetEnabled(false);
			BP->GetInspector()->SetEnabled(false);
		}
	}

	FApplicationMode::PostActivateMode();
}

////////////////////////////////////////
//
FBlueprintInterfaceApplicationMode::FBlueprintInterfaceApplicationMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor)
	: FApplicationMode(FBlueprintEditorApplicationModes::BlueprintInterfaceMode, FBlueprintEditorApplicationModes::GetLocalizedMode)
{
	MyBlueprintEditor = InBlueprintEditor;
	
	// Create the tab factories
	BlueprintInterfaceTabFactories.RegisterFactory(MakeShareable(new FDebugInfoSummoner(InBlueprintEditor)));
	BlueprintInterfaceTabFactories.RegisterFactory(MakeShareable(new FMyBlueprintSummoner(InBlueprintEditor)));
	if (GetDefault<UEditorExperimentalSettings>()->bEnableFindAndReplaceReferences)
	{
		BlueprintInterfaceTabFactories.RegisterFactory(MakeShareable(new FReplaceNodeReferencesSummoner(InBlueprintEditor)));
	}
	BlueprintInterfaceTabFactories.RegisterFactory(MakeShareable(new FCompilerResultsSummoner(InBlueprintEditor)));
	BlueprintInterfaceTabFactories.RegisterFactory(MakeShareable(new FFindResultsSummoner(InBlueprintEditor)));
	BlueprintInterfaceTabFactories.RegisterFactory(MakeShareable(new FSelectionDetailsSummoner(InBlueprintEditor)));

	TabLayout = FTabManager::NewLayout( "Standalone_BlueprintInterface_Layout_v3" )
		->AddArea
		(
			FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.70f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.186721f)
						->SetHideTabWell(true)
						->AddTab(InBlueprintEditor->GetToolbarTabId(), ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.80f )
						->AddTab( "Document", ETabState::ClosedTab )
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.20f )
						->AddTab( FBlueprintEditorTabs::CompilerResultsID, ETabState::ClosedTab )
						->AddTab( FBlueprintEditorTabs::FindResultsID, ETabState::ClosedTab )
					)
				)
				->Split
				(
					FTabManager::NewSplitter() ->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.15f)
					->Split
					(
						FTabManager::NewStack() ->SetSizeCoefficient(0.5f)
						->AddTab( FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab )
					)
					->Split
					(
						FTabManager::NewStack() ->SetSizeCoefficient(0.5f)
						->AddTab( FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab )
						->AddTab( FBlueprintEditorTabs::PaletteID, ETabState::ClosedTab )
					)
				)
			)
		);

	// setup toolbar
	InBlueprintEditor->GetToolbarBuilder()->AddCompileToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(ToolbarExtender);
}

void FBlueprintInterfaceApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(BlueprintInterfaceTabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FBlueprintInterfaceApplicationMode::PreDeactivateMode()
{
	FApplicationMode::PreDeactivateMode();

	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();

	BP->SaveEditedObjectState();
}

void FBlueprintInterfaceApplicationMode::PostActivateMode()
{
	// Reopen any documents that were open when the blueprint was last saved
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	BP->RestoreEditedObjectState();

	FApplicationMode::PostActivateMode();
}

////////////////////////////////////////
//
FBlueprintMacroApplicationMode::FBlueprintMacroApplicationMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor)
	: FApplicationMode(FBlueprintEditorApplicationModes::BlueprintMacroMode, FBlueprintEditorApplicationModes::GetLocalizedMode)
{
	MyBlueprintEditor = InBlueprintEditor;
	
	// Create the tab factories
	BlueprintMacroTabFactories.RegisterFactory(MakeShareable(new FDebugInfoSummoner(InBlueprintEditor)));
	BlueprintMacroTabFactories.RegisterFactory(MakeShareable(new FMyBlueprintSummoner(InBlueprintEditor)));
	if (GetDefault<UEditorExperimentalSettings>()->bEnableFindAndReplaceReferences)
	{
		BlueprintMacroTabFactories.RegisterFactory(MakeShareable(new FReplaceNodeReferencesSummoner(InBlueprintEditor)));
	}
	BlueprintMacroTabFactories.RegisterFactory(MakeShareable(new FPaletteSummoner(InBlueprintEditor)));
	BlueprintMacroTabFactories.RegisterFactory(MakeShareable(new FFindResultsSummoner(InBlueprintEditor)));
	BlueprintMacroTabFactories.RegisterFactory(MakeShareable(new FSelectionDetailsSummoner(InBlueprintEditor)));

	TabLayout = FTabManager::NewLayout( "Standalone_BlueprintMacro_Layout_v3" )
		->AddArea
		(
			FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.70f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.186721f)
						->SetHideTabWell(true)
						->AddTab(InBlueprintEditor->GetToolbarTabId(), ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.80f )
						->AddTab( "Document", ETabState::ClosedTab )
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.20f )
						->AddTab( FBlueprintEditorTabs::FindResultsID, ETabState::ClosedTab )
					)
				)
				->Split
				(
					FTabManager::NewSplitter() ->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.15f)
					->Split
					(
						FTabManager::NewStack() ->SetSizeCoefficient(0.5f)
						->AddTab( FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab )
					)
					->Split
					(
						FTabManager::NewStack() ->SetSizeCoefficient(0.5f)
						->AddTab( FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab )
						->AddTab( FBlueprintEditorTabs::PaletteID, ETabState::ClosedTab )
					)
				)
			)
		);

	// setup toolbar
	InBlueprintEditor->GetToolbarBuilder()->AddCompileToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddScriptingToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddDebuggingToolbar(ToolbarExtender);
}

void FBlueprintMacroApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(BlueprintMacroTabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FBlueprintMacroApplicationMode::PreDeactivateMode()
{
	FApplicationMode::PreDeactivateMode();

	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();

	BP->SaveEditedObjectState();
}

void FBlueprintMacroApplicationMode::PostActivateMode()
{
	// Reopen any documents that were open when the blueprint was last saved
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	BP->RestoreEditedObjectState();

	FApplicationMode::PostActivateMode();
}

////////////////////////////////////////
//
FBlueprintEditorUnifiedMode::FBlueprintEditorUnifiedMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor, FName InModeName, FText(*GetLocalizedMode)( const FName ), const bool bRegisterViewport)
	: FApplicationMode(InModeName, GetLocalizedMode)
{
	MyBlueprintEditor = InBlueprintEditor;

	// Create the tab factories
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FDebugInfoSummoner(InBlueprintEditor)));
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FPaletteSummoner(InBlueprintEditor)));
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FMyBlueprintSummoner(InBlueprintEditor)));
	if (GetDefault<UEditorExperimentalSettings>()->bEnableFindAndReplaceReferences)
	{
		BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FReplaceNodeReferencesSummoner(InBlueprintEditor)));
	}
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FCompilerResultsSummoner(InBlueprintEditor)));
	BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FFindResultsSummoner(InBlueprintEditor)));
	
	if( bRegisterViewport )
	{
		BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FConstructionScriptEditorSummoner(InBlueprintEditor)));
		BlueprintEditorTabFactories.RegisterFactory(MakeShareable(new FSCSViewportSummoner(InBlueprintEditor)));
	}

	CoreTabFactories.RegisterFactory(MakeShareable(new FSelectionDetailsSummoner(InBlueprintEditor)));

	if ( bRegisterViewport )
	{
		TabLayout = FTabManager::NewLayout( "Blueprints_Unified_Components_v6" )
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.15f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.30f)
						->AddTab( FBlueprintEditorTabs::ConstructionScriptEditorID, ETabState::OpenedTab )
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.70f)
						->AddTab( FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab )
					)
				)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.60f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.186721f)
						->SetHideTabWell(true)
						->AddTab(InBlueprintEditor->GetToolbarTabId(), ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.80f )
						->AddTab(FBlueprintEditorTabs::SCSViewportID, ETabState::OpenedTab)
						->AddTab( "Document", ETabState::ClosedTab )
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.20f )
						->AddTab( FBlueprintEditorTabs::CompilerResultsID, ETabState::ClosedTab )
						->AddTab( FBlueprintEditorTabs::FindResultsID, ETabState::ClosedTab )
					)
				)
				->Split
				(
					FTabManager::NewSplitter() ->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.25f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.50f)
						->AddTab( FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab )
						->AddTab( FBlueprintEditorTabs::PaletteID, ETabState::ClosedTab )
					)
				)
			)
		);
	}
	else
	{
		TabLayout = FTabManager::NewLayout( "Blueprints_Unified_v4" )
		->AddArea
		(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
		->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.15f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.50f)
						->AddTab( FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab )
					)
				)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.60f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.186721f)
						->SetHideTabWell(true)
						->AddTab(InBlueprintEditor->GetToolbarTabId(), ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.80f )
						->AddTab( "Document", ETabState::ClosedTab )
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient( 0.20f )
						->AddTab( FBlueprintEditorTabs::CompilerResultsID, ETabState::ClosedTab )
						->AddTab( FBlueprintEditorTabs::FindResultsID, ETabState::ClosedTab )
					)
				)
				->Split
				(
					FTabManager::NewSplitter() ->SetOrientation( Orient_Vertical )
					->SetSizeCoefficient(0.25f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.60f)
						->AddTab( FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab )
						->AddTab( FBlueprintEditorTabs::PaletteID, ETabState::ClosedTab )
					)
				)
			)
		);
	}
	
	// setup toolbar
	//@TODO: Keep this in sync with AnimBlueprintMode.cpp
	//InBlueprintEditor->GetToolbarBuilder()->AddBlueprintEditorModesToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddCompileToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddScriptingToolbar(ToolbarExtender);
	InBlueprintEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(ToolbarExtender);
	
	if ( bRegisterViewport )
	{
		InBlueprintEditor->GetToolbarBuilder()->AddComponentsToolbar(ToolbarExtender);
	}
	
	InBlueprintEditor->GetToolbarBuilder()->AddDebuggingToolbar(ToolbarExtender);

	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
	BlueprintEditorModule.OnRegisterTabsForEditor().Broadcast(BlueprintEditorTabFactories, InModeName, InBlueprintEditor);

	LayoutExtender = MakeShared<FLayoutExtender>();
	BlueprintEditorModule.OnRegisterLayoutExtensions().Broadcast(*LayoutExtender);
}

void FBlueprintEditorUnifiedMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(CoreTabFactories);
	BP->PushTabFactories(BlueprintEditorOnlyTabFactories);
	BP->PushTabFactories(BlueprintEditorTabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FBlueprintEditorUnifiedMode::PreDeactivateMode()
{
	FApplicationMode::PreDeactivateMode();

	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->SaveEditedObjectState();
	BP->GetMyBlueprintWidget()->ClearGraphActionMenuSelection();
}

void FBlueprintEditorUnifiedMode::PostActivateMode()
{
	// Reopen any documents that were open when the blueprint was last saved
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	BP->RestoreEditedObjectState();
	BP->SetupViewForBlueprintEditingMode();

	FApplicationMode::PostActivateMode();
}

#undef LOCTEXT_NAMESPACE
