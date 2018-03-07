// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IAndroidDeviceDetection.h: Declares the IAndroidDeviceDetection interface.
=============================================================================*/

#pragma once

#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"

template<typename KeyType,typename ValueType,typename SetAllocator ,typename KeyFuncs > class TMap;

struct FAndroidDeviceInfo
{
	// Device serial number, used to route ADB commands to a specific device
	FString SerialNumber;

	// Device model name
	FString Model;

	// Device name
	FString DeviceName;

	// User-visible version of android installed (ro.build.version.release)
	FString HumanAndroidVersion;

	// Android SDK version supported by the device (ro.build.version.sdk - note: deprecated in 4 according to docs, but version 4 devices return an empty string when querying the 'replacement' SDK_INT)
	int32 SDKVersion;

	// List of supported OpenGL extensions (retrieved via SurfaceFlinger)
	FString GLESExtensions;

	// Supported GLES version (ro.opengles.version)
	int32 GLESVersion;

	// Is the device authorized for USB communication?  If not, then none of the other properties besides the serial number will be valid
	bool bAuthorizedDevice;

	// TCP port number on our local host forwarded over adb to the device
	uint16 HostMessageBusPort;

	FAndroidDeviceInfo()
		: SDKVersion(INDEX_NONE)
		, GLESVersion(INDEX_NONE)
		, bAuthorizedDevice(true)
		, HostMessageBusPort(0)
	{
	}
};


/**
 * Interface for AndroidDeviceDetection module.
 */
class IAndroidDeviceDetection
{
public:
	virtual const TMap<FString,FAndroidDeviceInfo>& GetDeviceMap() = 0;
	virtual FCriticalSection* GetDeviceMapLock() = 0;
	virtual void UpdateADBPath() = 0;
	virtual FString GetADBPath() = 0;
protected:

	/**
	 * Virtual destructor
	 */
	virtual ~IAndroidDeviceDetection() { }
};
