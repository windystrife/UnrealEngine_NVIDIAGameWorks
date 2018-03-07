// Copyright 2017 Google Inc.

#include "GoogleARCorePlaneManager.h"
#include "GoogleARCoreBaseLogCategory.h"
#include "GoogleARCoreDevice.h"
#include "UObject/UObjectGlobals.h"

#if PLATFORM_ANDROID
#include "tango_client_api2.h"
#endif

namespace
{
	FMatrix ConvertTangoSpacePoseToUnrealSpace(const FMatrix& InPoseInTangoSpace)
	{
		static FMatrix UnrealTangoConvert = FMatrix(
			FPlane(0.0f, 1.0f, 0.0f, 0.0f),
			FPlane(1.0f, 0.0f, 0.0f, 0.0f),
			FPlane(0.0f, 0.0f, 1.0f, 0.0f),
			FPlane(0.0f, 0.0f, 0.0f, 1.0f));
		static FMatrix UnrealTangoConvertInverse = UnrealTangoConvert.InverseFast();
		return UnrealTangoConvert * InPoseInTangoSpace * UnrealTangoConvertInverse;
	}
	const float PlaneUpdateTimePeriodInSecond = 0.1f;
}

/************************************************************************/
/*                      GoogleARCorePlaneManager                        */
/************************************************************************/

UGoogleARCorePlaneManager::UGoogleARCorePlaneManager()
{
	TimeLeftToUpdatePlanes = 0.f;
}
void UGoogleARCorePlaneManager::GetAllPlanes(TArray<UGoogleARCorePlane*>& ARCorePlaneList) const
{
	ARCorePlaneMap.GenerateValueArray(ARCorePlaneList);
}

void UGoogleARCorePlaneManager::UpdatePlanes(float DeltaTime)
{
#if PLATFORM_ANDROID
	TangoPlaneData* Planes = nullptr;
	size_t PlaneNum = 0;

	for (auto& Item : ARCorePlaneMap)
	{
		Item.Value->bIsPlaneDataUpdated = false;
	}

	TimeLeftToUpdatePlanes -= DeltaTime;
	if (TimeLeftToUpdatePlanes > 0.0f)
	{
		return;
	}

	TimeLeftToUpdatePlanes = PlaneUpdateTimePeriodInSecond;
	if (TangoService_Experimental_getPlanes(&Planes, &PlaneNum) == TANGO_SUCCESS)
	{
		// If plane is nullptr, don't update plane states.
		if (Planes != nullptr)
		{
			TSet<int> PlaneIds;
			float WorldToMeterScale = FGoogleARCoreDevice::GetInstance()->GetWorldToMetersScale();
			for (int i = 0; i < PlaneNum; i++)
			{
				FTransform PlaneTangoPose = FGoogleARCoreDevice::GetInstance()->TangoMotionManager.ConvertTangoPoseToTransform(&Planes[i].pose);
				FTransform PlaneUnrealPose(ConvertTangoSpacePoseToUnrealSpace(PlaneTangoPose.ToMatrixNoScale()));

				int PlaneId = Planes[i].id;
				PlaneIds.Add(PlaneId);

				if (!ARCorePlaneMap.Contains(PlaneId))
				{
					ARCorePlaneMap.Add(PlaneId, NewObject<UGoogleARCorePlane>());
					ARCorePlaneMap[PlaneId]->UpdatePlaneData(PlaneUnrealPose, Planes[i], WorldToMeterScale);
				}
				else if (ARCorePlaneMap[PlaneId]->PoseData.Timestamp.TimestampValue < Planes[i].timestamp)
				{
					ARCorePlaneMap[PlaneId]->UpdatePlaneData(PlaneUnrealPose, Planes[i], WorldToMeterScale);
				}
			}

			// Delete planes that is no longer tracked
			for (auto& Item : ARCorePlaneMap)
			{
				if (!PlaneIds.Contains(Item.Key))
				{
					Item.Value->bIsPlaneDeleted = true;
					Item.Value->bIsPlaneDataUpdated = true;
					ARCorePlaneMap.Remove(Item.Key);
				}
			}

			// Update the subsumed plane reference
			for (auto& Item : ARCorePlaneMap)
			{
				if (Item.Value->SubsumedByPlaneId != -1)
				{
					UGoogleARCorePlane* SubsumedByPlane;
					int SubsumedBy = Item.Value->SubsumedByPlaneId;
					while (ARCorePlaneMap.Contains(SubsumedBy))
					{
						SubsumedByPlane = ARCorePlaneMap[SubsumedBy];
						SubsumedBy = SubsumedByPlane->SubsumedByPlaneId;
					}
					Item.Value->SubsumedByPlane = SubsumedByPlane;
				}
			}
			TangoPlaneData_free(Planes, PlaneNum);
		}
	}
	else
	{
		for (auto& Item : ARCorePlaneMap)
		{
			Item.Value->bIsPlaneDataUpdated = true;
			Item.Value->bIsPlaneValid = false;
		}
	}
#endif
}

void UGoogleARCorePlaneManager::EmptyPlanes()
{
	// Mark plane object as deleted before remove them from the map, so developer can know the plane is deleted from the system.
	for (auto& Item : ARCorePlaneMap)
	{
		Item.Value->bIsPlaneDeleted = true;
	}
	ARCorePlaneMap.Empty();
}

struct FPlaneIntersectionPoint
{
	FVector Position;
	int PlaneId;
};

bool UGoogleARCorePlaneManager::PerformLineTraceOnPlanes(FVector StartPoint, FVector EndPoint, FVector& ImpactPoint, FVector& ImpactNormal, UGoogleARCorePlane*& HitPlane, bool bCheckBoundingBoxOnly)
{
	TArray<UGoogleARCorePlane*> AllPlanes;
	if (ARCorePlaneMap.Num() == 0)
	{
		return false;
	}
	ARCorePlaneMap.GenerateValueArray(AllPlanes);

	float ClosestDist = FVector::Dist(StartPoint, EndPoint);
	UGoogleARCorePlane* ClosestPlane = nullptr;
	FVector ClosestIntersectionPoint;

	for (UGoogleARCorePlane* ARCorePlane : AllPlanes)
	{
		// No need to hit test against subsumed planes.
		if (ARCorePlane->GetTrackingState() != EGoogleARCorePlaneTrackingState::Tracking)
		{
			continue;
		}

		FVector IntersectionPoint;
		if (ARCorePlane->CalculateLinePlaneIntersectionWithBoundary(StartPoint, EndPoint, IntersectionPoint, bCheckBoundingBoxOnly))
		{
			float Dist = FVector::Dist(IntersectionPoint, StartPoint);
			if (Dist <= ClosestDist)
			{
				ClosestDist = Dist;
				ClosestPlane = ARCorePlane;
				ClosestIntersectionPoint = IntersectionPoint;
			}
		}
	}

	if (ClosestPlane != nullptr)
	{
		ImpactPoint = ClosestIntersectionPoint;
		ImpactNormal = ClosestPlane->GetPlane().GetSafeNormal();
		HitPlane = ClosestPlane;
		return true;
	}

	return false;
}
