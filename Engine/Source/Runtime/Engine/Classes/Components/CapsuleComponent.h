// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/ShapeComponent.h"
#include "CapsuleComponent.generated.h"

class FPrimitiveSceneProxy;

/** 
 * A capsule generally used for simple collision. Bounds are rendered as lines in the editor.
 */
UCLASS(ClassGroup="Collision", editinlinenew, hidecategories=(Object,LOD,Lighting,TextureStreaming), meta=(DisplayName="Capsule Collision", BlueprintSpawnableComponent))
class ENGINE_API UCapsuleComponent : public UShapeComponent
{
	GENERATED_UCLASS_BODY()

protected:
	/** 
	 *	Half-height, from center of capsule to the end of top or bottom hemisphere.  
	 *	This cannot be less than CapsuleRadius.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category=Shape, meta=(ClampMin="0", UIMin="0"))
	float CapsuleHalfHeight;

	/** 
	 *	Radius of cap hemispheres and center cylinder. 
	 *	This cannot be more than CapsuleHalfHeight.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category=Shape, meta=(ClampMin="0", UIMin="0"))
	float CapsuleRadius;

protected:
	UPROPERTY()
	float CapsuleHeight_DEPRECATED;

public:
	/** 
	 * Change the capsule size. This is the unscaled size, before component scale is applied.
	 * @param	InRadius : radius of end-cap hemispheres and center cylinder.
	 * @param	InHalfHeight : half-height, from capsule center to end of top or bottom hemisphere. 
	 * @param	bUpdateOverlaps: if true and this shape is registered and collides, updates touching array for owner actor.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Capsule")
	void SetCapsuleSize(float InRadius, float InHalfHeight, bool bUpdateOverlaps=true);

	/**
	 * Set the capsule radius. This is the unscaled radius, before component scale is applied.
	 * If this capsule collides, updates touching array for owner actor.
	 * @param	Radius : radius of end-cap hemispheres and center cylinder.
	 * @param	bUpdateOverlaps: if true and this shape is registered and collides, updates touching array for owner actor.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Capsule")
	void SetCapsuleRadius(float Radius, bool bUpdateOverlaps=true);
	
	/**
	 * Set the capsule half-height. This is the unscaled half-height, before component scale is applied.
	 * If this capsule collides, updates touching array for owner actor.
	 * @param	HalfHeight : half-height, from capsule center to end of top or bottom hemisphere. 
	 * @param	bUpdateOverlaps: if true and this shape is registered and collides, updates touching array for owner actor.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Capsule")
	void SetCapsuleHalfHeight(float HalfHeight, bool bUpdateOverlaps=true);

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	//~ Begin USceneComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void CalcBoundingCylinder(float& CylinderRadius, float& CylinderHalfHeight) const override;
	//~ End USceneComponent Interface

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual bool IsZeroExtent() const override;
	virtual struct FCollisionShape GetCollisionShape(float Inflation = 0.0f) const override;
	virtual bool AreSymmetricRotations(const FQuat& A, const FQuat& B, const FVector& Scale3D) const override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UShapeComponent Interface
	virtual void UpdateBodySetup() override;
	//~ End UShapeComponent Interface

	/**
	 * Returns the capsule radius scaled by the component scale.
	 * @return The capsule radius scaled by the component scale.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Capsule")
	float GetScaledCapsuleRadius() const;

	/**
	 * Returns the capsule half-height scaled by the component scale. This includes both the cylinder and hemisphere cap.
	 * @return The capsule half-height scaled by the component scale.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Capsule")
	float GetScaledCapsuleHalfHeight() const;

	/** 
	* Returns the capsule half-height minus radius (to exclude the hemisphere), scaled by the component scale.
	* From the center of the capsule this is the vertical distance along the straight cylindrical portion to the point just before the curve of top hemisphere begins.
	* @return The capsule half-height minus radius, scaled by the component scale.
	*/
	UFUNCTION(BlueprintCallable, Category="Components|Capsule")
	float GetScaledCapsuleHalfHeight_WithoutHemisphere() const;
	
	/**
	 * Returns the capsule radius and half-height scaled by the component scale. Half-height includes the hemisphere end cap.
	 * @param OutRadius Radius of the capsule, scaled by the component scale.
	 * @param OutHalfHeight Half-height of the capsule, scaled by the component scale. Includes the hemisphere end cap.
	 * @return The capsule radius and half-height scaled by the component scale.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Capsule")
	void GetScaledCapsuleSize(float& OutRadius, float& OutHalfHeight) const;

	/**
	 * Returns the capsule radius and half-height scaled by the component scale. Half-height excludes the hemisphere end cap.
	 * @param OutRadius Radius of the capsule, ignoring component scaling.
	 * @param OutHalfHeightWithoutHemisphere Half-height of the capsule, scaled by the component scale. Excludes the hemisphere end cap.
	 * @return The capsule radius and half-height scaled by the component scale.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Capsule")
	void GetScaledCapsuleSize_WithoutHemisphere(float& OutRadius, float& OutHalfHeightWithoutHemisphere) const;

	/**
	 * Returns the capsule radius, ignoring component scaling.
	 * @return the capsule radius, ignoring component scaling.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Capsule")
	float GetUnscaledCapsuleRadius() const;

	/**
	 * Returns the capsule half-height, ignoring component scaling. This includes the hemisphere end cap.
	 * @return The capsule radius, ignoring component scaling.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Capsule")
	float GetUnscaledCapsuleHalfHeight() const;
	
	/** 
	* Returns the capsule half-height minus radius (to exclude the hemisphere), ignoring component scaling. This excludes the hemisphere end cap.
	* From the center of the capsule this is the vertical distance along the straight cylindrical portion to the point just before the curve of top hemisphere begins.
	* @return The capsule half-height minus radius, ignoring component scaling.
	*/
	UFUNCTION(BlueprintCallable, Category="Components|Capsule")
	float GetUnscaledCapsuleHalfHeight_WithoutHemisphere() const;
	
