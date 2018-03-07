// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SoundSubmixEditor.h"

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"
#include "AssetRegistryModule.h"

#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"

#include "GraphEditor.h"
#include "BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Toolkits/IToolkitHost.h"
#include "SDockTab.h"
#include "GenericCommands.h"
#include "Sound/SoundSubmix.h"
#include "SoundSubmixGraph/SoundSubmixGraph.h"
#include "SoundSubmixGraph/SoundSubmixGraphNode.h"
#include "SoundSubmixGraph/SoundSubmixGraphSchema.h"
#include "AudioEditorModule.h"
#include "SSoundSubmixActionMenu.h"
#include "Editor.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/SoundSubmixFactory.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "SoundSubmixEditor"
DEFINE_LOG_CATEGORY_STATIC( LogSoundSubmixEditor, Log, All );

const FName FSoundSubmixEditor::GraphCanvasTabId( TEXT( "SoundSubmixEditor_GraphCanvas" ) );
const FName FSoundSubmixEditor::PropertiesTabId( TEXT( "SoundSubmixEditor_Properties" ) );


//////////////////////////////////////////////////////////////////////////
// FSoundSubmixEditor

void FSoundSubmixEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_SoundSubmixEditor", "Sound Submix Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner( GraphCanvasTabId, FOnSpawnTab::CreateSP(this, &FSoundSubmixEditor::SpawnTab_GraphCanvas) )
		.SetDisplayName( LOCTEXT( "GraphCanvasTab", "Graph" ) )
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "GraphEditor.EventGraph_16x"));

	InTabManager->RegisterTabSpawner( PropertiesTabId, FOnSpawnTab::CreateSP(this, &FSoundSubmixEditor::SpawnTab_Properties) )
		.SetDisplayName( LOCTEXT( "PropertiesTab", "Details" ) )
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FSoundSubmixEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner( GraphCanvasTabId );
	InTabManager->UnregisterTabSpawner( PropertiesTabId );
}

void FSoundSubmixEditor::InitSoundSubmixEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit )
{
	SoundSubmix = CastChecked<USoundSubmix>(ObjectToEdit);

	while (SoundSubmix->ParentSubmix)
	{
		SoundSubmix = SoundSubmix->ParentSubmix;
	}

	// Support undo/redo
	SoundSubmix->SetFlags(RF_Transactional);

	GEditor->RegisterForUndo(this);

	ToolkitCommands->MapAction(
		FGenericCommands::Get().Undo,
		FExecuteAction::CreateSP( this, &FSoundSubmixEditor::UndoGraphAction ));

	ToolkitCommands->MapAction(
		FGenericCommands::Get().Redo,
		FExecuteAction::CreateSP( this, &FSoundSubmixEditor::RedoGraphAction ));

	if( !SoundSubmix->SoundSubmixGraph )
	{
		USoundSubmixGraph* SoundSubmixGraph = CastChecked<USoundSubmixGraph>(FBlueprintEditorUtils::CreateNewGraph(SoundSubmix, NAME_None, USoundSubmixGraph::StaticClass(), USoundSubmixGraphSchema::StaticClass()));
		SoundSubmixGraph->SetRootSoundSubmix(SoundSubmix);

		SoundSubmix->SoundSubmixGraph = SoundSubmixGraph;
	}

	CastChecked<USoundSubmixGraph>(SoundSubmix->SoundSubmixGraph)->RebuildGraph();

	CreateInternalWidgets();

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_SoundSubmixEditor_Layout_v2" )
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
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, TEXT("SoundSubmixEditorApp"), StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, SoundSubmix);

	IAudioEditorModule* AudioEditorModule = &FModuleManager::LoadModuleChecked<IAudioEditorModule>( "AudioEditor" );
	AddMenuExtender(AudioEditorModule->GetSoundSubmixMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
	AddToolbarExtender(AudioEditorModule->GetSoundSubmixToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	GraphEditor->SelectAllNodes();
	for (UObject* SelectedNode : GraphEditor->GetSelectedNodes())
	{
		USoundSubmixGraphNode* GraphNode = CastChecked<USoundSubmixGraphNode>(SelectedNode);
		if (GraphNode->SoundSubmix == ObjectToEdit)
		{
			GraphEditor->ClearSelectionSet();
			GraphEditor->SetNodeSelection(GraphNode, true);
			DetailsView->SetObject(ObjectToEdit);
			break;
		}
	}
}

FSoundSubmixEditor::FSoundSubmixEditor()
	: SoundSubmix(nullptr)
{
}

FSoundSubmixEditor::~FSoundSubmixEditor()
{
	GEditor->UnregisterForUndo( this );
	DetailsView.Reset();
}

void FSoundSubmixEditor::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject(SoundSubmix);
}

TSharedRef<SDockTab> FSoundSubmixEditor::SpawnTab_GraphCanvas(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId() == GraphCanvasTabId );

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("GraphCanvasTitle", "Graph"))
		[
			GraphEditor.ToSharedRef()
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FSoundSubmixEditor::SpawnTab_Properties(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId() == PropertiesTabId );

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("SoundSubmixEditor.Tabs.Properties") )
		.Label( LOCTEXT( "SoundSubmixPropertiesTitle", "Details" ) )
		[
			DetailsView.ToSharedRef()
		];
	
	return SpawnedTab;
}

FName FSoundSubmixEditor::GetToolkitFName() const
{
	return FName("SoundSubmixEditor");
}

FText FSoundSubmixEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Sound Submix Editor");
}

FString FSoundSubmixEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT( "WorldCentricTabPrefix", "Sound Submix " ).ToString();
}

