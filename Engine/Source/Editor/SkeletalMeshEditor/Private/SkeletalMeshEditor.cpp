// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshEditor.h"
#include "Modules/ModuleManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorReimportHandler.h"
#include "AssetData.h"
#include "EdGraph/EdGraphSchema.h"
#include "Editor/EditorEngine.h"
#include "EngineGlobals.h"
#include "ISkeletalMeshEditorModule.h"
#include "IPersonaToolkit.h"
#include "PersonaModule.h"
#include "SkeletalMeshEditorMode.h"
#include "IPersonaPreviewScene.h"
#include "SkeletalMeshEditorCommands.h"
#include "IDetailsView.h"
#include "ISkeletonTree.h"
#include "ISkeletonEditorModule.h"
#include "IAssetFamily.h"
#include "PersonaCommonCommands.h"
#include "EngineUtils.h"

#include "Animation/DebugSkelMeshComponent.h"
#include "ClothingAsset.h"
#include "SCreateClothingSettingsPanel.h"
#include "ClothingSystemEditorInterfaceModule.h"
#include "Classes/Preferences/PersonaOptions.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Application/SlateApplication.h"
#include "EditorViewportClient.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Algo/Transform.h"
#include "ISkeletonTreeItem.h"

const FName SkeletalMeshEditorAppIdentifier = FName(TEXT("SkeletalMeshEditorApp"));

const FName SkeletalMeshEditorModes::SkeletalMeshEditorMode(TEXT("SkeletalMeshEditorMode"));

const FName SkeletalMeshEditorTabs::DetailsTab(TEXT("DetailsTab"));
const FName SkeletalMeshEditorTabs::SkeletonTreeTab(TEXT("SkeletonTreeView"));
const FName SkeletalMeshEditorTabs::AssetDetailsTab(TEXT("AnimAssetPropertiesTab"));
const FName SkeletalMeshEditorTabs::ViewportTab(TEXT("Viewport"));
const FName SkeletalMeshEditorTabs::AdvancedPreviewTab(TEXT("AdvancedPreviewTab"));
const FName SkeletalMeshEditorTabs::MorphTargetsTab("MorphTargetsTab");
const FName SkeletalMeshEditorTabs::AnimationMappingTab("AnimationMappingWindow");

DEFINE_LOG_CATEGORY(LogSkeletalMeshEditor);

#define LOCTEXT_NAMESPACE "SkeletalMeshEditor"

FSkeletalMeshEditor::FSkeletalMeshEditor()
{
	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->RegisterForUndo(this);
	}
}

FSkeletalMeshEditor::~FSkeletalMeshEditor()
{
	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->UnregisterForUndo(this);
	}
}

void FSkeletalMeshEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_SkeletalMeshEditor", "Skeletal Mesh Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

void FSkeletalMeshEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

void FSkeletalMeshEditor::InitSkeletalMeshEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, USkeletalMesh* InSkeletalMesh)
{
	SkeletalMesh = InSkeletalMesh;

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	PersonaToolkit = PersonaModule.CreatePersonaToolkit(InSkeletalMesh);

	PersonaToolkit->GetPreviewScene()->SetDefaultAnimationMode(EPreviewSceneDefaultAnimationMode::ReferencePose);

	TSharedRef<IAssetFamily> AssetFamily = PersonaModule.CreatePersonaAssetFamily(InSkeletalMesh);
	AssetFamily->RecordAssetOpened(FAssetData(InSkeletalMesh));

	TSharedPtr<IPersonaPreviewScene> PreviewScene = PersonaToolkit->GetPreviewScene();

	FSkeletonTreeArgs SkeletonTreeArgs;
	SkeletonTreeArgs.OnSelectionChanged = FOnSkeletonTreeSelectionChanged::CreateSP(this, &FSkeletalMeshEditor::HandleSelectionChanged);
	SkeletonTreeArgs.PreviewScene = PreviewScene;

	ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::GetModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	SkeletonTree = SkeletonEditorModule.CreateSkeletonTree(PersonaToolkit->GetSkeleton(), SkeletonTreeArgs);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	const TSharedRef<FTabManager::FLayout> DummyLayout = FTabManager::NewLayout("NullLayout")->AddArea(FTabManager::NewPrimaryArea());
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, SkeletalMeshEditorAppIdentifier, DummyLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InSkeletalMesh);

	BindCommands();

	AddApplicationMode(
		SkeletalMeshEditorModes::SkeletalMeshEditorMode,
		MakeShareable(new FSkeletalMeshEditorMode(SharedThis(this), SkeletonTree.ToSharedRef())));

	SetCurrentMode(SkeletalMeshEditorModes::SkeletalMeshEditorMode);

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// Set up mesh click selection
	PreviewScene->RegisterOnMeshClick(FOnMeshClick::CreateSP(this, &FSkeletalMeshEditor::HandleMeshClick));
	PreviewScene->SetAllowMeshHitProxies(GetDefault<UPersonaOptions>()->bAllowMeshSectionSelection);
}

