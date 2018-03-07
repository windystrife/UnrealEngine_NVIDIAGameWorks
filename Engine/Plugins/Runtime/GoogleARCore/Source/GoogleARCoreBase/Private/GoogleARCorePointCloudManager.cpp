// Copyright 2017 Google Inc.

#include "GoogleARCorePointCloudManager.h"
#include "GoogleARCoreBaseLogCategory.h"
#include "GoogleARCoreAndroidHelper.h"
#include "GoogleARCoreDevice.h"
#include "GoogleARCoreCameraManager.h"
#include "GoogleARCorePrimitives.h"

#include "Engine/Engine.h" // for GEngine
#include "Misc/ScopeLock.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"

#if PLATFORM_ANDROID
#include "tango_client_api.h"

namespace
{
	int MaxPointCloudElements = 0;

	// constants for floor finding
	// The minimum number of points near a world position y to determine that it is a reasonable floor.
	const int32 RECOGNITION_THRESHOLD = 1024;
	// The minimum number of points near a world position y to determine that it is not simply noise points.
	const int32 NOISE_THRESHOLD = 512;
	// The interval in meters between buckets of points. For example, a high sensitivity of 0.01 will group
	// points into buckets every 1cm.
	const float SENSITIVITY = 2.0f;

	const float FEATURE_POINT_RAY_CAST_ANGLE_RANGE = 5.0f; // 5 degree

	FGoogleARCorePointCloudManager* TangoPointCloudPtr = nullptr;
	void OnPointCloudAvailableRouter(void* Context, const TangoPointCloud* PointCloud)
	{
		if (TangoPointCloudPtr != nullptr)
		{
			TangoPointCloudPtr->HandleOnPointCloudAvailable(PointCloud);
		}
	}
}
#endif

FGoogleARCorePointCloudManager::FGoogleARCorePointCloudManager()
{
	TangoToUnrealCameraMatrix = FMatrix::Identity;
	TangoToUnrealCameraMatrix.M[0][0] = 0;
	TangoToUnrealCameraMatrix.M[2][0] = 1;
	TangoToUnrealCameraMatrix.M[1][1] = 0;
	TangoToUnrealCameraMatrix.M[0][1] = 1;
	TangoToUnrealCameraMatrix.M[2][2] = 0;
	TangoToUnrealCameraMatrix.M[1][2] = -1;
	TangoToUnrealCameraTransform.SetFromMatrix(TangoToUnrealCameraMatrix);

	// Hard-coded for feature point for now.
	PointCloudType = EGoogleARCorePointCloudMode::FeaturePoint;
}

#if PLATFORM_ANDROID
bool FGoogleARCorePointCloudManager::ConnectPointCloud(TangoConfig Config)
{
	UE_LOG(LogGoogleARCore, Log, TEXT("Connecting Tango Point Cloud..."));

	if (!OnTangoDisconnectedHandle.IsValid())
	{
		FGoogleARCoreDevice::GetInstance()->OnTangoServiceUnboundDelegate.AddRaw(this, &FGoogleARCorePointCloudManager::DisconnectPointCloud);
	}
	if (Config == nullptr)
	{
		return false;
	}
	int32 MaxPointCloudElementsConfig = 0;
	bool bSuccess = TangoConfig_getInt32(Config, "max_point_cloud_elements", &MaxPointCloudElementsConfig) == TANGO_SUCCESS;
	if (bSuccess)
	{
		if (PointCloudManager != nullptr)
		{
			if (MaxPointCloudElements != MaxPointCloudElementsConfig)
			{
				TangoSupport_freePointCloudManager(PointCloudManager);
				PointCloudManager = nullptr;
			}
		}
		if (PointCloudManager == nullptr)
		{
			MaxPointCloudElements = MaxPointCloudElementsConfig;
			int32 ret = TangoSupport_createPointCloudManager(MaxPointCloudElements, &PointCloudManager);
			if (ret != TANGO_SUCCESS)
			{
				UE_LOG(LogGoogleARCore, Error, TEXT("createPointCloudManager failed with error code: %d"), ret);
				return false;
			}
			else
			{
				UE_LOG(LogGoogleARCore, Log, TEXT("Created point cloud manager for max point cloud elements %d"), MaxPointCloudElements);
			}
		}

		if (TangoPointCloudPtr == nullptr)
		{
			TangoPointCloudPtr = this;
		}

		int32 ret = TangoService_connectOnPointCloudAvailable(OnPointCloudAvailableRouter);
		if (ret != TANGO_SUCCESS)
		{
			UE_LOG(LogGoogleARCore, Error, TEXT("connectOnPointCloudAvailable failed with error code: %d"), ret);
			return false;
		}
		else
		{
			UE_LOG(LogGoogleARCore, Log, TEXT("Connected point cloud available callback"));
		}

		UE_LOG(LogGoogleARCore, Log, TEXT("Tango Point Cloud connected"));
		return true;
	}
	else
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("TangoPointCloud construction failed because read of max_point_cloud_elements was not successful."));
	}
	return false;
}

