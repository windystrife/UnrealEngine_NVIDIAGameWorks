// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IPAddress.h"
#include "SocketSubsystem.h"

/**
 * Sets the address to return to the caller
 *
 * @param InAddr the address that is being cached
 */
FResolveInfoCached::FResolveInfoCached(const FInternetAddr& InAddr)
{
	// @todo sockets: This should use Clone
	uint32 IpAddr;
	InAddr.GetIp(IpAddr);
	Addr = ISocketSubsystem::Get()->CreateInternetAddr(IpAddr, InAddr.GetPort());
}

/**
 * Copies the host name for async resolution
 *
 * @param InHostName the host name to resolve
 */
FResolveInfoAsync::FResolveInfoAsync(const ANSICHAR* InHostName) :
	ErrorCode(SE_NO_ERROR),
	bShouldAbandon(false),
	AsyncTask(this)
{
	FCStringAnsi::Strncpy(HostName,InHostName,256);
}

/**
 * Resolves the specified host name
 */
void FResolveInfoAsync::DoWork()
{
	int32 AttemptCount = 0;

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	Addr = SocketSubsystem->CreateInternetAddr(0,0);

	// Make up to 3 attempts to resolve it
	do 
	{
		ErrorCode = SocketSubsystem->GetHostByName(HostName, *Addr);
		if (ErrorCode != SE_NO_ERROR)
		{
			if (ErrorCode == SE_HOST_NOT_FOUND || ErrorCode == SE_NO_DATA || ErrorCode == SE_ETIMEDOUT)
			{
				// Force a failure
				AttemptCount = 3;
			}
		}
		AttemptCount++;
	}
	while (ErrorCode != SE_NO_ERROR && AttemptCount < 3 && bShouldAbandon == false);
	if (ErrorCode == SE_NO_ERROR)
	{
		// Cache for reuse
		SocketSubsystem->AddHostNameToCache(HostName, Addr);
	}
}
