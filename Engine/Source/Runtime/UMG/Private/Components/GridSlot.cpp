// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/GridSlot.h"
#include "Components/Widget.h"

/////////////////////////////////////////////////////
// UGridSlot

UGridSlot::UGridSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Slot(nullptr)
{
	HorizontalAlignment = HAlign_Fill;
	VerticalAlignment = VAlign_Fill;

	Layer = 0;
	Nudge = FVector2D(0, 0);
}

void UGridSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	Slot = nullptr;
}

void UGridSlot::BuildSlot(TSharedRef<SGridPanel> GridPanel)
{
	Slot = &GridPanel->AddSlot(Column, Row, SGridPanel::Layer(Layer))
		.Padding(Padding)
		.HAlign(HorizontalAlignment)
		.VAlign(VerticalAlignment)
		.RowSpan(RowSpan)
		.ColumnSpan(ColumnSpan)
		.Nudge(Nudge)
		[
			Content == nullptr ? SNullWidget::NullWidget : Content->TakeWidget()
		];
}

void UGridSlot::SetPadding(FMargin InPadding)
{
	Padding = InPadding;
	if ( Slot )
	{
		Slot->Padding(InPadding);
	}
}

void UGridSlot::SetRow(int32 InRow)
{
	Row = InRow;
	if ( Slot )
	{
		Slot->Row(InRow);
	}
}

void UGridSlot::SetRowSpan(int32 InRowSpan)
{
	RowSpan = InRowSpan;
	if ( Slot )
	{
		Slot->RowSpan(InRowSpan);
	}
}

void UGridSlot::SetColumn(int32 InColumn)
{
	Column = InColumn;
	if ( Slot )
	{
		Slot->Column(InColumn);
	}
}

void UGridSlot::SetColumnSpan(int32 InColumnSpan)
{
	ColumnSpan = InColumnSpan;
	if ( Slot )
	{
		Slot->ColumnSpan(InColumnSpan);
	}
}

void UGridSlot::SetLayer(int32 InLayer)
{
	Layer = InLayer;
	if (Slot)
	{
		Slot->Layer(InLayer);
	}
}

void UGridSlot::SetNudge(FVector2D InNudge)
{
	Nudge = InNudge;
	if ( Slot )
	{
		Slot->Nudge(InNudge);
	}
}

void UGridSlot::SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)
{
	HorizontalAlignment = InHorizontalAlignment;
	if ( Slot )
	{
		Slot->HAlignment = InHorizontalAlignment;
	}
}

void UGridSlot::SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)
{
	VerticalAlignment = InVerticalAlignment;
	if ( Slot )
	{
		Slot->VAlignment = InVerticalAlignment;
	}
}

void UGridSlot::SynchronizeProperties()
{
	SetHorizontalAlignment(HorizontalAlignment);
	SetVerticalAlignment(VerticalAlignment);
	SetPadding(Padding);

	SetRow(Row);
	SetRowSpan(RowSpan);
	SetColumn(Column);
	SetColumnSpan(ColumnSpan);
	SetNudge(Nudge);

	SetLayer(Layer);
}
