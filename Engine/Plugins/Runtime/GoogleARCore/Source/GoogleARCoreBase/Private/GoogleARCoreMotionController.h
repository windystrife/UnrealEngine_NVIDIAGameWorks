// Copyright 2017 Google Inc.

#pragma once

#include "IMotionController.h"
#include "Features/IModularFeatures.h"


class FGoogleARCoreDevice;

class FGoogleARCoreMotionController : public IMotionController
{
public:
	FGoogleARCoreMotionController();

	void RegisterController();

	void UnregisterController();

	/**
	 * Returns the calibration-space orientation of the requested controller's hand.
	 *
	 * @param ControllerIndex	The Unreal controller (player) index of the contoller set
	 * @param DeviceHand		Which hand, within the controller set for the player, to get the orientation and position for
	 * @param OutOrientation	(out) If tracked, the orientation (in calibrated-space) of the controller in the specified hand
	 * @param OutPosition		(out) If tracked, the position (in calibrated-space) of the controller in the specified hand
	 * @return					True if the device requested is valid and tracked, false otherwise
	 */
	virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const;

	/**
	 * Returns the tracking status (e.g. not tracked, intertial-only, fully tracked) of the specified controller
	 *
	 * @return	Tracking status of the specified controller, or ETrackingStatus::NotTracked if the device is not found
	 */
	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const override;


	virtual FName GetMotionControllerDeviceTypeName() const override;

private:
	FGoogleARCoreDevice* TangoDeviceInstance;
};