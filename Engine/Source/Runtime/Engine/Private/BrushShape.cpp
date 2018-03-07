// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/BrushShape.h"
#include "Engine/CollisionProfile.h"
#include "Components/BrushComponent.h"

ABrushShape::ABrushShape(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	GetBrushComponent()->AlwaysLoadOnClient = true;
	GetBrushComponent()->AlwaysLoadOnServer = false;

}

