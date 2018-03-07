// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StereoLayerFunctionLibrary.generated.h"

/**
 * StereoLayer Extensions Function Library
 */
UCLASS()
class ENGINE_API UStereoLayerFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	/**
	* Set splash screen attributes
	*
	* @param Texture			(in) A texture to be used for the splash. B8R8G8A8 format.
	* @param Scale				(in) Scale of the texture.
	* @param Offset				(in) Position from which to start rendering the texture.
	* @param ShowLoadingMovie	(in) Whether the splash screen presents loading movies.
	*/
	UFUNCTION(BlueprintCallable, Category = "VR")
		static void SetSplashScreen(class UTexture* Texture, FVector2D Scale = FVector2D(1.0f, 1.0f), FVector2D Offset = FVector2D(0.0f, 0.0f), bool bShowLoadingMovie = false, bool bShowOnSet = false);

	/**
	* Show the splash screen and override the VR display
	*/
	UFUNCTION(BlueprintCallable, Category = "VR")
		static void ShowSplashScreen();

	/**
	* Hide the splash screen and return to normal display.
	*/
	UFUNCTION(BlueprintCallable, Category = "VR")
		static void HideSplashScreen();

	/**
	 * Enables/disables splash screen to be automatically shown when LoadMap is called.
	 *
	 * @param	bAutoShowEnabled	(in)	True, if automatic showing of splash screens is enabled when map is being loaded.
	 */
	UFUNCTION(BlueprintCallable, Category = "VR")
		static void EnableAutoLoadingSplashScreen(bool InAutoShowEnabled);
};
