// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TP_VehicleGameMode.h"
#include "TP_VehiclePawn.h"
#include "TP_VehicleHud.h"

ATP_VehicleGameMode::ATP_VehicleGameMode()
{
	DefaultPawnClass = ATP_VehiclePawn::StaticClass();
	HUDClass = ATP_VehicleHud::StaticClass();
}
