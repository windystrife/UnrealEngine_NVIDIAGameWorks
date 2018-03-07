// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptInterface.h"
#include "Engine/BlendableInterface.h"
#include "Camera/CameraTypes.h"
#include "Components/SceneCaptureComponent.h"
#include "SceneCaptureComponent2D.generated.h"

class FSceneInterface;

/**
 *	Used to capture a 'snapshot' of the scene from a single plane and feed it to a render target.
 */
UCLASS(hidecategories=(Collision, Object, Physics, SceneComponent), ClassGroup=Rendering, editinlinenew, meta=(BlueprintSpawnableComponent))
class ENGINE_API USceneCaptureComponent2D : public USceneCaptureComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projection, meta=(DisplayName = "Projection Type"))
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionType;

	/** Camera field of view (in degrees). */
	UPROPERTY(interp, EditAnywhere, BlueprintReadWrite, Category=Projection, meta=(DisplayName = "Field of View", UIMin = "5.0", UIMax = "170", ClampMin = "0.001", ClampMax = "360.0"))
	float FOVAngle;

	/** The desired width (in world units) of the orthographic view (ignored in Perspective mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projection)
	float OrthoWidth;

	/** Output render target of the scene capture that can be read in materals. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
	class UTextureRenderTarget2D* TextureTarget;

	UPROPERTY(interp, Category=SceneCapture, meta=(DisplayName = "Capture Source"))
	TEnumAsByte<enum ESceneCaptureSource> CaptureSource;

	/** When enabled, the scene capture will composite into the render target instead of overwriting its contents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
	TEnumAsByte<enum ESceneCaptureCompositeMode> CompositeMode;

	UPROPERTY(interp, Category=PostProcessVolume, meta=(ShowOnlyInnerProperties))
	struct FPostProcessSettings PostProcessSettings;

	/** Range (0.0, 1.0) where 0 indicates no effect, 1 indicates full effect. */
	UPROPERTY(interp, Category=PostProcessVolume, BlueprintReadWrite, meta=(UIMin = "0.0", UIMax = "1.0"))
	float PostProcessBlendWeight;

	/** Whether a custom projection matrix will be used during rendering. Use with caution. Does not currently affect culling */
	UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category = Projection)
	bool bUseCustomProjectionMatrix;

	/** The custom projection matrix to use */
	UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category = Projection)
	FMatrix CustomProjectionMatrix;

	/** 
	 * Enables a clip plane while rendering the scene capture which is useful for portals.  
	 * The global clip plane must be enabled in the renderer project settings for this to work.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=SceneCapture)
	bool bEnableClipPlane;

	/** Base position for the clip plane, can be any position on the plane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=SceneCapture)
	FVector ClipPlaneBase;

	/** Normal for the plane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=SceneCapture)
	FVector ClipPlaneNormal;
	
	/** 
	 * True if we did a camera cut this frame. Automatically reset to false at every capture.
	 * This flag affects various things in the renderer (such as whether to use the occlusion queries from last frame, and motion blur).
	 * Similar to UPlayerCameraManager::bGameCameraCutThisFrame.
	 */
	UPROPERTY(Transient, BlueprintReadWrite, Category = SceneCapture)
	uint32 bCameraCutThisFrame : 1;


	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void SendRenderTransform_Concurrent() override;
	virtual bool RequiresGameThreadEndOfFrameUpdates() const override
	{
		// this method could probably be removed allowing them to run on any thread, but it isn't worth the trouble
		return true;
	}
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	//~ End UActorComponent Interface

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	
	virtual void Serialize(FArchive& Ar);

	//~ End UObject Interface

	/** Adds an Blendable (implements IBlendableInterface) to the array of Blendables (if it doesn't exist) and update the weight */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	void AddOrUpdateBlendable(TScriptInterface<IBlendableInterface> InBlendableObject, float InWeight = 1.0f) { PostProcessSettings.AddBlendable(InBlendableObject, InWeight); }

	/** Render the scene to the texture the next time the main view is rendered. */
	void CaptureSceneDeferred();

	// For backwards compatibility
	void UpdateContent() { CaptureSceneDeferred(); }

	/** 
	 * Render the scene to the texture target immediately.  
	 * This should not be used if bCaptureEveryFrame is enabled, or the scene capture will render redundantly. 
	 */
	UFUNCTION(BlueprintCallable,Category = "Rendering|SceneCapture")
	void CaptureScene();

	void UpdateSceneCaptureContents(FSceneInterface* Scene) override;
};
