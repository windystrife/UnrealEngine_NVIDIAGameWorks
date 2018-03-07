// Copyright 2017 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "UObject/Interface.h"
#include "GoogleVRComponentPointerResponder.generated.h"

class UGoogleVRPointerInputComponent;

/**
 * IGoogleVRComponentPointerResponder is an interface for a Component
 * to respond to pointer input events from UGoogleVRPointerInputComponent.
 */
UINTERFACE(BlueprintType)
class UGoogleVRComponentPointerResponder : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class GOOGLEVRCONTROLLER_API IGoogleVRComponentPointerResponder
{
	GENERATED_IINTERFACE_BODY()

public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="PointerResponder")
	void OnPointerEnter(const FHitResult& HitResult, UGoogleVRPointerInputComponent* Source);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="PointerResponder")
	void OnPointerExit(UPrimitiveComponent* PreviousComponent, const FHitResult& HitResult, UGoogleVRPointerInputComponent* Source);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="PointerResponder")
	void OnPointerHover(const FHitResult& HitResult, UGoogleVRPointerInputComponent* Source);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="PointerResponder")
	void OnPointerClick(const FHitResult& HitResult, UGoogleVRPointerInputComponent* Source);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="PointerResponder")
	void OnPointerPressed(const FHitResult& HitResult, UGoogleVRPointerInputComponent* Source);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="PointerResponder")
	void OnPointerReleased(const FHitResult& HitResult, UGoogleVRPointerInputComponent* Source);
};
