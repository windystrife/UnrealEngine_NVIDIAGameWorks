// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StaticMeshEditor.h"
#include "AssetData.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"
#include "EditorReimportHandler.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/StaticMesh.h"
#include "Editor.h"
#include "UnrealEdGlobals.h"
#include "StaticMeshEditorModule.h"

#include "SStaticMeshEditorViewport.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "IDetailCustomization.h"
#include "StaticMeshEditorTools.h"
#include "StaticMeshEditorActions.h"

#include "StaticMeshResources.h"
#include "BusyCursor.h"
#include "Private/GeomFitUtils.h"
#include "EditorViewportCommands.h"
#include "Private/ConvexDecompTool.h"

#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Commands/GenericCommands.h"
#include "Widgets/Input/STextComboBox.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/BodySetup.h"

#include "AdvancedPreviewSceneModule.h"

#define LOCTEXT_NAMESPACE "StaticMeshEditor"

DEFINE_LOG_CATEGORY_STATIC(LogStaticMeshEditor, Log, All);

class FStaticMeshStatusMessageContext : public FScopedSlowTask
{
public:
	explicit FStaticMeshStatusMessageContext(const FText& InMessage)
		: FScopedSlowTask(0, InMessage)
	{
		UE_LOG(LogStaticMesh, Log, TEXT("%s"), *InMessage.ToString());
		MakeDialog();
	}
};

const FName FStaticMeshEditor::ViewportTabId( TEXT( "StaticMeshEditor_Viewport" ) );
const FName FStaticMeshEditor::PropertiesTabId( TEXT( "StaticMeshEditor_Properties" ) );
const FName FStaticMeshEditor::SocketManagerTabId( TEXT( "StaticMeshEditor_SocketManager" ) );
const FName FStaticMeshEditor::CollisionTabId( TEXT( "StaticMeshEditor_Collision" ) );
const FName FStaticMeshEditor::PreviewSceneSettingsTabId(TEXT("StaticMeshEditor_PreviewScene"));

void FStaticMeshEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_StaticMeshEditor", "Static Mesh Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner( ViewportTabId, FOnSpawnTab::CreateSP(this, &FStaticMeshEditor::SpawnTab_Viewport) )
		.SetDisplayName( LOCTEXT("ViewportTab", "Viewport") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner( PropertiesTabId, FOnSpawnTab::CreateSP(this, &FStaticMeshEditor::SpawnTab_Properties) )
		.SetDisplayName( LOCTEXT("PropertiesTab", "Details") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner( SocketManagerTabId, FOnSpawnTab::CreateSP(this, &FStaticMeshEditor::SpawnTab_SocketManager) )
		.SetDisplayName( LOCTEXT("SocketManagerTab", "Socket Manager") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "StaticMeshEditor.Tabs.SocketManager"));

	InTabManager->RegisterTabSpawner( CollisionTabId, FOnSpawnTab::CreateSP(this, &FStaticMeshEditor::SpawnTab_Collision) )
		.SetDisplayName( LOCTEXT("CollisionTab", "Convex Decomposition") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "StaticMeshEditor.Tabs.ConvexDecomposition"));

	InTabManager->RegisterTabSpawner(PreviewSceneSettingsTabId, FOnSpawnTab::CreateSP(this, &FStaticMeshEditor::SpawnTab_PreviewSceneSettings))
		.SetDisplayName(LOCTEXT("PreviewSceneTab", "Preview Scene Settings"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));	
}

void FStaticMeshEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner( ViewportTabId );
	InTabManager->UnregisterTabSpawner( PropertiesTabId );
	InTabManager->UnregisterTabSpawner( SocketManagerTabId );
	InTabManager->UnregisterTabSpawner( CollisionTabId );
	InTabManager->UnregisterTabSpawner( PreviewSceneSettingsTabId );
}


FStaticMeshEditor::~FStaticMeshEditor()
{
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);

	GEditor->UnregisterForUndo( this );
	GEditor->OnObjectReimported().RemoveAll(this);
}

void FStaticMeshEditor::InitStaticMeshEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UStaticMesh* ObjectToEdit )
{
	FReimportManager::Instance()->OnPostReimport().AddRaw(this, &FStaticMeshEditor::OnPostReimport);

	// Support undo/redo
	ObjectToEdit->SetFlags( RF_Transactional );

	GEditor->RegisterForUndo( this );

	// Register our commands. This will only register them if not previously registered
	FStaticMeshEditorCommands::Register();

	// Register to be notified when an object is reimported.
	GEditor->OnObjectReimported().AddSP(this, &FStaticMeshEditor::OnObjectReimported);

	BindCommands();

	Viewport = SNew(SStaticMeshEditorViewport)
		.StaticMeshEditor(SharedThis(this))
		.ObjectToEdit(ObjectToEdit);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.NotifyHook = this;

	StaticMeshDetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );

	FOnGetDetailCustomizationInstance LayoutCustomStaticMeshProperties = FOnGetDetailCustomizationInstance::CreateSP( this, &FStaticMeshEditor::MakeStaticMeshDetails );
	StaticMeshDetailsView->RegisterInstancedCustomPropertyLayout( UStaticMesh::StaticClass(), LayoutCustomStaticMeshProperties );

	SetEditorMesh(ObjectToEdit);

	BuildSubTools();

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_StaticMeshEditor_Layout_v4" )
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
			FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal)
			->SetSizeCoefficient(0.9f)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.6f)
				->AddTab(ViewportTabId, ETabState::OpenedTab)
				->SetHideTabWell( true )
			)
			->Split
			(
				FTabManager::NewSplitter() ->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.2f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.7f)					
					->AddTab(PreviewSceneSettingsTabId, ETabState::OpenedTab)
					->AddTab(PropertiesTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->AddTab(SocketManagerTabId, ETabState::OpenedTab)
					->AddTab(CollisionTabId, ETabState::ClosedTab)
				)
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, StaticMeshEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultToolbar, bCreateDefaultStandaloneMenu, ObjectToEdit );
	
	ExtendMenu();
	ExtendToolBar();
	RegenerateMenusAndToolbars();
}

TSharedRef<IDetailCustomization> FStaticMeshEditor::MakeStaticMeshDetails()
{
	TSharedRef<FStaticMeshDetails> NewDetails = MakeShareable( new FStaticMeshDetails( *this ) );
	StaticMeshDetails = NewDetails;
	return NewDetails;
}

void FStaticMeshEditor::ExtendMenu()
{
	struct Local
	{
		static void FillEditMenu( FMenuBuilder& InMenuBuilder )
		{
			InMenuBuilder.BeginSection("Sockets", LOCTEXT("EditStaticMeshSockets", "Sockets"));
			{
				InMenuBuilder.AddMenuEntry( FGenericCommands::Get().Delete, "DeleteSocket", LOCTEXT("DeleteSocket", "Delete Socket"), LOCTEXT("DeleteSocketToolTip", "Deletes the selected socket from the mesh.") );
				InMenuBuilder.AddMenuEntry( FGenericCommands::Get().Duplicate, "DuplicateSocket", LOCTEXT("DuplicateSocket", "Duplicate Socket"), LOCTEXT("DuplicateSocketToolTip", "Duplicates the selected socket.") );
			}
			InMenuBuilder.EndSection();
		}

		static void FillMeshMenu( FMenuBuilder& InMenuBuilder )
		{
			// @todo mainframe: These menus, and indeed all menus like them, should be updated with extension points, plus expose public module
			// access to extending the menus.  They may also need to extend the command list, or be able to PUSH a command list of their own.
			// If we decide to only allow PUSHING, then nothing else should be needed (happens by extender automatically).  But if we want to
			// augment the asset editor's existing command list, then we need to think about how to expose support for that.

			InMenuBuilder.BeginSection("MeshFindSource");
			{
				InMenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().FindSource);
			}
			InMenuBuilder.EndSection();
			
			InMenuBuilder.BeginSection("MeshChange");
			{
				InMenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().ChangeMesh);
				static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.StaticMesh.EnableSaveGeneratedLODsInPackage"));
				if (CVar && CVar->GetValueOnGameThread() != 0)
				{
					InMenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().SaveGeneratedLODs);
				}
			}
			InMenuBuilder.EndSection();
		}

		static void FillCollisionMenu( FMenuBuilder& InMenuBuilder )
		{
			InMenuBuilder.BeginSection("CollisionEditCollision");
			{
				InMenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().CreateSphereCollision);
				InMenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().CreateSphylCollision);
				InMenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().CreateBoxCollision);
				InMenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().CreateDOP10X);
				InMenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().CreateDOP10Y);
				InMenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().CreateDOP10Z);
				InMenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().CreateDOP18);
				InMenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().CreateDOP26);	
				InMenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().ConvertBoxesToConvex);
				InMenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().RemoveCollision);
				InMenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete, "DeleteCollision", LOCTEXT("DeleteCollision", "Delete Selected Collision"), LOCTEXT("DeleteCollisionToolTip", "Deletes the selected Collision from the mesh."));
				InMenuBuilder.AddMenuEntry(FGenericCommands::Get().Duplicate, "DuplicateCollision", LOCTEXT("DuplicateCollision", "Duplicate Selected Collision"), LOCTEXT("DuplicateCollisionToolTip", "Duplicates the selected Collision."));
			}
			InMenuBuilder.EndSection();

			InMenuBuilder.BeginSection("CollisionAutoConvexCollision");
			{
				InMenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().CreateAutoConvexCollision);
			}
			InMenuBuilder.EndSection();

			InMenuBuilder.BeginSection("CollisionCopy");
			{
				InMenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().CopyCollisionFromSelectedMesh);
			}
			InMenuBuilder.EndSection();
		}

		static void GenerateMeshAndCollisionMenuBars( FMenuBarBuilder& InMenuBarBuilder)
		{
			InMenuBarBuilder.AddPullDownMenu(
				LOCTEXT("StaticMeshEditorMeshMenu", "Mesh"),
				LOCTEXT("StaticMeshEditorMeshMenu_ToolTip", "Opens a menu with commands for altering this mesh"),
				FNewMenuDelegate::CreateStatic(&Local::FillMeshMenu),
				"Mesh");

			InMenuBarBuilder.AddPullDownMenu(
				LOCTEXT("StaticMeshEditorCollisionMenu", "Collision"),
				LOCTEXT("StaticMeshEditorCollisionMenu_ToolTip", "Opens a menu with commands for editing this mesh's collision"),
				FNewMenuDelegate::CreateStatic(&Local::FillCollisionMenu),
				"Collision");
		}
	};
	
	TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender);

	MenuExtender->AddMenuExtension(
		"EditHistory",
		EExtensionHook::After,
		GetToolkitCommands(),
		FMenuExtensionDelegate::CreateStatic( &Local::FillEditMenu ) );

	MenuExtender->AddMenuBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FMenuBarExtensionDelegate::CreateStatic( &Local::GenerateMeshAndCollisionMenuBars )
		);
	
	AddMenuExtender(MenuExtender);
	
	IStaticMeshEditorModule* StaticMeshEditorModule = &FModuleManager::LoadModuleChecked<IStaticMeshEditorModule>( "StaticMeshEditor" );
	AddMenuExtender(StaticMeshEditorModule->GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

void FStaticMeshEditor::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( StaticMesh );
}

