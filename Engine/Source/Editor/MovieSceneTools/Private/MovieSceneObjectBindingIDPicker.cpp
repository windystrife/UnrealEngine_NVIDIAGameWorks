// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneObjectBindingIDPicker.h"
#include "IPropertyUtilities.h"
#include "MovieSceneBindingOwnerInterface.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Sections/MovieSceneSubSection.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Textures/SlateIcon.h"
#include "SlateIconFinder.h"
#include "EditorStyleSet.h"
#include "SImage.h"
#include "SOverlay.h"
#include "ISequencer.h"
#include "MovieSceneEvaluationTemplateInstance.h"
#include "MovieSceneSequenceHierarchy.h"

#define LOCTEXT_NAMESPACE "MovieSceneObjectBindingIDPicker"

DECLARE_DELEGATE_OneParam(FOnSelectionChanged, const FMovieSceneObjectBindingID&);

/** Node that represents an object binding, or a sub sequence (where the guid is zero) */
struct FSequenceBindingNode
{
	FSequenceBindingNode(FText InDisplayString, FMovieSceneObjectBindingID InBindingID, FSlateIcon InIcon)
		: BindingID(InBindingID)
		, DisplayString(InDisplayString)
		, Icon(InIcon)
		, bIsSpawnable(false)
	{}

	/** Add a child */
	void AddChild(TSharedRef<FSequenceBindingNode> Child)
	{
		Child->ParentID = BindingID;
		Children.Add(Child);
	}

	/** This object's ID, and its parent's */
	FMovieSceneObjectBindingID BindingID, ParentID;
	/** The display string that represents this node */
	FText DisplayString;
	/** A representative icon for the node */
	FSlateIcon Icon;
	/** Whether this is a spawnable or not */
	bool bIsSpawnable;
	/** Array holding this node's children */
	TArray<TSharedRef<FSequenceBindingNode>> Children;
};

/** Stack of sequence IDs from parent to child */
struct FSequenceIDStack
{
	/** Get the current accumulated sequence ID */
	FMovieSceneSequenceID GetCurrent() const
	{
		FMovieSceneSequenceID ID = MovieSceneSequenceID::Root;
		for (int32 Index = IDs.Num() - 1; Index >= 0; --Index)
		{
			ID = ID.AccumulateParentID(IDs[Index]);
		}
		return ID;
	}

	/** Push a sequence ID onto the stack */
	void Push(FMovieSceneSequenceID InSequenceID) { IDs.Add(InSequenceID); }

	/** Pop the last sequence ID off the stack */
	void Pop() { IDs.RemoveAt(IDs.Num() - 1, 1, false); }
	
private:
	TArray<FMovieSceneSequenceID> IDs;
};


/** Data structure used internally to represent the bindings of a sequence recursively */
struct FSequenceBindingTree
{
	/**
	 * Construct the tree structure from the specified sequence.
	 *
	 * @param InSequence			The sequence to generate the tree for
	 * @param InActiveSequence		A sequence at which point we can start to generate localally resolving IDs
	 * @param InActiveSequenceID	The sequence ID for the above sequence within the root context
	 */
	void Build(UMovieSceneSequence* InSequence, FObjectKey InActiveSequence, FMovieSceneSequenceID InActiveSequenceID)
	{
		// Reset state
		ActiveSequenceID = InActiveSequenceID;
		ActiveSequence = InActiveSequence;
		Hierarchy.Reset();

		ActiveSequenceNode = nullptr;

		// Create a node for the root sequence
		FMovieSceneObjectBindingID RootSequenceID;
		TSharedRef<FSequenceBindingNode> RootSequenceNode = MakeShared<FSequenceBindingNode>(FText(), RootSequenceID, FSlateIcon());
		Hierarchy.Add(RootSequenceID, RootSequenceNode);

		TopLevelNode = RootSequenceNode;

		if (InSequence)
		{
			RootSequenceNode->DisplayString = FText::FromString(InSequence->GetName());
			RootSequenceNode->Icon = FSlateIconFinder::FindIconForClass(InSequence->GetClass());

			// Build the tree
			FSequenceIDStack SequenceIDStack;
			Build(InSequence, SequenceIDStack);

			// Sort the tree
			Sort(RootSequenceNode);

			// We don't show cross-references to the same sequence since this would result in erroneous mixtures of absolute and local bindings
			if (ActiveSequenceNode.IsValid() && ActiveSequenceNode != RootSequenceNode)
			{
				// Remove it from its parent, and put it at the root for quick access
				TSharedPtr<FSequenceBindingNode> ActiveParent = Hierarchy.FindChecked(ActiveSequenceNode->ParentID);
				ActiveParent->Children.Remove(ActiveSequenceNode.ToSharedRef());

				// Make a new top level node (with an invalid ID)
				FMovieSceneObjectBindingID TopLevelID = FMovieSceneObjectBindingID(FGuid(), FMovieSceneSequenceID());
				TopLevelNode = MakeShared<FSequenceBindingNode>(FText(), TopLevelID, FSlateIcon());

				// Override the display string and icon
				ActiveSequenceNode->DisplayString = LOCTEXT("ThisSequenceText", "This Sequence");
				ActiveSequenceNode->Icon = FSlateIcon();

				TopLevelNode->Children.Add(ActiveSequenceNode.ToSharedRef());
				TopLevelNode->Children.Add(RootSequenceNode);
			}
		}
	}

