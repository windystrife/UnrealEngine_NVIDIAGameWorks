// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"

/////////////////////////////////////////////////////
// UCanvasPanelSlot

UCanvasPanelSlot::UCanvasPanelSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Slot(nullptr)
{
	LayoutData.Offsets = FMargin(0, 0, 100, 30);
	LayoutData.Anchors = FAnchors(0.0f, 0.0f);
	LayoutData.Alignment = FVector2D(0.0f, 0.0f);
	bAutoSize = false;
	ZOrder = 0;
}

void UCanvasPanelSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	Slot = nullptr;
}

void UCanvasPanelSlot::BuildSlot(TSharedRef<SConstraintCanvas> Canvas)
{
	Slot = &Canvas->AddSlot()
		[
			Content == nullptr ? SNullWidget::NullWidget : Content->TakeWidget()
		];

	SynchronizeProperties();
}

#if WITH_EDITOR

bool UCanvasPanelSlot::NudgeByDesigner(const FVector2D& NudgeDirection, const TOptional<int32>& GridSnapSize)
{
	const FVector2D OldPosition = GetPosition();
	FVector2D NewPosition = OldPosition + NudgeDirection;

	// Determine the new position aligned to the grid.
	if (GridSnapSize.IsSet())
	{
		if (NudgeDirection.X != 0)
		{
			NewPosition.X = ((int32)NewPosition.X) - (((int32)NewPosition.X) % GridSnapSize.GetValue());
		}
		if (NudgeDirection.Y != 0)
		{
			NewPosition.Y = ((int32)NewPosition.Y) - (((int32)NewPosition.Y) % GridSnapSize.GetValue());
		}
	}

	// Offset the size by the same amount moved if we're anchoring along that axis.
	const FVector2D OldSize = GetSize();
	FVector2D NewSize = OldSize;
	if (GetAnchors().IsStretchedHorizontal())
	{
		NewSize.X -= NewPosition.X - OldPosition.X;
	}
	if (GetAnchors().IsStretchedVertical())
	{
		NewSize.Y -= NewPosition.Y - OldPosition.Y;
	}

	// Return false and early out if there are no effective changes.
	if (OldPosition == NewPosition && OldSize == NewSize)
	{
		return false;
	}

	Modify();

	SetPosition(NewPosition);
	SetSize(NewSize);

	return true;
}

bool UCanvasPanelSlot::DragDropPreviewByDesigner(const FVector2D& LocalCursorPosition, const TOptional<int32>& XGridSnapSize, const TOptional<int32>& YGridSnapSize)
{
	// HACK UMG - This seems like a bad idea to call TakeWidget
	TSharedPtr<SWidget> SlateWidget = Content->TakeWidget();
	SlateWidget->SlatePrepass();
	const FVector2D& WidgetDesiredSize = SlateWidget->GetDesiredSize();

	static const FVector2D MinimumDefaultSize(100, 40);
	FVector2D LocalSize = FVector2D(FMath::Max(WidgetDesiredSize.X, MinimumDefaultSize.X), FMath::Max(WidgetDesiredSize.Y, MinimumDefaultSize.Y));

	FVector2D NewPosition = LocalCursorPosition;
	if (XGridSnapSize.IsSet())
	{
		NewPosition.X = ((int32)NewPosition.X) - (((int32)NewPosition.X) % XGridSnapSize.GetValue());
	}
	if (YGridSnapSize.IsSet())
	{
		NewPosition.Y = ((int32)NewPosition.Y) - (((int32)NewPosition.Y) % YGridSnapSize.GetValue());
	}

	// Return false and early out if there are no effective changes.
	if (GetSize() == LocalSize && GetPosition() == NewPosition)
	{
		return false;
	}

	SetPosition(NewPosition);
	SetSize(LocalSize);

	return true;
}

void UCanvasPanelSlot::SynchronizeFromTemplate(const UPanelSlot* const TemplateSlot)
{
	const ThisClass* const TemplateCanvasPanelSlot = CastChecked<ThisClass>(TemplateSlot);
	SetPosition(TemplateCanvasPanelSlot->GetPosition());
	SetSize(TemplateCanvasPanelSlot->GetSize());
}

