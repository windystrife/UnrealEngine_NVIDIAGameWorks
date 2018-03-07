// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/HorizontalBoxSlot.h"
#include "Components/Widget.h"

/////////////////////////////////////////////////////
// UHorizontalBoxSlot

UHorizontalBoxSlot::UHorizontalBoxSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HorizontalAlignment = HAlign_Fill;
	VerticalAlignment = VAlign_Fill;
	Size = FSlateChildSize(ESlateSizeRule::Automatic);
}

void UHorizontalBoxSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	Slot = NULL;
}

void UHorizontalBoxSlot::BuildSlot(TSharedRef<SHorizontalBox> HorizontalBox)
{
	Slot = &HorizontalBox->AddSlot()
		.HAlign(HorizontalAlignment)
		.VAlign(VerticalAlignment)
		.Padding(Padding)
		[
			Content == NULL ? SNullWidget::NullWidget : Content->TakeWidget()
		];

	Slot->SizeParam = UWidget::ConvertSerializedSizeParamToRuntime(Size);
}

void UHorizontalBoxSlot::SetPadding(FMargin InPadding)
{
	Padding = InPadding;
	if ( Slot )
	{
		Slot->Padding(InPadding);
	}
}

void UHorizontalBoxSlot::SetSize(FSlateChildSize InSize)
{
	Size = InSize;
	if ( Slot )
	{
		Slot->SizeParam = UWidget::ConvertSerializedSizeParamToRuntime(InSize);
	}
}

void UHorizontalBoxSlot::SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)
{
	HorizontalAlignment = InHorizontalAlignment;
	if ( Slot )
	{
		Slot->HAlign(InHorizontalAlignment);
	}
}

void UHorizontalBoxSlot::SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)
{
	VerticalAlignment = InVerticalAlignment;
	if ( Slot )
	{
		Slot->VAlign(InVerticalAlignment);
	}
}

void UHorizontalBoxSlot::SynchronizeProperties()
{
	SetPadding(Padding);
	SetSize(Size);
	SetHorizontalAlignment(HorizontalAlignment);
	SetVerticalAlignment(VerticalAlignment);
}
