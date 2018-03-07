// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/Scene.h"
#include "CameraAnim.generated.h"

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogCameraAnim, Log, All);

/**
 * A predefined animation to be played on a camera
 */
UCLASS(BlueprintType, notplaceable, MinimalAPI)
class UCameraAnim : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** The UInterpGroup that holds our actual interpolation data. */
	UPROPERTY()
	class UInterpGroup* CameraInterpGroup;

#if WITH_EDITORONLY_DATA
	/** This is to preview and they only exists in editor */
	UPROPERTY(transient)
	class UInterpGroup* PreviewInterpGroup;
#endif // WITH_EDITORONLY_DATA

	/** Length, in seconds. */
	UPROPERTY()
	float AnimLength;

	/** AABB in local space. */
	UPROPERTY()
	FBox BoundingBox;

	/** 
	 * If true, assume all transform keys are intended be offsets from the start of the animation. This allows the animation to be authored at any world location and be applied as a delta to the camera. 
	 * If false, assume all transform keys are authored relative to the world origin. Positions will be directly applied as deltas to the camera.
	*/
	UPROPERTY(EditDefaultsOnly, Category=CameraAnim)
	uint8 bRelativeToInitialTransform : 1;

	/**
	* If true, assume all FOV keys are intended be offsets from the start of the animation.
	* If false, assume all FOV keys are authored relative to the current FOV of the camera at the start of the animation.
	*/
	UPROPERTY(EditDefaultsOnly, Category = CameraAnim)
	uint8 bRelativeToInitialFOV : 1;

	/** The base FOV that all FOV keys are relative to. */
	UPROPERTY(EditDefaultsOnly, Category = CameraAnim)
	float BaseFOV;

	/** Default PP settings to put on the animated camera. For modifying PP without keyframes. */
	UPROPERTY()
	FPostProcessSettings BasePostProcessSettings;

	/** Default PP blend weight to put on the animated camera. For modifying PP without keyframes. */
	UPROPERTY()
	float BasePostProcessBlendWeight;

	//~ Begin UObject Interface
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	virtual void PostLoad() override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	//~ End UObject Interface

	/** 
	 * Construct a camera animation from an InterpGroup.  The InterpGroup must control a CameraActor.  
	 * Used by the editor to "export" a camera animation from a normal Matinee scene.
	 */
	ENGINE_API bool CreateFromInterpGroup(class UInterpGroup* SrcGroup, class AMatineeActor* InMatineeActor);
	
	/** 
	 * Gets AABB of the camera's path. Useful for rough testing if you can play an animation at a certain
	 * location in the world without penetrating geometry.
	 * @return Returns the local-space axis-aligned bounding box of the entire motion of this animation. 
	 */
	ENGINE_API FBox GetAABB(FVector const& BaseLoc, FRotator const& BaseRot, float Scale) const;

protected:
	/** Internal. Computes and stores the local AABB of the camera's motion. */
	void CalcLocalAABB();
};



