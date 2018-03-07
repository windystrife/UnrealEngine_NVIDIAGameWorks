// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DisplayNodes/SequencerDisplayNode.h"
#include "Curves/KeyHandle.h"
#include "Widgets/SNullWidget.h"
#include "Rendering/DrawElements.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Layout/SSpacer.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "DisplayNodes/SequencerObjectBindingNode.h"
#include "DisplayNodes/SequencerSectionCategoryNode.h"
#include "DisplayNodes/SequencerSectionKeyAreaNode.h"
#include "IKeyArea.h"
#include "DisplayNodes/SequencerTrackNode.h"
#include "Sequencer.h"
#include "GroupedKeyArea.h"
#include "SAnimationOutlinerTreeNode.h"
#include "SequencerSettings.h"
#include "SSequencerSectionAreaView.h"
#include "CommonMovieSceneTools.h"
#include "Framework/Commands/GenericCommands.h"
#include "ScopedTransaction.h"
#include "SequencerKeyTimeCache.h"

#define LOCTEXT_NAMESPACE "SequencerDisplayNode"


/** When 0, regeneration of dynamic key groups is enabled, when non-zero, such behaviour is disabled */
FThreadSafeCounter KeyGroupRegenerationLock;

namespace SequencerNodeConstants
{
	extern const float CommonPadding;
	const float CommonPadding = 4.f;

	static const FVector2D KeyMarkSize = FVector2D(3.f, 21.f);
}

struct FNameAndSignature
{
	FGuid Signature;
	FName Name;

	bool IsValid() const
	{
		return Signature.IsValid() && !Name.IsNone();
	}

	friend bool operator==(const FNameAndSignature& A, const FNameAndSignature& B)
	{
		return A.Signature == B.Signature && A.Name == B.Name;
	}

	friend uint32 GetTypeHash(const FNameAndSignature& In)
	{
		return HashCombine(GetTypeHash(In.Signature), GetTypeHash(In.Name));
	}
};

class SSequencerObjectTrack
	: public SLeafWidget
{
public:

	SLATE_BEGIN_ARGS(SSequencerObjectTrack) {}
		/** The view range of the section area */
		SLATE_ATTRIBUTE( TRange<float>, ViewRange )
	SLATE_END_ARGS()

	/** SLeafWidget Interface */
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	void Construct( const FArguments& InArgs, TSharedRef<FSequencerDisplayNode> InRootNode )
	{
		RootNode = InRootNode;
		
		ViewRange = InArgs._ViewRange;

		check(RootNode->GetType() == ESequencerNode::Object);
	}

protected:
	// Begin SWidget overrides.
	virtual FVector2D ComputeDesiredSize(float) const override;
	// End SWidget overrides.

private:

	/** Collects all key times from the root node */
	void GenerateCachedKeyPositions(const FGeometry& AllottedGeometry);

private:

	/** Root node of this track view panel */
	TSharedPtr<FSequencerDisplayNode> RootNode;

	/** The current view range */
	TAttribute< TRange<float> > ViewRange;

	FSequencerKeyCollectionSignature KeyCollectionSignature;

	/** The time-range for which KeyDrawPositions was generated */
	TRange<float> CachedViewRange;
	/** Cached pixel positions for all keys in the current view range */
	TArray<float> KeyDrawPositions;
	/** Cached key times per key area. Updated when section signature changes */
	TMap<FNameAndSignature, FSequencerCachedKeys> SectionToKeyTimeCache;
};


void SSequencerObjectTrack::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	FSequencerKeyCollectionSignature NewCollectionSignature = FSequencerKeyCollectionSignature::FromNodesRecursive({ RootNode.Get() }, 0.f);
	if (NewCollectionSignature != KeyCollectionSignature || CachedViewRange != ViewRange.Get())
	{
		CachedViewRange = ViewRange.Get();
		KeyCollectionSignature = MoveTemp(NewCollectionSignature);
		GenerateCachedKeyPositions(AllottedGeometry);
	}
}

int32 SSequencerObjectTrack::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (RootNode->GetSequencer().GetSettings()->GetShowCombinedKeyframes())
	{
		for (float KeyPosition : KeyDrawPositions)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId+1,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(
						KeyPosition - FMath::CeilToFloat(SequencerNodeConstants::KeyMarkSize.X/2.f),
						FMath::CeilToFloat(AllottedGeometry.GetLocalSize().Y/2.f - SequencerNodeConstants::KeyMarkSize.Y/2.f)
					),
					SequencerNodeConstants::KeyMarkSize
				),
				FEditorStyle::GetBrush("Sequencer.KeyMark"),
				ESlateDrawEffect::None,
				FLinearColor(1.f, 1.f, 1.f, 1.f)
			);
		}
		return LayerId+1;
	}

	return LayerId;
}


