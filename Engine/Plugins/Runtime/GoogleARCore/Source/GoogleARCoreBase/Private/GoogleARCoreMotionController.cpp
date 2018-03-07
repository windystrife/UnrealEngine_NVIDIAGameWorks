// Copyright 2017 Google Inc.

#include "GoogleARCoreMotionController.h"
#include "GoogleARCoreBaseLogCategory.h"

#include "GoogleARCoreMotionManager.h"
#include "GoogleARCoreDevice.h"

static bool bLastRenderThreadPoseWasValid = false;

FGoogleARCoreMotionController::FGoogleARCoreMotionController()
	: TangoDeviceInstance(nullptr)
{
	TangoDeviceInstance = FGoogleARCoreDevice::GetInstance();
}

void FGoogleARCoreMotionController::RegisterController()
{
	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
}

void FGoogleARCoreMotionController::UnregisterController()
{
	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
}

bool FGoogleARCoreMotionController::GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
{
	if (IsInGameThread())
	{
		FGoogleARCorePose OutPose;
		bool bIsValidPose = TangoDeviceInstance->TangoMotionManager.GetCurrentPose(EGoogleARCoreReferenceFrame::DEVICE, OutPose);

		OutOrientation = FRotator(OutPose.Pose.GetRotation());
		OutPosition = OutPose.Pose.GetTranslation();
		return bIsValidPose;
	}
	else // presumed render thread.
	{
		if(FGoogleARCoreDevice::GetInstance()->GetIsTangoRunning())
		{
			FGoogleARCorePose OutPose;
			bool bIsValidPose = TangoDeviceInstance->TangoMotionManager.GetPoseAtTime(EGoogleARCoreReferenceFrame::DEVICE, 0, OutPose);

			bLastRenderThreadPoseWasValid = bIsValidPose;

			OutOrientation = FRotator(OutPose.Pose.GetRotation());
			OutPosition = OutPose.Pose.GetTranslation();
			return bIsValidPose;
		}
		else
		{
			return false;
		}
	}
}

ETrackingStatus FGoogleARCoreMotionController::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
	bool isTracked;

	if (IsInGameThread())
	{
		isTracked = TangoDeviceInstance->TangoMotionManager.IsTrackingValid();
	}
	else
	{
		// TODO:
		// When called on the render thread, this assumes very blatantly that GetControllerOrientationAndPosition will be
		// called immediately preceeding this, as is the behaviour of UMotionControllerComponent::PollControllerState().
		//
		// The essential problem is how to get the 'most current and up to date' tracking during rendering but still use
		// the same pose or all associated calls to prevent inconsistencies.
		//
		// There is surely a cleaner way to do this.

		isTracked = bLastRenderThreadPoseWasValid;
	}

	return isTracked ? ETrackingStatus::Tracked : ETrackingStatus::NotTracked;
}

FName FGoogleARCoreMotionController::GetMotionControllerDeviceTypeName() const
{
	static const FName DeviceName(TEXT("TangoMotionController"));
	return DeviceName;
}
