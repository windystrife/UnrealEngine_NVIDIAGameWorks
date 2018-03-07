// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSequencerTreeView.h"
#include "SSequencerTrackLane.h"
#include "EditorStyleSet.h"
#include "SequencerDisplayNodeDragDropOp.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

static FName TrackAreaName = "TrackArea";

namespace Utils
{
	enum class ESearchState { Before, After, Found };

	template<typename T, typename F>
	const T* BinarySearch(const TArray<T>& InContainer, const F& InPredicate)
	{
		int32 Min = 0;
		int32 Max = InContainer.Num();

		ESearchState State = ESearchState::Before;

		for ( ; Min != Max ; )
		{
			int32 SearchIndex = Min + (Max - Min) / 2;

			auto& Item = InContainer[SearchIndex];
			State = InPredicate(Item);
			switch (State)
			{
				case ESearchState::Before:	Max = SearchIndex; break;
				case ESearchState::After:	Min = SearchIndex + 1; break;
				case ESearchState::Found: 	return &Item;
			}
		}

		return nullptr;
	}
}

SSequencerTreeViewRow::~SSequencerTreeViewRow()
{
	auto TreeView = StaticCastSharedPtr<SSequencerTreeView>(OwnerTablePtr.Pin());
	auto PinnedNode = Node.Pin();
	if (TreeView.IsValid() && PinnedNode.IsValid())
	{
		TreeView->OnChildRowRemoved(PinnedNode.ToSharedRef());
	}
}

/** Construct function for this widget */
void SSequencerTreeViewRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView, const FDisplayNodeRef& InNode)
{
	Node = InNode;
	OnGenerateWidgetForColumn = InArgs._OnGenerateWidgetForColumn;

	SMultiColumnTableRow::Construct(
		SMultiColumnTableRow::FArguments()
			.OnDragDetected(this, &SSequencerTreeViewRow::OnDragDetected)
			.OnCanAcceptDrop(this, &SSequencerTreeViewRow::OnCanAcceptDrop)
			.OnAcceptDrop(this, &SSequencerTreeViewRow::OnAcceptDrop),
		OwnerTableView);
}

TSharedRef<SWidget> SSequencerTreeViewRow::GenerateWidgetForColumn(const FName& ColumnId)
{
	auto PinnedNode = Node.Pin();
	if (PinnedNode.IsValid())
	{
		return OnGenerateWidgetForColumn.Execute(PinnedNode.ToSharedRef(), ColumnId, SharedThis(this));
	}

	return SNullWidget::NullWidget;
}

FReply SSequencerTreeViewRow::OnDragDetected( const FGeometry& InGeometry, const FPointerEvent& InPointerEvent )
{
	TSharedPtr<FSequencerDisplayNode> DisplayNode = Node.Pin();
	if ( DisplayNode.IsValid() )
	{
		FSequencer& Sequencer = DisplayNode->GetParentTree().GetSequencer();
		if ( Sequencer.GetSelection().GetSelectedOutlinerNodes().Num() > 0 )
		{
			TArray<TSharedRef<FSequencerDisplayNode> > DraggableNodes;
			for ( const TSharedRef<FSequencerDisplayNode> SelectedNode : Sequencer.GetSelection().GetSelectedOutlinerNodes() )
			{
				if ( SelectedNode->CanDrag() )
				{
					DraggableNodes.Add(SelectedNode);
				}
			}

			FText DefaultText = FText::Format( NSLOCTEXT( "SequencerTreeViewRow", "DefaultDragDropFormat", "Move {0} item(s)" ), FText::AsNumber( DraggableNodes.Num() ) );
			TSharedRef<FSequencerDisplayNodeDragDropOp> DragDropOp = FSequencerDisplayNodeDragDropOp::New( DraggableNodes, DefaultText, nullptr );

			return FReply::Handled().BeginDragDrop( DragDropOp );
		}
	}
	return FReply::Unhandled();
}

