// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/WindowTitleBarAreaSlot.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SWindowTitleBarArea.h"

/////////////////////////////////////////////////////
// UWindowTitleBarAreaSlot

UWindowTitleBarAreaSlot::UWindowTitleBarAreaSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Padding = FMargin(0);

	HorizontalAlignment = HAlign_Fill;
	VerticalAlignment = VAlign_Fill;
}

void UWindowTitleBarAreaSlot::BuildSlot(TSharedRef<SWindowTitleBarArea> InWindowTitleBarArea)
{
	WindowTitleBarArea = InWindowTitleBarArea;

	WindowTitleBarArea->SetPadding(Padding);
	WindowTitleBarArea->SetHAlign(HorizontalAlignment);
	WindowTitleBarArea->SetVAlign(VerticalAlignment);

	WindowTitleBarArea->SetContent(Content ? Content->TakeWidget() : SNullWidget::NullWidget);
}

void UWindowTitleBarAreaSlot::SetPadding(FMargin InPadding)
{
	CastChecked<UWindowTitleBarArea>(Parent)->SetPadding(InPadding);
}

void UWindowTitleBarAreaSlot::SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)
{
	CastChecked<UWindowTitleBarArea>(Parent)->SetHorizontalAlignment(InHorizontalAlignment);
}

void UWindowTitleBarAreaSlot::SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)
{
	CastChecked<UWindowTitleBarArea>(Parent)->SetVerticalAlignment(InVerticalAlignment);
}

void UWindowTitleBarAreaSlot::SynchronizeProperties()
{
	if (WindowTitleBarArea.IsValid())
	{
		SetPadding(Padding);
		SetHorizontalAlignment(HorizontalAlignment);
		SetVerticalAlignment(VerticalAlignment);
	}
}

void UWindowTitleBarAreaSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	WindowTitleBarArea.Reset();
}