FName FSkeletalMeshEditor::GetToolkitFName() const
{
	return FName("SkeletalMeshEditor");
}

FText FSkeletalMeshEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "SkeletalMeshEditor");
}

FString FSkeletalMeshEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "SkeletalMeshEditor ").ToString();
}

FLinearColor FSkeletalMeshEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

void FSkeletalMeshEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(SkeletalMesh);
}

void FSkeletalMeshEditor::BindCommands()
{
	FSkeletalMeshEditorCommands::Register();

	ToolkitCommands->MapAction(FSkeletalMeshEditorCommands::Get().ReimportMesh,
		FExecuteAction::CreateSP(this, &FSkeletalMeshEditor::HandleReimportMesh));

	ToolkitCommands->MapAction(FSkeletalMeshEditorCommands::Get().MeshSectionSelection,
		FExecuteAction::CreateSP(this, &FSkeletalMeshEditor::ToggleMeshSectionSelection),
		FCanExecuteAction(), 
		FIsActionChecked::CreateSP(this, &FSkeletalMeshEditor::IsMeshSectionSelectionChecked));

	ToolkitCommands->MapAction(FPersonaCommonCommands::Get().TogglePlay,
		FExecuteAction::CreateRaw(&GetPersonaToolkit()->GetPreviewScene().Get(), &IPersonaPreviewScene::TogglePlayback));
}

void FSkeletalMeshEditor::ExtendToolbar()
{
	// If the ToolbarExtender is valid, remove it before rebuilding it
	if (ToolbarExtender.IsValid())
	{
		RemoveToolbarExtender(ToolbarExtender);
		ToolbarExtender.Reset();
	}

	ToolbarExtender = MakeShareable(new FExtender);


	// extend extra menu/toolbars
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.BeginSection("Mesh");
			{
				ToolbarBuilder.AddToolBarButton(FSkeletalMeshEditorCommands::Get().ReimportMesh);
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("Selection");
			{
				ToolbarBuilder.AddToolBarButton(FSkeletalMeshEditorCommands::Get().MeshSectionSelection);
			}
			ToolbarBuilder.EndSection();
		}
	};

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar)
		);

	AddToolbarExtender(ToolbarExtender);

	ISkeletalMeshEditorModule& SkeletalMeshEditorModule = FModuleManager::GetModuleChecked<ISkeletalMeshEditorModule>("SkeletalMeshEditor");
	AddToolbarExtender(SkeletalMeshEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	TArray<ISkeletalMeshEditorModule::FSkeletalMeshEditorToolbarExtender> ToolbarExtenderDelegates = SkeletalMeshEditorModule.GetAllSkeletalMeshEditorToolbarExtenders();

	for (auto& ToolbarExtenderDelegate : ToolbarExtenderDelegates)
	{
		if (ToolbarExtenderDelegate.IsBound())
		{
			AddToolbarExtender(ToolbarExtenderDelegate.Execute(GetToolkitCommands(), SharedThis(this)));
		}
	}

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& ParentToolbarBuilder)
	{
		FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
		TSharedRef<class IAssetFamily> AssetFamily = PersonaModule.CreatePersonaAssetFamily(SkeletalMesh);
		AddToolbarWidget(PersonaModule.CreateAssetFamilyShortcutWidget(SharedThis(this), AssetFamily));
	}
	));
}