void FGoogleARCorePointCloudManager::DisconnectPointCloud()
{
	UE_LOG(LogGoogleARCore, Log, TEXT("Tango Point Cloud disconnected"));
}

void FGoogleARCorePointCloudManager::HandleOnPointCloudAvailable(const TangoPointCloud* PointCloud)
{
	FScopeLock ScopeLock(&PointCloudLock);
	if (PointCloudManager != nullptr && PointCloud != nullptr)
	{
		if (TangoSupport_updatePointCloud(PointCloudManager, PointCloud) != TANGO_SUCCESS)
		{
#if ENABLE_ARCORE_DEBUG_LOG
			UE_LOG(LogGoogleARCore, Error, TEXT("TangoSupport_updatePointCloud failed"));
#endif
		}
	}
}
#endif

void FGoogleARCorePointCloudManager::UpdateBaseFrame(EGoogleARCoreReferenceFrame InBaseFrame)
{
	BaseFrame = InBaseFrame;
}

void FGoogleARCorePointCloudManager::UpdatePointCloud()
{
	FScopeLock ScopeLock(&PointCloudLock);
#if PLATFORM_ANDROID
	if (PointCloudManager == nullptr)
	{
		return;
	}
	TangoPointCloud* PointCloud = nullptr;
	TangoPoseData RawPose;
	TangoSupport_Rotation DisplayRotation = TANGO_SUPPORT_ROTATION_0;
	switch (FGoogleARCoreAndroidHelper::GetDisplayRotation())
	{
	case 1:
		DisplayRotation = TANGO_SUPPORT_ROTATION_90;
		break;
	case 2:
		DisplayRotation = TANGO_SUPPORT_ROTATION_180;
		break;
	case 3:
		DisplayRotation = TANGO_SUPPORT_ROTATION_270;
		break;
	default:
		break;
	}
	// @TODO
	// Can't use BaseFrame if it's GLOBAL_WGS84 with getLatestPointCloudWithPose (always fails with invalid pose).
	// So instead we hard code it to start_of_service and only use the timestamp from RawPose.
	// We'll call TangoMotionManager to get the pose which will do the right thing in all cases.
	if (TangoSupport_getLatestPointCloudWithPose(
		PointCloudManager, TANGO_COORDINATE_FRAME_START_OF_SERVICE,
		TANGO_SUPPORT_ENGINE_UNREAL, TANGO_SUPPORT_ENGINE_UNREAL,
		DisplayRotation, &PointCloud, &RawPose) != TANGO_SUCCESS)
	{
#if ENABLE_ARCORE_DEBUG_LOG
		UE_LOG(LogGoogleARCore, Log, TEXT("getLatestPointCloudWithPose failed"));
#endif
		return;
	}

	// Calculate the point cloud local to world transform
	FGoogleARCorePose PointCloudPose;
	if (!FGoogleARCoreDevice::GetInstance()->TangoMotionManager.GetPoseAtTime(
		GetTargetFrame(), RawPose.timestamp, PointCloudPose, true))
	{
#if ENABLE_ARCORE_DEBUG_LOG
		UE_LOG(LogGoogleARCore, Log, TEXT("Failed to get point cloud pose at timestamp: %f!"), RawPose.timestamp);
#endif
		return;
	}
	const float UnrealUnitsPerMeter = FGoogleARCoreDevice::GetInstance()->GetWorldToMetersScale();
	FTransform UnrealDepthToWorldTransform = PointCloudPose.Pose;
	FTransform TangoToUnrealScaleTransform(FQuat::Identity, FVector(0, 0, 0),
		FVector(UnrealUnitsPerMeter, UnrealUnitsPerMeter, UnrealUnitsPerMeter)
	);

	LatestPointCloud.RawPointCloud = PointCloud;
	LatestPointCloud.PointCloudTimestamp = RawPose.timestamp;
	LatestPointCloud.LocalToWorldTransfrom = TangoToUnrealScaleTransform * TangoToUnrealCameraTransform * UnrealDepthToWorldTransform;
#endif
}

