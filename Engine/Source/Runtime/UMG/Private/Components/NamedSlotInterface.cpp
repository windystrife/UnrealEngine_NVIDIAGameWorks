// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/NamedSlotInterface.h"
#include "Components/Widget.h"

UNamedSlotInterface::UNamedSlotInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool INamedSlotInterface::ContainsContent(UWidget* Content) const
{
	TArray<FName> SlotNames;
	GetSlotNames(SlotNames);

	for ( const FName& SlotName : SlotNames )
	{
		if ( GetContentForSlot(SlotName) == Content )
		{
			return true;
		}
	}

	return false;
}

void INamedSlotInterface::ReleaseNamedSlotSlateResources(bool bReleaseChildren)
{
	if ( bReleaseChildren )
	{
		TArray<FName> SlotNames;
		GetSlotNames(SlotNames);

		for ( const FName& SlotName : SlotNames )
		{
			if ( UWidget* Content = GetContentForSlot(SlotName) )
			{
				Content->ReleaseSlateResources(bReleaseChildren);
			}
		}
	}
}
