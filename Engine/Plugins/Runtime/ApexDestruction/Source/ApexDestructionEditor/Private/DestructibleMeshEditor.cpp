// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DestructibleMeshEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SSlider.h"
#include "EditorReimportHandler.h"
#include "PhysXIncludes.h"
#include "ApexDestructionEditorModule.h"
#include "DestructibleFractureSettings.h"
#include "Editor.h"

#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Docking/SDockTab.h"

#include "DestructibleMesh.h"
#include "Engine/Selection.h"

#define LOCTEXT_NAMESPACE "DestructibleMeshEditor"

const FName FDestructibleMeshEditor::ViewportTabId( TEXT( "DestructibleMeshEditor_Viewport" ) );
const FName FDestructibleMeshEditor::PropertiesTabId( TEXT( "DestructibleMeshEditor_Properties" ) );
const FName FDestructibleMeshEditor::FractureSettingsTabId( TEXT( "DestructibleMeshEditor_FractureSettings" ) );
const FName FDestructibleMeshEditor::ChunkParametersTabId( TEXT( "DestructibleMeshEditor_ChunkParameters" ) );

static float ExplodeRange = 5.0f;

/////////////////////////////////////////////////////////////////////////
// FDestructibleMeshEditorCommands

void FDestructibleMeshEditorCommands::RegisterCommands()
{
	UI_COMMAND( Fracture, "Fracture Mesh", "Fractures the mesh's root chunk(s) based upon the Fracture Settings.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( Refresh, "Refresh", "Refreshes the DestructibleMesh from the StaticMesh it was created from.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ImportFBXChunks, "Import FBX Chunks", "Imports a FBX as level 1 chunks.", EUserInterfaceActionType::Button, FInputChord() );
}


/////////////////////////////////////////////////////////////////////////
// FDestructibleMeshEditor

FDestructibleMeshEditor::~FDestructibleMeshEditor()
{
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);

	GEditor->OnObjectReimported().RemoveAll(this);
}

void FDestructibleMeshEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_DestructibleMeshEditor", "Destructible Mesh Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner( ViewportTabId, FOnSpawnTab::CreateSP(this, &FDestructibleMeshEditor::SpawnTab_Viewport) )
		.SetDisplayName( LOCTEXT("ViewportTab", "Viewport") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner( PropertiesTabId, FOnSpawnTab::CreateSP(this, &FDestructibleMeshEditor::SpawnTab_Properties) )
		.SetDisplayName( LOCTEXT("PropertiesTab", "Destructible Settings") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "DestructibleMeshEditor.Tabs.DestructibleSettings"));

	InTabManager->RegisterTabSpawner( FractureSettingsTabId, FOnSpawnTab::CreateSP(this, &FDestructibleMeshEditor::SpawnTab_FractureSettings) )
		.SetDisplayName( LOCTEXT("FractureSettingsTab", "Fracture Settings") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "DestructibleMeshEditor.Tabs.FractureSettings"));

	InTabManager->RegisterTabSpawner( ChunkParametersTabId, FOnSpawnTab::CreateSP(this, &FDestructibleMeshEditor::SpawnTab_ChunkParameters) )
		.SetDisplayName( LOCTEXT("ChunkParametersTab", "Chunk Parameters") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "DestructibleMeshEditor.Tabs.ChunkParameters"));
}

void FDestructibleMeshEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner( ViewportTabId );
	InTabManager->UnregisterTabSpawner( PropertiesTabId );
	InTabManager->UnregisterTabSpawner( FractureSettingsTabId );
	InTabManager->UnregisterTabSpawner( ChunkParametersTabId );
}