TOptional<EItemDropZone> SSequencerTreeViewRow::OnCanAcceptDrop( const FDragDropEvent& DragDropEvent, EItemDropZone InItemDropZone, FDisplayNodeRef DisplayNode )
{
	TSharedPtr<FSequencerDisplayNodeDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FSequencerDisplayNodeDragDropOp>();
	if ( DragDropOp.IsValid() )
	{
		DragDropOp->ResetToDefaultToolTip();
		TOptional<EItemDropZone> AllowedDropZone = DisplayNode->CanDrop( *DragDropOp, InItemDropZone );
		if ( AllowedDropZone.IsSet() == false )
		{
			DragDropOp->CurrentIconBrush = FEditorStyle::GetBrush( TEXT( "Graph.ConnectorFeedback.Error" ) );
		}
		return AllowedDropZone;
	}
	return TOptional<EItemDropZone>();
}

FReply SSequencerTreeViewRow::OnAcceptDrop( const FDragDropEvent& DragDropEvent, EItemDropZone InItemDropZone, FDisplayNodeRef DisplayNode )
{
	TSharedPtr<FSequencerDisplayNodeDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FSequencerDisplayNodeDragDropOp>();
	if ( DragDropOp.IsValid())
	{
		DisplayNode->Drop( DragDropOp->GetDraggedNodes(), InItemDropZone );
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

TSharedPtr<FSequencerDisplayNode> SSequencerTreeViewRow::GetDisplayNode() const
{
	return Node.Pin();
}

void SSequencerTreeViewRow::AddTrackAreaReference(const TSharedPtr<SSequencerTrackLane>& Lane)
{
	TrackLaneReference = Lane;
}

void SSequencerTreeViewRow::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	StaticCastSharedPtr<SSequencerTreeView>(OwnerTablePtr.Pin())->ReportChildRowGeometry(Node.Pin().ToSharedRef(), AllottedGeometry);
}

void SSequencerTreeView::Construct(const FArguments& InArgs, const TSharedRef<FSequencerNodeTree>& InNodeTree, const TSharedRef<SSequencerTrackArea>& InTrackArea)
{
	SequencerNodeTree = InNodeTree;
	TrackArea = InTrackArea;
	bUpdatingSequencerSelection = false;
	bUpdatingTreeSelection = false;
	bSequencerSelectionChangeBroadcastWasSupressed = false;
	bPhysicalNodesNeedUpdate = false;

	// We 'leak' these delegates (they'll get cleaned up automatically when the invocation list changes)
	// It's not safe to attempt their removal in ~SSequencerTreeView because SequencerNodeTree->GetSequencer() may not be valid
	FSequencer& Sequencer = InNodeTree->GetSequencer();
	Sequencer.GetSelection().GetOnOutlinerNodeSelectionChanged().AddSP(this, &SSequencerTreeView::SynchronizeTreeSelectionWithSequencerSelection);

	HeaderRow = SNew(SHeaderRow).Visibility(EVisibility::Collapsed);
	OnGetContextMenuContent = InArgs._OnGetContextMenuContent;

	SetupColumns(InArgs);

	STreeView::Construct
	(
		STreeView::FArguments()
		.TreeItemsSource(&RootNodes)
		.SelectionMode(ESelectionMode::Multi)
		.OnGenerateRow(this, &SSequencerTreeView::OnGenerateRow)
		.OnGetChildren(this, &SSequencerTreeView::OnGetChildren)
		.HeaderRow(HeaderRow)
		.ExternalScrollbar(InArgs._ExternalScrollbar)
		.OnExpansionChanged(this, &SSequencerTreeView::OnExpansionChanged)
		.AllowOverscroll(EAllowOverscroll::No)
		.OnContextMenuOpening( this, &SSequencerTreeView::OnContextMenuOpening )
	);
}

void SSequencerTreeView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	STreeView::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	
	// These are updated in both tick and paint since both calls can cause changes to the cached rows and the data needs
	// to be kept synchronized so that external measuring calls get correct and reliable results.
	if (bPhysicalNodesNeedUpdate)
	{
		PhysicalNodes.Reset();
		CachedRowGeometry.GenerateValueArray(PhysicalNodes);

		PhysicalNodes.Sort([](const FCachedGeometry& A, const FCachedGeometry& B) {
			return A.PhysicalTop < B.PhysicalTop;
		});
	}

	HighlightRegion = TOptional<FHighlightRegion>();

	if (SequencerNodeTree->GetHoveredNode().IsValid())
	{
		TSharedRef<FSequencerDisplayNode> OutermostParent = SequencerNodeTree->GetHoveredNode()->GetOutermostParent();
		if (OutermostParent->GetType() == ESequencerNode::Spacer)
		{
			return;
		}

		TOptional<float> PhysicalTop = ComputeNodePosition(OutermostParent);

		if (PhysicalTop.IsSet())
		{
			// Compute total height of the highlight
			float TotalHeight = 0.f;
			OutermostParent->TraverseVisible_ParentFirst([&](FSequencerDisplayNode& InNode){
				TotalHeight += InNode.GetNodeHeight() + InNode.GetNodePadding().Combined();
				return true;
			});

			HighlightRegion = FHighlightRegion(PhysicalTop.GetValue(), PhysicalTop.GetValue() + TotalHeight);
		}
	}
}

