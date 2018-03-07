// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Type definition for shared pointers to instances of FIOSDeviceHelper
 */
typedef TSharedPtr<class FIOSDeviceHelper, ESPMode::ThreadSafe> FIOSDeviceHelperPtr;

/**
 * Type definition for shared references to instances of FIOSDeviceHelper
 */
typedef TSharedRef<class FIOSDeviceHelper, ESPMode::ThreadSafe> FIOSDeviceHelperRef;

/**
 * Delegate type for devices being connected or disconnected from the machine
 *
 * The first parameter is newly added or removed device
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDeviceConnectEvent, const FIOSLaunchDaemonPong&)

/**
 * Implemented the iOS device helper class
 */
class FIOSDeviceHelper
{
public:
    static void Initialize(bool bIsTVOS);
    
    /**
     * Returns a delegate that is executed when a device is connected
     *
     * @return the delegate
     */
    static FOnDeviceConnectEvent& OnDeviceConnected()
    {
        static FOnDeviceConnectEvent DeviceConnectedDelegate;
        return DeviceConnectedDelegate;
    }
    
    /**
     * Returns a delegate the is executed when a device is disconnected
     *
     * @return the delegate
     */
    static FOnDeviceConnectEvent& OnDeviceDisconnected()
    {
        static FOnDeviceConnectEvent DeviceDisconnectedDelegate;
        return DeviceDisconnectedDelegate;
    }
    
    /**
     * Installs an ipa on to the device
     */
    static bool InstallIPAOnDevice(const FTargetDeviceId& DeviceId, const FString& IPAPath);
    
	/**
	 * Suspends/Enables the device connect/disconnect thread
	 */
	static void EnableDeviceCheck(bool OnOff);

private:
    static void DeviceCallback(void*);
    static void DoDeviceConnect(void*);
    static void DoDeviceDisconnect(void*);
	static bool MessageTickDelegate(float);
};
