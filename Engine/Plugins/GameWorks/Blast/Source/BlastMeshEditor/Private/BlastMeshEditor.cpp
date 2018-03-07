// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlastMeshEditor.h"
#include "BlastMeshEditorCommands.h"
#include "BlastChunkParamsProxy.h"
#include "BlastMeshEditorModule.h"
#include "ViewportBlastMeshComponent.h"
#include "BlastFractureSettings.h"
#include "BlastFracture.h"
#include "BlastMesh.h"
#include "SBlastChunkTree.h"
#include "SBlastDepthFilter.h"
#include "BlastMeshEditorDialogs.h"

#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"
#include "EditorDirectories.h"
#include "DesktopPlatformModule.h"
#include "Framework/Commands/UICommandList.h"
#include "Public/FbxImporter.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SSlider.h"
#include "EditorReimportHandler.h"
#include "SListView.h"
#include "SUniformGridPanel.h"
#include "SNumericEntryBox.h"
#include "SScrollBox.h"
#include "Editor.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Docking/SDockTab.h"
#include "Engine/Selection.h"
#include "SAdvancedPreviewDetailsTab.h"
#include "EngineUtils.h"
#include "Math/UnrealMathUtility.h"
#include "SButton.h"
#include "EditorStyleSet.h"
#include "Widgets/Images/SImage.h"
#include "WidgetLayoutLibrary.h"
#include "CanvasPanelSlot.h"

#define LOCTEXT_NAMESPACE "BlastMeshEditor"

const FName FBlastMeshEditor::ChunkHierarchyTabId(TEXT("BlastMeshEditor_ChunkHierarchy"));
const FName FBlastMeshEditor::ViewportTabId( TEXT( "BlastMeshEditor_Viewport" ) );
const FName FBlastMeshEditor::PropertiesTabId( TEXT( "BlastMeshEditor_Properties" ) );
const FName FBlastMeshEditor::FractureSettingsTabId( TEXT( "BlastMeshEditor_FractureSettings" ) );
//const FName FBlastMeshEditor::FractureHistoryTabId(TEXT("BlastMeshEditor_FractureHistory"));
const FName FBlastMeshEditor::ChunkParametersTabId( TEXT( "BlastMeshEditor_ChunkParameters" ) );
const FName FBlastMeshEditor::AdvancedPreviewTabId(TEXT("BlastMeshEditor_AdvancedPreview"));


static float ExplodeRange = 5.0f;

FBlastMeshEditor::~FBlastMeshEditor()
{
	if (FractureSettings && FractureSettings->IsValidLowLevel() 
		&& FractureSettings->FractureSession.IsValid() && Fracturer.IsValid())
	{
		Fracturer->FinishFractureSession(FractureSettings->FractureSession);
		FractureSettings->FractureSession.Reset();
	}
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);

	GEditor->OnObjectReimported().RemoveAll(this);
}

void FBlastMeshEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_BlastMeshEditor", "Blast Mesh Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(ChunkHierarchyTabId, FOnSpawnTab::CreateSP(this, &FBlastMeshEditor::SpawnTab_ChunkHierarchy))
		.SetDisplayName(LOCTEXT("ChunkHierarchyTab", "Chunks"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.ChunkHierarchy"));

	InTabManager->RegisterTabSpawner( ViewportTabId, FOnSpawnTab::CreateSP(this, &FBlastMeshEditor::SpawnTab_Viewport) )
		.SetDisplayName( LOCTEXT("ViewportTab", "Viewport") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports"));
		
	InTabManager->RegisterTabSpawner( PropertiesTabId, FOnSpawnTab::CreateSP(this, &FBlastMeshEditor::SpawnTab_Properties) )
		.SetDisplayName( LOCTEXT("PropertiesTab", "Blast Settings") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "BlastMeshEditor.Tabs.BlastSettings"));

	InTabManager->RegisterTabSpawner( FractureSettingsTabId, FOnSpawnTab::CreateSP(this, &FBlastMeshEditor::SpawnTab_FractureSettings) )
		.SetDisplayName( LOCTEXT("FractureSettingsTab", "Fracture Settings") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "BlastMeshEditor.Tabs.FractureSettings"));

	//InTabManager->RegisterTabSpawner(FractureHistoryTabId, FOnSpawnTab::CreateSP(this, &FBlastMeshEditor::SpawnTab_FractureHistory))
	//	.SetDisplayName(LOCTEXT("FractureScriptsTab", "Fracture history"))
	//	.SetGroup(WorkspaceMenuCategoryRef)
	//	.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "BlastMeshEditor.Tabs.FractureHistory"));

	InTabManager->RegisterTabSpawner( ChunkParametersTabId, FOnSpawnTab::CreateSP(this, &FBlastMeshEditor::SpawnTab_ChunkParameters) )
		.SetDisplayName( LOCTEXT("ChunkParametersTab", "Chunk Parameters") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "BlastMeshEditor.Tabs.ChunkParameters"));

	InTabManager->RegisterTabSpawner(AdvancedPreviewTabId, FOnSpawnTab::CreateSP(this, &FBlastMeshEditor::SpawnTab_AdvancedPreview))
		.SetDisplayName(NSLOCTEXT("PersonaModes", "PreviewSceneSettingsTab", "Preview Scene Settings"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"))
		.SetTooltipText(NSLOCTEXT("PersonaModes", "AdvancedPreviewSettingsToolTip", "The Advanced Preview Settings tab will let you alter the preview scene's settings."));

}

void FBlastMeshEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(ChunkHierarchyTabId);
	InTabManager->UnregisterTabSpawner(ViewportTabId);
	InTabManager->UnregisterTabSpawner(PropertiesTabId);
	InTabManager->UnregisterTabSpawner(FractureSettingsTabId);
	//InTabManager->UnregisterTabSpawner(FractureHistoryTabId);
	InTabManager->UnregisterTabSpawner(ChunkParametersTabId);
	InTabManager->UnregisterTabSpawner(AdvancedPreviewTabId);
}