FVector2D SSequencerObjectTrack::ComputeDesiredSize( float ) const
{
	// Note: X Size is not used
	return FVector2D( 100.0f, RootNode->GetNodeHeight() );
}


void SSequencerObjectTrack::GenerateCachedKeyPositions(const FGeometry& AllottedGeometry)
{
	static float DuplicateThresholdPx = 3.f;

	// Swap the last frame's cache with a temporary so we start this frame's cache from a clean slate
	TMap<FNameAndSignature, FSequencerCachedKeys> PreviouslyCachedKeyTimes;
	Swap(PreviouslyCachedKeyTimes, SectionToKeyTimeCache);

	// Unnamed key areas are uncacheable, so we track those separately
	TArray<FSequencerCachedKeys> UncachableKeyTimes;

	// First off, accumulate (and cache) KeyDrawPositions as times, we convert to positions in the later loop
	for (auto& CachePair : KeyCollectionSignature.GetKeyAreas())
	{
		TSharedRef<IKeyArea> KeyArea = CachePair.Key;

		UMovieSceneSection* Section = KeyArea->GetOwningSection();
		FNameAndSignature CacheKey{ CachePair.Value, KeyArea->GetName() };

		// If we cached this last frame, use those key times again
		FSequencerCachedKeys* CachedKeyTimes = CacheKey.IsValid() ? PreviouslyCachedKeyTimes.Find(CacheKey) : nullptr;
		if (CachedKeyTimes)
		{
			SectionToKeyTimeCache.Add(CacheKey, MoveTemp(*CachedKeyTimes));
			continue;
		}

		// Generate a new cache
		FSequencerCachedKeys TempCache;
		TempCache.Update(KeyArea);

		if (CacheKey.IsValid())
		{
			SectionToKeyTimeCache.Add(CacheKey, MoveTemp(TempCache));
		}
		else
		{
			UncachableKeyTimes.Add(MoveTemp(TempCache));
		}
	}

	KeyDrawPositions.Reset();

	// Instead of accumulating all key times into a single array and then sorting (which doesn't scale well with large numbers),
	// we use a collection of iterators that are only incremented when they've been added to the KeyDrawPositions array
	struct FIter
	{
		FIter(TArrayView<const FSequencerCachedKey> In) : KeysInRange(In), CurrentIndex(0) {}

		explicit                     operator bool() const   { return KeysInRange.IsValidIndex(CurrentIndex); }
		FIter&                       operator++()            { ++CurrentIndex; return *this; }

		const FSequencerCachedKey&   operator*() const       { return KeysInRange[CurrentIndex]; }
		const FSequencerCachedKey*   operator->() const      { return &KeysInRange[CurrentIndex]; }
	private:
		TArrayView<const FSequencerCachedKey> KeysInRange;
		int32 CurrentIndex;
	};

	TArray<FIter> AllIterators;
	for (auto& Pair : SectionToKeyTimeCache)
	{
		AllIterators.Add(Pair.Value.GetKeysInRange(CachedViewRange));
	}
	for (auto& Uncached: UncachableKeyTimes)
	{
		AllIterators.Add(Uncached.GetKeysInRange(CachedViewRange));
	}

	FTimeToPixel TimeToPixelConverter(AllottedGeometry, CachedViewRange);

	// While any iterator is still valid, find and add the earliest time
	while (AllIterators.ContainsByPredicate([](FIter& It){ return It; }))
	{
		float EarliestTime = TNumericLimits<float>::Max();
		for (FIter& It : AllIterators)
		{
			if (It && It->Time < EarliestTime)
			{
				EarliestTime = It->Time;
			}
		}

		// Add the position as a pixel position
		const float KeyPosition = TimeToPixelConverter.TimeToPixel(EarliestTime);
		KeyDrawPositions.Add(KeyPosition);

		// Increment any other iterators that are close enough to the time we just added
		for (FIter& It : AllIterators)
		{
			while (It && FMath::IsNearlyEqual(KeyPosition, TimeToPixelConverter.TimeToPixel(It->Time), DuplicateThresholdPx))
			{
				++It;
			}
		}
	}
}


