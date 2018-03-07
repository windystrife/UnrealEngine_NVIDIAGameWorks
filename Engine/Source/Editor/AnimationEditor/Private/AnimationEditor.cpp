// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimationEditor.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "EditorReimportHandler.h"
#include "Animation/SmartName.h"
#include "Animation/AnimationAsset.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AssetData.h"
#include "EdGraph/EdGraphSchema.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Editor/EditorEngine.h"
#include "Factories/AnimSequenceFactory.h"
#include "Factories/PoseAssetFactory.h"
#include "EngineGlobals.h"
#include "Editor.h"
#include "IAnimationEditorModule.h"
#include "IPersonaToolkit.h"
#include "PersonaModule.h"
#include "AnimationEditorMode.h"
#include "IPersonaPreviewScene.h"
#include "AnimationEditorCommands.h"
#include "IDetailsView.h"
#include "ISkeletonTree.h"
#include "ISkeletonEditorModule.h"
#include "IDocumentation.h"
#include "Widgets/Docking/SDockTab.h"
#include "Animation/PoseAsset.h"
#include "AnimPreviewInstance.h"
#include "ScopedTransaction.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "AnimationEditorUtils.h"
#include "AssetRegistryModule.h"
#include "IAssetFamily.h"
#include "IAnimationSequenceBrowser.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "PersonaCommonCommands.h"
#include "Sound/SoundWave.h"
#include "Engine/CurveTable.h"
#include "Developer/AssetTools/Public/IAssetTools.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "SequenceRecorderUtils.h"
#include "ISkeletonTreeItem.h"
#include "Algo/Transform.h"

const FName AnimationEditorAppIdentifier = FName(TEXT("AnimationEditorApp"));

const FName AnimationEditorModes::AnimationEditorMode(TEXT("AnimationEditorMode"));

const FName AnimationEditorTabs::DetailsTab(TEXT("DetailsTab"));
const FName AnimationEditorTabs::SkeletonTreeTab(TEXT("SkeletonTreeView"));
const FName AnimationEditorTabs::ViewportTab(TEXT("Viewport"));
const FName AnimationEditorTabs::AdvancedPreviewTab(TEXT("AdvancedPreviewTab"));
const FName AnimationEditorTabs::DocumentTab(TEXT("Document"));
const FName AnimationEditorTabs::AssetBrowserTab(TEXT("SequenceBrowser"));
const FName AnimationEditorTabs::AssetDetailsTab(TEXT("AnimAssetPropertiesTab"));
const FName AnimationEditorTabs::CurveNamesTab(TEXT("AnimCurveViewerTab"));
const FName AnimationEditorTabs::SlotNamesTab(TEXT("SkeletonSlotNames"));

DEFINE_LOG_CATEGORY(LogAnimationEditor);

#define LOCTEXT_NAMESPACE "AnimationEditor"

FAnimationEditor::FAnimationEditor()
{
	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->RegisterForUndo(this);
	}
}

FAnimationEditor::~FAnimationEditor()
{
	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->UnregisterForUndo(this);
	}

	FEditorDelegates::OnAssetPostImport.RemoveAll(this);
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);
}

void FAnimationEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_AnimationEditor", "Animation Editor"));

	FAssetEditorToolkit::RegisterTabSpawners( InTabManager );
}

void FAnimationEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

void FAnimationEditor::InitAnimationEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UAnimationAsset* InAnimationAsset)
{
	AnimationAsset = InAnimationAsset;

	// Register post import callback to catch animation imports when we have the asset open (we need to reinit)
	FReimportManager::Instance()->OnPostReimport().AddRaw(this, &FAnimationEditor::HandlePostReimport);
	FEditorDelegates::OnAssetPostImport.AddRaw(this, &FAnimationEditor::HandlePostImport);

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	PersonaToolkit = PersonaModule.CreatePersonaToolkit(InAnimationAsset);

	PersonaToolkit->GetPreviewScene()->SetDefaultAnimationMode(EPreviewSceneDefaultAnimationMode::Animation);

	FSkeletonTreeArgs SkeletonTreeArgs;
	SkeletonTreeArgs.OnSelectionChanged = FOnSkeletonTreeSelectionChanged::CreateSP(this, &FAnimationEditor::HandleSelectionChanged);
	SkeletonTreeArgs.PreviewScene = PersonaToolkit->GetPreviewScene();

	ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::GetModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	SkeletonTree = SkeletonEditorModule.CreateSkeletonTree(PersonaToolkit->GetSkeleton(), SkeletonTreeArgs);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	const TSharedRef<FTabManager::FLayout> DummyLayout = FTabManager::NewLayout("NullLayout")->AddArea(FTabManager::NewPrimaryArea());
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, AnimationEditorAppIdentifier, DummyLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InAnimationAsset);

	BindCommands();

	AddApplicationMode(
		AnimationEditorModes::AnimationEditorMode,
		MakeShareable(new FAnimationEditorMode(SharedThis(this), SkeletonTree.ToSharedRef())));

	SetCurrentMode(AnimationEditorModes::AnimationEditorMode);

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	OpenNewAnimationDocumentTab(AnimationAsset);
}

FName FAnimationEditor::GetToolkitFName() const
{
	return FName("AnimationEditor");
}

FText FAnimationEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "AnimationEditor");
}

FString FAnimationEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "AnimationEditor ").ToString();
}

FLinearColor FAnimationEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

void FAnimationEditor::Tick(float DeltaTime)
{
	GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
}

