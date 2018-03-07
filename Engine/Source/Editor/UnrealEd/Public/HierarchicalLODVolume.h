// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Volume.h"
#include "HierarchicalLODVolume.generated.h"

/** An invisible volume used to manually define/create a HLOD cluster. */
UCLASS(MinimalAPI)
class AHierarchicalLODVolume : public AVolume
{
	GENERATED_UCLASS_BODY()
};
