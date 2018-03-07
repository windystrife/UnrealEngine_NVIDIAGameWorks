// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HeadMountedDisplayTypes.h"
#include "IIdentifiableXRDevice.h"
#include "UObject/ObjectMacros.h"
#include "Features/IModularFeature.h"
#include "Engine/GameViewportClient.h"
#include "IXRInput.h"

class IXRCamera;

enum class EXRTrackedDeviceType
{
	/** Represents a head mounted display */
	HeadMountedDisplay,
	/** Represents a controller */
	Controller,
	/** Represents a static tracking reference device, such as a Lighthouse or tracking camera */
	TrackingReference,
	/** Misc. device types, for future expansion */
	Other,
	/** DeviceId is invalid */
	Invalid = -2,
	/** Pass to EnumerateTrackedDevices to get all devices regardless of type */
	Any = -1,
};

/**
 * Struct representing the properties of an external tracking sensor.
 */
struct FXRSensorProperties
{
	/** The field of view of the sensor to the left in degrees */
	float LeftFOV;
	/** The field of view of the sensor to the right in degrees */
	float RightFOV;
	/** The upwards field of view of the sensor in degrees */
	float TopFOV;
	/** The downwards field of view of the sensor in degrees */
	float BottomFOV;
	/** The near plane of the sensor's effective tracking area */
	float NearPlane; 
	/** The far plane of the sensor's effective tracking area */
	float FarPlane;

	/** The focal distance of the camera. Can be zero if this does not make sense for the type of tracking sensor. */
	float CameraDistance;
};


/**
 * Main access point to an XR tracking system. Use it to enumerate devices and query their poses.
 */
class HEADMOUNTEDDISPLAY_API  IXRTrackingSystem : public IModularFeature, public IXRSystemIdentifier
{
public:
	static FName GetModularFeatureName()
	{
		static const FName FeatureName = FName(TEXT("XRTrackingSystem"));
		return FeatureName;
	}

	/**
	 * Returns version string.
	 */
	virtual FString GetVersionString() const = 0;

	/**
	 * Device id 0 is reserved for an HMD. This should represent the HMD or the first HMD in case multiple HMDs are supported.
	 * Other devices can have arbitrary ids defined by each system.
	 * If a tracking system does not support tracking HMDs, device ID zero should be treated as invalid.
	 */
	static const int32 HMDDeviceId = 0;

	/**
	 * Whether or not the system supports positional tracking (either via sensor or other means)
	 */
	virtual bool DoesSupportPositionalTracking() const = 0;

	/**
	 * If the system currently has valid tracking positions. If not supported at all, returns false.
	 */
	virtual bool HasValidTrackingPosition() = 0;

