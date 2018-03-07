// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GoogleARCoreXRCamera.h"
#include "GoogleARCoreHMD.h"
#include "SceneView.h"

FGoogleARCoreXRCamera::FGoogleARCoreXRCamera(const FAutoRegister& AutoRegister, FGoogleARCoreHMD& InTangoSystem, int32 InDeviceID)
: FDefaultXRCamera(AutoRegister, &InTangoSystem, InDeviceID)
, TangoSystem(InTangoSystem)
{
}

void FGoogleARCoreXRCamera::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	TrackingSystem->GetCurrentPose(DeviceId, InView.BaseHmdOrientation, InView.BaseHmdLocation);
}

void FGoogleARCoreXRCamera::SetupViewProjectionMatrix(FSceneViewProjectionData& InOutProjectionData)
{
	if (TangoSystem.TangoDeviceInstance->GetIsTangoRunning() && TangoSystem.bARCameraEnabled)
	{
		FIntRect ViewRect = InOutProjectionData.GetViewRect();
		InOutProjectionData.ProjectionMatrix = TangoSystem.TangoDeviceInstance->TangoARCameraManager.CalculateColorCameraProjectionMatrix(ViewRect.Size());
	}
}

void FGoogleARCoreXRCamera::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (TangoSystem.TangoDeviceInstance->GetIsTangoRunning() && TangoSystem.bARCameraEnabled)
	{
		TangoSystem.TangoDeviceInstance->TangoARCameraManager.OnBeginRenderView();
	}
}

void FGoogleARCoreXRCamera::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	// Late Update Camera Poses
	if (TangoSystem.TangoDeviceInstance->GetIsTangoRunning() && TangoSystem.bLateUpdateEnabled && TangoSystem.bLateUpdatePoseIsValid)
	{
		const FTransform OldLocalCameraTransform(InView.BaseHmdOrientation, InView.BaseHmdLocation);
		const FTransform OldWorldCameraTransform(InView.ViewRotation, InView.ViewLocation);
		const FTransform CameraParentTransform = OldLocalCameraTransform.Inverse() * OldWorldCameraTransform;
		const FTransform NewWorldCameraTransform = TangoSystem.LateUpdatePose.Pose * CameraParentTransform;

		InView.ViewLocation = NewWorldCameraTransform.GetLocation();
		InView.ViewRotation = NewWorldCameraTransform.Rotator();
		InView.UpdateViewMatrix();
	}
}

void FGoogleARCoreXRCamera::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	FGoogleARCoreHMD& TS = TangoSystem;

	if (TS.TangoDeviceInstance->GetIsTangoRunning() && TS.bLateUpdateEnabled)
	{
		if (TS.bARCameraEnabled)
		{
			// If Ar camera enabled, we need to sync the camera pose with the camera image timestamp.
			TS.TangoDeviceInstance->TangoARCameraManager.LateUpdateColorCameraTexture_RenderThread();

			double Timestamp = TS.TangoDeviceInstance->TangoARCameraManager.GetColorCameraImageTimestamp();

			TS.bLateUpdatePoseIsValid = TS.TangoDeviceInstance->TangoMotionManager.GetPoseAtTime_Blocking(EGoogleARCoreReferenceFrame::CAMERA_COLOR, Timestamp, TS.LateUpdatePose);

			if (!TS.bLateUpdatePoseIsValid)
			{
				UE_LOG(LogGoogleARCoreHMD, Warning, TEXT("Failed to late update tango color camera pose at timestamp %f."), Timestamp);
			}
		}
		else
		{
			// If we are not using Ar camera mode, we just need to late update the camera pose to the latest tango device pose.
			TS.bLateUpdatePoseIsValid = TS.TangoDeviceInstance->TangoMotionManager.GetPoseAtTime(EGoogleARCoreReferenceFrame::DEVICE, 0, TS.LateUpdatePose);
		}

		if (TS.bLateUpdatePoseIsValid)
		{
			const FSceneView* MainView = InViewFamily.Views[0];
			const FTransform OldRelativeTransform(MainView->BaseHmdOrientation, MainView->BaseHmdLocation);

			// @todo viewext re-enable
			//ApplyLateUpdate(InViewFamily.Scene, OldRelativeTransform, LateUpdatePose.Pose);
		}
	}
}

void FGoogleARCoreXRCamera::PostRenderMobileBasePass_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	if (TangoSystem.TangoDeviceInstance->GetIsTangoRunning() && TangoSystem.bARCameraEnabled && TangoSystem.bColorCameraRenderingEnabled)
	{
		TangoSystem.TangoDeviceInstance->TangoARCameraManager.RenderColorCameraOverlay_RenderThread(RHICmdList, InView);
	}
}

bool FGoogleARCoreXRCamera::IsActiveThisFrame(class FViewport* InViewport) const
{
	return TangoSystem.IsHeadTrackingAllowed();
}
