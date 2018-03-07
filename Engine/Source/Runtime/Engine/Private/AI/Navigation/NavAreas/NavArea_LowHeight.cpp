// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavAreas/NavArea_LowHeight.h"

UNavArea_LowHeight::UNavArea_LowHeight(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DefaultCost = BIG_NUMBER;
	DrawColor = FColor(0, 0, 128);

	// can't traverse
	AreaFlags = 0;
}