void FSkeletalMeshEditor::FillMeshClickMenu(FMenuBuilder& MenuBuilder, HActor* HitProxy, const FViewportClick& Click)
{
	UDebugSkelMeshComponent* MeshComp = GetPersonaToolkit()->GetPreviewMeshComponent();

	// Must have hit something, but if the preview is invalid, bail
	if(!MeshComp)
	{
		return;
	}

	const int32 LodIndex = MeshComp->PredictedLODLevel;
	const int32 SectionIndex = HitProxy->SectionIndex;

	// Potentially we should display a different index if we have a clothing asset
	int32 DisplaySectionIndex = SectionIndex;
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();
	if(Mesh && Mesh->GetImportedResource())
	{
		if(FSkeletalMeshResource* Resource = Mesh->GetImportedResource())
		{
			if(Resource->LODModels.IsValidIndex(LodIndex))
			{
				if(Resource->LODModels[LodIndex].Sections.IsValidIndex(SectionIndex))
				{
					FSkelMeshSection& Section = Resource->LODModels[LodIndex].Sections[SectionIndex];

					if(Section.CorrespondClothSectionIndex != INDEX_NONE)
					{
						DisplaySectionIndex = Section.CorrespondClothSectionIndex;
					}
				}
			}
		}
	}

	TSharedRef<SWidget> InfoWidget = SNew(SBox)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(2.5f, 5.0f, 2.5f, 0.0f))
		[
			SNew(SBorder)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			//.Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("CurveEd.LabelFont"))
				.Text(FText::Format(LOCTEXT("MeshClickMenu_SectionInfo", "LOD{0} - Section {1}"), LodIndex, DisplaySectionIndex))
				]
			]
		];


	MenuBuilder.AddWidget(InfoWidget, FText::GetEmpty(), true, false);

	MenuBuilder.BeginSection(TEXT("MeshClickMenu_Asset"), LOCTEXT("MeshClickMenu_Section_Asset", "Asset"));
	{
		FUIAction Action;
		Action.CanExecuteAction = FCanExecuteAction::CreateSP(this, &FSkeletalMeshEditor::CanApplyClothing, LodIndex, SectionIndex);

		MenuBuilder.AddSubMenu(
			LOCTEXT("MeshClickMenu_AssetApplyMenu", "Apply Clothing Data..."),
			LOCTEXT("MeshClickMenu_AssetApplyMenu_ToolTip", "Select clothing data to apply to the selected section."),
			FNewMenuDelegate::CreateSP(this, &FSkeletalMeshEditor::FillApplyClothingAssetMenu, LodIndex, SectionIndex),
			Action,
			TEXT(""),
			EUserInterfaceActionType::Button
			);

		Action.ExecuteAction = FExecuteAction::CreateSP(this, &FSkeletalMeshEditor::OnRemoveClothingAssetMenuItemClicked, LodIndex, SectionIndex);
		Action.CanExecuteAction = FCanExecuteAction::CreateSP(this, &FSkeletalMeshEditor::CanRemoveClothing, LodIndex, SectionIndex);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("MeshClickMenu_RemoveClothing", "Remove Clothing Data"),
			LOCTEXT("MeshClickMenu_RemoveClothing_ToolTip", "Remove the currently assigned clothing data."),
			FSlateIcon(),
			Action
			);
			
		Action.ExecuteAction = FExecuteAction();
		Action.CanExecuteAction = FCanExecuteAction::CreateSP(this, &FSkeletalMeshEditor::CanCreateClothing, LodIndex, SectionIndex);

		MenuBuilder.AddSubMenu(
			LOCTEXT("MeshClickMenu_CreateClothing_Label", "Create Clothing Data from Section"),
			LOCTEXT("MeshClickMenu_CreateClothing_ToolTip", "Create a new clothing data using the selected section as a simulation mesh"),
			FNewMenuDelegate::CreateSP(this, &FSkeletalMeshEditor::FillCreateClothingMenu, LodIndex, SectionIndex),
			Action,
			TEXT(""),
			EUserInterfaceActionType::Button
			);

		Action.CanExecuteAction = FCanExecuteAction::CreateSP(this, &FSkeletalMeshEditor::CanCreateClothingLod, LodIndex, SectionIndex);

		MenuBuilder.AddSubMenu(
			LOCTEXT("MeshClickMenu_CreateClothingNewLod_Label", "Create Clothing LOD from Section"),
			LOCTEXT("MeshClickMenu_CreateClothingNewLod_ToolTip", "Create a clothing simulation mesh from the selected section and add it as a LOD to existing clothing data."),
			FNewMenuDelegate::CreateSP(this, &FSkeletalMeshEditor::FillCreateClothingLodMenu, LodIndex, SectionIndex),
			Action,
			TEXT(""),
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();
}