FSequencerDisplayNode::FSequencerDisplayNode( FName InNodeName, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree )
	: VirtualTop( 0.f )
	, VirtualBottom( 0.f )
	, ParentNode( InParentNode )
	, ParentTree( InParentTree )
	, NodeName( InNodeName )
	, bExpanded( false )
{
}


void FSequencerDisplayNode::Initialize(float InVirtualTop, float InVirtualBottom)
{
	bExpanded = ParentTree.GetSavedExpansionState( *this );

	VirtualTop = InVirtualTop;
	VirtualBottom = InVirtualBottom;
}


void FSequencerDisplayNode::AddObjectBindingNode(TSharedRef<FSequencerObjectBindingNode> ObjectBindingNode)
{
	AddChildAndSetParent( ObjectBindingNode );
}


TSharedPtr<FSequencerObjectBindingNode> FSequencerDisplayNode::FindParentObjectBindingNode() const
{
	TSharedPtr<FSequencerDisplayNode> CurrentParentNode = GetParent();

	while (CurrentParentNode.IsValid())
	{
		if (CurrentParentNode.Get()->GetType() == ESequencerNode::Object)
		{
			TSharedPtr<FSequencerObjectBindingNode> ObjectNode = StaticCastSharedPtr<FSequencerObjectBindingNode>(CurrentParentNode);
			if (ObjectNode.IsValid())
			{
				return ObjectNode;
			}
		}
		CurrentParentNode = CurrentParentNode->GetParent();
	}

	return nullptr;
}


bool FSequencerDisplayNode::Traverse_ChildFirst(const TFunctionRef<bool(FSequencerDisplayNode&)>& InPredicate, bool bIncludeThisNode)
{
	for (auto& Child : GetChildNodes())
	{
		if (!Child->Traverse_ChildFirst(InPredicate, true))
		{
			return false;
		}
	}

	return bIncludeThisNode ? InPredicate(*this) : true;
}


bool FSequencerDisplayNode::Traverse_ParentFirst(const TFunctionRef<bool(FSequencerDisplayNode&)>& InPredicate, bool bIncludeThisNode)
{
	if (bIncludeThisNode && !InPredicate(*this))
	{
		return false;
	}

	for (auto& Child : GetChildNodes())
	{
		if (!Child->Traverse_ParentFirst(InPredicate, true))
		{
			return false;
		}
	}

	return true;
}


bool FSequencerDisplayNode::TraverseVisible_ChildFirst(const TFunctionRef<bool(FSequencerDisplayNode&)>& InPredicate, bool bIncludeThisNode)
{
	// If the item is not expanded, its children ain't visible
	if (IsExpanded())
	{
		for (auto& Child : GetChildNodes())
		{
			if (!Child->IsHidden() && !Child->TraverseVisible_ChildFirst(InPredicate, true))
			{
				return false;
			}
		}
	}

	if (bIncludeThisNode && !IsHidden())
	{
		return InPredicate(*this);
	}

	// Continue iterating regardless of visibility
	return true;
}


bool FSequencerDisplayNode::TraverseVisible_ParentFirst(const TFunctionRef<bool(FSequencerDisplayNode&)>& InPredicate, bool bIncludeThisNode)
{
	if (bIncludeThisNode && !IsHidden() && !InPredicate(*this))
	{
		return false;
	}

	// If the item is not expanded, its children ain't visible
	if (IsExpanded())
	{
		for (auto& Child : GetChildNodes())
		{
			if (!Child->IsHidden() && !Child->TraverseVisible_ParentFirst(InPredicate, true))
			{
				return false;
			}
		}
	}

	return true;
}

TSharedRef<FSequencerSectionCategoryNode> FSequencerDisplayNode::AddCategoryNode( FName CategoryName, const FText& DisplayLabel )
{
	TSharedPtr<FSequencerSectionCategoryNode> CategoryNode;

	// See if there is an already existing category node to use
	for (const TSharedRef<FSequencerDisplayNode>& Node : ChildNodes)
	{
		if ((Node->GetNodeName() == CategoryName) && (Node->GetType() == ESequencerNode::Category))
		{
			CategoryNode = StaticCastSharedRef<FSequencerSectionCategoryNode>(Node);
		}
	}

	if (!CategoryNode.IsValid())
	{
		// No existing category found, make a new one
		CategoryNode = MakeShareable(new FSequencerSectionCategoryNode(CategoryName, DisplayLabel, SharedThis(this), ParentTree));
		ChildNodes.Add(CategoryNode.ToSharedRef());
	}

	return CategoryNode.ToSharedRef();
}


