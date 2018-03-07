// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestGameMode.h"
#include "GameFramework/SpectatorPawn.h"

AFunctionalTestGameMode::AFunctionalTestGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}
