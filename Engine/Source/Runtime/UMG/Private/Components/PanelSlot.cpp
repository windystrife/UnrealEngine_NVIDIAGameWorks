// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"

/////////////////////////////////////////////////////
// UPanelSlot

UPanelSlot::UPanelSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPanelSlot::IsDesignTime() const
{
	return Parent->IsDesignTime();
}

void UPanelSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	if ( Content )
	{
		Content->ReleaseSlateResources(bReleaseChildren);
	}
}