void FDestructibleMeshEditor::InitDestructibleMeshEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UDestructibleMesh* InDestructibleMesh )
{
	FReimportManager::Instance()->OnPostReimport().AddRaw(this, &FDestructibleMeshEditor::OnPostReimport);

	// Register our commands. This will only register them if not previously registered
	FDestructibleMeshEditorCommands::Register();

	ExplodeFractionOfRange = 0.1f / ExplodeRange;

	this->DestructibleMesh = NULL;

	Viewport = SNew(SDestructibleMeshEditorViewport)
	.DestructibleMeshEditor(SharedThis(this))
	.ObjectToEdit(InDestructibleMesh);

	FDetailsViewArgs Args;
	Args.bLockable = false;
	Args.bHideSelectionTip = true;
	Args.NotifyHook = Viewport.Get();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	DestructibleMeshDetailsView = PropertyModule.CreateDetailView(Args);
	DestructibleMeshDetailsView->SetObject( InDestructibleMesh );

	// In case this mesh has no fracture settings (it may have been imported)
#if WITH_EDITORONLY_DATA
	if (InDestructibleMesh->FractureSettings == NULL)
	{
		InDestructibleMesh->CreateFractureSettings();
#if WITH_APEX
		if (InDestructibleMesh->ApexDestructibleAsset != NULL)
		{
			InDestructibleMesh->FractureSettings->BuildRootMeshFromApexDestructibleAsset(*InDestructibleMesh->ApexDestructibleAsset);
			// Fill materials
			InDestructibleMesh->FractureSettings->Materials.Reset(InDestructibleMesh->Materials.Num());
			for (int32 MaterialIndex = 0; MaterialIndex < InDestructibleMesh->Materials.Num(); ++MaterialIndex)
			{
				InDestructibleMesh->FractureSettings->Materials.Insert(InDestructibleMesh->Materials[MaterialIndex].MaterialInterface, MaterialIndex);
			}
		}
#endif
	}
#endif	// WITH_EDITORONLY_DATA

	DestructibleFractureSettingsView = PropertyModule.CreateDetailView(Args);
	DestructibleFractureSettingsView->SetObject( InDestructibleMesh->FractureSettings );

	ChunkParametersView = PropertyModule.CreateDetailView(Args);
	ChunkParametersView->SetObject(NULL, false);

	SetEditorMesh(InDestructibleMesh);

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_DestructibleMeshEditor_Layout_v4.1" )
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->AddTab(GetToolbarTabId(), ETabState::OpenedTab) ->SetHideTabWell( true )
		)
		->Split
		(
			FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal) ->SetSizeCoefficient(0.9f)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.66f)
				->AddTab(ViewportTabId, ETabState::OpenedTab) ->SetHideTabWell( true )
			)
			->Split
			(
				FTabManager::NewSplitter() ->SetOrientation(Orient_Vertical) ->SetSizeCoefficient(0.4f)
				->Split
				(
					FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal) ->SetSizeCoefficient(0.5f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->AddTab(PropertiesTabId, ETabState::OpenedTab)
						->AddTab(ChunkParametersTabId, ETabState::OpenedTab)
						->SetForegroundTab(PropertiesTabId)
					)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.55f)
					->AddTab(FractureSettingsTabId, ETabState::OpenedTab)
				)
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, DestructibleMeshEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InDestructibleMesh );

	FDestructibleMeshEditorModule& DestructibleMeshEditorModule = FModuleManager::LoadModuleChecked<FDestructibleMeshEditorModule>( "ApexDestructionEditor" );
	AddMenuExtender(DestructibleMeshEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

TSharedRef<SDockTab> FDestructibleMeshEditor::SpawnTab_Viewport( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == ViewportTabId );

	return SNew(SDockTab)
		.Label( LOCTEXT("DestructibleMeshViewport_TabTitle", "Viewport") )
		[
			Viewport.ToSharedRef()
		];
}

TSharedRef<SDockTab> FDestructibleMeshEditor::SpawnTab_Properties( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == PropertiesTabId );

	DestructibleMeshDetailsViewTab = SNew(SDockTab)
		.Label( LOCTEXT("DestructibleMeshProperties_TabTitle", "Destructible Settings") )
		[
			DestructibleMeshDetailsView.ToSharedRef()
		];

	return DestructibleMeshDetailsViewTab.ToSharedRef();
}

TSharedRef<SDockTab> FDestructibleMeshEditor::SpawnTab_FractureSettings( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == FractureSettingsTabId );

	return SNew(SDockTab)
		.Label( LOCTEXT("DestructibleMeshFractureSettings_TabTitle", "Fracture Settings") )
		[
			DestructibleFractureSettingsView.ToSharedRef()
		];
}

TSharedRef<SDockTab> FDestructibleMeshEditor::SpawnTab_ChunkParameters( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == ChunkParametersTabId );

	ChunkParametersViewTab = SNew(SDockTab)
		.Label( LOCTEXT("DestructibleMeshChunkParameters_TabTitle", "Chunk Parameters") )
		[
			ChunkParametersView.ToSharedRef()
		];

	return ChunkParametersViewTab.ToSharedRef();
}

FName FDestructibleMeshEditor::GetToolkitFName() const
{
	return FName("DestructibleMeshEditor");
}

FText FDestructibleMeshEditor::GetBaseToolkitName() const
{
	return LOCTEXT( "AppLabel", "DestructibleMesh Editor" );
}

FString FDestructibleMeshEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "DestructibleMesh ").ToString();
}

