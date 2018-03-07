// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSequencerTrackArea.h"
#include "Types/PaintArgs.h"
#include "Layout/ArrangedChildren.h"
#include "Rendering/DrawElements.h"
#include "Layout/LayoutUtils.h"
#include "Widgets/SWeakWidget.h"
#include "EditorStyleSet.h"
#include "SSequencerTrackLane.h"
#include "SSequencerTreeView.h"
#include "ISequencerHotspot.h"
#include "SequencerHotspots.h"
#include "Tools/SequencerEditTool_Movement.h"
#include "Tools/SequencerEditTool_Selection.h"
#include "ISequencerTrackEditor.h"
#include "DisplayNodes/SequencerTrackNode.h"

FTrackAreaSlot::FTrackAreaSlot(const TSharedPtr<SSequencerTrackLane>& InSlotContent)
{
	TrackLane = InSlotContent;
	
	HAlignment = HAlign_Fill;
	VAlignment = VAlign_Top;

	this->AttachWidget(
		SNew(SWeakWidget)
			.PossiblyNullContent(InSlotContent)
	);
}


float FTrackAreaSlot::GetVerticalOffset() const
{
	auto PinnedTrackLane = TrackLane.Pin();
	return PinnedTrackLane.IsValid() ? PinnedTrackLane->GetPhysicalPosition() : 0.f;
}


void SSequencerTrackArea::Construct(const FArguments& InArgs, TSharedRef<FSequencerTimeSliderController> InTimeSliderController, TSharedRef<FSequencer> InSequencer)
{
	Sequencer = InSequencer;
	TimeSliderController = InTimeSliderController;

	// Input stack in order or priority

	// Space for the edit tool
	InputStack.AddHandler(nullptr);
	// The time slider controller
	InputStack.AddHandler(TimeSliderController.Get());

	EditTools.Add(MakeShared<FSequencerEditTool_Selection>(*InSequencer));
	EditTools.Add(MakeShared<FSequencerEditTool_Movement>(*InSequencer));
}


void SSequencerTrackArea::SetTreeView(const TSharedPtr<SSequencerTreeView>& InTreeView)
{
	TreeView = InTreeView;
}

void SSequencerTrackArea::Empty()
{
	TrackSlots.Empty();
	Children.Empty();
}

void SSequencerTrackArea::AddTrackSlot(const TSharedRef<FSequencerDisplayNode>& InNode, const TSharedPtr<SSequencerTrackLane>& InSlot)
{
	TrackSlots.Add(InNode, InSlot);
	Children.Add(new FTrackAreaSlot(InSlot));
}


TSharedPtr<SSequencerTrackLane> SSequencerTrackArea::FindTrackSlot(const TSharedRef<FSequencerDisplayNode>& InNode)
{
	return TrackSlots.FindRef(InNode).Pin();
}


void SSequencerTrackArea::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
	{
		const FTrackAreaSlot& CurChild = Children[ChildIndex];

		const EVisibility ChildVisibility = CurChild.GetWidget()->GetVisibility();
		if (!ArrangedChildren.Accepts(ChildVisibility))
		{
			continue;
		}

		const FMargin Padding(0, CurChild.GetVerticalOffset(), 0, 0);

		AlignmentArrangeResult XResult = AlignChild<Orient_Horizontal>(AllottedGeometry.GetLocalSize().X, CurChild, Padding, 1.0f, false);
		AlignmentArrangeResult YResult = AlignChild<Orient_Vertical>(AllottedGeometry.GetLocalSize().Y, CurChild, Padding, 1.0f, false);

		ArrangedChildren.AddWidget(ChildVisibility,
			AllottedGeometry.MakeChild(
				CurChild.GetWidget(),
				FVector2D(XResult.Offset,YResult.Offset),
				FVector2D(XResult.Size, YResult.Size)
			)
		);
	}
}


FVector2D SSequencerTrackArea::ComputeDesiredSize( float ) const
{
	FVector2D MaxSize(0,0);
	for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
	{
		const FTrackAreaSlot& CurChild = Children[ChildIndex];

		const EVisibility ChildVisibilty = CurChild.GetWidget()->GetVisibility();
		if (ChildVisibilty != EVisibility::Collapsed)
		{
			FVector2D ChildDesiredSize = CurChild.GetWidget()->GetDesiredSize();
			MaxSize.X = FMath::Max(MaxSize.X, ChildDesiredSize.X);
			MaxSize.Y = FMath::Max(MaxSize.Y, ChildDesiredSize.Y);
		}
	}

	return MaxSize;
}


