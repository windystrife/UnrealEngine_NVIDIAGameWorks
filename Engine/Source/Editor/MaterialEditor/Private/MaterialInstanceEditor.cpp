// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MaterialInstanceEditor.h"
#include "Widgets/Text/STextBlock.h"
#include "EngineGlobals.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Views/SListView.h"
#include "UObject/Package.h"
#include "Editor.h"
#include "EditorStyleSet.h"
#include "MaterialEditor/DEditorTextureParameterValue.h"
#include "Materials/Material.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "ThumbnailRendering/SceneThumbnailInfoWithPrimitive.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialEditorModule.h"

#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"

#include "MaterialEditor.h"
#include "MaterialEditorActions.h"
#include "MaterialEditorUtilities.h"

#include "PropertyEditorModule.h"
#include "MaterialEditorInstanceDetailCustomization.h"

#include "EditorViewportCommands.h"
#include "Widgets/Docking/SDockTab.h"
#include "CanvasTypes.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "AdvancedPreviewSceneModule.h"


#define LOCTEXT_NAMESPACE "MaterialInstanceEditor"

DEFINE_LOG_CATEGORY_STATIC(LogMaterialInstanceEditor, Log, All);

const FName FMaterialInstanceEditor::PreviewTabId( TEXT( "MaterialInstanceEditor_Preview" ) );
const FName FMaterialInstanceEditor::PropertiesTabId( TEXT( "MaterialInstanceEditor_MaterialProperties" ) );
const FName FMaterialInstanceEditor::ParentsTabId( TEXT( "MaterialInstanceEditor_MaterialParents" ) );
const FName FMaterialInstanceEditor::PreviewSettingsTabId(TEXT("MaterialInstanceEditor_PreviewSettings"));

//////////////////////////////////////////////////////////////////////////
// SMaterialTreeWidgetItem
class SMaterialTreeWidgetItem : public SMultiColumnTableRow< TWeakObjectPtr<UMaterialInterface> >
{
public:
	SLATE_BEGIN_ARGS(SMaterialTreeWidgetItem)
		: _ParentIndex( -1 )
		, _WidgetInfoToVisualize()
		{}
		SLATE_ARGUMENT( int32, ParentIndex )
		SLATE_ARGUMENT( TWeakObjectPtr<UMaterialInterface>, WidgetInfoToVisualize )
	SLATE_END_ARGS()

	/**
	 * Construct child widgets that comprise this widget.
	 *
	 * @param InArgs  Declaration from which to construct this widget
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		this->WidgetInfo = InArgs._WidgetInfoToVisualize;
		this->ParentIndex = InArgs._ParentIndex;

		SMultiColumnTableRow< TWeakObjectPtr<UMaterialInterface> >::Construct( FSuperRowType::FArguments(), InOwnerTableView );
	}

	/** @return Widget based on the column name */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		FText Entry;
		FSlateFontInfo FontInfo = FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 9 );
		if ( ColumnName == "Parent" )
		{
			if ( ParentIndex == 0 )
			{
				Entry = NSLOCTEXT("UnrealEd", "Material", "Material");
			}
			else if ( ParentIndex != -1 )
			{
				FFormatNamedArguments Args;
				Args.Add( TEXT("Index"), ParentIndex );
				Entry = FText::Format( FText::FromString("Parent {Index}"), Args );
			}
			else
			{
				Entry = NSLOCTEXT("UnrealEd", "Current", "Current");
				FontInfo = FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 9 );
			}
		}
		else
		{
			Entry = FText::FromString( WidgetInfo.Get()->GetName() );
			if ( ParentIndex == -1 )
			{
				FontInfo = FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 9 );
			}
		}
		
		return
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2)
			[
				SNew( STextBlock )
				.Text( Entry )
				.Font( FontInfo )
			];
	}

protected:
	/** The info about the widget that we are visualizing */
	TAttribute< TWeakObjectPtr<UMaterialInterface> > WidgetInfo;

	/** The index this material has in our parents array */
	int32 ParentIndex;
};



void FMaterialInstanceEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_MaterialInstanceEditor", "Material Instance Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
	
	InTabManager->RegisterTabSpawner( PreviewTabId, FOnSpawnTab::CreateSP( this, &FMaterialInstanceEditor::SpawnTab_Preview ) )
		.SetDisplayName( LOCTEXT( "ViewportTab", "Viewport" ) )
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon( FSlateIcon( FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports" ) );
	
	InTabManager->RegisterTabSpawner( PropertiesTabId, FOnSpawnTab::CreateSP( this, &FMaterialInstanceEditor::SpawnTab_Properties ) )
		.SetDisplayName( LOCTEXT( "PropertiesTab", "Details" ) )
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon( FSlateIcon( FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details" ) );
	
	InTabManager->RegisterTabSpawner( ParentsTabId, FOnSpawnTab::CreateSP(this, &FMaterialInstanceEditor::SpawnTab_Parents) )
		.SetDisplayName( LOCTEXT("ParentsTab", "Parents") )
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon( FSlateIcon( FEditorStyle::GetStyleSetName(), "Kismet.Tabs.Palette" ) );

	InTabManager->RegisterTabSpawner(PreviewSettingsTabId, FOnSpawnTab::CreateSP(this, &FMaterialInstanceEditor::SpawnTab_PreviewSettings))
		.SetDisplayName(LOCTEXT("PreviewSceneSettingsTab", "Preview Scene Settings"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));

	OnRegisterTabSpawners().Broadcast(InTabManager);
}

void FMaterialInstanceEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner( PreviewTabId );		
	InTabManager->UnregisterTabSpawner( PropertiesTabId );	
	InTabManager->UnregisterTabSpawner( ParentsTabId );		
	InTabManager->UnregisterTabSpawner( PreviewSettingsTabId );

	OnUnregisterTabSpawners().Broadcast(InTabManager);
}


//////////////////////////////////////////////////////////////////////////
// FMaterialInstanceEditor

void FMaterialInstanceEditor::InitMaterialInstanceEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit )
{
	GEditor->RegisterForUndo( this );

	check( ObjectToEdit );
	UMaterialInstanceConstant* InstanceConstant = Cast<UMaterialInstanceConstant>(ObjectToEdit);

	bShowAllMaterialParameters = false;
	bShowMobileStats = false;

	// Construct a temp holder for our instance parameters.
	MaterialEditorInstance = NewObject<UMaterialEditorInstanceConstant>(GetTransientPackage(), NAME_None, RF_Transactional);

	bool bTempUseOldStyleMICEditorGroups = true;
	GConfig->GetBool(TEXT("/Script/UnrealEd.EditorEngine"), TEXT("UseOldStyleMICEditorGroups"), bTempUseOldStyleMICEditorGroups, GEngineIni);	
	MaterialEditorInstance->bUseOldStyleMICEditorGroups = bTempUseOldStyleMICEditorGroups;
	MaterialEditorInstance->SetSourceInstance(InstanceConstant);

	// Register our commands. This will only register them if not previously registered
	FMaterialEditorCommands::Register();

	CreateInternalWidgets();

	BindCommands();

	UpdatePreviewViewportsVisibility();

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_MaterialInstanceEditor_Layout_v3" )
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation( Orient_Vertical )
		->Split
		(
			FTabManager::NewStack() ->SetSizeCoefficient( 0.1f ) ->SetHideTabWell( true )
			->AddTab( GetToolbarTabId(), ETabState::OpenedTab )
		)
		->Split
		(
			FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal) ->SetSizeCoefficient(0.9f)
			->Split
			(
				FTabManager::NewSplitter() ->SetOrientation(Orient_Vertical) ->SetSizeCoefficient(0.27f)
				->Split
				(
					FTabManager::NewStack() ->SetSizeCoefficient(0.6f) 
					->AddTab( PropertiesTabId, ETabState::OpenedTab )
					->AddTab(PreviewSettingsTabId, ETabState::ClosedTab)
				)
				->Split
				(
					FTabManager::NewStack() ->SetSizeCoefficient(0.4f)
					->AddTab( ParentsTabId, ETabState::OpenedTab )
				)
			)
			->Split
			(
				FTabManager::NewStack() ->SetSizeCoefficient(0.73f) ->SetHideTabWell( true )
				->AddTab( PreviewTabId, ETabState::OpenedTab )
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, MaterialInstanceEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectToEdit );

	IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>( "MaterialEditor" );
	AddMenuExtender(MaterialEditorModule->GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// @todo toolkit world centric editing
	/*if( IsWorldCentricAssetEditor() )
	{
		SpawnToolkitTab(GetToolbarTabId(), FString(), EToolkitTabSpot::ToolBar);
		SpawnToolkitTab(PreviewTabId, FString(), EToolkitTabSpot::Viewport);		
		SpawnToolkitTab(PropertiesTabId, FString(), EToolkitTabSpot::Details);
		SpawnToolkitTab(ParentsTabId, FString(), EToolkitTabSpot::Details);
	}*/

	// Load editor settings.
	LoadSettings();
	
	// Set the preview mesh for the material.  This call must occur after the toolbar is initialized.
	
	if ( !SetPreviewAssetByName( *InstanceConstant->PreviewMesh.ToString() ) )
	{
		// If the preview mesh could not be found for this instance, attempt to use the preview mesh for the parent material if one exists,
		//	or use a default instead if the parent's preview mesh cannot be used

		if ( InstanceConstant->Parent == nullptr || !SetPreviewAssetByName( *InstanceConstant->Parent->PreviewMesh.ToString() ) )
		{
			USceneThumbnailInfoWithPrimitive* ThumbnailInfoWithPrim = Cast<USceneThumbnailInfoWithPrimitive>( InstanceConstant->ThumbnailInfo );

			if ( ThumbnailInfoWithPrim != nullptr )
			{
				SetPreviewAssetByName( *ThumbnailInfoWithPrim->PreviewMesh.ToString() );
			}
		}
	}

	Refresh();
}

FMaterialInstanceEditor::FMaterialInstanceEditor()
: MaterialEditorInstance(nullptr)
, MenuExtensibilityManager(new FExtensibilityManager)
, ToolBarExtensibilityManager(new FExtensibilityManager)
{
	UPackage::PreSavePackageEvent.AddRaw(this, &FMaterialInstanceEditor::PreSavePackage);
}

FMaterialInstanceEditor::~FMaterialInstanceEditor()
{
	// Broadcast that this editor is going down to all listeners
	OnMaterialEditorClosed().Broadcast();

	GEditor->UnregisterForUndo( this );

	UPackage::PreSavePackageEvent.RemoveAll(this);

	// The streaming data will be null if there were any edits
	if (MaterialEditorInstance && MaterialEditorInstance->SourceInstance && !MaterialEditorInstance->SourceInstance->HasTextureStreamingData())
	{
		UPackage* Package = MaterialEditorInstance->SourceInstance->GetOutermost();
		if (Package && Package->IsDirty() && Package != GetTransientPackage())
		{
			FMaterialEditorUtilities::BuildTextureStreamingData(MaterialEditorInstance->SourceInstance);
		}
	}

	MaterialEditorInstance = NULL;
	ParentList.Empty();

	SaveSettings();

	MaterialInstanceDetails.Reset();
	MaterialInstanceParentsList.Reset();
}

void FMaterialInstanceEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	// Serialize our custom object instance
	Collector.AddReferencedObject(MaterialEditorInstance);

	// Serialize all parent material instances that are stored in the list.
	for (int32 Index = 0; Index < ParentList.Num(); Index++)
	{
		UMaterialInterface* Parent = ParentList[Index].Get();
		Collector.AddReferencedObject(Parent);
	}
}

void FMaterialInstanceEditor::BindCommands()
{
	const FMaterialEditorCommands& Commands = FMaterialEditorCommands::Get();

	ToolkitCommands->MapAction(
		Commands.ShowAllMaterialParameters,
		FExecuteAction::CreateSP( this, &FMaterialInstanceEditor::ToggleShowAllMaterialParameters ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &FMaterialInstanceEditor::IsShowAllMaterialParametersChecked ) );

	ToolkitCommands->MapAction(
		FEditorViewportCommands::Get().ToggleRealTime,
		FExecuteAction::CreateSP( PreviewVC.ToSharedRef(), &SMaterialEditor3DPreviewViewport::OnToggleRealtime ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( PreviewVC.ToSharedRef(), &SMaterialEditor3DPreviewViewport::IsRealtime ) );

	ToolkitCommands->MapAction(
		Commands.ToggleMobileStats,
		FExecuteAction::CreateSP( this, &FMaterialInstanceEditor::ToggleMobileStats ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &FMaterialInstanceEditor::IsToggleMobileStatsChecked ) );
}

void FMaterialInstanceEditor::ToggleShowAllMaterialParameters()
{
	bShowAllMaterialParameters = !bShowAllMaterialParameters;
	UpdatePropertyWindow();
}

bool FMaterialInstanceEditor::IsShowAllMaterialParametersChecked() const
{
	return bShowAllMaterialParameters;
}

void FMaterialInstanceEditor::ToggleMobileStats()
{
	bShowMobileStats = !bShowMobileStats;
	UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(GetMaterialInterface());
	if (bShowMobileStats && MIC)
	{
		UMaterial* BaseMaterial = MIC->GetBaseMaterial();
		if (BaseMaterial)
		{
			FMaterialUpdateContext UpdateContext;
			UpdateContext.AddMaterial(BaseMaterial);
			do 
			{
				MIC->SetFeatureLevelToCompile(ERHIFeatureLevel::ES2,bShowMobileStats);
				if (MIC->bHasStaticPermutationResource)
				{
					MIC->ForceRecompileForRendering();
				}
				MIC = Cast<UMaterialInstanceConstant>(MIC->Parent);
			} while (MIC);
			BaseMaterial->SetFeatureLevelToCompile(ERHIFeatureLevel::ES2,bShowMobileStats);
			BaseMaterial->ForceRecompileForRendering();
		}
	}
	PreviewVC->RefreshViewport();
}

bool FMaterialInstanceEditor::IsToggleMobileStatsChecked() const
{
	return bShowMobileStats;
}

void FMaterialInstanceEditor::OnOpenMaterial()
{
	OpenSelectedParentEditor( GetSelectedParent() );
}

void FMaterialInstanceEditor::OnShowInContentBrowser()
{
	SyncSelectedParentToGB();
}

void FMaterialInstanceEditor::OnInheritanceListDoubleClick( TWeakObjectPtr<UMaterialInterface> InMaterialInterface )
{
	OpenSelectedParentEditor( InMaterialInterface.Get() );
}

TSharedPtr<SWidget> FMaterialInstanceEditor::OnInheritanceListRightClick()
{
	UMaterialInterface* SelectedMaterialInterface = GetSelectedParent();

	// Get all menu extenders for this context menu from the material editor module
	IMaterialEditorModule& MaterialEditor = FModuleManager::GetModuleChecked<IMaterialEditorModule>( TEXT("MaterialEditor") );
	TArray<IMaterialEditorModule::FMaterialMenuExtender_MaterialInterface> MenuExtenderDelegates = MaterialEditor.GetAllMaterialDragDropContextMenuExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute(SelectedMaterialInterface));
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bCloseAfterSelection = true;
	FMenuBuilder MenuBuilder( bCloseAfterSelection, NULL, MenuExtender );

	MenuBuilder.BeginSection("MaterialInstanceOptions", LOCTEXT("ParentOptions", "Options") );
	{
		// If this material isn't the currently open one, present the user an option to open it
		if(MaterialEditorInstance->SourceInstance!=SelectedMaterialInterface)
		{
			check(SelectedMaterialInterface != NULL )
			FText Label;

			if(SelectedMaterialInterface->IsA(UMaterial::StaticClass()))
			{
				Label = NSLOCTEXT("UnrealEd", "MaterialEditor", "Material Editor");
			}
			else
			{
				Label = NSLOCTEXT("UnrealEd", "MaterialInstanceEditor", "Material Instance Editor");
			}

			MenuBuilder.AddMenuEntry(
				Label,
				LOCTEXT("OpenMaterialTooltilp", "Opens the selected material for editing"),
				FSlateIcon(),
				FUIAction(
				FExecuteAction::CreateSP( this, &FMaterialInstanceEditor::OnOpenMaterial ),
				FCanExecuteAction()
				)
				);
		}

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ShowInCB", "Find in Content Browser..."),
			LOCTEXT("ShowInCBTooltilp", "Finds the selected material in the Content Browser"),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateSP( this, &FMaterialInstanceEditor::OnShowInContentBrowser ),
			FCanExecuteAction()
			)
			);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<ITableRow> FMaterialInstanceEditor::OnInheritanceListGenerateRow( TWeakObjectPtr<UMaterialInterface> InMaterialInterface, const TSharedRef<STableViewBase>& OwnerTable )
{
	// Find the right index to attribute to the material in the widget
	int32 TempIndex, Index = -1;
	if ( ParentList.Find( InMaterialInterface, TempIndex ) )
	{
		if ( TempIndex == 0 )
		{
			Index = 0;
		}
		else if ( TempIndex < ParentList.Num() - 1 )
		{
			Index = ParentList.Num() - 1 - TempIndex;
		}	
	}

	return
		SNew(SMaterialTreeWidgetItem, OwnerTable)
		.WidgetInfoToVisualize(InMaterialInterface)
		.ParentIndex( Index );
}

UMaterialInterface* FMaterialInstanceEditor::GetSelectedParent() const
{
	TArray< TWeakObjectPtr<UMaterialInterface> > SelectedItems = MaterialInstanceParentsList->GetSelectedItems();
	UMaterialInterface* SelectedMaterialInterface = NULL;
	if( SelectedItems.Num() > 0 )
	{
		check( SelectedItems.Last().IsValid() );
		SelectedMaterialInterface = SelectedItems.Last().Get();
	}
	else
	{
		SelectedMaterialInterface = MaterialEditorInstance->SourceInstance;
	}
	return SelectedMaterialInterface;
}

void FMaterialInstanceEditor::CreateInternalWidgets()
{
	PreviewVC = SNew(SMaterialEditor3DPreviewViewport)
		.MaterialEditor(SharedThis(this));

	PreviewUIViewport = SNew(SMaterialEditorUIPreviewViewport, GetMaterialInterface());

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	const FDetailsViewArgs DetailsViewArgs( false, false, true, FDetailsViewArgs::HideNameArea, true, this );
	MaterialInstanceDetails = PropertyEditorModule.CreateDetailView( DetailsViewArgs );
	FOnGetDetailCustomizationInstance LayoutMICDetails = FOnGetDetailCustomizationInstance::CreateStatic( 
		&FMaterialInstanceParameterDetails::MakeInstance, MaterialEditorInstance, FGetShowHiddenParameters::CreateSP(this, &FMaterialInstanceEditor::GetShowHiddenParameters) );
	MaterialInstanceDetails->RegisterInstancedCustomPropertyLayout( UMaterialEditorInstanceConstant::StaticClass(), LayoutMICDetails );

	MaterialInstanceParentsList = SNew(SListView<TWeakObjectPtr<UMaterialInterface>>)
		.SelectionMode(ESelectionMode::Single)
		.ListItemsSource( &ParentList )
		.OnGenerateRow( this, &FMaterialInstanceEditor::OnInheritanceListGenerateRow )
		.OnContextMenuOpening(this, &FMaterialInstanceEditor::OnInheritanceListRightClick)
		.OnMouseButtonDoubleClick(this, &FMaterialInstanceEditor::OnInheritanceListDoubleClick)
		.HeaderRow
		(
			SNew(SHeaderRow)
			+SHeaderRow::Column(FName(TEXT("Parent")))
			.DefaultLabel(NSLOCTEXT("MaterialInstanceEditor", "Parent", "Parent"))
			+SHeaderRow::Column(FName(TEXT("Name")))
			.DefaultLabel(NSLOCTEXT("MaterialInstanceEditor", "Name", "Name"))
		);

}


void FMaterialInstanceEditor::UpdatePreviewViewportsVisibility()
{
	UMaterial* PreviewMaterial = GetMaterialInterface()->GetBaseMaterial();
	if( PreviewMaterial->IsUIMaterial() )
	{
		PreviewVC->SetVisibility(EVisibility::Collapsed);
		PreviewUIViewport->SetVisibility(EVisibility::Visible);
	}
	else
	{
		PreviewVC->SetVisibility(EVisibility::Visible);
		PreviewUIViewport->SetVisibility(EVisibility::Collapsed);
	}
}

void FMaterialInstanceEditor::ExtendToolbar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.BeginSection("Command");
			{
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().ShowAllMaterialParameters);
				// TODO: support in material instance editor.
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().ToggleMobileStats);
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic( &Local::FillToolbar )
		);

	AddToolbarExtender(ToolbarExtender);

	AddToolbarExtender(GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>( "MaterialEditor" );
	AddToolbarExtender(MaterialEditorModule->GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

TSharedRef<SDockTab> FMaterialInstanceEditor::SpawnTab_Preview( const FSpawnTabArgs& Args )
{	
	check( Args.GetTabId().TabType == PreviewTabId );

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("ViewportTabTitle", "Viewport"))
		[
			SNew( SOverlay )
			+ SOverlay::Slot()
			[
				PreviewVC.ToSharedRef()
			]
			+ SOverlay::Slot()
			[
				PreviewUIViewport.ToSharedRef()
			]
		];

	PreviewVC->OnAddedToTab( SpawnedTab );

	AddToSpawnedToolPanels( Args.GetTabId().TabType, SpawnedTab );
	return SpawnedTab;
}


TSharedRef<SDockTab> FMaterialInstanceEditor::SpawnTab_Properties( const FSpawnTabArgs& Args )
{	
	check( Args.GetTabId().TabType == PropertiesTabId );

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("MaterialInstanceEditor.Tabs.Properties") )
		.Label(LOCTEXT("MaterialPropertiesTitle", "Details"))
		[
			SNew(SBorder)
			.Padding(4)
			[
				MaterialInstanceDetails.ToSharedRef()
			]
		];

	UpdatePropertyWindow();

	AddToSpawnedToolPanels( Args.GetTabId().TabType, SpawnedTab );
	return SpawnedTab;
}


