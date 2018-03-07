// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/WidgetSwitcherSlot.h"
#include "SlateFwd.h"
#include "Components/Widget.h"

/////////////////////////////////////////////////////
// UWidgetSwitcherSlot

UWidgetSwitcherSlot::UWidgetSwitcherSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Slot(NULL)
{
	HorizontalAlignment = HAlign_Fill;
	VerticalAlignment = VAlign_Fill;
}

void UWidgetSwitcherSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	Slot = NULL;
}

void UWidgetSwitcherSlot::BuildSlot(TSharedRef<SWidgetSwitcher> WidgetSwitcher)
{
	Slot = &WidgetSwitcher->AddSlot()
		.Padding(Padding)
		.HAlign(HorizontalAlignment)
		.VAlign(VerticalAlignment)
		[
			Content == NULL ? SNullWidget::NullWidget : Content->TakeWidget()
		];
}

void UWidgetSwitcherSlot::SetContent(UWidget* NewContent)
{
	Content = NewContent;
	if (Slot)
	{
		Slot->AttachWidget(NewContent ? NewContent->TakeWidget() : SNullWidget::NullWidget);
	}
}

void UWidgetSwitcherSlot::SetPadding(FMargin InPadding)
{
	Padding = InPadding;
	if ( Slot )
	{
		Slot->Padding(InPadding);
	}
}

void UWidgetSwitcherSlot::SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)
{
	HorizontalAlignment = InHorizontalAlignment;
	if ( Slot )
	{
		Slot->HAlign(InHorizontalAlignment);
	}
}

void UWidgetSwitcherSlot::SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)
{
	VerticalAlignment = InVerticalAlignment;
	if ( Slot )
	{
		Slot->VAlign(InVerticalAlignment);
	}
}

void UWidgetSwitcherSlot::SynchronizeProperties()
{
	SetPadding(Padding);
	SetHorizontalAlignment(HorizontalAlignment);
	SetVerticalAlignment(VerticalAlignment);
}