void FSkeletalMeshEditor::FillApplyClothingAssetMenu(FMenuBuilder& MenuBuilder, int32 InLodIndex, int32 InSectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	// Nothing to fill
	if(!Mesh)
	{
		return;
	}

	MenuBuilder.BeginSection(TEXT("ApplyClothingMenu"), LOCTEXT("ApplyClothingMenuHeader", "Available Assets"));
	{
		for(UClothingAssetBase* BaseAsset : Mesh->MeshClothingAssets)
		{
			UClothingAsset* ClothAsset = CastChecked<UClothingAsset>(BaseAsset);

			FUIAction Action;
			Action.CanExecuteAction = FCanExecuteAction::CreateSP(this, &FSkeletalMeshEditor::CanApplyClothing, InLodIndex, InSectionIndex);

			const int32 NumClothLods = ClothAsset->LodData.Num();
			for(int32 ClothLodIndex = 0; ClothLodIndex < NumClothLods; ++ClothLodIndex)
			{
				Action.ExecuteAction = FExecuteAction::CreateSP(this, &FSkeletalMeshEditor::OnApplyClothingAssetClicked, BaseAsset, InLodIndex, InSectionIndex, ClothLodIndex);

				MenuBuilder.AddMenuEntry(
					FText::Format(LOCTEXT("ApplyClothingMenuItem", "{0} - LOD{1}"), FText::FromString(ClothAsset->GetName()), FText::AsNumber(ClothLodIndex)),
					LOCTEXT("ApplyClothingMenuItem_ToolTip", "Apply this clothing asset to the selected mesh LOD and section"),
					FSlateIcon(),
					Action
					);
			}
		}
	}
	MenuBuilder.EndSection();
}

void FSkeletalMeshEditor::FillCreateClothingMenu(FMenuBuilder& MenuBuilder, int32 InLodIndex, int32 InSectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	if(!Mesh)
	{
		return;
	}

	TSharedRef<SWidget> Widget = SNew(SCreateClothingSettingsPanel)
		.Mesh(Mesh)
		.MeshName(Mesh->GetName())
		.LodIndex(InLodIndex)
		.SectionIndex(InSectionIndex)
		.OnCreateRequested(this, &FSkeletalMeshEditor::OnCreateClothingAssetMenuItemClicked)
		.bIsSubImport(false);

	MenuBuilder.AddWidget(Widget, FText::GetEmpty(), true, false);
}

void FSkeletalMeshEditor::FillCreateClothingLodMenu(FMenuBuilder& MenuBuilder, int32 InLodIndex, int32 InSectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	if(!Mesh)
	{
		return;
	}

	TSharedRef<SWidget> Widget = SNew(SCreateClothingSettingsPanel)
		.Mesh(Mesh)
		.MeshName(Mesh->GetName())
		.LodIndex(InLodIndex)
		.SectionIndex(InSectionIndex)
		.OnCreateRequested(this, &FSkeletalMeshEditor::OnCreateClothingAssetMenuItemClicked)
		.bIsSubImport(true);

		MenuBuilder.AddWidget(Widget, FText::GetEmpty(), true, false);
}

void FSkeletalMeshEditor::OnRemoveClothingAssetMenuItemClicked(int32 InLodIndex, int32 InSectionIndex)
{
	RemoveClothing(InLodIndex, InSectionIndex);
}