TSharedRef<SDockTab> FMaterialInstanceEditor::SpawnTab_Parents( const FSpawnTabArgs& Args )
{	
	check( Args.GetTabId().TabType == ParentsTabId );

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("MaterialInstanceEditor.Tabs.Parents") )
		.Label( LOCTEXT("MaterialParentsTitle", "Instance Parents") )
		[
			MaterialInstanceParentsList.ToSharedRef()
		];

	RebuildInheritanceList();

	AddToSpawnedToolPanels( Args.GetTabId().TabType, SpawnedTab );
	return SpawnedTab;
}


TSharedRef<SDockTab> FMaterialInstanceEditor::SpawnTab_PreviewSettings(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PreviewSettingsTabId);

	TSharedRef<SWidget> InWidget = SNullWidget::NullWidget;
	if (PreviewVC.IsValid())
	{
		FAdvancedPreviewSceneModule& AdvancedPreviewSceneModule = FModuleManager::LoadModuleChecked<FAdvancedPreviewSceneModule>("AdvancedPreviewScene");
		InWidget = AdvancedPreviewSceneModule.CreateAdvancedPreviewSceneSettingsWidget(PreviewVC->GetPreviewScene());
	}

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("LevelEditor.Tabs.Details"))
		.Label(LOCTEXT("PreviewSceneSettingsTab", "Preview Scene Settings"))
		[
			SNew(SBox)
			[
				InWidget
			]
		];

	return SpawnedTab;
}

