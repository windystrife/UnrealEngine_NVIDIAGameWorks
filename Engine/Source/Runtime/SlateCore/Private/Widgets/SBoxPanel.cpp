// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SBoxPanel.h"
#include "Layout/LayoutUtils.h"


/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SHorizontalBox::Construct( const SHorizontalBox::FArguments& InArgs )
{
	const int32 NumSlots = InArgs.Slots.Num();
	for ( int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex )
	{
		Children.Add( InArgs.Slots[SlotIndex] );
	}
}


/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SVerticalBox::Construct( const SVerticalBox::FArguments& InArgs )
{
	const int32 NumSlots = InArgs.Slots.Num();
	for ( int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex )
	{
		Children.Add( InArgs.Slots[SlotIndex] );
	}
}

template<EOrientation Orientation>
static void ArrangeChildrenAlong( const TPanelChildren<SBoxPanel::FSlot>& Children, const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren )
{
	// Allotted space will be given to fixed-size children first.
	// Remaining space will be proportionately divided between stretch children (SizeRule_Stretch)
	// based on their stretch coefficient

	if (Children.Num() > 0)
	{
		float StretchCoefficientTotal = 0;
		float FixedTotal = 0;

		// Compute the sum of stretch coefficients (SizeRule_Stretch) and space required by fixed-size widgets
		// (SizeRule_Auto).
		for( int32 ChildIndex=0; ChildIndex < Children.Num(); ++ChildIndex )
		{
			const SBoxPanel::FSlot& CurChild = Children[ChildIndex];

			if ( CurChild.GetWidget()->GetVisibility() != EVisibility::Collapsed )
			{
				// All widgets contribute their margin to the fixed space requirement
				FixedTotal += CurChild.SlotPadding.Get().GetTotalSpaceAlong<Orientation>();

				if ( CurChild.SizeParam.SizeRule == FSizeParam::SizeRule_Stretch )
				{
					// for stretch children we save sum up the stretch coefficients
					StretchCoefficientTotal += CurChild.SizeParam.Value.Get();
				}
				else
				{
					FVector2D ChildDesiredSize = CurChild.GetWidget()->GetDesiredSize();

					// Auto-sized children contribute their desired size to the fixed space requirement
					const float ChildSize = (Orientation == Orient_Vertical)
						? ChildDesiredSize.Y
						: ChildDesiredSize.X;

					// Clamp to the max size if it was specified
					float MaxSize = CurChild.MaxSize.Get();
					FixedTotal += MaxSize > 0 ? FMath::Min( MaxSize, ChildSize ) : ChildSize ;
				}
			}
		}

		// The space available for SizeRule_Stretch widgets is any space that wasn't taken up by fixed-sized widgets.
		const float NonFixedSpace = FMath::Max( 0.0f, (Orientation == Orient_Vertical)
			? AllottedGeometry.GetLocalSize().Y - FixedTotal
			: AllottedGeometry.GetLocalSize().X - FixedTotal );

		float PositionSoFar = 0;

		// Now that we have the total fixed-space requirement and the total stretch coefficients we can
		// arrange widgets top-to-bottom or left-to-right (depending on the orientation).
		for( int32 ChildIndex=0; ChildIndex < Children.Num(); ++ChildIndex )
		{
			const SBoxPanel::FSlot& CurChild = Children[ChildIndex];
			const EVisibility ChildVisibility = CurChild.GetWidget()->GetVisibility();
	
			// Figure out the area allocated to the child in the direction of BoxPanel
			// The area allocated to the slot is ChildSize + the associated margin.
			float ChildSize = 0;
			if ( ChildVisibility != EVisibility::Collapsed )
			{
				// The size of the widget depends on its size type
				if ( CurChild.SizeParam.SizeRule == FSizeParam::SizeRule_Stretch )
				{
					if (StretchCoefficientTotal > 0)
					{
						// Stretch widgets get a fraction of the space remaining after all the fixed-space requirements are met
						ChildSize = NonFixedSpace * CurChild.SizeParam.Value.Get() / StretchCoefficientTotal;
					}
				}
				else
				{
					const FVector2D ChildDesiredSize = CurChild.GetWidget()->GetDesiredSize();

					// Auto-sized widgets get their desired-size value
					ChildSize = (Orientation == Orient_Vertical)
						? ChildDesiredSize.Y
						: ChildDesiredSize.X;
				}

				// Clamp to the max size if it was specified
				float MaxSize = CurChild.MaxSize.Get();
				if( MaxSize > 0 )
				{
					ChildSize = FMath::Min( MaxSize, ChildSize );
				}
			}

			const FMargin SlotPadding(CurChild.SlotPadding.Get());

			FVector2D SlotSize = (Orientation == Orient_Vertical)
				? FVector2D( AllottedGeometry.GetLocalSize().X, ChildSize + SlotPadding.GetTotalSpaceAlong<Orient_Vertical>() )
				: FVector2D( ChildSize + SlotPadding.GetTotalSpaceAlong<Orient_Horizontal>(), AllottedGeometry.GetLocalSize().Y );

			// Figure out the size and local position of the child within the slot			
			AlignmentArrangeResult XAlignmentResult = AlignChild<Orient_Horizontal>( SlotSize.X, CurChild, SlotPadding );					
			AlignmentArrangeResult YAlignmentResult = AlignChild<Orient_Vertical>( SlotSize.Y, CurChild, SlotPadding );

			const FVector2D LocalPosition = (Orientation == Orient_Vertical)
				? FVector2D(XAlignmentResult.Offset, PositionSoFar + YAlignmentResult.Offset)
				: FVector2D(PositionSoFar + XAlignmentResult.Offset, YAlignmentResult.Offset);

			const FVector2D LocalSize = FVector2D(XAlignmentResult.Size, YAlignmentResult.Size);

			// Add the information about this child to the output list (ArrangedChildren)
			ArrangedChildren.AddWidget( ChildVisibility, AllottedGeometry.MakeChild(
				// The child widget being arranged
				CurChild.GetWidget(),
				// Child's local position (i.e. position within parent)
				LocalPosition,
				// Child's size
				LocalSize
				));

			if ( ChildVisibility != EVisibility::Collapsed )
			{
				// Offset the next child by the size of the current child and any post-child (bottom/right) margin
				PositionSoFar += (Orientation == Orient_Vertical) ? SlotSize.Y : SlotSize.X;
			}
		}
	}
}