#endif //WITH_EDITOR

void UCanvasPanelSlot::SetLayout(const FAnchorData& InLayoutData)
{
	LayoutData = InLayoutData;

	if ( Slot )
	{
		Slot->Offset(LayoutData.Offsets);
		Slot->Anchors(LayoutData.Anchors);
		Slot->Alignment(LayoutData.Alignment);
	}
}

FAnchorData UCanvasPanelSlot::GetLayout() const
{
	return LayoutData;
}

void UCanvasPanelSlot::SetPosition(FVector2D InPosition)
{
	LayoutData.Offsets.Left = InPosition.X;
	LayoutData.Offsets.Top = InPosition.Y;

	if ( Slot )
	{
		Slot->Offset(LayoutData.Offsets);
	}
}

FVector2D UCanvasPanelSlot::GetPosition() const
{
	if ( Slot )
	{
		FMargin Offsets = Slot->OffsetAttr.Get();
		return FVector2D(Offsets.Left, Offsets.Top);
	}

	return FVector2D(LayoutData.Offsets.Left, LayoutData.Offsets.Top);
}

void UCanvasPanelSlot::SetSize(FVector2D InSize)
{
	LayoutData.Offsets.Right = InSize.X;
	LayoutData.Offsets.Bottom = InSize.Y;

	if ( Slot )
	{
		Slot->Offset(LayoutData.Offsets);
	}
}

FVector2D UCanvasPanelSlot::GetSize() const
{
	if ( Slot )
	{
		FMargin Offsets = Slot->OffsetAttr.Get();
		return FVector2D(Offsets.Right, Offsets.Bottom);
	}

	return FVector2D(LayoutData.Offsets.Right, LayoutData.Offsets.Bottom);
}

void UCanvasPanelSlot::SetOffsets(FMargin InOffset)
{
	LayoutData.Offsets = InOffset;
	if ( Slot )
	{
		Slot->Offset(InOffset);
	}
}

FMargin UCanvasPanelSlot::GetOffsets() const
{
	if ( Slot )
	{
		return Slot->OffsetAttr.Get();
	}

	return LayoutData.Offsets;
}

void UCanvasPanelSlot::SetAnchors(FAnchors InAnchors)
{
	LayoutData.Anchors = InAnchors;
	if ( Slot )
	{
		Slot->Anchors(InAnchors);
	}
}

FAnchors UCanvasPanelSlot::GetAnchors() const
{
	if ( Slot )
	{
		return Slot->AnchorsAttr.Get();
	}

	return LayoutData.Anchors;
}

void UCanvasPanelSlot::SetAlignment(FVector2D InAlignment)
{
	LayoutData.Alignment = InAlignment;
	if ( Slot )
	{
		Slot->Alignment(InAlignment);
	}
}

FVector2D UCanvasPanelSlot::GetAlignment() const
{
	if ( Slot )
	{
		return Slot->AlignmentAttr.Get();
	}

	return LayoutData.Alignment;
}

void UCanvasPanelSlot::SetAutoSize(bool InbAutoSize)
{
	bAutoSize = InbAutoSize;
	if ( Slot )
	{
		Slot->AutoSize(InbAutoSize);
	}
}

bool UCanvasPanelSlot::GetAutoSize() const
{
	if ( Slot )
	{
		return Slot->AutoSizeAttr.Get();
	}

	return bAutoSize;
}

void UCanvasPanelSlot::SetZOrder(int32 InZOrder)
{
	ZOrder = InZOrder;
	if ( Slot )
	{
		Slot->ZOrder(InZOrder);
	}
}

int32 UCanvasPanelSlot::GetZOrder() const
{
	if ( Slot )
	{
		return Slot->ZOrderAttr.Get();
	}

	return ZOrder;
}

void UCanvasPanelSlot::SetMinimum(FVector2D InMinimumAnchors)
{
	LayoutData.Anchors.Minimum = InMinimumAnchors;
	if ( Slot )
	{
		Slot->Anchors(LayoutData.Anchors);
	}
}

