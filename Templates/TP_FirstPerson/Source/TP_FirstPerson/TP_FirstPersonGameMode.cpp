// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TP_FirstPersonGameMode.h"
#include "TP_FirstPersonHUD.h"
#include "TP_FirstPersonCharacter.h"
#include "UObject/ConstructorHelpers.h"

ATP_FirstPersonGameMode::ATP_FirstPersonGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = ATP_FirstPersonHUD::StaticClass();
}
