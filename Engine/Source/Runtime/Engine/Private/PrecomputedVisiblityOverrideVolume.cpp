// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Engine/CollisionProfile.h"
#include "Components/BrushComponent.h"
#include "Lightmass/PrecomputedVisibilityOverrideVolume.h"

APrecomputedVisibilityOverrideVolume::APrecomputedVisibilityOverrideVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	bColored = true;
	BrushColor.R = 25;
	BrushColor.G = 120;
	BrushColor.B = 90;
	BrushColor.A = 255;

}
