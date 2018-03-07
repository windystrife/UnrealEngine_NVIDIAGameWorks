// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Features/IModularFeature.h"

class UPrimitiveComponent;
class AActor;

class HEADMOUNTEDDISPLAY_API IXRDeviceAssets : public IModularFeature
{
public:
	static FName GetModularFeatureName()
	{
		static FName FeatureName = FName(TEXT("XRDeviceAssets"));
		return FeatureName;
	}

	/**
	 * Fills out a device list with unique identifiers that can be used
	 * to reference system specific devices. 
	 *
	 * These IDs are intended to be used with certain methods to reference a 
	 * specific device (like with CreateRenderComponent(), etc.).
	 * NOTE: that these IDs are NOT interoperable across XR systems (vive vs. oculus, 
	 * etc.). Using an ID from one system with another will have undefined results. 
	 *
	 * @param  DeviceListOut	
	 *
	 * @return True if the DeviceList was successfully updated, otherwise false. 
	 */
	virtual bool EnumerateRenderableDevices(TArray<int32>& DeviceListOut) = 0;

	/**
	 * Attempts to spawn a renderable component for the specified device. Returns
	 * a component that needs to be attached and registered by the caller.
	 *
	 * NOTE: Resource loads for this component may be asynchronous. The 
	 * component can be attached and registered immediately, but there may be a 
	 * delay before it renders properly.
	 *
	 * @param  DeviceId		Uniquely identifies the XR device you want to render.
	 * @param  Owner		The actor which this component will be attached to.
	 * @param  Flags		Object creation flags to spawn the component with.
	 *
	 * @return A valid component pointer if the method succeeded, otherwise null.
	 */
	virtual UPrimitiveComponent* CreateRenderComponent(const int32 DeviceId, AActor* Owner, EObjectFlags Flags) = 0;
};