/**
 * Panels arrange their children in a space described by the AllottedGeometry parameter. The results of the arrangement
 * should be returned by appending a FArrangedWidget pair for every child widget. See StackPanel for an example
 *
 * @param AllottedGeometry    The geometry allotted for this widget by its parent.
 * @param ArrangedChildren    The array to which to add the WidgetGeometries that represent the arranged children.
 */
void SBoxPanel::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	if ( this->Orientation == Orient_Horizontal )
	{
		ArrangeChildrenAlong<Orient_Horizontal>(this->Children, AllottedGeometry, ArrangedChildren );
	}
	else
	{
		ArrangeChildrenAlong<Orient_Vertical>(this->Children, AllottedGeometry, ArrangedChildren );
	}
}

/**
 * Helper to ComputeDesiredSize().
 *
 * @param Orientation   Template parameters that controls the orientation in which the children are layed out
 * @param Children      The children whose size we want to assess in a horizontal or vertical arrangement.
 *
 * @return The size desired by the children given an orientation.
 */
template<EOrientation Orientation>
static FVector2D ComputeDesiredSizeForBox( const TPanelChildren<SBoxPanel::FSlot>& Children ) 
{
	// The desired size of this panel is the total size desired by its children plus any margins specified in this panel.
	// The layout along the panel's axis is describe dy the SizeParam, while the perpendicular layout is described by the
	// alignment property.
	FVector2D MyDesiredSize(0,0);
	for( int32 ChildIndex=0; ChildIndex < Children.Num(); ++ChildIndex )
	{
		const SBoxPanel::FSlot& CurChild = Children[ChildIndex];
		const FVector2D& CurChildDesiredSize = CurChild.GetWidget()->GetDesiredSize();
		if ( CurChild.GetWidget()->GetVisibility() != EVisibility::Collapsed )
		{
			if (Orientation == Orient_Vertical)
			{
				// For a vertical panel, we want to find the maximum desired width (including margin).
				// That will be the desired width of the whole panel.
				MyDesiredSize.X = FMath::Max(MyDesiredSize.X, CurChildDesiredSize.X + CurChild.SlotPadding.Get().GetTotalSpaceAlong<Orient_Horizontal>());

				// Clamp to the max size if it was specified
				float FinalChildDesiredSize = CurChildDesiredSize.Y;
				float MaxSize = CurChild.MaxSize.Get();
				if( MaxSize > 0 )
				{
					FinalChildDesiredSize = FMath::Min( MaxSize, FinalChildDesiredSize );
				}

				MyDesiredSize.Y += FinalChildDesiredSize + CurChild.SlotPadding.Get().GetTotalSpaceAlong<Orient_Vertical>();				
			}
			else
			{
				// A horizontal panel is just a sideways vertical panel: the axes are swapped.

				MyDesiredSize.Y = FMath::Max(MyDesiredSize.Y, CurChildDesiredSize.Y + CurChild.SlotPadding.Get().GetTotalSpaceAlong<Orient_Vertical>());

				// Clamp to the max size if it was specified
				float FinalChildDesiredSize = CurChildDesiredSize.X;
				float MaxSize = CurChild.MaxSize.Get();
				if( MaxSize > 0 )
				{
					FinalChildDesiredSize = FMath::Min( MaxSize, FinalChildDesiredSize );
				}

				MyDesiredSize.X += FinalChildDesiredSize + CurChild.SlotPadding.Get().GetTotalSpaceAlong<Orient_Horizontal>();
			}
		}
	}

	return MyDesiredSize;
}

