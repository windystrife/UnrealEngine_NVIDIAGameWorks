// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DefaultXRCamera.h"
#include "GameFramework/PlayerController.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneViewport.h"
#include "StereoRendering.h"
#include "StereoRenderTargetManager.h"
#include "IHeadMountedDisplay.h"

FDefaultXRCamera::FDefaultXRCamera(const FAutoRegister& AutoRegister, IXRTrackingSystem* InTrackingSystem, int32 InDeviceId)
	: FSceneViewExtensionBase(AutoRegister)
	, TrackingSystem(InTrackingSystem)
	, DeviceId(InDeviceId)
	, DeltaControlRotation(0, 0, 0)
	, DeltaControlOrientation(FQuat::Identity)
	, bUseImplicitHMDPosition(false)
{
}

void FDefaultXRCamera::ApplyHMDRotation(APlayerController* PC, FRotator& ViewRotation)
{
	ViewRotation.Normalize();
	FQuat DeviceOrientation;
	FVector DevicePosition;
	TrackingSystem->GetCurrentPose(DeviceId, DeviceOrientation, DevicePosition);

	const FRotator DeltaRot = ViewRotation - PC->GetControlRotation();
	DeltaControlRotation = (DeltaControlRotation + DeltaRot).GetNormalized();

	// Pitch from other sources is never good, because there is an absolute up and down that must be respected to avoid motion sickness.
	// Same with roll.
	DeltaControlRotation.Pitch = 0;
	DeltaControlRotation.Roll = 0;
	DeltaControlOrientation = DeltaControlRotation.Quaternion();

	ViewRotation = FRotator(DeltaControlOrientation * DeviceOrientation);
}

bool FDefaultXRCamera::UpdatePlayerCamera(FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	FQuat DeviceOrientation;
	FVector DevicePosition;
	if (!TrackingSystem->GetCurrentPose(DeviceId, DeviceOrientation, DevicePosition))
	{
		return false;
	}

	if (GEnableVREditorHacks && !bUseImplicitHMDPosition)
	{
		DeltaControlOrientation = CurrentOrientation;
		DeltaControlRotation = DeltaControlOrientation.Rotator();
	}

	CurrentPosition = DevicePosition;
	CurrentOrientation = DeviceOrientation;

	return true;
}

void FDefaultXRCamera::OverrideFOV(float& InOutFOV)
{
	// The default camera does not override the FOV
}

void FDefaultXRCamera::SetupLateUpdate(const FTransform& ParentToWorld, USceneComponent* Component)
{
	LateUpdate.Setup(ParentToWorld, Component);
}

void FDefaultXRCamera::CalculateStereoCameraOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, FVector& ViewLocation)
{
	if (StereoPassType != eSSP_FULL)
	{
		FQuat EyeOrientation;
		FVector EyeOffset;
		if (TrackingSystem->GetRelativeEyePose(DeviceId, StereoPassType, EyeOrientation, EyeOffset))
		{
			ViewLocation += ViewRotation.Quaternion().RotateVector(EyeOffset);
			ViewRotation = FRotator(ViewRotation.Quaternion() * EyeOrientation);

			if (!bUseImplicitHMDPosition)
			{
				FQuat DeviceOrientation; // Unused
				FVector DevicePosition;
				TrackingSystem->GetCurrentPose(DeviceId, DeviceOrientation, DevicePosition);
				ViewLocation += DeltaControlOrientation.RotateVector(DevicePosition);
			}
		}
		
	}
}

static const FName DayDreamHMD(TEXT("FGoogleVRHMD"));

void FDefaultXRCamera::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& View)
{
	check(IsInRenderingThread());

	// Disable late update for day dream, their compositor doesn't support it.
	const bool bDoLateUpdate = (TrackingSystem->GetSystemName() != DayDreamHMD);

	if (bDoLateUpdate)
	{
		FQuat DeviceOrientation;
		FVector DevicePosition;
		TrackingSystem->GetCurrentPose(DeviceId, DeviceOrientation, DevicePosition);
		const FQuat DeltaOrient = View.BaseHmdOrientation.Inverse() * DeviceOrientation;
		View.ViewRotation = FRotator(View.ViewRotation.Quaternion() * DeltaOrient);

		if (bUseImplicitHMDPosition)
		{
			const FQuat LocalDeltaControlOrientation = View.ViewRotation.Quaternion() * DeviceOrientation.Inverse();
			const FVector DeltaPosition = DevicePosition - View.BaseHmdLocation;
			View.ViewLocation += LocalDeltaControlOrientation.RotateVector(DeltaPosition);
		}

		View.UpdateViewMatrix();
	}
}

void FDefaultXRCamera::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	auto HMD = TrackingSystem->GetHMDDevice();
	if (HMD)
	{
		HMD->BeginRendering_GameThread();
	}
}

void FDefaultXRCamera::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());
	const FSceneView* MainView = ViewFamily.Views[0];

	FQuat NewOrientation;
	FVector NewPosition;
	TrackingSystem->RefreshPoses();
	TrackingSystem->GetCurrentPose(DeviceId, NewOrientation, NewPosition);

	const FTransform OldRelativeTransform(MainView->BaseHmdOrientation, MainView->BaseHmdLocation);
	const FTransform NewRelativeTransform(NewOrientation, NewPosition);

	LateUpdate.Apply_RenderThread(ViewFamily.Scene, OldRelativeTransform, NewRelativeTransform);
	auto HMD = TrackingSystem->GetHMDDevice();
	if (HMD)
	{
		HMD->BeginRendering_RenderThread(NewRelativeTransform, RHICmdList, ViewFamily);
	}
}

void FDefaultXRCamera::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	static const auto CVarAllowMotionBlurInVR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.AllowMotionBlurInVR"));
	const bool allowMotionBlur = (CVarAllowMotionBlurInVR && CVarAllowMotionBlurInVR->GetValueOnAnyThread() != 0);
	auto HMD = TrackingSystem->GetHMDDevice();
	InViewFamily.EngineShowFlags.MotionBlur = allowMotionBlur;
	InViewFamily.EngineShowFlags.HMDDistortion = HMD?HMD->GetHMDDistortionEnabled():false;
	InViewFamily.EngineShowFlags.StereoRendering = bCurrentFrameIsStereoRendering;
}

void FDefaultXRCamera::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	FQuat DeviceOrientation;
	FVector DevicePosition;
	TrackingSystem->GetCurrentPose(DeviceId, DeviceOrientation, DevicePosition);

	InView.BaseHmdOrientation = DeviceOrientation;
	InView.BaseHmdLocation = DevicePosition;
	auto StereoRendering = TrackingSystem->GetStereoRenderingDevice();
	auto RenderTargetManager = StereoRendering.IsValid() ? StereoRendering->GetRenderTargetManager() : nullptr;
	InViewFamily.bUseSeparateRenderTarget = RenderTargetManager ? RenderTargetManager->ShouldUseSeparateRenderTarget() : false;
}

bool FDefaultXRCamera::IsActiveThisFrame(class FViewport* InViewport) const
{
	bCurrentFrameIsStereoRendering = GEngine && GEngine->IsStereoscopic3D(InViewport); // The current viewport might disallow stereo rendering. Save it so we'll use the correct value in SetupViewFamily.
	return TrackingSystem->IsHeadTrackingAllowed();
}