int32 SSequencerTreeView::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = STreeView::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	// These are updated in both tick and paint since both calls can cause changes to the cached rows and the data needs
	// to be kept synchronized so that external measuring calls get correct and reliable results.
	if (bPhysicalNodesNeedUpdate)
	{
		PhysicalNodes.Reset();
		CachedRowGeometry.GenerateValueArray(PhysicalNodes);

		PhysicalNodes.Sort([](const FCachedGeometry& A, const FCachedGeometry& B) {
			return A.PhysicalTop < B.PhysicalTop;
		});
	}

	if (HighlightRegion.IsSet())
	{
		// Black tint for highlighted regions
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId+1,
			AllottedGeometry.ToPaintGeometry(FVector2D(2.f, HighlightRegion->Top - 4.f), FVector2D(AllottedGeometry.Size.X - 4.f, 4.f)),
			FEditorStyle::GetBrush("Sequencer.TrackHoverHighlight_Top"),
			ESlateDrawEffect::None,
			FLinearColor::Black
		);
		
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId+1,
			AllottedGeometry.ToPaintGeometry(FVector2D(2.f, HighlightRegion->Bottom), FVector2D(AllottedGeometry.Size.X - 4.f, 4.f)),
			FEditorStyle::GetBrush("Sequencer.TrackHoverHighlight_Bottom"),
			ESlateDrawEffect::None,
			FLinearColor::Black
		);
	}

	return LayerId + 1;
}

TOptional<SSequencerTreeView::FCachedGeometry> SSequencerTreeView::GetPhysicalGeometryForNode(const FDisplayNodeRef& InNode) const
{
	if (const FCachedGeometry* FoundGeometry = CachedRowGeometry.Find(InNode))
	{
		return *FoundGeometry;
	}

	return TOptional<FCachedGeometry>();
}

TOptional<float> SSequencerTreeView::ComputeNodePosition(const FDisplayNodeRef& InNode) const
{
	// Positioning strategy:
	// Attempt to root out any visible node in the specified node's sub-hierarchy, and compute the node's offset from that
	float NegativeOffset = 0.f;
	TOptional<float> Top;
	
	// Iterate parent first until we find a tree view row we can use for the offset height
	auto Iter = [&](FSequencerDisplayNode& InDisplayNode){
		
		TOptional<FCachedGeometry> ChildRowGeometry = GetPhysicalGeometryForNode(InDisplayNode.AsShared());
		if (ChildRowGeometry.IsSet())
		{
			Top = ChildRowGeometry->PhysicalTop;
			// Stop iterating
			return false;
		}

		NegativeOffset -= InDisplayNode.GetNodeHeight() + InDisplayNode.GetNodePadding().Combined();
		return true;
	};

	InNode->TraverseVisible_ParentFirst(Iter);

	if (Top.IsSet())
	{
		return NegativeOffset + Top.GetValue();
	}

	return Top;
}

void SSequencerTreeView::ReportChildRowGeometry(const FDisplayNodeRef& InNode, const FGeometry& InGeometry)
{
	float ChildOffset = TransformPoint(
		Concatenate(
			InGeometry.GetAccumulatedLayoutTransform(),
			GetCachedGeometry().GetAccumulatedLayoutTransform().Inverse()
		),
		FVector2D(0,0)
	).Y;

	CachedRowGeometry.Add(InNode, FCachedGeometry(InNode, ChildOffset, InGeometry.Size.Y));
	bPhysicalNodesNeedUpdate = true;
}