/**
 * A Panel's desired size in the space required to arrange of its children on the screen while respecting all of
 * the children's desired sizes and any layout-related options specified by the user. See StackPanel for an example.
 *
 * @return The desired size.
 */
FVector2D SBoxPanel::ComputeDesiredSize( float ) const
{
	return (Orientation == Orient_Horizontal)
		? ComputeDesiredSizeForBox<Orient_Horizontal>(this->Children)
		: ComputeDesiredSizeForBox<Orient_Vertical>(this->Children);
}

/** @return  The children of a panel in a slot-agnostic way. */
FChildren* SBoxPanel::GetChildren()
{
	return &Children;
}

int32 SBoxPanel::RemoveSlot( const TSharedRef<SWidget>& SlotWidget )
{
	for (int32 SlotIdx = 0; SlotIdx < Children.Num(); ++SlotIdx)
	{
		if ( SlotWidget == Children[SlotIdx].GetWidget() )
		{
			Children.RemoveAt(SlotIdx);
			return SlotIdx;
		}
	}

	return -1;
}

void SBoxPanel::ClearChildren()
{
	Children.Empty();
}

/**
 * A Box Panel's orientation cannot be changed once it is constructed..
 *
 * @param InOrientation   The orientation of the Box Panel
 */
SBoxPanel::SBoxPanel( EOrientation InOrientation )
: Children()
, Orientation(InOrientation)
{

}


void SDragAndDropVerticalBox::Construct(const FArguments& InArgs)
{
	SVerticalBox::Construct(SVerticalBox::FArguments());

	OnCanAcceptDrop = InArgs._OnCanAcceptDrop;
	OnAcceptDrop = InArgs._OnAcceptDrop;
	OnDragDetected_Handler = InArgs._OnDragDetected;
	OnDragEnter_Handler = InArgs._OnDragEnter;
	OnDragLeave_Handler = InArgs._OnDragLeave;
	OnDrop_Handler = InArgs._OnDrop;

	CurrentDragOperationScreenSpaceLocation = FVector2D::ZeroVector;
	CurrentDragOverSlotIndex = INDEX_NONE;
}

