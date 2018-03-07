// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AI/Navigation/NavAreas/NavArea.h"
#include "NavArea_Obstacle.generated.h"

/** In general represents a high cost area, that shouldn't be traversed by anyone unless no other path exist.*/
UCLASS(Config = Engine)
class ENGINE_API UNavArea_Obstacle : public UNavArea
{
	GENERATED_UCLASS_BODY()
};
