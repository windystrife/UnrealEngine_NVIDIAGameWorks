// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * 
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/SceneCapture.h"
#include "SceneCapture2D.generated.h"

UCLASS(hidecategories=(Collision, Material, Attachment, Actor), MinimalAPI)
class ASceneCapture2D : public ASceneCapture
{
	GENERATED_UCLASS_BODY()

private:
	/** Scene capture component. */
	UPROPERTY(Category = DecalActor, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	class USceneCaptureComponent2D* CaptureComponent2D;
	/** To allow drawing the camera frustum in the editor. */
	UPROPERTY()
	class UDrawFrustumComponent* DrawFrustum;

public:

	//~ Begin AActor Interface
	ENGINE_API virtual void PostActorCreated() override;
	//~ End AActor Interface.

	/** Used to synchronize the DrawFrustumComponent with the SceneCaptureComponent2D settings. */
	void UpdateDrawFrustum();

	UFUNCTION(BlueprintCallable, Category="Rendering")
	void OnInterpToggle(bool bEnable);

	/** Returns CaptureComponent2D subobject **/
	ENGINE_API class USceneCaptureComponent2D* GetCaptureComponent2D() const { return CaptureComponent2D; }
	/** Returns DrawFrustum subobject **/
	ENGINE_API class UDrawFrustumComponent* GetDrawFrustum() const { return DrawFrustum; }
};