TStatId FAnimationEditor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FAnimationEditor, STATGROUP_Tickables);
}

void FAnimationEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(AnimationAsset);
}

void FAnimationEditor::BindCommands()
{
	FAnimationEditorCommands::Register();
	
	ToolkitCommands->MapAction(FAnimationEditorCommands::Get().ApplyCompression,
		FExecuteAction::CreateSP(this, &FAnimationEditor::OnApplyCompression),
		FCanExecuteAction::CreateSP(this, &FAnimationEditor::HasValidAnimationSequence));

	ToolkitCommands->MapAction(FAnimationEditorCommands::Get().SetKey,
		FExecuteAction::CreateSP(this, &FAnimationEditor::OnSetKey),
		FCanExecuteAction::CreateSP(this, &FAnimationEditor::CanSetKey));

	ToolkitCommands->MapAction(FAnimationEditorCommands::Get().ReimportAnimation,
		FExecuteAction::CreateSP(this, &FAnimationEditor::OnReimportAnimation),
		FCanExecuteAction::CreateSP(this, &FAnimationEditor::HasValidAnimationSequence));

	ToolkitCommands->MapAction(FAnimationEditorCommands::Get().ApplyAnimation,
		FExecuteAction::CreateSP(this, &FAnimationEditor::OnApplyRawAnimChanges),
		FCanExecuteAction::CreateSP(this, &FAnimationEditor::CanApplyRawAnimChanges));

	ToolkitCommands->MapAction(FAnimationEditorCommands::Get().ExportToFBX_AnimData,
		FExecuteAction::CreateSP(this, &FAnimationEditor::OnExportToFBX, EPoseSourceOption::CurrentAnimation_AnimData),
		FCanExecuteAction::CreateSP(this, &FAnimationEditor::HasValidAnimationSequence));

	ToolkitCommands->MapAction(FAnimationEditorCommands::Get().ExportToFBX_PreviewMesh,
		FExecuteAction::CreateSP(this, &FAnimationEditor::OnExportToFBX, EPoseSourceOption::CurrentAnimation_PreviewMesh),
		FCanExecuteAction::CreateSP(this, &FAnimationEditor::HasValidAnimationSequence));

	ToolkitCommands->MapAction(FAnimationEditorCommands::Get().AddLoopingInterpolation,
		FExecuteAction::CreateSP(this, &FAnimationEditor::OnAddLoopingInterpolation),
		FCanExecuteAction::CreateSP(this, &FAnimationEditor::HasValidAnimationSequence));

	ToolkitCommands->MapAction(FPersonaCommonCommands::Get().TogglePlay,
		FExecuteAction::CreateRaw(&GetPersonaToolkit()->GetPreviewScene().Get(), &IPersonaPreviewScene::TogglePlayback));
}

void FAnimationEditor::ExtendToolbar()
{
	// If the ToolbarExtender is valid, remove it before rebuilding it
	if (ToolbarExtender.IsValid())
	{
		RemoveToolbarExtender(ToolbarExtender);
		ToolbarExtender.Reset();
	}

	ToolbarExtender = MakeShareable(new FExtender);

	AddToolbarExtender(ToolbarExtender);

	IAnimationEditorModule& AnimationEditorModule = FModuleManager::GetModuleChecked<IAnimationEditorModule>("AnimationEditor");
	AddToolbarExtender(AnimationEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	TArray<IAnimationEditorModule::FAnimationEditorToolbarExtender> ToolbarExtenderDelegates = AnimationEditorModule.GetAllAnimationEditorToolbarExtenders();

	for (auto& ToolbarExtenderDelegate : ToolbarExtenderDelegates)
	{
		if (ToolbarExtenderDelegate.IsBound())
		{
			AddToolbarExtender(ToolbarExtenderDelegate.Execute(GetToolkitCommands(), SharedThis(this)));
		}
	}

	// extend extra menu/toolbars
	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.BeginSection("Animation");
			{
				// create button
				{
					ToolbarBuilder.AddComboButton(
						FUIAction(),
						FOnGetContent::CreateSP(this, &FAnimationEditor::GenerateCreateAssetMenu),
						LOCTEXT("CreateAsset_Label", "Create Asset"),
						LOCTEXT("CreateAsset_ToolTip", "Create Assets for this skeleton."),
						FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.CreateAsset")
					);
				}

				ToolbarBuilder.AddToolBarButton(FAnimationEditorCommands::Get().ReimportAnimation);
				ToolbarBuilder.AddToolBarButton(FAnimationEditorCommands::Get().ApplyCompression, NAME_None, LOCTEXT("Toolbar_ApplyCompression", "Compression"));

				{
					ToolbarBuilder.AddComboButton(
						FUIAction(),
						FOnGetContent::CreateSP(this, &FAnimationEditor::GenerateExportAssetMenu),
						LOCTEXT("ExportAsset_Label", "Export Asset"),
						LOCTEXT("ExportAsset_ToolTip", "Export Assets for this skeleton."),
						FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.ExportToFBX")
					);
				}
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("Editing");
			{
				ToolbarBuilder.AddToolBarButton(FAnimationEditorCommands::Get().SetKey, NAME_None, LOCTEXT("Toolbar_SetKey", "Key"));
				ToolbarBuilder.AddToolBarButton(FAnimationEditorCommands::Get().ApplyAnimation, NAME_None, LOCTEXT("Toolbar_ApplyAnimation", "Apply"));
			}
			ToolbarBuilder.EndSection();

			FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
			PersonaModule.AddCommonToolbarExtensions(ToolbarBuilder, PersonaToolkit.ToSharedRef());

			TSharedRef<class IAssetFamily> AssetFamily = PersonaModule.CreatePersonaAssetFamily(AnimationAsset);
			AddToolbarWidget(PersonaModule.CreateAssetFamilyShortcutWidget(SharedThis(this), AssetFamily));
		}	
	));
}

