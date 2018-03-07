// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/ScrollBoxSlot.h"
#include "Components/Widget.h"

/////////////////////////////////////////////////////
// UScrollBoxSlot

UScrollBoxSlot::UScrollBoxSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Slot(nullptr)
{
	HorizontalAlignment = HAlign_Fill;
}

void UScrollBoxSlot::BuildSlot(TSharedRef<SScrollBox> ScrollBox)
{
	Slot = &ScrollBox->AddSlot()
		.Padding(Padding)
		.HAlign(HorizontalAlignment)
		[
			Content == nullptr ? SNullWidget::NullWidget : Content->TakeWidget()
		];
}

void UScrollBoxSlot::SetPadding(FMargin InPadding)
{
	Padding = InPadding;
	if ( Slot )
	{
		Slot->Padding(InPadding);
	}
}

void UScrollBoxSlot::SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)
{
	HorizontalAlignment = InHorizontalAlignment;
	if ( Slot )
	{
		Slot->HAlign(InHorizontalAlignment);
	}
}

void UScrollBoxSlot::SynchronizeProperties()
{
	SetPadding(Padding);
	SetHorizontalAlignment(HorizontalAlignment);
}

void UScrollBoxSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Slot = nullptr;
}
