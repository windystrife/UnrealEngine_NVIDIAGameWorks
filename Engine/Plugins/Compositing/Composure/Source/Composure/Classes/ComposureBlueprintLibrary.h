// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Classes/Kismet/BlueprintFunctionLibrary.h"
#include "ComposurePostMoves.h"
#include "ComposureUVMap.h"
#include "ComposureBlueprintLibrary.generated.h"


UCLASS(MinimalAPI)
class UComposureBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Creates a Player Compositing Target which you can modify during gameplay. */
	UFUNCTION(BlueprintCallable, Category = "Composure", meta = (WorldContext = "WorldContextObject"))
	static class UComposurePlayerCompositingTarget* CreatePlayerCompositingTarget(UObject* WorldContextObject);

	/** 
	 * Returns a non-centered projection matrix.
	 * @param HorizontalFOVAngle The desired horizontal FOV in degrees.
	 * @param AspectRatio The desired aspect ratio.
	 */
	UFUNCTION(BlueprintPure, Category = "Composure")
	static void GetProjectionMatrixFromPostMoveSettings(
		const FComposurePostMoveSettings& PostMoveSettings, float HorizontalFOVAngle, float AspectRatio, FMatrix& ProjectionMatrix);
		
	/**
	 * Returns UV transformation matrix and its inversed to crop.
	 * @param AspectRatio The desired aspect ratio.
	 */
	UFUNCTION(BlueprintPure, Category = "Composure")
	static void GetCroppingUVTransformationMatrixFromPostMoveSettings(
		const FComposurePostMoveSettings& PostMoveSettings, float AspectRatio,
		FMatrix& CropingUVTransformationMatrix, FMatrix& UncropingUVTransformationMatrix);

	/** Sets parameters of a material that uses Composure's MF_UVMap_SampleLocation material function. */
	UFUNCTION(BlueprintCallable, Category = "Composure")
	static void SetUVMapSettingsToMaterialParameters(
		const FComposureUVMapSettings& UVMapSettings, class UMaterialInstanceDynamic* Material)
	{
		UVMapSettings.SetMaterialParameters(Material);
	}
	
	/**
	 * Converts displacement encoding parameters to decoding parameters.
	 * Can also be used to convert displacement decoding parameters to encoding parameters.
	 */
	UFUNCTION(BlueprintPure, Category = "Composure")
	static void InvertUVDisplacementMapEncodingParameters(
		const FVector2D& In, FVector2D& Out)
	{
		Out = FComposureUVMapSettings::InvertEncodingParameters(In);
	}

	/** Returns the red and green channel factors from percentage of chromatic aberration. */
	UFUNCTION(BlueprintPure, Category = "Composure")
		static void GetRedGreenUVFactorsFromChromaticAberration(float ChromaticAberrationAmount, FVector2D& RedGreenUVFactors);

	/** Returns display gamma of a given player camera manager, or 0 if no scene viewport attached. */
	UFUNCTION(BlueprintPure, Category = "Composure")
	static void GetPlayerDisplayGamma(const APlayerCameraManager* PlayerCameraManager, float& DisplayGamma);
};
