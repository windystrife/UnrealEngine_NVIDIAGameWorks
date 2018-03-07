// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/UniformGridSlot.h"
#include "Components/Widget.h"

/////////////////////////////////////////////////////
// UUniformGridSlot

UUniformGridSlot::UUniformGridSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Slot(NULL)
{
	HorizontalAlignment = HAlign_Left;
	VerticalAlignment = VAlign_Top;
}

void UUniformGridSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	Slot = NULL;
}

void UUniformGridSlot::BuildSlot(TSharedRef<SUniformGridPanel> GridPanel)
{
	Slot = &GridPanel->AddSlot(Column, Row)
		.HAlign(HorizontalAlignment)
		.VAlign(VerticalAlignment)
		[
			Content == NULL ? SNullWidget::NullWidget : Content->TakeWidget()
		];
}

void UUniformGridSlot::SetRow(int32 InRow)
{
	Row = InRow;
	if ( Slot )
	{
		Slot->Row = InRow;
	}
}

void UUniformGridSlot::SetColumn(int32 InColumn)
{
	Column = InColumn;
	if ( Slot )
	{
		Slot->Column = InColumn;
	}
}

void UUniformGridSlot::SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)
{
	HorizontalAlignment = InHorizontalAlignment;
	if ( Slot )
	{
		Slot->HAlignment = InHorizontalAlignment;
	}
}

void UUniformGridSlot::SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)
{
	VerticalAlignment = InVerticalAlignment;
	if ( Slot )
	{
		Slot->VAlignment = InVerticalAlignment;
	}
}

void UUniformGridSlot::SynchronizeProperties()
{
	SetRow(Row);
	SetColumn(Column);
	SetHorizontalAlignment(HorizontalAlignment);
	SetVerticalAlignment(VerticalAlignment);
}