void FBlastMeshEditor::InitBlastMeshEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UBlastMesh* InBlastMesh )
{
	FReimportManager::Instance()->OnPostReimport().AddRaw(this, &FBlastMeshEditor::OnPostReimport);

	// Register our commands. This will only register them if not previously registered
	FBlastMeshEditorCommands::Register();

	BindCommands();

	ExplodeFractionOfRange = 0.1f / ExplodeRange;

	this->BlastMesh = NULL;

	ChunkHierarchy = SNew(SBlastChunkTree, SharedThis(this));

	Viewport = SNew(SBlastMeshEditorViewport)
	.BlastMeshEditor(SharedThis(this))
	.ObjectToEdit(InBlastMesh);
	
	FDetailsViewArgs Args;
	Args.bLockable = false;
	Args.bHideSelectionTip = true;
	Args.NotifyHook = Viewport.Get();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	BlastMeshDetailsView = PropertyModule.CreateDetailView(Args);
	BlastMeshDetailsView->SetObject( InBlastMesh );

	Fracturer = FBlastFracture::GetInstance();
	FractureSettings = Fracturer->CreateFractureSettings(this);
	FractureSettingsView = PropertyModule.CreateDetailView(Args);
	FractureSettingsView->SetObject(FractureSettings);

	FractureSettingsCustomView = PropertyModule.CreateDetailView(Args);
	OnFractureMethodChanged();

	FractureSettings->OnFractureMethodChanged.BindSP(this, &FBlastMeshEditor::OnFractureMethodChanged);
	FractureSettings->OnMaterialSelected.BindSP(this, &FBlastMeshEditor::OnBlastMeshReloaded);
	
	ChunkParametersView = PropertyModule.CreateDetailView(Args);
	ChunkParametersView->SetObject(NULL, false);

	//FractureHistoryView = SNew(SListView<TSharedPtr<FString>>)
	//	.ListItemsSource(&FractureSettings->FractureHistory)
	//	.SelectionMode(ESelectionMode::None)
	//	.OnGenerateRow(this, &FBlastMeshEditor::OnGenerateRowForFractureHistory);


	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_BlastMeshEditor_Layout_v4.1" )
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->AddTab(GetToolbarTabId(), ETabState::OpenedTab) ->SetHideTabWell(true)
		)
		->Split
		(
			FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal) ->SetSizeCoefficient(0.9f)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.15f)
				->AddTab(ChunkHierarchyTabId, ETabState::OpenedTab)->SetHideTabWell(true)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.5f)
				->AddTab(ViewportTabId, ETabState::OpenedTab) ->SetHideTabWell(true)
			)
			->Split
			(
				FTabManager::NewSplitter() ->SetOrientation(Orient_Vertical) ->SetSizeCoefficient(0.35f)
				->Split
				(
					FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal) ->SetSizeCoefficient(0.5f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->AddTab(PropertiesTabId, ETabState::OpenedTab)
						->AddTab(ChunkParametersTabId, ETabState::OpenedTab)
						->AddTab(AdvancedPreviewTabId, ETabState::OpenedTab)
						->SetForegroundTab(PropertiesTabId)
					)
				)
				//->Split
				//(
				//	FTabManager::NewStack()
				//	->SetSizeCoefficient(0.55f)
				//	->AddTab(FractureSettingsTabId, ETabState::OpenedTab)
				//	->AddTab(FractureHistoryTabId, ETabState::OpenedTab)
				//)
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, BlastMeshEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InBlastMesh );

	FBlastMeshEditorModule& BlastMeshEditorModule = FModuleManager::LoadModuleChecked<FBlastMeshEditorModule>( "BlastMeshEditor" );
	AddMenuExtender(BlastMeshEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	ExtendToolbar();

	SetBlastMesh(InBlastMesh);
	Viewport->ResetCamera();

	RegenerateMenusAndToolbars();
}

