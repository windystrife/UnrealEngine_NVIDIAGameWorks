// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Containers/Ticker.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Templates/SharedPointer.h"

#include "ITargetDeviceProxyManager.h"

class FMessageEndpoint;
class FTargetDeviceProxy;
class IMessageContext;

struct FTargetDeviceServicePong;


/**
 * Implements a class which locates devices based on criteria for use in the Launcher.
 */
class FTargetDeviceProxyManager
	: public ITargetDeviceProxyManager
{
public:

	/** Default constructor. */
	FTargetDeviceProxyManager();

	/** Virtual destructor. */
	virtual ~FTargetDeviceProxyManager();

public:

	//~ ITargetDeviceProxyLocator interface

	virtual TSharedRef<ITargetDeviceProxy> FindOrAddProxy(const FString& Name) override;
	virtual TSharedPtr<ITargetDeviceProxy> FindProxy(const FString& Name) override;
	virtual TSharedPtr<ITargetDeviceProxy> FindProxyDeviceForTargetDevice(const FString& DeviceId) override;
	virtual void GetProxies(FName TargetPlatformName, bool IncludeUnshared, TArray<TSharedPtr<ITargetDeviceProxy>>& OutProxies) override;

	DECLARE_DERIVED_EVENT(FTargetDeviceProxyManager, ITargetDeviceProxyManager::FOnTargetDeviceProxyAdded, FOnTargetDeviceProxyAdded);
	virtual FOnTargetDeviceProxyAdded& OnProxyAdded() override
	{
		return ProxyAddedDelegate;
	}

	DECLARE_DERIVED_EVENT(FTargetDeviceProxyManager, ITargetDeviceProxyManager::FOnTargetDeviceProxyRemoved, FOnTargetDeviceProxyRemoved);
	virtual FOnTargetDeviceProxyRemoved& OnProxyRemoved() override
	{
		return ProxyRemovedDelegate;
	}

protected:

	/** Removes all target device proxies that timed out. */
	void RemoveDeadProxies();

	/** Pings all target devices on the network. */
	void SendPing();

private:
	
	/** Handles FTargetDeviceServicePong messages. */
	void HandlePongMessage(const FTargetDeviceServicePong& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Handles ticks from the ticker. */
	bool HandleTicker(float DeltaTime);

private:

	/** Holds the message endpoint. */
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;

	/** Holds the collection of proxies. */
	TMap<FString, TSharedPtr<FTargetDeviceProxy>> Proxies;

private:

	/** Holds a delegate that is invoked when a target device proxy has been added. */
	FOnTargetDeviceProxyAdded ProxyAddedDelegate;

	/** Holds a delegate that is invoked when a target device proxy has been removed. */
	FOnTargetDeviceProxyRemoved ProxyRemovedDelegate;

	/** Holds a delegate to be invoked when the widget ticks. */
	FTickerDelegate TickDelegate;

	/** Handle to the registered TickDelegate. */
	FDelegateHandle TickDelegateHandle;
};
