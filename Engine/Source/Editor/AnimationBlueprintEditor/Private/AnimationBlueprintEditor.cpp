// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "AnimationBlueprintEditor.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "EditorReimportHandler.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "AssetData.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimBlueprint.h"
#include "Editor.h"
#include "IDetailsView.h"
#include "IAnimationBlueprintEditorModule.h"
#include "AnimationBlueprintEditorModule.h"

#include "BlueprintEditorTabs.h"
#include "SKismetInspector.h"


#include "EdGraphUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/DebuggerCommands.h"

#include "AnimationBlueprintEditorMode.h"

#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_BlendListByInt.h"
#include "AnimGraphNode_BlendSpaceEvaluator.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimGraphNode_PoseByName.h"
#include "AnimGraphNode_PoseBlendNode.h"
#include "AnimGraphNode_MultiWayBlend.h"

#include "Animation/AnimNotifies/AnimNotifyState.h"

#include "AnimPreviewInstance.h"


#include "AnimationEditorUtils.h"
#include "Framework/Commands/GenericCommands.h"

#include "SSingleObjectDetailsPanel.h"

#include "IPersonaToolkit.h"
#include "ISkeletonTree.h"
#include "ISkeletonEditorModule.h"
#include "SBlueprintEditorToolbar.h"
#include "PersonaModule.h"
#include "IPersonaPreviewScene.h"
#include "IPersonaEditorModeManager.h"
#include "AnimationGraph.h"
#include "IAssetFamily.h"
#include "PersonaCommonCommands.h"
#include "AnimGraphCommands.h"

#include "AnimGraphNode_AimOffsetLookAt.h"
#include "AnimGraphNode_RotationOffsetBlendSpace.h"
#include "Algo/Transform.h"
#include "ISkeletonTreeItem.h"

#define LOCTEXT_NAMESPACE "AnimationBlueprintEditor"

const FName AnimationBlueprintEditorAppName(TEXT("AnimationBlueprintEditorApp"));

const FName FAnimationBlueprintEditorModes::AnimationBlueprintEditorMode("GraphName");	// For backwards compatibility we keep the old mode name here

namespace AnimationBlueprintEditorTabs
{
	const FName DetailsTab(TEXT("DetailsTab"));
	const FName SkeletonTreeTab(TEXT("SkeletonTreeView"));
	const FName ViewportTab(TEXT("Viewport"));
	const FName AdvancedPreviewTab(TEXT("AdvancedPreviewTab"));
	const FName AssetBrowserTab(TEXT("SequenceBrowser"));
	const FName AnimBlueprintPreviewEditorTab(TEXT("AnimBlueprintPreviewEditor"));
	const FName AssetOverridesTab(TEXT("AnimBlueprintParentPlayerEditor"));
	const FName SlotNamesTab(TEXT("SkeletonSlotNames"));
	const FName CurveNamesTab(TEXT("AnimCurveViewerTab"));
};

/////////////////////////////////////////////////////
// SAnimBlueprintPreviewPropertyEditor

class SAnimBlueprintPreviewPropertyEditor : public SSingleObjectDetailsPanel
{
public:
	SLATE_BEGIN_ARGS(SAnimBlueprintPreviewPropertyEditor) {}
	SLATE_END_ARGS()

private:
	// Pointer back to owning Persona editor instance (the keeper of state)
	TWeakPtr<FAnimationBlueprintEditor> AnimationBlueprintEditorPtr;
public:
	void Construct(const FArguments& InArgs, TSharedPtr<FAnimationBlueprintEditor> InAnimationBlueprintEditor)
	{
		AnimationBlueprintEditorPtr = InAnimationBlueprintEditor;

		SSingleObjectDetailsPanel::Construct(SSingleObjectDetailsPanel::FArguments().HostCommandList(InAnimationBlueprintEditor->GetToolkitCommands()).HostTabManager(InAnimationBlueprintEditor->GetTabManager()), /*bAutomaticallyObserveViaGetObjectToObserve*/ true, /*bAllowSearch*/ true);

		PropertyView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateStatic([] { return !GIntraFrameDebuggingGameThread; }));
	}

	// SSingleObjectDetailsPanel interface
	virtual UObject* GetObjectToObserve() const override
	{
		if (UDebugSkelMeshComponent* PreviewMeshComponent = AnimationBlueprintEditorPtr.Pin()->GetPersonaToolkit()->GetPreviewMeshComponent())
		{
			return PreviewMeshComponent->GetAnimInstance();
		}

		return nullptr;
	}

	virtual TSharedRef<SWidget> PopulateSlot(TSharedRef<SWidget> PropertyEditorWidget) override
	{
		return SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Persona.PreviewPropertiesWarning"))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AnimBlueprintEditPreviewText", "Changes to preview options are not saved in the asset."))
					.Font(FEditorStyle::GetFontStyle("PropertyWindow.NormalFont"))
					.ShadowColorAndOpacity(FLinearColor::Black.CopyWithNewOpacity(0.3f))
					.ShadowOffset(FVector2D::UnitVector)
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1)
			[
				PropertyEditorWidget
			];
	}
	// End of SSingleObjectDetailsPanel interface
};

/////////////////////////////////////////////////////
// FAnimationBlueprintEditor

FAnimationBlueprintEditor::FAnimationBlueprintEditor()
	: PersonaMeshDetailLayout(NULL)
{
	GEditor->OnBlueprintPreCompile().AddRaw(this, &FAnimationBlueprintEditor::OnBlueprintPreCompile);
}

FAnimationBlueprintEditor::~FAnimationBlueprintEditor()
{
	GEditor->OnBlueprintPreCompile().RemoveAll(this);

	FEditorDelegates::OnAssetPostImport.RemoveAll(this);
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);

	// NOTE: Any tabs that we still have hanging out when destroyed will be cleaned up by FBaseToolkit's destructor
}

UAnimBlueprint* FAnimationBlueprintEditor::GetAnimBlueprint() const
{
	return Cast<UAnimBlueprint>(GetBlueprintObj());
}