TSharedRef<SDockTab> FBlastMeshEditor::SpawnTab_ChunkHierarchy(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == ChunkHierarchyTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("BlastChunkHierarchy_TabTitle", "Chunks"))
		[
			ChunkHierarchy.ToSharedRef()
		];
}

TSharedRef<SDockTab> FBlastMeshEditor::SpawnTab_Viewport( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == ViewportTabId );

	return SNew(SDockTab)
		.Label( LOCTEXT("BlastMeshViewport_TabTitle", "Viewport") )
		[
			Viewport.ToSharedRef()
		];
}

TSharedRef<SDockTab> FBlastMeshEditor::SpawnTab_Properties( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == PropertiesTabId );

	BlastMeshDetailsViewTab = SNew(SDockTab)
		.Label( LOCTEXT("BlastMeshProperties_TabTitle", "Blast Settings") )
		[
			BlastMeshDetailsView.ToSharedRef()
		];

	return BlastMeshDetailsViewTab.ToSharedRef();
}

TSharedRef<SDockTab> FBlastMeshEditor::SpawnTab_FractureSettings( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == FractureSettingsTabId );

	return SNew(SDockTab)
		.Label( LOCTEXT("BlastMeshFractureSettings_TabTitle", "Fracture Settings") )
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			//.Padding(FMargin(8.0f, 4.0f))
			[
				FractureSettingsView.ToSharedRef()
			]
			+ SScrollBox::Slot()
			//.Padding(FMargin(8.0f, 4.0f))
			[
				FractureSettingsCustomView.ToSharedRef()
			]
		];
}

//TSharedRef<SDockTab> FBlastMeshEditor::SpawnTab_FractureHistory(const FSpawnTabArgs& Args)
//{
//	check(Args.GetTabId() == FractureHistoryTabId);
//
//	return SNew(SDockTab)
//		.Label(LOCTEXT("BlastMeshFractureHistory_TabTitle", "Fracture History"))
//		[
//			FractureHistoryView.ToSharedRef()
//		];
//}

TSharedRef<SDockTab> FBlastMeshEditor::SpawnTab_ChunkParameters( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == ChunkParametersTabId );

	ChunkParametersViewTab = SNew(SDockTab)
		.Label( LOCTEXT("BlastMeshChunkParameters_TabTitle", "Chunk Parameters") )
		[
			ChunkParametersView.ToSharedRef()
		];

	return ChunkParametersViewTab.ToSharedRef();
}


TSharedRef<SDockTab> FBlastMeshEditor::SpawnTab_AdvancedPreview(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == AdvancedPreviewTabId);

	return SNew(SDockTab)
		.Label(NSLOCTEXT("PersonaModes", "PreviewSceneSettingsTab", "Preview Scene Settings"))
		[
			SNew(SAdvancedPreviewDetailsTab, Viewport->GetPreviewScene().ToSharedRef())
		];
}

FName FBlastMeshEditor::GetToolkitFName() const
{
	return FName("BlastMeshEditor");
}

FText FBlastMeshEditor::GetBaseToolkitName() const
{
	return LOCTEXT( "AppLabel", "BlastMesh Editor" );
}

FString FBlastMeshEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "BlastMesh ").ToString();
}

FLinearColor FBlastMeshEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}