void SSequencerTreeView::OnChildRowRemoved(const FDisplayNodeRef& InNode)
{
	CachedRowGeometry.Remove(InNode);
	bPhysicalNodesNeedUpdate = true;
}

TSharedPtr<FSequencerDisplayNode> SSequencerTreeView::HitTestNode(float InPhysical) const
{
	auto* Found = Utils::BinarySearch<FCachedGeometry>(PhysicalNodes, [&](const FCachedGeometry& In){

		if (InPhysical < In.PhysicalTop)
		{
			return Utils::ESearchState::Before;
		}
		else if (InPhysical > In.PhysicalTop + In.PhysicalHeight)
		{
			return Utils::ESearchState::After;
		}

		return Utils::ESearchState::Found;

	});

	if (Found)
	{
		return Found->Node;
	}
	
	return nullptr;
}

float SSequencerTreeView::PhysicalToVirtual(float InPhysical) const
{
	int32 SearchIndex = PhysicalNodes.Num() / 2;

	auto* Found = Utils::BinarySearch<FCachedGeometry>(PhysicalNodes, [&](const FCachedGeometry& In){

		if (InPhysical < In.PhysicalTop)
		{
			return Utils::ESearchState::Before;
		}
		else if (InPhysical > In.PhysicalTop + In.PhysicalHeight)
		{
			return Utils::ESearchState::After;
		}

		return Utils::ESearchState::Found;

	});


	if (Found)
	{
		const float FractionalHeight = (InPhysical - Found->PhysicalTop) / Found->PhysicalHeight;
		return Found->Node->GetVirtualTop() + (Found->Node->GetVirtualBottom() - Found->Node->GetVirtualTop()) * FractionalHeight;
	}

	if (PhysicalNodes.Num())
	{
		auto& Last = PhysicalNodes.Last();
		return Last.Node->GetVirtualTop() + (InPhysical - Last.PhysicalTop);
	}

	return InPhysical;
}

float SSequencerTreeView::VirtualToPhysical(float InVirtual) const
{
	auto* Found = Utils::BinarySearch(PhysicalNodes, [&](const FCachedGeometry& In){

		if (InVirtual < In.Node->GetVirtualTop())
		{
			return Utils::ESearchState::Before;
		}
		else if (InVirtual > In.Node->GetVirtualBottom())
		{
			return Utils::ESearchState::After;
		}

		return Utils::ESearchState::Found;

	});

	if (Found)
	{
		const float FractionalHeight = (InVirtual - Found->Node->GetVirtualTop()) / (Found->Node->GetVirtualBottom() - Found->Node->GetVirtualTop());
		return Found->PhysicalTop + Found->PhysicalHeight * FractionalHeight;
	}
	
	if (PhysicalNodes.Num())
	{
		auto Last = PhysicalNodes.Last();
		return Last.PhysicalTop + (InVirtual - Last.Node->GetVirtualTop());
	}

	return InVirtual;
}

void SSequencerTreeView::SetupColumns(const FArguments& InArgs)
{
	FSequencer& Sequencer = SequencerNodeTree->GetSequencer();

	// Define a column for the Outliner
	auto GenerateOutliner = [=](const FDisplayNodeRef& InNode, const TSharedRef<SSequencerTreeViewRow>& InRow)
	{
		return InNode->GenerateContainerWidgetForOutliner(InRow);
	};

	Columns.Add("Outliner", FSequencerTreeViewColumn(GenerateOutliner, 1.f));

	// Now populate the header row with the columns
	for (auto& Pair : Columns)
	{
		if (Pair.Key != TrackAreaName || !Sequencer.GetShowCurveEditor())
		{
			HeaderRow->AddColumn(
				SHeaderRow::Column(Pair.Key)
				.FillWidth(Pair.Value.Width)
			);
		}
	}
}