	/**
	 * Reports all devices currently available to the system, optionally limiting the result to a given class of devices.
	 *
	 * @param OutDevices The device ids of available devices will be appended to this array.
	 * @param Type Optionally limit the list of devices to a certain type.
	 */
	virtual bool EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type = EXRTrackedDeviceType::Any) = 0;

	/**
	 * Get the count of tracked devices
	 * @param Type Optionally limit the count to a certain type
	 * @return the count of matching tracked devices
	 */
	virtual uint32 CountTrackedDevices(EXRTrackedDeviceType Type = EXRTrackedDeviceType::Any) = 0;

	/**
	 * Check current tracking status of a device.
	 * @param DeviceId the device to request status for.
	 * @return true if the system currently has valid tracking info for the given device ID.
	 */
	virtual bool IsTracking(int32 DeviceId) = 0;

	/**
	 * Refresh poses. Tells the system to update the poses for its tracked devices.
	 * May be called both from the game and the render thread.
	 */
	virtual void RefreshPoses() = 0;

	/** 
	 * Temporary method until Morpheus controller code has been refactored.
	 */
	virtual void RebaseObjectOrientationAndPosition(FVector& Position, FQuat& Orientation) const {};

	/** 
	 * Get the current pose for a device.
	 * This method must be callable both on the render thread and the game thread.
	 * For devices that don't support positional tracking, OutPosition will be at the base position.
	 *
	 * @param DeviceId the device to request the pose for.
	 * @param OutOrientation The current orientation of the device
	 * @param OutPosition The current position of the device
	 * @return true if the pose is valid or not.
	 */
	virtual bool GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition) = 0;

	/** 
	 * If the device id represents a head mounted display, fetches the relative position of the given eye relative to the eye.
	 * If the device is does not represent a stereoscopic tracked camera, orientation and position should be identity and zero and the return value should be false.
	 *
	 * @param DeviceId the device to request the eye pose for.
	 * @param Eye the eye the pose should be requested for, if passing in any other value than eSSP_LEFT_EYE or eSSP_RIGHT_EYE, the method should return a zero offset.
	 * @param OutOrientation The orientation of the eye relative to the device orientation.
	 * @param OutPosition The position of the eye relative to the tracked device
	 * @return true if the pose is valid or not. If the device is not a stereoscopic device, return false.
	 */
	virtual bool GetRelativeEyePose(int32 DeviceId, EStereoscopicPass Eye, FQuat& OutOrientation, FVector& OutPosition) = 0;

	/** 
	 * If the device id represents a tracking sensor, reports the frustum properties in game-world space of the sensor.
	 * @param DeviceId the device to request information for.
	 * @param OutOrientation The current orientation of the device.
	 * @param OutPosition The current position of the device.
	 * @param OutSensorProperties A struct containing the tracking sensor properties.
	 * @return true if the device tracking is valid and supports returning tracking sensor properties.
	 */
	virtual bool GetTrackingSensorProperties(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition, FXRSensorProperties& OutSensorProperties) = 0;

	/**
	 * Sets tracking origin (either 'eye'-level or 'floor'-level).
	 */
	virtual void SetTrackingOrigin(EHMDTrackingOrigin::Type NewOrigin) = 0;

	/**
	 * Returns current tracking origin.
	 */
	virtual EHMDTrackingOrigin::Type GetTrackingOrigin() = 0;

	/**
	 * Get the offset, in device space, of the reported device (screen / eye) position to the center of the head.
	 *
	 * @return a vector containing the offset coordinates, ZeroVector if not supported.
	 */
	virtual FVector GetAudioListenerOffset(int32 DeviceId = HMDDeviceId) const { return FVector::ZeroVector; }

	/**
	* Resets orientation by setting roll and pitch to 0, assuming that current yaw is forward direction and assuming
	* current position as a 'zero-point' (for positional tracking).
	*
	* @param Yaw				(in) the desired yaw to be set after orientation reset.
	*/
	virtual void ResetOrientationAndPosition(float Yaw = 0.f) = 0;

	/**
	* Resets orientation by setting roll and pitch to 0, assuming that current yaw is forward direction. Position is not changed.
	*
	* @param Yaw				(in) the desired yaw to be set after orientation reset.
	*/
	virtual void ResetOrientation(float Yaw = 0.f) {}

	/**
	* Resets position, assuming current position as a 'zero-point'.
	*/
	virtual void ResetPosition() {}

	/**
	* Sets base orientation by setting yaw, pitch, roll, assuming that this is forward direction.
	* Position is not changed.
	*
	* @param BaseRot			(in) the desired orientation to be treated as a base orientation.
	*/
	virtual void SetBaseRotation(const FRotator& BaseRot) {}

	/**
	* Returns current base orientation of HMD as yaw-pitch-roll combination.
	*/
	virtual FRotator GetBaseRotation() const { return FRotator::ZeroRotator; }

	/**
	* Sets base orientation, assuming that this is forward direction.
	* Position is not changed.
	*
	* @param BaseOrient		(in) the desired orientation to be treated as a base orientation.
	*/
	virtual void SetBaseOrientation(const FQuat& BaseOrient) {}

	/**
	* Returns current base orientation of HMD as a quaternion.
	*/
	virtual FQuat GetBaseOrientation() const { return FQuat::Identity; }

	/** 
	 * Get the IXCamera instance for the given device.
	 *
	 * @param DeviceId the device the camera should track.
	 * @return a shared pointer to an IXRCamera.
	 */
	virtual class TSharedPtr< class IXRCamera, ESPMode::ThreadSafe > GetXRCamera(int32 DeviceId = HMDDeviceId) = 0;

	/** 
	 * Access HMD rendering-related features.
	 *
	 * @return a IHeadmountedDisplay pointer or a nullptr if this tracking system does not support head mounted displays.
	 */
	virtual class IHeadMountedDisplay* GetHMDDevice() { return nullptr; }

	/**
	* Access Stereo rendering device associated with this XR system.
	* If GetHMDDevice() returns non-null, this method should also return a vaild instance.
	*
	* @return a IStereoRendering pointer or a nullptr if this tracking system does not support stereo rendering.
	*/
	virtual class TSharedPtr< class IStereoRendering, ESPMode::ThreadSafe > GetStereoRenderingDevice()
	{
		check(GetHMDDevice() == nullptr);
		return nullptr; 
	}

	/**
	 * Access optional HMD input override interface.
	 *
	 * @return a IXRInput pointer or a nullptr if not supported
	 */
	virtual IXRInput* GetXRInput() { return nullptr; }

	/*** XR System related methods moved from IHeadMontedDisplay ***/

	/**
	* Returns true, if head tracking is allowed. Most common case: it returns true when GEngine->IsStereoscopic3D() is true,
	* but some overrides are possible.
	*/
	virtual bool IsHeadTrackingAllowed() const = 0;

	/**
	* This method is called when playing begins. Useful to reset all runtime values stored in the plugin.
	*/
	virtual void OnBeginPlay(FWorldContext& InWorldContext) {}

	/**
	* This method is called when playing ends. Useful to reset all runtime values stored in the plugin.
	*/
	virtual void OnEndPlay(FWorldContext& InWorldContext) {}

	/**
	* This method is called when new game frame begins (called on a game thread).
	*/
	virtual bool OnStartGameFrame(FWorldContext& WorldContext) { return false; }

	/**
	* This method is called when game frame ends (called on a game thread).
	*/
	virtual bool OnEndGameFrame(FWorldContext& WorldContext) { return false; }
};
