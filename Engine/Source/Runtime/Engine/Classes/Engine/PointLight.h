// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Light.h"
#include "PointLight.generated.h"

UCLASS(ClassGroup=(Lights, PointLights), ComponentWrapperClass, MinimalAPI, meta=(ChildCanTick))
class APointLight : public ALight
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Light", meta=(ExposeFunctionCategories="PointLight,Rendering|Lighting"))
	class UPointLightComponent* PointLightComponent;

	// BEGIN DEPRECATED (use component functions now in level script)
	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting", meta=(DeprecatedFunction))
	ENGINE_API void SetRadius(float NewRadius);
	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting", meta=(DeprecatedFunction))
	ENGINE_API void SetLightFalloffExponent(float NewLightFalloffExponent);
	// END DEPRECATED

#if WITH_EDITOR
	//~ Begin AActor Interface.
	virtual void EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;
	//~ End AActor Interface.
#endif

	//~ Begin UObject Interface.
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void LoadedFromAnotherClass(const FName& OldClassName) override;
#endif
	//~ End UObject Interface.
};



