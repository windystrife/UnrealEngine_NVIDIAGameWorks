// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UWrapBox

UWrapBox::UWrapBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = false;

	SWrapBox::FArguments Defaults;
	Visibility = UWidget::ConvertRuntimeToSerializedVisibility(Defaults._Visibility.Get());

	WrapWidth = 500;
	bExplicitWrapWidth = false;
}

void UWrapBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyWrapBox.Reset();
}

UClass* UWrapBox::GetSlotClass() const
{
	return UWrapBoxSlot::StaticClass();
}

void UWrapBox::OnSlotAdded(UPanelSlot* InSlot)
{
	// Add the child to the live canvas if it already exists
	if ( MyWrapBox.IsValid() )
	{
		CastChecked<UWrapBoxSlot>(InSlot)->BuildSlot(MyWrapBox.ToSharedRef());
	}
}

void UWrapBox::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyWrapBox.IsValid() )
	{
		TSharedPtr<SWidget> Widget = InSlot->Content->GetCachedWidget();
		if ( Widget.IsValid() )
		{
			MyWrapBox->RemoveSlot(Widget.ToSharedRef());
		}
	}
}

UWrapBoxSlot* UWrapBox::AddChildWrapBox(UWidget* Content)
{
	return Cast<UWrapBoxSlot>(Super::AddChild(Content));
}

TSharedRef<SWidget> UWrapBox::RebuildWidget()
{
	MyWrapBox = SNew(SWrapBox)
		.UseAllottedWidth(!bExplicitWrapWidth)
		.PreferredWidth(WrapWidth);

	for ( UPanelSlot* PanelSlot : Slots )
	{
		if ( UWrapBoxSlot* TypedSlot = Cast<UWrapBoxSlot>(PanelSlot) )
		{
			TypedSlot->Parent = this;
			TypedSlot->BuildSlot(MyWrapBox.ToSharedRef());
		}
	}

	return MyWrapBox.ToSharedRef();
}

void UWrapBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyWrapBox->SetInnerSlotPadding(InnerSlotPadding);
	MyWrapBox->SetUseAllottedWidth(!bExplicitWrapWidth);
	MyWrapBox->SetWrapWidth(WrapWidth);
}

void UWrapBox::SetInnerSlotPadding(FVector2D InPadding)
{
	InnerSlotPadding = InPadding;

	if ( MyWrapBox.IsValid() )
	{
		MyWrapBox->SetInnerSlotPadding(InPadding);
	}
}

#if WITH_EDITOR

const FText UWrapBox::GetPaletteCategory()
{
	return LOCTEXT("Panel", "Panel");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