void FMaterialInstanceEditor::AddToSpawnedToolPanels(const FName& TabIdentifier, const TSharedRef<SDockTab>& SpawnedTab)
{
	TWeakPtr<SDockTab>* TabSpot = SpawnedToolPanels.Find(TabIdentifier);
	if (!TabSpot)
	{
		SpawnedToolPanels.Add(TabIdentifier, SpawnedTab);
	}
	else
	{
		check(!TabSpot->IsValid());
		*TabSpot = SpawnedTab;
	}
}

FName FMaterialInstanceEditor::GetToolkitFName() const
{
	return FName("MaterialInstanceEditor");
}

FText FMaterialInstanceEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Material Instance Editor");
}

FString FMaterialInstanceEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Material Instance ").ToString();
}

FLinearColor FMaterialInstanceEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.3f, 0.2f, 0.5f, 0.5f );
}

UMaterialInterface* FMaterialInstanceEditor::GetMaterialInterface() const
{
	return MaterialEditorInstance->SourceInstance;
}

void FMaterialInstanceEditor::NotifyPreChange(UProperty* PropertyThatChanged)
{
}

void FMaterialInstanceEditor::NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged)
{
	// If they changed the parent, regenerate the parent list.
	if(PropertyThatChanged->GetName()==TEXT("Parent"))
	{
		bool bSetEmptyParent = false;

		// Check to make sure they didnt set the parent to themselves.
		if(MaterialEditorInstance->Parent==MaterialEditorInstance->SourceInstance)
		{
			bSetEmptyParent = true;
		}

		if (bSetEmptyParent)
		{
			FMaterialUpdateContext UpdateContext;
			MaterialEditorInstance->Parent = NULL;

			if(MaterialEditorInstance->SourceInstance)
			{
				MaterialEditorInstance->SourceInstance->SetParentEditorOnly(NULL);
				MaterialEditorInstance->SourceInstance->PostEditChange();
			}
			UpdateContext.AddMaterialInstance(MaterialEditorInstance->SourceInstance);
		}

		RebuildInheritanceList();

		UpdatePropertyWindow();
	}
	else if(PropertyThatChanged->GetName() == TEXT("PreviewMesh"))
	{
		RefreshPreviewAsset();
	}

	//rebuild the property window to account for the possibility that the item changed was
	//a static switch

	UObject* PropertyClass = PropertyThatChanged->GetOuter();
	if(PropertyClass && PropertyClass->GetName() == TEXT("DEditorStaticSwitchParameterValue")  && MaterialEditorInstance->Parent && MaterialEditorInstance->SourceInstance )
	{
		TArray<FGuid> PreviousExpressions(MaterialEditorInstance->VisibleExpressions);
		MaterialEditorInstance->VisibleExpressions.Empty();
		FMaterialEditorUtilities::GetVisibleMaterialParameters(MaterialEditorInstance->Parent->GetMaterial(), MaterialEditorInstance->SourceInstance, MaterialEditorInstance->VisibleExpressions);
	}

	// Update the preview window when the user changes a property.
	PreviewVC->RefreshViewport();
}