FLinearColor FDestructibleMeshEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}

void FDestructibleMeshEditor::ExtendToolbar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, TSharedRef<SWidget> PreviewBox, TSharedRef<SWidget> ExplodeBox)
		{
			ToolbarBuilder.BeginSection("Toolbar");
			{
				ToolbarBuilder.AddToolBarButton(FDestructibleMeshEditorCommands::Get().Fracture);
				ToolbarBuilder.AddToolBarButton(FDestructibleMeshEditorCommands::Get().Refresh);
				ToolbarBuilder.AddToolBarButton(FDestructibleMeshEditorCommands::Get().ImportFBXChunks);
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
			SAssignNew(PreviewDepthCombo, SComboBox< TSharedPtr<FString> >)
			.OptionsSource(&PreviewDepths)
			.OnGenerateWidget(this, &FDestructibleMeshEditor::MakeWidgetFromString)
			.OnSelectionChanged(this, &FDestructibleMeshEditor::PreviewDepthSelectionChanged)
			.InitiallySelectedItem(PreviewDepths.Num() > 0 ? PreviewDepths[0] : NULL)
			.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() )
			.AddMetaData<FTagMetaData>(TEXT("Destructible.PreviewDepth"))
			.Content()
			[
				SNew( STextBlock )
				.Text(this, &FDestructibleMeshEditor::HandlePreviewDepthComboBoxContent)
			]
		];

	TSharedRef<SWidget> ExplodeBox = SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( FMargin(8.0f, 2.0f, 8.0f, 2.0f) )
		.HAlign( HAlign_Left )
		[
			SNew( SVerticalBox )
			.AddMetaData<FTagMetaData>(TEXT("Destructible.ExplodeAmount")) 
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
					.Value(this, &FDestructibleMeshEditor::GetExplodeAmountSliderPosition)
					.OnValueChanged(this, &FDestructibleMeshEditor::OnSetExplodeAmount)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 8.0f, 2.0f, 0.0f, 2.0f)
				[
					SNew( STextBlock )
					.Text(this, &FDestructibleMeshEditor::GetButtonLabel )
					.Font( FEditorStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) )
				]
			]
		];

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic( &Local::FillToolbar, PreviewBox, ExplodeBox )
		);

	AddToolbarExtender(ToolbarExtender);

	FDestructibleMeshEditorModule& DestructibleMeshEditorModule = FModuleManager::LoadModuleChecked<FDestructibleMeshEditorModule>( "ApexDestructionEditor" );
	AddToolbarExtender(DestructibleMeshEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders());
}

void FDestructibleMeshEditor::SetEditorMesh(UDestructibleMesh* InDestructibleMesh)
{
	DestructibleMesh = InDestructibleMesh;

	// Set the details view.
	{
		TArray<UObject*> SelectedObjects;
		SelectedObjects.Add(DestructibleMesh);
		DestructibleMeshDetailsView->SetObjects(SelectedObjects);
	}

	// Set the fracture settings view.
	{
		TArray<UObject*> SelectedObjects;
		SelectedObjects.Add(DestructibleMesh->FractureSettings);
		DestructibleFractureSettingsView->SetObjects(SelectedObjects);
	}

	// Set the Chunk Parameters view
	{
		ChunkParametersView->SetObject(NULL, true);
	}

	if (PreviewDepthCombo.IsValid())
	{
		PreviewDepthCombo->RefreshOptions();
		PreviewDepthCombo->SetSelectedItem(PreviewDepths[0]);
	}

	Viewport->UpdatePreviewMesh(DestructibleMesh);
	RefreshTool();
}

void FDestructibleMeshEditor::OnChangeMesh()
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();
	UDestructibleMesh* SelectedMesh = GEditor->GetSelectedObjects()->GetTop<UDestructibleMesh>();
	if(SelectedMesh && SelectedMesh != DestructibleMesh)
	{
		RemoveEditingObject(DestructibleMesh);
		AddEditingObject(SelectedMesh);

		SetEditorMesh(SelectedMesh);
	}
}

void FDestructibleMeshEditor::OnPostReimport(UObject* InObject, bool bSuccess)
{
	// Ignore if this is regarding a different object
	if ( InObject != DestructibleMesh )
	{
		return;
	}

	if ( bSuccess )
	{
		RefreshTool();
	}
}

UDestructibleMesh* FDestructibleMeshEditor::GetDestructibleMesh() 
{
	return DestructibleMesh;
}

