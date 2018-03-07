// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/OverlaySlot.h"
#include "Components/Widget.h"

/////////////////////////////////////////////////////
// UOverlaySlot

UOverlaySlot::UOverlaySlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HorizontalAlignment = HAlign_Left;
	VerticalAlignment = VAlign_Top;
}

void UOverlaySlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	Slot = NULL;
}

void UOverlaySlot::BuildSlot(TSharedRef<SOverlay> Overlay)
{
	Slot = &Overlay->AddSlot()
		.Padding(Padding)
		.HAlign(HorizontalAlignment)
		.VAlign(VerticalAlignment)
		[
			Content == NULL ? SNullWidget::NullWidget : Content->TakeWidget()
		];
}

void UOverlaySlot::SetPadding(FMargin InPadding)
{
	Padding = InPadding;
	if ( Slot )
	{
		Slot->Padding(InPadding);
	}
}

void UOverlaySlot::SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)
{
	HorizontalAlignment = InHorizontalAlignment;
	if ( Slot )
	{
		Slot->HAlign(InHorizontalAlignment);
	}
}

void UOverlaySlot::SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)
{
	VerticalAlignment = InVerticalAlignment;
	if ( Slot )
	{
		Slot->VAlign(InVerticalAlignment);
	}
}

void UOverlaySlot::SynchronizeProperties()
{
	SetPadding(Padding);
	SetHorizontalAlignment(HorizontalAlignment);
	SetVerticalAlignment(VerticalAlignment);
}