FChildren* SSequencerTrackArea::GetChildren()
{
	return &Children;
}


int32 SSequencerTrackArea::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	if ( Sequencer.IsValid() )
	{
		// give track editors a chance to paint
		auto TrackEditors = Sequencer.Pin()->GetTrackEditors();

		for (const auto& TrackEditor : TrackEditors)
		{
			LayerId = TrackEditor->PaintTrackArea(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 1, InWidgetStyle);
		}

		// paint the child widgets
		FArrangedChildren ArrangedChildren(EVisibility::Visible);
		ArrangeChildren(AllottedGeometry, ArrangedChildren);

		const FPaintArgs NewArgs = Args.WithNewParent(this);

		for (int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ++ChildIndex)
		{
			FArrangedWidget& CurWidget = ArrangedChildren[ChildIndex];
			FSlateRect ChildClipRect = MyCullingRect.IntersectionWith( CurWidget.Geometry.GetLayoutBoundingRect() );
			const int32 ThisWidgetLayerId = CurWidget.Widget->Paint( NewArgs, CurWidget.Geometry, ChildClipRect, OutDrawElements, LayerId + 2, InWidgetStyle, ShouldBeEnabled( bParentEnabled ) );

			LayerId = FMath::Max(LayerId, ThisWidgetLayerId);
		}

		if (EditTool.IsValid())
		{
			LayerId = EditTool->OnPaint(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 1);
		}

		TOptional<FHighlightRegion> HighlightRegion = TreeView.Pin()->GetHighlightRegion();
		if (HighlightRegion.IsSet())
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId+1,
				AllottedGeometry.ToPaintGeometry(FVector2D(0.f, HighlightRegion->Top - 4.f), FVector2D(AllottedGeometry.GetLocalSize().X, 4.f)),
				FEditorStyle::GetBrush("Sequencer.TrackHoverHighlight_Top"),
				ESlateDrawEffect::None,
				FLinearColor::Black
			);
		
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId+1,
				AllottedGeometry.ToPaintGeometry(FVector2D(0.f, HighlightRegion->Bottom), FVector2D(AllottedGeometry.GetLocalSize().X, 4.f)),
				FEditorStyle::GetBrush("Sequencer.TrackHoverHighlight_Bottom"),
				ESlateDrawEffect::None,
				FLinearColor::Black
			);
		}


		// Draw drop target
		if (DroppedNode.IsValid() && TrackSlots.Contains(DroppedNode.Pin()))
		{
			TSharedPtr<SSequencerTrackLane> TrackLane = TrackSlots.FindRef(DroppedNode.Pin()).Pin();
			
			FGeometry NodeGeometry = TrackLane.Get()->GetCachedGeometry();

			FLinearColor DashColor = bAllowDrop ? FLinearColor::Green: FLinearColor::Red;

			const FSlateBrush* HorizontalBrush = FEditorStyle::GetBrush("WideDash.Horizontal");
			const FSlateBrush* VerticalBrush = FEditorStyle::GetBrush("WideDash.Vertical");

			int32 DashLayer = LayerId + 1;

			// Top
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				DashLayer,
				AllottedGeometry.ToPaintGeometry(FVector2D(0, TrackLane.Get()->GetPhysicalPosition()), FVector2D(NodeGeometry.GetLocalSize().X, HorizontalBrush->ImageSize.Y)),
				HorizontalBrush,
				ESlateDrawEffect::None,
				DashColor);

			// Bottom
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				DashLayer,
				AllottedGeometry.ToPaintGeometry(FVector2D(0, TrackLane.Get()->GetPhysicalPosition() + (TrackLane.Get()->GetCachedGeometry().GetLocalSize().Y - HorizontalBrush->ImageSize.Y)), FVector2D(AllottedGeometry.Size.X, HorizontalBrush->ImageSize.Y)),
				HorizontalBrush,
				ESlateDrawEffect::None,
				DashColor);

			// Left
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				DashLayer,
				AllottedGeometry.ToPaintGeometry(FVector2D(0, TrackLane.Get()->GetPhysicalPosition()), FVector2D(VerticalBrush->ImageSize.X, TrackLane.Get()->GetCachedGeometry().GetLocalSize().Y)),
				VerticalBrush,
				ESlateDrawEffect::None,
				DashColor);

			// Right
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				DashLayer,
				AllottedGeometry.ToPaintGeometry(FVector2D(AllottedGeometry.GetLocalSize().X - VerticalBrush->ImageSize.X, TrackLane.Get()->GetPhysicalPosition()), FVector2D(VerticalBrush->ImageSize.X, TrackLane.Get()->GetCachedGeometry().GetLocalSize().Y)),
				VerticalBrush,
				ESlateDrawEffect::None,
				DashColor);
		}
	}

	return LayerId;
}


