// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Lightmass/LightmassCharacterIndirectDetailVolume.h"
#include "Engine/CollisionProfile.h"
#include "Components/BrushComponent.h"


ALightmassCharacterIndirectDetailVolume::ALightmassCharacterIndirectDetailVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	bColored = true;
	BrushColor.R = 155;
	BrushColor.G = 185;
	BrushColor.B = 25;
	BrushColor.A = 255;

}