void UCanvasPanelSlot::SetMaximum(FVector2D InMaximumAnchors)
{
	LayoutData.Anchors.Maximum = InMaximumAnchors;
	if ( Slot )
	{
		Slot->Anchors(LayoutData.Anchors);
	}
}

void UCanvasPanelSlot::SynchronizeProperties()
{
	SetOffsets(LayoutData.Offsets);
	SetAnchors(LayoutData.Anchors);
	SetAlignment(LayoutData.Alignment);
	SetAutoSize(bAutoSize);
	SetZOrder(ZOrder);
}

#if WITH_EDITOR

void UCanvasPanelSlot::PreEditChange(class FEditPropertyChain& PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	SaveBaseLayout();
}

void UCanvasPanelSlot::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	SynchronizeProperties();

	static FName AnchorsProperty(TEXT("Anchors"));

	FEditPropertyChain::TDoubleLinkedListNode* AnchorNode = PropertyChangedEvent.PropertyChain.GetHead()->GetNextNode();
	if ( !AnchorNode )
	{
		return;
	}

	FEditPropertyChain::TDoubleLinkedListNode* LayoutDataNode = AnchorNode->GetNextNode();

	if ( !LayoutDataNode )
	{
		return;
	}

	UProperty* AnchorProperty = LayoutDataNode->GetValue();

	if ( AnchorProperty && AnchorProperty->GetFName() == AnchorsProperty )
	{
		RebaseLayout();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UCanvasPanelSlot::SaveBaseLayout()
{
	// Get the current location
	if ( UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent) )
	{
		FGeometry Geometry;
		if ( Canvas->GetGeometryForSlot(this, Geometry) )
		{
			PreEditGeometry = Geometry;
			PreEditLayoutData = LayoutData;
		}
	}
}

void UCanvasPanelSlot::SetDesiredPosition(FVector2D InPosition)
{
	DesiredPosition = InPosition;
}

