// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/TargetDeviceId.h"

class ITargetDevice;
class ITargetDeviceOutput;

/**
 * Enumerates features that may be supported by target devices.
 */
enum class ETargetDeviceFeatures
{
	/** Multiple instances of a game can run at the same time. */
	MultiLaunch,

	/** The device can be powered off remotely. */
	PowerOff,

	/** The device can be powered on remotely. */	  
	PowerOn,

	/** Snapshot of processes running on the device. */
	ProcessSnapshot,

	/** The device can be rebooted remotely. */
	Reboot
};


/**
 * Enumerates target device types.
 */
enum class ETargetDeviceTypes
{
	/** Indeterminate device type. */
	Indeterminate,

	/** The device is a web browser (i.e. Flash). */
	Browser,

	/** The device is a game console. */
	Console,

	/** The device is a desktop computer. */
	Desktop,

	/** The device is a smart phone. */
	Phone,

	/** The device is a tablet computer. */
	Tablet
};


namespace TargetDeviceTypes
{
	/**
	 * Returns the string representation of the specified ETargetDeviceTypes value.
	 *
	 * @param Configuration The value to get the string for.
	 * @return A string value.
	 */
	inline FString ToString(ETargetDeviceTypes DeviceType)
	{
		switch (DeviceType)
		{
		case ETargetDeviceTypes::Browser:
			return FString("Browser");

		case ETargetDeviceTypes::Console:
			return FString("Console");

		case ETargetDeviceTypes::Desktop:
			return FString("Desktop");

		case ETargetDeviceTypes::Phone:
			return FString("Phone");

		case ETargetDeviceTypes::Tablet:
			return FString("Tablet");

		default:
			return FString("Indeterminate");
		}
	}
}


/**
 * Enumerates states of threads running on a target device.
 */
enum class ETargetDeviceThreadStates
{
	/** Unknown thread state. */
	Unknown,

	/** The thread can run, but is not running right now. */
	CanRun,

	/** The thread is inactive, i.e. has just been created or exited. */
	Inactive,

	/** The thread cannot run right now. */
	Inhibited,

	/** The thread is in the run queue. */
	RunQueue,

	/** The thread is running. */
	Running
};


/**
 * Enumerates wait states of threads running on a target device.
 */
enum class ETargetDeviceThreadWaitStates
{
	/** Unknown wait state. */
	Unknown,

	/** The thread is blocked by a lock. */
	Locked,
	
	/** The thread is sleeping. */
	Sleeping,

	/** The thread is suspended. */
	Suspended,

	/** The thread is swapped. */
	Swapped,

	/** The thread is waiting on an interrupt. */
	Waiting
};


/**
 * Structure for thread information.
 */
struct FTargetDeviceThreadInfo
{
	/** Holds the exit code. */
	uint64 ExitCode;

	/** Holds the thread identifier. */
	uint32 Id;

	/** Holds the name of the thread. */
	FString Name;

	/** Holds the thread's stack size. */
	uint64 StackSize;

	/** Holds the thread's current state. */
	ETargetDeviceThreadStates State;

	/** Holds the thread's current wait state. */
	ETargetDeviceThreadWaitStates WaitState;
};


/**
 * Structure for information for processes that are running on a target device.
 */
struct FTargetDeviceProcessInfo
{
	/** Holds the process identifier. */
	int64 Id;

	/** Holds the process name. */
	FString Name;

	/** Holds the identifier of the parent process. */
	uint64 ParentId;

	/** Holds the collection of threads that belong to this process. */
	TArray<FTargetDeviceThreadInfo> Threads;

	/** The name of the user that owns this process. */
	FString UserName;
};


/** Type definition for shared pointers to instances of ITargetDevice. */
typedef TSharedPtr<class ITargetDevice, ESPMode::ThreadSafe> ITargetDevicePtr;

/** Type definition for shared references to instances of ITargetDevice. */
typedef TSharedRef<class ITargetDevice, ESPMode::ThreadSafe> ITargetDeviceRef;

/** Type definition for weak pointers to instances of ITargetDevice. */
typedef TWeakPtr<class ITargetDevice, ESPMode::ThreadSafe> ITargetDeviceWeakPtr;

/** Type definition for shared pointers to instances of ITargetDeviceOutput. */
typedef TSharedPtr<class ITargetDeviceOutput, ESPMode::ThreadSafe> ITargetDeviceOutputPtr;

/**
 * Interface for target devices.
 */
class ITargetDevice
{
public:

	/**
	 * Connect to the physical device.
	 *
	 * @return true if the device is connected, false otherwise.
	 */
	virtual bool Connect() = 0;

	/**
	 * Deploys an application in the specified folder to the device.
	 *
	 * @param SourceFolder The path to the files and directories to be deployed.
	 * @param OutAppId Will hold the identifier of the deployed application (used for launching).
	 * @return true on success, false otherwise.
	 */
	virtual bool Deploy( const FString& SourceFolder, FString& OutAppId ) = 0;

	/**
	 * Disconnect from the physical device.
	 */
	virtual void Disconnect() = 0;

	/**
	 * Gets the device type.
	 *
	 * @return Device type.
	 */
	virtual ETargetDeviceTypes GetDeviceType() const = 0;

	/**
	 * Gets the unique device identifier.
	 *
	 * @return Device identifier.
	 * @see GetName
	 */
	virtual FTargetDeviceId GetId() const = 0;