void FSkeletalMeshEditor::OnCreateClothingAssetMenuItemClicked(FSkeletalMeshClothBuildParams& Params)
{
	// Close the menu we created
	FSlateApplication::Get().DismissAllMenus();

	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	if(Mesh)
	{
		// Handle the creation through the clothing asset factory
		FClothingSystemEditorInterfaceModule& ClothingEditorModule = FModuleManager::LoadModuleChecked<FClothingSystemEditorInterfaceModule>("ClothingSystemEditorInterface");
		UClothingAssetFactoryBase* AssetFactory = ClothingEditorModule.GetClothingAssetFactory();

		Mesh->Modify();

		// See if we're importing a LOD or new asset
		if(Params.TargetAsset.IsValid())
		{
			AssetFactory->ImportLodToClothing(Mesh, Params);
		}
		else
		{
			UClothingAssetBase* NewClothingAsset = AssetFactory->CreateFromSkeletalMesh(Mesh, Params);

			if(NewClothingAsset)
			{
				Mesh->AddClothingAsset(NewClothingAsset);
			}
		}
	}
}

void FSkeletalMeshEditor::OnApplyClothingAssetClicked(UClothingAssetBase* InAssetToApply, int32 InMeshLodIndex, int32 InMeshSectionIndex, int32 InClothLodIndex)
{
	ApplyClothing(InAssetToApply, InMeshLodIndex, InMeshSectionIndex, InClothLodIndex);
}

bool FSkeletalMeshEditor::CanApplyClothing(int32 InLodIndex, int32 InSectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	if(Mesh->MeshClothingAssets.Num() > 0)
	{
		FSkeletalMeshResource* MeshResource = Mesh->GetImportedResource();

		if(MeshResource->LODModels.IsValidIndex(InLodIndex))
		{
			FStaticLODModel& LodModel = MeshResource->LODModels[InLodIndex];

			if(LodModel.Sections.IsValidIndex(InSectionIndex))
			{
				FSkelMeshSection& Section = LodModel.Sections[InSectionIndex];

				return Section.CorrespondClothSectionIndex == INDEX_NONE;
			}
		}
	}

	return false;
}

bool FSkeletalMeshEditor::CanRemoveClothing(int32 InLodIndex, int32 InSectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	FSkeletalMeshResource* MeshResource = Mesh->GetImportedResource();

	if(MeshResource->LODModels.IsValidIndex(InLodIndex))
	{
		FStaticLODModel& LodModel = MeshResource->LODModels[InLodIndex];

		if(LodModel.Sections.IsValidIndex(InSectionIndex))
		{
			FSkelMeshSection& Section = LodModel.Sections[InSectionIndex];

			return Section.CorrespondClothSectionIndex != INDEX_NONE;
		}
	}

	return false;
}

bool FSkeletalMeshEditor::CanCreateClothing(int32 InLodIndex, int32 InSectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	FSkeletalMeshResource* MeshResource = Mesh->GetImportedResource();

	if(MeshResource->LODModels.IsValidIndex(InLodIndex))
	{
		FStaticLODModel& LodModel = MeshResource->LODModels[InLodIndex];

		if(LodModel.Sections.IsValidIndex(InSectionIndex))
		{
			FSkelMeshSection& Section = LodModel.Sections[InSectionIndex];

			return Section.CorrespondClothSectionIndex == INDEX_NONE;
		}
	}

	return false;
}

bool FSkeletalMeshEditor::CanCreateClothingLod(int32 InLodIndex, int32 InSectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	return Mesh && Mesh->MeshClothingAssets.Num() > 0 && CanApplyClothing(InLodIndex, InSectionIndex);
}

void FSkeletalMeshEditor::ApplyClothing(UClothingAssetBase* InAsset, int32 InLodIndex, int32 InSectionIndex, int32 InClothingLod)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	if(UClothingAsset* ClothingAsset = Cast<UClothingAsset>(InAsset))
	{
		ClothingAsset->BindToSkeletalMesh(Mesh, InLodIndex, InSectionIndex, InClothingLod);
	}
	else
	{
		RemoveClothing(InLodIndex, InSectionIndex);
	}
}

void FSkeletalMeshEditor::RemoveClothing(int32 InLodIndex, int32 InSectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	if(Mesh)
	{
		if(UClothingAssetBase* CurrentAsset = Mesh->GetSectionClothingAsset(InLodIndex, InSectionIndex))
		{
			CurrentAsset->UnbindFromSkeletalMesh(Mesh, InLodIndex);
		}
	}
}

