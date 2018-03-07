// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Classes/Engine/Scene.h"
#include "ComposurePostProcessPass.h"
#include "ComposureLensBloomPass.generated.h"


/**
 * Bloom only pass implemented on top of the in-engine bloom.
 */
UCLASS(hidecategories = (Collision, Object, Physics, SceneComponent, Transform), ClassGroup = "Composure", editinlinenew, meta = (BlueprintSpawnableComponent))
class COMPOSURE_API UComposureLensBloomPass : public UComposurePostProcessPass
{
	GENERATED_UCLASS_BODY()

public:

	/** Bloom settings. */
	UPROPERTY(Interp, BlueprintReadWrite, Category = "Lens Bloom Settings")
	FLensBloomSettings Settings;
	

	/** Sets a custom tonemapper replacing material instance. */
	UFUNCTION(BlueprintCallable, Category = "Lens Bloom Settings")
	void SetTonemapperReplacingMaterial(UMaterialInstanceDynamic* Material);

	
	/** 
	 * Blurs the input into the output.
	 */
	UFUNCTION(BlueprintCallable, Category = "Outputs")
	void BloomToRenderTarget();
};
