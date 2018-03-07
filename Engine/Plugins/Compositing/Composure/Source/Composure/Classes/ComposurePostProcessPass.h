// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Classes/Components/SceneComponent.h"
#include "ComposurePostProcessPass.generated.h"


class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;
class USceneCaptureComponent2D;

class UComposurePostProcessBlendable;


/**
 * In engine post process based pass.
 */
UCLASS()
class COMPOSURE_API UComposurePostProcessPass
	: public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	/** 
	 * Sets a custom setup post process material. The material location must be set at BeforeTranslucency.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inputs")
	void SetSetupMaterial(UMaterialInterface* Material);
	
	/** Gets current setup material. */
	UFUNCTION(BlueprintPure, Category = "Inputs")
	UMaterialInterface* GetSetupMaterial() const
	{
		return SetupMaterial;
	}

	/** 
	 * Gets current output render target.
	 */
	UFUNCTION(BlueprintCallable, Category = "Outputs")
	UTextureRenderTarget2D* GetOutputRenderTarget() const;
	
	/** 
	 * Sets current output render target.
	 */
	UFUNCTION(BlueprintCallable, Category = "Outputs")
	void SetOutputRenderTarget(UTextureRenderTarget2D* RenderTarget);
	

	// Begins UActorComponent
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	// Ends UActorComponent


protected:
	// Underlying scene capture.
	UPROPERTY(Transient)
	USceneCaptureComponent2D* SceneCapture;

	// Blendable interface to intercept the OverrideBlendableSettings.
	UPROPERTY(Transient)
	UComposurePostProcessBlendable* BlendableInterface;

	// Setup post process material.
	UPROPERTY(Transient)
	UMaterialInterface* SetupMaterial;

	// Internal material that replace the tonemapper to output linear color space.
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* TonemapperReplacingMID;

	// Called by UComposurePostProcessBlendable::OverrideBlendableSettings.
	void OverrideBlendableSettings(class FSceneView& View, float Weight) const;


	friend class UComposurePostProcessBlendable;

};
