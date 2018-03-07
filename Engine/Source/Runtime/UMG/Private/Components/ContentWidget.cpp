// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/ContentWidget.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UContentWidget

UContentWidget::UContentWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanHaveMultipleChildren = false;
}

UPanelSlot* UContentWidget::GetContentSlot() const
{
	return Slots.Num() > 0 ? Slots[0] : nullptr;
}

UWidget* UContentWidget::GetContent() const
{
	if ( UPanelSlot* ContentSlot = GetContentSlot() )
	{
		return ContentSlot->Content;
	}

	return nullptr;
}

UPanelSlot* UContentWidget::SetContent(UWidget* Content)
{
	ClearChildren();
	return AddChild(Content);
}

UClass* UContentWidget::GetSlotClass() const
{
	return UPanelSlot::StaticClass();
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