void FAnimationBlueprintEditor::ExtendMenu()
{
	if(MenuExtender.IsValid())
	{
		RemoveMenuExtender(MenuExtender);
		MenuExtender.Reset();
	}

	MenuExtender = MakeShareable(new FExtender);

	UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();
	if(AnimBlueprint)
	{
		TSharedPtr<FExtender> AnimBPMenuExtender = MakeShareable(new FExtender);
		FKismet2Menu::SetupBlueprintEditorMenu(AnimBPMenuExtender, *this);
		AddMenuExtender(AnimBPMenuExtender);
	}

	AddMenuExtender(MenuExtender);

	// add extensible menu if exists
	FAnimationBlueprintEditorModule& AnimationBlueprintEditorModule = FModuleManager::LoadModuleChecked<FAnimationBlueprintEditorModule>("AnimationBlueprintEditor");
	AddMenuExtender(AnimationBlueprintEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

void FAnimationBlueprintEditor::InitAnimationBlueprintEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UAnimBlueprint* InAnimBlueprint)
{
	if (!Toolbar.IsValid())
	{
		Toolbar = MakeShareable(new FBlueprintEditorToolbar(SharedThis(this)));
	}

	GetToolkitCommands()->Append(FPlayWorldCommands::GlobalPlayWorldActions.ToSharedRef());

	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");
	PersonaToolkit = PersonaModule.CreatePersonaToolkit(InAnimBlueprint);

	PersonaToolkit->GetPreviewScene()->SetDefaultAnimationMode(EPreviewSceneDefaultAnimationMode::AnimationBlueprint);
	PersonaToolkit->GetPreviewScene()->RegisterOnPreviewMeshChanged(FOnPreviewMeshChanged::CreateSP(this, &FAnimationBlueprintEditor::HandlePreviewMeshChanged));

	TSharedRef<IAssetFamily> AssetFamily = PersonaModule.CreatePersonaAssetFamily(InAnimBlueprint);
	AssetFamily->RecordAssetOpened(FAssetData(InAnimBlueprint));

	// create the skeleton tree
	FSkeletonTreeArgs SkeletonTreeArgs;
	SkeletonTreeArgs.OnSelectionChanged = FOnSkeletonTreeSelectionChanged::CreateSP(this, &FAnimationBlueprintEditor::HandleSelectionChanged);
	SkeletonTreeArgs.PreviewScene = GetPreviewScene();

	ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	SkeletonTree = SkeletonEditorModule.CreateSkeletonTree(PersonaToolkit->GetSkeleton(), SkeletonTreeArgs);

	// Build up a list of objects being edited in this asset editor
	TArray<UObject*> ObjectsBeingEdited;
	ObjectsBeingEdited.Add(InAnimBlueprint);

	// Initialize the asset editor and spawn tabs
	const TSharedRef<FTabManager::FLayout> DummyLayout = FTabManager::NewLayout("NullLayout")->AddArea(FTabManager::NewPrimaryArea());
	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	InitAssetEditor(Mode, InitToolkitHost, AnimationBlueprintEditorAppName, DummyLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsBeingEdited);

	TArray<UBlueprint*> AnimBlueprints;
	AnimBlueprints.Add(InAnimBlueprint);

	CommonInitialization(AnimBlueprints);

	BindCommands();

	AddApplicationMode(
		FAnimationBlueprintEditorModes::AnimationBlueprintEditorMode,
		MakeShareable(new FAnimationBlueprintEditorMode(SharedThis(this))));

	UDebugSkelMeshComponent* PreviewMeshComponent = PersonaToolkit->GetPreviewMeshComponent();
	UAnimBlueprint* AnimBlueprint = PersonaToolkit->GetAnimBlueprint();
	PreviewMeshComponent->SetAnimInstanceClass(AnimBlueprint ? AnimBlueprint->GeneratedClass : NULL);

	// Make sure the object being debugged is the preview instance
	AnimBlueprint->SetObjectBeingDebugged(PreviewMeshComponent->GetAnimInstance());

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// Activate the initial mode (which will populate with a real layout)
	SetCurrentMode(FAnimationBlueprintEditorModes::AnimationBlueprintEditorMode);

	// Post-layout initialization
	PostLayoutBlueprintEditorInitialization();

	// register customization of Slot node for this Animation Blueprint Editor
	// this is so that you can open the manage window per Animation Blueprint Editor
	PersonaModule.CustomizeSlotNodeDetails(Inspector->GetPropertyView().ToSharedRef(), FOnInvokeTab::CreateSP(this, &FAssetEditorToolkit::InvokeTab));
}

void FAnimationBlueprintEditor::BindCommands()
{
	GetToolkitCommands()->MapAction(FPersonaCommonCommands::Get().TogglePlay,
		FExecuteAction::CreateRaw(&GetPersonaToolkit()->GetPreviewScene().Get(), &IPersonaPreviewScene::TogglePlayback));
}

void FAnimationBlueprintEditor::ExtendToolbar()
{
	// If the ToolbarExtender is valid, remove it before rebuilding it
	if(ToolbarExtender.IsValid())
	{
		RemoveToolbarExtender(ToolbarExtender);
		ToolbarExtender.Reset();
	}

	ToolbarExtender = MakeShareable(new FExtender);

	AddToolbarExtender(ToolbarExtender);

	FAnimationBlueprintEditorModule& AnimationBlueprintEditorModule = FModuleManager::LoadModuleChecked<FAnimationBlueprintEditorModule>("AnimationBlueprintEditor");
	AddToolbarExtender(AnimationBlueprintEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	TArray<IAnimationBlueprintEditorModule::FAnimationBlueprintEditorToolbarExtender> ToolbarExtenderDelegates = AnimationBlueprintEditorModule.GetAllAnimationBlueprintEditorToolbarExtenders();

	for (auto& ToolbarExtenderDelegate : ToolbarExtenderDelegates)
	{
		if(ToolbarExtenderDelegate.IsBound())
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
		PersonaModule.AddCommonToolbarExtensions(ParentToolbarBuilder, PersonaToolkit.ToSharedRef());

		TSharedRef<class IAssetFamily> AssetFamily = PersonaModule.CreatePersonaAssetFamily(GetBlueprintObj());
		AddToolbarWidget(PersonaModule.CreateAssetFamilyShortcutWidget(SharedThis(this), AssetFamily));
	}
	));
}

UBlueprint* FAnimationBlueprintEditor::GetBlueprintObj() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	for (int32 i = 0; i < EditingObjs.Num(); ++i)
	{
		if (EditingObjs[i]->IsA<UAnimBlueprint>()) {return (UBlueprint*)EditingObjs[i];}
	}
	return nullptr;
}

void FAnimationBlueprintEditor::SetDetailObjects(const TArray<UObject*>& InObjects)
{
	Inspector->ShowDetailsForObjects(InObjects);
}

void FAnimationBlueprintEditor::SetDetailObject(UObject* Obj)
{
	TArray<UObject*> Objects;
	if (Obj)
	{
		Objects.Add(Obj);
	}
	SetDetailObjects(Objects);
}

