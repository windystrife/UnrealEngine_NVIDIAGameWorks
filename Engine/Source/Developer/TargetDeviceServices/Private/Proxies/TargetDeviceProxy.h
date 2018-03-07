// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "IMessageContext.h"
#include "MessageEndpoint.h"

#include "ITargetDeviceProxy.h"

class FTargetDeviceProxy;
class IMessageContext;

struct FTargetDeviceServiceDeployFinished;
struct FTargetDeviceServiceLaunchFinished;
struct FTargetDeviceServicePong;


/** Type definition for shared references to instances of FTargetDeviceProxy. */
typedef TSharedRef<class FTargetDeviceProxy> FTargetDeviceProxyRef;


/**
 * Implementation of the device proxy.
 */
class FTargetDeviceProxy
	: public ITargetDeviceProxy
{
public:

	/** Default constructor. */
	FTargetDeviceProxy() { }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InId The identifier of the target device to create this proxy for.
	 */
	FTargetDeviceProxy(const FString& InId);

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InId The identifier of the target device to create this proxy for.
	 * @param Message The message to initialize from.
	 * @param Context The message context.
	 */
	FTargetDeviceProxy(const FString& InId, const FTargetDeviceServicePong& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

public:

	/**
	 * Gets the time at which the proxy was last updated.
	 *
	 * @return Date and time of the last update.
	 */
	const FDateTime& GetLastUpdateTime() const
	{
		return LastUpdateTime;
	}

	/**
	 * Updates the proxy's information from the given device service response.
	 *
	 * @param Message The message containing the response.
	 * @param Context The message context.
	 */
	void UpdateFromMessage(const FTargetDeviceServicePong& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

public:

	// ITargetDeviceProxy interface

	virtual const bool CanMultiLaunch() const override
	{
		return SupportsMultiLaunch;
	}

	virtual bool CanPowerOff() const override
	{
		return SupportsPowerOff;
	}

	virtual bool CanPowerOn() const override
	{
		return SupportsPowerOn;
	}

	virtual bool CanReboot() const override
	{
		return SupportsReboot;
	}

	virtual bool CanSupportVariants() const override
	{
		return SupportsVariants;
	}

	virtual int32 GetNumVariants() const override;
	virtual int32 GetVariants(TArray<FName>& OutVariants) const override;
	virtual FName GetTargetDeviceVariant(const FString& InDeviceId) const override;
	virtual const FString& GetTargetDeviceId(FName InVariant) const override;
	virtual FString GetTargetPlatformName(FName InVariant) const override;
	virtual FName GetTargetPlatformId(FName InVariant) const override;
	virtual FName GetVanillaPlatformId(FName InVariant) const override;
	virtual FText GetPlatformDisplayName(FName InVariant) const override;

	virtual const FString& GetHostName() const override
	{
		return HostName;
	}

	virtual const FString& GetHostUser() const override
	{
		return HostUser;
	}

	virtual const FString& GetMake() const override
	{
		return Make;
	}

	virtual const FString& GetModel() const override
	{
		return Model;
	}

	virtual const FString& GetName() const override
	{
		return Name;
	}

	virtual const FString& GetDeviceUser() const override
	{
		return DeviceUser;
	}

	virtual const FString& GetDeviceUserPassword() const override
	{
		return DeviceUserPassword;
	}

	virtual const FString& GetType() const override
	{
		return Type;
	}

	virtual bool HasDeviceId(const FString& InDeviceId) const override;
	virtual bool HasVariant(FName InVariant) const override;
	virtual bool HasTargetPlatform(FName InTargetPlatformId) const override;

	virtual bool IsConnected() const override
	{
		return Connected;
	}

	virtual bool IsShared() const override
	{
		return Shared;
	}

	virtual bool DeployApp(FName InVariant, const TMap<FString, FString>& Files, const FGuid& TransactionId) override;
	virtual bool LaunchApp(FName InVariant, const FString& AppId, EBuildConfigurations::Type BuildConfiguration, const FString& Params) override;

	virtual FOnTargetDeviceProxyDeployCommitted& OnDeployCommitted() override
	{
		return DeployCommittedDelegate;
	}

	virtual FOnTargetDeviceProxyDeployFailed& OnDeployFailed() override
	{
		return DeployFailedDelegate;
	}

	virtual FOnTargetDeviceProxyLaunchFailed& OnLaunchFailed() override
	{
		return LaunchFailedDelegate;
	}

	virtual void PowerOff(bool Force) override;
	virtual void PowerOn() override;
	virtual void Reboot() override;
	virtual void Run(FName InVariant, const FString& ExecutablePath, const FString& Params) override;

protected:

	/** Initializes the message endpoint. */
	void InitializeMessaging();

private:

	/** Handles FTargetDeviceServiceDeployFinishedMessage messages. */
	void HandleDeployFinishedMessage(const FTargetDeviceServiceDeployFinished& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Handles FTargetDeviceServiceLaunchFinishedMessage messages. */
	void HandleLaunchFinishedMessage(const FTargetDeviceServiceLaunchFinished& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

private:

	/** Holds a flag indicating whether the device is connected. */
	bool Connected;

	/** Holds the name of the computer that hosts the device. */
	FString HostName;

	/** Holds the name of the user that owns the device. */
	FString HostUser;

	/** Holds the time at which the last ping reply was received. */
	FDateTime LastUpdateTime;

	/** Holds the device make. */
	FString Make;

	/** Holds the remote device's message bus address. */
	FMessageAddress MessageAddress;

	/** Holds the local message bus endpoint. */
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;

	/** Holds the remote device's model. */
	FString Model;

	/** Holds the name of the device. */
	FString Name;

	/** Holds device user. */
	FString DeviceUser;

	/** Holds device user password. */
	FString DeviceUserPassword;

	/** Holds a flag indicating whether the device is being shared with other users. */
	bool Shared;

	/** Holds a flag indicating whether the device supports multi-launch. */
	bool SupportsMultiLaunch;

	/** Holds a flag indicating whether the device can power off. */
	bool SupportsPowerOff;

	/** Holds a flag indicating whether the device can be power on. */
	bool SupportsPowerOn;

	/** Holds a flag indicating whether the device can reboot. */
	bool SupportsReboot;

	/** Holds a flag indicating whether the device's target platform supports variants. */
	bool SupportsVariants;

	/** Holds the device type. */
	FString Type;

	/** Holds default variant name. */
	FName DefaultVariant;


	/** Holds data about a device proxy variant. */
	class FTargetDeviceProxyVariant
	{
	public:

		/** Holds a string version of the variants device id. */
		FString DeviceID;

		/** Holds the variant name, this is the the map key as well. */
		FName VariantName;

		/** Platform information. */
		FString TargetPlatformName;
		FName TargetPlatformId;
		FName VanillaPlatformId;
		FText PlatformDisplayName;
	};

	/** Map of all the Variants for this Device. */
	TMap<FName, FTargetDeviceProxyVariant> TargetDeviceVariants;

private:

	/** Holds a delegate to be invoked when a build has been deployed to the target device. */
	FOnTargetDeviceProxyDeployCommitted DeployCommittedDelegate;

	/** Holds a delegate to be invoked when a build has failed to deploy to the target device. */
	FOnTargetDeviceProxyDeployFailed DeployFailedDelegate;

	/** Holds a delegate to be invoked when a build has failed to launch on the target device. */
	FOnTargetDeviceProxyLaunchFailed LaunchFailedDelegate;

	/** Holds a delegate to be invoked when a build has succeeded to launch on the target device. */
	FOnTargetDeviceProxyLaunchSucceeded LaunchSucceededDelegate;
};
