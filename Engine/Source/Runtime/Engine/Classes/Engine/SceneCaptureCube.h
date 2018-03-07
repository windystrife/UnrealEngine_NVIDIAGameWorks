// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * 
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/SceneCapture.h"
#include "SceneCaptureCube.generated.h"

UCLASS(hidecategories = (Collision, Material, Attachment, Actor))
class ENGINE_API ASceneCaptureCube : public ASceneCapture
{
	GENERATED_UCLASS_BODY()

private:
	/** Scene capture component. */
	UPROPERTY(Category = DecalActor, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	class USceneCaptureComponentCube* CaptureComponentCube;

	/** To allow drawing the camera frustum in the editor. */
	UPROPERTY()
	class UDrawFrustumComponent* DrawFrustum;

public:

	//~ Begin AActor Interface
	virtual void PostActorCreated() override;
#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
#endif
	//~ End AActor Interface.

	/** Used to synchronize the DrawFrustumComponent with the SceneCaptureComponentCube settings. */
	void UpdateDrawFrustum();

	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void OnInterpToggle(bool bEnable);

	/** Returns CaptureComponentCube subobject **/
	class USceneCaptureComponentCube* GetCaptureComponentCube() const { return CaptureComponentCube; }
	/** Returns DrawFrustum subobject **/
	class UDrawFrustumComponent* GetDrawFrustum() const { return DrawFrustum; }
};



