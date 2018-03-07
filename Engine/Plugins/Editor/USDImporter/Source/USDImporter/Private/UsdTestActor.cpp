// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UsdTestActor.h"



AUsdTestActor::AUsdTestActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RootComponent = TestComponent = CreateDefaultSubobject<UUsdTestComponent>(TEXT("Root"));
}

