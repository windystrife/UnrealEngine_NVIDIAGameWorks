// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"
#include "MaterialBillboardComponent.generated.h"

class FPrimitiveSceneProxy;
class UCurveFloat;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FMaterialSpriteElement
{
	GENERATED_USTRUCT_BODY()

	/** The material that the sprite is rendered with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialSpriteElement)
	class UMaterialInterface* Material;
	
	/** A curve that maps distance on the X axis to the sprite opacity on the Y axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialSpriteElement)
	UCurveFloat* DistanceToOpacityCurve;
	
	/** Whether the size is defined in screen-space or world-space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialSpriteElement)
	uint32 bSizeIsInScreenSpace:1;

	/** The base width of the sprite, multiplied with the DistanceToSizeCurve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialSpriteElement)
	float BaseSizeX;

	/** The base height of the sprite, multiplied with the DistanceToSizeCurve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialSpriteElement)
	float BaseSizeY;

	/** A curve that maps distance on the X axis to the sprite size on the Y axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialSpriteElement)
	UCurveFloat* DistanceToSizeCurve;

	FMaterialSpriteElement()
		: Material(NULL)
		, DistanceToOpacityCurve(NULL)
		, bSizeIsInScreenSpace(false)
		, BaseSizeX(32)
		, BaseSizeY(32)
		, DistanceToSizeCurve(NULL)
	{
	}

	friend FArchive& operator<<(FArchive& Ar,FMaterialSpriteElement& LODElement);
};

/** 
 * A 2d material that will be rendered always facing the camera.
 */
UCLASS(ClassGroup=Rendering, collapsecategories, hidecategories=(Object,Activation,"Components|Activation",Physics,Collision,Lighting,Mesh,PhysicsVolume), editinlinenew, meta=(BlueprintSpawnableComponent))
class ENGINE_API UMaterialBillboardComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	/** Current array of material billboard elements */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Sprite)
	TArray<FMaterialSpriteElement> Elements;

	/** Set all elements of this material billboard component */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|MaterialSprite")
	void SetElements(const TArray<FMaterialSpriteElement>& NewElements);

	/** Adds an element to the sprite. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|MaterialSprite")
	void AddElement(
		class UMaterialInterface* Material,
		class UCurveFloat* DistanceToOpacityCurve,
		bool bSizeIsInScreenSpace,
		float BaseSizeX,
		float BaseSizeY,
		class UCurveFloat* DistanceToSizeCurve
		);

	//~ Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual UMaterialInterface* GetMaterial(int32 Index) const override;
	virtual void SetMaterial(int32 ElementIndex, class UMaterialInterface* Material) override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	//~ End UPrimitiveComponent Interface
};
