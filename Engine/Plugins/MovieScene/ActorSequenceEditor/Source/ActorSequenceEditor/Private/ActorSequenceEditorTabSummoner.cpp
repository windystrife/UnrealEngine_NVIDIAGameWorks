// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ActorSequenceEditorTabSummoner.h"

#include "ActorSequence.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "LevelEditorSequencerIntegration.h"
#include "SSCSEditor.h"
#include "SlateIconFinder.h"
#include "BlueprintEditorUtils.h"
#include "EditorStyleSet.h"
#include "Widgets/Images/SImage.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"


#define LOCTEXT_NAMESPACE "ActorSequenceEditorSummoner"

DECLARE_DELEGATE_OneParam(FOnComponentSelected, TSharedPtr<FSCSEditorTreeNode>);
DECLARE_DELEGATE_RetVal_OneParam(bool, FIsComponentValid, UActorComponent*);


class SComponentSelectionTree
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SComponentSelectionTree) : _IsInEditMode(false) {}

		SLATE_EVENT(FOnComponentSelected, OnComponentSelected)
		SLATE_EVENT(FIsComponentValid, IsComponentValid)
		SLATE_ARGUMENT(bool, IsInEditMode)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, AActor* InPreviewActor)
	{
		bIsInEditMode = InArgs._IsInEditMode;
		OnComponentSelected = InArgs._OnComponentSelected;
		IsComponentValid = InArgs._IsComponentValid;

		ChildSlot
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FSCSEditorTreeNode>>)
			.TreeItemsSource(&RootNodes)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SComponentSelectionTree::GenerateRow)
			.OnGetChildren(this, &SComponentSelectionTree::OnGetChildNodes)
			.OnSelectionChanged(this, &SComponentSelectionTree::OnSelectionChanged)
			.ItemHeight(24)
		];

		BuildTree(InPreviewActor);

		if (RootNodes.Num() == 0)
		{
			ChildSlot
			[
				SNew(SBox)
				.Padding(FMargin(5.f))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoValidComponentsFound", "No valid components available"))
				]
			];
		}
	}

	void BuildTree(AActor* Actor)
	{
		RootNodes.Reset();
		ObjectToNode.Reset();

		for (UActorComponent* Component : TInlineComponentArray<UActorComponent*>(Actor))
		{
			if (IsComponentVisibleInTree(Component))
			{
				FindOrAddNodeForComponent(Component);
			}
		}
	}

private:

	void OnSelectionChanged(TSharedPtr<FSCSEditorTreeNode> InNode, ESelectInfo::Type SelectInfo)
	{
		OnComponentSelected.ExecuteIfBound(InNode);
	}

	void OnGetChildNodes(TSharedPtr<FSCSEditorTreeNode> InNodePtr, TArray<TSharedPtr<FSCSEditorTreeNode>>& OutChildren)
	{
		OutChildren = InNodePtr->GetChildren();
	}

	TSharedRef<ITableRow> GenerateRow(TSharedPtr<FSCSEditorTreeNode> InNodePtr, const TSharedRef<STableViewBase>& OwnerTable)
	{
		const FSlateBrush* ComponentIcon = FEditorStyle::GetBrush("SCS.NativeComponent");
		if (InNodePtr->GetComponentTemplate() != NULL)
		{
			ComponentIcon = FSlateIconFinder::FindIconBrushForClass( InNodePtr->GetComponentTemplate()->GetClass(), TEXT("SCS.Component") );
		}

		FText Label = InNodePtr->IsInherited() && !bIsInEditMode
			? FText::Format(LOCTEXT("NativeComponentFormatString","{0} (Inherited)"), FText::FromString(InNodePtr->GetDisplayString()))
			: FText::FromString(InNodePtr->GetDisplayString());

		TSharedRef<STableRow<FSCSEditorTreeNodePtrType>> Row = SNew(STableRow<FSCSEditorTreeNodePtrType>, OwnerTable).Padding(FMargin(0.f, 0.f, 0.f, 4.f));
		Row->SetContent(
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(ComponentIcon)
				.ColorAndOpacity(SSCS_RowWidget::GetColorTintForIcon(InNodePtr))
			]

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(2, 0, 0, 0)
			[
				SNew(STextBlock)
				.Text(Label)
			]);

		return Row;
	}

	bool IsComponentVisibleInTree(UActorComponent* ActorComponent) const
	{
		return !IsComponentValid.IsBound() || IsComponentValid.Execute(ActorComponent);
	}

	TSharedPtr<FSCSEditorTreeNode> FindOrAddNodeForComponent(UActorComponent* ActorComponent)
	{
		if (ActorComponent->IsEditorOnly())
		{
			return nullptr;
		}

		if (TSharedPtr<FSCSEditorTreeNode>* Existing = ObjectToNode.Find(ActorComponent))
		{
			return *Existing;
		}
		else if (USceneComponent* SceneComponent = Cast<USceneComponent>(ActorComponent))
		{
			if (UActorComponent* Parent = SceneComponent->GetAttachParent())
			{
				TSharedPtr<FSCSEditorTreeNode> ParentNode = FindOrAddNodeForComponent(Parent);

				if (!ParentNode.IsValid())
				{
					return nullptr;
				}

				TreeView->SetItemExpansion(ParentNode, true);

				TSharedPtr<FSCSEditorTreeNode> ChildNode = ParentNode->AddChildFromComponent(ActorComponent);
				ObjectToNode.Add(ActorComponent, ChildNode);

				return ChildNode;
			}
		}
		
		TSharedPtr<FSCSEditorTreeNode> RootNode = FSCSEditorTreeNode::FactoryNodeFromComponent(ActorComponent);
		RootNodes.Add(RootNode);
		ObjectToNode.Add(ActorComponent, RootNode);

		TreeView->SetItemExpansion(RootNode, true);

		return RootNode;
	}