TSharedRef<SDockTab> FStaticMeshEditor::SpawnTab_Viewport( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == ViewportTabId );
	
	TSharedRef<SDockTab> SpawnedTab =
	 SNew(SDockTab)
		.Label( LOCTEXT("StaticMeshViewport_TabTitle", "Viewport") )
		[
			Viewport.ToSharedRef()
		];

	Viewport->SetParentTab( SpawnedTab );

	return SpawnedTab;
}

TSharedRef<SDockTab> FStaticMeshEditor::SpawnTab_Properties( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == PropertiesTabId );

	return SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("StaticMeshEditor.Tabs.Properties") )
		.Label( LOCTEXT("StaticMeshProperties_TabTitle", "Details") )
		[
			StaticMeshDetailsView.ToSharedRef()
		];
}

TSharedRef<SDockTab> FStaticMeshEditor::SpawnTab_SocketManager( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == SocketManagerTabId );

	return SNew(SDockTab)
		.Label( LOCTEXT("StaticMeshSocketManager_TabTitle", "Socket Manager") )
		[
			SocketManager.ToSharedRef()
		];
}

TSharedRef<SDockTab> FStaticMeshEditor::SpawnTab_Collision( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == CollisionTabId );

	return SNew(SDockTab)
		.Label( LOCTEXT("StaticMeshConvexDecomp_TabTitle", "Convex Decomposition") )
		[
			ConvexDecomposition.ToSharedRef()
		];
}

TSharedRef<SDockTab> FStaticMeshEditor::SpawnTab_PreviewSceneSettings(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PreviewSceneSettingsTabId);
	return SNew(SDockTab)
		.Label(LOCTEXT("StaticMeshPreviewScene_TabTitle", "Preview Scene Settings"))
		[
			AdvancedPreviewSettingsWidget.ToSharedRef()
		];
}

void FStaticMeshEditor::BindCommands()
{
	const FStaticMeshEditorCommands& Commands = FStaticMeshEditorCommands::Get();

	const TSharedRef<FUICommandList>& UICommandList = GetToolkitCommands();

	UICommandList->MapAction( FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP( this, &FStaticMeshEditor::DeleteSelected ),
		FCanExecuteAction::CreateSP(this, &FStaticMeshEditor::CanDeleteSelected));

	UICommandList->MapAction( FGenericCommands::Get().Undo, 
		FExecuteAction::CreateSP( this, &FStaticMeshEditor::UndoAction ) );

	UICommandList->MapAction( FGenericCommands::Get().Redo, 
		FExecuteAction::CreateSP( this, &FStaticMeshEditor::RedoAction ) );

	UICommandList->MapAction(
		FGenericCommands::Get().Duplicate,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::DuplicateSelected),
		FCanExecuteAction::CreateSP(this, &FStaticMeshEditor::CanDuplicateSelected));

	UICommandList->MapAction(
		FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::RequestRenameSelectedSocket),
		FCanExecuteAction::CreateSP(this, &FStaticMeshEditor::CanRenameSelected));

	UICommandList->MapAction(
		Commands.CreateDOP10X,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::GenerateKDop, KDopDir10X, (uint32)10));

	UICommandList->MapAction(
		Commands.CreateDOP10Y,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::GenerateKDop, KDopDir10Y, (uint32)10));

	UICommandList->MapAction(
		Commands.CreateDOP10Z,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::GenerateKDop, KDopDir10Z, (uint32)10));

	UICommandList->MapAction(
		Commands.CreateDOP18,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::GenerateKDop, KDopDir18, (uint32)18));

	UICommandList->MapAction(
		Commands.CreateDOP26,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::GenerateKDop, KDopDir26, (uint32)26));

	UICommandList->MapAction(
		Commands.CreateBoxCollision,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::OnCollisionBox));

	UICommandList->MapAction(
		Commands.CreateSphereCollision,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::OnCollisionSphere));

	UICommandList->MapAction(
		Commands.CreateSphylCollision,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::OnCollisionSphyl));

	UICommandList->MapAction(
		Commands.RemoveCollision,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::OnRemoveCollision),
		FCanExecuteAction::CreateSP(this, &FStaticMeshEditor::CanRemoveCollision));

	UICommandList->MapAction(
		Commands.ConvertBoxesToConvex,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::OnConvertBoxToConvexCollision));

	UICommandList->MapAction(
		Commands.CopyCollisionFromSelectedMesh,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::OnCopyCollisionFromSelectedStaticMesh),
		FCanExecuteAction::CreateSP(this, &FStaticMeshEditor::CanCopyCollisionFromSelectedStaticMesh));

	// Mesh menu
	UICommandList->MapAction(
		Commands.FindSource,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::ExecuteFindInExplorer),
		FCanExecuteAction::CreateSP(this, &FStaticMeshEditor::CanExecuteSourceCommands));

	UICommandList->MapAction(
		Commands.ChangeMesh,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::OnChangeMesh),
		FCanExecuteAction::CreateSP(this, &FStaticMeshEditor::CanChangeMesh));

	UICommandList->MapAction(
		Commands.SaveGeneratedLODs,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::OnSaveGeneratedLODs));

	// Collision Menu
	UICommandList->MapAction(
		Commands.CreateAutoConvexCollision,
		FExecuteAction::CreateSP(this, &FStaticMeshEditor::OnConvexDecomposition));
}

