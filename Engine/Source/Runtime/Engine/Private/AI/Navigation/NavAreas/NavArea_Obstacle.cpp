// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavAreas/NavArea_Obstacle.h"

UNavArea_Obstacle::UNavArea_Obstacle(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DrawColor = FColor(127, 51, 0);	// brownish
	DefaultCost = 1000000.0f;
}
