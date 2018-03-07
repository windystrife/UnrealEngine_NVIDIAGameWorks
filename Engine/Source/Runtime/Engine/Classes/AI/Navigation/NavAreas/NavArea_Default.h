// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AI/Navigation/NavAreas/NavArea.h"
#include "NavArea_Default.generated.h"

/** Regular navigation area, applied to entire navigation data by default */
UCLASS(Config=Engine)
class ENGINE_API UNavArea_Default : public UNavArea
{
	GENERATED_UCLASS_BODY()
};