void SSequencerTreeView::UpdateTrackArea()
{
	FSequencer& Sequencer = SequencerNodeTree->GetSequencer();

	// Add or remove the column
	if (Sequencer.GetShowCurveEditor())
	{
		HeaderRow->RemoveColumn(TrackAreaName);
	}
	else if (const auto* Column = Columns.Find(TrackAreaName))
	{
		HeaderRow->AddColumn(
			SHeaderRow::Column(TrackAreaName)
			.FillWidth(Column->Width)
		);
	}
}

void SSequencerTreeView::SynchronizeTreeSelectionWithSequencerSelection()
{
	if ( bUpdatingSequencerSelection == false )
	{
		bUpdatingTreeSelection = true;
		{
			Private_ClearSelection();

			FSequencer& Sequencer = SequencerNodeTree->GetSequencer();
			for ( auto& Node : Sequencer.GetSelection().GetSelectedOutlinerNodes() )
			{
				Private_SetItemSelection( Node, true, false );
			}

			Private_SignalSelectionChanged( ESelectInfo::Direct );
		}
		bUpdatingTreeSelection = false;
	}
}

void SSequencerTreeView::Private_SetItemSelection( FDisplayNodeRef TheItem, bool bShouldBeSelected, bool bWasUserDirected )
{
	STreeView::Private_SetItemSelection( TheItem, bShouldBeSelected, bWasUserDirected );
	if ( bUpdatingTreeSelection == false )
	{
		// Don't broadcast the sequencer selection change on individual tree changes.  Wait for signal selection changed.
		FSequencerSelection& SequencerSelection = SequencerNodeTree->GetSequencer().GetSelection();
		SequencerSelection.SuspendBroadcast();
		bSequencerSelectionChangeBroadcastWasSupressed = true;
		if ( bShouldBeSelected )
		{
			SequencerSelection.AddToSelection( TheItem );
		}
		else
		{
			SequencerSelection.RemoveFromSelection( TheItem );
		}
		SequencerSelection.ResumeBroadcast();
	}
}


void SSequencerTreeView::Private_ClearSelection()
{
	STreeView::Private_ClearSelection();
	if ( bUpdatingTreeSelection == false )
	{
		// Don't broadcast the sequencer selection change on individual tree changes.  Wait for signal selection changed.
		FSequencerSelection& SequencerSelection = SequencerNodeTree->GetSequencer().GetSelection();
		SequencerSelection.SuspendBroadcast();
		bSequencerSelectionChangeBroadcastWasSupressed = true;
		SequencerSelection.EmptySelectedOutlinerNodes();
		SequencerSelection.ResumeBroadcast();
	}
}

void SSequencerTreeView::Private_SelectRangeFromCurrentTo( FDisplayNodeRef InRangeSelectionEnd )
{
	STreeView::Private_SelectRangeFromCurrentTo( InRangeSelectionEnd );
	if ( bUpdatingTreeSelection == false )
	{
		// Don't broadcast the sequencer selection change on individual tree changes.  Wait for signal selection changed.
		FSequencerSelection& SequencerSelection = SequencerNodeTree->GetSequencer().GetSelection();
		SequencerSelection.SuspendBroadcast();
		bSequencerSelectionChangeBroadcastWasSupressed = true;
		SynchronizeSequencerSelectionWithTreeSelection();
		SequencerSelection.ResumeBroadcast();
	}
}

void SSequencerTreeView::Private_SignalSelectionChanged(ESelectInfo::Type SelectInfo)
{
	if ( bUpdatingTreeSelection == false )
	{
		bUpdatingSequencerSelection = true;
		{
			FSequencerSelection& SequencerSelection = SequencerNodeTree->GetSequencer().GetSelection();
			SequencerSelection.SuspendBroadcast();
			bool bSelectionChanged = SynchronizeSequencerSelectionWithTreeSelection();
			SequencerSelection.ResumeBroadcast();
			if ( bSequencerSelectionChangeBroadcastWasSupressed || bSelectionChanged )
			{
				SequencerSelection.RequestOutlinerNodeSelectionChangedBroadcast();
				bSequencerSelectionChangeBroadcastWasSupressed = false;
			}
		}
		bUpdatingSequencerSelection = false;
	}

	STreeView::Private_SignalSelectionChanged(SelectInfo);
}

