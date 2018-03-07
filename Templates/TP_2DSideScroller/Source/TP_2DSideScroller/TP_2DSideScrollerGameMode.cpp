// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TP_2DSideScrollerGameMode.h"
#include "TP_2DSideScrollerCharacter.h"

ATP_2DSideScrollerGameMode::ATP_2DSideScrollerGameMode()
{
	// Set default pawn class to our character
	DefaultPawnClass = ATP_2DSideScrollerCharacter::StaticClass();	
}
