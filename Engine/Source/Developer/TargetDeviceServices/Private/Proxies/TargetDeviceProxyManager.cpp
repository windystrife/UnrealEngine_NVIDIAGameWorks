// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TargetDeviceProxyManager.h"

#include "HAL/PlatformProcess.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"

#include "TargetDeviceProxy.h"
#include "TargetDeviceServiceMessages.h"


/** Defines the interval in seconds in which devices are being pinged by the proxy manager. */
#define TARGET_DEVICE_SERVICES_PING_INTERVAL 2.5f


/* FTargetDeviceProxyManager structors
 *****************************************************************************/

FTargetDeviceProxyManager::FTargetDeviceProxyManager()
{
	MessageEndpoint = FMessageEndpoint::Builder("FTargetDeviceProxyManager")
		.Handling<FTargetDeviceServicePong>(this, &FTargetDeviceProxyManager::HandlePongMessage);

	if (MessageEndpoint.IsValid())
	{
		TickDelegate = FTickerDelegate::CreateRaw(this, &FTargetDeviceProxyManager::HandleTicker);
		TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate, TARGET_DEVICE_SERVICES_PING_INTERVAL);

		SendPing();
	}
}


FTargetDeviceProxyManager::~FTargetDeviceProxyManager()
{
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	FMessageEndpoint::SafeRelease(MessageEndpoint);
}


/* ITargetDeviceProxyLocator interface
 *****************************************************************************/

TSharedPtr<ITargetDeviceProxy> FTargetDeviceProxyManager::FindProxy(const FString& Name) 
{
	return Proxies.FindRef(Name);
}


TSharedRef<ITargetDeviceProxy> FTargetDeviceProxyManager::FindOrAddProxy(const FString& Name)
{
	TSharedPtr<FTargetDeviceProxy>& Proxy = Proxies.FindOrAdd(Name);

	if (!Proxy.IsValid())
	{
		Proxy = MakeShareable(new FTargetDeviceProxy(Name));

		ProxyAddedDelegate.Broadcast(Proxy.ToSharedRef());
	}

	return Proxy.ToSharedRef();
}


TSharedPtr<ITargetDeviceProxy> FTargetDeviceProxyManager::FindProxyDeviceForTargetDevice(const FString& DeviceId)
{
	for (TMap<FString, TSharedPtr<FTargetDeviceProxy> >::TConstIterator ItProxies(Proxies); ItProxies; ++ItProxies)
	{
		const TSharedPtr<FTargetDeviceProxy>& Proxy = ItProxies.Value();

		if (Proxy->HasDeviceId(DeviceId))
		{
			return Proxy;
		}
	}

	return TSharedPtr<ITargetDeviceProxy>();
}


void FTargetDeviceProxyManager::GetProxies(FName TargetPlatformName, bool IncludeUnshared, TArray<TSharedPtr<ITargetDeviceProxy>>& OutProxies)
{
	OutProxies.Reset();

	for (TMap<FString, TSharedPtr<FTargetDeviceProxy> >::TConstIterator It(Proxies); It; ++It)
	{
		const TSharedPtr<FTargetDeviceProxy>& Proxy = It.Value();

		if ((IncludeUnshared || Proxy->IsShared()) || (Proxy->GetHostUser() == FPlatformProcess::UserName(false)))
		{
			if (TargetPlatformName == NAME_None || Proxy->HasTargetPlatform(TargetPlatformName))
			{
				OutProxies.Add(Proxy);
			}		
		}
	}
}

/* FTargetDeviceProxyManager implementation
 *****************************************************************************/

void FTargetDeviceProxyManager::RemoveDeadProxies()
{
	FDateTime CurrentTime = FDateTime::UtcNow();

	for (auto ProxyIter = Proxies.CreateIterator(); ProxyIter; ++ProxyIter)
	{
		if (ProxyIter.Value()->GetLastUpdateTime() + FTimespan::FromSeconds(3.0 * TARGET_DEVICE_SERVICES_PING_INTERVAL) < CurrentTime)
		{
			TSharedPtr<ITargetDeviceProxy> RemovedProxy = ProxyIter.Value();
			ProxyIter.RemoveCurrent();
			ProxyRemovedDelegate.Broadcast(RemovedProxy.ToSharedRef());
		}
	}
}


void FTargetDeviceProxyManager::SendPing()
{
	if (MessageEndpoint.IsValid())
	{
		MessageEndpoint->Publish(new FTargetDeviceServicePing(FPlatformProcess::UserName(false)), EMessageScope::Network);
	}
}


/* FTargetDeviceProxyManager callbacks
 *****************************************************************************/

void FTargetDeviceProxyManager::HandlePongMessage(const FTargetDeviceServicePong& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	// Another HACK: Ignore devices from other machines. See FTargetDeviceService::HandleClaimDeniedMessage()
	if (Message.HostName != FPlatformProcess::ComputerName())
 	{
 		return;
 	}

	TSharedPtr<FTargetDeviceProxy>& Proxy = Proxies.FindOrAdd(Message.Name);

	if (!Proxy.IsValid())
	{
		Proxy = MakeShareable(new FTargetDeviceProxy(Message.Name, Message, Context));
		ProxyAddedDelegate.Broadcast(Proxy.ToSharedRef());
	}
	else
	{
		Proxy->UpdateFromMessage(Message, Context);
	}
}


bool FTargetDeviceProxyManager::HandleTicker(float DeltaTime)
{
	RemoveDeadProxies();
	SendPing();

	return true;
}
