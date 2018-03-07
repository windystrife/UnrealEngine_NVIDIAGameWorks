// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SoundClassEditor.h"
#include "Modules/ModuleManager.h"
#include "EditorStyleSet.h"
#include "EdGraph/EdGraph.h"
#include "SoundClassGraph/SoundClassGraph.h"
#include "SoundClassGraph/SoundClassGraphNode.h"
#include "SoundClassGraph/SoundClassGraphSchema.h"
#include "Sound/SoundClass.h"
#include "Editor.h"

#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "AssetRegistryModule.h"

#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "SSoundClassActionMenu.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Commands/GenericCommands.h"
#include "AudioEditorModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/SoundClassFactory.h"
#include "UObject/Package.h"
#include "UObjectIterator.h"

#define LOCTEXT_NAMESPACE "SoundClassEditor"
DEFINE_LOG_CATEGORY_STATIC( LogSoundClassEditor, Log, All );

const FName FSoundClassEditor::GraphCanvasTabId( TEXT( "SoundClassEditor_GraphCanvas" ) );
const FName FSoundClassEditor::PropertiesTabId( TEXT( "SoundClassEditor_Properties" ) );


//////////////////////////////////////////////////////////////////////////
// FSoundClassEditor

void FSoundClassEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_SoundClassEditor", "Sound Class Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner( GraphCanvasTabId, FOnSpawnTab::CreateSP(this, &FSoundClassEditor::SpawnTab_GraphCanvas) )
		.SetDisplayName( LOCTEXT( "GraphCanvasTab", "Graph" ) )
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "GraphEditor.EventGraph_16x"));

	InTabManager->RegisterTabSpawner( PropertiesTabId, FOnSpawnTab::CreateSP(this, &FSoundClassEditor::SpawnTab_Properties) )
		.SetDisplayName( LOCTEXT( "PropertiesTab", "Details" ) )
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FSoundClassEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner( GraphCanvasTabId );
	InTabManager->UnregisterTabSpawner( PropertiesTabId );
}

void FSoundClassEditor::InitSoundClassEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit )
{
	SoundClass = CastChecked<USoundClass>(ObjectToEdit);

	while (SoundClass->ParentClass)
	{
		SoundClass = SoundClass->ParentClass;
	}

	// Support undo/redo
	SoundClass->SetFlags(RF_Transactional);

	GEditor->RegisterForUndo(this);

	ToolkitCommands->MapAction(
		FGenericCommands::Get().Undo,
		FExecuteAction::CreateSP( this, &FSoundClassEditor::UndoGraphAction ));

	ToolkitCommands->MapAction(
		FGenericCommands::Get().Redo,
		FExecuteAction::CreateSP( this, &FSoundClassEditor::RedoGraphAction ));

	if( !SoundClass->SoundClassGraph )
	{
		USoundClassGraph* SoundClassGraph = CastChecked<USoundClassGraph>(FBlueprintEditorUtils::CreateNewGraph(SoundClass, NAME_None, USoundClassGraph::StaticClass(), USoundClassGraphSchema::StaticClass()));
		SoundClassGraph->SetRootSoundClass(SoundClass);

		SoundClass->SoundClassGraph = SoundClassGraph;
	}

	CastChecked<USoundClassGraph>(SoundClass->SoundClassGraph)->RebuildGraph();

	CreateInternalWidgets();

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_SoundClassEditor_Layout_v2" )
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->SetHideTabWell( true )
			->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewSplitter()
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.2f)
				->AddTab(PropertiesTabId, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.8f)
				->AddTab(GraphCanvasTabId, ETabState::OpenedTab)
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, TEXT("SoundClassEditorApp"), StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, SoundClass);

	IAudioEditorModule* AudioEditorModule = &FModuleManager::LoadModuleChecked<IAudioEditorModule>( "AudioEditor" );
	AddMenuExtender(AudioEditorModule->GetSoundClassMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
	AddToolbarExtender(AudioEditorModule->GetSoundClassToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	GraphEditor->SelectAllNodes();
	for (UObject* SelectedNode : GraphEditor->GetSelectedNodes())
	{
		USoundClassGraphNode* GraphNode = CastChecked<USoundClassGraphNode>(SelectedNode);
		if (GraphNode->SoundClass == ObjectToEdit)
		{
			GraphEditor->ClearSelectionSet();
			GraphEditor->SetNodeSelection(GraphNode, true);
			DetailsView->SetObject(ObjectToEdit);
			break;
		}
	}
}

FSoundClassEditor::FSoundClassEditor()
	: SoundClass(nullptr)
{
}

FSoundClassEditor::~FSoundClassEditor()
{
	GEditor->UnregisterForUndo( this );
	DetailsView.Reset();
}

void FSoundClassEditor::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject(SoundClass);
}

TSharedRef<SDockTab> FSoundClassEditor::SpawnTab_GraphCanvas(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId() == GraphCanvasTabId );

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("GraphCanvasTitle", "Graph"))
		[
			GraphEditor.ToSharedRef()
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FSoundClassEditor::SpawnTab_Properties(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId() == PropertiesTabId );

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("SoundClassEditor.Tabs.Properties") )
		.Label( LOCTEXT( "SoundClassPropertiesTitle", "Details" ) )
		[
			DetailsView.ToSharedRef()
		];
	
	return SpawnedTab;
}

FName FSoundClassEditor::GetToolkitFName() const
{
	return FName("SoundClassEditor");
}

FText FSoundClassEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Sound Class Editor");
}

