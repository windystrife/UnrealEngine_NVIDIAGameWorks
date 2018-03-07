// Copyright 2017 Google Inc.

#include "GoogleARCorePlane.h"
#include "GoogleARCoreBaseLogCategory.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	bool IsPointInsideConvexPolygon(const FVector& Point, const TArray<FVector>& PolygonPoints)
	{
		FVector PreviousCross = FVector::ZeroVector;
		int CrossDir = 0;
		for (int i = 0; i < PolygonPoints.Num(); i++)
		{
			if (Point == PolygonPoints[i])
			{
				return true;
			}
			int j = i == PolygonPoints.Num() - 1 ? 0 : i + 1;
			FVector Seg1 = Point - PolygonPoints[i];
			FVector Seg2 = PolygonPoints[j] - PolygonPoints[i];
			FVector Cross = Seg1 ^ Seg2;

			if (i == 0)
			{
				PreviousCross = Cross;
			}
			else if(i == 1)
			{
				CrossDir = FMath::Sign(PreviousCross | Cross);
			}
			else if (CrossDir != FMath::Sign(PreviousCross | Cross))
			{
				return false;
			}
		}
		return true;
	}
}


UGoogleARCorePlane::UGoogleARCorePlane()
{
	Id = -1;
	SubsumedByPlaneId = -1;
	SubsumedByPlane = nullptr;
	bIsPlaneValid = false;
	bIsPlaneDeleted = false;
	bIsPlaneDataUpdated = false;
}

#if PLATFORM_ANDROID
void UGoogleARCorePlane::UpdatePlaneData(const FTransform& UnrealPlanePose, const TangoPlaneData& TangoPlaneData, float WorldToMeterScale)
{
	Id = TangoPlaneData.id;
	Plane = FPlane(UnrealPlanePose.GetLocation(), UnrealPlanePose.TransformVector(FVector::UpVector));
	PoseData.Pose = UnrealPlanePose;
	PoseData.Timestamp = TangoPlaneData.timestamp;
	BoundaryPolygon.Empty();
	for (int i = 0; i < TangoPlaneData.boundary_point_num; i++)
	{

		FVector PointLocalPosition = FVector(TangoPlaneData.boundary_polygon[2 * i + 1] * WorldToMeterScale, TangoPlaneData.boundary_polygon[2 * i] * WorldToMeterScale, 0.f);
		FVector PointWorldPosition = PoseData.Pose.TransformPosition(PointLocalPosition);
		BoundaryPolygon.Add(PointWorldPosition);
	}

	FVector BoundingBoxWorldLocation = PoseData.Pose.TransformPosition(FVector(TangoPlaneData.center_y * WorldToMeterScale, TangoPlaneData.center_x * WorldToMeterScale, 0));
	// Assuming horizontal plane for the rotation for now
	FQuat BoundingBoxWorldRotation = FQuat(FVector::UpVector, -TangoPlaneData.yaw);
	BoundingBoxWorldTransform.SetLocation(BoundingBoxWorldLocation);
	BoundingBoxWorldTransform.SetRotation(BoundingBoxWorldRotation);
	BoundingBoxWorldTransform.SetScale3D(FVector::OneVector);

	BoundingBoxSize = FVector2D(TangoPlaneData.height * WorldToMeterScale, TangoPlaneData.width * WorldToMeterScale);
	bIsPlaneValid = TangoPlaneData.is_valid;
	SubsumedByPlaneId = TangoPlaneData.subsumed_by;

	bIsPlaneDataUpdated = true;
}
#endif

bool UGoogleARCorePlane::CalculateLinePlaneIntersectionWithBoundary(const FVector& StartPoint, const FVector& EndPoint, FVector& IntersectionPoint, bool bCheckBoundingBoxOnly)
{
	IntersectionPoint = FMath::LinePlaneIntersection(StartPoint, EndPoint, Plane);

	// Check for bounding box first
	FVector IntersectionPointLocalPosition = BoundingBoxWorldTransform.InverseTransformPositionNoScale(IntersectionPoint);
	if (FMath::Abs(IntersectionPointLocalPosition.X) <= BoundingBoxSize.X / 2.0f && FMath::Abs(IntersectionPointLocalPosition.Y) <= BoundingBoxSize.Y / 2.0f)
	{
		if (bCheckBoundingBoxOnly)
		{
			return true;
		}
		else if (IsPointInsideConvexPolygon(IntersectionPoint, BoundaryPolygon))
		{
			return true;
		}
	}

	IntersectionPoint = FVector::ZeroVector;
	return false;
}