void FBlastMeshEditor::BindCommands()
{
	const FBlastMeshEditorCommands& Commands = FBlastMeshEditorCommands::Get();

	const TSharedRef<FUICommandList>& UICommandList = GetToolkitCommands();

	UICommandList->MapAction(
		Commands.Fracture,
		FExecuteAction::CreateSP<FBlastMeshEditor>(this, &FBlastMeshEditor::Fracture),
		FCanExecuteAction(),
		FIsActionChecked());

	UICommandList->MapAction(
		Commands.Reset,
		FExecuteAction::CreateSP<FBlastMeshEditor, int32>(this, &FBlastMeshEditor::RemoveChildren, INDEX_NONE),
		FCanExecuteAction::CreateSP(this, &FBlastMeshEditor::IsFractured),
		FIsActionChecked());

	UICommandList->MapAction(
		Commands.FixChunkHierarchy,
		FExecuteAction::CreateSP(this, &FBlastMeshEditor::FixChunkHierarchy),
		FCanExecuteAction::CreateSP(this, &FBlastMeshEditor::IsFractured),
		FIsActionChecked());

	UICommandList->MapAction(
		Commands.FitUvCoordinates,
		FExecuteAction::CreateSP(this, &FBlastMeshEditor::FitUvCoordinates),
		FCanExecuteAction::CreateSP(this, &FBlastMeshEditor::IsFractured),
		FIsActionChecked());

	UICommandList->MapAction(
		Commands.ExportAssetToFile,
		FExecuteAction::CreateSP(this, &FBlastMeshEditor::ExportAssetToFile),
		FCanExecuteAction(),
		FIsActionChecked());

	/*UICommandList->MapAction(
		Commands.Refresh,
		FExecuteAction::CreateSP(this, &FBlastMeshEditor::RefreshFromStaticMesh),
		FCanExecuteAction::CreateSP(this, &FBlastMeshEditor::CanRefreshFromStaticMesh),
		FIsActionChecked());*/

	UICommandList->MapAction(
		Commands.RebuildCollisionMesh,
		FExecuteAction::CreateSP(this, &FBlastMeshEditor::RebuildCollisionMesh),
		FCanExecuteAction::CreateSP(this, &FBlastMeshEditor::IsFractured),
		FIsActionChecked());

	UICommandList->MapAction(
		Commands.ImportRootFromStaticMesh,
		FExecuteAction::CreateSP(this, &FBlastMeshEditor::ImportRootFromStaticMesh),
		FCanExecuteAction::CreateSP(this, &FBlastMeshEditor::CanImportRootFromStaticMesh),
		FCanExecuteAction(),
		FIsActionChecked());
}

void FBlastMeshEditor::ExtendToolbar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, TSharedRef<SWidget> PreviewBox, TSharedRef<SWidget> ExplodeBox/*, TSharedRef<SWidget> FractureScriptsWidget*/)
		{
			ToolbarBuilder.BeginSection("Toolbar");
			{
				ToolbarBuilder.AddToolBarButton(FBlastMeshEditorCommands::Get().Fracture);
				ToolbarBuilder.AddToolBarButton(FBlastMeshEditorCommands::Get().Reset);
				ToolbarBuilder.AddToolBarButton(FBlastMeshEditorCommands::Get().FixChunkHierarchy);
				//ToolbarBuilder.AddToolBarButton(FBlastMeshEditorCommands::Get().Refresh);
				ToolbarBuilder.AddToolBarButton(FBlastMeshEditorCommands::Get().ImportRootFromStaticMesh);
				ToolbarBuilder.AddToolBarButton(FBlastMeshEditorCommands::Get().FitUvCoordinates);
				ToolbarBuilder.AddToolBarButton(FBlastMeshEditorCommands::Get().RebuildCollisionMesh);
				ToolbarBuilder.AddToolBarButton(FBlastMeshEditorCommands::Get().ExportAssetToFile);

				ToolbarBuilder.AddWidget(PreviewBox);
				ToolbarBuilder.AddWidget(ExplodeBox);
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	TSharedRef<SWidget> PreviewBox = SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4,0)
		.VAlign(VAlign_Center)
		[
			SAssignNew(PreviewDepthWidget, SBlastDepthFilter)
			.Text(LOCTEXT("BlastMeshEditor_PreviewDepth", "Preview Depth:"))
			.IsMultipleSelection(false)
			.OnDepthFilterChanged(this, &FBlastMeshEditor::PreviewDepthSelectionChanged)
		];

	TSharedRef<SWidget> ExplodeBox = SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( FMargin(8.0f, 2.0f, 8.0f, 2.0f) )
		.HAlign( HAlign_Left )
		[
			SNew( SVerticalBox )
			.AddMetaData<FTagMetaData>(TEXT("Blast.ExplodeAmount")) 
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding( FMargin(40.0f, 0.0f) )
			.HAlign(HAlign_Center)
			[
				SNew( STextBlock )
				.Text( LOCTEXT("ExplodeAmount", "Explode Amount")  )
				.Font( FEditorStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) )
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( FMargin(8.0f, 4.0f) )
			[	
				SNew( SHorizontalBox )
				+SHorizontalBox::Slot()
				.MaxWidth(200.0f)
				.FillWidth(1)
				.Padding( FMargin(0.0f, 2.0f) )
				[
					SAssignNew(ExplodeAmountSlider, SSlider)
					.Value(this, &FBlastMeshEditor::GetExplodeAmountSliderPosition)
					.OnValueChanged(this, &FBlastMeshEditor::OnSetExplodeAmount)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 8.0f, 2.0f, 0.0f, 2.0f)
				[
					SNew( STextBlock )
					.Text(this, &FBlastMeshEditor::GetButtonLabel )
					.Font( FEditorStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) )
				]
			]
		];

	//FractureScriptsWidget = SNew(SBlastFractureScripts).OnRunFractureScript(this, &FBlastMeshEditor::Fracture);
	//static_cast<SBlastFractureScripts*>(FractureScriptsWidget.Get())->SetFractureHistory(FractureSettings->FractureHistory);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar, PreviewBox, ExplodeBox /*, FractureScriptsWidget.ToSharedRef()*/)
		);

	AddToolbarExtender(ToolbarExtender);

	FBlastMeshEditorModule& BlastMeshEditorModule = FModuleManager::LoadModuleChecked<FBlastMeshEditorModule>( "BlastMeshEditor" );
	AddToolbarExtender(BlastMeshEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders());
}

