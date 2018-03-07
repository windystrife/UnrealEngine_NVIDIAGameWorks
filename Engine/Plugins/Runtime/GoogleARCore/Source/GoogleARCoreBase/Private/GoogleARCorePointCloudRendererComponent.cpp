// Copyright 2017 Google Inc.

#include "GoogleARCorePointCloudRendererComponent.h"
#include "GoogleARCoreFunctionLibrary.h"
#include "DrawDebugHelpers.h"
#include "tango_client_api.h"

UGoogleARCorePointCloudRendererComponent::UGoogleARCorePointCloudRendererComponent()
{
	PreviousPointCloudTimestamp = 0.0;
	PrimaryComponentTick.bCanEverTick = true;
}

void UGoogleARCorePointCloudRendererComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction * ThisTickFunction)
{
	DrawPointCloud();
}

void UGoogleARCorePointCloudRendererComponent::DrawPointCloud()
{
	UWorld* World = GetWorld();
#if PLATFORM_ANDROID
	if (UGoogleARCoreSessionFunctionLibrary::GetSessionStatus() == EGoogleARCoreSessionStatus::Tracking)
	{
		FGoogleARCorePointCloud LatestPointCloud = UGoogleARCoreFrameFunctionLibrary::GetLatestPointCloud();
		TangoPointCloud* MostRecentPointCloud = LatestPointCloud.RawPointCloud;
		double PointCloudTimestamp = LatestPointCloud.PointCloudTimestamp;

		if (PreviousPointCloudTimestamp < PointCloudTimestamp)
		{
			// Point cloud has updated.
			PointCloudInWorldSpace.Empty();
			for (int i = 0; i < MostRecentPointCloud->num_points; i++)
			{
				FVector PointLocalSpace(MostRecentPointCloud->points[i][0], MostRecentPointCloud->points[i][1], MostRecentPointCloud->points[i][2]);
				FVector PointWorldSpace = LatestPointCloud.LocalToWorldTransfrom.TransformPosition(PointLocalSpace);
				PointCloudInWorldSpace.Add(FVector(PointWorldSpace));
			}
			PreviousPointCloudTimestamp = PointCloudTimestamp;
		}

		for (int i = 0; i < PointCloudInWorldSpace.Num(); i++)
		{
			DrawDebugPoint(World, PointCloudInWorldSpace[i], PointSize, PointColor, false);
		}
	}
#endif
}
