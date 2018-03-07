// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "FlexFluidSurface.generated.h"

UCLASS(hidecategories = Object, MinimalAPI, BlueprintType)
class UFlexFluidSurface : public UObject
{
	GENERATED_UCLASS_BODY()

	/* Smoothing radius in world space. Smoothing is skipped with a value of 0.0*/
	UPROPERTY(EditAnywhere, Category = Flex, meta = (DisplayName = "Smoothing Radius", UIMin = "0.0", ClampMin = "0.0"))
	float SmoothingRadius;

	/* Limitation for number of samples used for smoothing (expressed as radius in pixel space). Smoothing is skipped with a value of 1*/
	UPROPERTY(EditAnywhere, Category = Flex, meta = (DisplayName = "Max Radial Samples", UIMin = "1", ClampMin = "1"))
	int32 MaxRadialSamples;

	UPROPERTY(EditAnywhere, Category = Flex, meta = (DisplayName = "Depth Edge Falloff", UIMin = "0.0", ClampMin = "0.0"))
	float DepthEdgeFalloff;

	/* Relative scale applied to particles for thickness rendering. Higher values result in smoother thickness, but can reduce definition.
	A value of 0.0 disables thickness rendering. Default is 2.0*/
	UPROPERTY(EditAnywhere, Category = Flex, meta = (DisplayName = "Thickness Particle Scale", UIMin = "0.0", ClampMin = "0.0"))
	float ThicknessParticleScale;

	/* Relative scale applied to particles for depth rendering. Default is 1.0*/
	UPROPERTY(EditAnywhere, Category = Flex, meta = (DisplayName = "Depth Particle Scale", UIMin = "0.0", ClampMin = "0.0"))
	float DepthParticleScale;

	/* Compute the fluid surface in half resolution to improve performance. */
	UPROPERTY(EditAnywhere, Category = Flex, meta = (DisplayName = "Half Resolution"))
	bool HalfRes;

	/** Enables shadowing from static geometry*/
	UPROPERTY(EditAnywhere, Category = Flex)
	bool ReceiveShadows;

	/* Material used to render the surface*/
	UPROPERTY(EditAnywhere, Category = Flex)
	UMaterialInterface* Material;

public:
	// Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject Interface
};