bool SSequencerTreeView::SynchronizeSequencerSelectionWithTreeSelection()
{
	bool bSelectionChanged = false;
	const TSet<TSharedRef<FSequencerDisplayNode>>& SequencerSelection = SequencerNodeTree->GetSequencer().GetSelection().GetSelectedOutlinerNodes();
	if ( SelectedItems.Num() != SequencerSelection.Num() || SelectedItems.Difference( SequencerSelection ).Num() != 0 )
	{
		FSequencer& Sequencer = SequencerNodeTree->GetSequencer();
		FSequencerSelection& Selection = Sequencer.GetSelection();
		Selection.EmptySelectedOutlinerNodes();
		for ( auto& Item : GetSelectedItems() )
		{
			Selection.AddToSelection( Item );
		}
		bSelectionChanged = true;
	}
	return bSelectionChanged;
}

TSharedPtr<SWidget> SSequencerTreeView::OnContextMenuOpening()
{
	const TSet<TSharedRef<FSequencerDisplayNode>> SelectedNodes = SequencerNodeTree->GetSequencer().GetSelection().GetSelectedOutlinerNodes();
	if ( SelectedNodes.Num() > 0 )
	{
		return SelectedNodes.Array()[0]->OnSummonContextMenu();
	}

	// Otherwise, add a general menu for options
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, SequencerNodeTree->GetSequencer().GetCommandBindings());

	OnGetContextMenuContent.ExecuteIfBound(MenuBuilder);
	
	MenuBuilder.BeginSection("Edit");
	MenuBuilder.AddMenuEntry(FGenericCommands::Get().Paste);
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SSequencerTreeView::Refresh()
{
	RootNodes.Reset(SequencerNodeTree->GetRootNodes().Num());

	for (const auto& RootNode : SequencerNodeTree->GetRootNodes())
	{
		if (RootNode->IsExpanded())
		{
			SetItemExpansion(RootNode, true);
		}

		if (!RootNode->IsHidden())
		{
			RootNodes.Add(RootNode);
		}
	}

	// Force synchronization of selected tree view items here since the tree nodes may have been rebuilt
	// and the treeview's selection will now be invalid.
	bUpdatingTreeSelection = true;
	SynchronizeTreeSelectionWithSequencerSelection();
	bUpdatingTreeSelection = false;

	RequestTreeRefresh();
}

void SSequencerTreeView::ScrollByDelta(float DeltaInSlateUnits)
{
	ScrollBy( GetCachedGeometry(), DeltaInSlateUnits, EAllowOverscroll::No );
}

template<typename T>
bool ShouldExpand(const T& InContainer, ETreeRecursion Recursion)
{
	bool bAllExpanded = true;
	for (auto& Item : InContainer)
	{
		bAllExpanded &= Item->IsExpanded();
		if (Recursion == ETreeRecursion::Recursive)
		{
			Item->TraverseVisible_ParentFirst([&](FSequencerDisplayNode& InNode){
				bAllExpanded &= InNode.IsExpanded();
				return true;
			});
		}
	}
	return !bAllExpanded;
}

void SSequencerTreeView::ToggleExpandCollapseNodes(ETreeRecursion Recursion, bool bExpandAll)
{
	bool bExpand = false;
	if (bExpandAll)
	{
		bExpand = ShouldExpand(SequencerNodeTree->GetRootNodes(), Recursion);
	}
	else
	{
		FSequencer& Sequencer = SequencerNodeTree->GetSequencer();

		const TSet< FDisplayNodeRef >& SelectedNodes = Sequencer.GetSelection().GetSelectedOutlinerNodes();

		bExpand = ShouldExpand(SelectedNodes, Recursion);
	}

	ExpandOrCollapseNodes(Recursion, bExpandAll, bExpand);
}

void SSequencerTreeView::ExpandNodes(ETreeRecursion Recursion, bool bExpandAll)
{
	const bool bExpand = true;
	ExpandOrCollapseNodes(Recursion, bExpandAll, bExpand);
}