EGoogleARCoreReferenceFrame FGoogleARCorePointCloudManager::GetTargetFrame() const
{
	switch (PointCloudType)
	{
	case EGoogleARCorePointCloudMode::DepthCamera:
		return EGoogleARCoreReferenceFrame::CAMERA_DEPTH;
	case EGoogleARCorePointCloudMode::FeaturePoint:
		return EGoogleARCoreReferenceFrame::CAMERA_COLOR;
	default:
		return EGoogleARCoreReferenceFrame::INVALID;
	}
}

FGoogleARCorePointCloud FGoogleARCorePointCloudManager::GetLatestPointCloud() const
{
	return LatestPointCloud;
}

bool FGoogleARCorePointCloudManager::PerformLineTraceOnPointCloud(float U, float V, FVector& ImpactPoint, FVector& ImpactNormal)
{
#if PLATFORM_ANDROID
	if (PointCloudType != EGoogleARCorePointCloudMode::DepthCamera)
	{
		UE_LOG(LogGoogleARCore, Log, TEXT("Failed to line trace depth point cloud: EGoogleARCorePointCloudMode not set to DepthCamera!"));
		return false;
	}

	if (LatestPointCloud.RawPointCloud->num_points < 50)
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("FitPlane: Point cloud number < 50"));
		return false;
	}
	/// Calculate the conversion from the latest depth camera position to the
	/// position of the most recent color camera image. This corrects for screen
	/// lag between the two systems.

	TangoPoseData pose_color_camera_t0_T_depth_camera_t1;
	double camera_time_stamp = FGoogleARCoreDevice::GetInstance()->TangoARCameraManager.GetColorCameraImageTimestamp();

	int32 ret = TangoSupport_calculateRelativePose(
		LatestPointCloud.PointCloudTimestamp, TANGO_COORDINATE_FRAME_CAMERA_DEPTH,
		camera_time_stamp, TANGO_COORDINATE_FRAME_CAMERA_COLOR,
		&pose_color_camera_t0_T_depth_camera_t1);
	if (ret != TANGO_SUCCESS)
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("FitPlane: could not calculate relative pose"));
		return false;
	}
	TangoSupport_Rotation DisplayRotation = TANGO_SUPPORT_ROTATION_0;
	switch (FGoogleARCoreAndroidHelper::GetDisplayRotation())
	{
	case 1:
		DisplayRotation = TANGO_SUPPORT_ROTATION_90;
		break;
	case 2:
		DisplayRotation = TANGO_SUPPORT_ROTATION_180;
		break;
	case 3:
		DisplayRotation = TANGO_SUPPORT_ROTATION_270;
		break;
	default:
		break;
	}
	float UV[2] = { U, V };
	double double_depth_position[3];
	double double_depth_plane_equation[4];
	double identity_translation[3] = { 0.0, 0.0, 0.0 };
	double identity_orientation[4] = { 0.0, 0.0, 0.0, 1.0 };
	if (TangoSupport_fitPlaneModelNearPoint(
		LatestPointCloud.RawPointCloud, identity_translation, identity_orientation,
		UV,
		DisplayRotation,
		pose_color_camera_t0_T_depth_camera_t1.translation,
		pose_color_camera_t0_T_depth_camera_t1.orientation,
		double_depth_position, double_depth_plane_equation) != TANGO_SUCCESS)
	{
		UE_LOG(LogGoogleARCore, Log, TEXT("fitPlaneModelNearPoint failed"));
		return false;
	}
	FMatrix DepthToWorldMatrix = LatestPointCloud.LocalToWorldTransfrom.ToMatrixWithScale();
	FVector DepthPosition(
		(float)double_depth_position[0],
		(float)double_depth_position[1],
		(float)double_depth_position[2]
	);
	FPlane DepthPlane(
		(float)double_depth_plane_equation[0],
		(float)double_depth_plane_equation[1],
		(float)double_depth_plane_equation[2],
		-(float)double_depth_plane_equation[3]
	);

	FVector WorldPoint = DepthToWorldMatrix.TransformPosition(DepthPosition);
	FVector WorldUp = DepthPlane.TransformBy(DepthToWorldMatrix);

	ImpactPoint = WorldPoint;
	ImpactNormal = WorldUp;

	return true;
#endif
	return false;
}