void FMaterialInstanceEditor::RefreshPreviewAsset()
{
	UObject* PreviewAsset = MaterialEditorInstance->SourceInstance->PreviewMesh.TryLoad();
	if (!PreviewAsset)
	{
		// Attempt to use the parent material's preview mesh if the instance's preview mesh is invalid, and use a default
		//	sphere instead if the parent's mesh is also invalid
		UMaterialInterface* ParentMaterial = MaterialEditorInstance->SourceInstance->Parent;

		UObject* ParentPreview = ParentMaterial != nullptr ? ParentMaterial->PreviewMesh.TryLoad() : nullptr;
		PreviewAsset = ParentPreview != nullptr ? ParentPreview : GUnrealEd->GetThumbnailManager()->EditorSphere;

		USceneThumbnailInfoWithPrimitive* ThumbnailInfo = Cast<USceneThumbnailInfoWithPrimitive>(MaterialEditorInstance->SourceInstance->ThumbnailInfo);
		if (ThumbnailInfo)
		{
			ThumbnailInfo->PreviewMesh.Reset();
		}

	}
	PreviewVC->SetPreviewAsset(PreviewAsset);
}

void FMaterialInstanceEditor::PreSavePackage(UPackage* Package)
{
	// The streaming data will be null if there were any edits
	if (MaterialEditorInstance && 
		MaterialEditorInstance->SourceInstance && 
		MaterialEditorInstance->SourceInstance->GetOutermost() == Package &&
		!MaterialEditorInstance->SourceInstance->HasTextureStreamingData())
	{
		FMaterialEditorUtilities::BuildTextureStreamingData(MaterialEditorInstance->SourceInstance);
	}
}

