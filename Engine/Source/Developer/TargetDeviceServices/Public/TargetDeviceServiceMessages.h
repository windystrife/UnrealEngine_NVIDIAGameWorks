// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/Guid.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"

#include "TargetDeviceServiceMessages.generated.h"


/* Application deployment messages
 *****************************************************************************/

/**
 * Implements a message for committing a deployment transaction.
 *
 * @see FTargetDeviceServiceDeployFile, FTargetDeviceServiceDeployFinished
 */
USTRUCT()
struct FTargetDeviceServiceDeployCommit
{
	GENERATED_USTRUCT_BODY()

	/** Holds the variant identifier of the target device to use. */
	UPROPERTY(EditAnywhere, Category="Message")
	FName Variant;

	/** Holds the identifier of the deployment transaction to commit. */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid TransactionId;


	/** Default constructor. */
	FTargetDeviceServiceDeployCommit() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServiceDeployCommit(FName InVariant, const FGuid& InTransactionId)
		: Variant(InVariant)
		, TransactionId(InTransactionId)
	{ }
};


/**
 * Implements a message for deploying a single file to a target device.
 *
 * The actual file data must be attached to the message.
 *
 * @see FTargetDeviceServiceDeployCommit, FTargetDeviceServiceDeployFinished
 */
USTRUCT()
struct FTargetDeviceServiceDeployFile
{
	GENERATED_USTRUCT_BODY()

	/** Holds the name and path of the file as it will be stored on the target device. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString TargetFileName;

	/** Holds the identifier of the deployment transaction that this file is part of. */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid TransactionId;


	/** Default constructor. */
	FTargetDeviceServiceDeployFile() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServiceDeployFile(const FString& InTargetFileName, const FGuid& InTransactionId)
		: TargetFileName(InTargetFileName)
		, TransactionId(InTransactionId)
	{ }
};


/**
 * Implements a message for notifying a target device proxy that a deployment transaction has finished.
 *
 * @see FTargetDeviceServiceDeployFile, FTargetDeviceServiceDeployCommit
 */
USTRUCT()
struct FTargetDeviceServiceDeployFinished
{
	GENERATED_USTRUCT_BODY()

	/** Holds the variant identifier of the target device to use. */
	UPROPERTY(EditAnywhere, Category="Message")
	FName Variant;

	/**
	 * Holds the created identifier for the deployed application.
	 *
	 * The semantics of this identifier are target platform specific. In some cases it may be
	 * a GUID, in other cases it may be the path to the application or some other means of
	 * identifying the application. Application identifiers are returned from target device
	 * services as result of successful deployment transactions.
	 */
	UPROPERTY(EditAnywhere, Category="Message")
	FString AppID;

	/** Holds a flag indicating whether the deployment transaction finished successfully. */
	UPROPERTY(EditAnywhere, Category="Message")
	bool Succeeded;

	/** Holds the identifier of the deployment transaction that this file is part of. */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid TransactionId;


	/** Default constructor. */
	FTargetDeviceServiceDeployFinished() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServiceDeployFinished(FName InVariant, const FString& InAppId, bool InSucceeded, FGuid InTransactionId)
		: Variant(InVariant)
		, AppID(InAppId)
		, Succeeded(InSucceeded)
		, TransactionId(InTransactionId)
	{ }
};


/* Application launch messages
*****************************************************************************/

/**
 * Implements a message for committing a deployment transaction.
 *
 * To launch an arbitrary executable on a device use the FTargetDeviceServiceRunExecutable message instead.
 *
 * @see FTargetDeviceServiceLaunchFinished
 */
USTRUCT()
struct FTargetDeviceServiceLaunchApp
{
	GENERATED_USTRUCT_BODY()

	/** Holds the variant identifier of the target device to use. */
	UPROPERTY(EditAnywhere, Category="Message")
	FName Variant;

