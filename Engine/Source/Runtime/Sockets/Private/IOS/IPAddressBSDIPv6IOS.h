// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BSDSockets/SocketSubsystemBSDPrivate.h"
#include "BSDIPv6Sockets/IPAddressBSDIPv6.h"

#if PLATFORM_HAS_BSD_IPV6_SOCKETS

class FInternetAddrBSDIPv6IOS : public FInternetAddrBSDIPv6
{

public:

	/** Sets the address to broadcast */
	virtual void SetBroadcastAddress() override
	{
		UE_LOG(LogSockets, Verbose, TEXT("SetBroadcastAddress() FInternetAddrBSDIPv6IOS"));
		SetIp(INADDR_BROADCAST);
		SetPort(0);
	}
};

#endif