void FAnimationEditor::ExtendMenu()
{
	MenuExtender = MakeShareable(new FExtender);

	struct Local
	{
		static void AddAssetMenu(FMenuBuilder& MenuBuilder, FAnimationEditor* InAnimationEditor)
		{
			MenuBuilder.BeginSection("AnimationEditor", LOCTEXT("AnimationEditorAssetMenu_Animation", "Animation"));
			{
				MenuBuilder.AddMenuEntry(FAnimationEditorCommands::Get().ApplyCompression);

				MenuBuilder.AddSubMenu(
					LOCTEXT("ExportToFBX", "Export to FBX"),
					LOCTEXT("ExportToFBX_ToolTip", "Export current animation to FBX"),
					FNewMenuDelegate::CreateSP(InAnimationEditor, &FAnimationEditor::FillExportAssetMenu),
					false,
					FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.")
				);

				MenuBuilder.AddMenuEntry(FAnimationEditorCommands::Get().AddLoopingInterpolation);

				MenuBuilder.AddSubMenu(
					LOCTEXT("CopyCurvesToSoundWave", "Copy Curves To SoundWave"),
					LOCTEXT("CopyCurvesToSoundWave_ToolTip", "Copy curves from this animation to the selected SoundWave"),
					FNewMenuDelegate::CreateSP(InAnimationEditor, &FAnimationEditor::FillCopyToSoundWaveMenu),
					false,
					FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.")
				);
			}
			MenuBuilder.EndSection();
		}
	};

	MenuExtender->AddMenuExtension(
		"AssetEditorActions",
		EExtensionHook::After,
		GetToolkitCommands(),
		FMenuExtensionDelegate::CreateStatic(&Local::AddAssetMenu, this)
		);

	AddMenuExtender(MenuExtender);

	IAnimationEditorModule& AnimationEditorModule = FModuleManager::GetModuleChecked<IAnimationEditorModule>("AnimationEditor");
	AddMenuExtender(AnimationEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

void FAnimationEditor::HandleObjectsSelected(const TArray<UObject*>& InObjects)
{
	if (DetailsView.IsValid())
	{
		DetailsView->SetObjects(InObjects);
	}
}

void FAnimationEditor::HandleSelectionChanged(const TArrayView<TSharedPtr<ISkeletonTreeItem>>& InSelectedItems, ESelectInfo::Type InSelectInfo)
{
	if (DetailsView.IsValid())
	{
		TArray<UObject*> Objects;
		Algo::TransformIf(InSelectedItems, Objects, [](const TSharedPtr<ISkeletonTreeItem>& InItem) { return InItem->GetObject() != nullptr; }, [](const TSharedPtr<ISkeletonTreeItem>& InItem) { return InItem->GetObject(); });
		DetailsView->SetObjects(Objects);
	}
}

void FAnimationEditor::HandleObjectSelected(UObject* InObject)
{
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(InObject);
	}
}

void FAnimationEditor::PostUndo(bool bSuccess)
{
	OnPostUndo.Broadcast();
}

void FAnimationEditor::PostRedo(bool bSuccess)
{
	OnPostUndo.Broadcast();
}

void FAnimationEditor::HandleDetailsCreated(const TSharedRef<IDetailsView>& InDetailsView)
{
	DetailsView = InDetailsView;
}

TSharedPtr<SDockTab> FAnimationEditor::OpenNewAnimationDocumentTab(UAnimationAsset* InAnimAsset)
{
	TSharedPtr<SDockTab> OpenedTab;

	if (InAnimAsset != nullptr)
	{
		FString	DocumentLink;

		FAnimDocumentArgs Args(PersonaToolkit->GetPreviewScene(), GetPersonaToolkit(), GetSkeletonTree()->GetEditableSkeleton(), OnPostUndo, OnChangeAnimNotifies, OnSectionsChanged);
		Args.OnDespatchObjectsSelected = FOnObjectsSelected::CreateSP(this, &FAnimationEditor::HandleObjectsSelected);
		Args.OnDespatchAnimNotifiesChanged = FSimpleDelegate::CreateSP(this, &FAnimationEditor::HandleAnimNotifiesChanged);
		Args.OnDespatchInvokeTab = FOnInvokeTab::CreateSP(this, &FAssetEditorToolkit::InvokeTab);
		Args.OnDespatchSectionsChanged = FSimpleDelegate::CreateSP(this, &FAnimationEditor::HandleSectionsChanged);

		FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");
		TSharedRef<SWidget> TabContents = PersonaModule.CreateEditorWidgetForAnimDocument(SharedThis(this), InAnimAsset, Args, DocumentLink);

		if (AnimationAsset)
		{
			RemoveEditingObject(AnimationAsset);
		}

		AddEditingObject(InAnimAsset);
		AnimationAsset = InAnimAsset;

		GetPersonaToolkit()->GetPreviewScene()->SetPreviewAnimationAsset(InAnimAsset);
		GetPersonaToolkit()->SetAnimationAsset(InAnimAsset);

		struct Local
		{
			static FText GetObjectName(UObject* Object)
			{
				return FText::FromString(Object->GetName());
			}
		};

		TAttribute<FText> NameAttribute = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&Local::GetObjectName, (UObject*)InAnimAsset));

		if (SharedAnimDocumentTab.IsValid())
		{
			OpenedTab = SharedAnimDocumentTab.Pin();
			OpenedTab->SetContent(TabContents);
			OpenedTab->ActivateInParent(ETabActivationCause::SetDirectly);
			OpenedTab->SetLabel(NameAttribute);
			OpenedTab->SetLeftContent(IDocumentation::Get()->CreateAnchor(DocumentLink));
		}
		else
		{
			OpenedTab = SNew(SDockTab)
				.Label(NameAttribute)
				.TabRole(ETabRole::DocumentTab)
				.TabColorScale(GetTabColorScale())
				[
					TabContents
				];

			OpenedTab->SetLeftContent(IDocumentation::Get()->CreateAnchor(DocumentLink));

			TabManager->InsertNewDocumentTab(AnimationEditorTabs::DocumentTab, FTabManager::ESearchPreference::RequireClosedTab, OpenedTab.ToSharedRef());

			SharedAnimDocumentTab = OpenedTab;
		}

		if (SequenceBrowser.IsValid())
		{
			SequenceBrowser.Pin()->SelectAsset(InAnimAsset);
		}

		// let the asset family know too
		TSharedRef<IAssetFamily> AssetFamily = PersonaModule.CreatePersonaAssetFamily(InAnimAsset);
		AssetFamily->RecordAssetOpened(FAssetData(InAnimAsset));
	}

	return OpenedTab;
}