	/**
	 * Holds the identifier of the application to launch.
	 *
	 * The semantics of this identifier are target platform specific. In some cases it may be
	 * a GUID, in other cases it may be the path to the application or some other means of
	 * identifying the application. Application identifiers are returned from target device
	 * services as result of successful deployment transactions.
	 */
	UPROPERTY(EditAnywhere, Category="Message")
	FString AppID;

	/** The application's build configuration, i.e. Debug or Shipping. */
	UPROPERTY(EditAnywhere, Category="Message")
	uint8 BuildConfiguration;

	/** Holds optional command line parameters for the application. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString Params;


	/** Default constructor. */
	FTargetDeviceServiceLaunchApp() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServiceLaunchApp(FName InVariant, const FString& InAppId, uint8 InBuildConfiguration, const FString& InParams)
		: Variant(InVariant)
		, AppID(InAppId)
		, BuildConfiguration(InBuildConfiguration)
		, Params(InParams)
	{ }
};


/**
 * Implements a message for notifying a target device proxy that launching an application has finished.
 *
 * @see FTargetDeviceServiceLaunchApp
 */
USTRUCT()
struct FTargetDeviceServiceLaunchFinished
{
	GENERATED_USTRUCT_BODY()

	/**
	 * Holds the identifier of the launched application.
	 *
	 * The semantics of this identifier are target platform specific. In some cases it may be
	 * a GUID, in other cases it may be the path to the application or some other means of
	 * identifying the application. Application identifiers are returned from target device
	 * services as result of successful deployment transactions.
	 */
	UPROPERTY(EditAnywhere, Category="Message")
	FString AppID;

	/** Holds the process identifier for the launched application. */
	UPROPERTY(EditAnywhere, Category="Message")
	int32 ProcessId;

	/** Holds a flag indicating whether the application was launched successfully. */
	UPROPERTY(EditAnywhere, Category="Message")
	bool Succeeded;


	/** Default constructor. */
	FTargetDeviceServiceLaunchFinished() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServiceLaunchFinished(const FString& InAppId, int32 InProcessId, bool InSucceeded)
		: AppID(InAppId)
		, ProcessId(InProcessId)
		, Succeeded(InSucceeded)
	{ }
};


/* Device claiming messages
 *****************************************************************************/

/**
 * Implements a message that is sent when a device is already claimed by someone else.
 *
 * @see FTargetDeviceClaimDropped
 * @see FTargetDeviceClaimRequest
 */
USTRUCT()
struct FTargetDeviceClaimDenied
{
	GENERATED_USTRUCT_BODY()

	/** Holds the identifier of the device that is already claimed. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString DeviceName;

	/** Holds the name of the host computer that claimed the device. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString HostName;

	/** Holds the name of the user that claimed the device. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString HostUser;


	/** Default constructor. */
	FTargetDeviceClaimDenied() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceClaimDenied(const FString& InDeviceName, const FString& InHostName, const FString& InHostUser)
		: DeviceName(InDeviceName)
		, HostName(InHostName)
		, HostUser(InHostUser)
	{ }
};


/**
 * Implements a message that is sent when a service claimed a device.
 *
 * @see FTargetDeviceClaimDenied
 * @see FTargetDeviceClaimDropped
 */
USTRUCT()
struct FTargetDeviceClaimed
{
	GENERATED_USTRUCT_BODY()

	/** Holds the identifier of the device that is being claimed. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString DeviceName;

	/** Holds the name of the host computer that is claiming the device. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString HostName;

	/** Holds the name of the user that is claiming the device. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString HostUser;


	/** Default constructor. */
	FTargetDeviceClaimed() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceClaimed(const FString& InDeviceName, const FString& InHostName, const FString& InHostUser)
		: DeviceName(InDeviceName)
		, HostName(InHostName)
		, HostUser(InHostUser)
	{ }
};


