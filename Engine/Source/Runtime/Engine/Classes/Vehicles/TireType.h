// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/DataAsset.h"
#include "TireType.generated.h"

/** DEPRECATED - Only used to allow conversion to new TireConfig in PhysXVehicles plugin */
UCLASS()
class ENGINE_API UTireType : public UDataAsset
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleAnywhere, Category = Deprecated)
	float FrictionScale;
};