/** Called when graph editor focus is changed */
void FAnimationBlueprintEditor::OnGraphEditorFocused(const TSharedRef<class SGraphEditor>& InGraphEditor)
{
	// in the future, depending on which graph editor is this will act different
	FBlueprintEditor::OnGraphEditorFocused(InGraphEditor);

	// install callback to allow us to propagate pin default changes live to the preview
	UAnimationGraph* AnimationGraph = Cast<UAnimationGraph>(InGraphEditor->GetCurrentGraph());
	if (AnimationGraph)
	{
		OnPinDefaultValueChangedHandle = AnimationGraph->OnPinDefaultValueChanged.Add(FOnPinDefaultValueChanged::FDelegate::CreateSP(this, &FAnimationBlueprintEditor::HandlePinDefaultValueChanged));
	}
}

void FAnimationBlueprintEditor::OnGraphEditorBackgrounded(const TSharedRef<SGraphEditor>& InGraphEditor)
{
	FBlueprintEditor::OnGraphEditorBackgrounded(InGraphEditor);

	UAnimationGraph* AnimationGraph = Cast<UAnimationGraph>(InGraphEditor->GetCurrentGraph());
	if (AnimationGraph)
	{
		AnimationGraph->OnPinDefaultValueChanged.Remove(OnPinDefaultValueChangedHandle);
	}
}

/** Create Default Tabs **/
void FAnimationBlueprintEditor::CreateDefaultCommands() 
{
	if (GetBlueprintObj())
	{
		FBlueprintEditor::CreateDefaultCommands();
	}
	else
	{
		ToolkitCommands->MapAction( FGenericCommands::Get().Undo, 
			FExecuteAction::CreateSP( this, &FAnimationBlueprintEditor::UndoAction ));
		ToolkitCommands->MapAction( FGenericCommands::Get().Redo, 
			FExecuteAction::CreateSP( this, &FAnimationBlueprintEditor::RedoAction ));
	}
}

void FAnimationBlueprintEditor::OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList)
{
	GraphEditorCommandsList->MapAction(FAnimGraphCommands::Get().TogglePoseWatch,
		FExecuteAction::CreateSP(this, &FAnimationBlueprintEditor::OnTogglePoseWatch));
}


void FAnimationBlueprintEditor::OnAddPosePin()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	if (SelectedNodes.Num() == 1)
	{
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UObject* Node = *NodeIt;

			if (UAnimGraphNode_BlendListByInt* BlendNode = Cast<UAnimGraphNode_BlendListByInt>(Node))
			{
				BlendNode->AddPinToBlendList();
				break;
			}
			else if (UAnimGraphNode_LayeredBoneBlend* FilterNode = Cast<UAnimGraphNode_LayeredBoneBlend>(Node))
			{
				FilterNode->AddPinToBlendByFilter();
				break;
			}
			else if (UAnimGraphNode_MultiWayBlend* MultiBlendNode = Cast<UAnimGraphNode_MultiWayBlend>(Node))
			{
				MultiBlendNode->AddPinToBlendNode();
				break;
			}
		}
	}
}

bool FAnimationBlueprintEditor::CanAddPosePin() const
{
	return true;
}

void FAnimationBlueprintEditor::OnRemovePosePin()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	UAnimGraphNode_BlendListByInt* BlendListIntNode = nullptr;
	UAnimGraphNode_LayeredBoneBlend* BlendByFilterNode = nullptr;
	UAnimGraphNode_MultiWayBlend* BlendByMultiway = nullptr;

	if (SelectedNodes.Num() == 1)
	{
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			if (UAnimGraphNode_BlendListByInt* BlendNode = Cast<UAnimGraphNode_BlendListByInt>(*NodeIt))
			{
				BlendListIntNode = BlendNode;
				break;
			}
			else if (UAnimGraphNode_LayeredBoneBlend* LayeredBlendNode = Cast<UAnimGraphNode_LayeredBoneBlend>(*NodeIt))
			{
				BlendByFilterNode = LayeredBlendNode;
				break;
			}		
			else if (UAnimGraphNode_MultiWayBlend* MultiwayBlendNode = Cast<UAnimGraphNode_MultiWayBlend>(*NodeIt))
			{
				BlendByMultiway = MultiwayBlendNode;
				break;
			}
		}
	}


	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		// @fixme: I think we can make blendlistbase to have common functionality
		// and each can implement the common function, but for now, we separate them
		// each implement their menu, so we still can use listbase as the root
		if (BlendListIntNode)
		{
			// make sure we at least have BlendListNode selected
			UEdGraphPin* SelectedPin = FocusedGraphEd->GetGraphPinForMenu();

			BlendListIntNode->RemovePinFromBlendList(SelectedPin);

			// Update the graph so that the node will be refreshed
			FocusedGraphEd->NotifyGraphChanged();
		}

		if (BlendByFilterNode)
		{
			// make sure we at least have BlendListNode selected
			UEdGraphPin* SelectedPin = FocusedGraphEd->GetGraphPinForMenu();

			BlendByFilterNode->RemovePinFromBlendByFilter(SelectedPin);

			// Update the graph so that the node will be refreshed
			FocusedGraphEd->NotifyGraphChanged();
		}

		if (BlendByMultiway)
		{
			// make sure we at least have BlendListNode selected
			UEdGraphPin* SelectedPin = FocusedGraphEd->GetGraphPinForMenu();

			BlendByMultiway->RemovePinFromBlendNode(SelectedPin);

			// Update the graph so that the node will be refreshed
			FocusedGraphEd->NotifyGraphChanged();
		}
	}
}

void FAnimationBlueprintEditor::OnTogglePoseWatch()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	UAnimBlueprint* AnimBP = GetAnimBlueprint();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		if (UAnimGraphNode_Base* SelectedNode = Cast<UAnimGraphNode_Base>(*NodeIt))
		{
			UPoseWatch* PoseWatch = AnimationEditorUtils::FindPoseWatchForNode(SelectedNode, AnimBP);
			if (PoseWatch)
			{
				AnimationEditorUtils::RemovePoseWatch(PoseWatch, AnimBP);
			}
			else
			{
				AnimationEditorUtils::MakePoseWatchForNode(AnimBP, SelectedNode, FColor::Red);
			}
		}
	}
}