	/** Get the root of the tree */
	TSharedRef<FSequenceBindingNode> GetRootNode() const
	{
		return TopLevelNode.ToSharedRef();
	}

	/** Find a node in the tree */
	TSharedPtr<FSequenceBindingNode> FindNode(FMovieSceneObjectBindingID BindingID) const
	{
		return Hierarchy.FindRef(BindingID);
	}

private:

	/** Recursive sort helper for a sequence binding node */
	static void Sort(TSharedRef<FSequenceBindingNode> Node)
	{
		Node->Children.Sort(
			[](TSharedRef<FSequenceBindingNode> A, TSharedRef<FSequenceBindingNode> B)
			{
				// Sort shots first
				if (A->BindingID.IsValid() != B->BindingID.IsValid())
				{
					return !A->BindingID.IsValid();
				}
				return A->DisplayString.CompareToCaseIgnored(B->DisplayString) < 0;
			}
		);

		for (TSharedRef<FSequenceBindingNode> Child : Node->Children)
		{
			Sort(Child);
		}
	}

	/** Recursive builder function that iterates into sub sequences */
	void Build(UMovieSceneSequence* InSequence, FSequenceIDStack& SequenceIDStack)
	{
		check(InSequence);

		UMovieScene* MovieScene = InSequence->GetMovieScene();
		if (!MovieScene)
		{
			return;
		}
		
		if (ActiveSequence == InSequence)
		{
			// Don't allow cross-references to the same sequence (ie, re-entrant references)
			if (SequenceIDStack.GetCurrent() != ActiveSequenceID)
			{
				return;
			}

			// Keep track of the active sequence node
			ActiveSequenceNode = Hierarchy.FindChecked(FMovieSceneObjectBindingID(FGuid(), SequenceIDStack.GetCurrent()));
		}

		// Iterate all sub sections
		for (const UMovieSceneTrack* MasterTrack : MovieScene->GetMasterTracks())
		{
			const UMovieSceneSubTrack* SubTrack = Cast<const UMovieSceneSubTrack>(MasterTrack);
			if (SubTrack)
			{
				for (UMovieSceneSection* Section : SubTrack->GetAllSections())
				{
					UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
					UMovieSceneSequence* SubSequence = SubSection ? SubSection->GetSequence() : nullptr;
					if (SubSequence)
					{
						// Hold onto the current parent ID before adding our ID onto the stack
						FMovieSceneSequenceID ParentID = SequenceIDStack.GetCurrent();
						SequenceIDStack.Push(SubSection->GetSequenceID());
						
						FMovieSceneObjectBindingID CurrentID(FGuid(), SequenceIDStack.GetCurrent());

						UMovieSceneCinematicShotSection* ShotSection = Cast<UMovieSceneCinematicShotSection>(Section);
						FText DisplayString = ShotSection ? ShotSection->GetShotDisplayName() : FText::FromName(SubSection->GetFName());
						FSlateIcon Icon(FEditorStyle::GetStyleSetName(), ShotSection ? "Sequencer.Tracks.CinematicShot" : "Sequencer.Tracks.Sub");
						
						TSharedRef<FSequenceBindingNode> NewNode = MakeShared<FSequenceBindingNode>(DisplayString, CurrentID, Icon);
						ensure(!Hierarchy.Contains(CurrentID));
						Hierarchy.Add(CurrentID, NewNode);

						EnsureParent(FGuid(), MovieScene, ParentID)->AddChild(NewNode);

						Build(SubSequence, SequenceIDStack);

						SequenceIDStack.Pop();
					}
				}
			}
		}

		FMovieSceneSequenceID CurrentSequenceID = SequenceIDStack.GetCurrent();

		// Add all spawnables first (since possessables can be children of spawnables)
		int32 SpawnableCount = MovieScene->GetSpawnableCount();
		for (int32 Index = 0; Index < SpawnableCount; ++Index)
		{
			const FMovieSceneSpawnable& Spawnable = MovieScene->GetSpawnable(Index);
			
			FMovieSceneObjectBindingID ID(Spawnable.GetGuid(), CurrentSequenceID);

			FSlateIcon Icon = FSlateIconFinder::FindIconForClass(Spawnable.GetObjectTemplate()->GetClass());
			TSharedRef<FSequenceBindingNode> NewNode = MakeShared<FSequenceBindingNode>(MovieScene->GetObjectDisplayName(Spawnable.GetGuid()), ID, Icon);
			NewNode->bIsSpawnable = true;

			EnsureParent(FGuid(), MovieScene, CurrentSequenceID)->AddChild(NewNode);
			ensure(!Hierarchy.Contains(ID));
			Hierarchy.Add(ID, NewNode);
		}

		// Add all possessables
		const int32 PossessableCount = MovieScene->GetPossessableCount();
		for (int32 Index = 0; Index < PossessableCount; ++Index)
		{
			const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(Index);
			if (InSequence->CanRebindPossessable(Possessable))
			{
				FMovieSceneObjectBindingID ID(Possessable.GetGuid(), CurrentSequenceID);

				FSlateIcon Icon = FSlateIconFinder::FindIconForClass(Possessable.GetPossessedObjectClass());
				TSharedRef<FSequenceBindingNode> NewNode = MakeShared<FSequenceBindingNode>(MovieScene->GetObjectDisplayName(Possessable.GetGuid()), ID, Icon);

				EnsureParent(Possessable.GetParent(), MovieScene, CurrentSequenceID)->AddChild(NewNode);
				ensure(!Hierarchy.Contains(ID));
				Hierarchy.Add(ID, NewNode);
			}
		}
	}

