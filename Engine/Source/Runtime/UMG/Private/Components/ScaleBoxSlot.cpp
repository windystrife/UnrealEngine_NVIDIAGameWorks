// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/ScaleBoxSlot.h"
#include "Widgets/SNullWidget.h"
#include "Components/Widget.h"
#include "Widgets/Layout/SScaleBox.h"

/////////////////////////////////////////////////////
// UScaleBoxSlot

UScaleBoxSlot::UScaleBoxSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Padding = FMargin(0, 0);

	HorizontalAlignment = HAlign_Center;
	VerticalAlignment = VAlign_Center;
}

void UScaleBoxSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	ScaleBox.Reset();
}

void UScaleBoxSlot::BuildSlot(TSharedRef<SScaleBox> InScaleBox)
{
	ScaleBox = InScaleBox;

	//ScaleBox->SetPadding(Padding);
	ScaleBox->SetHAlign(HorizontalAlignment);
	ScaleBox->SetVAlign(VerticalAlignment);

	ScaleBox->SetContent(Content ? Content->TakeWidget() : SNullWidget::NullWidget);
}

void UScaleBoxSlot::SetPadding(FMargin InPadding)
{
	Padding = InPadding;
	if ( ScaleBox.IsValid() )
	{
		//ScaleBox->SetPadding(InPadding);
	}
}

void UScaleBoxSlot::SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)
{
	HorizontalAlignment = InHorizontalAlignment;
	if ( ScaleBox.IsValid() )
	{
		ScaleBox->SetHAlign(InHorizontalAlignment);
	}
}

void UScaleBoxSlot::SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)
{
	VerticalAlignment = InVerticalAlignment;
	if ( ScaleBox.IsValid() )
	{
		ScaleBox->SetVAlign(InVerticalAlignment);
	}
}

void UScaleBoxSlot::SynchronizeProperties()
{
	SetPadding(Padding);
	SetHorizontalAlignment(HorizontalAlignment);
	SetVerticalAlignment(VerticalAlignment);
}