static TSharedRef< SWidget > GenerateCollisionMenuContent(TSharedPtr<const FUICommandList> InCommandList)
{
	FMenuBuilder MenuBuilder(true, InCommandList);

	MenuBuilder.BeginSection("ShowCollision", LOCTEXT("ShowCollision", "Show Collision"));
	{
		MenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().SetShowSimpleCollision);
		MenuBuilder.AddMenuEntry(FStaticMeshEditorCommands::Get().SetShowComplexCollision);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FStaticMeshEditor::ExtendToolBar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, FStaticMeshEditor* ThisEditor, TSharedPtr< class STextComboBox > LODLevelCombo)
		{
			ToolbarBuilder.BeginSection("Realtime");
			{
				ToolbarBuilder.AddToolBarButton(FEditorViewportCommands::Get().ToggleRealTime);
			}
			ToolbarBuilder.EndSection();
	
			ToolbarBuilder.BeginSection("Command");
			{
				ToolbarBuilder.AddToolBarButton(FStaticMeshEditorCommands::Get().SetShowSockets);
				ToolbarBuilder.AddToolBarButton(FStaticMeshEditorCommands::Get().SetShowWireframe);
				ToolbarBuilder.AddToolBarButton(FStaticMeshEditorCommands::Get().SetShowVertexColor);
				ToolbarBuilder.AddToolBarButton(FStaticMeshEditorCommands::Get().SetShowGrid);
				ToolbarBuilder.AddToolBarButton(FStaticMeshEditorCommands::Get().SetShowBounds);

				TSharedPtr<const FUICommandList> CommandList = ToolbarBuilder.GetTopCommandList();

				ToolbarBuilder.AddComboButton(
					FUIAction(),
					FOnGetContent::CreateStatic(&GenerateCollisionMenuContent, CommandList),
					LOCTEXT("Collision_Label", "Collision"),
					LOCTEXT("Collision_Tooltip", "Collision drawing options"),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "StaticMeshEditor.SetShowCollision")
				);

				ToolbarBuilder.AddToolBarButton(FStaticMeshEditorCommands::Get().SetShowPivot);
				ToolbarBuilder.AddToolBarButton(FStaticMeshEditorCommands::Get().SetShowNormals);
				ToolbarBuilder.AddToolBarButton(FStaticMeshEditorCommands::Get().SetShowTangents);
				ToolbarBuilder.AddToolBarButton(FStaticMeshEditorCommands::Get().SetShowBinormals);
				ToolbarBuilder.AddToolBarButton(FStaticMeshEditorCommands::Get().SetShowVertices);

				FOnGetContent OnGetUVMenuContent = FOnGetContent::CreateRaw(ThisEditor, &FStaticMeshEditor::GenerateUVChannelComboList);

				ToolbarBuilder.AddComboButton(
					FUIAction(), 
					OnGetUVMenuContent, 
					LOCTEXT("UVToolbarText", "UV"), 
					LOCTEXT("UVToolbarTooltip", "Toggles display of the static mesh's UVs for the specified channel."),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "StaticMeshEditor.SetDrawUVs"));
#if WITH_FLEX
				ToolbarBuilder.AddToolBarButton(FStaticMeshEditorCommands::Get().SetDrawFlexPreview);
#endif
			}

			ToolbarBuilder.EndSection();
	
			ToolbarBuilder.BeginSection("Camera");
			{
				ToolbarBuilder.AddToolBarButton(FStaticMeshEditorCommands::Get().ResetCamera);
			}
			ToolbarBuilder.EndSection();
	
			ToolbarBuilder.BeginSection("LOD");
			{
				ToolbarBuilder.AddWidget(LODLevelCombo.ToSharedRef());
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.AddToolBarButton(FStaticMeshEditorCommands::Get().SetDrawAdditionalData);
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	FStaticMeshEditorViewportClient& ViewportClient = Viewport->GetViewportClient();

	FStaticMeshEditor* ThisEditor = this;

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		Viewport->GetCommandList(),
		FToolBarExtensionDelegate::CreateStatic( &Local::FillToolbar, ThisEditor, LODLevelCombo )
		);
	
	AddToolbarExtender(ToolbarExtender);
	
	IStaticMeshEditorModule* StaticMeshEditorModule = &FModuleManager::LoadModuleChecked<IStaticMeshEditorModule>( "StaticMeshEditor" );
	AddToolbarExtender(StaticMeshEditorModule->GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

void FStaticMeshEditor::BuildSubTools()
{
	FSimpleDelegate OnSocketSelectionChanged = FSimpleDelegate::CreateSP( SharedThis(this), &FStaticMeshEditor::OnSocketSelectionChanged );

	SocketManager = ISocketManager::CreateSocketManager( SharedThis(this) , OnSocketSelectionChanged );

	SAssignNew( ConvexDecomposition, SConvexDecomposition )
		.StaticMeshEditorPtr(SharedThis(this));

	LODLevelCombo = SNew(STextComboBox)
		.OptionsSource(&LODLevels)
		.OnSelectionChanged(this, &FStaticMeshEditor::LODLevelsSelectionChanged)
		.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() );

	if(LODLevels.IsValidIndex(0))
	{
		LODLevelCombo->SetSelectedItem(LODLevels[0]);
	}

	FAdvancedPreviewSceneModule& AdvancedPreviewSceneModule = FModuleManager::LoadModuleChecked<FAdvancedPreviewSceneModule>("AdvancedPreviewScene");
	AdvancedPreviewSettingsWidget = AdvancedPreviewSceneModule.CreateAdvancedPreviewSceneSettingsWidget(Viewport->GetPreviewScene());
}

FName FStaticMeshEditor::GetToolkitFName() const
{
	return FName("StaticMeshEditor");
}

FText FStaticMeshEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "StaticMesh Editor");
}

FString FStaticMeshEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "StaticMesh ").ToString();
}

FLinearColor FStaticMeshEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.3f, 0.2f, 0.5f, 0.5f );
}

UStaticMeshComponent* FStaticMeshEditor::GetStaticMeshComponent() const
{
	return Viewport->GetStaticMeshComponent();
}

void FStaticMeshEditor::SetSelectedSocket(UStaticMeshSocket* InSelectedSocket)
{
	SocketManager->SetSelectedSocket(InSelectedSocket);
}

UStaticMeshSocket* FStaticMeshEditor::GetSelectedSocket() const
{
	check(SocketManager.IsValid());

	return SocketManager->GetSelectedSocket();
}

void FStaticMeshEditor::DuplicateSelectedSocket()
{
	SocketManager->DuplicateSelectedSocket();
}

void FStaticMeshEditor::RequestRenameSelectedSocket()
{
	SocketManager->RequestRenameSelectedSocket();
}

bool FStaticMeshEditor::IsPrimValid(const FPrimData& InPrimData) const
{
	if (StaticMesh->BodySetup)
	{
		const FKAggregateGeom* AggGeom = &StaticMesh->BodySetup->AggGeom;

		switch (InPrimData.PrimType)
		{
		case EAggCollisionShape::Sphere:
			return AggGeom->SphereElems.IsValidIndex(InPrimData.PrimIndex);
		case EAggCollisionShape::Box:
			return AggGeom->BoxElems.IsValidIndex(InPrimData.PrimIndex);
		case EAggCollisionShape::Sphyl:
			return AggGeom->SphylElems.IsValidIndex(InPrimData.PrimIndex);
		case EAggCollisionShape::Convex:
			return AggGeom->ConvexElems.IsValidIndex(InPrimData.PrimIndex);
		}
	}
	return false;
}

bool FStaticMeshEditor::HasSelectedPrims() const
{
	return (SelectedPrims.Num() > 0 ? true : false);
}

void FStaticMeshEditor::AddSelectedPrim(const FPrimData& InPrimData, bool bClearSelection)
{
	check(IsPrimValid(InPrimData));

	// Enable collision, if not already
	if( !Viewport->GetViewportClient().IsShowSimpleCollisionChecked() )
	{
		Viewport->GetViewportClient().ToggleShowSimpleCollision();
	}

	if( bClearSelection )
	{
		ClearSelectedPrims();
	}
	SelectedPrims.Add(InPrimData);	
}

void FStaticMeshEditor::RemoveSelectedPrim(const FPrimData& InPrimData)
{
	SelectedPrims.Remove(InPrimData);
}

void FStaticMeshEditor::RemoveInvalidPrims()
{
	for (int32 PrimIdx = SelectedPrims.Num() - 1; PrimIdx >= 0; PrimIdx--)
	{
		FPrimData& PrimData = SelectedPrims[PrimIdx];

		if (!IsPrimValid(PrimData))
		{
			SelectedPrims.RemoveAt(PrimIdx);
		}
	}
}

bool FStaticMeshEditor::IsSelectedPrim(const FPrimData& InPrimData) const
{
	return SelectedPrims.Contains(InPrimData);
}

void FStaticMeshEditor::ClearSelectedPrims()
{
	SelectedPrims.Empty();
}

void FStaticMeshEditor::DuplicateSelectedPrims(const FVector* InOffset)
{
	if (SelectedPrims.Num() > 0)
	{
		check(StaticMesh->BodySetup);

		FKAggregateGeom* AggGeom = &StaticMesh->BodySetup->AggGeom;

		GEditor->BeginTransaction(LOCTEXT("FStaticMeshEditor_DuplicateSelectedPrims", "Duplicate Collision"));
		StaticMesh->BodySetup->Modify();

		//Clear the cache (PIE may have created some data), create new GUID
		StaticMesh->BodySetup->InvalidatePhysicsData();

		for (int32 PrimIdx = 0; PrimIdx < SelectedPrims.Num(); PrimIdx++)
		{
			FPrimData& PrimData = SelectedPrims[PrimIdx];

			check(IsPrimValid(PrimData));
			switch (PrimData.PrimType)
			{
			case EAggCollisionShape::Sphere:
				{					
					const FKSphereElem SphereElem = AggGeom->SphereElems[PrimData.PrimIndex];
					PrimData.PrimIndex = AggGeom->SphereElems.Add(SphereElem);
				}
				break;
			case EAggCollisionShape::Box:
				{
					const FKBoxElem BoxElem = AggGeom->BoxElems[PrimData.PrimIndex];
					PrimData.PrimIndex = AggGeom->BoxElems.Add(BoxElem);
				}
				break;
			case EAggCollisionShape::Sphyl:
				{
					const FKSphylElem SphylElem = AggGeom->SphylElems[PrimData.PrimIndex];
					PrimData.PrimIndex = AggGeom->SphylElems.Add(SphylElem);
				}
				break;
			case EAggCollisionShape::Convex:
				{
					const FKConvexElem ConvexElem = AggGeom->ConvexElems[PrimData.PrimIndex];
					PrimData.PrimIndex = AggGeom->ConvexElems.Add(ConvexElem);
				}
				break;
			}

			// If specified, offset the duplicate by a specific amount
			if (InOffset)
			{
				FTransform PrimTransform = GetPrimTransform(PrimData);
				FVector PrimLocation = PrimTransform.GetLocation();
				PrimLocation += *InOffset;
				PrimTransform.SetLocation(PrimLocation);
				SetPrimTransform(PrimData, PrimTransform);
			}
		}

		// refresh collision change back to staticmesh components
		RefreshCollisionChange(*StaticMesh);

		GEditor->EndTransaction();

		// Mark staticmesh as dirty, to help make sure it gets saved.
		StaticMesh->MarkPackageDirty();

		// Update views/property windows
		Viewport->RefreshViewport();

		StaticMesh->bCustomizedCollision = true;	//mark the static mesh for collision customization
	}
}

