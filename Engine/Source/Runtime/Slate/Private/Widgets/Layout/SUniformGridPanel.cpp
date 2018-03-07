// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SUniformGridPanel.h"
#include "Layout/LayoutUtils.h"

SUniformGridPanel::SUniformGridPanel()
: Children()
{
}

void SUniformGridPanel::Construct( const FArguments& InArgs )
{
	SlotPadding = InArgs._SlotPadding;
	NumColumns = 0;
	NumRows = 0;
	MinDesiredSlotWidth = InArgs._MinDesiredSlotWidth.Get();
	MinDesiredSlotHeight = InArgs._MinDesiredSlotHeight.Get();

	Children.Reserve( InArgs.Slots.Num() );
	for (int32 ChildIndex=0; ChildIndex < InArgs.Slots.Num(); ChildIndex++)
	{
		FSlot* ChildSlot = InArgs.Slots[ChildIndex];
		Children.Add( ChildSlot );
	}
}

void SUniformGridPanel::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	if ( Children.Num() > 0 )
	{
		const FVector2D CellSize(AllottedGeometry.GetLocalSize().X / NumColumns, AllottedGeometry.GetLocalSize().Y / NumRows);
		const FMargin& CurrentSlotPadding(SlotPadding.Get());
		for ( int32 ChildIndex=0; ChildIndex < Children.Num(); ++ChildIndex )
		{
			const FSlot& Child = Children[ChildIndex];
			const EVisibility ChildVisibility = Child.GetWidget()->GetVisibility();
			if ( ArrangedChildren.Accepts(ChildVisibility) )
			{
				// Do the standard arrangement of elements within a slot
				// Takes care of alignment and padding.
				AlignmentArrangeResult XAxisResult = AlignChild<Orient_Horizontal>(CellSize.X, Child, CurrentSlotPadding);
				AlignmentArrangeResult YAxisResult = AlignChild<Orient_Vertical>(CellSize.Y, Child, CurrentSlotPadding);

				ArrangedChildren.AddWidget(ChildVisibility,
					AllottedGeometry.MakeChild(Child.GetWidget(),
					FVector2D(CellSize.X*Child.Column + XAxisResult.Offset, CellSize.Y*Child.Row + YAxisResult.Offset),
					FVector2D(XAxisResult.Size, YAxisResult.Size)
					));
			}

		}
	}
}

FVector2D SUniformGridPanel::ComputeDesiredSize( float ) const
{
	FVector2D MaxChildDesiredSize = FVector2D::ZeroVector;
	const FVector2D SlotPaddingDesiredSize = SlotPadding.Get().GetDesiredSize();
	
	const float CachedMinDesiredSlotWidth = MinDesiredSlotWidth.Get();
	const float CachedMinDesiredSlotHeight = MinDesiredSlotHeight.Get();
	
	NumColumns = 0;
	NumRows = 0;

	for ( int32 ChildIndex=0; ChildIndex < Children.Num(); ++ChildIndex )
	{
		const FSlot& Child = Children[ ChildIndex ];

		if (Child.GetWidget()->GetVisibility() != EVisibility::Collapsed)
		{
			// A single cell at (N,M) means our grid size is (N+1, M+1)
			NumColumns = FMath::Max(Child.Column + 1, NumColumns);
			NumRows = FMath::Max(Child.Row + 1, NumRows);

			FVector2D ChildDesiredSize = Child.GetWidget()->GetDesiredSize() + SlotPaddingDesiredSize;

			ChildDesiredSize.X = FMath::Max( ChildDesiredSize.X, CachedMinDesiredSlotWidth);
			ChildDesiredSize.Y = FMath::Max( ChildDesiredSize.Y, CachedMinDesiredSlotHeight);

			MaxChildDesiredSize.X = FMath::Max( MaxChildDesiredSize.X, ChildDesiredSize.X );
			MaxChildDesiredSize.Y = FMath::Max( MaxChildDesiredSize.Y, ChildDesiredSize.Y );
		}
	}

	return FVector2D( NumColumns*MaxChildDesiredSize.X, NumRows*MaxChildDesiredSize.Y );
}

FChildren* SUniformGridPanel::GetChildren()
{
	return &Children;
}

void SUniformGridPanel::SetSlotPadding(TAttribute<FMargin> InSlotPadding)
{
	SlotPadding = InSlotPadding;
}

void SUniformGridPanel::SetMinDesiredSlotWidth(TAttribute<float> InMinDesiredSlotWidth)
{
	MinDesiredSlotWidth = InMinDesiredSlotWidth;
}

void SUniformGridPanel::SetMinDesiredSlotHeight(TAttribute<float> InMinDesiredSlotHeight)
{
	MinDesiredSlotHeight = InMinDesiredSlotHeight;
}

SUniformGridPanel::FSlot& SUniformGridPanel::AddSlot( int32 Column, int32 Row )
{
	FSlot& NewSlot = *(new FSlot( Column, Row ));

	Children.Add( &NewSlot );

	return NewSlot;
}

bool SUniformGridPanel::RemoveSlot( const TSharedRef<SWidget>& SlotWidget )
{
	for (int32 SlotIdx = 0; SlotIdx < Children.Num(); ++SlotIdx)
	{
		if ( SlotWidget == Children[SlotIdx].GetWidget() )
		{
			Children.RemoveAt(SlotIdx);
			return true;
		}
	}
	
	return false;
}

void SUniformGridPanel::ClearChildren()
{
	NumColumns = 0;
	NumRows = 0;
	Children.Empty();
}
