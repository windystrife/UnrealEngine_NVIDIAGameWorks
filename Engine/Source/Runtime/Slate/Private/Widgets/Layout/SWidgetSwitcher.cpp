// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Layout/LayoutUtils.h"


SWidgetSwitcher::SWidgetSwitcher()
	: AllChildren()
{ }


SWidgetSwitcher::FSlot& SWidgetSwitcher::AddSlot( int32 SlotIndex )
{
	FSlot* NewSlot = new FSlot();

	if (!AllChildren.IsValidIndex(SlotIndex))
	{
		AllChildren.Add(NewSlot);
	}
	else
	{
		AllChildren.Insert(NewSlot, SlotIndex);

		// adjust the active WidgetIndex based on this slot change
		if (!WidgetIndex.IsBound() && WidgetIndex.Get() >= SlotIndex)
		{
			WidgetIndex = WidgetIndex.Get() + 1;
		}
	}

	return *NewSlot;
}


void SWidgetSwitcher::Construct( const FArguments& InArgs )
{
	OneDynamicChild = FOneDynamicChild( &AllChildren, &WidgetIndex );

	for (int32 Index = 0; Index < InArgs.Slots.Num(); ++Index)
	{
		AllChildren.Add(InArgs.Slots[Index]);
	}

	WidgetIndex = InArgs._WidgetIndex;
}


TSharedPtr<SWidget> SWidgetSwitcher::GetActiveWidget( ) const
{
	const FSlot* ActiveSlot = GetActiveSlot();

	if (ActiveSlot != nullptr)
	{
		return ActiveSlot->GetWidget();
	}

	return nullptr;
}


TSharedPtr<SWidget> SWidgetSwitcher::GetWidget( int32 SlotIndex ) const
{
	if (AllChildren.IsValidIndex(SlotIndex))
	{
		return AllChildren[SlotIndex].GetWidget();
	}

	return nullptr;
}


int32 SWidgetSwitcher::GetWidgetIndex( TSharedRef<SWidget> Widget ) const
{
	for (int32 Index = 0; Index < AllChildren.Num(); ++Index)
	{
		const FSlot& Slot = AllChildren[Index];

		if (Slot.GetWidget() == Widget)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}


int32 SWidgetSwitcher::RemoveSlot( TSharedRef<SWidget> WidgetToRemove )
{
	for (int32 SlotIndex=0; SlotIndex < AllChildren.Num(); ++SlotIndex)
	{
		if (AllChildren[SlotIndex].GetWidget() == WidgetToRemove)
		{
			// adjust the active WidgetIndex based on this slot change
			if (!WidgetIndex.IsBound())
			{
				int32 ActiveWidgetIndex = WidgetIndex.Get();
				if (ActiveWidgetIndex > 0 && ActiveWidgetIndex >= SlotIndex)
				{
					WidgetIndex = ActiveWidgetIndex - 1;
				}
			}

			AllChildren.RemoveAt(SlotIndex);
			return SlotIndex;
		}
	}

	return -1;
}


void SWidgetSwitcher::SetActiveWidgetIndex( int32 Index )
{
	WidgetIndex = Index;
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}


/* SCompoundWidget interface
 *****************************************************************************/

void SWidgetSwitcher::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	const TAttribute<FVector2D> ContentScale = FVector2D::UnitVector;

	if (AllChildren.Num() > 0)
	{
		const FSlot* ActiveSlotPtr = GetActiveSlot();	// Returns null if unsafe attribute WidgetIndex is out-of-bounds
		if (ActiveSlotPtr != nullptr)
		{
			ArrangeSingleChild(AllottedGeometry, ArrangedChildren, *ActiveSlotPtr, ContentScale);
		}
	}
}
	
FVector2D SWidgetSwitcher::ComputeDesiredSize( float ) const
{
	if (AllChildren.Num() > 0)
	{
		const FSlot* ActiveSlotPtr = GetActiveSlot();	// Returns null if unsafe attribute WidgetIndex is out-of-bounds
		if (ActiveSlotPtr != nullptr)
		{
			return ActiveSlotPtr->GetWidget()->GetDesiredSize();
		}
	}

	return FVector2D::ZeroVector;
}

FChildren* SWidgetSwitcher::GetChildren( )
{
	return &OneDynamicChild;
}

const SWidgetSwitcher::FSlot* SWidgetSwitcher::GetActiveSlot() const
{
	const int32 ActiveWidgetIndex = WidgetIndex.Get();
	if (ActiveWidgetIndex >= 0 && ActiveWidgetIndex < AllChildren.Num())
	{
		return &AllChildren[ActiveWidgetIndex];
	}

	return nullptr;
}
