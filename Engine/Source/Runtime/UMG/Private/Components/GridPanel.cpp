// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/GridPanel.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Components/GridSlot.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UGridPanel

UGridPanel::UGridPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = false;

	SGridPanel::FArguments Defaults;
	Visibility = UWidget::ConvertRuntimeToSerializedVisibility(Defaults._Visibility.Get());
}

void UGridPanel::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyGridPanel.Reset();
}

UClass* UGridPanel::GetSlotClass() const
{
	return UGridSlot::StaticClass();
}

void UGridPanel::OnSlotAdded(UPanelSlot* InSlot)
{
	// Add the child to the live canvas if it already exists
	if ( MyGridPanel.IsValid() )
	{
		CastChecked<UGridSlot>(InSlot)->BuildSlot(MyGridPanel.ToSharedRef());
	}
}

void UGridPanel::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyGridPanel.IsValid() )
	{
		TSharedPtr<SWidget> Widget = InSlot->Content->GetCachedWidget();
		if ( Widget.IsValid() )
		{
			MyGridPanel->RemoveSlot(Widget.ToSharedRef());
		}
	}
}

TSharedRef<SWidget> UGridPanel::RebuildWidget()
{
	MyGridPanel = SNew(SGridPanel);

	for ( UPanelSlot* PanelSlot : Slots )
	{
		if ( UGridSlot* TypedSlot = Cast<UGridSlot>(PanelSlot) )
		{
			TypedSlot->Parent = this;
			TypedSlot->BuildSlot( MyGridPanel.ToSharedRef() );
		}
	}

	return MyGridPanel.ToSharedRef();
}

UGridSlot* UGridPanel::AddChildToGrid(UWidget* Content)
{
	return Cast<UGridSlot>(Super::AddChild(Content));
}

void UGridPanel::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyGridPanel->ClearFill();

	for ( int ColumnIndex = 0; ColumnIndex < ColumnFill.Num(); ColumnIndex++ )
	{
		MyGridPanel->SetColumnFill(ColumnIndex, ColumnFill[ColumnIndex]);
	}

	for ( int RowIndex = 0; RowIndex < RowFill.Num(); RowIndex++ )
	{
		MyGridPanel->SetRowFill(RowIndex, RowFill[RowIndex]);
	}
}

#if WITH_EDITOR

const FText UGridPanel::GetPaletteCategory()
{
	return LOCTEXT("Panel", "Panel");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
