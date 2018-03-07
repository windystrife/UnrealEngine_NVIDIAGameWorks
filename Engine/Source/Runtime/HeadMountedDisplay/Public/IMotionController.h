// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Features/IModularFeature.h"

enum class EControllerHand : uint8;

UENUM(BlueprintType)
enum class ETrackingStatus : uint8
{
	NotTracked,
	InertialOnly,
	Tracked
};

/**
 * Motion Controller device interface
 *
 * NOTE:  This intentionally does NOT derive from IInputDeviceModule, to allow a clean separation for devices which exclusively track motion with no tactile input
 * NOTE:  You must MANUALLY call IModularFeatures::Get().RegisterModularFeature( GetModularFeatureName(), this ) in your implementation!  This allows motion controllers
 *			to be both piggy-backed off HMD devices which support them, as well as standing alone.
 */

class HEADMOUNTEDDISPLAY_API IMotionController : public IModularFeature
{
public:
	static FName GetModularFeatureName()
	{
		static FName FeatureName = FName(TEXT("MotionController"));
		return FeatureName;
	}

	/**
	* Returns the device type of the controller.
	*
	* @return	Device type of the controller.
	*/
	virtual FName GetMotionControllerDeviceTypeName() const = 0;

	/**
	 * Returns the calibration-space orientation of the requested controller's hand.
	 *
	 * @param ControllerIndex	The Unreal controller (player) index of the contoller set
	 * @param DeviceHand		Which hand, within the controller set for the player, to get the orientation and position for
	 * @param OutOrientation	(out) If tracked, the orientation (in calibrated-space) of the controller in the specified hand
	 * @param OutPosition		(out) If tracked, the position (in calibrated-space) of the controller in the specified hand
	 * @param WorldToMetersScale The world scaling factor.
	 *
	 * @return					True if the device requested is valid and tracked, false otherwise
	 */
	virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const = 0;

	/**
	 * Returns the tracking status (e.g. not tracked, intertial-only, fully tracked) of the specified controller
	 *
	 * @return	Tracking status of the specified controller, or ETrackingStatus::NotTracked if the device is not found
	 */
	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const = 0;
};
