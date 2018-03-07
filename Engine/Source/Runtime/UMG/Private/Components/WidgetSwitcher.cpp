// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/WidgetSwitcher.h"
#include "SlateFwd.h"
#include "Components/WidgetSwitcherSlot.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UWidgetSwitcher

UWidgetSwitcher::UWidgetSwitcher(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = true;

	SWidgetSwitcher::FArguments Defaults;
	Visibility = UWidget::ConvertRuntimeToSerializedVisibility(Defaults._Visibility.Get());
}

void UWidgetSwitcher::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyWidgetSwitcher.Reset();
}

int32 UWidgetSwitcher::GetNumWidgets() const
{
	if ( MyWidgetSwitcher.IsValid() )
	{
		return MyWidgetSwitcher->GetNumWidgets();
	}

	return Slots.Num();
}

int32 UWidgetSwitcher::GetActiveWidgetIndex() const
{
	if ( MyWidgetSwitcher.IsValid() )
	{
		return MyWidgetSwitcher->GetActiveWidgetIndex();
	}

	return ActiveWidgetIndex;
}

void UWidgetSwitcher::SetActiveWidgetIndex(int32 Index)
{
	ActiveWidgetIndex = Index;
	if ( MyWidgetSwitcher.IsValid() )
	{
		// Ensure the index is clamped to a valid range.
		int32 SafeIndex = FMath::Clamp(ActiveWidgetIndex, 0, FMath::Max(0, Slots.Num() - 1));
		MyWidgetSwitcher->SetActiveWidgetIndex(SafeIndex);
	}
}

void UWidgetSwitcher::SetActiveWidget(UWidget* Widget)
{
	ActiveWidgetIndex = GetChildIndex(Widget);
	if ( MyWidgetSwitcher.IsValid() )
	{
		// Ensure the index is clamped to a valid range.
		int32 SafeIndex = FMath::Clamp(ActiveWidgetIndex, 0, FMath::Max(0, Slots.Num() - 1));
		MyWidgetSwitcher->SetActiveWidgetIndex(SafeIndex);
	}
}

UWidget* UWidgetSwitcher::GetWidgetAtIndex( int32 Index ) const
{
	if ( Slots.IsValidIndex( Index ) )
	{
		return Slots[ Index ]->Content;
	}

	return nullptr;
}

UWidget* UWidgetSwitcher::GetActiveWidget()const
{
	return GetWidgetAtIndex(GetActiveWidgetIndex());
}

UClass* UWidgetSwitcher::GetSlotClass() const
{
	return UWidgetSwitcherSlot::StaticClass();
}

void UWidgetSwitcher::OnSlotAdded(UPanelSlot* InSlot)
{
	// Add the child to the live canvas if it already exists
	if ( MyWidgetSwitcher.IsValid() )
	{
		CastChecked<UWidgetSwitcherSlot>(InSlot)->BuildSlot(MyWidgetSwitcher.ToSharedRef());
	}
}

void UWidgetSwitcher::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyWidgetSwitcher.IsValid() )
	{
		TSharedPtr<SWidget> Widget = InSlot->Content->GetCachedWidget();
		if ( Widget.IsValid() )
		{
			MyWidgetSwitcher->RemoveSlot(Widget.ToSharedRef());
		}
	}
}

TSharedRef<SWidget> UWidgetSwitcher::RebuildWidget()
{
	MyWidgetSwitcher = SNew(SWidgetSwitcher);

	for ( UPanelSlot* PanelSlot : Slots )
	{
		if ( UWidgetSwitcherSlot* TypedSlot = Cast<UWidgetSwitcherSlot>(PanelSlot) )
		{
			TypedSlot->Parent = this;
			TypedSlot->BuildSlot(MyWidgetSwitcher.ToSharedRef());
		}
	}

	return MyWidgetSwitcher.ToSharedRef();
}

void UWidgetSwitcher::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	SetActiveWidgetIndex(ActiveWidgetIndex);
}

#if WITH_EDITOR

const FText UWidgetSwitcher::GetPaletteCategory()
{
	return LOCTEXT("Panel", "Panel");
}

void UWidgetSwitcher::OnDescendantSelectedByDesigner(UWidget* DescendantWidget)
{
	// Temporarily sets the active child to the selected child to make
	// dragging and dropping easier in the editor.
	UWidget* SelectedChild = UWidget::FindChildContainingDescendant(this, DescendantWidget);
	if ( SelectedChild )
	{
		int32 OverrideIndex = GetChildIndex(SelectedChild);
		if ( OverrideIndex != -1 && MyWidgetSwitcher.IsValid() )
		{
			MyWidgetSwitcher->SetActiveWidgetIndex(OverrideIndex);
		}
	}
}

void UWidgetSwitcher::OnDescendantDeselectedByDesigner(UWidget* DescendantWidget)
{
	SetActiveWidgetIndex(ActiveWidgetIndex);
}

void UWidgetSwitcher::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	ActiveWidgetIndex = FMath::Clamp(ActiveWidgetIndex, 0, FMath::Max(0, Slots.Num() - 1));

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
