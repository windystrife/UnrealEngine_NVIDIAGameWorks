// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AI/Navigation/NavAreas/NavArea.h"
#include "NavArea_Null.generated.h"

/** In general represents an empty area, that cannot be traversed by anyone. Ever.*/
UCLASS(Config=Engine)
class ENGINE_API UNavArea_Null : public UNavArea
{
	GENERATED_UCLASS_BODY()
};
