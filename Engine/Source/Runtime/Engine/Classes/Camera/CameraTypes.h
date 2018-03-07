// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "EngineDefines.h"
#include "Engine/Scene.h"
#include "CameraTypes.generated.h"

//@TODO: Document
UENUM()
namespace ECameraProjectionMode
{
	enum Type
	{
		Perspective,
		Orthographic
	};
}

UENUM()
namespace ECameraAnimPlaySpace
{
	enum Type
	{
 		/** This anim is applied in camera space. */
 		CameraLocal,
 		/** This anim is applied in world space. */
 		World,
 		/** This anim is applied in a user-specified space (defined by UserPlaySpaceMatrix). */
 		UserDefined,
 	};
}

USTRUCT(BlueprintType)
struct FMinimalViewInfo
{
	GENERATED_USTRUCT_BODY()

	/** Location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Camera)
	FVector Location;

	/** Rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Camera)
	FRotator Rotation;

	/** The field of view (in degrees) in perspective mode (ignored in Orthographic mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Camera)
	float FOV;

	/** The desired width (in world units) of the orthographic view (ignored in Perspective mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Camera)
	float OrthoWidth;

	/** The near plane distance of the orthographic view (in world units) */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category=Camera)
	float OrthoNearClipPlane;

	/** The far plane distance of the orthographic view (in world units) */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category=Camera)
	float OrthoFarClipPlane;

	// Aspect Ratio (Width/Height); ignored unless bConstrainAspectRatio is true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Camera)
	float AspectRatio;

	// If bConstrainAspectRatio is true, black bars will be added if the destination view has a different aspect ratio than this camera requested.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Camera)
	uint32 bConstrainAspectRatio:1; 

	// If true, account for the field of view angle when computing which level of detail to use for meshes.
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=CameraSettings)
	uint32 bUseFieldOfViewForLOD:1;

	// The type of camera
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Camera)
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionMode;

	/** Indicates if PostProcessSettings should be applied. */
	UPROPERTY(BlueprintReadWrite, Category = Camera)
	float PostProcessBlendWeight;

	/** Post-process settings to use if PostProcessBlendWeight is non-zero. */
	UPROPERTY(BlueprintReadWrite, Category = Camera)
	struct FPostProcessSettings PostProcessSettings;

	/** Off-axis / off-center projection offset as proportion of screen dimensions */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadWrite, Category = Camera)
	FVector2D OffCenterProjectionOffset;

	FMinimalViewInfo()
		: Location(ForceInit)
		, Rotation(ForceInit)
		, FOV(90.0f)
		, OrthoWidth(512.0f)
		, OrthoNearClipPlane(0.0f)
		, OrthoFarClipPlane(WORLD_MAX)
		, AspectRatio(1.33333333f)
		, bConstrainAspectRatio(false)
		, bUseFieldOfViewForLOD(true)
		, ProjectionMode(ECameraProjectionMode::Perspective)
		, PostProcessBlendWeight(0.0f)
		, OffCenterProjectionOffset(ForceInitToZero)
	{
	}

	// Is this equivalent to the other one?
	ENGINE_API bool Equals(const FMinimalViewInfo& OtherInfo) const;

	// Blends view information
	// Note: booleans are orred together, instead of blending
	ENGINE_API void BlendViewInfo(FMinimalViewInfo& OtherInfo, float OtherWeight);

	/** Applies weighting to this view, in order to be blended with another one. Equals to this *= Weight. */
	ENGINE_API void ApplyBlendWeight(const float& Weight);

	/** Combines this view with another one which will be weighted. Equals to this += OtherView * Weight. */
	ENGINE_API void AddWeightedViewInfo(const FMinimalViewInfo& OtherView, const float& Weight);

	/** Calculates the projection matrix using this view info's aspect ratio (regardless of bConstrainAspectRatio) */
	ENGINE_API FMatrix CalculateProjectionMatrix() const;

	/** Calculates the projection matrix (and potentially a constrained view rectangle) given a FMinimalViewInfo and partially configured projection data (must have the view rect already set) */
	ENGINE_API static void CalculateProjectionMatrixGivenView(const FMinimalViewInfo& ViewInfo, TEnumAsByte<enum EAspectRatioAxisConstraint> AspectRatioAxisConstraint, class FViewport* Viewport, struct FSceneViewProjectionData& InOutProjectionData);
};