void FBlastMeshEditor::SetBlastMesh(UBlastMesh* InBlastMesh)
{
	BlastMesh = InBlastMesh;

	// Set the details view.
	{
		TArray<UObject*> SelectedObjects;
		SelectedObjects.Add(BlastMesh);
		BlastMeshDetailsView->SetObjects(SelectedObjects);
	}

	// Set the fracture settings view.
	{
		//TArray<UObject*> SelectedObjects;
		//SelectedObjects.Add(FractureSettings);
		//FractureSettingsView->SetObjects(SelectedObjects);
	}

	// Set the Chunk Parameters view
	{
		ChunkParametersView->SetObject(nullptr, true);
	}

	if (PreviewDepthWidget.IsValid())
	{
		PreviewDepthWidget->SetBlastMesh(BlastMesh);
		PreviewDepthWidget->SetSelectedDepths(TArray<int32>({ FBlastMeshEditorModule::MaxChunkDepth }));
	}
	if (BlastMesh)
	{		
		if (BlastMesh->Mesh)
		{
			if (FractureSettings->FractureSession.IsValid())
			{
				Fracturer->FinishFractureSession(FractureSettings->FractureSession);
				FractureSettings->Reset();
			}
			FractureSettings->FractureSession = Fracturer->StartFractureSession(InBlastMesh, nullptr, FractureSettings);
		}
	}

	Viewport->UpdatePreviewMesh(BlastMesh);
	RefreshTool();
}

void FBlastMeshEditor::OnChangeMesh()
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();
	UBlastMesh* SelectedMesh = GEditor->GetSelectedObjects()->GetTop<UBlastMesh>();
	if(SelectedMesh && SelectedMesh != BlastMesh)
	{
		RemoveEditingObject(BlastMesh);
		AddEditingObject(SelectedMesh);

		SetBlastMesh(SelectedMesh);
		Viewport->ResetCamera();
	}
}

void FBlastMeshEditor::OnPostReimport(UObject* InObject, bool bSuccess)
{
	// Ignore if this is regarding a different object
	if ( InObject != BlastMesh )
	{
		return;
	}

	if ( bSuccess )
	{
		RefreshTool();
	}
}

UBlastMesh* FBlastMeshEditor::GetBlastMesh() 
{
	return BlastMesh;
}

int32 FBlastMeshEditor::GetCurrentPreviewDepth() const
{
	if (!PreviewDepthWidget.IsValid() || PreviewDepthWidget->GetSelectedDepths().Num() == 0)
	{
		return 0;
	}

	return PreviewDepthWidget->GetSelectedDepths()[0];
}