int32 FDestructibleMeshEditor::GetCurrentPreviewDepth() const
{
	if (!PreviewDepthCombo.IsValid())
	{
		return 0;
	}

	const int32 PreviewDepth = PreviewDepths.Find(PreviewDepthCombo->GetSelectedItem());

	return PreviewDepth < 0 ? 0 : PreviewDepth;
}

void FDestructibleMeshEditor::SetCurrentPreviewDepth(uint32 InPreviewDepthDepth)
{
	if (PreviewDepths.Num() > 0)
	{
		const uint32 NewPreviewDepth = FMath::Clamp(InPreviewDepthDepth, (uint32)0, (uint32)PreviewDepths.Num()-1);
		Viewport->SetPreviewDepth(NewPreviewDepth);
		PreviewDepthCombo->SetSelectedItem(PreviewDepths[NewPreviewDepth]);
	}
}

void FDestructibleMeshEditor::RefreshTool()
{
	RegeneratePreviewDepthComboList();
	RefreshViewport();
}

void FDestructibleMeshEditor::RefreshViewport()
{
	Viewport->RefreshViewport();
}

FText FDestructibleMeshEditor::HandlePreviewDepthComboBoxContent() const
{
	const int32 CurrentPreviewDepth = GetCurrentPreviewDepth();

	if (CurrentPreviewDepth < PreviewDepths.Num())
	{
		return FText::FromString(*PreviewDepths[CurrentPreviewDepth]);
	}

	return LOCTEXT("Invalid", "Invalid");
}

void FDestructibleMeshEditor::RegeneratePreviewDepthComboList()
{
#if WITH_APEX
	if (DestructibleMesh->ApexDestructibleAsset != NULL)
	{
		const uint32 DepthCount = DestructibleMesh->ApexDestructibleAsset->getDepthCount();
		if (DepthCount > 0)
		{
			const int32 OldPreviewDepth = GetCurrentPreviewDepth();

			// Fill out the preview depth combo.
			PreviewDepths.Empty();
			for (uint32 PreviewDepth = 0; PreviewDepth < DepthCount; ++PreviewDepth)
			{
				PreviewDepths.Add( MakeShareable( new FString( FText::Format( LOCTEXT("PreviewDepth_ID", "Preview Depth {0}"), FText::AsNumber( PreviewDepth ) ).ToString() ) ) );
			}

			if (PreviewDepthCombo.IsValid())
			{
				PreviewDepthCombo->RefreshOptions();

				if (OldPreviewDepth < PreviewDepths.Num())
				{
					PreviewDepthCombo->SetSelectedItem(PreviewDepths[OldPreviewDepth]);
				}
				else
				{
					PreviewDepthCombo->SetSelectedItem(PreviewDepths[0]);
				}
			}
		}
	}
#endif // WITH_APEX
}

TSharedRef<SWidget> FDestructibleMeshEditor::MakeWidgetFromString( TSharedPtr<FString> InItem )
{
	return SNew(STextBlock) .Text( FText::FromString(*InItem) );
}

FText FDestructibleMeshEditor::GetButtonLabel() const
{
	static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(1)
		.SetMaximumFractionalDigits(1);
	return FText::AsNumber( ExplodeFractionOfRange*ExplodeRange, &FormatOptions );
}

void FDestructibleMeshEditor::ComboBoxSelectionChanged( TSharedPtr<FString> NewSelection )
{
	Viewport->RefreshViewport();
}

void FDestructibleMeshEditor::PreviewDepthSelectionChanged( TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo  )
{
	const int32 CurrentPreviewDepth = GetCurrentPreviewDepth();

	Viewport->SetPreviewDepth(CurrentPreviewDepth);
}

float FDestructibleMeshEditor::GetExplodeAmountSliderPosition() const
{
	return ExplodeFractionOfRange;
}

void FDestructibleMeshEditor::OnSetExplodeAmount(float NewValue)
{
	ExplodeFractionOfRange = NewValue;
	Viewport->SetExplodeAmount(ExplodeFractionOfRange*ExplodeRange);
}

void FDestructibleMeshEditor::SetSelectedChunks( const TArray<UObject*>& SelectedChunks )
{
	if (SelectedChunks.Num() > 0)
	{
		if(ChunkParametersViewTab.IsValid())
		{
			ChunkParametersViewTab->ActivateInParent(ETabActivationCause::SetDirectly);
		}
	}
	else
	{
		if(DestructibleMeshDetailsViewTab.IsValid())
		{
			DestructibleMeshDetailsViewTab->ActivateInParent(ETabActivationCause::SetDirectly);
		}
	}

	ChunkParametersView->SetObjects(SelectedChunks, true);
}

#undef LOCTEXT_NAMESPACE