void FAnimationBlueprintEditor::OnConvertToSequenceEvaluator()
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	if (SelectedNodes.Num() > 0)
	{
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			UAnimGraphNode_SequencePlayer* OldNode = Cast<UAnimGraphNode_SequencePlayer>(*NodeIter);

			// see if sequence player
			if ( OldNode && OldNode->Node.Sequence )
			{
				//const FScopedTransaction Transaction( LOCTEXT("ConvertToSequenceEvaluator", "Convert to Single Frame Animation") );

				// convert to sequence evaluator
				UEdGraph* TargetGraph = OldNode->GetGraph();
				// create new evaluator
				FGraphNodeCreator<UAnimGraphNode_SequenceEvaluator> NodeCreator(*TargetGraph);
				UAnimGraphNode_SequenceEvaluator* NewNode = NodeCreator.CreateNode();
				NewNode->Node.Sequence = OldNode->Node.Sequence;
				NodeCreator.Finalize();

				// get default data from old node to new node
				FEdGraphUtilities::CopyCommonState(OldNode, NewNode);

				UEdGraphPin* OldPosePin = OldNode->FindPin(TEXT("Pose"));
				UEdGraphPin* NewPosePin = NewNode->FindPin(TEXT("Pose"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->MovePersistentDataFromOldPin(*OldPosePin);
				}

				// remove from selection and from graph
				NodeIter.RemoveCurrent();
				TargetGraph->RemoveNode(OldNode);

				NewNode->Modify();
			}
		}

		// @todo fixme: below code doesn't work
		// because of SetAndCenterObject kicks in after new node is added
		// will need to disable that first
		TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();

		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();
		// It's possible to leave invalid objects in the selection set if they get GC'd, so clear it out
		FocusedGraphEd->ClearSelectionSet();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetAnimBlueprint());
	}
}

void FAnimationBlueprintEditor::OnConvertToSequencePlayer()
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	if (SelectedNodes.Num() > 0)
	{
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			UAnimGraphNode_SequenceEvaluator* OldNode = Cast<UAnimGraphNode_SequenceEvaluator>(*NodeIter);

			// see if sequence player
			if ( OldNode && OldNode->Node.Sequence )
			{
				//const FScopedTransaction Transaction( LOCTEXT("ConvertToSequenceEvaluator", "Convert to Single Frame Animation") );
				// convert to sequence player
				UEdGraph* TargetGraph = OldNode->GetGraph();
				// create new player
				FGraphNodeCreator<UAnimGraphNode_SequencePlayer> NodeCreator(*TargetGraph);
				UAnimGraphNode_SequencePlayer* NewNode = NodeCreator.CreateNode();
				NewNode->Node.Sequence = OldNode->Node.Sequence;
				NodeCreator.Finalize();

				// get default data from old node to new node
				FEdGraphUtilities::CopyCommonState(OldNode, NewNode);

				UEdGraphPin* OldPosePin = OldNode->FindPin(TEXT("Pose"));
				UEdGraphPin* NewPosePin = NewNode->FindPin(TEXT("Pose"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->MovePersistentDataFromOldPin(*OldPosePin);
				}

				// remove from selection and from graph
				NodeIter.RemoveCurrent();
				TargetGraph->RemoveNode(OldNode);

				NewNode->Modify();
			}
		}

		// @todo fixme: below code doesn't work
		// because of SetAndCenterObject kicks in after new node is added
		// will need to disable that first
		TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();

		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();
		// It's possible to leave invalid objects in the selection set if they get GC'd, so clear it out
		FocusedGraphEd->ClearSelectionSet();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetAnimBlueprint());
	}
}

void FAnimationBlueprintEditor::OnConvertToBlendSpaceEvaluator() 
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	if (SelectedNodes.Num() > 0)
	{
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			UAnimGraphNode_BlendSpacePlayer* OldNode = Cast<UAnimGraphNode_BlendSpacePlayer>(*NodeIter);

			// see if sequence player
			if ( OldNode && OldNode->Node.BlendSpace )
			{
				//const FScopedTransaction Transaction( LOCTEXT("ConvertToSequenceEvaluator", "Convert to Single Frame Animation") );

				// convert to sequence evaluator
				UEdGraph* TargetGraph = OldNode->GetGraph();
				// create new evaluator
				FGraphNodeCreator<UAnimGraphNode_BlendSpaceEvaluator> NodeCreator(*TargetGraph);
				UAnimGraphNode_BlendSpaceEvaluator* NewNode = NodeCreator.CreateNode();
				NewNode->Node.BlendSpace = OldNode->Node.BlendSpace;
				NodeCreator.Finalize();

				// get default data from old node to new node
				FEdGraphUtilities::CopyCommonState(OldNode, NewNode);

				UEdGraphPin* OldPosePin = OldNode->FindPin(TEXT("X"));
				UEdGraphPin* NewPosePin = NewNode->FindPin(TEXT("X"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->MovePersistentDataFromOldPin(*OldPosePin);
				}

				OldPosePin = OldNode->FindPin(TEXT("Y"));
				NewPosePin = NewNode->FindPin(TEXT("Y"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->MovePersistentDataFromOldPin(*OldPosePin);
				}


				OldPosePin = OldNode->FindPin(TEXT("Pose"));
				NewPosePin = NewNode->FindPin(TEXT("Pose"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->MovePersistentDataFromOldPin(*OldPosePin);
				}

				// remove from selection and from graph
				NodeIter.RemoveCurrent();
				TargetGraph->RemoveNode(OldNode);

				NewNode->Modify();
			}
		}

		// @todo fixme: below code doesn't work
		// because of SetAndCenterObject kicks in after new node is added
		// will need to disable that first
		TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();

		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();
		// It's possible to leave invalid objects in the selection set if they get GC'd, so clear it out
		FocusedGraphEd->ClearSelectionSet();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetAnimBlueprint());
	}
}
void FAnimationBlueprintEditor::OnConvertToBlendSpacePlayer() 
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	if (SelectedNodes.Num() > 0)
	{
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			UAnimGraphNode_BlendSpaceEvaluator* OldNode = Cast<UAnimGraphNode_BlendSpaceEvaluator>(*NodeIter);

			// see if sequence player
			if ( OldNode && OldNode->Node.BlendSpace )
			{
				//const FScopedTransaction Transaction( LOCTEXT("ConvertToSequenceEvaluator", "Convert to Single Frame Animation") );
				// convert to sequence player
				UEdGraph* TargetGraph = OldNode->GetGraph();
				// create new player
				FGraphNodeCreator<UAnimGraphNode_BlendSpacePlayer> NodeCreator(*TargetGraph);
				UAnimGraphNode_BlendSpacePlayer* NewNode = NodeCreator.CreateNode();
				NewNode->Node.BlendSpace = OldNode->Node.BlendSpace;
				NodeCreator.Finalize();

				// get default data from old node to new node
				FEdGraphUtilities::CopyCommonState(OldNode, NewNode);

				UEdGraphPin* OldPosePin = OldNode->FindPin(TEXT("X"));
				UEdGraphPin* NewPosePin = NewNode->FindPin(TEXT("X"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->MovePersistentDataFromOldPin(*OldPosePin);
				}

				OldPosePin = OldNode->FindPin(TEXT("Y"));
				NewPosePin = NewNode->FindPin(TEXT("Y"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->MovePersistentDataFromOldPin(*OldPosePin);
				}


				OldPosePin = OldNode->FindPin(TEXT("Pose"));
				NewPosePin = NewNode->FindPin(TEXT("Pose"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->MovePersistentDataFromOldPin(*OldPosePin);
				}

				// remove from selection and from graph
				NodeIter.RemoveCurrent();
				TargetGraph->RemoveNode(OldNode);

				NewNode->Modify();
			}
		}

		// @todo fixme: below code doesn't work
		// because of SetAndCenterObject kicks in after new node is added
		// will need to disable that first
		TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();
		// It's possible to leave invalid objects in the selection set if they get GC'd, so clear it out
		FocusedGraphEd->ClearSelectionSet();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetAnimBlueprint());
	}
}