void FStaticMeshEditor::TranslateSelectedPrims(const FVector& InDrag)
{
	check(StaticMesh->BodySetup);
	StaticMesh->BodySetup->InvalidatePhysicsData();

	for (int32 PrimIdx = 0; PrimIdx < SelectedPrims.Num(); PrimIdx++)
	{
		const FPrimData& PrimData = SelectedPrims[PrimIdx];

		FTransform PrimTransform = GetPrimTransform(PrimData);

		FVector PrimLocation = PrimTransform.GetLocation();
		PrimLocation += InDrag;
		PrimTransform.SetLocation(PrimLocation);

		SetPrimTransform(PrimData, PrimTransform);
	}

	// refresh collision change back to staticmesh components
	RefreshCollisionChange(*StaticMesh);
}

void FStaticMeshEditor::RotateSelectedPrims(const FRotator& InRot)
{
	check(StaticMesh->BodySetup);
	StaticMesh->BodySetup->InvalidatePhysicsData();

	const FQuat DeltaQ = InRot.Quaternion();

	for (int32 PrimIdx = 0; PrimIdx < SelectedPrims.Num(); PrimIdx++)
	{
		const FPrimData& PrimData = SelectedPrims[PrimIdx];

		FTransform PrimTransform = GetPrimTransform(PrimData);

		FRotator ActorRotWind, ActorRotRem;
		PrimTransform.Rotator().GetWindingAndRemainder(ActorRotWind, ActorRotRem);

		const FQuat ActorQ = ActorRotRem.Quaternion();
		FRotator NewActorRotRem = FRotator(DeltaQ * ActorQ);
		NewActorRotRem.Normalize();
		PrimTransform.SetRotation(NewActorRotRem.Quaternion());

		SetPrimTransform(PrimData, PrimTransform);
	}

	// refresh collision change back to staticmesh components
	RefreshCollisionChange(*StaticMesh);
}

void FStaticMeshEditor::ScaleSelectedPrims(const FVector& InScale)
{
	check(StaticMesh->BodySetup);
	StaticMesh->BodySetup->InvalidatePhysicsData();

	FKAggregateGeom* AggGeom = &StaticMesh->BodySetup->AggGeom;

	FVector ModifiedScale = InScale;
	if (GEditor->UsePercentageBasedScaling())
	{
		ModifiedScale = InScale * ((GEditor->GetScaleGridSize() / 100.0f) / GEditor->GetGridSize());
	}

	//Multiply in estimated size of the mesh so scaling of sphere, box and sphyl is similar speed to other scaling
	float SimplePrimitiveScaleSpeedFactor = StaticMesh->GetBounds().SphereRadius;

	for (int32 PrimIdx = 0; PrimIdx < SelectedPrims.Num(); PrimIdx++)
	{
		const FPrimData& PrimData = SelectedPrims[PrimIdx];

		check(IsPrimValid(PrimData));
		switch (PrimData.PrimType)
		{
		case EAggCollisionShape::Sphere:
			AggGeom->SphereElems[PrimData.PrimIndex].ScaleElem(SimplePrimitiveScaleSpeedFactor * ModifiedScale, MinPrimSize);
			break;
		case EAggCollisionShape::Box:
			AggGeom->BoxElems[PrimData.PrimIndex].ScaleElem(SimplePrimitiveScaleSpeedFactor * ModifiedScale, MinPrimSize);
			break;
		case EAggCollisionShape::Sphyl:
			AggGeom->SphylElems[PrimData.PrimIndex].ScaleElem(SimplePrimitiveScaleSpeedFactor * ModifiedScale, MinPrimSize);
			break;
		case EAggCollisionShape::Convex:
			AggGeom->ConvexElems[PrimData.PrimIndex].ScaleElem(ModifiedScale, MinPrimSize);
			break;
		}

		StaticMesh->bCustomizedCollision = true;	//mark the static mesh for collision customization
	}

	// refresh collision change back to staticmesh components
	RefreshCollisionChange(*StaticMesh);
}

bool FStaticMeshEditor::CalcSelectedPrimsAABB(FBox &OutBox) const
{
	check(StaticMesh->BodySetup);

	FKAggregateGeom* AggGeom = &StaticMesh->BodySetup->AggGeom;

	for (int32 PrimIdx = 0; PrimIdx < SelectedPrims.Num(); PrimIdx++)
	{
		const FPrimData& PrimData = SelectedPrims[PrimIdx];

		check(IsPrimValid(PrimData));
		switch (PrimData.PrimType)
		{
		case EAggCollisionShape::Sphere:
			OutBox += AggGeom->SphereElems[PrimData.PrimIndex].CalcAABB(FTransform::Identity, 1.f);
			break;
		case EAggCollisionShape::Box:
			OutBox += AggGeom->BoxElems[PrimData.PrimIndex].CalcAABB(FTransform::Identity, 1.f);
			break;
		case EAggCollisionShape::Sphyl:
			OutBox += AggGeom->SphylElems[PrimData.PrimIndex].CalcAABB(FTransform::Identity, 1.f);
			break;
		case EAggCollisionShape::Convex:
			OutBox += AggGeom->ConvexElems[PrimData.PrimIndex].CalcAABB(FTransform::Identity, FVector(1.f));
			break;
		}
	}
	return HasSelectedPrims();
}

bool FStaticMeshEditor::GetLastSelectedPrimTransform(FTransform& OutTransform) const
{
	if (SelectedPrims.Num() > 0)
	{
		check(StaticMesh->BodySetup);

		const FKAggregateGeom* AggGeom = &StaticMesh->BodySetup->AggGeom;

		const FPrimData& PrimData = SelectedPrims.Last();

		check(IsPrimValid(PrimData));
		switch (PrimData.PrimType)
		{
		case EAggCollisionShape::Sphere:
			OutTransform = AggGeom->SphereElems[PrimData.PrimIndex].GetTransform();
			break;
		case EAggCollisionShape::Box:
			OutTransform = AggGeom->BoxElems[PrimData.PrimIndex].GetTransform();
			break;
		case EAggCollisionShape::Sphyl:
			OutTransform = AggGeom->SphylElems[PrimData.PrimIndex].GetTransform();
			break;
		case EAggCollisionShape::Convex:
			OutTransform = AggGeom->ConvexElems[PrimData.PrimIndex].GetTransform();
			break;
		}
	}
	return HasSelectedPrims();
}

FTransform FStaticMeshEditor::GetPrimTransform(const FPrimData& InPrimData) const
{
	check(StaticMesh->BodySetup);

	const FKAggregateGeom* AggGeom = &StaticMesh->BodySetup->AggGeom;

	check(IsPrimValid(InPrimData));
	switch (InPrimData.PrimType)
	{
	case EAggCollisionShape::Sphere:
		return AggGeom->SphereElems[InPrimData.PrimIndex].GetTransform();
	case EAggCollisionShape::Box:
		return AggGeom->BoxElems[InPrimData.PrimIndex].GetTransform();
	case EAggCollisionShape::Sphyl:
		return AggGeom->SphylElems[InPrimData.PrimIndex].GetTransform();
	case EAggCollisionShape::Convex:
		return AggGeom->ConvexElems[InPrimData.PrimIndex].GetTransform();
	}
	return FTransform::Identity;
}

void FStaticMeshEditor::SetPrimTransform(const FPrimData& InPrimData, const FTransform& InPrimTransform) const
{
	check(StaticMesh->BodySetup);

	FKAggregateGeom* AggGeom = &StaticMesh->BodySetup->AggGeom;

	check(IsPrimValid(InPrimData));
	switch (InPrimData.PrimType)
	{
	case EAggCollisionShape::Sphere:
		AggGeom->SphereElems[InPrimData.PrimIndex].SetTransform(InPrimTransform);
		break;
	case EAggCollisionShape::Box:
		AggGeom->BoxElems[InPrimData.PrimIndex].SetTransform(InPrimTransform);
		break;
	case EAggCollisionShape::Sphyl:
		AggGeom->SphylElems[InPrimData.PrimIndex].SetTransform(InPrimTransform);
		break;
	case EAggCollisionShape::Convex:
		AggGeom->ConvexElems[InPrimData.PrimIndex].SetTransform(InPrimTransform);
		break;
	}

	StaticMesh->bCustomizedCollision = true;	//mark the static mesh for collision customization
}