FString FSoundClassEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT( "WorldCentricTabPrefix", "Sound Class " ).ToString();
}

FLinearColor FSoundClassEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.3f, 0.2f, 0.5f, 0.5f );
}

void FSoundClassEditor::CreateInternalWidgets()
{
	GraphEditor = CreateGraphEditorWidget();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	const FDetailsViewArgs DetailsViewArgs( false, false, true, FDetailsViewArgs::ObjectsUseNameArea, false );
	DetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );
	DetailsView->SetObject( SoundClass );
}

TSharedRef<SGraphEditor> FSoundClassEditor::CreateGraphEditorWidget()
{
	if ( !GraphEditorCommands.IsValid() )
	{
		GraphEditorCommands = MakeShareable( new FUICommandList );

		// Editing commands
		GraphEditorCommands->MapAction( FGenericCommands::Get().SelectAll,
			FExecuteAction::CreateSP( this, &FSoundClassEditor::SelectAllNodes ),
			FCanExecuteAction::CreateSP( this, &FSoundClassEditor::CanSelectAllNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Delete,
			FExecuteAction::CreateSP( this, &FSoundClassEditor::RemoveSelectedNodes ),
			FCanExecuteAction::CreateSP( this, &FSoundClassEditor::CanRemoveNodes )
			);
	}

	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_SoundClass", "SOUND CLASS");

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FSoundClassEditor::OnSelectedNodesChanged);
	InEvents.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateSP(this, &FSoundClassEditor::OnCreateGraphActionMenu);

	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(true)
		.Appearance(AppearanceInfo)
		.GraphToEdit(SoundClass->SoundClassGraph)
		.GraphEvents(InEvents)
		.ShowGraphStateOverlay(false);
}

void FSoundClassEditor::OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection)
{
	TArray<UObject*> Selection;

	if(NewSelection.Num())
	{
		for(TSet<class UObject*>::TConstIterator SetIt(NewSelection);SetIt;++SetIt)
		{
			USoundClassGraphNode* GraphNode = CastChecked<USoundClassGraphNode>(*SetIt);
			Selection.Add(GraphNode->SoundClass);
		}
		DetailsView->SetObjects(Selection);
	}
	else
	{
		DetailsView->SetObject(SoundClass);
	}
}

FActionMenuContent FSoundClassEditor::OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed)
{
	TSharedRef<SSoundClassActionMenu> ActionMenu = 
		SNew(SSoundClassActionMenu)
		.GraphObj(InGraph)
		.NewNodePosition(InNodePosition)
		.DraggedFromPins(InDraggedPins)
		.AutoExpandActionMenu(bAutoExpand)
		.OnClosedCallback(InOnMenuClosed);

	return FActionMenuContent( ActionMenu, ActionMenu );
}

void FSoundClassEditor::SelectAllNodes()
{
	GraphEditor->SelectAllNodes();
}

bool FSoundClassEditor::CanSelectAllNodes() const
{
	return true;
}

void FSoundClassEditor::RemoveSelectedNodes()
{
	const FScopedTransaction Transaction( LOCTEXT("SoundClassEditorRemoveSelectedNode", "Sound Class Editor: Remove Selected SoundClasses from editor") );

	CastChecked<USoundClassGraph>(SoundClass->SoundClassGraph)->RecursivelyRemoveNodes(GraphEditor->GetSelectedNodes());

	GraphEditor->ClearSelectionSet();
}

bool FSoundClassEditor::CanRemoveNodes() const
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		USoundClassGraphNode* Node = Cast<USoundClassGraphNode>(*NodeIt);

		if (Node && Node->CanUserDeleteNode())
		{
			return true;
		}
	}

	return false;
}

void FSoundClassEditor::UndoGraphAction()
{
	GEditor->UndoTransaction();
}

void FSoundClassEditor::RedoGraphAction()
{
	// Clear selection, to avoid holding refs to nodes that go away
	GraphEditor->ClearSelectionSet();

	GEditor->RedoTransaction();
}

void FSoundClassEditor::CreateSoundClass(UEdGraphPin* FromPin, const FVector2D& Location, const FString& Name)
{
	// If we have a valid name
	if (!Name.IsEmpty() && Name != SoundClass->GetName())
	{
		// Derive new package path from existing asset's path
		FString PackagePath = SoundClass->GetPathName();
		FString AssetName = FString::Printf(TEXT("/%s.%s"), *SoundClass->GetName(), *SoundClass->GetName());
		PackagePath.RemoveFromEnd(AssetName);

		// Create a sound class factory to create a new sound class
		USoundClassFactory* SoundClassFactory = NewObject<USoundClassFactory>();

		// Load asset tools to create the asset properly
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		USoundClass* NewSoundClass = Cast<USoundClass>(AssetToolsModule.Get().CreateAsset(Name, PackagePath, USoundClass::StaticClass(), SoundClassFactory, FName("SoundClassEditorNewAsset")));

		// This may fail if the asset has same name as existing asset, etc.
		if (NewSoundClass)
		{
			CastChecked<USoundClassGraph>(SoundClass->SoundClassGraph)->AddNewSoundClass(FromPin, NewSoundClass, Location.X, Location.Y);

			NewSoundClass->PostEditChange();
			NewSoundClass->MarkPackageDirty();
		}
	}
}

void FSoundClassEditor::PostUndo(bool bSuccess)
{
	GraphEditor->ClearSelectionSet();
	GraphEditor->NotifyGraphChanged();
}

#undef LOCTEXT_NAMESPACE
