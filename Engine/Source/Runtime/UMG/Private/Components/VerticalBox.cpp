// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UVerticalBox

UVerticalBox::UVerticalBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = false;

	SVerticalBox::FArguments Defaults;
	Visibility = UWidget::ConvertRuntimeToSerializedVisibility(Defaults._Visibility.Get());
}

void UVerticalBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyVerticalBox.Reset();
}

UClass* UVerticalBox::GetSlotClass() const
{
	return UVerticalBoxSlot::StaticClass();
}

void UVerticalBox::OnSlotAdded(UPanelSlot* InSlot)
{
	// Add the child to the live canvas if it already exists
	if ( MyVerticalBox.IsValid() )
	{
		CastChecked<UVerticalBoxSlot>(InSlot)->BuildSlot(MyVerticalBox.ToSharedRef());
	}
}

void UVerticalBox::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyVerticalBox.IsValid() )
	{
		TSharedPtr<SWidget> Widget = InSlot->Content->GetCachedWidget();
		if ( Widget.IsValid() )
		{
			MyVerticalBox->RemoveSlot(Widget.ToSharedRef());
		}
	}
}

UVerticalBoxSlot* UVerticalBox::AddChildToVerticalBox(UWidget* Content)
{
	return Cast<UVerticalBoxSlot>(Super::AddChild(Content));
}

TSharedRef<SWidget> UVerticalBox::RebuildWidget()
{
	MyVerticalBox = SNew(SVerticalBox);

	for ( UPanelSlot* PanelSlot : Slots )
	{
		if ( UVerticalBoxSlot* TypedSlot = Cast<UVerticalBoxSlot>(PanelSlot) )
		{
			TypedSlot->Parent = this;
			TypedSlot->BuildSlot(MyVerticalBox.ToSharedRef());
		}
	}

	return MyVerticalBox.ToSharedRef();
}

#if WITH_EDITOR

const FText UVerticalBox::GetPaletteCategory()
{
	return LOCTEXT("Panel", "Panel");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
