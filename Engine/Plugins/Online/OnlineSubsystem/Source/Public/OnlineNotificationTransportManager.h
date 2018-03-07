// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineNotificationTransportInterface.h"
#include "OnlineSubsystemPackage.h"

struct FOnlineNotification;

//forward declare
typedef TSharedPtr<class IOnlineNotificationTransport, ESPMode::ThreadSafe> IOnlineNotificationTransportPtr;
class IOnlineNotificationTransportMessage;
struct FOnlineNotification;


/** This class is a static manager used to track notification transports and map the delivered notifications to subscribed notification handlers */
class ONLINESUBSYSTEM_API FOnlineNotificationTransportManager
{

protected:

	/** Map from a transport type to the transport object */
	TMap< FNotificationTransportId, IOnlineNotificationTransportPtr > TransportMap;

public:

	/** Lifecycle is managed by OnlineSubSystem, all access should be through there */
	FOnlineNotificationTransportManager()
	{
	}

	virtual ~FOnlineNotificationTransportManager();
	
	/** Send a notification using a specific transport */
	bool SendNotification(FNotificationTransportId TransportType, const FOnlineNotification& Notification);

	/** Receive a message from a specific transport, convert to notification, and pass on for delivery */
	bool ReceiveTransportMessage(FNotificationTransportId TransportType, const IOnlineNotificationTransportMessage& TransportMessage);

	// NOTIFICATION TRANSPORTS

	/** Get a notification transport of a specific type */
	IOnlineNotificationTransportPtr GetNotificationTransport(FNotificationTransportId TransportType);

	/** Add a notification transport */
	void AddNotificationTransport(IOnlineNotificationTransportPtr Transport);

	/** Remove a notification transport */
	void RemoveNotificationTransport(FNotificationTransportId TransportType);

	/** Resets all transports */
	void ResetNotificationTransports();

	/** Base function for letting the notifications flow */
	virtual FOnlineTransportTapHandle OpenTap(const FUniqueNetId& User, const FOnlineTransportTap& Tap)
	{
		return FOnlineTransportTapHandle();
	}

	/** Base function for stanching the notifications */
	virtual void CloseTap(FOnlineTransportTapHandle TapHandle)
	{
	}
};

typedef TSharedPtr<FOnlineNotificationTransportManager, ESPMode::ThreadSafe> FOnlineNotificationTransportManagerPtr;