bool FStaticMeshEditor::OverlapsExistingPrim(const FPrimData& InPrimData) const
{
	check(StaticMesh->BodySetup);

	const FKAggregateGeom* AggGeom = &StaticMesh->BodySetup->AggGeom;

	// Assume that if the transform of the prim is the same, then it overlaps (FKConvexElem doesn't have an operator==, and no shape takes tolerances into account)
	check(IsPrimValid(InPrimData));
	switch (InPrimData.PrimType)
	{
	case EAggCollisionShape::Sphere:
		{
			const FKSphereElem InSphereElem = AggGeom->SphereElems[InPrimData.PrimIndex];
			const FTransform InElemTM = InSphereElem.GetTransform();
			for (int32 i = 0; i < AggGeom->SphereElems.Num(); ++i)
			{
				if( i == InPrimData.PrimIndex )
				{
					continue;
				}

				const FKSphereElem& SphereElem = AggGeom->SphereElems[i];
				const FTransform ElemTM = SphereElem.GetTransform();
				if( InElemTM.Equals(ElemTM) )
				{
					return true;
				}
			}
		}
		break;
	case EAggCollisionShape::Box:
		{
			const FKBoxElem InBoxElem = AggGeom->BoxElems[InPrimData.PrimIndex];
			const FTransform InElemTM = InBoxElem.GetTransform();
			for (int32 i = 0; i < AggGeom->BoxElems.Num(); ++i)
			{
				if( i == InPrimData.PrimIndex )
				{
					continue;
				}

				const FKBoxElem& BoxElem = AggGeom->BoxElems[i];
				const FTransform ElemTM = BoxElem.GetTransform();
				if( InElemTM.Equals(ElemTM) )
				{
					return true;
				}
			}
		}
		break;
	case EAggCollisionShape::Sphyl:
		{
			const FKSphylElem InSphylElem = AggGeom->SphylElems[InPrimData.PrimIndex];
			const FTransform InElemTM = InSphylElem.GetTransform();
			for (int32 i = 0; i < AggGeom->SphylElems.Num(); ++i)
			{
				if( i == InPrimData.PrimIndex )
				{
					continue;
				}

				const FKSphylElem& SphylElem = AggGeom->SphylElems[i];
				const FTransform ElemTM = SphylElem.GetTransform();
				if( InElemTM.Equals(ElemTM) )
				{
					return true;
				}
			}
		}
		break;
	case EAggCollisionShape::Convex:
		{
			const FKConvexElem InConvexElem = AggGeom->ConvexElems[InPrimData.PrimIndex];
			const FTransform InElemTM = InConvexElem.GetTransform();
			for (int32 i = 0; i < AggGeom->ConvexElems.Num(); ++i)
			{
				if( i == InPrimData.PrimIndex )
				{
					continue;
				}

				const FKConvexElem& ConvexElem = AggGeom->ConvexElems[i];
				const FTransform ElemTM = ConvexElem.GetTransform();
				if( InElemTM.Equals(ElemTM) )
				{
					return true;
				}
			}
		}
		break;
	}

	return false;
}

void FStaticMeshEditor::RefreshTool()
{
	int32 NumLODs = StaticMesh->GetNumLODs();
	for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
	{
		UpdateLODStats(LODIndex);
	}

	OnSelectedLODChangedResetOnRefresh.Clear();
	bool bForceRefresh = true;
	StaticMeshDetailsView->SetObject( StaticMesh, bForceRefresh );

	RegenerateLODComboList();
	RefreshViewport();
}

void FStaticMeshEditor::RefreshViewport()
{
	Viewport->RefreshViewport();
}

void FStaticMeshEditor::RegenerateLODComboList()
{
	if( StaticMesh->RenderData )
	{
		int32 OldLOD = GetCurrentLODLevel();

		NumLODLevels = StaticMesh->RenderData->LODResources.Num();

		// Fill out the LOD level combo.
		LODLevels.Empty();
		LODLevels.Add( MakeShareable( new FString( LOCTEXT("AutoLOD", "Auto LOD").ToString() ) ) );
		LODLevels.Add( MakeShareable( new FString( LOCTEXT("BaseLOD", "Base LOD").ToString() ) ) );
		for(int32 LODLevelID = 1; LODLevelID < NumLODLevels; ++LODLevelID)
		{
			LODLevels.Add( MakeShareable( new FString( FString::Printf(*LOCTEXT("LODLevel_ID", "LOD Level %d").ToString(), LODLevelID ) ) ) );
		}

		if( LODLevelCombo.IsValid() )
		{
			LODLevelCombo->RefreshOptions();

			if(LODLevels.IsValidIndex(OldLOD) && OldLOD < LODLevels.Num() )
			{
				LODLevelCombo->SetSelectedItem(LODLevels[OldLOD]);
			}
			else
			{
				LODLevelCombo->SetSelectedItem(LODLevels[0]);
			}

		}
	}
	else
	{
		NumLODLevels = 0;
		LODLevels.Empty();
		LODLevels.Add( MakeShareable( new FString( LOCTEXT("AutoLOD", "Auto LOD").ToString() ) ) );
	}
}

TSharedRef<SWidget> FStaticMeshEditor::GenerateUVChannelComboList()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	FUIAction DrawUVsAction;

	FStaticMeshEditorViewportClient& ViewportClient = GetViewportClient();

	DrawUVsAction.ExecuteAction = FExecuteAction::CreateRaw(&ViewportClient, &FStaticMeshEditorViewportClient::SetDrawUVOverlay, false);

	// Note, the logic is inversed here.  We show the radio button as checked if no uv channels are being shown
	DrawUVsAction.GetActionCheckState = FGetActionCheckState::CreateLambda([&ViewportClient]() {return ViewportClient.IsDrawUVOverlayChecked() ? ECheckBoxState::Unchecked : ECheckBoxState::Checked; });

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ShowUVSToggle", "None"),
		LOCTEXT("ShowUVSToggle_Tooltip", "Toggles display of the static mesh's UVs."),
		FSlateIcon(),
		DrawUVsAction,
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);


	MenuBuilder.AddMenuSeparator();

	// Fill out the UV channels combo.
	int32 MaxUVChannels = FMath::Max<int32>(GetNumUVChannels(),1);
	for(int32 UVChannelID = 0; UVChannelID < MaxUVChannels; ++UVChannelID)
	{
		FUIAction MenuAction;
		MenuAction.ExecuteAction.BindSP(this, &FStaticMeshEditor::SetCurrentViewedUVChannel, UVChannelID);
		MenuAction.GetActionCheckState.BindSP(this, &FStaticMeshEditor::GetUVChannelCheckState, UVChannelID);

		MenuBuilder.AddMenuEntry(
			FText::Format(LOCTEXT("UVChannel_ID", "UV Channel {0}"), FText::AsNumber(UVChannelID)),
			FText::Format(LOCTEXT("UVChannel_ID_ToolTip", "Overlay UV Channel {0} on the viewport"), FText::AsNumber(UVChannelID)),
			FSlateIcon(),
			MenuAction,
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	}

	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}


void FStaticMeshEditor::UpdateLODStats(int32 CurrentLOD) 
{
	NumTriangles[CurrentLOD] = 0;
	NumVertices[CurrentLOD] = 0;
	NumUVChannels[CurrentLOD] = 0;
	NumLODLevels = 0;

	if( StaticMesh->RenderData )
	{
		NumLODLevels = StaticMesh->RenderData->LODResources.Num();
		if (CurrentLOD >= 0 && CurrentLOD < NumLODLevels)
		{
			FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[CurrentLOD];
			NumTriangles[CurrentLOD] = LODModel.GetNumTriangles();
			NumVertices[CurrentLOD] = LODModel.GetNumVertices();
			NumUVChannels[CurrentLOD] = LODModel.VertexBuffer.GetNumTexCoords();
		}
	}
}

void FStaticMeshEditor::ComboBoxSelectionChanged( TSharedPtr<FString> NewSelection, ESelectInfo::Type /*SelectInfo*/ )
{
	Viewport->RefreshViewport();
}

void FStaticMeshEditor::LODLevelsSelectionChanged( TSharedPtr<FString> NewSelection, ESelectInfo::Type /*SelectInfo*/ )
{
	int32 CurrentLOD = 0;
	LODLevels.Find(LODLevelCombo->GetSelectedItem(), CurrentLOD);
	if (GetStaticMeshComponent() != nullptr)
	{
		GetStaticMeshComponent()->ForcedLodModel = CurrentLOD;
	}
	UpdateLODStats( CurrentLOD > 0? CurrentLOD - 1 : 0 );
	Viewport->ForceLODLevel(CurrentLOD);
	if (OnSelectedLODChanged.IsBound())
	{
		OnSelectedLODChanged.Broadcast();
	}
	if (OnSelectedLODChangedResetOnRefresh.IsBound())
	{
		OnSelectedLODChangedResetOnRefresh.Broadcast();
	}
}

