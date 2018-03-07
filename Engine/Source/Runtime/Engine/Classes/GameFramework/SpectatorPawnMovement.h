// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Movement component used by SpectatorPawn.
 * Primarily exists to be able to ignore time dilation during tick.
 */
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "SpectatorPawnMovement.generated.h"

UCLASS()
class ENGINE_API USpectatorPawnMovement : public UFloatingPawnMovement
{
	GENERATED_UCLASS_BODY()

	/** If true, component moves at full speed no matter the time dilation. Default is false. */
	UPROPERTY()
	uint32 bIgnoreTimeDilation:1;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
};