void FBlastMeshEditor::RefreshTool()
{
	ChunkEditorModels.Empty();
	ChunkHierarchy->GetRootChunks().Empty();

	for (uint32 ChunkIndex = 0; ChunkIndex < BlastMesh->GetChunkCount(); ++ChunkIndex)
	{
		FString ChunkName = FString::FormatAsNumber(ChunkIndex)
			+ TEXT(", depth: ") + FString::FormatAsNumber(BlastMesh->GetChunkDepth(ChunkIndex));
		FBlastChunkEditorModelPtr Model(new FBlastChunkEditorModel(FName(*ChunkName), false, ChunkIndex,
			BlastMesh->IsSupportChunk(ChunkIndex), BlastMesh->IsChunkStatic(ChunkIndex)));
		Model->VoronoiSites = MakeShared<TArray<FVector>>();
		Fracturer->GetVoronoiSites(FractureSettings->FractureSession, Model->ChunkIndex, *Model->VoronoiSites);
		Model->bBold = Model->VoronoiSites->Num() > 0;
		ChunkEditorModels.Add(Model);
	}

	for (int32 i = 0; i < BlastMesh->GetRootChunks().Num(); ++i)
	{
		ChunkHierarchy->GetRootChunks().Add(ChunkEditorModels[BlastMesh->GetRootChunks()[i]]);
	}
	
	FractureSettings->FractureSession->IsRootFractured = FractureSettings->FractureSession->FractureData.IsValid() && BlastMesh->GetChunkCount() > (uint32)BlastMesh->GetRootChunks().Num();

	ChunkHierarchy->Refresh();
	PreviewDepthWidget->Refresh();
	RefreshViewport();
}

void FBlastMeshEditor::RefreshViewport()
{
	Viewport->RefreshViewport();
}

FText FBlastMeshEditor::GetButtonLabel() const
{
	static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(1)
		.SetMaximumFractionalDigits(1);
	return FText::AsNumber( ExplodeFractionOfRange*ExplodeRange, &FormatOptions );
}

void FBlastMeshEditor::PreviewDepthSelectionChanged( int32 NewPreviewDepth )
{
	if (PreviewDepthWidget.IsValid())
	{
		for (auto& ChunkModel : ChunkEditorModels)
		{
			int32 Depth = BlastMesh->GetChunkDepth(ChunkModel->ChunkIndex);
			auto& chunkInfo = BlastMesh->GetChunkInfo(ChunkModel->ChunkIndex);
			ChunkModel->bVisible = (Depth == NewPreviewDepth
				|| (NewPreviewDepth == FBlastMeshEditorModule::MaxChunkDepth && (chunkInfo.childIndexStop - chunkInfo.firstChildIndex == 0)));
		}
		
		ChunkHierarchy->Refresh();
		ChunkHierarchy->UpdateSelection();
		Viewport->RefreshViewport();
	}
}

float FBlastMeshEditor::GetExplodeAmountSliderPosition() const
{
	return ExplodeFractionOfRange;
}

void FBlastMeshEditor::OnSetExplodeAmount(float NewValue)
{
	ExplodeFractionOfRange = NewValue;
	Viewport->SetExplodeAmount(ExplodeFractionOfRange*ExplodeRange);
}

void FBlastMeshEditor::UpdateChunkSelection()
{
	// Store the proxies in the ppol
	UnusedProxies.Append(SelectedChunks);

	// Clear selected chunks
	SelectedChunks.Empty(SelectedChunkIndices.Num());

	// make sure we have enough proxies to fill the selection array
	while (UnusedProxies.Num() < SelectedChunkIndices.Num())
	{
		UnusedProxies.Add(NewObject<UBlastChunkParamsProxy>());
	}

	TArray<UObject*> SelectedObjects;

	// Setup the selection array
	for (int32 ChunkIndex : SelectedChunkIndices)
	{
		UBlastChunkParamsProxy* Proxy = UnusedProxies.Pop();

		Proxy->BlastMesh = GetBlastMesh();
		Proxy->ChunkIndex = ChunkIndex;
		Proxy->ChunkCentroid = FVector(reinterpret_cast<const FVector&>(Proxy->BlastMesh->GetChunkInfo(Proxy->ChunkIndex)));
		const NvBlastChunk& ChunkInfo = Proxy->BlastMesh->GetChunkInfo(Proxy->ChunkIndex);
		Proxy->ChunkVolume = ChunkInfo.volume;
		//Proxy->BlastMeshEditorPtr = this;

		/*if (FractureSettings != NULL && FractureSettings->ChunkParameters.Num() > Proxy->ChunkIndex)
		{
		Proxy->ChunkParams = Proxy->BlastMesh->FractureSettings->ChunkParameters[Proxy->ChunkIndex];
		}*/

		SelectedChunks.Add(Proxy);
		SelectedObjects.Add(Proxy);
	}

	//if (SelectedChunks.Num() > 0)
	//{
	//	if(ChunkParametersViewTab.IsValid())
	//	{
	//		ChunkParametersViewTab->ActivateInParent(ETabActivationCause::SetDirectly);
	//	}
	//}
	//else
	//{
	//	if(BlastMeshDetailsViewTab.IsValid())
	//	{
	//		BlastMeshDetailsViewTab->ActivateInParent(ETabActivationCause::SetDirectly);
	//	}
	//}

	ChunkParametersView->SetObjects(SelectedObjects, true);

	ChunkHierarchy->UpdateSelection();
	Viewport.Get()->RedrawViewport();
}

