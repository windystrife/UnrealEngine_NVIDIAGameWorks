// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PointLightComponent.h"
#include "SpotLightComponent.generated.h"

class FLightSceneProxy;

// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
UENUM()
namespace EFalloffMode
{
	enum Type
	{
		NONE,
		FIXED,
		CUSTOM,
	};
}
// NVCHANGE_END: Nvidia Volumetric Lighting


/**
 * A spot light component emits a directional cone shaped light (Eg a Torch).
 */
UCLASS(Blueprintable, ClassGroup=Lights, hidecategories=Object, editinlinenew, meta=(BlueprintSpawnableComponent))
class ENGINE_API USpotLightComponent : public UPointLightComponent
{
	GENERATED_UCLASS_BODY()

	/** Degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, meta=(UIMin = "1.0", UIMax = "80.0"))
	float InnerConeAngle;

	/** Degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, meta=(UIMin = "1.0", UIMax = "80.0"))
	float OuterConeAngle;

	/** Degrees. */
	UPROPERTY(/*EditAnywhere, BlueprintReadOnly, Category=LightShaft, meta=(UIMin = "1.0", UIMax = "180.0")*/)
	float LightShaftConeAngle;

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetInnerConeAngle(float NewInnerConeAngle);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetOuterConeAngle(float NewOuterConeAngle);

	// Disable for now
	//UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	//void SetLightShaftConeAngle(float NewLightShaftConeAngle);


	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
	/** Equation to use for angular falloff */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = NvidiaVolumetricLighting)
		TEnumAsByte<EFalloffMode::Type> FalloffMode;

	/** falloff angle (Degrees.) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = NvidiaVolumetricLighting, meta = (UIMin = "1.0", UIMax = "80.0"))
		float FalloffAngle;

	/** falloff power */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = NvidiaVolumetricLighting)
		float FalloffPower;
	// NVCHANGE_END: Nvidia Volumetric Lighting

	// ULightComponent interface.
	virtual FSphere GetBoundingSphere() const override;
	virtual bool AffectsBounds(const FBoxSphereBounds& InBounds) const override;
	virtual ELightComponentType GetLightType() const override;
	virtual FLightSceneProxy* CreateSceneProxy() const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
	virtual bool CanEditChange(const UProperty* InProperty) const override;
	// NVCHANGE_END: Nvidia Volumetric Lighting
#endif

	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	virtual void GetNvVlFalloff(int32& OutFalloffMode, FVector2D& OutFalloffAngleAndPower) const override
	{
		OutFalloffMode = FalloffMode;
		OutFalloffAngleAndPower = FVector2D(FalloffAngle * (float)PI / 180.0f, FalloffPower);
	}

#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting
};