void FAnimationEditor::HandleAnimNotifiesChanged()
{
	OnChangeAnimNotifies.Broadcast();
}

void FAnimationEditor::HandleSectionsChanged()
{
	OnSectionsChanged.Broadcast();
}

void FAnimationEditor::SetAnimationAsset(UAnimationAsset* AnimAsset)
{
	HandleOpenNewAsset(AnimAsset);
}

void FAnimationEditor::HandleOpenNewAsset(UObject* InNewAsset)
{
	if (UAnimationAsset* NewAnimationAsset = Cast<UAnimationAsset>(InNewAsset))
	{
		OpenNewAnimationDocumentTab(NewAnimationAsset);
	}
}

UObject* FAnimationEditor::HandleGetAsset()
{
	return GetEditingObject();
}

bool FAnimationEditor::HasValidAnimationSequence() const
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimationAsset);
	return (AnimSequence != nullptr);
}

bool FAnimationEditor::CanSetKey() const
{
	UDebugSkelMeshComponent* PreviewMeshComponent = PersonaToolkit->GetPreviewMeshComponent();
	return (HasValidAnimationSequence() && PreviewMeshComponent->BonesOfInterest.Num() > 0);
}

void FAnimationEditor::OnSetKey()
{
	if (AnimationAsset)
	{
		UDebugSkelMeshComponent* Component = PersonaToolkit->GetPreviewMeshComponent();
		Component->PreviewInstance->SetKey();
	}
}

bool FAnimationEditor::CanApplyRawAnimChanges() const
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimationAsset);

	// ideally would be great if we can only show if something changed
	return (AnimSequence && (AnimSequence->DoesNeedRebake() || AnimSequence->DoesNeedRecompress()));
}

void FAnimationEditor::OnApplyRawAnimChanges()
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimationAsset);
	if (AnimSequence)
	{
		if (AnimSequence->DoesNeedRebake() || AnimSequence->DoesNeedRecompress())
		{
			FScopedTransaction ScopedTransaction(LOCTEXT("BakeAnimation", "Bake Animation"));
			if (AnimSequence->DoesNeedRebake())
			{
				AnimSequence->Modify(true);
				AnimSequence->BakeTrackCurvesToRawAnimation();
			}

			if (AnimSequence->DoesNeedRecompress())
			{
				AnimSequence->Modify(true);
				AnimSequence->RequestSyncAnimRecompression(false);
			}
		}
	}
}

void FAnimationEditor::OnReimportAnimation()
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimationAsset);
	if (AnimSequence)
	{
		FReimportManager::Instance()->Reimport(AnimSequence, true);
	}
}

void FAnimationEditor::OnApplyCompression()
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimationAsset);
	if (AnimSequence)
	{
		TArray<TWeakObjectPtr<UAnimSequence>> AnimSequences;
		AnimSequences.Add(AnimSequence);
		FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");
		PersonaModule.ApplyCompression(AnimSequences);
	}
}

void FAnimationEditor::OnExportToFBX(const EPoseSourceOption Option)
{
	UAnimSequence* AnimSequenceToRecord = nullptr;
	if (Option == EPoseSourceOption::CurrentAnimation_AnimData)
	{
		TArray<UObject*> AssetsToExport;
		AssetsToExport.Add(AnimationAsset);
		ExportToFBX(AssetsToExport, false);
	}
	else if (Option == EPoseSourceOption::CurrentAnimation_PreviewMesh)
	{
		TArray<TWeakObjectPtr<UObject>> Skeletons;
		Skeletons.Add(PersonaToolkit->GetSkeleton());

		AnimationEditorUtils::CreateAnimationAssets(Skeletons, UAnimSequence::StaticClass(), FString("_PreviewMesh"), FAnimAssetCreated::CreateSP(this, &FAnimationEditor::ExportToFBX, true), AnimationAsset, true);
	}
	else
	{
		ensure(false);
	}
}

