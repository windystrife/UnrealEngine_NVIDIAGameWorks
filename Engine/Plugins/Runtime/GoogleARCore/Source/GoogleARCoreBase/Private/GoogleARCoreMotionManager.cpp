// Copyright 2017 Google Inc.

#include "GoogleARCoreMotionManager.h"
#include "GoogleARCoreDevice.h"
#include "GoogleARCoreBaseLogCategory.h"
#include "GoogleARCoreAndroidHelper.h"
#include "Misc/ScopeLock.h"
#include "Containers/StackTracker.h"
#include "CoreGlobals.h"

#define GET_POSE_MAX_RETRY 5
#define GET_POSE_MAX_WAIT 16 //16ms

#if PLATFORM_ANDROID
#include "tango_client_api.h"

static void OnPoseAvailable(void* Context, const TangoPoseData* Pose)
{
	FGoogleARCoreDevice* TangoDeviceContext = static_cast<FGoogleARCoreDevice*> (Context);
	if (TangoDeviceContext == nullptr)
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("Error: Failed to cast to FGoogleARCoreDevice!"));
	}
	else
	{
		TangoDeviceContext->TangoMotionManager.OnTangoPoseUpdated(Pose);
	}
}
#endif

FGoogleARCoreMotionManager::FGoogleARCoreMotionManager()
	: bIsDevicePoseValid(false)
	, bIsColorCameraPoseValid(false)
	, bIsEcefPoseValid(false)
	, bWaitingForNewPose(false)
	, bIsRelocalized(false)
	, NewPoseAvailable(nullptr)
{
}

FGoogleARCoreMotionManager::~FGoogleARCoreMotionManager()
{
	if (NewPoseAvailable != nullptr)
	{
		FPlatformProcess::ReturnSynchEventToPool(NewPoseAvailable);
	}
}

#if PLATFORM_ANDROID
void FGoogleARCoreMotionManager::OnTangoPoseUpdated(const TangoPoseData* Pose)
{
	// We query is the tracking is localized here to avoid getPoseAtTime call every frame.
	if (Pose->frame.base == TANGO_COORDINATE_FRAME_AREA_DESCRIPTION && Pose->frame.target == TANGO_COORDINATE_FRAME_START_OF_SERVICE)
	{
		bIsRelocalized = Pose->status_code == TANGO_POSE_VALID;
	}
	else if (bWaitingForNewPose && NewPoseAvailable)
	{
		NewPoseAvailable->Trigger();
	}
}
#endif

bool FGoogleARCoreMotionManager::OnTrackingSessionStarted(EGoogleARCoreReferenceFrame PoseBaseFrame)
{
#if PLATFORM_ANDROID
	TangoCoordinateFramePair PoseFramePairs[2];
	// We need to check the pose status between area_description and start of service to see if we are localized.
	PoseFramePairs[0].base = TANGO_COORDINATE_FRAME_AREA_DESCRIPTION;
	PoseFramePairs[0].target = TANGO_COORDINATE_FRAME_START_OF_SERVICE;
	PoseFramePairs[1].base = TANGO_COORDINATE_FRAME_START_OF_SERVICE;
	PoseFramePairs[1].target = TANGO_COORDINATE_FRAME_DEVICE;
	if (TangoService_connectOnPoseAvailable(2, PoseFramePairs, OnPoseAvailable) != TANGO_SUCCESS)
	{
		return false;
	}
#endif
	return true;
}

void FGoogleARCoreMotionManager::OnTrackingSessionStopped()
{
	// Invalid cached pose;
	bIsDevicePoseValid = false;
	bIsColorCameraPoseValid = false;
}

void FGoogleARCoreMotionManager::UpdateBaseFrame(EGoogleARCoreReferenceFrame InBaseFrame)
{
	BaseFrame = InBaseFrame;
}

void FGoogleARCoreMotionManager::UpdateTangoPoses()
{
	bIsDevicePoseValid = GetPoseAtTime(EGoogleARCoreReferenceFrame::DEVICE, 0.0f, LatestDevicePose);
	// Make sure the device pose and camera pose are at the same timestamp.
	bIsColorCameraPoseValid = GetPoseAtTime(EGoogleARCoreReferenceFrame::CAMERA_COLOR, LatestDevicePose.Timestamp.TimestampValue, LatestColorCameraPose);
}

FGoogleARCoreTimestamp FGoogleARCoreMotionManager::GetCurrentPoseTimestamp()
{
	return LatestDevicePose.Timestamp;
}

bool FGoogleARCoreMotionManager::GetCurrentPose(EGoogleARCoreReferenceFrame TargetFrame, FGoogleARCorePose& OutTangoPose)
{
	switch (TargetFrame)
	{
	case EGoogleARCoreReferenceFrame::DEVICE:
		OutTangoPose = LatestDevicePose;
		return bIsDevicePoseValid;
	case EGoogleARCoreReferenceFrame::CAMERA_COLOR:
		OutTangoPose = LatestColorCameraPose;
		return bIsColorCameraPoseValid;
	}
	return false;
}

bool FGoogleARCoreMotionManager::IsTrackingValid()
{
	return bIsDevicePoseValid;
}