void FAnimationBlueprintEditor::OnConvertToPoseBlender()
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	if (SelectedNodes.Num() > 0)
	{
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			UAnimGraphNode_PoseByName* OldNode = Cast<UAnimGraphNode_PoseByName>(*NodeIter);

			// see if sequence player
			if (OldNode && OldNode->Node.PoseAsset)
			{
				// convert to sequence player
				UEdGraph* TargetGraph = OldNode->GetGraph();
				// create new player
				FGraphNodeCreator<UAnimGraphNode_PoseBlendNode> NodeCreator(*TargetGraph);
				UAnimGraphNode_PoseBlendNode* NewNode = NodeCreator.CreateNode();
				NewNode->Node.PoseAsset = OldNode->Node.PoseAsset;
				NodeCreator.Finalize();

				// get default data from old node to new node
				FEdGraphUtilities::CopyCommonState(OldNode, NewNode);

				UEdGraphPin* OldPosePin = OldNode->FindPin(TEXT("Pose"));
				UEdGraphPin* NewPosePin = NewNode->FindPin(TEXT("Pose"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->MovePersistentDataFromOldPin(*OldPosePin);
				}

				// remove from selection and from graph
				NodeIter.RemoveCurrent();
				TargetGraph->RemoveNode(OldNode);

				NewNode->Modify();
			}
		}

		// @todo fixme: below code doesn't work
		// because of SetAndCenterObject kicks in after new node is added
		// will need to disable that first
		TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();

		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();
		// It's possible to leave invalid objects in the selection set if they get GC'd, so clear it out
		FocusedGraphEd->ClearSelectionSet();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetAnimBlueprint());
	}
}

void FAnimationBlueprintEditor::OnConvertToPoseByName()
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	if (SelectedNodes.Num() > 0)
	{
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			UAnimGraphNode_PoseBlendNode* OldNode = Cast<UAnimGraphNode_PoseBlendNode>(*NodeIter);

			// see if sequence player
			if (OldNode && OldNode->Node.PoseAsset)
			{
				//const FScopedTransaction Transaction( LOCTEXT("ConvertToSequenceEvaluator", "Convert to Single Frame Animation") );
				// convert to sequence player
				UEdGraph* TargetGraph = OldNode->GetGraph();
				// create new player
				FGraphNodeCreator<UAnimGraphNode_PoseByName> NodeCreator(*TargetGraph);
				UAnimGraphNode_PoseByName* NewNode = NodeCreator.CreateNode();
				NewNode->Node.PoseAsset = OldNode->Node.PoseAsset;
				NodeCreator.Finalize();

				// get default data from old node to new node
				FEdGraphUtilities::CopyCommonState(OldNode, NewNode);

				UEdGraphPin* OldPosePin = OldNode->FindPin(TEXT("Pose"));
				UEdGraphPin* NewPosePin = NewNode->FindPin(TEXT("Pose"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->MovePersistentDataFromOldPin(*OldPosePin);
				}

				// remove from selection and from graph
				NodeIter.RemoveCurrent();
				TargetGraph->RemoveNode(OldNode);

				NewNode->Modify();
			}
		}

		// @todo fixme: below code doesn't work
		// because of SetAndCenterObject kicks in after new node is added
		// will need to disable that first
		TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();

		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();
		// It's possible to leave invalid objects in the selection set if they get GC'd, so clear it out
		FocusedGraphEd->ClearSelectionSet();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetAnimBlueprint());
	}
}

void FAnimationBlueprintEditor::OnConvertToAimOffsetLookAt()
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	if (SelectedNodes.Num() > 0)
	{
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			UAnimGraphNode_RotationOffsetBlendSpace* OldNode = Cast<UAnimGraphNode_RotationOffsetBlendSpace>(*NodeIter);

			// see if sequence player
			if (OldNode && OldNode->Node.BlendSpace)
			{
				//const FScopedTransaction Transaction( LOCTEXT("ConvertToSequenceEvaluator", "Convert to Single Frame Animation") );

				// convert to sequence evaluator
				UEdGraph* TargetGraph = OldNode->GetGraph();
				// create new evaluator
				FGraphNodeCreator<UAnimGraphNode_AimOffsetLookAt> NodeCreator(*TargetGraph);
				UAnimGraphNode_AimOffsetLookAt* NewNode = NodeCreator.CreateNode();
				NewNode->Node.BlendSpace = OldNode->Node.BlendSpace;
				NodeCreator.Finalize();

				// get default data from old node to new node
				FEdGraphUtilities::CopyCommonState(OldNode, NewNode);

				UEdGraphPin* OldPosePin = OldNode->FindPin(TEXT("Pose"));
				UEdGraphPin* NewPosePin = NewNode->FindPin(TEXT("Pose"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->MovePersistentDataFromOldPin(*OldPosePin);
				}

				OldPosePin = OldNode->FindPin(TEXT("BasePose"));
				NewPosePin = NewNode->FindPin(TEXT("BasePose"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->MovePersistentDataFromOldPin(*OldPosePin);
				}

				// remove from selection and from graph
				NodeIter.RemoveCurrent();
				TargetGraph->RemoveNode(OldNode);

				NewNode->Modify();
			}
		}

		// @todo fixme: below code doesn't work
		// because of SetAndCenterObject kicks in after new node is added
		// will need to disable that first
		TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();

		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();
		// It's possible to leave invalid objects in the selection set if they get GC'd, so clear it out
		FocusedGraphEd->ClearSelectionSet();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetAnimBlueprint());
	}
}

