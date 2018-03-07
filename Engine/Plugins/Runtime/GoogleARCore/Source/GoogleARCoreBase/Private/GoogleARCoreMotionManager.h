// Copyright 2017 Google Inc.

#pragma once
#include "GoogleARCorePrimitives.h"
#include "HAL/ThreadSafeBool.h"

class FGoogleARCoreMotionManager
{
	friend class FGoogleARCoreDevice;
public:
	FGoogleARCoreMotionManager();
	~FGoogleARCoreMotionManager();

	bool OnTrackingSessionStarted(EGoogleARCoreReferenceFrame PoseBaseFrame);

	void OnTrackingSessionStopped();

	bool IsTrackingValid();

	bool IsRelocalized();

	FGoogleARCoreTimestamp GetCurrentPoseTimestamp();

	bool GetCurrentPose(EGoogleARCoreReferenceFrame TargetFrame, FGoogleARCorePose& OutTangoPose);

	bool GetPoseAtTime(EGoogleARCoreReferenceFrame TargetFrame, double Timestamp, FGoogleARCorePose& OutTangoPose, bool bIgnoreDisplayRotation = false);

	/* This function will force to get the pose at the given timestamp by always retrying when failed, unless it didn't get the PoseAvailable signal for too long
	   Note that the function could block the caller thread so use caution.
	*/
	bool GetPoseAtTime_Blocking(EGoogleARCoreReferenceFrame TargetFrame, double Timestamp, FGoogleARCorePose& OutTangoPose, bool bIgnoreDisplayRotation = false);

#if PLATFORM_ANDROID
	FTransform ConvertTangoPoseToTransform(const struct TangoPoseData* TangoPose);
	void OnTangoPoseUpdated(const TangoPoseData* Pose);
#endif
private:

	void UpdateBaseFrame(EGoogleARCoreReferenceFrame InBaseFrame);

	void UpdateTangoPoses();

private:
	EGoogleARCoreReferenceFrame BaseFrame;

	FGoogleARCorePose LatestDevicePose;
	FGoogleARCorePose LatestColorCameraPose;

	bool bIsDevicePoseValid;
	bool bIsColorCameraPoseValid;
	bool bIsEcefPoseValid;
	FThreadSafeBool bWaitingForNewPose;
	FThreadSafeBool bIsRelocalized;
	FEvent* NewPoseAvailable;
};