void FBlastMeshEditor::RemoveChildren(int32 ChunkId)
{
	TSet<int32> ChunkIndices;
	if (ChunkId >= 0 && ChunkId < (int32)BlastMesh->GetChunkCount())
	{
		ChunkIndices.Add(ChunkId);
	}
	else if (ChunkId == INDEX_NONE)
	{
		ChunkIndices = SelectedChunkIndices;
	}
	else
	{
		return;
	}
	Viewport->UpdatePreviewMesh(nullptr);
	Fracturer->RemoveChildren(FractureSettings, ChunkIndices);
	OnBlastMeshReloaded();
}

void FBlastMeshEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(FractureSettings);
	Collector.AddReferencedObjects(SelectedChunks);
	Collector.AddReferencedObjects(UnusedProxies);
}

//TSharedRef<ITableRow> FBlastMeshEditor::OnGenerateRowForFractureHistory(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable)
//{
//	return SNew(STableRow<TSharedPtr <FString>>, OwnerTable)
//		[
//			SNew(STextBlock).Text(FText::FromString(*InItem))
//		];
//}

void FBlastMeshEditor::Fracture()
{
	if (BlastMesh != nullptr)
	{
		//Invalidate viewport
		Viewport->UpdatePreviewMesh(nullptr);

		Fracturer->Fracture(FractureSettings, SelectedChunkIndices);
		OnBlastMeshReloaded();
	}
}

void FBlastMeshEditor::OnBlastMeshReloaded()
{
	if (BlastMesh != nullptr && BlastMesh->Mesh != nullptr)
	{
		Viewport->UpdatePreviewMesh(BlastMesh);
		RefreshTool();

		RefreshViewport(); //Second call. First call in RefreshTool dosn't update chunks rendering properly

		//invalidate modified assets
		for (TObjectIterator<UBlastMeshComponent> Itr; Itr; ++Itr)
		{
			UBlastMeshComponent* BlastComponent = *Itr;
			if (BlastComponent->GetBlastMesh() == BlastMesh)
			{
				if (BlastComponent->GetModifiedAsset() != nullptr)
				{
					BlastComponent->MarkPackageDirty();
				}
				BlastComponent->SetModifiedAsset(nullptr);
			}
		}
	}
}

void FBlastMeshEditor::OnFractureMethodChanged()
{
	UBlastFractureSettingsVoronoi* VoronoiParams = FractureSettings->VoronoiUniformFracture;
	UBlastFractureSettingsNoise* NoiseParams = FractureSettings->UniformSlicingFracture;

	switch (FractureSettings->PreviousFractureMethod)
	{
	case EBlastFractureMethod::VoronoiClustered:
		VoronoiParams = FractureSettings->VoronoiClusteredFracture;
		break;
	case EBlastFractureMethod::VoronoiRadial:
		VoronoiParams = FractureSettings->RadialFracture;
		break;
	case EBlastFractureMethod::VoronoiInSphere:
		VoronoiParams = FractureSettings->InSphereFracture;
		break;
	case EBlastFractureMethod::VoronoiRemoveInSphere:
		VoronoiParams = FractureSettings->RemoveInSphere;
		break;
	case EBlastFractureMethod::Cut:
		NoiseParams = FractureSettings->CutFracture;
		break;
	case EBlastFractureMethod::Cutout:
		NoiseParams = FractureSettings->CutoutFracture;
		break;
	}

	FractureSettings->VoronoiUniformFracture->Setup(*VoronoiParams);
	FractureSettings->UniformSlicingFracture->Setup(*NoiseParams);
	FractureSettings->PreviousFractureMethod = FractureSettings->FractureMethod;

	switch (FractureSettings->FractureMethod)
	{
	case EBlastFractureMethod::VoronoiUniform:
		FractureSettingsCustomView->SetObject(FractureSettings->VoronoiUniformFracture);
		break;
	case EBlastFractureMethod::VoronoiClustered:
		FractureSettingsCustomView->SetObject(FractureSettings->VoronoiClusteredFracture);
		FractureSettings->VoronoiClusteredFracture->Setup(*VoronoiParams);
		break;
	case EBlastFractureMethod::VoronoiRadial:
		FractureSettingsCustomView->SetObject(FractureSettings->RadialFracture);
		FractureSettings->RadialFracture->Setup(*VoronoiParams);
		FractureSettings->RadialFracture->Origin.Activate();
		break;
	case EBlastFractureMethod::VoronoiInSphere:
		FractureSettingsCustomView->SetObject(FractureSettings->InSphereFracture);
		FractureSettings->InSphereFracture->Setup(*VoronoiParams);
		FractureSettings->InSphereFracture->Origin.Activate();
		break;
	case EBlastFractureMethod::VoronoiRemoveInSphere:
		FractureSettingsCustomView->SetObject(FractureSettings->RemoveInSphere);
		FractureSettings->RemoveInSphere->Setup(*VoronoiParams);
		FractureSettings->RemoveInSphere->Origin.Activate();
		break;
	case EBlastFractureMethod::UniformSlicing:
		FractureSettingsCustomView->SetObject(FractureSettings->UniformSlicingFracture);
		break;
	case EBlastFractureMethod::Cutout:
		FractureSettingsCustomView->SetObject(FractureSettings->CutoutFracture);
		FractureSettings->CutoutFracture->Setup(*NoiseParams);
		FractureSettings->CutoutFracture->Origin.Activate();
		break;
	case EBlastFractureMethod::Cut:
		FractureSettingsCustomView->SetObject(FractureSettings->CutFracture);
		FractureSettings->CutFracture->Setup(*NoiseParams);
		FractureSettings->CutFracture->Point.Activate();
		break;
	}

	RefreshViewport();
}

