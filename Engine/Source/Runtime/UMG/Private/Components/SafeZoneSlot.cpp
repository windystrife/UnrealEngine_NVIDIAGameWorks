// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/SafeZoneSlot.h"

#include "Components/SafeZone.h"

USafeZoneSlot::USafeZoneSlot()
{
	bIsTitleSafe = true;
	SafeAreaScale = FMargin(1, 1, 1, 1);
}

void USafeZoneSlot::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if ( IsValid( Parent ) )
	{
		CastChecked< USafeZone >( Parent )->UpdateWidgetProperties();
	}
}