void UCanvasPanelSlot::RebaseLayout(bool PreserveSize)
{
	// Ensure we have a parent canvas
	if ( UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent) )
	{
		FGeometry Geometry;
		if ( Canvas->GetGeometryForSlot(this, Geometry) )
		{
			// Calculate the default anchor offset, ie where would this control be laid out if no offset were provided.
			FVector2D CanvasSize = Canvas->GetCanvasWidget()->GetCachedGeometry().Size;
			FMargin AnchorPositions = FMargin(
				LayoutData.Anchors.Minimum.X * CanvasSize.X,
				LayoutData.Anchors.Minimum.Y * CanvasSize.Y,
				LayoutData.Anchors.Maximum.X * CanvasSize.X,
				LayoutData.Anchors.Maximum.Y * CanvasSize.Y);
			FVector2D DefaultAnchorPosition = FVector2D(AnchorPositions.Left, AnchorPositions.Top);

			// Determine the amount that would be offset from the anchor position if alignment was applied.
			FVector2D AlignmentOffset = LayoutData.Alignment * PreEditGeometry.Size;

			FVector2D MoveDelta = Geometry.Position - PreEditGeometry.Position;

			// Determine where the widget's new position needs to be to maintain a stable location when the anchors change.
			FVector2D LeftTopDelta = PreEditGeometry.Position - DefaultAnchorPosition;

			const bool bAnchorsMoved = PreEditLayoutData.Anchors.Minimum != LayoutData.Anchors.Minimum || PreEditLayoutData.Anchors.Maximum != LayoutData.Anchors.Maximum;
			const bool bMoved = PreEditLayoutData.Offsets.Left != LayoutData.Offsets.Left || PreEditLayoutData.Offsets.Top != LayoutData.Offsets.Top;

			if ( bAnchorsMoved )
			{
				// Adjust the size to remain constant
				if ( !LayoutData.Anchors.IsStretchedHorizontal() && PreEditLayoutData.Anchors.IsStretchedHorizontal() )
				{
					// Adjust the position to remain constant
					LayoutData.Offsets.Left = LeftTopDelta.X + AlignmentOffset.X;
					LayoutData.Offsets.Right = PreEditGeometry.Size.X;
				}
				else if ( !PreserveSize && LayoutData.Anchors.IsStretchedHorizontal() && !PreEditLayoutData.Anchors.IsStretchedHorizontal() )
				{
					// Adjust the position to remain constant
					LayoutData.Offsets.Left = 0;
					LayoutData.Offsets.Right = 0;
				}
				else if ( LayoutData.Anchors.IsStretchedHorizontal() )
				{
					// Adjust the position to remain constant
					LayoutData.Offsets.Left = LeftTopDelta.X;
					LayoutData.Offsets.Right = AnchorPositions.Right - ( AnchorPositions.Left + LayoutData.Offsets.Left + PreEditGeometry.Size.X );
				}
				else
				{
					// Adjust the position to remain constant
					LayoutData.Offsets.Left = LeftTopDelta.X + AlignmentOffset.X;
				}

				if ( !LayoutData.Anchors.IsStretchedVertical() && PreEditLayoutData.Anchors.IsStretchedVertical() )
				{
					// Adjust the position to remain constant
					LayoutData.Offsets.Top = LeftTopDelta.Y + AlignmentOffset.Y;
					LayoutData.Offsets.Bottom = PreEditGeometry.Size.Y;
				}
				else if ( !PreserveSize && LayoutData.Anchors.IsStretchedVertical() && !PreEditLayoutData.Anchors.IsStretchedVertical() )
				{
					// Adjust the position to remain constant
					LayoutData.Offsets.Top = 0;
					LayoutData.Offsets.Bottom = 0;
				}
				else if ( LayoutData.Anchors.IsStretchedVertical() )
				{
					// Adjust the position to remain constant
					LayoutData.Offsets.Top = LeftTopDelta.Y;
					LayoutData.Offsets.Bottom = AnchorPositions.Bottom - ( AnchorPositions.Top + LayoutData.Offsets.Top + PreEditGeometry.Size.Y );
				}
				else
				{
					// Adjust the position to remain constant
					LayoutData.Offsets.Top = LeftTopDelta.Y + AlignmentOffset.Y;
				}
			}
			else if ( DesiredPosition.IsSet() )
			{
				FVector2D NewLocalPosition = DesiredPosition.GetValue();

				LayoutData.Offsets.Left = NewLocalPosition.X - AnchorPositions.Left;
				LayoutData.Offsets.Top = NewLocalPosition.Y - AnchorPositions.Top;

				if ( LayoutData.Anchors.IsStretchedHorizontal() )
				{
					LayoutData.Offsets.Right -= LayoutData.Offsets.Left - PreEditLayoutData.Offsets.Left;
				}
				else
				{
					LayoutData.Offsets.Left += AlignmentOffset.X;
				}

				if ( LayoutData.Anchors.IsStretchedVertical() )
				{
					LayoutData.Offsets.Bottom -= LayoutData.Offsets.Top - PreEditLayoutData.Offsets.Top;
				}
				else
				{
					LayoutData.Offsets.Top += AlignmentOffset.Y;
				}

				DesiredPosition.Reset();
			}
			else if ( bMoved )
			{
				//LayoutData.Offsets.Left += LeftTopDelta.X;
				//LayoutData.Offsets.Top += LeftTopDelta.Y;
				LayoutData.Offsets.Left -= DefaultAnchorPosition.X;
				LayoutData.Offsets.Top -= DefaultAnchorPosition.Y;

				// If the slot is stretched horizontally we need to move the right side as it no longer represents width, but
				// now represents margin from the right stretched side.
				if ( LayoutData.Anchors.IsStretchedHorizontal() )
				{
					//LayoutData.Offsets.Right = PreEditLayoutData.Offsets.Top;
				}
				else
				{
					LayoutData.Offsets.Left += AlignmentOffset.X;
				}

				// If the slot is stretched vertically we need to move the bottom side as it no longer represents width, but
				// now represents margin from the bottom stretched side.
				if ( LayoutData.Anchors.IsStretchedVertical() )
				{
					//LayoutData.Offsets.Bottom -= MoveDelta.Y;
				}
				else
				{
					LayoutData.Offsets.Top += AlignmentOffset.Y;
				}
			}
		}

		// Apply the changes to the properties.
		SynchronizeProperties();
	}
}

#endif
