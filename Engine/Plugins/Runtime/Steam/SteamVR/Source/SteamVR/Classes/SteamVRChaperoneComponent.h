// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "SteamVRChaperoneComponent.generated.h"

/**
 * SteamVR Extensions Function Library
 */
UCLASS(meta = (BlueprintSpawnableComponent), ClassGroup = SteamVR)
class STEAMVR_API USteamVRChaperoneComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSteamVRChaperoneEvent);

	UPROPERTY(BlueprintAssignable, Category = "SteamVR")
	FSteamVRChaperoneEvent OnLeaveBounds;

	UPROPERTY(BlueprintAssignable, Category = "SteamVR")
	FSteamVRChaperoneEvent OnReturnToBounds;

	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/**
	* Returns the bounds from the Chaperone, in Unreal-scale HMD-space coordinates, centered around the HMD's calibration origin (0,0,0).  Each set of four bounds will form a quad to define a set of bounds
	*/
	UFUNCTION(BlueprintPure, Category = "SteamVR")
	TArray<FVector> GetBounds() const;

private:
	/** Whether or not we were inside the bounds last time, so we can detect changes */
	bool bWasInsideBounds;
};