/**
 * Implements a message that is sent when a device is no longer claimed.
 *
 * @see FTargetDeviceClaimDenied, FTargetDeviceClaimRequest
 */
USTRUCT()
struct FTargetDeviceUnclaimed
{
	GENERATED_USTRUCT_BODY()

	/** Holds the identifier of the device that is no longer claimed. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString DeviceName;

	/** Holds the name of the host computer that had claimed the device. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString HostName;

	/** Holds the name of the user that had claimed the device. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString HostUser;


	/** Default constructor. */
	FTargetDeviceUnclaimed() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceUnclaimed(const FString& InDeviceName, const FString& InHostName, const FString& InHostUser)
		: DeviceName(InDeviceName)
		, HostName(InHostName)
		, HostUser(InHostUser)
	{ }
};


/* Device discovery messages
 *****************************************************************************/

/**
 * Implements a message for discovering target device services on the network.
 */
USTRUCT()
struct FTargetDeviceServicePing
{
	GENERATED_USTRUCT_BODY()

	/** Holds the name of the user who generated the ping. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString HostUser;


	/** Default constructor. */
	FTargetDeviceServicePing() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServicePing(const FString& InHostUser)
		: HostUser(InHostUser)
	{ }
};


/**
* Struct for a flavor's information
*/
USTRUCT()
struct FTargetDeviceVariant
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category="Variant")
	FString DeviceID;

	UPROPERTY(EditAnywhere, Category="Variant")
	FName VariantName;

	UPROPERTY(EditAnywhere, Category="Variant")
	FString TargetPlatformName;

	UPROPERTY(EditAnywhere, Category="Variant")
	FName TargetPlatformId;

	UPROPERTY(EditAnywhere, Category="Variant")
	FName VanillaPlatformId;

	UPROPERTY(EditAnywhere, Category="Variant")
	FString PlatformDisplayName;
};


/**
 * Implements a message that is sent in response to target device service discovery messages.
 */
USTRUCT()
struct FTargetDeviceServicePong
{
	GENERATED_USTRUCT_BODY()

	/** Holds a flag indicating whether the device is currently connected. */
	UPROPERTY(EditAnywhere, Category="Message")
	bool Connected;

	/** Holds the name of the host computer that the device is attached to. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString HostName;

	/** Holds the name of the user under which the host computer is running. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString HostUser;

	/** Holds the make of the device, i.e. Microsoft or Sony. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString Make;

	/** Holds the model of the device. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString Model;

	/** Holds the human readable name of the device, i.e "Bob's XBox'. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString Name;

	/** Holds the name of the user that we log in to remote device as, i.e "root". */
	UPROPERTY(EditAnywhere, Category="Message")
	FString DeviceUser;

	/** Holds the password of the user that we log in to remote device as, i.e "12345". */
	UPROPERTY(EditAnywhere, Category="Message")
	FString DeviceUserPassword;

	/** Holds a flag indicating whether this device is shared with other users on the network. */
	UPROPERTY(EditAnywhere, Category="Message")
	bool Shared;

	/** Holds a flag indicating whether the device supports running multiple application instances in parallel. */
	UPROPERTY(EditAnywhere, Category="Message")
	bool SupportsMultiLaunch;

	/** Holds a flag indicating whether the device can be powered off. */
	UPROPERTY(EditAnywhere, Category="Message")
	bool SupportsPowerOff;

	/** Holds a flag indicating whether the device can be powered on. */
	UPROPERTY(EditAnywhere, Category="Message")
	bool SupportsPowerOn;

	/** Holds a flag indicating whether the device can be rebooted. */
	UPROPERTY(EditAnywhere, Category="Message")
	bool SupportsReboot;

	/** Holds a flag indicating whether the device's target platform supports variants. */
	UPROPERTY(EditAnywhere, Category="Message")
	bool SupportsVariants;

	/** Holds the device type. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString Type;

	/** Holds the variant name of the default device. */
	UPROPERTY(EditAnywhere, Category="Message")
	FName DefaultVariant;