int32 FStaticMeshEditor::GetCurrentUVChannel()
{
	return FMath::Min(CurrentViewedUVChannel, GetNumUVChannels());
}

int32 FStaticMeshEditor::GetCurrentLODLevel()
{
	int32 Index = 0;
	LODLevels.Find(LODLevelCombo->GetSelectedItem(), Index);
	if (GetStaticMeshComponent() != nullptr)
	{
		if (GetStaticMeshComponent()->ForcedLodModel != Index)
		{
			LODLevelCombo->SetSelectedItem(LODLevels[GetStaticMeshComponent()->ForcedLodModel]);
			LODLevels.Find(LODLevelCombo->GetSelectedItem(), Index);
		}
	}

	return Index;
}

int32 FStaticMeshEditor::GetCurrentLODIndex()
{
	int32 Index = GetCurrentLODLevel();

	return Index == 0? 0 : Index - 1;
}

void FStaticMeshEditor::GenerateKDop(const FVector* Directions, uint32 NumDirections)
{
	TArray<FVector>	DirArray;
	for(uint32 DirectionIndex = 0;DirectionIndex < NumDirections;DirectionIndex++)
	{
		DirArray.Add(Directions[DirectionIndex]);
	}

	GEditor->BeginTransaction(LOCTEXT("FStaticMeshEditor_GenerateKDop", "Create Convex Collision"));
	const int32 PrimIndex = GenerateKDopAsSimpleCollision(StaticMesh, DirArray);
	GEditor->EndTransaction();
	if (PrimIndex != INDEX_NONE)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.Collision"), TEXT("Type"), TEXT("KDop Collision"));
		}
		const FPrimData PrimData = FPrimData(EAggCollisionShape::Convex, PrimIndex);
		ClearSelectedPrims();
		AddSelectedPrim(PrimData, true);
		// Don't 'nudge' KDop prims, as they are fitted specifically around the geometry
	}

	Viewport->RefreshViewport();
}

void FStaticMeshEditor::OnCollisionBox()
{
	GEditor->BeginTransaction(LOCTEXT("FStaticMeshEditor_OnCollisionBox", "Create Box Collision"));
	const int32 PrimIndex = GenerateBoxAsSimpleCollision(StaticMesh);
	GEditor->EndTransaction();
	if (PrimIndex != INDEX_NONE)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.Collision"), TEXT("Type"), TEXT("Box Collision"));
		}
		const FPrimData PrimData = FPrimData(EAggCollisionShape::Box, PrimIndex);
		ClearSelectedPrims();
		AddSelectedPrim(PrimData, true);
		while( OverlapsExistingPrim(PrimData) )
		{
			TranslateSelectedPrims(OverlapNudge);
		}
	}

	Viewport->RefreshViewport();
}

void FStaticMeshEditor::OnCollisionSphere()
{
	GEditor->BeginTransaction(LOCTEXT("FStaticMeshEditor_OnCollisionSphere", "Create Sphere Collision"));
	const int32 PrimIndex = GenerateSphereAsSimpleCollision(StaticMesh);
	GEditor->EndTransaction();
	if (PrimIndex != INDEX_NONE)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.Collision"), TEXT("Type"), TEXT("Sphere Collision"));
		}
		const FPrimData PrimData = FPrimData(EAggCollisionShape::Sphere, PrimIndex);
		ClearSelectedPrims();
		AddSelectedPrim(PrimData, true);
		while( OverlapsExistingPrim(PrimData) )
		{
			TranslateSelectedPrims(OverlapNudge);
		}
	}

	Viewport->RefreshViewport();
}

void FStaticMeshEditor::OnCollisionSphyl()
{
	GEditor->BeginTransaction(LOCTEXT("FStaticMeshEditor_OnCollisionSphyl", "Create Capsule Collision"));
	const int32 PrimIndex = GenerateSphylAsSimpleCollision(StaticMesh);
	GEditor->EndTransaction();
	if (PrimIndex != INDEX_NONE)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.Collision"), TEXT("Type"), TEXT("Capsule Collision"));
		}
		const FPrimData PrimData = FPrimData(EAggCollisionShape::Sphyl, PrimIndex);
		ClearSelectedPrims();
		AddSelectedPrim(PrimData, true);
		while( OverlapsExistingPrim(PrimData) )
		{
			TranslateSelectedPrims(OverlapNudge);
		}
	}

	Viewport->RefreshViewport();
}

void FStaticMeshEditor::OnRemoveCollision(void)
{
	UBodySetup* BS = StaticMesh->BodySetup;
	check(BS != NULL && BS->AggGeom.GetElementCount() > 0);

	ClearSelectedPrims();

	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	GEditor->BeginTransaction(LOCTEXT("FStaticMeshEditor_RemoveCollision", "Remove Collision"));
	StaticMesh->BodySetup->Modify();

	StaticMesh->BodySetup->RemoveSimpleCollision();

	GEditor->EndTransaction();

	// refresh collision change back to staticmesh components
	RefreshCollisionChange(*StaticMesh);

	// Mark staticmesh as dirty, to help make sure it gets saved.
	StaticMesh->MarkPackageDirty();

	// Update views/property windows
	Viewport->RefreshViewport();

	StaticMesh->bCustomizedCollision = true;	//mark the static mesh for collision customization
}

bool FStaticMeshEditor::CanRemoveCollision()
{
	UBodySetup* BS = StaticMesh->BodySetup;
	return (BS != NULL && BS->AggGeom.GetElementCount() > 0);
}

/** Util for adding vertex to an array if it is not already present. */
static void AddVertexIfNotPresent(TArray<FVector>& Vertices, const FVector& NewVertex)
{
	bool bIsPresent = false;

	for(int32 i=0; i<Vertices.Num(); i++)
	{
		float diffSqr = (NewVertex - Vertices[i]).SizeSquared();
		if(diffSqr < 0.01f * 0.01f)
		{
			bIsPresent = 1;
			break;
		}
	}

	if(!bIsPresent)
	{
		Vertices.Add(NewVertex);
	}
}

void FStaticMeshEditor::OnConvertBoxToConvexCollision()
{
	// If we have a collision model for this staticmesh, ask if we want to replace it.
	if (StaticMesh->BodySetup != NULL)
	{
		int32 ShouldReplace = FMessageDialog::Open( EAppMsgType::YesNo, LOCTEXT("ConvertBoxCollisionPrompt", "Are you sure you want to convert all box collision?") );
		if (ShouldReplace == EAppReturnType::Yes)
		{
			UBodySetup* BodySetup = StaticMesh->BodySetup;

			int32 NumBoxElems = BodySetup->AggGeom.BoxElems.Num();
			if (NumBoxElems > 0)
			{
				ClearSelectedPrims();

				// Make sure rendering is done - so we are not changing data being used by collision drawing.
				FlushRenderingCommands();

				FKConvexElem* NewConvexColl = NULL;

				//For each box elem, calculate the new convex collision representation
				//Stored in a temp array so we can undo on failure.
				TArray<FKConvexElem> TempArray;

				for (int32 i=0; i<NumBoxElems; i++)
				{
					const FKBoxElem& BoxColl = BodySetup->AggGeom.BoxElems[i];

					//Create a new convex collision element
					NewConvexColl = new(TempArray) FKConvexElem();
					NewConvexColl->ConvexFromBoxElem(BoxColl);
				}

				//Clear the cache (PIE may have created some data), create new GUID
				BodySetup->InvalidatePhysicsData();

				//Copy the new data into the static mesh
				BodySetup->AggGeom.ConvexElems.Append(TempArray);

				//Clear out what we just replaced
				BodySetup->AggGeom.BoxElems.Empty();

				BodySetup->CreatePhysicsMeshes();

				// Select the new prims
				FKAggregateGeom* AggGeom = &StaticMesh->BodySetup->AggGeom;
				for (int32 i = 0; i < NumBoxElems; ++i)
				{
					AddSelectedPrim(FPrimData(EAggCollisionShape::Convex, (AggGeom->ConvexElems.Num() - (i+1))), false);
				}

				RefreshCollisionChange(*StaticMesh);
				// Mark static mesh as dirty, to help make sure it gets saved.
				StaticMesh->MarkPackageDirty();

				// Update views/property windows
				Viewport->RefreshViewport();

				StaticMesh->bCustomizedCollision = true;	//mark the static mesh for collision customization
			}
		}
	}
}

