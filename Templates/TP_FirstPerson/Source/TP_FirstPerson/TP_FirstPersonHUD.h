// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TP_FirstPersonHUD.generated.h"

UCLASS()
class ATP_FirstPersonHUD : public AHUD
{
	GENERATED_BODY()

public:
	ATP_FirstPersonHUD();

	/** Primary draw call for the HUD */
	virtual void DrawHUD() override;

private:
	/** Crosshair asset pointer */
	class UTexture2D* CrosshairTex;

};

