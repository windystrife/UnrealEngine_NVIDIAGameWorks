// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "../../OculusHMD/Public/OculusHMDTypes.h"
#include "OculusHMDRuntimeSettings.generated.h"

/**
* Implements the settings for the OculusVR plugin.
*/
UCLASS(config = Engine, defaultconfig)
class OCULUSHMD_API UOculusHMDRuntimeSettings : public UObject
{
	GENERATED_UCLASS_BODY()
	
	/** Whether the Splash screen is enabled. */
	UPROPERTY(config, EditAnywhere, Category = SplashScreen)
	bool bAutoEnabled;

	/** An array of splash screen descriptors listing textures to show and their positions. */
	UPROPERTY(config, EditAnywhere, Category = SplashScreen)
	TArray<FOculusSplashDesc> SplashDescs;
};
