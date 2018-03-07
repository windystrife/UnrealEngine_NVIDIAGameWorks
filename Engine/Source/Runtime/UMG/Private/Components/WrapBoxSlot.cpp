// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/WrapBoxSlot.h"
#include "Components/Widget.h"

/////////////////////////////////////////////////////
// UWrapBoxSlot

UWrapBoxSlot::UWrapBoxSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Slot(NULL)
{
	HorizontalAlignment = HAlign_Fill;
	VerticalAlignment = VAlign_Fill;

	bFillEmptySpace = false;
	FillSpanWhenLessThan = 0;
}

void UWrapBoxSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	Slot = NULL;
}

void UWrapBoxSlot::BuildSlot(TSharedRef<SWrapBox> WrapBox)
{
	Slot = &WrapBox->AddSlot()
		.Padding(Padding)
		.HAlign(HorizontalAlignment)
		.VAlign(VerticalAlignment)
		.FillEmptySpace(bFillEmptySpace)
		.FillLineWhenWidthLessThan(FillSpanWhenLessThan == 0 ? TOptional<float>() : TOptional<float>(FillSpanWhenLessThan))
		[
			Content == NULL ? SNullWidget::NullWidget : Content->TakeWidget()
		];
}

void UWrapBoxSlot::SetPadding(FMargin InPadding)
{
	Padding = InPadding;
	if ( Slot )
	{
		Slot->Padding(InPadding);
	}
}

void UWrapBoxSlot::SetFillEmptySpace(bool InbFillEmptySpace)
{
	bFillEmptySpace = InbFillEmptySpace;
	if ( Slot )
	{
		Slot->FillEmptySpace(InbFillEmptySpace);
	}
}

void UWrapBoxSlot::SetFillSpanWhenLessThan(float InFillSpanWhenLessThan)
{
	FillSpanWhenLessThan = InFillSpanWhenLessThan;
	if ( Slot )
	{
		Slot->FillLineWhenWidthLessThan(InFillSpanWhenLessThan == 0 ? TOptional<float>() : TOptional<float>(InFillSpanWhenLessThan));
	}
}

void UWrapBoxSlot::SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)
{
	HorizontalAlignment = InHorizontalAlignment;
	if ( Slot )
	{
		Slot->HAlign(InHorizontalAlignment);
	}
}

void UWrapBoxSlot::SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)
{
	VerticalAlignment = InVerticalAlignment;
	if ( Slot )
	{
		Slot->VAlign(InVerticalAlignment);
	}
}

void UWrapBoxSlot::SynchronizeProperties()
{
	SetPadding(Padding);
	SetFillEmptySpace(bFillEmptySpace);
	SetFillSpanWhenLessThan(FillSpanWhenLessThan);
	SetHorizontalAlignment(HorizontalAlignment);
	SetVerticalAlignment(VerticalAlignment);
}
