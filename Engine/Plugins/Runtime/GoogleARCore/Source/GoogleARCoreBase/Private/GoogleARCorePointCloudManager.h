// Copyright 2017 Google Inc.

#pragma once
#include "GoogleARCorePrimitives.h"
#if PLATFORM_ANDROID
#include "tango_support_api.h"
#include <set>
#endif

class FGoogleARCoreDevice;

enum class EGoogleARCorePointCloudMode : uint8
{
	None,
	FeaturePoint,
	DepthCamera
};

class FGoogleARCorePointCloudManager
{
	friend class FGoogleARCoreDevice;

public:
	FGoogleARCorePointCloudManager();

#if PLATFORM_ANDROID
	bool ConnectPointCloud(TangoConfig Config);
	void DisconnectPointCloud();
#endif

	EGoogleARCoreReferenceFrame GetTargetFrame() const;
	FGoogleARCorePointCloud GetLatestPointCloud() const;
	bool PerformLineTraceOnPointCloud(float U, float V, FVector& ImpactPoint, FVector& ImpactNormal);
	bool PerformLineTraceOnFeaturePoint(const FVector& StartPoint, const FVector& EndPoint, FVector& ImpactPoint, FVector& ImpactNormal);

#if PLATFORM_ANDROID
	bool FindFloorPlane(TMap<int32, int32>& NumUpPoints, std::set<int32>& NonNoiseBuckets,	double& LastPointCloudTimestamp, float& PlaneZ);
	void EvalPointCloud(TFunction<void(const TangoPointCloud*)> Func);
	void HandleOnPointCloudAvailable(const TangoPointCloud* PointCloud);
#endif

private:
	void UpdateBaseFrame(EGoogleARCoreReferenceFrame InBaseFrame);
	void UpdatePointCloud();
private:
	EGoogleARCoreReferenceFrame BaseFrame;
	EGoogleARCorePointCloudMode PointCloudType;
#if PLATFORM_ANDROID
	TangoSupport_PointCloudManager* PointCloudManager;
#endif
	FCriticalSection PointCloudLock;
	FDelegateHandle OnTangoDisconnectedHandle;
	FMatrix TangoToUnrealCameraMatrix;
	FTransform TangoToUnrealCameraTransform;

	FGoogleARCorePointCloud LatestPointCloud;

};
