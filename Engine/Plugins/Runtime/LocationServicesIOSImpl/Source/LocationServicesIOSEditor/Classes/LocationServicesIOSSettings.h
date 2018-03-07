// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "LocationServicesIOSSettings.generated.h"

UCLASS(config = Engine, defaultconfig)
class ULocationServicesIOSSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()
	
	/* Text to display when requesting permission for Location Services */
    UPROPERTY(config, EditAnywhere, Category = LocationServices, meta = (DisplayName = "Location Services Always Use Permission Text" ))
    FString LocationAlwaysUsageDescription;
    
    /* Text to display when requesting permission for Location Services */
    UPROPERTY(config, EditAnywhere, Category = LocationServices, meta = (DisplayName = "Location Services In-Use Permission Text" ))
    FString LocationWhenInUseDescription;
};