void FMaterialInstanceEditor::RebuildInheritanceList()
{
	MaterialInstanceParentsList->ClearSelection();
	ParentList.Empty();

	// Travel up the parent chain for this material instance until we reach the root material.
	UMaterialInstance* InstanceConstant = MaterialEditorInstance->SourceInstance;

	if(InstanceConstant)
	{
		ParentList.Add(InstanceConstant);

		// Add all parents
		UMaterialInterface* Parent = InstanceConstant->Parent;
		while(Parent && Parent != InstanceConstant)
		{
			ParentList.Insert(Parent,0);

			// If the parent is a material then break.
			InstanceConstant = Cast<UMaterialInstance>(Parent);

			if(InstanceConstant)
			{
				Parent = InstanceConstant->Parent;
			}
			else
			{
				break;
			}
		}

		// MaterialInstanceParentsList->SetSelection( MaterialEditorInstance->SourceInstance );
	}

	MaterialInstanceParentsList->RequestListRefresh();
}

void FMaterialInstanceEditor::RebuildMaterialInstanceEditor()
{
	if( MaterialEditorInstance )
	{
		MaterialEditorInstance->RegenerateArrays();
		RebuildInheritanceList(); // Required b/c recompiled parent materials result in invalid weak object pointers
		UpdatePropertyWindow();
	}
}