void FSkeletalMeshEditor::ExtendMenu()
{
	MenuExtender = MakeShareable(new FExtender);

	AddMenuExtender(MenuExtender);

	ISkeletalMeshEditorModule& SkeletalMeshEditorModule = FModuleManager::GetModuleChecked<ISkeletalMeshEditorModule>("SkeletalMeshEditor");
	AddMenuExtender(SkeletalMeshEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

void FSkeletalMeshEditor::HandleObjectsSelected(const TArray<UObject*>& InObjects)
{
	if (DetailsView.IsValid())
	{
		DetailsView->SetObjects(InObjects);
	}
}

void FSkeletalMeshEditor::HandleObjectSelected(UObject* InObject)
{
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(InObject);
	}
}

void FSkeletalMeshEditor::HandleSelectionChanged(const TArrayView<TSharedPtr<ISkeletonTreeItem>>& InSelectedItems, ESelectInfo::Type InSelectInfo)
{
	if (DetailsView.IsValid())
	{
		TArray<UObject*> Objects;
		Algo::TransformIf(InSelectedItems, Objects, [](const TSharedPtr<ISkeletonTreeItem>& InItem) { return InItem->GetObject() != nullptr; }, [](const TSharedPtr<ISkeletonTreeItem>& InItem) { return InItem->GetObject(); });
		DetailsView->SetObjects(Objects);
	}
}

void FSkeletalMeshEditor::PostUndo(bool bSuccess)
{
	OnPostUndo.Broadcast();
}

void FSkeletalMeshEditor::PostRedo(bool bSuccess)
{
	OnPostUndo.Broadcast();
}

void FSkeletalMeshEditor::Tick(float DeltaTime)
{
	GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
}

TStatId FSkeletalMeshEditor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FSkeletalMeshEditor, STATGROUP_Tickables);
}

void FSkeletalMeshEditor::HandleDetailsCreated(const TSharedRef<IDetailsView>& InDetailsView)
{
	DetailsView = InDetailsView;
}

void FSkeletalMeshEditor::HandleMeshDetailsCreated(const TSharedRef<IDetailsView>& InDetailsView)
{
	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");
	PersonaModule.CustomizeMeshDetails(InDetailsView, GetPersonaToolkit());
}

UObject* FSkeletalMeshEditor::HandleGetAsset()
{
	return GetEditingObject();
}

void FSkeletalMeshEditor::HandleReimportMesh()
{
	// Reimport the asset
	if (SkeletalMesh)
	{
		FReimportManager::Instance()->Reimport(SkeletalMesh, true);
	}
}

void FSkeletalMeshEditor::ToggleMeshSectionSelection()
{
	TSharedRef<IPersonaPreviewScene> PreviewScene = GetPersonaToolkit()->GetPreviewScene();
	bool bState = !PreviewScene->AllowMeshHitProxies();
	GetMutableDefault<UPersonaOptions>()->bAllowMeshSectionSelection = bState;
	PreviewScene->SetAllowMeshHitProxies(bState);	
}

bool FSkeletalMeshEditor::IsMeshSectionSelectionChecked() const
{
	return GetPersonaToolkit()->GetPreviewScene()->AllowMeshHitProxies();
}

void FSkeletalMeshEditor::HandleMeshClick(HActor* HitProxy, const FViewportClick& Click)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	if(Mesh)
	{
		Mesh->SelectedEditorSection = HitProxy->SectionIndex;
	}

	USkeletalMeshComponent* Component = GetPersonaToolkit()->GetPreviewMeshComponent();
	if (Component)
	{
		Component->PushSelectionToProxy();
	}

	if(Click.GetKey() == EKeys::RightMouseButton)
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		FillMeshClickMenu(MenuBuilder, HitProxy, Click);

		FSlateApplication::Get().PushMenu(
			FSlateApplication::Get().GetActiveTopLevelWindow().ToSharedRef(),
			FWidgetPath(),
			MenuBuilder.MakeWidget(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
			);
	}
}

#undef LOCTEXT_NAMESPACE