bool FGoogleARCorePointCloudManager::PerformLineTraceOnFeaturePoint(const FVector& StartPoint, const FVector& EndPoint, FVector& ImpactPoint, FVector& ImpactNormal)
{
#if PLATFORM_ANDROID
	if (PointCloudType != EGoogleARCorePointCloudMode::FeaturePoint)
	{
		UE_LOG(LogGoogleARCore, Log, TEXT("Failed to line trace feature point: EGoogleARCorePointCloudMode not set to FeaturePoint!"));
		return false;
	}

	TangoPointCloud* RawPointCloud = LatestPointCloud.RawPointCloud;

	if (RawPointCloud == nullptr)
	{
		return false;
	}

	FVector RayDirection = (EndPoint - StartPoint).GetSafeNormal();

	FVector BestFeaturePoint;
	float MinimalAngle = FEATURE_POINT_RAY_CAST_ANGLE_RANGE;

	float RayDistance = (EndPoint - StartPoint).Size();
	for (int i = 0; i < RawPointCloud->num_points; i++)
	{
		FVector PointInLocalSpace(RawPointCloud->points[i][0], RawPointCloud->points[i][1], RawPointCloud->points[i][2]);
		FVector PointInWorldSpace = LatestPointCloud.LocalToWorldTransfrom.TransformPosition(PointInLocalSpace);

		FVector StartToPoint = PointInWorldSpace - StartPoint;
		if (StartToPoint.Size() < RayDistance)
		{
			float Angle = FMath::RadiansToDegrees(FMath::Acos(StartToPoint.GetSafeNormal() | RayDirection));
			if (Angle < MinimalAngle)
			{
				MinimalAngle = Angle;
				BestFeaturePoint = PointInWorldSpace;
			}
		}
	}

	if (MinimalAngle != FEATURE_POINT_RAY_CAST_ANGLE_RANGE)
	{
		ImpactPoint = BestFeaturePoint;
		ImpactNormal = FVector::UpVector; // Hardcoded to Up Vector for now.
		return true;
	}
#endif
	return false;
}

#if PLATFORM_ANDROID
bool FGoogleARCorePointCloudManager::FindFloorPlane(TMap<int32,int32>& NumUpPoints, std::set<int32>& NonNoiseBuckets, double& LastPointCloudTimestamp, float& PlaneZ)
{
	if (LatestPointCloud.PointCloudTimestamp == LastPointCloudTimestamp)
	{
		return false;
	}
	FMatrix DepthToWorldMatrix = LatestPointCloud.LocalToWorldTransfrom.ToMatrixWithScale();
	LastPointCloudTimestamp = LatestPointCloud.PointCloudTimestamp;
	TangoPointCloud* PointCloudData = LatestPointCloud.RawPointCloud;
	const float UnrealUnitsPerMeter = FGoogleARCoreDevice::GetInstance()->GetWorldToMetersScale();
	// Count each depth point into a bucket based on its world up value
	for (int32 i = 0; i < PointCloudData->num_points; i++)
	{
		// Group similar points into buckets based on sensitivity
		FVector DepthPoint(PointCloudData->points[i][0], PointCloudData->points[i][1], PointCloudData->points[i][2]);
		FVector WorldPoint = DepthToWorldMatrix.TransformPosition(DepthPoint);
		int32 Units = FMath::RoundToInt(WorldPoint.Z / SENSITIVITY) * SENSITIVITY;
		int32* NumPtr = NumUpPoints.Find(Units);
		if (!NumPtr)
		{
			NumPtr = &NumUpPoints.Add(Units, 0);
		}
		int32& Num = *NumPtr;
		Num++;
		// Check if the plane is a non-noise plane.
		if (Num > NOISE_THRESHOLD)
		{
			NonNoiseBuckets.insert(Units);
		}
	}
	if (NonNoiseBuckets.empty())
	{
		return false;
	}
	// Find a plane at the lowest Z value. The Z value must be below the camera's Z position.
	int32 Bucket = *NonNoiseBuckets.begin();
	int32 NumPoints = NumUpPoints[Bucket];
	if (NumPoints > RECOGNITION_THRESHOLD)
	{
		FGoogleARCorePose CurrentDevicePose;
		FGoogleARCoreDevice::GetInstance()->TangoMotionManager.GetCurrentPose(EGoogleARCoreReferenceFrame::DEVICE, CurrentDevicePose);
		const FVector CameraPosition = CurrentDevicePose.Pose.GetLocation();
		if (Bucket < CameraPosition.Z)
		{
			PlaneZ = Bucket;
			return true;
		}
	}

	return false;
}

void FGoogleARCorePointCloudManager::EvalPointCloud(TFunction<void(const TangoPointCloud*)> Func)
{
	FScopeLock ScopeLock(&PointCloudLock);
	Func(LatestPointCloud.RawPointCloud);
}
#endif