void FAnimationEditor::ExportToFBX(const TArray<UObject*> AssetsToExport, bool bRecordAnimation)
{
	TArray<TWeakObjectPtr<UAnimSequence>> AnimSequences;
	if (AssetsToExport.Num() > 0)
	{
		UAnimSequence* AnimationToRecord = Cast<UAnimSequence>(AssetsToExport[0]);
		if (AnimationToRecord)
		{
			if (bRecordAnimation)
			{
				USkeletalMeshComponent* MeshComponent = PersonaToolkit->GetPreviewMeshComponent();
				RecordMeshToAnimation(MeshComponent, AnimationToRecord);
			}

			AnimSequences.Add(AnimationToRecord);
		}
	}

	if (AnimSequences.Num() > 0)
	{
		FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");
		PersonaModule.ExportToFBX(AnimSequences, GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent()->SkeletalMesh);
	}
}

void FAnimationEditor::OnAddLoopingInterpolation()
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimationAsset);
	if (AnimSequence)
	{
		TArray<TWeakObjectPtr<UAnimSequence>> AnimSequences;
		AnimSequences.Add(AnimSequence);
		FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");
		PersonaModule.AddLoopingInterpolation(AnimSequences);
	}
}

TSharedRef< SWidget > FAnimationEditor::GenerateExportAssetMenu() const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, GetToolkitCommands());
	FillExportAssetMenu(MenuBuilder);
	return MenuBuilder.MakeWidget();
}

TSharedRef< SWidget > FAnimationEditor::GenerateCreateAssetMenu() const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, NULL);

	// Create Animation menu
	MenuBuilder.BeginSection("CreateAnimation", LOCTEXT("CreateAnimationMenuHeading", "Animation"));
	{
		// create menu
		MenuBuilder.AddSubMenu(
			LOCTEXT("CreateAnimationSubmenu", "Create Animation"),
			LOCTEXT("CreateAnimationSubmenu_ToolTip", "Create Animation for this skeleton"),
			FNewMenuDelegate::CreateSP(this, &FAnimationEditor::FillCreateAnimationMenu),
			false,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.CreateAnimAsset")
			);

		MenuBuilder.AddSubMenu(
			LOCTEXT("CreatePoseAssetSubmenu", "Create PoseAsset"),
			LOCTEXT("CreatePoseAsssetSubmenu_ToolTip", "Create PoseAsset for this skeleton"),
			FNewMenuDelegate::CreateSP(this, &FAnimationEditor::FillCreatePoseAssetMenu),
			false,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.PoseAsset")
		);
	}
	MenuBuilder.EndSection();

	TArray<TWeakObjectPtr<UObject>> Objects;

	if (PersonaToolkit->GetPreviewMesh())
	{
		Objects.Add(PersonaToolkit->GetPreviewMesh());
	}
	else
	{
		Objects.Add(PersonaToolkit->GetSkeleton());
	}

	AnimationEditorUtils::FillCreateAssetMenu(MenuBuilder, Objects, FAnimAssetCreated::CreateSP(this, &FAnimationEditor::HandleAssetCreated), false);

	return MenuBuilder.MakeWidget();
}

void FAnimationEditor::FillCreateAnimationMenu(FMenuBuilder& MenuBuilder) const
{
	TArray<TWeakObjectPtr<UObject>> Objects;

	if (PersonaToolkit->GetPreviewMesh())
	{
		Objects.Add(PersonaToolkit->GetPreviewMesh());
	}
	else
	{
		Objects.Add(PersonaToolkit->GetSkeleton());
	}

	// create rig
	MenuBuilder.BeginSection("CreateAnimationSubMenu", LOCTEXT("CreateAnimationSubMenuHeading", "Create Animation"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CreateAnimation_RefPose", "Reference Pose"),
			LOCTEXT("CreateAnimation_RefPose_Tooltip", "Create Animation from reference pose."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&AnimationEditorUtils::ExecuteNewAnimAsset<UAnimSequenceFactory, UAnimSequence>, Objects, FString("_Sequence"), FAnimAssetCreated::CreateSP(this, &FAnimationEditor::CreateAnimation, EPoseSourceOption::ReferencePose), false),
				FCanExecuteAction()
				)
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("CreateAnimation_CurrentPose", "Current Pose"),
			LOCTEXT("CreateAnimation_CurrentPose_Tooltip", "Create Animation from current pose."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&AnimationEditorUtils::ExecuteNewAnimAsset<UAnimSequenceFactory, UAnimSequence>, Objects, FString("_Sequence"), FAnimAssetCreated::CreateSP(this, &FAnimationEditor::CreateAnimation, EPoseSourceOption::CurrentPose), false),
				FCanExecuteAction()
				)
			);

		MenuBuilder.AddSubMenu(
			LOCTEXT("CreateAnimation_CurrenAnimationSubMenu", "Current Animation"),
			LOCTEXT("CreateAnimation_CurrenAnimationSubMenu_ToolTip", "Create Animation from current animation"),
			FNewMenuDelegate::CreateSP(this, &FAnimationEditor::FillCreateAnimationFromCurrentAnimationMenu),
			false,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.CreateAnimAsset")
		);
	}
	MenuBuilder.EndSection();
}

