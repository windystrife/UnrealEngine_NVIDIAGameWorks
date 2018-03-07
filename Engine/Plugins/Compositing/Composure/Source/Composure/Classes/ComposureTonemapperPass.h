// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Classes/Engine/Scene.h"
#include "ComposurePostProcessPass.h"
#include "ComposureTonemapperPass.generated.h"


/**
 * Tonemapper only pass implemented on top of the in-engine tonemapper.
 */
UCLASS(hidecategories = (Collision, Object, Physics, SceneComponent, Transform), ClassGroup = "Composure", editinlinenew, meta = (BlueprintSpawnableComponent))
class COMPOSURE_API UComposureTonemapperPass : public UComposurePostProcessPass
{
	GENERATED_UCLASS_BODY()

public:

	/** Color grading settings. */
	UPROPERTY(Interp, BlueprintReadWrite, Category = "Settings")
	FColorGradingSettings ColorGradingSettings;
	
	/** Film stock settings. */
	UPROPERTY(Interp, BlueprintReadWrite, Category = "Settings")
	FFilmStockSettings FilmStockSettings;

	/** in percent, Scene chromatic aberration / color fringe (camera imperfection) to simulate an artifact that happens in real-world lens, mostly visible in the image corners. */
	UPROPERTY(Interp, BlueprintReadWrite, Category = "Lens", meta = (UIMin = "0.0", UIMax = "5.0"))
	float ChromaticAberration;

	
	/** 
	 * Tone map the input into the output.
	 */
	UFUNCTION(BlueprintCallable, Category = "Outputs")
	void TonemapToRenderTarget();
};
