// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"
#include "ImagePlateComponent.generated.h"

class FPrimitiveSceneProxy;
class UImagePlateFrustumComponent;
class UMaterialInterface;
class UTexture;

USTRUCT(BlueprintType)
struct IMAGEPLATE_API FImagePlateParameters
{
	GENERATED_BODY()

	FImagePlateParameters();

	/** The material that the image plate is rendered with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Image Plate")
	UMaterialInterface* Material;

	/** Name of a texture parameter inside the material to patch the render target texture to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Image Plate")
	FName TextureParameterName;

	/** Automaticaly size the plate based on the active camera's lens and filmback settings. Target Camera is found by looking for an active camera component from this component's actor, through its attached parents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Image Plate")
	bool bFillScreen;

	/** The amount to fill the screen with when attached to a camera */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Image Plate", meta=(EditCondition="bFillScreen", AllowPreserveRatio))
	FVector2D FillScreenAmount;

	/** The fixed size of the image plate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Image Plate", meta=(EditCondition="!bFillScreen"))
	FVector2D FixedSize;

	/** Transient texture that receives image frames */
	UPROPERTY(transient, BlueprintReadOnly, Category="Image Plate")
	UTexture* RenderTexture;

	/** Transient MID to hold the material with the render texture patched in */
	UPROPERTY(transient)
	UMaterialInstanceDynamic* DynamicMaterial;
};

/** 
 * A 2d plate that will be rendered always facing the camera.
 */
UCLASS(ClassGroup=Rendering, hidecategories=(Object,Activation,Collision,"Components|Activation",Physics), editinlinenew, meta=(BlueprintSpawnableComponent))
class IMAGEPLATE_API UImagePlateComponent : public UPrimitiveComponent
{
public:

	GENERATED_BODY()
	UImagePlateComponent(const FObjectInitializer& Init);

	static inline FVector TransfromFromProjection(const FMatrix& Matrix, const FVector4& InVector)
	{
		FVector4 HomogenousVector = Matrix.TransformFVector4(InVector);
		FVector ReturnVector = HomogenousVector;
		if (HomogenousVector.W != 0.0f)
		{
			ReturnVector /= HomogenousVector.W;
		}

		return ReturnVector;
	}

	/** Add an image plate to this actor */
	UFUNCTION(BlueprintCallable, Category="Game|Image Plate")
	void SetImagePlate(FImagePlateParameters Plate);

	/** Get this actor's image plates */
	UFUNCTION(BlueprintCallable, Category="Game|Image Plate")
	FImagePlateParameters GetPlate() const
	{
		return Plate;
	}

	/** Called by sequencer if a texture is changed */
	UFUNCTION()
	void OnRenderTextureChanged();

	/** Access this component's cached view projection matrix. Only valid when the plate is set to fill screen */
	const FMatrix& GetCachedViewProjectionMatrix() const
	{
		return ViewProjectionMatrix;
	}

	/** Access this component's cached inverse view projection matrix. Only valid when the plate is set to fill screen */
	const FMatrix& GetCachedInvViewProjectionMatrix() const
	{
		return InvViewProjectionMatrix;
	}

	//~ Begin UPrimitiveComponent Interface
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual UMaterialInterface* GetMaterial(int32 Index) const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const override;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material) override;
	virtual void OnRegister() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif

#if WITH_EDITOR
	/** Access the property relating to this component's image plate */
	static UStructProperty* GetImagePlateProperty();
#endif

	/**
	 * Finds a view target that this image plate is presenting to
	 */
	AActor* FindViewTarget() const;

	/**
	 * Calculate projection matrices from a specified view target
	 */
	static void GetProjectionMatricesFromViewTarget(AActor* InViewTarget, FMatrix& OutViewProjectionMatrix, FMatrix& OutInvViewProjectionMatrix);

protected:

	void UpdateMaterialParametersForMedia();

	void UpdateTransformScale();

private:

	/** Array of image plates to render for this component */
	UPROPERTY(EditAnywhere, Category="Image Plate", DisplayName="Image Plates", meta=(ShowOnlyInnerProperties))
	FImagePlateParameters Plate;

	FMatrix ViewProjectionMatrix;
	FMatrix InvViewProjectionMatrix;

	bool bReenetrantTranformChange;

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	UImagePlateFrustumComponent* EditorFrustum;
#endif
};