void FAnimationBlueprintEditor::OnConvertToAimOffsetSimple()
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	if (SelectedNodes.Num() > 0)
	{
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			UAnimGraphNode_AimOffsetLookAt* OldNode = Cast<UAnimGraphNode_AimOffsetLookAt>(*NodeIter);

			// see if sequence player
			if (OldNode && OldNode->Node.BlendSpace)
			{
				//const FScopedTransaction Transaction( LOCTEXT("ConvertToSequenceEvaluator", "Convert to Single Frame Animation") );
				// convert to sequence player
				UEdGraph* TargetGraph = OldNode->GetGraph();
				// create new player
				FGraphNodeCreator<UAnimGraphNode_RotationOffsetBlendSpace> NodeCreator(*TargetGraph);
				UAnimGraphNode_RotationOffsetBlendSpace* NewNode = NodeCreator.CreateNode();
				NewNode->Node.BlendSpace = OldNode->Node.BlendSpace;
				NodeCreator.Finalize();

				// get default data from old node to new node
				FEdGraphUtilities::CopyCommonState(OldNode, NewNode);

				UEdGraphPin* OldPosePin = OldNode->FindPin(TEXT("Pose"));
				UEdGraphPin* NewPosePin = NewNode->FindPin(TEXT("Pose"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->MovePersistentDataFromOldPin(*OldPosePin);
				}

				OldPosePin = OldNode->FindPin(TEXT("BasePose"));
				NewPosePin = NewNode->FindPin(TEXT("BasePose"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->MovePersistentDataFromOldPin(*OldPosePin);
				}

				// remove from selection and from graph
				NodeIter.RemoveCurrent();
				TargetGraph->RemoveNode(OldNode);

				NewNode->Modify();
			}
		}

		// @todo fixme: below code doesn't work
		// because of SetAndCenterObject kicks in after new node is added
		// will need to disable that first
		TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();
		// It's possible to leave invalid objects in the selection set if they get GC'd, so clear it out
		FocusedGraphEd->ClearSelectionSet();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetAnimBlueprint());
	}
}

void FAnimationBlueprintEditor::OnOpenRelatedAsset()
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	
	EToolkitMode::Type Mode = EToolkitMode::Standalone;
	if (SelectedNodes.Num() > 0)
	{
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			if(UAnimGraphNode_Base* Node = Cast<UAnimGraphNode_Base>(*NodeIter))
			{
				UAnimationAsset* AnimAsset = Node->GetAnimationAsset();
				if(AnimAsset)
				{
					FAssetEditorManager::Get().OpenEditorForAsset(AnimAsset, Mode);
				}
			}
		}
	}
}

bool FAnimationBlueprintEditor::CanRemovePosePin() const
{
	return true;
}

void FAnimationBlueprintEditor::RecompileAnimBlueprintIfDirty()
{
	if (UBlueprint* Blueprint = GetBlueprintObj())
	{
		if (!Blueprint->IsUpToDate())
		{
			Compile();
		}
	}
}

void FAnimationBlueprintEditor::Compile()
{
	// Grab the currently debugged object, so we can re-set it below
	USkeletalMeshComponent* DebuggedMeshComponent = nullptr;
	if (UBlueprint* Blueprint = GetBlueprintObj())
	{
		UAnimInstance* CurrentDebugObject = Cast<UAnimInstance>(Blueprint->GetObjectBeingDebugged());
		if(CurrentDebugObject)
		{
			// Force close any asset editors that are using the AnimScriptInstance (such as the Property Matrix), the class will be garbage collected
			FAssetEditorManager::Get().CloseOtherEditors(CurrentDebugObject, nullptr);
			DebuggedMeshComponent = CurrentDebugObject->GetSkelMeshComponent();
		}
	}

	// Compile the blueprint
	FBlueprintEditor::Compile();

	if (DebuggedMeshComponent != nullptr)
	{
		if (DebuggedMeshComponent->GetAnimInstance() == nullptr)
		{
			// try reinitialize animation if it doesn't exist
			DebuggedMeshComponent->InitAnim(true);
		}

		GetBlueprintObj()->SetObjectBeingDebugged(DebuggedMeshComponent->GetAnimInstance());
	}

	// reset the selected skeletal control node
	SelectedAnimGraphNode.Reset();

	// if the user manipulated Pin values directly from the node, then should copy updated values to the internal node to retain data consistency
	OnPostCompile();
}

FName FAnimationBlueprintEditor::GetToolkitFName() const
{
	return FName("AnimationBlueprintEditor");
}

FText FAnimationBlueprintEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Animation Blueprint Editor");
}

FText FAnimationBlueprintEditor::GetToolkitToolTipText() const
{
	return FAssetEditorToolkit::GetToolTipTextForObject(GetBlueprintObj());
}

FString FAnimationBlueprintEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Animation Blueprint Editor ").ToString();
}


FLinearColor FAnimationBlueprintEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.5f, 0.25f, 0.35f, 0.5f );
}

void FAnimationBlueprintEditor::OnActiveTabChanged( TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated )
{
	if (!NewlyActivated.IsValid())
	{
		TArray<UObject*> ObjArray;
		Inspector->ShowDetailsForObjects(ObjArray);
	}
	else 
	{
		FBlueprintEditor::OnActiveTabChanged(PreviouslyActive, NewlyActivated);
	}
}

void FAnimationBlueprintEditor::SetPreviewMesh(USkeletalMesh* NewPreviewMesh)
{
	GetSkeletonTree()->SetSkeletalMesh(NewPreviewMesh);
}

void FAnimationBlueprintEditor::RefreshPreviewInstanceTrackCurves()
{
	// need to refresh the preview mesh
	UDebugSkelMeshComponent* PreviewMeshComponent = PersonaToolkit->GetPreviewMeshComponent();
	if(PreviewMeshComponent->PreviewInstance)
	{
		PreviewMeshComponent->PreviewInstance->RefreshCurveBoneControllers();
	}
}

void FAnimationBlueprintEditor::PostUndo(bool bSuccess)
{
	DocumentManager->CleanInvalidTabs();
	DocumentManager->RefreshAllTabs();

	FBlueprintEditor::PostUndo(bSuccess);

	// If we undid a node creation that caused us to clean up a tab/graph we need to refresh the UI state
	RefreshEditors();

	// PostUndo broadcast
	OnPostUndo.Broadcast();	

	RefreshPreviewInstanceTrackCurves();

	// clear up preview anim notify states
	// animnotify states are saved in AnimInstance
	// if those are undoed or redoed, they have to be 
	// cleared up, otherwise, they might have invalid data
	ClearupPreviewMeshAnimNotifyStates();

	OnPostCompile();
}