void FSequencerDisplayNode::AddKeyAreaNode(FName KeyAreaName, const FText& DisplayName, TSharedRef<IKeyArea> KeyArea)
{
	TSharedPtr<FSequencerSectionKeyAreaNode> KeyAreaNode;

	// See if there is an already existing key area node to use
	for (const TSharedRef<FSequencerDisplayNode>& Node : ChildNodes)
	{
		if ((Node->GetNodeName() == KeyAreaName) && (Node->GetType() == ESequencerNode::KeyArea))
		{
			KeyAreaNode = StaticCastSharedRef<FSequencerSectionKeyAreaNode>(Node);
		}
	}

	if (!KeyAreaNode.IsValid())
	{
		// No existing node found make a new one
		KeyAreaNode = MakeShareable( new FSequencerSectionKeyAreaNode( KeyAreaName, DisplayName, SharedThis( this ), ParentTree ) );
		ChildNodes.Add(KeyAreaNode.ToSharedRef());
	}

	KeyAreaNode->AddKeyArea(KeyArea);
}

FLinearColor FSequencerDisplayNode::GetDisplayNameColor() const
{
	return FLinearColor( 1.f, 1.f, 1.f, 1.f );
}

FText FSequencerDisplayNode::GetDisplayNameToolTipText() const
{
	return FText();
}

TSharedRef<SWidget> FSequencerDisplayNode::GenerateContainerWidgetForOutliner(const TSharedRef<SSequencerTreeViewRow>& InRow)
{
	auto NewWidget = SNew(SAnimationOutlinerTreeNode, SharedThis(this), InRow)
	.IconBrush(this, &FSequencerDisplayNode::GetIconBrush)
	.IconColor(this, &FSequencerDisplayNode::GetIconColor)
	.IconOverlayBrush(this, &FSequencerDisplayNode::GetIconOverlayBrush)
	.IconToolTipText(this, &FSequencerDisplayNode::GetIconToolTipText)
	.CustomContent()
	[
		GetCustomOutlinerContent()
	];

	return NewWidget;
}

TSharedRef<SWidget> FSequencerDisplayNode::GetCustomOutlinerContent()
{
	return SNew(SSpacer);
}

const FSlateBrush* FSequencerDisplayNode::GetIconBrush() const
{
	return nullptr;
}

const FSlateBrush* FSequencerDisplayNode::GetIconOverlayBrush() const
{
	return nullptr;
}

FSlateColor FSequencerDisplayNode::GetIconColor() const
{
	return FSlateColor( FLinearColor::White );
}

FText FSequencerDisplayNode::GetIconToolTipText() const
{
	return FText();
}

TSharedRef<SWidget> FSequencerDisplayNode::GenerateWidgetForSectionArea(const TAttribute< TRange<float> >& ViewRange)
{
	if (GetType() == ESequencerNode::Track && static_cast<FSequencerTrackNode&>(*this).GetSubTrackMode() != FSequencerTrackNode::ESubTrackMode::ParentTrack)
	{
		return SNew(SSequencerSectionAreaView, SharedThis(this))
			.ViewRange(ViewRange);
	}
	
	if (GetType() == ESequencerNode::Object)
	{
		return SNew(SSequencerObjectTrack, SharedThis(this))
			.ViewRange(ViewRange);
	}

	// currently only section areas display widgets
	return SNullWidget::NullWidget;
}


TSharedPtr<FSequencerDisplayNode> FSequencerDisplayNode::GetSectionAreaAuthority() const
{
	TSharedPtr<FSequencerDisplayNode> Authority = SharedThis(const_cast<FSequencerDisplayNode*>(this));

	while (Authority.IsValid())
	{
		if (Authority->GetType() == ESequencerNode::Object || Authority->GetType() == ESequencerNode::Track)
		{
			return Authority;
		}
		else
		{
			Authority = Authority->GetParent();
		}
	}

	return Authority;
}


FString FSequencerDisplayNode::GetPathName() const
{
	// First get our parent's path
	FString PathName;

	if (ParentNode.IsValid())
	{
		PathName = ParentNode.Pin()->GetPathName() + TEXT(".");
	}

	//then append our path
	PathName += GetNodeName().ToString();

	return PathName;
}


