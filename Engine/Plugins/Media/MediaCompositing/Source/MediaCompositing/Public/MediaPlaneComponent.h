// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"
#include "MediaPlaneComponent.generated.h"

class FPrimitiveSceneProxy;
class UMediaPlaneFrustumComponent;
class UMaterialInterface;
class UTexture;

USTRUCT(BlueprintType)
struct MEDIACOMPOSITING_API FMediaPlaneParameters
{
	GENERATED_BODY()

	FMediaPlaneParameters();

	/** The material that the media plane is rendered with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Media Plane")
	UMaterialInterface* Material;

	/** Name of a texture parameter inside the material to patch the render target texture to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Media Plane")
	FName TextureParameterName;

	/** Automaticaly size the plane based on the active camera's lens and filmback settings. Target Camera is found by looking for an active camera component from this component's actor, through its attached parents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Media Plane")
	bool bFillScreen;

	/** The amount to fill the screen with when attached to a camera */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Media Plane", meta=(EditCondition="bFillScreen", AllowPreserveRatio))
	FVector2D FillScreenAmount;

	/** The fixed size of the media plane */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Media Plane", meta=(EditCondition="!bFillScreen"))
	FVector2D FixedSize;

	/** Transient texture that receives image frames */
	UPROPERTY(transient, BlueprintReadOnly, Category="Media Plane")
	UTexture* RenderTexture;

	/** Transient MID to hold the material with the render texture patched in */
	UPROPERTY(transient)
	UMaterialInstanceDynamic* DynamicMaterial;
};

/** 
 * A 2d plane that will be rendered always facing the camera.
 */
UCLASS(ClassGroup=Rendering, hidecategories=(Object,Activation,Collision,"Components|Activation",Physics), editinlinenew, meta=(BlueprintSpawnableComponent))
class MEDIACOMPOSITING_API UMediaPlaneComponent : public UPrimitiveComponent
{
public:

	GENERATED_BODY()
	UMediaPlaneComponent(const FObjectInitializer& Init);

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

	/** Add an media mlane to this actor */
	UFUNCTION(BlueprintCallable, Category="Game|Media Plane")
	void SetMediaPlane(FMediaPlaneParameters Plane);

	/** Get this actor's media planes */
	UFUNCTION(BlueprintCallable, Category="Game|Media Plane")
	FMediaPlaneParameters GetPlane() const
	{
		return Plane;
	}

	/** Called by sequencer if a texture is changed */
	UFUNCTION()
	void OnRenderTextureChanged();

	/** Access this component's cached view projection matrix. Only valid when the plane is set to fill screen */
	const FMatrix& GetCachedViewProjectionMatrix() const
	{
		return ViewProjectionMatrix;
	}

	/** Access this component's cached inverse view projection matrix. Only valid when the plane is set to fill screen */
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
	/** Access the property relating to this component's Media Plane */
	static UStructProperty* GetMediaPlaneProperty();
#endif

	/**
	 * Finds a view target that this Media Plane is presenting to
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

	/** Array of Media Planes to render for this component */
	UPROPERTY(EditAnywhere, Category="Media Plane", DisplayName="Media Planes", meta=(ShowOnlyInnerProperties))
	FMediaPlaneParameters Plane;

	FMatrix ViewProjectionMatrix;
	FMatrix InvViewProjectionMatrix;

	bool bReenetrantTranformChange;

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	UMediaPlaneFrustumComponent* EditorFrustum;
#endif
};