FLinearColor FSoundSubmixEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.2f, 0.4f, 0.8f, 0.5f );
}

void FSoundSubmixEditor::CreateInternalWidgets()
{
	GraphEditor = CreateGraphEditorWidget();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	const FDetailsViewArgs DetailsViewArgs( false, false, true, FDetailsViewArgs::ObjectsUseNameArea, false );
	DetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );
	DetailsView->SetObject( SoundSubmix );
}

TSharedRef<SGraphEditor> FSoundSubmixEditor::CreateGraphEditorWidget()
{
	if ( !GraphEditorCommands.IsValid() )
	{
		GraphEditorCommands = MakeShareable( new FUICommandList );

		// Editing commands
		GraphEditorCommands->MapAction( FGenericCommands::Get().SelectAll,
			FExecuteAction::CreateSP( this, &FSoundSubmixEditor::SelectAllNodes ),
			FCanExecuteAction::CreateSP( this, &FSoundSubmixEditor::CanSelectAllNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Delete,
			FExecuteAction::CreateSP( this, &FSoundSubmixEditor::RemoveSelectedNodes ),
			FCanExecuteAction::CreateSP( this, &FSoundSubmixEditor::CanRemoveNodes )
			);
	}

	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_SoundSubmix", "SOUND SUBMIX");

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FSoundSubmixEditor::OnSelectedNodesChanged);
	InEvents.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateSP(this, &FSoundSubmixEditor::OnCreateGraphActionMenu);

	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(true)
		.Appearance(AppearanceInfo)
		.GraphToEdit(SoundSubmix->SoundSubmixGraph)
		.GraphEvents(InEvents)
		.ShowGraphStateOverlay(false);
}

void FSoundSubmixEditor::OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection)
{
	TArray<UObject*> Selection;

	if(NewSelection.Num())
	{
		for(TSet<class UObject*>::TConstIterator SetIt(NewSelection);SetIt;++SetIt)
		{
			USoundSubmixGraphNode* GraphNode = CastChecked<USoundSubmixGraphNode>(*SetIt);
			Selection.Add(GraphNode->SoundSubmix);
		}
		DetailsView->SetObjects(Selection);
	}
	else
	{
		DetailsView->SetObject(SoundSubmix);
	}
}

FActionMenuContent FSoundSubmixEditor::OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed)
{
	TSharedRef<SSoundSubmixActionMenu> ActionMenu = 
		SNew(SSoundSubmixActionMenu)
		.GraphObj(InGraph)
		.NewNodePosition(InNodePosition)
		.DraggedFromPins(InDraggedPins)
		.AutoExpandActionMenu(bAutoExpand)
		.OnClosedCallback(InOnMenuClosed);

	return FActionMenuContent( ActionMenu, ActionMenu );
}

void FSoundSubmixEditor::SelectAllNodes()
{
	GraphEditor->SelectAllNodes();
}

bool FSoundSubmixEditor::CanSelectAllNodes() const
{
	return true;
}

void FSoundSubmixEditor::RemoveSelectedNodes()
{
	const FScopedTransaction Transaction( LOCTEXT("SoundSubmixEditorRemoveSelectedNode", "Sound Submix Editor: Remove Selected SoundSubmixes from editor") );

	CastChecked<USoundSubmixGraph>(SoundSubmix->SoundSubmixGraph)->RecursivelyRemoveNodes(GraphEditor->GetSelectedNodes());

	GraphEditor->ClearSelectionSet();
}

bool FSoundSubmixEditor::CanRemoveNodes() const
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		USoundSubmixGraphNode* Node = Cast<USoundSubmixGraphNode>(*NodeIt);

		if (Node && Node->CanUserDeleteNode())
		{
			return true;
		}
	}

	return false;
}

void FSoundSubmixEditor::UndoGraphAction()
{
	GEditor->UndoTransaction();
}

void FSoundSubmixEditor::RedoGraphAction()
{
	// Clear selection, to avoid holding refs to nodes that go away
	GraphEditor->ClearSelectionSet();

	GEditor->RedoTransaction();
}

void FSoundSubmixEditor::CreateSoundSubmix(UEdGraphPin* FromPin, const FVector2D Location, const FString& Name)
{
	// If we have a valid name
	if (!Name.IsEmpty() && Name != SoundSubmix->GetName())
	{
		// Derive new package path from existing asset's path
		FString PackagePath = SoundSubmix->GetPathName();
		FString AssetName = FString::Printf(TEXT("/%s.%s"), *SoundSubmix->GetName(), *SoundSubmix->GetName());
		PackagePath.RemoveFromEnd(AssetName);

		// Create a sound submix factory to create a new sound class
		USoundSubmixFactory* SoundSubmixFactory = NewObject<USoundSubmixFactory>();

		// Load asset tools to create the asset properly
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		USoundSubmix* NewSoundSubmix = Cast<USoundSubmix>(AssetToolsModule.Get().CreateAsset(Name, PackagePath, USoundSubmix::StaticClass(), SoundSubmixFactory, FName("SoundSubmixEditorNewAsset")));

		if (NewSoundSubmix)
		{
			CastChecked<USoundSubmixGraph>(SoundSubmix->SoundSubmixGraph)->AddNewSoundSubmix(FromPin, NewSoundSubmix, Location.X, Location.Y);

			NewSoundSubmix->PostEditChange();
			NewSoundSubmix->MarkPackageDirty();
		}
	}
}

void FSoundSubmixEditor::PostUndo(bool bSuccess)
{
	GraphEditor->ClearSelectionSet();
	GraphEditor->NotifyGraphChanged();
}

#undef LOCTEXT_NAMESPACE
