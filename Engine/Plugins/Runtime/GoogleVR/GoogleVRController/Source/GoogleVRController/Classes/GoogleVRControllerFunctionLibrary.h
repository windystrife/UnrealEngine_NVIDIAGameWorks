// Copyright 2017 Google Inc.

#pragma once

#include "GoogleVRControllerEventManager.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GoogleVRControllerFunctionLibrary.generated.h"

UENUM(BlueprintType)
enum class EGoogleVRControllerAPIStatus : uint8
{
	// API is happy and healthy. This doesn't mean the controller itself
	// is connected, it just means that the underlying service is working
	// properly.
	OK = 0,
	// API failed because this device does not support controllers (API is too
	// low, or other required feature not present).
	Unsupported = 1,
	// This app was not authorized to use the service (e.g., missing permissions,
	// the app is blacklisted by the underlying service, etc).
	NotAuthorized = 2,
	// The underlying VR service is not present.
	Unavailable = 3,
	// The underlying VR service is too old, needs upgrade.
	ServiceObsolete = 4,
	// The underlying VR service is too new, is incompatible with current client.
	ClientObsolete = 5,
	// The underlying VR service is malfunctioning. Try again later.
	Malfunction = 6,
	// This means GoogleVRController plugin is not support on the platform.
	Unknown = 7,
};

UENUM(BlueprintType)
enum class EGoogleVRControllerHandedness : uint8
{
	RightHanded,
	LeftHanded,
	Unknown
};

// Represents when gaze-following behavior should occur in the ArmModel.
// This is useful if you have an application that requires the user to turn around.
UENUM(BlueprintType)
enum class EGoogleVRArmModelFollowGazeBehavior : uint8
{
	Never, // The shoulder will never follow the gaze.
	DuringMotion, // The shoulder will follow the gaze during controller motion.
	Always // The shoulder will always follow the gaze.
};

// Represents the controller battery level.
UENUM(BlueprintType)
enum class EGoogleVRControllerBatteryLevel : uint8
{
	Unknown = 0,
	CriticalLow = 1,
	Low = 2,
	Medium = 3,
	AlmostFull = 4,
	Full = 5,
};

/**
 * GoogleVRController Extensions Function Library
 */
