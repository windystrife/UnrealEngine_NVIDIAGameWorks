// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BSDSockets/SocketSubsystemBSDPrivate.h"
#include "Sockets.h"
#include "BSDIPv6Sockets/SocketsBSDIPv6.h"

class FInternetAddr;

#if PLATFORM_HAS_BSD_IPV6_SOCKETS


/**
 * Implements a BSD network socket on IOS.
 */
class FSocketBSDIPv6IOS
	: public FSocketBSDIPv6
{
public:

	FSocketBSDIPv6IOS(SOCKET InSocket, ESocketType InSocketType, const FString& InSocketDescription, ISocketSubsystem* InSubsystem)
		:FSocketBSDIPv6(InSocket, InSocketType, InSocketDescription, InSubsystem)
	{
	}

	virtual ~FSocketBSDIPv6IOS()
	{	
		FSocketBSDIPv6::Close();
	}

};

#endif
