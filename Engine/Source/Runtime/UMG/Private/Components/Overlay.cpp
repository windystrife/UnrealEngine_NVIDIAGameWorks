// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UOverlay

UOverlay::UOverlay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = false;

	SOverlay::FArguments Defaults;
	Visibility = UWidget::ConvertRuntimeToSerializedVisibility(Defaults._Visibility.Get());
}

void UOverlay::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyOverlay.Reset();
}

UOverlaySlot* UOverlay::AddChildToOverlay(UWidget* Content)
{
	return Cast<UOverlaySlot>(Super::AddChild(Content));
}

UClass* UOverlay::GetSlotClass() const
{
	return UOverlaySlot::StaticClass();
}

void UOverlay::OnSlotAdded(UPanelSlot* InSlot)
{
	// Add the child to the live canvas if it already exists
	if ( MyOverlay.IsValid() )
	{
		CastChecked<UOverlaySlot>(InSlot)->BuildSlot(MyOverlay.ToSharedRef());
	}
}

void UOverlay::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyOverlay.IsValid() )
	{
		TSharedPtr<SWidget> Widget = InSlot->Content->GetCachedWidget();
		if ( Widget.IsValid() )
		{
			MyOverlay->RemoveSlot(Widget.ToSharedRef());
		}
	}
}

TSharedRef<SWidget> UOverlay::RebuildWidget()
{
	MyOverlay = SNew(SOverlay);

	for ( UPanelSlot* PanelSlot : Slots )
	{
		if ( UOverlaySlot* TypedSlot = Cast<UOverlaySlot>(PanelSlot) )
		{
			TypedSlot->Parent = this;
			TypedSlot->BuildSlot(MyOverlay.ToSharedRef());
		}
	}

	return MyOverlay.ToSharedRef();
}

#if WITH_EDITOR

const FText UOverlay::GetPaletteCategory()
{
	return LOCTEXT("Panel", "Panel");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
