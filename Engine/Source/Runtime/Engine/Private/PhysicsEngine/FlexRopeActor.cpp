// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PhysicsEngine/FlexRopeActor.h"
#include "PhysicsEngine/FlexRopeComponent.h"

AFlexRopeActor::AFlexRopeActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FlexRopeComponent = ObjectInitializer.CreateDefaultSubobject<UFlexRopeComponent>(this, TEXT("FlexRopeComponent0"));
	RootComponent = FlexRopeComponent;
}