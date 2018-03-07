// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h" // for FName
#include "Templates/TypeHash.h" // for HashCombine()


class HEADMOUNTEDDISPLAY_API IXRSystemIdentifier
{
public:
	/**
	 * Returns a unique identifier that's supposed to represent the third party 
	 * system that this object is part of (Vive, Oculus, PSVR, GearVR, etc.).
	 *
	 * @return  A name unique to the system which this object belongs to.
	 */
	virtual FName GetSystemName() const = 0;
};

/** 
 * Generic device identifier interface
 *
 * This class is meant to provide a way to identify and distinguish
 * XR devices across various XR systems in a platform-agnostic way. 
 *
 * Additionally, it can be used to tie various IModularFeature device interfaces 
 * together. For example, if you have separate IMotionController and IXRDeviceAssets
 * interfaces which both reference the same devices, then this base class gives 
 * you a way to communicate between the two.
 */
class HEADMOUNTEDDISPLAY_API IIdentifiableXRDevice : public IXRSystemIdentifier
{
public:
	/**
	 * Returns a unique identifier that can be used to reference this device 
	 * within the system is belongs to.
	 *
	 * @return  A numerical identifier, unique to this device (not guaranteed to be unique outside of the system this belongs to).
	 */
	virtual int32 GetSystemDeviceId() const = 0;

	/** Combines the different aspects of IIdentifiableXRDevice to produce a unique identifier across all XR systems */
	friend uint32 GetTypeHash(const IIdentifiableXRDevice& XRDevice)
	{
		const uint32 DomainHash = GetTypeHash(XRDevice.GetSystemName());
		const uint32 DeviceHash = GetTypeHash(XRDevice.GetSystemDeviceId());
		return HashCombine(DomainHash, DeviceHash);
	}
};
