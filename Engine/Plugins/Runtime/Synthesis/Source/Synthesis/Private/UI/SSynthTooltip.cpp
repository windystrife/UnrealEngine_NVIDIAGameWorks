// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UI/SSynthTooltip.h"
#include "Widgets/SToolTip.h"
#include "SlateApplication.h"

SSynthTooltip::~SSynthTooltip()
{
	if (WindowContainer.IsValid())
	{
		WindowContainer->RequestDestroyWindow();
	}
	WindowContainer.Reset();
}

void SSynthTooltip::Construct(const FArguments& InArgs)
{
	// Show window at the cursor
	WindowContainer = SWindow::MakeCursorDecorator();
	FSlateApplication::Get().AddWindow(WindowContainer.ToSharedRef(), false);

	TooltipText = SNew(STextBlock)
		.Text(FText::GetEmpty());

	WindowContainer->SetContent(
		// Im using Stooltip only because it looks nicer than empty window
		SNew(SToolTip)
		.Content()
		[
			TooltipText.ToSharedRef()
		]);

	const int32 NumSlots = InArgs.Slots.Num();
	for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
	{
		Children.Add(InArgs.Slots[SlotIndex]);
	}

	bIsVisible = false;
}

void SSynthTooltip::SetWindowContainerVisibility(bool bShowVisibility)
{
	if (WindowContainer.IsValid())
	{
		if (bShowVisibility && !bIsVisible)
		{
			bIsVisible = true;
			WindowContainer->MoveWindowTo(WindowPosition);
			WindowContainer->ShowWindow();
		}
		else if (!bShowVisibility && bIsVisible)
		{
			bIsVisible = false;
			WindowContainer->HideWindow();
		}
	}
}

void SSynthTooltip::SetOverlayWindowPosition(FVector2D Position)
{
	WindowPosition = Position;
	WindowContainer->MoveWindowTo(WindowPosition);
}

void SSynthTooltip::SetOverlayText(const FText& InText)
{
	if (TooltipText.IsValid())
	{
		TooltipText->SetText(InText);
	}
}