	/**
	 * Gets the name of the device.
	 *
	 * In contrast to GetId(), this method is intended to return a human readable
	 * name for use in the user interface. Depending on the target platform, this
	 * name may be some user defined string, a host name, an IP address, or some
	 * other string identifying the device that does not need to be unique.
	 *
	 * @return Device name.
	 * @see GetId
	 */
	virtual FString GetName() const = 0;

	/**
	 * Gets the name of the operating system running on this device.
	 *
	 * @return Operating system name.
	 */
	virtual FString GetOperatingSystemName() = 0;

	/**
	 * Creates a snapshot of processes currently running on the device.
	 *
	 * @param OutProcessInfos Will contain the information for running processes.
	 * @return The number of returned processes.
	 */
	virtual int32 GetProcessSnapshot( TArray<FTargetDeviceProcessInfo>& OutProcessInfos ) = 0;

	/**
	 * Gets the TargetPlatform that this device belongs to.
	 */
	virtual const class ITargetPlatform& GetTargetPlatform() const = 0;

	/**
	 * Checks whether this device is connected.
	 *
	 * @return true if the device is connected, false otherwise.
	 */
	virtual bool IsConnected() = 0;

	/**
	 * Checks whether this is the default device.
	 *
	 * Note that not all platforms may have a notion of default devices.
	 *
	 * @return true if this is the default device, false otherwise.
	 */
	virtual bool IsDefault() const = 0;

	/**
	* Checks whether this device is authorized to be used with this computer.
	*
	* This is true for most platforms by default, but may be false for mobile platforms
	*
	* @return true if this this device is authorized for launch
	*/
	virtual bool IsAuthorized() const { return true;  }

	/**
	 * Launches a previously deployed build.
	 *
	 * @param AppId The identifier of the application to launch (as returned by the Deploy() method).
	 * @param BuildConfiguration The build configuration to launch.
	 * @param BuildTarget The build target type to launch
	 * @param Params The command line parameters to launch with.
	 * @param OutProcessId Will hold the identifier of the created process (can be NULL).
	 * @return true on success, false otherwise.
	 */
	virtual bool Launch( const FString& AppId, EBuildConfigurations::Type BuildConfiguration, EBuildTargets::Type BuildTarget, const FString& Params, uint32* OutProcessId ) = 0;

	/**
	 * Powers off the device.
	 *
	 * @param Force Whether to force powering off.
	 * @return true if the device will be powered off, false otherwise.
	 */
	virtual bool PowerOff( bool Force ) = 0;

	/**
	 * Powers on the device.
	 *
	 * @return true if the device will be powered on, false otherwise.
	 */
	virtual bool PowerOn() = 0;

	/** 
	 * Reboot the device.
	 *
	 * @param bReconnect If true, wait and reconnect when done.
	 * @return true if the reboot was successful from the perspective of the PC .
	 */
	virtual bool Reboot( bool bReconnect = false ) = 0;

	/**
	 * Runs an executable on the device.
	 *
	 * @param ExecutablePath The path to the executable to run.
	 * @param Params The command line parameters.
	 * @param OutProcessId Will hold the identifier of the created process (can be NULL).
	 * @return true if the executable was started, false otherwise.
	 */
	virtual bool Run( const FString& ExecutablePath, const FString& Params, uint32* OutProcessId ) = 0;

	/**
	 * Checks whether the target device supports the specified feature.
	 *
	 * @param Feature The feature to check.
	 * @return true if the feature is supported, false otherwise.
	 */
	virtual bool SupportsFeature( ETargetDeviceFeatures Feature ) const = 0;

	/**
	 * Checks whether this device supports the specified SDK version.
	 *
	 * @param VersionString The SDK version string.
	 * @return true if the SDK version is supported, false otherwise.
	 */
	virtual bool SupportsSdkVersion( const FString& VersionString ) const = 0;

	/**
	 * Terminates a process that was launched on the device using the Launch() or Run() methods.
	 *
	 * @param ProcessId The identifier of the process to terminate.
	 * @return true if the process was terminated, false otherwise.
	 */
	virtual bool TerminateProcess( const int64 ProcessId ) = 0;

	/**
	 * Set credentials for the user account to use on the device
	 * 
	 * @param UserName The user account on the device we will run under
	 * @param UserPassword The password for the user account on the device we will run under.
	 */
	virtual void SetUserCredentials( const FString& UserName, const FString& UserPassword ) = 0;

	/**
	 * Get credentials for the user account to use on the device
	 * 
	 * @param OutUserName The user account on the device we will run under
	 * @param OutUserPassword The password for the user account on the device we will run under
	 * @return true on success, false if not supported.
	 */
	virtual bool GetUserCredentials( FString& OutUserName, FString& OutUserPassword ) = 0;

	/**
	 * Execute console command on the device
	 * 
	 * @param ExecCommand Command to execute
	 */
	virtual void ExecuteConsoleCommand(const FString& ExecCommand) const {};

	/**
	 * Create device output router
	 *
	 * This will route device logs into specified OutputDevice 
	 * until connection to device is alive
	 *
	 * @param Output OutputDevice to where output should be routed (has to be thread safe)
	 * @return Valid router object for devices that support output routing
	 */
	virtual ITargetDeviceOutputPtr CreateDeviceOutputRouter(FOutputDevice* Output) const { return nullptr; };

public:
	
	/** Virtual destructor. */
	virtual ~ITargetDevice() { }
};