FReply SSequencerTrackArea::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( Sequencer.IsValid() )
	{
		// Always ensure the edit tool is set up
		InputStack.SetHandlerAt(0, EditTool.Get());
		return InputStack.HandleMouseButtonDown(*this, MyGeometry, MouseEvent);
	}
	return FReply::Unhandled();
}


FReply SSequencerTrackArea::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( Sequencer.IsValid() )
	{
		FContextMenuSuppressor SuppressContextMenus(TimeSliderController.ToSharedRef());

		// Always ensure the edit tool is set up
		InputStack.SetHandlerAt(0, EditTool.Get());
		return InputStack.HandleMouseButtonUp(*this, MyGeometry, MouseEvent);
	}
	return FReply::Unhandled();
}

bool SSequencerTrackArea::CanActivateEditTool(FName Identifier) const
{
	auto IdentifierIsFound = [=](TSharedPtr<ISequencerEditTool> InEditTool){ return InEditTool->GetIdentifier() == Identifier; };
	if (InputStack.GetCapturedIndex() != INDEX_NONE)
	{
		return false;
	}
	else if (!EditTool.IsValid())
	{
		return EditTools.ContainsByPredicate(IdentifierIsFound);
	}
	// Can't activate a tool that's already active
	else if (EditTool->GetIdentifier() == Identifier)
	{
		return false;
	}
	// Can only activate a new tool if the current one will let us
	else
	{
		return EditTool->CanDeactivate() && EditTools.ContainsByPredicate(IdentifierIsFound);
	}
}

bool SSequencerTrackArea::AttemptToActivateTool(FName Identifier)
{
	if ( Sequencer.IsValid() && CanActivateEditTool(Identifier) )
	{
		EditTool = *EditTools.FindByPredicate([=](TSharedPtr<ISequencerEditTool> InEditTool){ return InEditTool->GetIdentifier() == Identifier; });
		return true;
	}

	return false;
}

void SSequencerTrackArea::UpdateHoverStates( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	TSharedPtr<SSequencerTreeView> PinnedTreeView = TreeView.Pin();

	// Set the node that we are hovering
	TSharedPtr<FSequencerDisplayNode> NewHoveredNode = PinnedTreeView->HitTestNode(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).Y);
	PinnedTreeView->GetNodeTree()->SetHoveredNode(NewHoveredNode);

	if ( Sequencer.IsValid() )
	{
		TSharedPtr<ISequencerHotspot> Hotspot = Sequencer.Pin()->GetHotspot();
		if (Hotspot.IsValid())
		{
			Hotspot->UpdateOnHover(*this, *Sequencer.Pin());
			return;
		}
	}

	// Any other region implies selection mode
	AttemptToActivateTool(FSequencerEditTool_Selection::Identifier);
}

FReply SSequencerTrackArea::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( Sequencer.IsValid() )
	{
		UpdateHoverStates(MyGeometry, MouseEvent);

		// Always ensure the edit tool is set up
		InputStack.SetHandlerAt(0, EditTool.Get());

		FReply Reply = InputStack.HandleMouseMove(*this, MyGeometry, MouseEvent);

		// Handle right click scrolling on the track area, if the captured index is that of the time slider
		if (Reply.IsEventHandled() && InputStack.GetCapturedIndex() == 1)
		{
			if (MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton) && HasMouseCapture())
			{
				TreeView.Pin()->ScrollByDelta(-MouseEvent.GetCursorDelta().Y);
			}
		}

		return Reply;
	}
	return FReply::Unhandled();
}


FReply SSequencerTrackArea::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( Sequencer.IsValid() )
	{
		// Always ensure the edit tool is set up
		InputStack.SetHandlerAt(0, EditTool.Get());
		return InputStack.HandleMouseWheel(*this, MyGeometry, MouseEvent);
	}
	return FReply::Unhandled();
}


void SSequencerTrackArea::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	DroppedNode.Reset();
	bAllowDrop = false;

	if ( Sequencer.IsValid() )
	{
		if (EditTool.IsValid())
		{
			EditTool->OnMouseEnter(*this, MyGeometry, MouseEvent);
		}
	}
}