void FAnimationBlueprintEditor::ClearupPreviewMeshAnimNotifyStates()
{
	UDebugSkelMeshComponent* PreviewMeshComponent = PersonaToolkit->GetPreviewMeshComponent();
	if ( PreviewMeshComponent )
	{
		UAnimInstance* AnimInstanace = PreviewMeshComponent->GetAnimInstance();

		if (AnimInstanace)
		{
			// empty this because otherwise, it can have corrupted data
			// this will cause state to be interrupted, but that is better
			// than crashing
			AnimInstanace->ActiveAnimNotifyState.Empty();
		}
	}
}

void FAnimationBlueprintEditor::GetCustomDebugObjects(TArray<FCustomDebugObject>& DebugList) const
{
	UDebugSkelMeshComponent* PreviewMeshComponent = PersonaToolkit->GetPreviewMeshComponent();
	if (PreviewMeshComponent->IsAnimBlueprintInstanced())
	{
		new (DebugList) FCustomDebugObject(PreviewMeshComponent->GetAnimInstance(), LOCTEXT("PreviewObjectLabel", "Preview Instance").ToString());
	}
}

void FAnimationBlueprintEditor::CreateDefaultTabContents(const TArray<UBlueprint*>& InBlueprints)
{
	FBlueprintEditor::CreateDefaultTabContents(InBlueprints);

	PreviewEditor = SNew(SAnimBlueprintPreviewPropertyEditor, SharedThis(this));
}

FGraphAppearanceInfo FAnimationBlueprintEditor::GetGraphAppearance(UEdGraph* InGraph) const
{
	FGraphAppearanceInfo AppearanceInfo = FBlueprintEditor::GetGraphAppearance(InGraph);

	if ( GetBlueprintObj()->IsA(UAnimBlueprint::StaticClass()) )
	{
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_Animation", "ANIMATION");
	}

	return AppearanceInfo;
}

void FAnimationBlueprintEditor::ClearSelectedActor()
{
	GetPreviewScene()->ClearSelectedActor();
}

void FAnimationBlueprintEditor::ClearSelectedAnimGraphNode()
{
	SelectedAnimGraphNode.Reset();
}

void FAnimationBlueprintEditor::DeselectAll()
{
	GetSkeletonTree()->DeselectAll();
	ClearSelectedActor();
	ClearSelectedAnimGraphNode();
}

void FAnimationBlueprintEditor::PostRedo(bool bSuccess)
{
	DocumentManager->RefreshAllTabs();

	FBlueprintEditor::PostRedo(bSuccess);

	// PostUndo broadcast, OnPostRedo
	OnPostUndo.Broadcast();

	// clear up preview anim notify states
	// animnotify states are saved in AnimInstance
	// if those are undoed or redoed, they have to be 
	// cleared up, otherwise, they might have invalid data
	ClearupPreviewMeshAnimNotifyStates();

	// calls PostCompile to copy proper values between anim nodes
	OnPostCompile();
}

void FAnimationBlueprintEditor::UndoAction()
{
	GEditor->UndoTransaction();
}

void FAnimationBlueprintEditor::RedoAction()
{
	GEditor->RedoTransaction();
}

void FAnimationBlueprintEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged)
{
	FBlueprintEditor::NotifyPostChange(PropertyChangedEvent, PropertyThatChanged);

	// When you change properties on a node, call CopyNodeDataToPreviewNode to allow pushing those to preview instance, for live editing
	UAnimGraphNode_Base* SelectedNode = SelectedAnimGraphNode.Get();
	if (SelectedNode)
	{
		FAnimNode_Base* PreviewNode = FindAnimNode(SelectedNode);
		if (PreviewNode)
		{
			SelectedNode->CopyNodeDataToPreviewNode(PreviewNode);
		}
	}
}

void FAnimationBlueprintEditor::Tick(float DeltaTime)
{
	FBlueprintEditor::Tick(DeltaTime);

	GetPreviewScene()->InvalidateViews();
}

bool FAnimationBlueprintEditor::IsEditable(UEdGraph* InGraph) const
{
	bool bEditable = FBlueprintEditor::IsEditable(InGraph);
	bEditable &= IsGraphInCurrentBlueprint(InGraph);

	return bEditable;
}

FText FAnimationBlueprintEditor::GetGraphDecorationString(UEdGraph* InGraph) const
{
	if (!IsGraphInCurrentBlueprint(InGraph))
	{
		return LOCTEXT("PersonaExternalGraphDecoration", " Parent Graph Preview");
	}
	return FText::GetEmpty();
}

TStatId FAnimationBlueprintEditor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FAnimationBlueprintEditor, STATGROUP_Tickables);
}

void FAnimationBlueprintEditor::OnBlueprintPreCompile(UBlueprint* BlueprintToCompile)
{
	if (PersonaToolkit.IsValid())
	{
		UDebugSkelMeshComponent* PreviewMeshComponent = PersonaToolkit->GetPreviewMeshComponent();
		if(PreviewMeshComponent && PreviewMeshComponent->PreviewInstance)
		{
			// If we are compiling an anim notify state the class will soon be sanitized and 
			// if an anim instance is running a state when that happens it will likely
			// crash, so we end any states that are about to compile.
			UAnimPreviewInstance* Instance = PreviewMeshComponent->PreviewInstance;
			USkeletalMeshComponent* SkelMeshComp = Instance->GetSkelMeshComponent();

			for(int32 Idx = Instance->ActiveAnimNotifyState.Num() - 1 ; Idx >= 0 ; --Idx)
			{
				FAnimNotifyEvent& Event = Instance->ActiveAnimNotifyState[Idx];
				if(Event.NotifyStateClass->GetClass() == BlueprintToCompile->GeneratedClass)
				{
					Event.NotifyStateClass->NotifyEnd(SkelMeshComp, Cast<UAnimSequenceBase>(Event.NotifyStateClass->GetOuter()));
					Instance->ActiveAnimNotifyState.RemoveAt(Idx);
				}
			}
		}
	}
}

void FAnimationBlueprintEditor::OnBlueprintChangedImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled /*= false*/)
{
	FBlueprintEditor::OnBlueprintChangedImpl(InBlueprint, bIsJustBeingCompiled);

	// calls PostCompile to copy proper values between anim nodes
	OnPostCompile();
}

TSharedRef<IPersonaPreviewScene> FAnimationBlueprintEditor::GetPreviewScene() const
{ 
	return PersonaToolkit->GetPreviewScene(); 
}

void FAnimationBlueprintEditor::HandleObjectsSelected(const TArray<UObject*>& InObjects)
{
	SetDetailObjects(InObjects);
}