void FBlastMeshEditor::ImportRootFromStaticMesh()
{
	if (BlastMesh)
	{
		UStaticMesh* SourceStaticMesh = SSelectStaticMeshDialog::ShowWindow();
		if (SourceStaticMesh)
		{
			FTextBuilder TextBuilder;
			TextBuilder.AppendLine(LOCTEXT("BlastMeshEditor_IsReplaceSourceMesh", "Source mesh already exist. Do you want replace it with seleted static mesh?"));
			if (BlastMesh->Mesh == nullptr || FMessageDialog::Open(EAppMsgType::YesNo, TextBuilder.ToText()) == EAppReturnType::Yes)
			{
				if (FractureSettings->FractureSession.IsValid())
				{
					Fracturer->FinishFractureSession(FractureSettings->FractureSession);
					FractureSettings->Reset();
				}
				FractureSettings->FractureSession = Fracturer->StartFractureSession(BlastMesh, SourceStaticMesh, FractureSettings);
				SelectedChunkIndices.Empty();
				OnBlastMeshReloaded();
				Viewport->ResetCamera();
			}
		}
	}
}

bool FBlastMeshEditor::CanImportRootFromStaticMesh()
{
	if (!BlastMesh)
	{
		return false;
	}
	return true;
	/*
	const auto* ImportInfo = BlastMesh->SourceStaticMesh->AssetImportData;
	FDateTime CurrentSourceTimestamp = FDateTime::MinValue();
	if (ImportInfo && ImportInfo->SourceData.SourceFiles.Num() == 1)
	{
		CurrentSourceTimestamp = ImportInfo->SourceData.SourceFiles[0].Timestamp;
	}

	return (CurrentSourceTimestamp > BlastMesh->SourceSMImportTimestamp);
	*/
}

bool FBlastMeshEditor::IsFractured()
{ 
	return FractureSettings->FractureSession.IsValid() && FractureSettings->FractureSession->FractureData.IsValid() && FractureSettings->FractureSession->FractureData->chunkCount > 1; 
}

void FBlastMeshEditor::FixChunkHierarchy()
{
	if (FractureSettings->FractureSession->IsRootFractured && SFixChunkHierarchyDialog::ShowWindow(Fracturer, FractureSettings))
	{
		OnBlastMeshReloaded();
	}
}

void FBlastMeshEditor::ExportAssetToFile()
{
	SExportAssetToFileDialog::ShowWindow(Fracturer, FractureSettings);
}

void FBlastMeshEditor::FitUvCoordinates()
{
	if (SFitUvCoordinatesDialog::ShowWindow(Fracturer, FractureSettings, SelectedChunkIndices))
	{
		OnBlastMeshReloaded();
	}
}

void FBlastMeshEditor::RebuildCollisionMesh()
{
	if (SRebuildCollisionMeshDialog::ShowWindow(Fracturer, FractureSettings, SelectedChunkIndices))
	{
		//OnBlastMeshReloaded();
	}
}

#undef LOCTEXT_NAMESPACE
