// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "Light.generated.h"

class UMaterialInterface;

UCLASS(Abstract, ClassGroup=Lights, hideCategories=(Input,Collision,Replication), showCategories=("Input|MouseInput", "Input|TouchInput"), ComponentWrapperClass, ConversionRoot, meta=(ChildCanTick))
class ENGINE_API ALight : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	/** @todo document */
	UPROPERTY(Category = Light, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Light,Rendering,Rendering|Components|Light", AllowPrivateAccess = "true"))
	class ULightComponent* LightComponent;
public:

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	/** replicated copy of LightComponent's bEnabled property */
	UPROPERTY(replicatedUsing=OnRep_bEnabled)
	uint32 bEnabled:1;

	/** Replication Notification Callbacks */
	UFUNCTION()
	virtual void OnRep_bEnabled();

	/** Function to change mobility type of light */
	void SetMobility(EComponentMobility::Type InMobility);

	// BEGIN DEPRECATED (use component functions now in level script)
	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting", meta=(DeprecatedFunction))
	void SetEnabled(bool bSetEnabled);
	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting", meta=(DeprecatedFunction))
	bool IsEnabled() const;
	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting", meta=(DeprecatedFunction))
	void ToggleEnabled();
	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting", meta=(DeprecatedFunction))
	void SetBrightness(float NewBrightness);
	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting", meta=(DeprecatedFunction))
	float GetBrightness() const;
	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting", meta=(DeprecatedFunction))
	void SetLightColor(FLinearColor NewLightColor);
	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting", meta=(DeprecatedFunction))
	FLinearColor GetLightColor() const;
	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting", meta=(DeprecatedFunction))
	void SetLightFunctionMaterial(UMaterialInterface* NewLightFunctionMaterial);
	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting", meta=(DeprecatedFunction))
	void SetLightFunctionScale(FVector NewLightFunctionScale);
	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting", meta=(DeprecatedFunction))
	void SetLightFunctionFadeDistance(float NewLightFunctionFadeDistance);
	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting", meta=(DeprecatedFunction))
	void SetCastShadows(bool bNewValue);
	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting", meta=(DeprecatedFunction))
	void SetAffectTranslucentLighting(bool bNewValue);
	// END DEPRECATED

public:
#if WITH_EDITOR
	//~ Begin AActor Interface.
	virtual void CheckForErrors() override;
	//~ End AActor Interface.
#endif

	/**
	 * Return whether the light supports being toggled off and on on-the-fly.
	 */
	bool IsToggleable() const;

	//~ Begin AActor Interface.
	void Destroyed() override;
	virtual bool IsLevelBoundsRelevant() const override { return false; }
	//~ End AActor Interface.

	/** Returns LightComponent subobject **/
	class ULightComponent* GetLightComponent() const { return LightComponent; }
};