FReply SDragAndDropVerticalBox::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}

	return FReply::Unhandled();
}

FReply SDragAndDropVerticalBox::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TPanelChildren<FSlot>* PanelChildren = (TPanelChildren<FSlot>*)GetChildren();
	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildren(MyGeometry, ArrangedChildren);

	int32 NodeUnderPositionIndex = SWidget::FindChildUnderMouse(ArrangedChildren, MouseEvent);

	if (PanelChildren->IsValidIndex(NodeUnderPositionIndex))
	{
		SVerticalBox::FSlot* Slot = &(*PanelChildren)[NodeUnderPositionIndex];

		if (OnDragDetected_Handler.IsBound())
		{
			return OnDragDetected_Handler.Execute(MyGeometry, MouseEvent, NodeUnderPositionIndex, Slot);
		}
	}

	return FReply::Unhandled();
}

void SDragAndDropVerticalBox::OnDragEnter(FGeometry const& MyGeometry, FDragDropEvent const& DragDropEvent)
{
	if (OnDragEnter_Handler.IsBound())
	{
		OnDragEnter_Handler.Execute(DragDropEvent);
	}
}

void SDragAndDropVerticalBox::OnDragLeave(FDragDropEvent const& DragDropEvent)
{
	ItemDropZone = TOptional<EItemDropZone>();
	CurrentDragOperationScreenSpaceLocation = FVector2D::ZeroVector;
	CurrentDragOverSlotIndex = INDEX_NONE;

	if (OnDragLeave_Handler.IsBound())
	{
		OnDragLeave_Handler.Execute(DragDropEvent);
	}
}

SDragAndDropVerticalBox::EItemDropZone SDragAndDropVerticalBox::ZoneFromPointerPosition(FVector2D LocalPointerPos, const FGeometry& CurrentGeometry, const FGeometry& StartGeometry) const
{
	FSlateLayoutTransform StartGeometryLayoutTransform = StartGeometry.GetAccumulatedLayoutTransform();
	FSlateLayoutTransform CurrentGeometryLayoutTransform = CurrentGeometry.GetAccumulatedLayoutTransform();

	if (StartGeometryLayoutTransform.GetTranslation().Y > CurrentGeometryLayoutTransform.GetTranslation().Y) // going up
	{
		return EItemDropZone::AboveItem;
	}
	else if (StartGeometryLayoutTransform.GetTranslation().Y < CurrentGeometryLayoutTransform.GetTranslation().Y) // going down
	{
		return EItemDropZone::BelowItem;
	}
	else
	{
		if (LocalPointerPos.Y <= CurrentGeometry.GetLocalSize().Y / 2.0f)
		{
			return EItemDropZone::AboveItem;
		}
		else
		{
			return EItemDropZone::BelowItem;
		}
	}
}

FReply SDragAndDropVerticalBox::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (OnCanAcceptDrop.IsBound())
	{
		FArrangedChildren ArrangedChildren(EVisibility::Visible);
		ArrangeChildren(MyGeometry, ArrangedChildren);

		TSharedPtr<FDragAndDropVerticalBoxOp> DragOp = DragDropEvent.GetOperationAs<FDragAndDropVerticalBoxOp>();

		if (DragOp.IsValid())
		{
			int32 DragOverSlotIndex = SWidget::FindChildUnderPosition(ArrangedChildren, DragDropEvent.GetScreenSpacePosition());

			if (ArrangedChildren.IsValidIndex(DragOverSlotIndex))
			{
				FVector2D LocalPointerPos = ArrangedChildren[DragOverSlotIndex].Geometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
				EItemDropZone ItemHoverZone = ZoneFromPointerPosition(LocalPointerPos, ArrangedChildren[DragOverSlotIndex].Geometry, ArrangedChildren[DragOp->SlotIndexBeingDragged].Geometry);

				TPanelChildren<FSlot>* PanelChildren = (TPanelChildren<FSlot>*)GetChildren();

				if (PanelChildren->IsValidIndex(DragOverSlotIndex))
				{
					SVerticalBox::FSlot* Slot = &(*PanelChildren)[DragOverSlotIndex];

					ItemDropZone = OnCanAcceptDrop.Execute(DragDropEvent, ItemHoverZone, Slot);
					CurrentDragOperationScreenSpaceLocation = DragDropEvent.GetScreenSpacePosition();
					CurrentDragOverSlotIndex = DragOverSlotIndex;

					return FReply::Handled();
				}
			}
		}
	}

	return FReply::Unhandled();
}

