// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/SceneCaptureComponent.h"
#include "SceneCaptureComponentCube.generated.h"

class FSceneInterface;

/**
 *	Used to capture a 'snapshot' of the scene from a 6 planes and feed it to a render target.
 */
UCLASS(hidecategories = (Collision, Object, Physics, SceneComponent), ClassGroup=Rendering, editinlinenew, meta = (BlueprintSpawnableComponent))
class ENGINE_API USceneCaptureComponentCube : public USceneCaptureComponent
{
	GENERATED_UCLASS_BODY()

	/** Temporary render target that can be used by the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture)
	class UTextureRenderTargetCube* TextureTarget;

public:
	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void SendRenderTransform_Concurrent() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	/** Render the scene to the texture the next time the main view is rendered. */
	void CaptureSceneDeferred();

	/** 
	 * Render the scene to the texture target immediately.  
	 * This should not be used if bCaptureEveryFrame is enabled, or the scene capture will render redundantly. 
	 */
	UFUNCTION(BlueprintCallable,Category = "Rendering|SceneCapture")
	void CaptureScene();

	// For backwards compatibility
	void UpdateContent() { CaptureSceneDeferred(); }

	void UpdateSceneCaptureContents(FSceneInterface* Scene) override;
};
