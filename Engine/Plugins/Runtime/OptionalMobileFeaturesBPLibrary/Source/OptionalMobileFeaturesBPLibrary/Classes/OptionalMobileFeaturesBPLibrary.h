// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "OptionalMobileFeaturesBPLibrary.generated.h"

UCLASS()
class UOptionalMobileFeaturesBPLibrary :
	public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Returns the current volume state of the device in a range of 0-100 (%)  */
	UFUNCTION(BlueprintCallable, Category="Mobile")
	static int GetVolumeState();

	/** Returns the current battery level of the device in a range of [0, 100] */
	UFUNCTION(BlueprintCallable, Category = "Mobile")
	static int GetBatteryLevel();

	/** Returns the device's temperature, in Celsius */
	UFUNCTION(BlueprintCallable, Category = "Mobile")
	static float GetBatteryTemperature();

	/** Returns if headphones are plugged into the device */
	UFUNCTION(BlueprintCallable, Category = "Mobile")
	static bool AreHeadphonesPluggedIn();

};
