// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class ITargetDeviceProxy;

struct FGuid;


/** Type definition for shared pointers to instances of ITargetDeviceProxy. */
DEPRECATED(4.16, "ITargetDeviceProxyPtr is deprecated. Please use 'TSharedPtr<ITargetDeviceProxy>' instead!")
typedef TSharedPtr<class ITargetDeviceProxy> ITargetDeviceProxyPtr;

/** Type definition for shared references to instances of ITargetDeviceProxy. */
DEPRECATED(4.16, "ITargetDeviceProxyRef is deprecated. Please use 'TSharedPtr<ITargetDeviceProxy>' instead!")
typedef TSharedRef<class ITargetDeviceProxy> ITargetDeviceProxyRef;


/**
 * Delegate type for committed build deployments.
 *
 * The first parameter is the transaction identifier of the completed deployment.
 * The second parameter is the identifier of the deployed application (used for launching).
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTargetDeviceProxyDeployCommitted, const FGuid&, const FString&)

/**
 * Delegate type for failed build deployments.
 *
 * The first parameter is the transaction identifier of the failed deployment.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTargetDeviceProxyDeployFailed, const FGuid&)

/**
 * Delegate type for failed build launches.
 *
 * The first parameter is the application identifier of the failed launch.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTargetDeviceProxyLaunchFailed, const FString&)

/**
 * Delegate type for successful build launches.
 *
 * The first parameter is the application identifier of the successful launch.
 * The second parameter is the process identifier of the launched application.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTargetDeviceProxyLaunchSucceeded, const FString&, uint32)


/**
 * Interface for target device proxies.
 */
class ITargetDeviceProxy
{
public:

	/**
	 * Checks whether the device can launch multiple games or applications simultaneously.
	 *
	 * @return true if launching multiple instances is supported, false otherwise.
	 */
	virtual const bool CanMultiLaunch() const = 0;

	/**
	 * Checks whether this device can be powered off remotely.
	 *
	 * @return true if the device can be powered off, false otherwise.
	 */
	virtual bool CanPowerOff() const = 0;

	/**
	 * Checks whether this device can be powered on remotely.
	 *
	 * @return true if the device can be powered on, false otherwise.
	 */
	virtual bool CanPowerOn() const = 0;

	/**
	 * Checks whether this device can be rebooted remotely.
	 *
	 * @return true if the device can be rebooted, false otherwise.
	 */
	virtual bool CanReboot() const = 0;

	/**
	 * Checks whether this device's target platform supports variants.
	 *
	 * @return true if the device's target platform supports variants, false otherwise.
	 */
	virtual bool CanSupportVariants() const = 0;

	/**
	 * Gets the number of variants this device supports
	 *
	 * @return The number of variants supported.
	 */
	virtual int32 GetNumVariants() const = 0;

	/**
	 * Gets the list of variants this device supports
	 *
	 * @param OutVariants This list will be populated with the unique variant names. 
	 * @return The number of variants in the list.
	 */
	virtual int32 GetVariants(TArray<FName>& OutVariants) const = 0;

	/**
	 * Checks whether this device proxy contains a variant
	 *
	 * @param InVariant Variant to check for.
	 * @return true is this variant is found.
	 */
	virtual bool HasVariant(FName InVariant) const = 0;

	/**
	 * Gets the variant name of the target device.
	 *
	 * @param InDeviceId target device id to get the variant for.
	 * @return The target device variant name.
	 */
	virtual FName GetTargetDeviceVariant(const FString& InDeviceId) const = 0;

	/**
	 * Checks whether this device proxy contains a variant for the provided target device.
	 *
	 * @param InDeviceId DeviceId to check for a variant.
	 * @return true if this device has a variant of the provided DeviceId.
	 */
	virtual bool HasDeviceId(const FString& InDeviceId) const = 0;

	/**
	 * Gets the identifier of the device.
	 *
	 * @param InVariant name of the variant to return the device identifier for, If InVariant == NAME_None you'll get back the default target device identifier.
	 * @return The target device identifier.
	 */
	virtual const FString& GetTargetDeviceId(FName InVariant) const = 0;

	/**
	* Checks whether this device proxy contains a variant for the provided platform.
	*
	* @param InTargetPlatformName Target Platform Name for the target device to check for as a variant of this device.
	* @return true if this device has a variant of the provided Target Platform .
	*/
	virtual bool HasTargetPlatform(FName InTargetPlatformId) const = 0;

	/**
	 * Gets the target platform of the device variant.
	 *
	 * @param InVariant Variant to get the platform name for.
	 * @return Target Platform Name string.
	 */
	virtual FString GetTargetPlatformName(FName InVariant) const = 0;

	/**
	 * Gets the target platform of the device variant.
	 *
	 * @param InVariant Variant to get the platform id for.
	 * @return Target Platform Id.
	 */
	virtual FName GetTargetPlatformId(FName InVariant) const = 0;

	/**
	 * Gets the Vanilla platform of the device variant.
	 *
	 * @param InVariant Variant to get the platform id for.
	 * @return Vanilla Platform Id.
	 */
	virtual FName GetVanillaPlatformId(FName InVariant) const = 0;

