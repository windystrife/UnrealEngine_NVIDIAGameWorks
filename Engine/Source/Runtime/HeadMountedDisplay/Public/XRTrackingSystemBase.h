// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IXRTrackingSystem.h"
#include "IXRCamera.h"

/** 
 * Base utility class for implementations of the IXRTrackingSystem interface
 * Contains helpers and default implementation of most abstract methods, so final implementations only need to override features that they support.
 */
class HEADMOUNTEDDISPLAY_API FXRTrackingSystemBase : public IXRTrackingSystem
{
public:
	FXRTrackingSystemBase();
	virtual ~FXRTrackingSystemBase();

	/**
	 * Whether or not the system supports positional tracking (either via sensor or other means).
	 * The default implementation always returns false, indicating that only rotational tracking is supported.
	 */
	virtual bool DoesSupportPositionalTracking() const override { return false; }

	/**
	 * If the system currently has valid tracking positions. If not supported at all, returns false.
	 * Defaults to calling DoesSupportPositionalTracking();
	 */
	virtual bool HasValidTrackingPosition() override { return DoesSupportPositionalTracking(); }

	/**
	 * Get the count of tracked devices.
	 *
	 * @param Type Optionally limit the count to a certain type
	 * @return the count of matching tracked devices.
	 *
	 * The default implementation calls EnumerateTrackedDevices and returns the number of elements added to the array.
	 */
	virtual uint32 CountTrackedDevices(EXRTrackedDeviceType Type = EXRTrackedDeviceType::Any) override;

	/**
	 * Check current tracking status of a device.
	 * @param DeviceId the device to request status for.
	 * @return true if the system currently has valid tracking info for the given device ID.
	 *
	 * The default implementation returns the result of calling GetCurrentPose(DeviceId, ...), ignoring the returned pose.
	 */
	virtual bool IsTracking(int32 DeviceId) override;

	/**
	 * If the device id represents a tracking sensor, reports the frustum properties in game-world space of the sensor.
	 * @param DeviceId the device to request information for.
	 * @param OutOrientation The current orientation of the device.
	 * @param OutPosition The current position of the device.
	 * @param OutSensorProperties A struct containing the tracking sensor properties.
	 * @return true if the device tracking is valid and supports returning tracking sensor properties.
	 *
	 * The default implementation returns false for all device ids.
	 */
	virtual bool GetTrackingSensorProperties(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition, FXRSensorProperties& OutSensorProperties) override
	{
		return false;
	}

	/**
	 * Get the IXCamera instance for the given device.
	 * @param DeviceId the device the camera should track.
	 * @return a shared pointer to an IXRCamera.
	 *
	 * The default implementation only supports a single IXRCamera for the HMD Device, returning a FDefaultXRCamera instance.
	 */
	virtual TSharedPtr< class IXRCamera, ESPMode::ThreadSafe > GetXRCamera(int32 DeviceId = HMDDeviceId) override;

	/**
	 * Returns version string.
	 */
	virtual FString GetVersionString() const { return FString(TEXT("GenericHMD")); }

	virtual bool GetRelativeEyePose(int32 DeviceId, EStereoscopicPass Eye, FQuat& OutOrientation, FVector& OutPosition) override;

	/**
	 * Sets tracking origin (either 'eye'-level or 'floor'-level).
	 *
	 * The default implementations simply ignores the origin value.
	 */
	virtual void SetTrackingOrigin(EHMDTrackingOrigin::Type NewOrigin) override {}

	/**
	 * Returns current tracking origin.
	 *
	 * The default implementation always reports 'eye'-level tracking.
	 */
	virtual EHMDTrackingOrigin::Type GetTrackingOrigin() override
	{
		return EHMDTrackingOrigin::Eye;
	}

protected:

	/**
	 * This method should return the world to meters scale for the current frame.
	 * Should be callable on both the render and the game threads.
	 * @return the current world to meter scale.
	 */
	virtual float GetWorldToMetersScale() const =0;

	
	TSharedPtr< class FDefaultXRCamera, ESPMode::ThreadSafe > XRCamera;
};