FReply SDragAndDropVerticalBox::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	FReply DropReply = FReply::Unhandled();

	if (OnAcceptDrop.IsBound())
	{
		FArrangedChildren ArrangedChildren(EVisibility::Visible);
		ArrangeChildren(MyGeometry, ArrangedChildren);

		if (DragDropEvent.GetOperationAs<FDragAndDropVerticalBoxOp>().IsValid())
		{
			int32 NodeUnderPositionIndex = SWidget::FindChildUnderPosition(ArrangedChildren, DragDropEvent.GetScreenSpacePosition());
			TPanelChildren<FSlot>* PanelChildren = (TPanelChildren<FSlot>*)GetChildren();

			if (PanelChildren->IsValidIndex(NodeUnderPositionIndex))
			{
				SVerticalBox::FSlot* Slot = &(*PanelChildren)[NodeUnderPositionIndex];
				TOptional<EItemDropZone> ReportedZone = ItemDropZone;

				if (OnCanAcceptDrop.IsBound() && ItemDropZone.IsSet())
				{
					ReportedZone = OnCanAcceptDrop.Execute(DragDropEvent, ItemDropZone.GetValue(), Slot);
				}

				if (ReportedZone.IsSet())
				{
					DropReply = OnAcceptDrop.Execute(DragDropEvent, ReportedZone.GetValue(), NodeUnderPositionIndex, Slot);

					if (DropReply.IsEventHandled())
					{
						TSharedPtr<FDragAndDropVerticalBoxOp> DragOp = DragDropEvent.GetOperationAs<FDragAndDropVerticalBoxOp>();

						// Perform the slot changes
						PanelChildren->Move(DragOp->SlotIndexBeingDragged, NodeUnderPositionIndex);
					}
				}
			}

			ItemDropZone = TOptional<EItemDropZone>();
			CurrentDragOperationScreenSpaceLocation = FVector2D::ZeroVector;
			CurrentDragOverSlotIndex = INDEX_NONE;
		}		
	}

	return DropReply;
}

int32 SDragAndDropVerticalBox::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildren(AllottedGeometry, ArrangedChildren);

	LayerId = SPanel::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (ItemDropZone.IsSet())
	{
		// Draw feedback for user dropping an item above, below.
		const FSlateBrush* DropIndicatorBrush = nullptr;

		switch (ItemDropZone.GetValue())
		{
			default:
			case EItemDropZone::AboveItem: DropIndicatorBrush = &DropIndicator_Above; break;
			case EItemDropZone::BelowItem: DropIndicatorBrush = &DropIndicator_Below; break;
		};

		if (ArrangedChildren.IsValidIndex(CurrentDragOverSlotIndex))
		{
			const FArrangedWidget& CurWidget = ArrangedChildren[CurrentDragOverSlotIndex];

			FSlateDrawElement::MakeBox
			(
				OutDrawElements,
				LayerId++,
				CurWidget.Geometry.ToPaintGeometry(),
				DropIndicatorBrush,
				ESlateDrawEffect::None,
				DropIndicatorBrush->GetTint(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
			);
		}
	}

	return LayerId;
}