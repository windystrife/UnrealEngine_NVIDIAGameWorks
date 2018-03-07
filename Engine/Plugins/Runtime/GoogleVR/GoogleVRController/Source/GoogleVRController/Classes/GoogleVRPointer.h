// Copyright 2017 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "Engine/EngineTypes.h"
#include "GoogleVRPointer.generated.h"

/**
 * IGoogleVRPointer is an interface for pointer based input used with UGoogleVRPointerInputComponent.
 */
UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class GOOGLEVRCONTROLLER_API UGoogleVRPointer : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class GOOGLEVRCONTROLLER_API IGoogleVRPointer
{
	GENERATED_IINTERFACE_BODY()

public:

	/** Called when the pointer begins hitting an actor. */
	virtual void OnPointerEnter(const FHitResult& HitResult, bool IsHitInteractive) = 0;

	/** Called every frame the pointer is pointing at an actor. */
	virtual void OnPointerHover(const FHitResult& HitResult, bool IsHitInteractive) = 0;

	/** Called when the pointer stops hitting an actor. */
	virtual void OnPointerExit(const FHitResult& HitResult) = 0;

	/** Returns the origin of the ray in world space. */
	virtual FVector GetOrigin() const = 0;

	/** Returns the normalized direction of the ray in world space. */
	virtual FVector GetDirection() const = 0;

	/** Return the radius of the ray. It is used by GoogleVRPointerInputComponent
	 *  when searching for valid targets. If a radius is 0, then
	 *  a ray is used to find a valid target. Otherwise it will use a sphere trace.
	 *  The *EnterRadius* is used for finding new targets while the *ExitRadius*
	 *  is used to see if you are still nearby the object currently selected
	 *  to avoid a flickering effect when just at the border of the intersection.
	 */
	virtual void GetRadius(float& OutEnterRadius, float& OutExitRadius) const = 0;

	/** Returns the max distance this ray will be rendered at from the camera.
	 *  This is used by GoogleVRPointerInputComponent to calculate the ray when using
	 *  the default "Camera" EGoogleVRPointerInputMode. See GoogleVRPointerInputComponent.h for details.
	 */
	virtual float GetMaxPointerDistance() const = 0;

	/** Returns true if the pointer is active. 
	 *  If the pointer is inactive, then the UGoogleVRPointerInputComponent won't perform hit detection.
	 */
	virtual bool IsPointerActive() const = 0;
};