void FStaticMeshEditor::OnCopyCollisionFromSelectedStaticMesh()
{
	UStaticMesh* SelectedMesh = GetFirstSelectedStaticMeshInContentBrowser();
	check(SelectedMesh && SelectedMesh != StaticMesh && SelectedMesh->BodySetup != NULL);

	UBodySetup* BodySetup = StaticMesh->BodySetup;

	ClearSelectedPrims();

	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	GEditor->BeginTransaction(LOCTEXT("FStaticMeshEditor_CopyCollisionFromSelectedStaticMesh", "Copy Collision from Selected Static Mesh"));
	BodySetup->Modify();

	// Copy body properties from
	BodySetup->CopyBodyPropertiesFrom(SelectedMesh->BodySetup);

	// Enable collision, if not already
	if( !Viewport->GetViewportClient().IsShowSimpleCollisionChecked() )
	{
		Viewport->GetViewportClient().ToggleShowSimpleCollision();
	}

	// Invalidate physics data and create new meshes
	BodySetup->InvalidatePhysicsData();
	BodySetup->CreatePhysicsMeshes(); 

	GEditor->EndTransaction();

	RefreshCollisionChange(*StaticMesh);
	// Mark static mesh as dirty, to help make sure it gets saved.
	StaticMesh->MarkPackageDirty();

	// Redraw level editor viewports, in case the asset's collision is visible in a viewport and the viewport isn't set to realtime.
	// Note: This could be more intelligent and only trigger a redraw if the asset is referenced in the world.
	GUnrealEd->RedrawLevelEditingViewports();

	// Update views/property windows
	Viewport->RefreshViewport();

	StaticMesh->bCustomizedCollision = true;	//mark the static mesh for collision customization
}

bool FStaticMeshEditor::CanCopyCollisionFromSelectedStaticMesh() const
{
	bool CanCopy = false;

	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);
	if(SelectedAssets.Num() == 1)
	{
		FAssetData& Asset = SelectedAssets[0];
		if(Asset.GetClass() == UStaticMesh::StaticClass())
		{
			UStaticMesh* SelectedMesh = Cast<UStaticMesh>(Asset.GetAsset());
			if(SelectedMesh && SelectedMesh != StaticMesh && SelectedMesh->BodySetup != NULL)
			{
				CanCopy = true;
			}
		}
	}

	return CanCopy;
}

UStaticMesh* FStaticMeshEditor::GetFirstSelectedStaticMeshInContentBrowser() const
{
	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);

	for(auto& Asset : SelectedAssets)
	{
		UStaticMesh* SelectedMesh = Cast<UStaticMesh>(Asset.GetAsset());
		if(SelectedMesh)
		{
			return SelectedMesh;
		}
	}

	return NULL;
}

void FStaticMeshEditor::SetEditorMesh(UStaticMesh* InStaticMesh, bool bResetCamera/*=true*/)
{
	ClearSelectedPrims();

	StaticMesh = InStaticMesh;

	//Init stat arrays.
	const int32 ArraySize = MAX_STATIC_MESH_LODS;
	NumVertices.Empty(ArraySize);
	NumVertices.AddZeroed(ArraySize);
	NumTriangles.Empty(ArraySize);
	NumTriangles.AddZeroed(ArraySize);
	NumUVChannels.Empty(ArraySize);
	NumUVChannels.AddZeroed(ArraySize);

	// Always default the LOD to 0 when setting the mesh.
	UpdateLODStats(0);

	// Fill out the LOD level combo.
	LODLevels.Empty();
	LODLevels.Add( MakeShareable( new FString( LOCTEXT("AutoLOD", "Auto LOD").ToString() ) ) );
	LODLevels.Add( MakeShareable( new FString( LOCTEXT("BaseLOD", "Base LOD").ToString() ) ) );
	for(int32 LODLevelID = 1; LODLevelID < NumLODLevels; ++LODLevelID)
	{
		LODLevels.Add( MakeShareable( new FString( FString::Printf(*LOCTEXT("LODLevel_ID", "LOD Level %d").ToString(), LODLevelID ) ) ) );

		//Update LOD stats for each level
		UpdateLODStats(LODLevelID);
	}

	if( LODLevelCombo.IsValid() )
	{
		LODLevelCombo->RefreshOptions();

		if(LODLevels.Num())
		{
			LODLevelCombo->SetSelectedItem(LODLevels[0]);
		}
	}

	// Set the details view.
	StaticMeshDetailsView->SetObject(StaticMesh);

	Viewport->UpdatePreviewMesh(StaticMesh, bResetCamera);
	Viewport->RefreshViewport();
}

void FStaticMeshEditor::OnChangeMesh()
{
	UStaticMesh* SelectedMesh = GetFirstSelectedStaticMeshInContentBrowser();
	check(SelectedMesh != NULL && SelectedMesh != StaticMesh);

	RemoveEditingObject(StaticMesh);
	AddEditingObject(SelectedMesh);

	SetEditorMesh(SelectedMesh);

	// Clear selections made on previous mesh
	ClearSelectedPrims();
	GetSelectedEdges().Empty();

	if(SocketManager.IsValid())
	{
		SocketManager->UpdateStaticMesh();
	}
}

bool FStaticMeshEditor::CanChangeMesh() const
{
	bool CanChange = false;

	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);
	if(SelectedAssets.Num() == 1)
	{
		FAssetData& Asset = SelectedAssets[0];
		if(Asset.GetClass() == UStaticMesh::StaticClass())
		{
			UStaticMesh* SelectedMesh = Cast<UStaticMesh>(Asset.GetAsset());
			if(SelectedMesh && SelectedMesh != StaticMesh)
			{
				CanChange = true;
			}
		}
	}

	return CanChange;
}

void FStaticMeshEditor::OnSaveGeneratedLODs()
{
	if (StaticMesh)
	{
		StaticMesh->GenerateLodsInPackage();

		// Update editor UI as we modified LOD groups
		auto Selected = StaticMeshDetailsView->GetSelectedObjects();
		StaticMeshDetailsView->SetObjects(Selected, true);

		// Update screen
		Viewport->RefreshViewport();
	}
}

void FStaticMeshEditor::DoDecomp(float InAccuracy, int32 InMaxHullVerts)
{
	// Check we have a selected StaticMesh
	if(StaticMesh && StaticMesh->RenderData)
	{
		FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[0];

		// Start a busy cursor so the user has feedback while waiting
		const FScopedBusyCursor BusyCursor;

		// Make vertex buffer
		int32 NumVerts = LODModel.VertexBuffer.GetNumVertices();
		TArray<FVector> Verts;
		for(int32 i=0; i<NumVerts; i++)
		{
			FVector Vert = LODModel.PositionVertexBuffer.VertexPosition(i);
			Verts.Add(Vert);
		}

		// Grab all indices
		TArray<uint32> AllIndices;
		LODModel.IndexBuffer.GetCopy(AllIndices);

		// Only copy indices that have collision enabled
		TArray<uint32> CollidingIndices;
		for(const FStaticMeshSection& Section : LODModel.Sections)
		{
			if(Section.bEnableCollision)
			{
				for (uint32 IndexIdx = Section.FirstIndex; IndexIdx < Section.FirstIndex + (Section.NumTriangles * 3); IndexIdx++)
				{
					CollidingIndices.Add(AllIndices[IndexIdx]);
				}
			}
		}

		ClearSelectedPrims();

		// Make sure rendering is done - so we are not changing data being used by collision drawing.
		FlushRenderingCommands();

		// Get the BodySetup we are going to put the collision into
		UBodySetup* bs = StaticMesh->BodySetup;
		if(bs)
		{
			bs->RemoveSimpleCollision();
		}
		else
		{
			// Otherwise, create one here.
			StaticMesh->CreateBodySetup();
			bs = StaticMesh->BodySetup;
		}

		// Run actual util to do the work (if we have some valid input)
		if(Verts.Num() >= 3 && CollidingIndices.Num() >= 3)
		{
			DecomposeMeshToHulls(bs, Verts, CollidingIndices, InAccuracy, InMaxHullVerts);		
		}

		// Enable collision, if not already
		if( !Viewport->GetViewportClient().IsShowSimpleCollisionChecked() )
		{
			Viewport->GetViewportClient().ToggleShowSimpleCollision();
		}

		// refresh collision change back to staticmesh components
		RefreshCollisionChange(*StaticMesh);

		// Mark mesh as dirty
		StaticMesh->MarkPackageDirty();

		// Update screen.
		Viewport->RefreshViewport();

		StaticMesh->bCustomizedCollision = true;	//mark the static mesh for collision customization
	}
}

TSet< int32 >& FStaticMeshEditor::GetSelectedEdges()
{
	return Viewport->GetSelectedEdges();
}

int32 FStaticMeshEditor::GetNumTriangles( int32 LODLevel ) const
{
	return NumTriangles.IsValidIndex(LODLevel) ? NumTriangles[LODLevel] : 0;
}

int32 FStaticMeshEditor::GetNumVertices( int32 LODLevel ) const
{
	return NumVertices.IsValidIndex(LODLevel) ? NumVertices[LODLevel] : 0;
}

int32 FStaticMeshEditor::GetNumUVChannels( int32 LODLevel ) const
{
	return NumUVChannels.IsValidIndex(LODLevel) ? NumUVChannels[LODLevel] : 0;
}