private:
	bool bIsInEditMode;
	FOnComponentSelected OnComponentSelected;
	FIsComponentValid IsComponentValid;
	TSharedPtr<STreeView<TSharedPtr<FSCSEditorTreeNode>>> TreeView;
	TMap<FObjectKey, TSharedPtr<FSCSEditorTreeNode>> ObjectToNode;
	TArray<TSharedPtr<FSCSEditorTreeNode>> RootNodes;
};

class SActorSequenceEditorWidgetImpl : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SActorSequenceEditorWidgetImpl){}
	SLATE_END_ARGS();

	~SActorSequenceEditorWidgetImpl()
	{
		if (Sequencer.IsValid())
		{
			FLevelEditorSequencerIntegration::Get().RemoveSequencer(Sequencer.ToSharedRef());
			Sequencer->Close();
			Sequencer = nullptr;
		}

		GEditor->OnBlueprintPreCompile().Remove(OnBlueprintPreCompileHandle);
		FCoreUObjectDelegates::OnObjectSaved.Remove(OnObjectSavedHandle);
	}

	void Construct(const FArguments&, TWeakPtr<FBlueprintEditor> InBlueprintEditor)
	{
		OnBlueprintPreCompileHandle = GEditor->OnBlueprintPreCompile().AddSP(this, &SActorSequenceEditorWidgetImpl::OnBlueprintPreCompile);
		OnObjectSavedHandle = FCoreUObjectDelegates::OnObjectSaved.AddSP(this, &SActorSequenceEditorWidgetImpl::OnObjectPreSave);

		WeakBlueprintEditor = InBlueprintEditor;

		ChildSlot
		[
			SAssignNew(Content, SBox)
			.MinDesiredHeight(200)
		];
	}

	FText GetDisplayLabel() const
	{
		UActorSequence* Sequence = WeakSequence.Get();
		return Sequence ? Sequence->GetDisplayName() : LOCTEXT("DefaultSequencerLabel", "Sequencer");
	}

	UActorSequence* GetActorSequence() const
	{
		return WeakSequence.Get();
	}

	UObject* GetPlaybackContext() const
	{
		UActorSequence* LocalActorSequence = GetActorSequence();
		if (LocalActorSequence)
		{
			if (AActor* Actor = LocalActorSequence->GetTypedOuter<AActor>())
			{
				return Actor;
			}
			else if (UBlueprintGeneratedClass* GeneratedClass = LocalActorSequence->GetTypedOuter<UBlueprintGeneratedClass>())
			{
				return GeneratedClass->SimpleConstructionScript->GetComponentEditorActorInstance();
			}
		}
		
		return nullptr;
	}

	TArray<UObject*> GetEventContexts() const
	{
		TArray<UObject*> Contexts;
		if (auto* Context = GetPlaybackContext())
		{
			Contexts.Add(Context);
		}
		return Contexts;
	}

	void SetActorSequence(UActorSequence* NewSequence)
	{
		if (UActorSequence* OldSequence = WeakSequence.Get())
		{
			if (OnSequenceChangedHandle.IsValid())
			{
				OldSequence->OnSignatureChanged().Remove(OnSequenceChangedHandle);
			}
		}

		WeakSequence = NewSequence;

		if (NewSequence)
		{
			OnSequenceChangedHandle = NewSequence->OnSignatureChanged().AddSP(this, &SActorSequenceEditorWidgetImpl::OnSequenceChanged);
		}

		// If we already have a sequencer open, just assign the sequence
		if (Sequencer.IsValid() && NewSequence)
		{
			if (Sequencer->GetRootMovieSceneSequence() != NewSequence)
			{
				Sequencer->ResetToNewRootSequence(*NewSequence);
			}
			return;
		}

		// If we're setting the sequence to none, destroy sequencer
		if (!NewSequence)
		{
			if (Sequencer.IsValid())
			{
				FLevelEditorSequencerIntegration::Get().RemoveSequencer(Sequencer.ToSharedRef());
				Sequencer->Close();
				Sequencer = nullptr;
			}

			Content->SetContent(SNew(STextBlock).Text(LOCTEXT("NothingSelected", "Select a sequence")));
			return;
		}

		// We need to initialize a new sequencer instance
		FSequencerInitParams SequencerInitParams;
		{
			TWeakObjectPtr<UActorSequence> LocalWeakSequence = NewSequence;

			SequencerInitParams.RootSequence = NewSequence;
			SequencerInitParams.EventContexts = TAttribute<TArray<UObject*>>(this, &SActorSequenceEditorWidgetImpl::GetEventContexts);
			SequencerInitParams.PlaybackContext = TAttribute<UObject*>(this, &SActorSequenceEditorWidgetImpl::GetPlaybackContext);

			TSharedRef<FExtender> AddMenuExtender = MakeShareable(new FExtender);

			AddMenuExtender->AddMenuExtension("AddTracks", EExtensionHook::Before, nullptr,
				FMenuExtensionDelegate::CreateLambda([=](FMenuBuilder& MenuBuilder){

					MenuBuilder.AddSubMenu(
						LOCTEXT("AddComponent_Label", "Component"),
						LOCTEXT("AddComponent_ToolTip", "Add a binding to one of this actor's components and allow it to be animated by Sequencer"),
						FNewMenuDelegate::CreateRaw(this, &SActorSequenceEditorWidgetImpl::AddPossessComponentMenuExtensions),
						false /*bInOpenSubMenuOnClick*/,
						FSlateIcon()//"LevelSequenceEditorStyle", "LevelSequenceEditor.PossessNewActor")
						);

				})
			);

			SequencerInitParams.ViewParams.bReadOnly = !WeakBlueprintEditor.IsValid() && !NewSequence->IsEditable();
			SequencerInitParams.bEditWithinLevelEditor = false;
			SequencerInitParams.ViewParams.AddMenuExtender = AddMenuExtender;
			SequencerInitParams.ViewParams.UniqueName = "EmbeddedActorSequenceEditor";
			SequencerInitParams.ViewParams.OnReceivedFocus.BindRaw(this, &SActorSequenceEditorWidgetImpl::OnSequencerReceivedFocus);
		}

		Sequencer = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer").CreateSequencer(SequencerInitParams);
		Content->SetContent(Sequencer->GetSequencerWidget());

		FLevelEditorSequencerIntegrationOptions Options;
		Options.bRequiresLevelEvents = true;
		Options.bRequiresActorEvents = false;
		Options.bCanRecord = false;

		FLevelEditorSequencerIntegration::Get().AddSequencer(Sequencer.ToSharedRef(), Options);
	}

	void OnSequencerReceivedFocus()
	{
		if (Sequencer.IsValid())
		{
			FLevelEditorSequencerIntegration::Get().OnSequencerReceivedFocus(Sequencer.ToSharedRef());
		}
	}

	void OnObjectPreSave(UObject* InObject)
	{
		TSharedPtr<FBlueprintEditor> BlueprintEditor = WeakBlueprintEditor.Pin();
		if (Sequencer.IsValid() && BlueprintEditor.IsValid() && InObject && InObject == BlueprintEditor->GetBlueprintObj())
		{
			Sequencer->RestorePreAnimatedState();
		}
	}

	void OnBlueprintPreCompile(UBlueprint* InBlueprint)
	{
		TSharedPtr<FBlueprintEditor> BlueprintEditor = WeakBlueprintEditor.Pin();
		if (Sequencer.IsValid() && BlueprintEditor.IsValid() && InBlueprint && InBlueprint == BlueprintEditor->GetBlueprintObj())
		{
			Sequencer->RestorePreAnimatedState();
		}
	}

	void OnSelectionUpdated(TSharedPtr<FSCSEditorTreeNode> SelectedNode)
	{
		if (SelectedNode->GetNodeType() != FSCSEditorTreeNode::ComponentNode)
		{
			return;
		}

		UActorComponent* EditingComponent = nullptr;

		TSharedPtr<FBlueprintEditor> BlueprintEditor = WeakBlueprintEditor.Pin();
		if (BlueprintEditor.IsValid())
		{
			UBlueprint* Blueprint = BlueprintEditor->GetBlueprintObj();
			if (Blueprint)
			{
				EditingComponent = SelectedNode->GetEditableComponentTemplate(Blueprint);
			}
		}
		else if (AActor* Actor = GetPreviewActor())
		{
			EditingComponent = SelectedNode->FindComponentInstanceInActor(Actor);
		}

		if (EditingComponent)
		{
			const FScopedTransaction Transaction(LOCTEXT("AddComponentToSequencer", "Add component to Sequencer"));
			Sequencer->GetHandleToObject(EditingComponent, true);
		}

		FSlateApplication::Get().DismissAllMenus();
	}

	void AddPossessComponentMenuExtensions(FMenuBuilder& MenuBuilder)
	{
		AActor* Actor = GetPreviewActor();
		if (!Actor)
		{
			return;
		}

		Sequencer->State.ClearObjectCaches(*Sequencer);
		TSet<UObject*> AllBoundObjects;

		AllBoundObjects.Add(GetOwnerComponent());

		UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
		for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index)
		{
			FMovieScenePossessable& Possessable = MovieScene->GetPossessable(Index);
			for (TWeakObjectPtr<> WeakObject : Sequencer->FindBoundObjects(Possessable.GetGuid(), Sequencer->GetFocusedTemplateID()))
			{
				if (UObject* Object = WeakObject.Get())
				{
					AllBoundObjects.Add(Object);
				}
			}
		}

		bool bIdent = false;
		MenuBuilder.AddWidget(
			SNew(SComponentSelectionTree, Actor)
			.IsInEditMode(WeakBlueprintEditor.Pin().IsValid())
			.OnComponentSelected(this, &SActorSequenceEditorWidgetImpl::OnSelectionUpdated)
			.IsComponentValid_Lambda(
				[AllBoundObjects](UActorComponent* Component)
				{
					return !AllBoundObjects.Contains(Component);
				}
			)
			, FText(), !bIdent
		);
	}

	AActor* GetPreviewActor() const
	{
		TSharedPtr<FBlueprintEditor> BlueprintEditor = WeakBlueprintEditor.Pin();
		if (BlueprintEditor.IsValid())
		{
			return BlueprintEditor->GetPreviewActor();
		}
		if (UActorSequence* Sequence = WeakSequence.Get())
		{
			return Sequence->GetTypedOuter<AActor>();
		}
		return nullptr;
	}

	UActorComponent* GetOwnerComponent() const
	{
		UActorSequence* ActorSequence = WeakSequence.Get();
		AActor* Actor = ActorSequence ? GetPreviewActor() : nullptr;

		return Actor ? FindObject<UActorComponent>(Actor, *ActorSequence->GetOuter()->GetName()) : nullptr;
	}

	void OnSequenceChanged()
	{
		UActorSequence* ActorSequence = WeakSequence.Get();
		UBlueprint* Blueprint = ActorSequence ? ActorSequence->GetParentBlueprint() : nullptr;

		if (Blueprint)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
	}