void SSequencerTreeView::CollapseNodes(ETreeRecursion Recursion, bool bExpandAll)
{
	const bool bExpand = false;
	ExpandOrCollapseNodes(Recursion, bExpandAll, bExpand);
}

void SSequencerTreeView::ExpandOrCollapseNodes(ETreeRecursion Recursion, bool bExpandAll, bool bExpand)
{
	FSequencer& Sequencer = SequencerNodeTree->GetSequencer();

	if (bExpandAll)
	{
		for (auto& Item : SequencerNodeTree->GetRootNodes())
		{
			ExpandCollapseNode(Item, bExpand, Recursion);
		}	
	}
	else
	{
		const TSet< FDisplayNodeRef >& SelectedNodes = Sequencer.GetSelection().GetSelectedOutlinerNodes();

		for (auto& Item : SelectedNodes)
		{
			ExpandCollapseNode(Item, bExpand, Recursion);
		}
	}
}

void SSequencerTreeView::ExpandCollapseNode(const FDisplayNodeRef& InNode, bool bExpansionState, ETreeRecursion Recursion)
{
	SetItemExpansion(InNode, bExpansionState);

	if (Recursion == ETreeRecursion::Recursive)
	{
		for (auto& Node : InNode->GetChildNodes())
		{
			ExpandCollapseNode(Node, bExpansionState, ETreeRecursion::Recursive);
		}
	}
}

void SSequencerTreeView::OnExpansionChanged(FDisplayNodeRef InItem, bool bIsExpanded)
{
	InItem->SetExpansionState(bIsExpanded);
	
	// Expand any children that are also expanded
	for (auto& Child : InItem->GetChildNodes())
	{
		if (Child->IsExpanded())
		{
			SetItemExpansion(Child, true);
		}
	}
}

void SSequencerTreeView::OnGetChildren(FDisplayNodeRef InParent, TArray<FDisplayNodeRef>& OutChildren) const
{
	for (const auto& Node : InParent->GetChildNodes())
	{
		if (!Node->IsHidden())
		{
			OutChildren.Add(Node);
		}
	}
}

TSharedRef<ITableRow> SSequencerTreeView::OnGenerateRow(FDisplayNodeRef InDisplayNode, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SSequencerTreeViewRow> Row =
		SNew(SSequencerTreeViewRow, OwnerTable, InDisplayNode)
		.OnGenerateWidgetForColumn(this, &SSequencerTreeView::GenerateWidgetForColumn);

	// Ensure the track area is kept up to date with the virtualized scroll of the tree view
	TSharedPtr<FSequencerDisplayNode> SectionAuthority = InDisplayNode->GetSectionAreaAuthority();
	if (SectionAuthority.IsValid())
	{
		TSharedPtr<SSequencerTrackLane> TrackLane = TrackArea->FindTrackSlot(SectionAuthority.ToSharedRef());

		if (!TrackLane.IsValid())
		{
			// Add a track slot for the row
			TAttribute<TRange<float>> ViewRange = FAnimatedRange::WrapAttribute( TAttribute<FAnimatedRange>::Create(TAttribute<FAnimatedRange>::FGetter::CreateSP(&SequencerNodeTree->GetSequencer(), &FSequencer::GetViewRange)) );

			TrackLane = SNew(SSequencerTrackLane, SectionAuthority.ToSharedRef(), SharedThis(this))
			.IsEnabled(!InDisplayNode->GetSequencer().IsReadOnly())
			[
				SectionAuthority->GenerateWidgetForSectionArea(ViewRange)
			];

			TrackArea->AddTrackSlot(SectionAuthority.ToSharedRef(), TrackLane);
		}

		if (ensure(TrackLane.IsValid()))
		{
			Row->AddTrackAreaReference(TrackLane);
		}
	}

	return Row;
}

TSharedRef<SWidget> SSequencerTreeView::GenerateWidgetForColumn(const FDisplayNodeRef& InNode, const FName& ColumnId, const TSharedRef<SSequencerTreeViewRow>& Row) const
{
	const auto* Definition = Columns.Find(ColumnId);

	if (ensureMsgf(Definition, TEXT("Invalid column name specified")))
	{
		return Definition->Generator(InNode, Row);
	}

	return SNullWidget::NullWidget;
}