void FAnimationEditor::FillCreateAnimationFromCurrentAnimationMenu(FMenuBuilder& MenuBuilder) const
{
	{
		TArray<TWeakObjectPtr<UObject>> Objects;

		if (PersonaToolkit->GetPreviewMesh())
		{
			Objects.Add(PersonaToolkit->GetPreviewMesh());
		}
		else
		{
			Objects.Add(PersonaToolkit->GetSkeleton());
		}

		// create rig
		MenuBuilder.BeginSection("CreateAnimationSubMenu", LOCTEXT("CreateAnimationFromCurrentAnimationSubmenuHeading", "Create Animation"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("CreateAnimation_CurrentAnimation_AnimData", "Animation Data"),
				LOCTEXT("CreateAnimation_CurrentAnimation_AnimData_Tooltip", "Create Animation from Animation Source Data."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateStatic(&AnimationEditorUtils::ExecuteNewAnimAsset<UAnimSequenceFactory, UAnimSequence>, Objects, FString("_Sequence"), FAnimAssetCreated::CreateSP(this, &FAnimationEditor::CreateAnimation, EPoseSourceOption::CurrentAnimation_AnimData), false),
					FCanExecuteAction::CreateSP(this, &FAnimationEditor::HasValidAnimationSequence)
				)
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("CreateAnimation_CurrentAnimation_PreviewMesh", "Preview Mesh"),
				LOCTEXT("CreateAnimation_CurrentAnimation_PreviewMesh_Tooltip", "Create Animation by playing on the Current Preview Mesh, including Retargeting, Post Process Graph, or anything you see on the preview mesh."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateStatic(&AnimationEditorUtils::ExecuteNewAnimAsset<UAnimSequenceFactory, UAnimSequence>, Objects, FString("_Sequence"), FAnimAssetCreated::CreateSP(this, &FAnimationEditor::CreateAnimation, EPoseSourceOption::CurrentAnimation_PreviewMesh), false),
					FCanExecuteAction::CreateSP(this, &FAnimationEditor::HasValidAnimationSequence)
				)
			);
		}
		MenuBuilder.EndSection();
	}

}
void FAnimationEditor::FillCreatePoseAssetMenu(FMenuBuilder& MenuBuilder) const
{
	TArray<TWeakObjectPtr<UObject>> Objects;

	if (PersonaToolkit->GetPreviewMesh())
	{
		Objects.Add(PersonaToolkit->GetPreviewMesh());
	}
	else
	{
		Objects.Add(PersonaToolkit->GetSkeleton());
	}

	// create rig
	MenuBuilder.BeginSection("CreatePoseAssetSubMenu", LOCTEXT("CreatePoseAssetSubMenuHeading", "Create PoseAsset"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CreatePoseAsset_CurrentPose", "Current Pose"),
			LOCTEXT("CreatePoseAsset_CurrentPose_Tooltip", "Create PoseAsset from current pose."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&AnimationEditorUtils::ExecuteNewAnimAsset<UPoseAssetFactory, UPoseAsset>, Objects, FString("_PoseAsset"), FAnimAssetCreated::CreateSP(this, &FAnimationEditor::CreatePoseAsset, EPoseSourceOption::CurrentPose), false),
				FCanExecuteAction()
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("CreatePoseAsset_CurrentAnimation", "Current Animation"),
			LOCTEXT("CreatePoseAsset_CurrentAnimation_Tooltip", "Create Animation from current animation."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&AnimationEditorUtils::ExecuteNewAnimAsset<UPoseAssetFactory, UPoseAsset>, Objects, FString("_PoseAsset"), FAnimAssetCreated::CreateSP(this, &FAnimationEditor::CreatePoseAsset, EPoseSourceOption::CurrentAnimation_AnimData), false),
				FCanExecuteAction()
			)
		);
	}
	MenuBuilder.EndSection();

	// create pose asset
	MenuBuilder.BeginSection("InsertPoseSubMenuSection", LOCTEXT("InsertPoseSubMenuSubMenuHeading", "Insert Pose"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("InsertPoseSubmenu", "Insert Pose"),
			LOCTEXT("InsertPoseSubmenu_ToolTip", "Insert current pose to selected PoseAsset"),
			FNewMenuDelegate::CreateSP(this, &FAnimationEditor::FillInsertPoseMenu),
			false,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.PoseAsset")
		);
	}
	MenuBuilder.EndSection();
}

void FAnimationEditor::FillInsertPoseMenu(FMenuBuilder& MenuBuilder) const
{
	FAssetPickerConfig AssetPickerConfig;

	USkeleton* Skeleton = GetPersonaToolkit()->GetSkeleton();

	/** The asset picker will only show skeletons */
	AssetPickerConfig.Filter.ClassNames.Add(*UPoseAsset::StaticClass()->GetName());
	AssetPickerConfig.Filter.bRecursiveClasses = false;
	AssetPickerConfig.bAllowNullSelection = false;
	AssetPickerConfig.Filter.TagsAndValues.Add(TEXT("Skeleton"), FAssetData(Skeleton).GetExportTextName());

	/** The delegate that fires when an asset was selected */
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(this, &FAnimationEditor::InsertCurrentPoseToAsset);

	/** The default view mode should be a list view */
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	MenuBuilder.AddWidget(
		ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig),
		LOCTEXT("Select_Label", "")
	);
}