TSharedPtr<SWidget> FSequencerDisplayNode::OnSummonContextMenu()
{
	// @todo sequencer replace with UI Commands instead of faking it
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, GetSequencer().GetCommandBindings());

	BuildContextMenu(MenuBuilder);

	return MenuBuilder.MakeWidget();
}


namespace
{
	void AddEvalOptionsPropertyMenuItem(FMenuBuilder& MenuBuilder, FCanExecuteAction InCanExecute, const TArray<UMovieSceneTrack*>& AllTracks, const UBoolProperty* Property, TFunction<bool(UMovieSceneTrack*)> Validator = nullptr)
	{
		bool bIsChecked = AllTracks.ContainsByPredicate(
			[=](UMovieSceneTrack* InTrack)
			{
				return (!Validator || Validator(InTrack)) && Property->GetPropertyValue(Property->ContainerPtrToValuePtr<void>(&InTrack->EvalOptions));
			});

		MenuBuilder.AddMenuEntry(
			Property->GetDisplayNameText(),
			Property->GetToolTipText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([AllTracks, Property, Validator, bIsChecked]{
					FScopedTransaction Transaction(FText::Format(NSLOCTEXT("Sequencer", "TrackNodeSetRoundEvaluation", "Set '{0}'"), Property->GetDisplayNameText()));
					for (UMovieSceneTrack* Track : AllTracks)
					{
						if (Validator && !Validator(Track))
						{
							continue;
						}
						void* PropertyContainer = Property->ContainerPtrToValuePtr<void>(&Track->EvalOptions);
						Track->Modify();
						Property->SetPropertyValue(PropertyContainer, !bIsChecked);
					}
				}),
				InCanExecute,
				FIsActionChecked::CreateLambda([=]{ return bIsChecked; })
			),
			NAME_None,
			EUserInterfaceActionType::Check
		);
	}
}


void FSequencerDisplayNode::BuildContextMenu(FMenuBuilder& MenuBuilder)
{
	TSharedRef<FSequencerDisplayNode> ThisNode = SharedThis(this);

	bool bIsReadOnly = !GetSequencer().IsReadOnly();
	FCanExecuteAction CanExecute = FCanExecuteAction::CreateLambda([bIsReadOnly]{ return bIsReadOnly; });

	MenuBuilder.BeginSection("Edit", LOCTEXT("EditContextMenuSectionName", "Edit"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ToggleNodeActive", "Active"),
			LOCTEXT("ToggleNodeActiveTooltip", "Set this track or selected tracks active/inactive"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(&GetSequencer(), &FSequencer::ToggleNodeActive),
				CanExecute,
				FIsActionChecked::CreateSP(&GetSequencer(), &FSequencer::IsNodeActive)
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ToggleNodeLock", "Locked"),
			LOCTEXT("ToggleNodeLockTooltip", "Lock or unlock this node or selected tracks"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(&GetSequencer(), &FSequencer::ToggleNodeLocked),
				CanExecute,
				FIsActionChecked::CreateSP(&GetSequencer(), &FSequencer::IsNodeLocked)
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);


		// Add cut, copy and paste functions to the tracks
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Cut);

		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Copy);
		
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Paste);
		
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DeleteNode", "Delete"),
			LOCTEXT("DeleteNodeTooltip", "Delete this or selected tracks"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(&GetSequencer(), &FSequencer::DeleteNode, ThisNode), CanExecute)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("RenameNode", "Rename"),
			LOCTEXT("RenameNodeTooltip", "Rename this track"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FSequencerDisplayNode::HandleContextMenuRenameNodeExecute),
				FCanExecuteAction::CreateSP(this, &FSequencerDisplayNode::HandleContextMenuRenameNodeCanExecute)
			)
		);
	}
	MenuBuilder.EndSection();

	TArray<UMovieSceneTrack*> AllTracks;
	for (TSharedRef<FSequencerDisplayNode> Node : GetSequencer().GetSelection().GetSelectedOutlinerNodes())
	{
		if (Node->GetType() == ESequencerNode::Track)
		{
			UMovieSceneTrack* Track = static_cast<FSequencerTrackNode&>(Node.Get()).GetTrack();
			if (Track)
			{
				AllTracks.Add(Track);
			}
		}
	}
	
	if (AllTracks.Num())
	{
		MenuBuilder.BeginSection("GeneralTrackOptions", NSLOCTEXT("Sequencer", "TrackNodeGeneralOptions", "Track Options"));
		{
			UStruct* EvalOptionsStruct = FMovieSceneTrackEvalOptions::StaticStruct();

			const UBoolProperty* NearestSectionProperty = Cast<UBoolProperty>(EvalOptionsStruct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMovieSceneTrackEvalOptions, bEvalNearestSection)));
			auto CanEvaluateNearest = [](UMovieSceneTrack* InTrack) { return InTrack->EvalOptions.bCanEvaluateNearestSection != 0; };
			if (NearestSectionProperty && AllTracks.ContainsByPredicate(CanEvaluateNearest))
			{
				TFunction<bool(UMovieSceneTrack*)> Validator = CanEvaluateNearest;
				AddEvalOptionsPropertyMenuItem(MenuBuilder, CanExecute, AllTracks, NearestSectionProperty, Validator);
			}

			const UBoolProperty* PrerollProperty = Cast<UBoolProperty>(EvalOptionsStruct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMovieSceneTrackEvalOptions, bEvaluateInPreroll)));
			if (PrerollProperty)
			{
				AddEvalOptionsPropertyMenuItem(MenuBuilder, CanExecute, AllTracks, PrerollProperty);
			}

			const UBoolProperty* PostrollProperty = Cast<UBoolProperty>(EvalOptionsStruct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMovieSceneTrackEvalOptions, bEvaluateInPostroll)));
			if (PostrollProperty)
			{
				AddEvalOptionsPropertyMenuItem(MenuBuilder, CanExecute, AllTracks, PostrollProperty);
			}
		}
		MenuBuilder.EndSection();
	}
}


