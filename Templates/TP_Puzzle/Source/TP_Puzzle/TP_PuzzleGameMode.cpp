// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TP_PuzzleGameMode.h"
#include "TP_PuzzlePlayerController.h"
#include "TP_PuzzlePawn.h"

ATP_PuzzleGameMode::ATP_PuzzleGameMode()
{
	// no pawn by default
	DefaultPawnClass = ATP_PuzzlePawn::StaticClass();
	// use our own player controller class
	PlayerControllerClass = ATP_PuzzlePlayerController::StaticClass();
}