void FAnimationEditor::InsertCurrentPoseToAsset(const FAssetData& NewPoseAssetData)
{
	UPoseAsset* PoseAsset = Cast<UPoseAsset>(NewPoseAssetData.GetAsset());
	FScopedTransaction ScopedTransaction(LOCTEXT("InsertPose", "Insert Pose"));

	if (PoseAsset)
	{
		PoseAsset->Modify();

		UDebugSkelMeshComponent* PreviewMeshComponent = PersonaToolkit->GetPreviewMeshComponent();
		if (PreviewMeshComponent)
		{
			FSmartName NewPoseName;

			bool bSuccess = PoseAsset->AddOrUpdatePoseWithUniqueName(PreviewMeshComponent, &NewPoseName);

			if (bSuccess)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("PoseAsset"), FText::FromString(PoseAsset->GetName()));
				Args.Add(TEXT("PoseName"), FText::FromName(NewPoseName.DisplayName));
				FNotificationInfo Info(FText::Format(LOCTEXT("InsertPoseSucceeded", "The current pose has inserted to {PoseAsset} with {PoseName}"), Args));
				Info.ExpireDuration = 7.0f;
				Info.bUseLargeFont = false;
				TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
				if (Notification.IsValid())
				{
					Notification->SetCompletionState(SNotificationItem::CS_Success);
				}
			}
			else
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("PoseAsset"), FText::FromString(PoseAsset->GetName()));
				FNotificationInfo Info(FText::Format(LOCTEXT("InsertPoseFailed", "Inserting pose to asset {PoseAsset} has failed"), Args));
				Info.ExpireDuration = 7.0f;
				Info.bUseLargeFont = false;
				TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
				if (Notification.IsValid())
				{
					Notification->SetCompletionState(SNotificationItem::CS_Fail);
				}
			}
		}
	}

	// it doesn't work well if I leave the window open. The delegate goes weired or it stop showing the popups. 
	FSlateApplication::Get().DismissAllMenus();
}


void FAnimationEditor::FillCopyToSoundWaveMenu(FMenuBuilder& MenuBuilder) const
{
	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Add(*USoundWave::StaticClass()->GetName());
	AssetPickerConfig.bAllowNullSelection = false;
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(this, &FAnimationEditor::CopyCurveToSoundWave);
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	MenuBuilder.AddWidget(
		ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig),
		LOCTEXT("Select_Label", "")
	);

}

void FAnimationEditor::FillExportAssetMenu(FMenuBuilder& MenuBuilder) const
{
	MenuBuilder.BeginSection("AnimationExport", LOCTEXT("ExportAssetMenuHeading", "Export"));
	{
		MenuBuilder.AddMenuEntry(FAnimationEditorCommands::Get().ExportToFBX_AnimData);
		MenuBuilder.AddMenuEntry(FAnimationEditorCommands::Get().ExportToFBX_PreviewMesh);
	}
	MenuBuilder.EndSection();
}

FRichCurve* FindOrAddCurve(UCurveTable* CurveTable, FName CurveName)
{
	FRichCurve* Curve = nullptr;

	// Grab existing curve (if present)
	FRichCurve** ExistingCurvePtr = CurveTable->RowMap.Find(CurveName);
	if (ExistingCurvePtr)
	{
		check(*ExistingCurvePtr);
		Curve = *ExistingCurvePtr;
	}
	// Or allocate new curve
	else
	{
		Curve = new FRichCurve();
		CurveTable->RowMap.Add(CurveName, Curve);
	}

	return Curve;
}

void FAnimationEditor::CopyCurveToSoundWave(const FAssetData& SoundWaveAssetData) const
{
	USoundWave* SoundWave = Cast<USoundWave>(SoundWaveAssetData.GetAsset());
	UAnimSequence* Sequence = Cast<UAnimSequence>(AnimationAsset);

	if (!SoundWave || !Sequence)
	{
		return;
	}

	// If no internal table, create one now
	if (!SoundWave->InternalCurves)
	{
		static const FName InternalCurveTableName("InternalCurveTable");
		SoundWave->Curves = NewObject<UCurveTable>(SoundWave, InternalCurveTableName);
		SoundWave->Curves->ClearFlags(RF_Public);
		SoundWave->Curves->SetFlags(SoundWave->Curves->GetFlags() | RF_Standalone | RF_Transactional);
		SoundWave->InternalCurves = SoundWave->Curves;
	}

	UCurveTable* CurveTable = SoundWave->InternalCurves;

	// iterate over curves in anim data
	const int32 NumCurves = Sequence->RawCurveData.FloatCurves.Num();
	for (int32 CurveIdx = 0; CurveIdx < NumCurves; CurveIdx++)
	{
		FFloatCurve& AnimCurve = Sequence->RawCurveData.FloatCurves[CurveIdx];

		FRichCurve* Curve = FindOrAddCurve(CurveTable, AnimCurve.Name.DisplayName);
		*Curve = AnimCurve.FloatCurve; // copy data
	}

	// we will need to add a curve to tell us the time we want to start playing audio
	float PreRollTime = 0.f;
	static const FName AudioCurveName("Audio");
	FRichCurve* AudioCurve = FindOrAddCurve(CurveTable, AudioCurveName);
	AudioCurve->Reset();
	AudioCurve->AddKey(PreRollTime, 1.0f);

	// Mark dirty after 
	SoundWave->MarkPackageDirty();

	FNotificationInfo Notification(FText::Format(LOCTEXT("AddedClassSuccessNotification", "Copied curves to {0}"),  FText::FromString(SoundWave->GetName())));
	FSlateNotificationManager::Get().AddNotification(Notification);

	// Close menu after picking sound
	FSlateApplication::Get().DismissAllMenus();
}