void FSequencerDisplayNode::GetChildKeyAreaNodesRecursively(TArray< TSharedRef<FSequencerSectionKeyAreaNode> >& OutNodes) const
{
	for (const TSharedRef<FSequencerDisplayNode>& Node : ChildNodes)
	{
		if (Node->GetType() == ESequencerNode::KeyArea)
		{
			OutNodes.Add(StaticCastSharedRef<FSequencerSectionKeyAreaNode>(Node));
		}

		Node->GetChildKeyAreaNodesRecursively(OutNodes);
	}
}


void FSequencerDisplayNode::SetExpansionState(bool bInExpanded)
{
	bExpanded = bInExpanded;

	// Expansion state has changed, save it to the movie scene now
	ParentTree.SaveExpansionState(*this, bExpanded);
}


bool FSequencerDisplayNode::IsExpanded() const
{
	return bExpanded;
}


bool FSequencerDisplayNode::IsHidden() const
{
	return ParentTree.HasActiveFilter() && !ParentTree.IsNodeFiltered(AsShared());
}


bool FSequencerDisplayNode::IsHovered() const
{
	return ParentTree.GetHoveredNode().Get() == this;
}


TSharedRef<FGroupedKeyArea> FSequencerDisplayNode::GetKeyGrouping(UMovieSceneSection* InSection)
{
	TSharedRef<FGroupedKeyArea>* KeyGroup = KeyGroupings.FindByPredicate([=](const TSharedRef<FGroupedKeyArea>& InKeyArea) { return InKeyArea->GetOwningSection() == InSection; });
	if (KeyGroup)
	{
		if (KeyGroupRegenerationLock.GetValue() == 0)
		{
			(*KeyGroup)->Update();
		}
		return *KeyGroup;
	}

	// Just make a new one
	KeyGroupings.Emplace(MakeShared<FGroupedKeyArea>(*this, InSection));
	return KeyGroupings.Last();
}


void FSequencerDisplayNode::HandleContextMenuRenameNodeExecute()
{
	RenameRequestedEvent.Broadcast();
}


bool FSequencerDisplayNode::HandleContextMenuRenameNodeCanExecute() const
{
	return CanRenameNode();
}


void FSequencerDisplayNode::DisableKeyGoupingRegeneration()
{
	KeyGroupRegenerationLock.Increment();
}


void FSequencerDisplayNode::EnableKeyGoupingRegeneration()
{
	KeyGroupRegenerationLock.Decrement();
}


void FSequencerDisplayNode::AddChildAndSetParent( TSharedRef<FSequencerDisplayNode> InChild )
{
	ChildNodes.Add( InChild );
	InChild->ParentNode = SharedThis( this );
}


#undef LOCTEXT_NAMESPACE