	/** Ensure that a parent node exists for the specified object */
	TSharedRef<FSequenceBindingNode> EnsureParent(const FGuid& InParentGuid, UMovieScene* InMovieScene, FMovieSceneSequenceID SequenceID)
	{
		FMovieSceneObjectBindingID ParentPtr(InParentGuid, SequenceID);

		// If the node already exists
		TSharedPtr<FSequenceBindingNode> Parent = Hierarchy.FindRef(ParentPtr);
		if (Parent.IsValid())
		{
			return Parent.ToSharedRef();
		}

		// Non-object binding nodes should have already been added externally to EnsureParent
		check(InParentGuid.IsValid());

		// Need to add it
		FGuid AddToGuid;
		if (FMovieScenePossessable* GrandParentPossessable = InMovieScene->FindPossessable(InParentGuid))
		{
			AddToGuid = GrandParentPossessable->GetGuid();
		}

		// Deduce the icon for the node
		FSlateIcon Icon;
		bool bIsSpawnable = false;
		{
			const FMovieScenePossessable* Possessable = InMovieScene->FindPossessable(InParentGuid);
			const FMovieSceneSpawnable* Spawnable = Possessable ? nullptr : InMovieScene->FindSpawnable(InParentGuid);
			if (Possessable || Spawnable)
			{
				Icon = FSlateIconFinder::FindIconForClass(Possessable ? Possessable->GetPossessedObjectClass() : Spawnable->GetObjectTemplate()->GetClass());
			}

			bIsSpawnable = Spawnable != nullptr;
		}

		TSharedRef<FSequenceBindingNode> NewNode = MakeShared<FSequenceBindingNode>(InMovieScene->GetObjectDisplayName(InParentGuid), ParentPtr, Icon);
		NewNode->bIsSpawnable = bIsSpawnable;
		
		ensure(!Hierarchy.Contains(ParentPtr));
		Hierarchy.Add(ParentPtr, NewNode);

		EnsureParent(AddToGuid, InMovieScene, SequenceID)->AddChild(NewNode);

		return NewNode;
	}

private:

	/** The ID of the currently 'active' sequence from which to generate relative IDs */
	FMovieSceneSequenceID ActiveSequenceID;
	/** The currently 'active' sequence from which to generate relative IDs */
	FObjectKey ActiveSequence;
	/** The node relating to the currently active sequence ID (if any) */
	TSharedPtr<FSequenceBindingNode> ActiveSequenceNode;
	/** The top level (root) node in the tree */
	TSharedPtr<FSequenceBindingNode> TopLevelNode;
	/** Map of hierarchical information */
	TMap<FMovieSceneObjectBindingID, TSharedPtr<FSequenceBindingNode>> Hierarchy;
};

void FMovieSceneObjectBindingIDPicker::Initialize()
{
	if (!DataTree.IsValid())
	{
		DataTree = MakeShared<FSequenceBindingTree>();
	}

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

	UMovieSceneSequence* Sequence = Sequencer.IsValid() ? Sequencer->GetRootMovieSceneSequence() : GetSequence();
	UMovieSceneSequence* ActiveSequence = Sequencer.IsValid() ? Sequencer->GetFocusedMovieSceneSequence() : GetSequence();
	FMovieSceneSequenceID ActiveSequenceID = Sequencer.IsValid() ? Sequencer->GetFocusedTemplateID() : MovieSceneSequenceID::Root;

	DataTree->Build(Sequence, ActiveSequence, ActiveSequenceID);

	UpdateCachedData();
}

void FMovieSceneObjectBindingIDPicker::OnGetMenuContent(FMenuBuilder& MenuBuilder, TSharedPtr<FSequenceBindingNode> Node)
{
	check(Node.IsValid());

	bool bHadAnyEntries = false;

	if (Node->BindingID.GetGuid().IsValid())
	{
		bHadAnyEntries = true;
		MenuBuilder.AddMenuEntry(
			Node->DisplayString,
			FText(),
			Node->Icon,
			FUIAction(
				FExecuteAction::CreateRaw(this, &FMovieSceneObjectBindingIDPicker::SetBindingId, Node->BindingID)
			)
		);
	}

	for (const TSharedPtr<FSequenceBindingNode>& Child : Node->Children)
	{
		check(Child.IsValid())

		if (!Child->BindingID.GetGuid().IsValid())
		{
			if (Child->Children.Num())
			{
				bHadAnyEntries = true;
				MenuBuilder.AddSubMenu(
					Child->DisplayString,
					FText(),
					FNewMenuDelegate::CreateRaw(this, &FMovieSceneObjectBindingIDPicker::OnGetMenuContent, Child),
					false,
					Child->Icon
					);
			}
		}
		else
		{
			bHadAnyEntries = true;
			MenuBuilder.AddMenuEntry(
				Child->DisplayString,
				FText(),
				Child->Icon,
				FUIAction(
					FExecuteAction::CreateRaw(this, &FMovieSceneObjectBindingIDPicker::SetBindingId, Child->BindingID)
				)
			);
		}
	}

	if (!bHadAnyEntries)
	{
		MenuBuilder.AddMenuEntry(LOCTEXT("NoEntries", "No Object Bindings"), FText(), FSlateIcon(), FUIAction());
	}
}

TSharedRef<SWidget> FMovieSceneObjectBindingIDPicker::GetPickerMenu()
{
	// Close self only to enable use inside context menus
	FMenuBuilder MenuBuilder(true, nullptr, nullptr, true);

	Initialize();
	OnGetMenuContent(MenuBuilder, DataTree->GetRootNode());

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FMovieSceneObjectBindingIDPicker::GetCurrentItemWidget(TSharedRef<STextBlock> TextContent)
{
	TextContent->SetText(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateRaw(this, &FMovieSceneObjectBindingIDPicker::GetCurrentText)));
	
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image_Raw(this, &FMovieSceneObjectBindingIDPicker::GetCurrentIconBrush)
			]

			+ SOverlay::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Right)
			[
				SNew(SImage)
				.Visibility_Raw(this, &FMovieSceneObjectBindingIDPicker::GetSpawnableIconOverlayVisibility)
				.Image(FEditorStyle::GetBrush("Sequencer.SpawnableIconOverlay"))
			]
		]

		+ SHorizontalBox::Slot()
		.Padding(4.f,0,0,0)
		.VAlign(VAlign_Center)
		[
			TextContent
		];
}

void FMovieSceneObjectBindingIDPicker::SetBindingId(FMovieSceneObjectBindingID InBindingId)
{
	SetRemappedCurrentValue(InBindingId);
	UpdateCachedData();
}

