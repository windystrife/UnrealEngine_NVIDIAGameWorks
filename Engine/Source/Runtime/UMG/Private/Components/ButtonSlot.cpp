// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/ButtonSlot.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Input/SButton.h"
#include "Components/Widget.h"

/////////////////////////////////////////////////////
// UButtonSlot

UButtonSlot::UButtonSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Padding = FMargin(4, 2);

	HorizontalAlignment = HAlign_Center;
	VerticalAlignment = VAlign_Center;
}

void UButtonSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	Button.Reset();
}

void UButtonSlot::BuildSlot(TSharedRef<SButton> InButton)
{
	Button = InButton;

	Button->SetContentPadding(Padding);
	Button->SetHAlign(HorizontalAlignment);
	Button->SetVAlign(VerticalAlignment);

	Button->SetContent(Content ? Content->TakeWidget() : SNullWidget::NullWidget);
}

void UButtonSlot::SetPadding(FMargin InPadding)
{
	Padding = InPadding;
	if ( Button.IsValid() )
	{
		Button->SetContentPadding(InPadding);
	}
}

void UButtonSlot::SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)
{
	HorizontalAlignment = InHorizontalAlignment;
	if ( Button.IsValid() )
	{
		Button->SetHAlign(InHorizontalAlignment);
	}
}

void UButtonSlot::SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)
{
	VerticalAlignment = InVerticalAlignment;
	if ( Button.IsValid() )
	{
		Button->SetVAlign(InVerticalAlignment);
	}
}

void UButtonSlot::SynchronizeProperties()
{
	SetPadding(Padding);
	SetHorizontalAlignment(HorizontalAlignment);
	SetVerticalAlignment(VerticalAlignment);
}