void FStaticMeshEditor::DeleteSelected()
{
	if (GetSelectedSocket())
	{
		DeleteSelectedSockets();
	}
	
	if (HasSelectedPrims())
	{
		DeleteSelectedPrims();
	}
}

bool FStaticMeshEditor::CanDeleteSelected() const
{
	return (GetSelectedSocket() != NULL || HasSelectedPrims());
}

void FStaticMeshEditor::DeleteSelectedSockets()
{
	check(SocketManager.IsValid());

	SocketManager->DeleteSelectedSocket();
}

void FStaticMeshEditor::DeleteSelectedPrims()
{
	if (SelectedPrims.Num() > 0)
	{
		// Sort the selected prims by PrimIndex so when we're deleting them we don't mess up other prims indicies
		struct FCompareFPrimDataPrimIndex
		{
			FORCEINLINE bool operator()(const FPrimData& A, const FPrimData& B) const
			{
				return A.PrimIndex < B.PrimIndex;
			}
		};
		SelectedPrims.Sort(FCompareFPrimDataPrimIndex());

		check(StaticMesh->BodySetup);

		FKAggregateGeom* AggGeom = &StaticMesh->BodySetup->AggGeom;

		GEditor->BeginTransaction(LOCTEXT("FStaticMeshEditor_DeleteSelectedPrims", "Delete Collision"));
		StaticMesh->BodySetup->Modify();

		for (int32 PrimIdx = SelectedPrims.Num() - 1; PrimIdx >= 0; PrimIdx--)
		{
			const FPrimData& PrimData = SelectedPrims[PrimIdx];

			check(IsPrimValid(PrimData));
			switch (PrimData.PrimType)
			{
			case EAggCollisionShape::Sphere:
				AggGeom->SphereElems.RemoveAt(PrimData.PrimIndex);
				break;
			case EAggCollisionShape::Box:
				AggGeom->BoxElems.RemoveAt(PrimData.PrimIndex);
				break;
			case EAggCollisionShape::Sphyl:
				AggGeom->SphylElems.RemoveAt(PrimData.PrimIndex);
				break;
			case EAggCollisionShape::Convex:
				AggGeom->ConvexElems.RemoveAt(PrimData.PrimIndex);
				break;
			}
		}

		GEditor->EndTransaction();

		ClearSelectedPrims();

		// Make sure rendering is done - so we are not changing data being used by collision drawing.
		FlushRenderingCommands();

		// Make sure to invalidate cooked data
		StaticMesh->BodySetup->InvalidatePhysicsData();

		// refresh collision change back to staticmesh components
		RefreshCollisionChange(*StaticMesh);

		// Mark staticmesh as dirty, to help make sure it gets saved.
		StaticMesh->MarkPackageDirty();

		// Update views/property windows
		Viewport->RefreshViewport();

		StaticMesh->bCustomizedCollision = true;	//mark the static mesh for collision customization
	}
}

void FStaticMeshEditor::DuplicateSelected()
{
	DuplicateSelectedSocket();

	const FVector InitialOffset(20.f);
	DuplicateSelectedPrims(&InitialOffset);
}

bool FStaticMeshEditor::CanDuplicateSelected() const
{
	return (GetSelectedSocket() != NULL || HasSelectedPrims());
}

bool FStaticMeshEditor::CanRenameSelected() const
{
	return (GetSelectedSocket() != NULL);
}

void FStaticMeshEditor::ExecuteFindInExplorer()
{
	if ( ensure(StaticMesh->AssetImportData) )
	{
		const FString SourceFilePath = StaticMesh->AssetImportData->GetFirstFilename();
		if ( SourceFilePath.Len() && IFileManager::Get().FileSize( *SourceFilePath ) != INDEX_NONE )
		{
			FPlatformProcess::ExploreFolder( *FPaths::GetPath(SourceFilePath) );
		}
	}
}

bool FStaticMeshEditor::CanExecuteSourceCommands() const
{
	if ( !StaticMesh->AssetImportData )
	{
		return false;
	}

	const FString& SourceFilePath = StaticMesh->AssetImportData->GetFirstFilename();

	return SourceFilePath.Len() && IFileManager::Get().FileSize(*SourceFilePath) != INDEX_NONE;
}

void FStaticMeshEditor::OnObjectReimported(UObject* InObject)
{
	// Make sure we are using the object that is being reimported, otherwise a lot of needless work could occur.
	if(StaticMesh == InObject)
	{
		//When we re-import we want to avoid moving the camera in the staticmesh editor
		bool bResetCamera = false;
		SetEditorMesh(Cast<UStaticMesh>(InObject), bResetCamera);

		if (SocketManager.IsValid())
		{
			SocketManager->UpdateStaticMesh();
		}
	}
}

EViewModeIndex FStaticMeshEditor::GetViewMode() const
{
	if (Viewport.IsValid())
	{
		const FStaticMeshEditorViewportClient& ViewportClient = Viewport->GetViewportClient();
		return ViewportClient.GetViewMode();
	}
	else
	{
		return VMI_Unknown;
	}
}

FStaticMeshEditorViewportClient& FStaticMeshEditor::GetViewportClient()
{
	return Viewport->GetViewportClient();
}

const FStaticMeshEditorViewportClient& FStaticMeshEditor::GetViewportClient() const
{
	return Viewport->GetViewportClient();
}

void FStaticMeshEditor::OnConvexDecomposition()
{
	TabManager->InvokeTab(CollisionTabId);
}

bool FStaticMeshEditor::OnRequestClose()
{
	bool bAllowClose = true;
	if (StaticMeshDetails.IsValid() && StaticMeshDetails.Pin()->IsApplyNeeded())
	{
		// find out the user wants to do with this dirty material
		EAppReturnType::Type YesNoCancelReply = FMessageDialog::Open(
			EAppMsgType::YesNoCancel,
			FText::Format( LOCTEXT("ShouldApplyLODChanges", "Would you like to apply level of detail changes to {0}?\n\n(No will lose all changes!)"), FText::FromString( StaticMesh->GetName() ) )
		);

		switch (YesNoCancelReply)
		{
		case EAppReturnType::Yes:
			StaticMeshDetails.Pin()->ApplyChanges();
			bAllowClose = true;
			break;
		case EAppReturnType::No:
			// Do nothing, changes will be abandoned.
			bAllowClose = true;
			break;
		case EAppReturnType::Cancel:
			// Don't exit.
			bAllowClose = false;
			break;
		}
	}

	return bAllowClose;
}

void FStaticMeshEditor::RegisterOnPostUndo( const FOnPostUndo& Delegate )
{
	OnPostUndo.Add( Delegate );
}

void FStaticMeshEditor::UnregisterOnPostUndo( SWidget* Widget )
{
	OnPostUndo.RemoveAll( Widget );
}

void FStaticMeshEditor::NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged )
{
	if(StaticMesh && StaticMesh->BodySetup)
	{
		StaticMesh->BodySetup->CreatePhysicsMeshes();
	}

#if WITH_FLEX
	//update preview flex mesh post UFlexAsset::ReImport
	if (PropertyThatChanged->GetOwnerClass()->IsChildOf(UFlexAsset::StaticClass()) || *PropertyThatChanged->GetName() == FName(TEXT("FlexAsset")))
	{
		Viewport->UpdateFlexPreviewComponent();
	}
#endif
}

void FStaticMeshEditor::UndoAction()
{
	GEditor->UndoTransaction();
}

void FStaticMeshEditor::RedoAction()
{
	GEditor->RedoTransaction();
}

void FStaticMeshEditor::PostUndo( bool bSuccess )
{
	RemoveInvalidPrims();
	RefreshTool();

	OnPostUndo.Broadcast();
}

void FStaticMeshEditor::PostRedo( bool bSuccess )
{
	RemoveInvalidPrims();
	RefreshTool();

	OnPostUndo.Broadcast();
}

void FStaticMeshEditor::OnSocketSelectionChanged()
{
	UStaticMeshSocket* SelectedSocket = GetSelectedSocket();
	if (SelectedSocket)
	{
		ClearSelectedPrims();
	}
	Viewport->GetViewportClient().OnSocketSelectionChanged( SelectedSocket );
}

void FStaticMeshEditor::OnPostReimport(UObject* InObject, bool bSuccess)
{
	// Ignore if this is regarding a different object
	if ( InObject != StaticMesh )
	{
		return;
	}

	if (bSuccess)
	{
		RefreshTool();
	}

#if WITH_FLEX
	Viewport->UpdateFlexPreviewComponent();
#endif
}

void FStaticMeshEditor::SetCurrentViewedUVChannel(int32 InNewUVChannel)
{
	CurrentViewedUVChannel = FMath::Clamp(InNewUVChannel, 0, GetNumUVChannels());
	GetViewportClient().SetDrawUVOverlay(true);
}

ECheckBoxState FStaticMeshEditor::GetUVChannelCheckState(int32 TestUVChannel) const
{
	return CurrentViewedUVChannel == TestUVChannel && GetViewportClient().IsDrawUVOverlayChecked() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

#undef LOCTEXT_NAMESPACE
