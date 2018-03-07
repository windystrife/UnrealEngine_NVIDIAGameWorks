// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CableActor.h"
#include "CableComponent.h"


ACableActor::ACableActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CableComponent = CreateDefaultSubobject<UCableComponent>(TEXT("CableComponent0"));
	RootComponent = CableComponent;
}