void FMaterialInstanceEditor::DrawMessages( FViewport* Viewport, FCanvas* Canvas )
{
	Canvas->PushAbsoluteTransform(FMatrix::Identity);
	if ( MaterialEditorInstance->Parent && MaterialEditorInstance->SourceInstance )
	{
		const FMaterialResource* MaterialResource = MaterialEditorInstance->SourceInstance->GetMaterialResource(GMaxRHIFeatureLevel);
		UMaterial* BaseMaterial = MaterialEditorInstance->SourceInstance->GetMaterial();
		int32 DrawPositionY = 50;
		if ( BaseMaterial && MaterialResource )
		{
			FMaterialEditor::DrawMaterialInfoStrings( Canvas, BaseMaterial, MaterialResource, MaterialResource->GetCompileErrors(), DrawPositionY, true );
		}
		if (bShowMobileStats)
		{
			const FMaterialResource* MaterialResourceES2 = MaterialEditorInstance->SourceInstance->GetMaterialResource(ERHIFeatureLevel::ES2);
			if ( BaseMaterial && MaterialResourceES2 )
			{
				FMaterialEditor::DrawMaterialInfoStrings( Canvas, BaseMaterial, MaterialResourceES2, MaterialResourceES2->GetCompileErrors(), DrawPositionY, true );
			}
		}
		DrawSamplerWarningStrings( Canvas, DrawPositionY );
	}
	Canvas->PopTransform();
}

/**
 * Draws sampler/texture mismatch warning strings.
 * @param Canvas - The canvas on which to draw.
 * @param DrawPositionY - The Y position at which to draw. Upon return contains the Y value following the last line of text drawn.
 */
void FMaterialInstanceEditor::DrawSamplerWarningStrings(FCanvas* Canvas, int32& DrawPositionY)
{
	if ( MaterialEditorInstance->SourceInstance )
	{
		UMaterial* BaseMaterial = MaterialEditorInstance->SourceInstance->GetMaterial();
		if ( BaseMaterial )
		{
			UFont* FontToUse = GEngine->GetTinyFont();
			const int32 SpacingBetweenLines = 13;
			UEnum* SamplerTypeEnum = FindObject<UEnum>( NULL, TEXT("/Script/Engine.EMaterialSamplerType") );
			check( SamplerTypeEnum );

			const int32 GroupCount = MaterialEditorInstance->ParameterGroups.Num();
			for ( int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex )
			{
				const FEditorParameterGroup& Group = MaterialEditorInstance->ParameterGroups[ GroupIndex ];
				const int32 ParameterCount = Group.Parameters.Num();
				for ( int32 ParameterIndex = 0; ParameterIndex < ParameterCount; ++ParameterIndex )
				{
					UDEditorTextureParameterValue* TextureParameterValue = Cast<UDEditorTextureParameterValue>( Group.Parameters[ ParameterIndex ] );
					if ( TextureParameterValue && TextureParameterValue->ExpressionId.IsValid() )
					{
						UTexture* Texture = NULL;
						MaterialEditorInstance->SourceInstance->GetTextureParameterValue( TextureParameterValue->ParameterName, Texture );
						if ( Texture )
						{
							EMaterialSamplerType SamplerType = UMaterialExpressionTextureBase::GetSamplerTypeForTexture( Texture );
							UMaterialExpressionTextureSampleParameter* Expression = BaseMaterial->FindExpressionByGUID<UMaterialExpressionTextureSampleParameter>( TextureParameterValue->ExpressionId );

							if ( Expression && Expression->SamplerType != SamplerType )
							{
								FString SamplerTypeDisplayName = SamplerTypeEnum->GetDisplayNameTextByValue(Expression->SamplerType).ToString();

								Canvas->DrawShadowedString(
									5,
									DrawPositionY,
									*FString::Printf( TEXT("Warning: %s samples %s as %s."),
									*TextureParameterValue->ParameterName.ToString(),
									*Texture->GetPathName(),
									*SamplerTypeDisplayName ),
									FontToUse,
									FLinearColor(1,1,0) );
								DrawPositionY += SpacingBetweenLines;
							}
							if( Expression && ((Expression->SamplerType == (EMaterialSamplerType)TC_Normalmap || Expression->SamplerType ==  (EMaterialSamplerType)TC_Masks) && Texture->SRGB))
							{
								FString SamplerTypeDisplayName = SamplerTypeEnum->GetDisplayNameTextByValue(Expression->SamplerType).ToString();
								
								Canvas->DrawShadowedString(
									5,
									DrawPositionY,
									*FString::Printf( TEXT("Warning: %s samples texture as '%s'. SRGB should be disabled for '%s'."),
									*TextureParameterValue->ParameterName.ToString(),
									*SamplerTypeDisplayName,
									*Texture->GetPathName()),
									FontToUse,
									FLinearColor(1,1,0) );
								DrawPositionY += SpacingBetweenLines;
							}
						}
					}
				}
			}
		}
	}
}

bool FMaterialInstanceEditor::SetPreviewAsset(UObject* InAsset)
{
	if (PreviewVC.IsValid())
	{
		return PreviewVC->SetPreviewAsset(InAsset);
	}
	return false;
}