void SSequencerTrackArea::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	if ( Sequencer.IsValid() )
	{
		if (EditTool.IsValid())
		{
			EditTool->OnMouseLeave(*this, MouseEvent);
		}

		TreeView.Pin()->GetNodeTree()->SetHoveredNode(nullptr);
	}
}


void SSequencerTrackArea::OnMouseCaptureLost()
{
	if ( Sequencer.IsValid() )
	{
		if (EditTool.IsValid())
		{
			EditTool->OnMouseCaptureLost();
		}
	}
}


FCursorReply SSequencerTrackArea::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	if ( Sequencer.IsValid() )
	{
		if (CursorEvent.IsMouseButtonDown(EKeys::RightMouseButton) && HasMouseCapture())
		{
			return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
		}

		if (EditTool.IsValid())
		{
			return EditTool->OnCursorQuery(MyGeometry, CursorEvent);
		}
	}

	return FCursorReply::Unhandled();
}


void SSequencerTrackArea::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	CachedGeometry = AllottedGeometry;

	FVector2D Size = AllottedGeometry.GetLocalSize();

	if (SizeLastFrame.IsSet() && Size.X != SizeLastFrame->X)
	{
		// Zoom by the difference in horizontal size
		const float Difference = Size.X - SizeLastFrame->X;
		TRange<float> OldRange = TimeSliderController->GetViewRange().GetAnimationTarget();

		TimeSliderController->SetViewRange(
			OldRange.GetLowerBoundValue(),
			OldRange.GetUpperBoundValue() + (Difference * OldRange.Size<float>() / SizeLastFrame->X),
			EViewRangeInterpolation::Immediate
		);
	}

	SizeLastFrame = Size;

	for (int32 Index = 0; Index < Children.Num();)
	{
		if (!StaticCastSharedRef<SWeakWidget>(Children[Index].GetWidget())->ChildWidgetIsValid())
		{
			Children.RemoveAt(Index);
		}
		else
		{
			++Index;
		}
	}
}

		
void SSequencerTrackArea::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	SPanel::OnDragEnter(MyGeometry, DragDropEvent);
}
	
void SSequencerTrackArea::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	SPanel::OnDragLeave(DragDropEvent);
}
	 
FReply SSequencerTrackArea::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{	
	TSharedPtr<SSequencerTreeView> PinnedTreeView = TreeView.Pin();

	DroppedNode = PinnedTreeView->HitTestNode(MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition()).Y);
	bAllowDrop = false;

	if (DroppedNode.IsValid() && DroppedNode.Pin()->GetType() == ESequencerNode::Track && Sequencer.IsValid())
	{
		TSharedPtr<FSequencerDisplayNode> DisplayNode = DroppedNode.Pin();

		UMovieSceneTrack* Track = (StaticCastSharedPtr<FSequencerTrackNode>(DroppedNode.Pin()))->GetTrack();

		// give track editors a chance to accept the drag event
		auto TrackEditors = Sequencer.Pin()->GetTrackEditors();

		for (const auto& TrackEditor : TrackEditors)
		{
			if (TrackEditor->OnAllowDrop(DragDropEvent, Track))
			{
				bAllowDrop = true;
				return FReply::Handled();
			}
		}
	}

	return SPanel::OnDragOver(MyGeometry, DragDropEvent);
}

	 
FReply SSequencerTrackArea::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<SSequencerTreeView> PinnedTreeView = TreeView.Pin();

	DroppedNode = PinnedTreeView->HitTestNode(MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition()).Y);

	if (DroppedNode.IsValid() && DroppedNode.Pin()->GetType() == ESequencerNode::Track && Sequencer.IsValid())
	{
		UMovieSceneTrack* Track = (StaticCastSharedPtr<FSequencerTrackNode>(DroppedNode.Pin()))->GetTrack();

		// give track editors a chance to process the drag event
		auto TrackEditors = Sequencer.Pin()->GetTrackEditors();

		for (const auto& TrackEditor : TrackEditors)
		{
			if (TrackEditor->OnAllowDrop(DragDropEvent, Track))
			{
				DroppedNode.Reset();

				return TrackEditor->OnDrop(DragDropEvent, Track);
			}
		}
	}

	DroppedNode.Reset();

	return SPanel::OnDrop(MyGeometry, DragDropEvent);
}