	/** List of the Flavors this service supports */
	UPROPERTY(EditAnywhere, Category="Message")
	TArray<FTargetDeviceVariant> Variants;
};


/* Miscellaneous messages
 *****************************************************************************/

/**
 * Implements a message for powering on a target device.
 */
USTRUCT()
struct FTargetDeviceServicePowerOff
{
	GENERATED_USTRUCT_BODY()

	/**
	 * Holds a flag indicating whether the power-off should be enforced.
	 *
	 * If powering off is not enforced, if may fail if some running application prevents it.
	 */
	UPROPERTY(EditAnywhere, Category="Message")
	bool Force;

	/** Holds the name of the user that wishes to power off the device. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString Operator;


	/** Default constructor. */
	FTargetDeviceServicePowerOff() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServicePowerOff(const FString& InOperator, bool InForce)
		: Force(InForce)
		, Operator(InOperator)
	{ }
};


/**
 * Implements a message for powering on a target device.
 */
USTRUCT()
struct FTargetDeviceServicePowerOn
{
	GENERATED_USTRUCT_BODY()

	/** Holds the name of the user that wishes to power on the device. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString Operator;


	/** Default constructor. */
	FTargetDeviceServicePowerOn() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServicePowerOn(const FString& InOperator)
		: Operator(InOperator)
	{ }
};


/**
 * Implements a message for rebooting a target device.
 */
USTRUCT()
struct FTargetDeviceServiceReboot
{
	GENERATED_USTRUCT_BODY()

	/** Holds the name of the user that wishes to reboot the device. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString Operator;


	/** Default constructor. */
	FTargetDeviceServiceReboot() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServiceReboot(const FString& InOperator)
		: Operator(InOperator)
	{ }
};


/**
 * Implements a message for running an executable on a target device.
 *
 * To launch a previously deployed application on a device use the FTargetDeviceServiceLaunchApp message instead.
 *
 * @see FTargetDeviceServiceLaunchApp
 */
USTRUCT()
struct FTargetDeviceServiceRunExecutable
{
	GENERATED_USTRUCT_BODY()

	/** Holds the variant identifier of the target device to use for execution. */
	UPROPERTY(EditAnywhere, Category="Message")
	FName Variant;

	/** Holds the path to the executable on the device. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString ExecutablePath;

	/** Holds optional command line parameters for the executable. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString Params;


	/** Default constructor. */
	FTargetDeviceServiceRunExecutable() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServiceRunExecutable(FName InVariant, const FString& InExecutablePath, const FString& InParams)
		: Variant(InVariant)
		, ExecutablePath(InExecutablePath)
		, Params(InParams)
	{ }
};


/**
 * Implements a message for notifying a target device proxy that running an executable has finished.
 *
 * @see FTargetDeviceServiceRunExecutable
 */
USTRUCT()
struct FTargetDeviceServiceRunFinished
{
	GENERATED_USTRUCT_BODY()

	/** Holds the variant identifier of the target device to use. */
	UPROPERTY(EditAnywhere, Category="Message")
	FName Variant;

	/** Holds the path to the executable that was run. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString ExecutablePath;

	/** Holds the process identifier of the running executable. */
	UPROPERTY(EditAnywhere, Category="Message")
	int32 ProcessId;

	/** Holds a flag indicating whether the executable started successfully. */
	UPROPERTY(EditAnywhere, Category="Message")
	bool Succeeded;


	/** Default constructor. */
	FTargetDeviceServiceRunFinished() { }

	/** Creates and initializes a new instance. */
	FTargetDeviceServiceRunFinished(FName InVariant, const FString& InExecutablePath, int32 InProcessId, bool InSucceeded)
		: Variant(InVariant)
		, ExecutablePath(InExecutablePath)
		, ProcessId(InProcessId)
		, Succeeded(InSucceeded)
	{ }
};