UCLASS()
class GOOGLEVRCONTROLLER_API UGoogleVRControllerFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Get the GoogleVR Controller API status
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static EGoogleVRControllerAPIStatus GetGoogleVRControllerAPIStatus();

	/**
	 * Get the GoogleVR Controller state
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static EGoogleVRControllerState GetGoogleVRControllerState();

	/**
	 * Get user's handedness preference from GVRSDK
	 * @return A EGoogleVRControllerHandedness indicates the user's handedness preference in GoogleVR Settings.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static EGoogleVRControllerHandedness GetGoogleVRControllerHandedness();

	/**
	 * This function return the controller acceleration in gvr controller space.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static FVector GetGoogleVRControllerRawAccel();

	/**
	 * This function return the controller angular velocity about each axis (positive means clockwise when sighting along axis) in gvr controller space.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static FVector GetGoogleVRControllerRawGyro();

	/**
	 * This function return the orientation of the controller in unreal space.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static FRotator GetGoogleVRControllerOrientation();

	/**
	 * Return a pointer to the UGoogleVRControllerEventManager to hook up GoogleVR Controller specific event.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static UGoogleVRControllerEventManager* GetGoogleVRControllerEventManager();


	/** Below are functions for controlling the Arm Model.
	 *  The Arm Model is used to to create more realistic motion for the
	 *  GVR motion controller, by utilizing a joint based mathematical model with the
	 *  data retrieved from the controller. It is enabled by default.
	 */

	/** Determine if the arm model is enabled
	 *  @return true if the arm model is enabled
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static bool IsArmModelEnabled();

	/** Set the arm model enabled/disabled
	 *  @param bArmModelEnabled - value to set
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetArmModelEnabled(bool bArmModelEnabled);

	/** Returns the local position of the pointer in the unreal coordinate system relative to the motion controller.
	 *  The pointer is similar to the controller, except that it is slightly forward and rotated down by the
	 *  pointer tilt angle. This is used to create more ergonomic comfort when pointing at things.
	 *  This should be used for any reticle / laser implementation.
	 *  @return pointer position.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static FVector GetArmModelPointerPositionOffset();

	/** Get the elbow height used by the arm model in meters.
	 *  Used in the mathematical model for calculating the controller position/rotation.
	 *  @return user height.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static float GetArmModelAddedElbowHeight();

	/** Set the elbow height used by the arm model in meters.
	 *  Used in the mathematical model for calculating the controller position/rotation.
	 *  @param HeightMeters - value to set
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetArmModelAddedElbowHeight(float ElbowHeight);

	/** Get the elbow depth used by the arm model in meters.
	 *  Used in the mathematical model for calculating the controller position/rotation.
	 *  @return user height.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static float GetArmModelAddedElbowDepth();

	/** Set the elbow depth used by the arm model in meters.
	 *  Used in the mathematical model for calculating the controller position/rotation.
	 *  @param HeightMeters - value to set
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetArmModelAddedElbowDepth(float ElbowDepth);

	/** Get the pointer tilt angle.
	 *  @return degrees downward that the pointer tilts.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static float GetArmModelPointerTiltAngle();

	/** Set the pointer tilt angle.
	 *  Defaults to 15 degrees, which is comfortable for most use cases.
	 *  @param TiltAngle - value to set
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetArmModelPointerTiltAngle(float TiltAngle);

	/** Get gaze behavior
	 *  Gaze behavior determines if the controller should follow the player's gaze. Useful for games that require the
	 *  player to turn more than 90 degrees.
	 *  @return gaze behavior.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static EGoogleVRArmModelFollowGazeBehavior GetArmModelGazeBehavior();

	/** Set gaze behavior
	 *  Gaze behavior determines if the controller should follow the player's gaze. Useful for games that require the
	 *  player to turn more than 90 degrees.
	 *  @param GazeBehavior - value to set
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetArmModelGazeBehavior(EGoogleVRArmModelFollowGazeBehavior GazeBehavior);

	/** Get if the arm model will use accelerometer data
	 *  If this is turned on, then the arm model will estimate the position of the controller in space
	 *  using accelerometer data. This is useful when trying to make the player feel like they are moving
	 *  around a physical object. Not as useful when just interacting with UI.
	 *  @return true if accelerometer use is enabled
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static bool WillArmModelUseAccelerometer();

	/** Set if the arm model will use accelerometer data
	 *  If this is turned on, then the arm model will estimate the position of the controller in space
	 *  using accelerometer data. This is useful when trying to make the player feel like they are moving
	 *  around a physical object. Not as useful when just interacting with UI.
	 *  @param UseAccelerometer - value to set
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetWillArmModelUseAccelerometer(bool UseAccelerometer);

	/** Set if the Arm Model will be locked to the head Pose.
	* @param IsLockedToHead - value to set
	*/
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetArmModelIsLockedToHead(bool IsLockedToHead);

	/** Get if the Arm Model will be locked to the head Pose.
	* @return true if it is locked to the Head Pose
	*/
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static bool GetArmModelIsLockedToHead();

	/** Controller distance from the face after which the alpha value decreases (meters).
	 *  @return fade distance from face in meters.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static float GetFadeDistanceFromFace();

	/** Controller distance from the face after which the alpha value decreases (meters).
	 *  @param DistanceFromFace - value to set
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetFadeDistanceFromFace(float DistanceFromFace);

	/** Controller distance from the face after which the tooltips appear (meters).
	 *  @return tooltip mininum distance from face in meters.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static float GetTooltipMinDistanceFromFace();

	/** Controller distance from the face after which the tooltips appear (meters).
	 *  @param DistanceFromFace - value to set
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetTooltipMinDistanceFromFace(float DistanceFromFace);

	/** When the angle (degrees) between the controller and the head is larger than
	 *  this value, the tooltip disappears.
	 *  If the value is 180, then the tooltips are always shown.
	 *  If the value is 90, the tooltips are only shown when they are facing the camera.
	 *  @return tooltip max angle from camera in degrees.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static int GetTooltipMaxAngleFromCamera();

	/** When the angle (degrees) between the controller and the head is larger than
	 *  this value, the tooltip disappears.
	 *  If the value is 180, then the tooltips are always shown.
	 *  If the value is 90, the tooltips are only shown when they are facing the camera.
	 *  @param AngleFromCamera - value to set
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetTooltipMaxAngleFromCamera(int AngleFromCamera);

	/** Get the current desired alpha value of the controller visual.
	 *  This changes based on the FadeDistanceFromFace, and is used to prevent the controller
	 *  From clipping awkwardly into the user's face.
	 *  @return value between 0 and 1.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static float GetControllerAlphaValue();

	/** Get the current desired alpha value of the tooltip visual.
	 *  When the controller is farther than TooltipMinDistanceFromFace this becomes 0
	 *  When the controller is closer than FadeDistanceFromFace this becomes 0
	 *  This is used so that the tooltips are only visible when the controller is being held up.
	 *  @return value between 0 and 1.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static float GetTooltipAlphaValue();

	/** Get whether the controller battery is currently charging.
	 *  This may not be real time information and may be slow to be updated.
	 *  @return true if the battery is charging.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static bool GetBatteryCharging();

	/** Get the bucketed controller battery level.
	 *  Note this is an approximate level described by enumeration, not a percent.
	 *  @return the approximate battery level, or unknown if the level can not be determined.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static EGoogleVRControllerBatteryLevel GetBatteryLevel();

	/** Get the timestamp (nanos) when the last battery event was received.
	 *  @return the timestamp, or zero if unavailable.
	 */
	static int64_t GetLastBatteryTimestamp();

};