bool FGoogleARCoreMotionManager::IsRelocalized()
{
	return bIsRelocalized;
}

#if PLATFORM_ANDROID
FTransform FGoogleARCoreMotionManager::ConvertTangoPoseToTransform(const TangoPoseData* RawPose)
{
	float UnrealUnitsPerMeter = FGoogleARCoreDevice::GetInstance()->GetWorldToMetersScale();
	float Forward = float(RawPose->orientation[0]);
	float Right = float(RawPose->orientation[1]);
	float Up = float(RawPose->orientation[2]);
	FQuat Orientation(Forward, Right, Up, float(RawPose->orientation[3]));
	Forward = float(UnrealUnitsPerMeter*RawPose->translation[0]);
	Right = float(UnrealUnitsPerMeter*RawPose->translation[1]);
	Up = float(UnrealUnitsPerMeter*RawPose->translation[2]);
	FVector Position(Forward, Right, Up);
	return FTransform(Orientation, Position);
}
#endif

bool FGoogleARCoreMotionManager::GetPoseAtTime_Blocking(EGoogleARCoreReferenceFrame TargetFrame, double Timestamp, FGoogleARCorePose& OutTangoPose, bool bIgnoreDisplayRotation)
{
	bool bHasValidPose = false;

	bHasValidPose = GetPoseAtTime(TargetFrame, Timestamp, OutTangoPose, bIgnoreDisplayRotation);

	if (!bHasValidPose)
	{
#if ENABLE_ARCORE_DEBUG_LOG
		UE_LOG(LogGoogleARCore, Error, TEXT("Failed to get pose at timestamp %f. Blocking the thread to wait the pose available."), Timestamp);
#endif

		if (NewPoseAvailable == nullptr)
		{
			NewPoseAvailable = FPlatformProcess::GetSynchEventFromPool(false);
		}

		bWaitingForNewPose = true;
		int RetryTimes = 0;
		do
		{
			RetryTimes++;
			// We explicitly break the loop after we tried 5 times and in case
			// we still have the NewPoseAvailable triggered but not able to get a valid pose given the time stamp.
			if (RetryTimes > GET_POSE_MAX_RETRY || !NewPoseAvailable->Wait(GET_POSE_MAX_WAIT))
			{
#if ENABLE_ARCORE_DEBUG_LOG
				// tango core probably disconnected or the tracking is lost, give up
				UE_LOG(LogGoogleARCore, Error, TEXT("Timed out waiting for GetPoseAtTime at Timestamp %f"), Timestamp);
#endif
				break;
			}

			bHasValidPose = GetPoseAtTime(TargetFrame, Timestamp, OutTangoPose, bIgnoreDisplayRotation);
		} while (!bHasValidPose);
		bWaitingForNewPose = false;

		if (bHasValidPose)
		{
#if ENABLE_ARCORE_DEBUG_LOG
			UE_LOG(LogGoogleARCore, Error, TEXT("Get pose at time %f succeed after retrying %d times."), Timestamp, RetryTimes);
#endif
		}
	}

	return bHasValidPose;
}

bool FGoogleARCoreMotionManager::GetPoseAtTime(EGoogleARCoreReferenceFrame TargetFrame, double Timestamp, FGoogleARCorePose& OutTangoPose, bool bIgnoreDisplayRotation)
{
#if PLATFORM_ANDROID
	TangoCoordinateFrameType Base = static_cast<TangoCoordinateFrameType>(BaseFrame);
	TangoCoordinateFrameType Target = static_cast<TangoCoordinateFrameType>(TargetFrame);
	TangoPoseData RawPose;
	TangoSupport_Rotation DisplayRotation = TANGO_SUPPORT_ROTATION_IGNORED;
	if (!bIgnoreDisplayRotation)
	{
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
			DisplayRotation = TANGO_SUPPORT_ROTATION_0;
			break;
		}
	}
	auto Engine = TANGO_SUPPORT_ENGINE_UNREAL;
	TangoErrorType PoseFetchResult;
	const TangoSupport_Rotation RequestedDisplayRotation = DisplayRotation;

	PoseFetchResult = TangoSupport_getPoseAtTime(Timestamp, Base, Target, Engine, Engine, DisplayRotation, &RawPose);

	if (PoseFetchResult != TANGO_SUCCESS)
	{
#if ENABLE_ARCORE_DEBUG_LOG
		UE_LOG(LogGoogleARCore, Error, TEXT("getPoseAtTime failed: timestamp %f, base %d, target %d"), Timestamp, Base, Target);
#endif
		return false;
	}

	if (RawPose.status_code != TANGO_POSE_VALID)
	{
#if ENABLE_ARCORE_DEBUG_LOG
		UE_LOG(LogGoogleARCore, Error, TEXT("getPoseAtTime returns invalid pose."));
#endif
		return false;
	}

	FTransform Result;
	Result = ConvertTangoPoseToTransform(&RawPose);

	OutTangoPose.Timestamp.TimestampValue = RawPose.timestamp;
	OutTangoPose.Pose = Result;
	return true;
#else
	return false;
#endif
}