bool FMaterialInstanceEditor::SetPreviewAssetByName(const TCHAR* InAssetName)
{
	if (PreviewVC.IsValid())
	{
		return PreviewVC->SetPreviewAssetByName(InAssetName);
	}
	return false;
}

void FMaterialInstanceEditor::SetPreviewMaterial(UMaterialInterface* InMaterialInterface)
{
	if (PreviewVC.IsValid())
	{
		PreviewVC->SetPreviewMaterial(InMaterialInterface);
	}
}

void FMaterialInstanceEditor::GetShowHiddenParameters(bool& bShowHiddenParameters)
{
	bShowHiddenParameters = bShowAllMaterialParameters;
}

void FMaterialInstanceEditor::SaveSettings()
{
	GConfig->SetBool(TEXT("MaterialInstanceEditor"), TEXT("bShowGrid"), PreviewVC->IsTogglePreviewGridChecked(), GEditorPerProjectIni);
	GConfig->SetBool(TEXT("MaterialInstanceEditor"), TEXT("bDrawGrid"), PreviewVC->IsRealtime(), GEditorPerProjectIni);
	GConfig->SetInt(TEXT("MaterialInstanceEditor"), TEXT("PrimType"), PreviewVC->PreviewPrimType, GEditorPerProjectIni);
	GConfig->SetBool(TEXT("MaterialInstanceEditor"), TEXT("bWantsMobileStats"), IsToggleMobileStatsChecked(), GEditorPerProjectIni);
}

void FMaterialInstanceEditor::LoadSettings()
{
	bool bRealtime=false;
	bool bShowGrid=false;
	int32 PrimType=static_cast<EThumbnailPrimType>( TPT_Sphere );
	bool bWantsMobileStats=false;
	GConfig->GetBool(TEXT("MaterialInstanceEditor"), TEXT("bShowGrid"), bShowGrid, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("MaterialInstanceEditor"), TEXT("bDrawGrid"), bRealtime, GEditorPerProjectIni);
	GConfig->GetInt(TEXT("MaterialInstanceEditor"), TEXT("PrimType"), PrimType, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("MaterialInstanceEditor"), TEXT("bWantsMobileStats"), bWantsMobileStats, GEditorPerProjectIni);

	if (bWantsMobileStats)
	{
		ToggleMobileStats();
	}

	if(PreviewVC.IsValid())
	{
		if ( bShowGrid )
		{
			PreviewVC->TogglePreviewGrid();
		}
		if ( bRealtime )
		{
			PreviewVC->OnToggleRealtime();
		}

		PreviewVC->OnSetPreviewPrimitive( static_cast<EThumbnailPrimType>(PrimType), true);
	}
}

void FMaterialInstanceEditor::SyncSelectedParentToGB()
{
	TArray< UObject* > SelectedObjects;
	SelectedObjects.Add( GetSelectedParent() );
	GEditor->SyncBrowserToObjects(SelectedObjects);
}

void FMaterialInstanceEditor::OpenSelectedParentEditor(UMaterialInterface* InMaterialInterface)
{
	ensure(InMaterialInterface);

	// See if its a material or material instance constant.  Don't do anything if the user chose the current material instance.
	if(InMaterialInterface && MaterialEditorInstance->SourceInstance!=InMaterialInterface)
	{
		if(InMaterialInterface->IsA(UMaterial::StaticClass()))
		{
			// Show material editor
			UMaterial* Material = Cast<UMaterial>(InMaterialInterface);
			FAssetEditorManager::Get().OpenEditorForAsset(Material);
		}
		else if(InMaterialInterface->IsA(UMaterialInstance::StaticClass()))
		{
			// Show material instance editor
			UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(InMaterialInterface);
			FAssetEditorManager::Get().OpenEditorForAsset(MaterialInstance);
		}
	}
}


void FMaterialInstanceEditor::UpdatePropertyWindow()
{
	TArray<UObject*> SelectedObjects;
	SelectedObjects.Add( MaterialEditorInstance );
	MaterialInstanceDetails->SetObjects( SelectedObjects, true );
}

UObject* FMaterialInstanceEditor::GetSyncObject()
{
	if (MaterialEditorInstance)
	{
		return MaterialEditorInstance->SourceInstance;
	}
	return NULL;
}

bool FMaterialInstanceEditor::ApproveSetPreviewAsset(UObject* InAsset)
{
	// Default impl is to always accept.
	return true;
}

void FMaterialInstanceEditor::Refresh()
{
	int32 TempIndex;
	const bool bParentChanged = !ParentList.Find( MaterialEditorInstance->Parent, TempIndex );

	PreviewVC->RefreshViewport();

	if( bParentChanged )
	{
		RebuildInheritanceList();
	}
	
	UpdatePropertyWindow();
}

void FMaterialInstanceEditor::PostUndo( bool bSuccess )
{
	MaterialEditorInstance->CopyToSourceInstance();
	RefreshPreviewAsset();
	Refresh();
}

void FMaterialInstanceEditor::PostRedo( bool bSuccess )
{
	MaterialEditorInstance->CopyToSourceInstance();
	RefreshPreviewAsset();
	Refresh();
}

#undef LOCTEXT_NAMESPACE