	/**
	 * Gets the Vanilla platform of the device variant.
	 *
	 * @param InVariant Variant to get the vanilla platform name for.
	 * @return Platform Display Name string.
	 */
	virtual FText GetPlatformDisplayName(FName InVariant) const = 0;

	/**
	 * Gets the name of the host machine that claimed the device.
	 *
	 * @return Host name string.
	 */
	virtual const FString& GetHostName() const = 0;
	 
	/**
	 * Gets the name of the user that claimed the device.
	 *
	 * @return User name.
	 */
	virtual const FString& GetHostUser() const = 0;

	/**
	 * Gets the name of the (device's) user that is logged in on a device.
	 *
	 * @return Device user name.
	 */
	virtual const FString& GetDeviceUser() const = 0;

	/**
	 * Gets the password of the (device's) user that is logged in on a device.
	 *
	 * @return Device user password.
	 */
	virtual const FString& GetDeviceUserPassword() const = 0;

	/**
	 * Gets the device make (i.e. Apple or Sony).
	 *
	 * @return Device make.
	 */
	virtual const FString& GetMake() const = 0;

	/**
	 * Gets the device model (i.e. PS3 or XBox).
	 *
	 * @return Device model.
	 */
	virtual const FString& GetModel() const = 0;
	 
	/**
	 * Gets the name of the device (i.e. network name or IP address).
	 *
	 * @return Device name string.
	 */
	virtual const FString& GetName() const = 0;

	/**
	 * Gets the device type (i.e. Console, PC or Mobile).
	 *
	 * @return Device type.
	 */
	virtual const FString& GetType() const = 0;

	/**
	 * Checks whether the device is currently connected.
	 *
	 * @return true if the device is connected, false otherwise.
	 */
	virtual bool IsConnected() const = 0;

	/**
	 * Checks whether this device is being shared with other users.
	 *
	 * @return true if the device is being shared, false otherwise.
	 */
	virtual bool IsShared() const = 0;

public:

	/**
	 * Deploys the specified list of files to the device.
	 *
	 * If the deployment succeeds, the OnDeployCommitted() delegate will return an
	 * application identifier (AppId) that can be used to launch to the deployed
	 * application. If deploymend fails, the OnDeployFailed() delegate will be invoked.
	 *
	 * @param InVariant Variant to deploy files on.
	 * @param Files The files to deploy.
	 * @param TransactionId The transaction identifier for the deployment.
	 * @return true if deployment has been started, false otherwise.
	 * @see Launch, OnDeployCommitted, OnDeployFailed
	 */
	virtual bool DeployApp(FName InVariant, const TMap<FString, FString>& Files, const FGuid& TransactionId) = 0;

	/**
	 * Launches a previously deployed build on the target device.
	 *
	 * If the launching fails, the OnLaunchFailed() delegate will be invoked.
	 *
	 * @param InVariant Variant to launch the app with.
	 * @param AppId The identifier of the application to launch.
	 * @param BuildConfiguration The build configuration to launch with.
	 * @param Params The command line parameters to launch with.
	 * @return true on success, false otherwise.
	 * @see Deploy, OnLaunchFailed
	 */
	virtual bool LaunchApp(FName InVariant, const FString& AppId, EBuildConfigurations::Type BuildConfiguration, const FString& Params) = 0;

	/**
	 * Powers off the device.
	 *
	 * @param Force Whether to force powering off.
	 * @see PowerOn, Reboot
	 */
	virtual void PowerOff(bool Force) = 0;

	/**
	 * Powers on the device.
	 *
	 * @see PowerOff, Reboot
	 */
	virtual void PowerOn() = 0;

	/**
	 * Reboots the device.
	 *
	 * @see PowerOff, PowerOn
	 */
	virtual void Reboot() = 0;

	/**
	 * Attempts to run an executable on the device.
	 *
	 * @param InVariant Variant to run the executable with.
	 * @param ExecutablePath The path to the executable to run.
	 * @param Params The command line parameters.
	 */
	virtual void Run(FName InVariant, const FString& ExecutablePath, const FString& Params) = 0;

public:

	/**
	 * Returns a delegate to be invoked when a build has been deployed to the target device.
	 *
	 * @return The delegate.
	 * @see DeployApp
	 */
	virtual FOnTargetDeviceProxyDeployCommitted& OnDeployCommitted() = 0;

	/**
	 * Returns a delegate to be invoked when a build failed to deploy to the target device.
	 *
	 * @return The delegate.
	 * @see DeployApp
	 */
	virtual FOnTargetDeviceProxyDeployFailed& OnDeployFailed() = 0;

	/**
	 * Returns a delegate to be invoked when a build failed to launch on the target device.
	 *
	 * @return The delegate.
	 * @see LaunchApp
	 */
	virtual FOnTargetDeviceProxyLaunchFailed& OnLaunchFailed() = 0;

public:

	/** Virtual destructor. */
	virtual ~ITargetDeviceProxy() { }
};
