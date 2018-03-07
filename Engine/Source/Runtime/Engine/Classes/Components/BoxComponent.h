// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/ShapeComponent.h"
#include "BoxComponent.generated.h"

class FPrimitiveSceneProxy;

/** 
 * A box generally used for simple collision. Bounds are rendered as lines in the editor.
 */
UCLASS(ClassGroup="Collision", hidecategories=(Object,LOD,Lighting,TextureStreaming), editinlinenew, meta=(DisplayName="Box Collision", BlueprintSpawnableComponent))
class ENGINE_API UBoxComponent : public UShapeComponent
{
	GENERATED_UCLASS_BODY()

protected:
	/** The extents (radii dimensions) of the box **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category=Shape)
	FVector BoxExtent;

public:
	/** 
	 * Change the box extent size. This is the unscaled size, before component scale is applied.
	 * @param	InBoxExtent: new extent (radius) for the box.
	 * @param	bUpdateOverlaps: if true and this shape is registered and collides, updates touching array for owner actor.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Box")
	void SetBoxExtent(FVector InBoxExtent, bool bUpdateOverlaps=true);

	// @return the box extent, scaled by the component scale.
	UFUNCTION(BlueprintCallable, Category="Components|Box")
	FVector GetScaledBoxExtent() const;

	// @return the box extent, ignoring component scale.
	UFUNCTION(BlueprintCallable, Category="Components|Box")
	FVector GetUnscaledBoxExtent() const;

	//~ Begin UPrimitiveComponent Interface.
	virtual bool IsZeroExtent() const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual struct FCollisionShape GetCollisionShape(float Inflation = 0.0f) const override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin USceneComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface

	//~ Begin UShapeComponent Interface
	virtual void UpdateBodySetup() override;
	//~ End UShapeComponent Interface

	// Sets the box extents without triggering a render or physics update.
	FORCEINLINE void InitBoxExtent(const FVector& InBoxExtent) { BoxExtent = InBoxExtent; }
};


// ----------------- INLINES ---------------

FORCEINLINE FVector UBoxComponent::GetScaledBoxExtent() const
{
	return BoxExtent * GetComponentTransform().GetScale3D();
}

FORCEINLINE FVector UBoxComponent::GetUnscaledBoxExtent() const
{
	return BoxExtent;
}