void FAnimationEditor::CreateAnimation(const TArray<UObject*> NewAssets, const EPoseSourceOption Option)
{
	bool bResult = true;
	if (NewAssets.Num() > 0)
	{
		USkeletalMeshComponent* MeshComponent = PersonaToolkit->GetPreviewMeshComponent();
		UAnimSequence* Sequence = Cast<UAnimSequence>(AnimationAsset);

		for (auto NewAsset : NewAssets)
		{
			UAnimSequence* NewAnimSequence = Cast<UAnimSequence>(NewAsset);
			if (NewAnimSequence)
			{
				switch (Option)
				{
				case EPoseSourceOption::ReferencePose:
					bResult &= NewAnimSequence->CreateAnimation(MeshComponent->SkeletalMesh);
					break;
				case EPoseSourceOption::CurrentPose:
					bResult &= NewAnimSequence->CreateAnimation(MeshComponent);
					break;
				case EPoseSourceOption::CurrentAnimation_AnimData:
					bResult &= NewAnimSequence->CreateAnimation(Sequence);
					break;
				case EPoseSourceOption::CurrentAnimation_PreviewMesh:
					bResult &= RecordMeshToAnimation(MeshComponent, NewAnimSequence);
					break;
				default: 
					ensure(false);
					break;
				}
			}
		}

		if (bResult)
		{
			HandleAssetCreated(NewAssets);

			// if it created based on current mesh component, 
			if (Option == EPoseSourceOption::CurrentPose)
			{
				UDebugSkelMeshComponent* PreviewMeshComponent = PersonaToolkit->GetPreviewMeshComponent();
				if (PreviewMeshComponent && PreviewMeshComponent->PreviewInstance)
				{
					PreviewMeshComponent->PreviewInstance->ResetModifiedBone();
				}
			}
		}
	}
}

void FAnimationEditor::CreatePoseAsset(const TArray<UObject*> NewAssets, const EPoseSourceOption Option)
{
	bool bResult = false;
	if (NewAssets.Num() > 0)
	{
		UDebugSkelMeshComponent* PreviewComponent = PersonaToolkit->GetPreviewMeshComponent();
		UAnimSequence* Sequence = Cast<UAnimSequence>(AnimationAsset);

		for (auto NewAsset : NewAssets)
		{
			UPoseAsset* NewPoseAsset = Cast<UPoseAsset>(NewAsset);
			if (NewPoseAsset)
			{
				switch (Option)
				{
				case EPoseSourceOption::CurrentPose:
					NewPoseAsset->AddOrUpdatePoseWithUniqueName(PreviewComponent);
					bResult = true;
					break;
				case EPoseSourceOption::CurrentAnimation_AnimData:
					NewPoseAsset->CreatePoseFromAnimation(Sequence);
					bResult = true;
					break;
				default:
					ensure(false);
					bResult = false; 
					break;
				}

			}
		}

		// if it contains error, warn them
		if (bResult)
		{
			HandleAssetCreated(NewAssets);

			// if it created based on current mesh component, 
			if (Option == EPoseSourceOption::CurrentPose)
			{
				PreviewComponent->PreviewInstance->ResetModifiedBone();
			}
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToCreateAsset", "Failed to create asset"));
		}
	}
}

void FAnimationEditor::HandleAssetCreated(const TArray<UObject*> NewAssets)
{
	if (NewAssets.Num() > 0)
	{
		FAssetRegistryModule::AssetCreated(NewAssets[0]);

		if (UAnimationAsset* NewAnimAsset = Cast<UAnimationAsset>(NewAssets[0]))
		{
			OpenNewAnimationDocumentTab(NewAnimAsset);
		}
		else
		{
			// if not, we forward to asset manager to open the asset for us
			// this is path for animation blueprint
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
			TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(NewAssets[0]->GetClass());
			if (AssetTypeActions.IsValid())
			{
				AssetTypeActions.Pin()->OpenAssetEditor(NewAssets);
			}
		}
	}
}

void FAnimationEditor::ConditionalRefreshEditor(UObject* InObject)
{
	bool bInterestingAsset = true;

	if (InObject != GetPersonaToolkit()->GetSkeleton() && InObject != GetPersonaToolkit()->GetSkeleton()->GetPreviewMesh() && Cast<UAnimationAsset>(InObject) != AnimationAsset)
	{
		bInterestingAsset = false;
	}

	// Check that we aren't a montage that uses an incoming animation
	if (UAnimMontage* Montage = Cast<UAnimMontage>(AnimationAsset))
	{
		for (FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
		{
			if (bInterestingAsset)
			{
				break;
			}

			for (FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
			{
				if (Segment.AnimReference == InObject)
				{
					bInterestingAsset = true;
					break;
				}
			}
		}
	}

	if (bInterestingAsset)
	{
		GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
		OpenNewAnimationDocumentTab(Cast<UAnimationAsset>(InObject));
	}
}

void FAnimationEditor::HandlePostReimport(UObject* InObject, bool bSuccess)
{
	if (bSuccess)
	{
		ConditionalRefreshEditor(InObject);
	}
}

void FAnimationEditor::HandlePostImport(UFactory* InFactory, UObject* InObject)
{
	ConditionalRefreshEditor(InObject);
}

void FAnimationEditor::HandleAnimationSequenceBrowserCreated(const TSharedRef<IAnimationSequenceBrowser>& InSequenceBrowser)
{
	SequenceBrowser = InSequenceBrowser;
}

bool FAnimationEditor::RecordMeshToAnimation(USkeletalMeshComponent* PreviewComponent, UAnimSequence* NewAsset) const
{
	return SequenceRecorderUtils::RecordSingleNodeInstanceToAnimation(PreviewComponent, NewAsset);
}

#undef LOCTEXT_NAMESPACE