private:
	TWeakObjectPtr<UActorSequence> WeakSequence;

	TWeakPtr<FBlueprintEditor> WeakBlueprintEditor;

	TSharedPtr<SBox> Content;
	TSharedPtr<ISequencer> Sequencer;

	FDelegateHandle OnBlueprintPreCompileHandle;
	FDelegateHandle OnObjectSavedHandle;

	FDelegateHandle OnSequenceChangedHandle;
};

void SActorSequenceEditorWidget::Construct(const FArguments&, TWeakPtr<FBlueprintEditor> InBlueprintEditor)
{
	ChildSlot
	[
		SAssignNew(Impl, SActorSequenceEditorWidgetImpl, InBlueprintEditor)
	];
}

FText SActorSequenceEditorWidget::GetDisplayLabel() const
{
	return Impl.Pin()->GetDisplayLabel();
}

void SActorSequenceEditorWidget::AssignSequence(UActorSequence* NewActorSequence)
{
	Impl.Pin()->SetActorSequence(NewActorSequence);
}

UActorSequence* SActorSequenceEditorWidget::GetSequence() const
{
	return Impl.Pin()->GetActorSequence();
}

FActorSequenceEditorSummoner::FActorSequenceEditorSummoner(TSharedPtr<FBlueprintEditor> BlueprintEditor)
	: FWorkflowTabFactory("EmbeddedSequenceID", BlueprintEditor)
	, WeakBlueprintEditor(BlueprintEditor)
{
	bIsSingleton = true;

	TabLabel = LOCTEXT("SequencerTabName", "Sequencer");
}

TSharedRef<SWidget> FActorSequenceEditorSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SActorSequenceEditorWidget, WeakBlueprintEditor);
}

#undef LOCTEXT_NAMESPACE