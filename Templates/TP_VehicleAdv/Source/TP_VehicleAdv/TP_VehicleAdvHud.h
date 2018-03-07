// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TP_VehicleAdvHud.generated.h"

UCLASS(config = Game)
class ATP_VehicleAdvHud : public AHUD
{
	GENERATED_BODY()

public:
	ATP_VehicleAdvHud();

	/** Font used to render the vehicle info */
	UPROPERTY()
	UFont* HUDFont;

	// Begin AHUD interface
	virtual void DrawHUD() override;
	// End AHUD interface

};
