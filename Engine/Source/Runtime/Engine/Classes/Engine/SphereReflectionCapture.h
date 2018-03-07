// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/ReflectionCapture.h"
#include "SphereReflectionCapture.generated.h"

class UDrawSphereComponent;

/** 
 *	Actor used to capture the scene for reflection in a sphere shape.
 *	@see https://docs.unrealengine.com/latest/INT/Resources/ContentExamples/Reflections/1_4
 */
UCLASS(hidecategories = (Collision, Attachment, Actor), MinimalAPI)
class ASphereReflectionCapture : public AReflectionCapture
{
	GENERATED_UCLASS_BODY()

private:
	/** Sphere component used to visualize the capture radius */
	UPROPERTY()
	UDrawSphereComponent* DrawCaptureRadius;

public:

#if WITH_EDITOR
	//~ Begin AActor Interface.
	virtual void EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;
	//~ End AActor Interface.
#endif

	/** Returns DrawCaptureRadius subobject **/
	ENGINE_API UDrawSphereComponent* GetDrawCaptureRadius() const { return DrawCaptureRadius; }
};