void FMovieSceneObjectBindingIDPicker::UpdateCachedData()
{
	FMovieSceneObjectBindingID CurrentValue = GetRemappedCurrentValue();
	TSharedPtr<FSequenceBindingNode> Object = CurrentValue.IsValid() ? DataTree->FindNode(CurrentValue) : nullptr;

	if (!Object.IsValid())
	{
		CurrentIcon = FSlateIcon();
		CurrentText = LOCTEXT("UnresolvedBinding", "Unresolved Binding");
		ToolTipText = LOCTEXT("UnresolvedBinding_ToolTip", "The specified binding could not be located in the sequence");
		bIsCurrentItemSpawnable = false;
	}
	else
	{
		CurrentText = Object->DisplayString;
		CurrentIcon = Object->Icon;
		bIsCurrentItemSpawnable = Object->bIsSpawnable;

		ToolTipText = FText();
		while (Object.IsValid() && Object->BindingID != FMovieSceneObjectBindingID())
		{
			ToolTipText = ToolTipText.IsEmpty() ? Object->DisplayString : FText::Format(LOCTEXT("ToolTipFormat", "{0} -> {1}"), Object->DisplayString, ToolTipText);
			Object = DataTree->FindNode(Object->ParentID);
		}
	}
}

FText FMovieSceneObjectBindingIDPicker::GetToolTipText() const
{
	return ToolTipText;
}

FText FMovieSceneObjectBindingIDPicker::GetCurrentText() const
{
	return CurrentText;
}

FSlateIcon FMovieSceneObjectBindingIDPicker::GetCurrentIcon() const
{
	return CurrentIcon;
}

const FSlateBrush* FMovieSceneObjectBindingIDPicker::GetCurrentIconBrush() const
{
	return CurrentIcon.GetOptionalIcon();
}

EVisibility FMovieSceneObjectBindingIDPicker::GetSpawnableIconOverlayVisibility() const
{
	return bIsCurrentItemSpawnable ? EVisibility::Visible : EVisibility::Collapsed;
}

FMovieSceneObjectBindingID FMovieSceneObjectBindingIDPicker::GetRemappedCurrentValue() const
{
	FMovieSceneObjectBindingID ID = GetCurrentValue();

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

	// If the ID is in local space, remap it to the root space as according to the LocalSequenceID we were created with
	if (Sequencer.IsValid() && LocalSequenceID != MovieSceneSequenceID::Root && ID.IsValid() && ID.GetBindingSpace() == EMovieSceneObjectBindingSpace::Local)
	{
		ID = ID.ResolveLocalToRoot(LocalSequenceID, Sequencer->GetEvaluationTemplate().GetHierarchy());
	}

	return ID;
}

void FMovieSceneObjectBindingIDPicker::SetRemappedCurrentValue(FMovieSceneObjectBindingID InValue)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

	// If we have a local sequence ID set, and the supplied binding is in root space, we attempt to remap it into the local sequence ID's space, and use a sequence ID
	// that will resolve from LocalSequenceID instead of from the root. This ensures that you can work on sub sequences on their own, or within a master sequence
	// and the binding will resolve correctly.
	if (LocalSequenceID.IsValid() && Sequencer.IsValid() && InValue.GetGuid().IsValid() && InValue.GetBindingSpace() == EMovieSceneObjectBindingSpace::Root)
	{
		const FMovieSceneSequenceHierarchy& Hierarchy = Sequencer->GetEvaluationTemplate().GetHierarchy();

		FMovieSceneSequenceID NewLocalSequenceID = MovieSceneSequenceID::Root;
		FMovieSceneSequenceID CurrentSequenceID = InValue.GetSequenceID();
		
		while (CurrentSequenceID.IsValid())
		{
			if (LocalSequenceID == CurrentSequenceID)
			{
				// Found it
				InValue = FMovieSceneObjectBindingID(InValue.GetGuid(), NewLocalSequenceID, EMovieSceneObjectBindingSpace::Local);
				break;
			}

			const FMovieSceneSequenceHierarchyNode* CurrentNode = Hierarchy.FindNode(CurrentSequenceID);
			if (!ensureAlwaysMsgf(CurrentNode, TEXT("Malformed sequence hierarchy")))
			{
				break;
			}
			else if (const FMovieSceneSubSequenceData* SubData = Hierarchy.FindSubData(CurrentSequenceID))
			{
				NewLocalSequenceID = NewLocalSequenceID.AccumulateParentID(SubData->DeterministicSequenceID);
			}
			
			CurrentSequenceID = CurrentNode->ParentID;
		}
	}

	SetCurrentValue(InValue);
}

#undef LOCTEXT_NAMESPACE