void FAnimationBlueprintEditor::HandleObjectSelected(UObject* InObject)
{
	SetDetailObject(InObject);
}

void FAnimationBlueprintEditor::HandleSelectionChanged(const TArrayView<TSharedPtr<ISkeletonTreeItem>>& InSelectedItems, ESelectInfo::Type InSelectInfo)
{
	TArray<UObject*> Objects;
	Algo::TransformIf(InSelectedItems, Objects, [](const TSharedPtr<ISkeletonTreeItem>& InItem) { return InItem->GetObject() != nullptr; }, [](const TSharedPtr<ISkeletonTreeItem>& InItem) { return InItem->GetObject(); });
	SetDetailObjects(Objects);
}

UObject* FAnimationBlueprintEditor::HandleGetObject()
{
	return GetEditingObject();
}

void FAnimationBlueprintEditor::HandleOpenNewAsset(UObject* InNewAsset)
{
	FAssetEditorManager::Get().OpenEditorForAsset(InNewAsset);
}

FAnimNode_Base* FAnimationBlueprintEditor::FindAnimNode(UAnimGraphNode_Base* AnimGraphNode) const
{
	FAnimNode_Base* AnimNode = nullptr;
	if (AnimGraphNode)
	{
		UDebugSkelMeshComponent* PreviewMeshComponent = GetPreviewScene()->GetPreviewMeshComponent();
		if (PreviewMeshComponent != nullptr && PreviewMeshComponent->GetAnimInstance() != nullptr)
		{
			AnimNode = AnimGraphNode->FindDebugAnimNode(PreviewMeshComponent);
		}
	}

	return AnimNode;
}

void FAnimationBlueprintEditor::OnSelectedNodesChangedImpl(const TSet<class UObject*>& NewSelection)
{
	FBlueprintEditor::OnSelectedNodesChangedImpl(NewSelection);

	IPersonaEditorModeManager* PersonaEditorModeManager = static_cast<IPersonaEditorModeManager*>(GetAssetEditorModeManager());

	if (SelectedAnimGraphNode.IsValid())
	{
		FAnimNode_Base* PreviewNode = FindAnimNode(SelectedAnimGraphNode.Get());
		if (PersonaEditorModeManager)
		{
			SelectedAnimGraphNode->OnNodeSelected(false, *PersonaEditorModeManager, PreviewNode);
		}

		SelectedAnimGraphNode.Reset();
	}

	// if we only have one node selected, let it know
	UAnimGraphNode_Base* NewSelectedAnimGraphNode = nullptr;
	if (NewSelection.Num() == 1)
	{
		NewSelectedAnimGraphNode = Cast<UAnimGraphNode_Base>(*NewSelection.CreateConstIterator());
		if (NewSelectedAnimGraphNode != nullptr)
		{
			SelectedAnimGraphNode = NewSelectedAnimGraphNode;

			FAnimNode_Base* PreviewNode = FindAnimNode(SelectedAnimGraphNode.Get());
			if (PreviewNode && PersonaEditorModeManager)
			{
				SelectedAnimGraphNode->OnNodeSelected(true, *PersonaEditorModeManager, PreviewNode);
			}
		}
	}
}

void FAnimationBlueprintEditor::OnPostCompile()
{
	// act as if we have re-selected, so internal pointers are updated
	if (CurrentUISelection == FBlueprintEditor::SelectionState_Graph)
	{
		FGraphPanelSelectionSet SelectionSet = GetSelectedNodes();
		OnSelectedNodesChangedImpl(SelectionSet);
		FocusInspectorOnGraphSelection(SelectionSet, /*bForceRefresh=*/ true);
	}

	// if the user manipulated Pin values directly from the node, then should copy updated values to the internal node to retain data consistency
	UEdGraph* FocusedGraph = GetFocusedGraph();
	if (FocusedGraph)
	{
		// find UAnimGraphNode_Base
		for (UEdGraphNode* Node : FocusedGraph->Nodes)
		{
			UAnimGraphNode_Base* AnimGraphNode = Cast<UAnimGraphNode_Base>(Node);
			if (AnimGraphNode)
			{
				FAnimNode_Base* AnimNode = FindAnimNode(AnimGraphNode);
				if (AnimNode)
				{
					AnimGraphNode->CopyNodeDataToPreviewNode(AnimNode);
				}
			}
		}
	}
}

void FAnimationBlueprintEditor::HandlePinDefaultValueChanged(UEdGraphPin* InPinThatChanged)
{
	UAnimGraphNode_Base* AnimGraphNode = Cast<UAnimGraphNode_Base>(InPinThatChanged->GetOwningNode());
	if (AnimGraphNode)
	{
		FAnimNode_Base* AnimNode = FindAnimNode(AnimGraphNode);
		if (AnimNode)
		{
			AnimGraphNode->CopyNodeDataToPreviewNode(AnimNode);
		}
	}
}

void FAnimationBlueprintEditor::HandleSetObjectBeingDebugged(UObject* InObject)
{
	FBlueprintEditor::HandleSetObjectBeingDebugged(InObject);
	
	if (UAnimInstance* AnimInstance = Cast<UAnimInstance>(InObject))
	{
		USkeletalMeshComponent* SkeletalMeshComponent = AnimInstance->GetSkelMeshComponent();
		if (SkeletalMeshComponent)
		{
			// If we are selecting the preview instance, reset us back to 'normal'
			if (InObject->GetWorld()->IsPreviewWorld())
			{
				GetPreviewScene()->ShowDefaultMode();
				GetPreviewScene()->GetPreviewMeshComponent()->PreviewInstance->SetDebugSkeletalMeshComponent(nullptr);
			}
			else
			{
				// Otherwise eet us to display the debugged instance via copy-pose
				GetPreviewScene()->GetPreviewMeshComponent()->EnablePreview(true, nullptr);
				GetPreviewScene()->GetPreviewMeshComponent()->PreviewInstance->SetDebugSkeletalMeshComponent(SkeletalMeshComponent);
			}
		}
	}
	else
	{
		// Clear the copy-pose component and set us back to 'normal'
		GetPreviewScene()->ShowDefaultMode();
		GetPreviewScene()->GetPreviewMeshComponent()->PreviewInstance->SetDebugSkeletalMeshComponent(nullptr);
	}
}

void FAnimationBlueprintEditor::HandlePreviewMeshChanged(USkeletalMesh* OldPreviewMesh, USkeletalMesh* NewPreviewMesh)
{
	UObject* Object = GetBlueprintObj()->GetObjectBeingDebugged();
	if(Object)
	{
		HandleSetObjectBeingDebugged(Object);
	}
}

#undef LOCTEXT_NAMESPACE

