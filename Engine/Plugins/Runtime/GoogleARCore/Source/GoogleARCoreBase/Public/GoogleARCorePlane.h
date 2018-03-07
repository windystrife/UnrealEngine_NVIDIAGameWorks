// Copyright 2017 Google Inc.

#pragma once
#include "GoogleARCorePrimitives.h"
#include "CoreMinimal.h"

#include "GoogleARCorePlane.generated.h"

/**
 * @ingroup GoogleARCoreBase
 * An enum that describes the tracking state of an ARCore plane.
 */
UENUM(BlueprintType)
enum class EGoogleARCorePlaneTrackingState : uint8
{
	/** ARCore is tracking this Plane. */
	Tracking,
	/** This plane has been subsumed by another plane. And will be marked as StoppedTracking in next frame.*/
	Subsumed,
	/** ARCore is not currently tracking this Plane, but may resume tracking it in the future. */
	NotCurrentlyTracking,
	/** ARCore has stopped tracking this Plane and will never resume tracking it.*/
	StoppedTracking
};

/**
 * A UObject that describes the current best knowledge of a real-world planar surface.
 *
 * <h1>Plane Merging/Subsumption</h1>
 * Two or more planes may be automatically merged into a single parent plane, resulting in each child
 * plane's GetSubsumedBy() returning the parent plane.
 *
 * A subsumed plane becomes effectively a transformed view of the parent plane. The pose and
 * bounding geometry will still update, but only in response to changes to the subsuming (parent)
 * plane's properties.
 */
UCLASS(BlueprintType)
class GOOGLEARCOREBASE_API UGoogleARCorePlane : public UObject
{
	friend class UGoogleARCorePlaneManager;

	GENERATED_BODY()

public:
	UGoogleARCorePlane();

	/** Returns the infinite plane. */
	UFUNCTION(BlueprintPure, Category = "UGoogleARCorePlane")
	FPlane GetPlane() const
	{
		return Plane;
	}

	/** Returns the unique identifier of the plane object. */
	UFUNCTION(BlueprintPure, Category = "UGoogleARCorePlane")
	int GetPlaneId() const
	{
		return Id;
	}

	/** Returns the boundary polygon points of this plane in Unreal world space. */
	UFUNCTION(BlueprintPure, Category = "UGoogleARCorePlane")
	const TArray<FVector>& GetWorldSpaceBoundaryPolygon() const
	{
		return BoundaryPolygon;
	}

	/** Returns the transform of the plane polygon bounding box in Unreal world space. */
	UFUNCTION(BlueprintPure, Category = "UGoogleARCorePlane")
	FTransform GetBoundingBoxWorldTransform() const
	{
		return BoundingBoxWorldTransform;
	}

	/** Returns the size of the plane polygon bounding box. */
	UFUNCTION(BlueprintPure, Category = "UGoogleARCorePlane")
	FVector2D GetBoundingBoxSize() const
	{
		return BoundingBoxSize;
	}

	/** Return true if the plane got updated for this frame. */
	UFUNCTION(BlueprintPure, Category = "UGoogleARCorePlane")
	bool IsUpdated() const
	{
		return bIsPlaneDataUpdated;
	}

	/**
	 * Returns the UGoogleARCorePlane reference that subsumes this plane.
	 *
	 * @return The pointer to the plane that subsumes this plane. nullptr if this plane hasn't been subsumed.
	 */
	UFUNCTION(BlueprintPure, Category = "UGoogleARCorePlane")
	UGoogleARCorePlane* GetSubsumedBy() const
	{
		return SubsumedByPlane;
	}

	/** Returns the current tracking state of this plane. */
	UFUNCTION(BlueprintPure, Category = "UGoogleARCorePlane")
	EGoogleARCorePlaneTrackingState GetTrackingState()
	{
		if (bIsPlaneDeleted)
		{
			return EGoogleARCorePlaneTrackingState::StoppedTracking;
		}

		if (SubsumedByPlaneId != -1)
		{
			return EGoogleARCorePlaneTrackingState::Subsumed;
		}

		if (!bIsPlaneValid)
		{
			return EGoogleARCorePlaneTrackingState::NotCurrentlyTracking;
		}

		return EGoogleARCorePlaneTrackingState::Tracking;
	}

	/**
	 * Performs a ray trace against the plane.
	 *
	 * @param StartPoint			Start location of the ray.
	 * @param EndPoint			End location of the ray.
	 * @param IntersectionPoint		The world location of the interaction point.
	 * @param bCheckBoundingBoxOnly		Test against plane bounding box only or check against the boundary polygon.
	 */
	UFUNCTION(BlueprintPure, Category = "UGoogleARCorePlane")
	bool CalculateLinePlaneIntersectionWithBoundary(const FVector& StartPoint, const FVector& EndPoint, FVector& IntersectionPoint, bool bCheckBoundingBoxOnly);

private:
	int Id;
	FPlane Plane;
	FGoogleARCorePose PoseData;
	TArray<FVector> BoundaryPolygon; // in world space

	FTransform BoundingBoxWorldTransform;
	FVector2D BoundingBoxSize;

	UGoogleARCorePlane* SubsumedByPlane;
	int SubsumedByPlaneId;
	bool bIsPlaneValid;
	bool bIsPlaneDeleted;
	bool bIsPlaneDataUpdated;

#if PLATFORM_ANDROID
	void UpdatePlaneData(const FTransform& UnrealPlanePose, const TangoPlaneData& TangoPlaneData, float WorldToMeterScale);
#endif
};