	/**
	 * Returns the capsule radius and half-height scaled by the component scale. Half-height includes the hemisphere end cap.
	 * @param OutRadius Radius of the capsule, scaled by the component scale.
	 * @param OutHalfHeight Half-height of the capsule, scaled by the component scale. Includes the hemisphere end cap.
	 * @return The capsule radius and half-height scaled by the component scale.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Capsule")
	void GetUnscaledCapsuleSize(float& OutRadius, float& OutHalfHeight) const;

	/**
	 * Returns the capsule radius and half-height, ignoring component scaling. Half-height excludes the hemisphere end cap.
	 * @param OutRadius Radius of the capsule, ignoring component scaling.
	 * @param OutHalfHeightWithoutHemisphere Half-height of the capsule, scaled by the component scale. Excludes the hemisphere end cap.
	 * @return The capsule radius and half-height (excluding hemisphere end cap), ignoring component scaling.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Capsule")
	void GetUnscaledCapsuleSize_WithoutHemisphere(float& OutRadius, float& OutHalfHeightWithoutHemisphere) const;

	/**
	 * Get the scale used by this shape. This is a uniform scale that is the minimum of any non-uniform scaling.
	 * @return the scale used by this shape.
	 */ 
	UFUNCTION(BlueprintCallable, Category="Components|Capsule")
	float GetShapeScale() const;

	// Sets the capsule size without triggering a render or physics update. This is the preferred method when initializing a component in a class constructor.
	FORCEINLINE void InitCapsuleSize(float InRadius, float InHalfHeight)
	{
		CapsuleRadius = FMath::Max(0.f, InRadius);
		CapsuleHalfHeight = FMath::Max3(0.f, InHalfHeight, InRadius);
	}
};


// ----------------- INLINES ---------------

FORCEINLINE void UCapsuleComponent::SetCapsuleRadius(float Radius, bool bUpdateOverlaps)
{
	SetCapsuleSize(Radius, GetUnscaledCapsuleHalfHeight(), bUpdateOverlaps);
}

FORCEINLINE void UCapsuleComponent::SetCapsuleHalfHeight(float HalfHeight, bool bUpdateOverlaps)
{
	SetCapsuleSize(GetUnscaledCapsuleRadius(), HalfHeight, bUpdateOverlaps);
}

FORCEINLINE float UCapsuleComponent::GetScaledCapsuleRadius() const
{
	return CapsuleRadius * GetShapeScale();
}

FORCEINLINE float UCapsuleComponent::GetScaledCapsuleHalfHeight() const
{
	return CapsuleHalfHeight * GetShapeScale();
}

FORCEINLINE float UCapsuleComponent::GetScaledCapsuleHalfHeight_WithoutHemisphere() const
{
	return (CapsuleHalfHeight - CapsuleRadius) * GetShapeScale();
}

FORCEINLINE void UCapsuleComponent::GetScaledCapsuleSize(float& OutRadius, float& OutHalfHeight) const
{
	const float Scale = GetShapeScale();
	OutRadius = CapsuleRadius * Scale;
	OutHalfHeight = CapsuleHalfHeight * Scale;
}

FORCEINLINE void UCapsuleComponent::GetScaledCapsuleSize_WithoutHemisphere(float& OutRadius, float& OutHalfHeightWithoutHemisphere) const
{
	const float Scale = GetShapeScale();
	OutRadius = CapsuleRadius * Scale;
	OutHalfHeightWithoutHemisphere = (CapsuleHalfHeight - CapsuleRadius) * Scale;
}


FORCEINLINE float UCapsuleComponent::GetUnscaledCapsuleRadius() const
{
	return CapsuleRadius;
}

FORCEINLINE float UCapsuleComponent::GetUnscaledCapsuleHalfHeight() const
{
	return CapsuleHalfHeight;
}

FORCEINLINE float UCapsuleComponent::GetUnscaledCapsuleHalfHeight_WithoutHemisphere() const
{
	return CapsuleHalfHeight - CapsuleRadius;
}

FORCEINLINE void UCapsuleComponent::GetUnscaledCapsuleSize(float& OutRadius, float& OutHalfHeight) const
{
	OutRadius = CapsuleRadius;
	OutHalfHeight = CapsuleHalfHeight;
}

FORCEINLINE void UCapsuleComponent::GetUnscaledCapsuleSize_WithoutHemisphere(float& OutRadius, float& OutHalfHeightWithoutHemisphere) const
{
	OutRadius = CapsuleRadius;
	OutHalfHeightWithoutHemisphere = CapsuleHalfHeight - CapsuleRadius;
}

FORCEINLINE float UCapsuleComponent::GetShapeScale() const
{
	return GetComponentTransform().GetMinimumAxisScale();
}

