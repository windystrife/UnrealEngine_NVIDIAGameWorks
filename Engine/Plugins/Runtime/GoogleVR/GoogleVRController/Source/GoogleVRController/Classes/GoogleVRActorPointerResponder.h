// Copyright 2017 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "UObject/Interface.h"
#include "Engine/EngineTypes.h"
#include "GoogleVRActorPointerResponder.generated.h"

class UGoogleVRPointerInputComponent;

/**
 * IGoogleVRActorPointerResponder is an interface for an Actor
 * to respond to pointer input events from UGoogleVRPointerInputComponent.
 */
UINTERFACE(BlueprintType)
class UGoogleVRActorPointerResponder : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class GOOGLEVRCONTROLLER_API IGoogleVRActorPointerResponder
{
	GENERATED_IINTERFACE_BODY()

public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="PointerResponder")
	void OnPointerEnter(const FHitResult& HitResult, UGoogleVRPointerInputComponent* Source);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="PointerResponder")
	void OnPointerExit(AActor* PreviousActor, const FHitResult& HitResult, UGoogleVRPointerInputComponent* Source);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="PointerResponder")
	void OnPointerHover(const FHitResult& HitResult, UGoogleVRPointerInputComponent* Source);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="PointerResponder")
	void OnPointerComponentChanged(UPrimitiveComponent* PreviousComponent, const FHitResult& HitResult, UGoogleVRPointerInputComponent* Source);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="PointerResponder")
	void OnPointerClick(const FHitResult& HitResult, UGoogleVRPointerInputComponent* Source);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="PointerResponder")
	void OnPointerPressed(const FHitResult& HitResult, UGoogleVRPointerInputComponent* Source);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="PointerResponder")
	void OnPointerReleased(const FHitResult& HitResult, UGoogleVRPointerInputComponent* Source);
};
