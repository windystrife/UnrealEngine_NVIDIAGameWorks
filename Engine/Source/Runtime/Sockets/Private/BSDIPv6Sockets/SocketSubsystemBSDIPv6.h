// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDeviceRedirector.h"
#include "BSDSockets/SocketSubsystemBSDPrivate.h"
#include "BSDIPv6Sockets/SocketsBSDIPv6.h"
#include "IPAddress.h"
#include "SocketSubsystemPackage.h"

#if PLATFORM_HAS_BSD_IPV6_SOCKETS



/**
 * Standard BSD specific IPv6 socket subsystem implementation
 */
class FSocketSubsystemBSDIPv6
	: public FSocketSubsystemBSDCommon
{
public:

	//~ Begin ISocketSubsystem Interface

	virtual TSharedRef<FInternetAddr> CreateInternetAddr( uint32 Address = 0, uint32 Port = 0 ) override;

	virtual class FSocket* CreateSocket( const FName& SocketType, const FString& SocketDescription, bool bForceUDP = false ) override;

	virtual FResolveInfoCached* CreateResolveInfoCached(TSharedPtr<FInternetAddr> Addr) const override;

	virtual void DestroySocket( class FSocket* Socket ) override;

	virtual ESocketErrors GetHostByName( const ANSICHAR* HostName, FInternetAddr& OutAddr ) override;

	virtual bool GetHostName( FString& HostName ) override;

	virtual ESocketErrors GetLastErrorCode( ) override;

	virtual bool GetLocalAdapterAddresses( TArray<TSharedPtr<FInternetAddr> >& OutAdresses ) override
	{
		bool bCanBindAll;

		OutAdresses.Add(GetLocalHostAddr(*GLog, bCanBindAll));

		return true;
	}

	virtual const TCHAR* GetSocketAPIName( ) const override;

	virtual bool RequiresChatDataBeSeparate( ) override
	{
		return false;
	}

	virtual bool RequiresEncryptedPackets( ) override
	{
		return false;
	}

	virtual ESocketErrors TranslateErrorCode( int32 Code ) override;

	//~ End ISocketSubsystem Interface


protected:

	/**
	 * Allows a subsystem subclass to create a FSocketBSD sub class.
	 */
	virtual class FSocketBSDIPv6* InternalBSDSocketFactory( SOCKET Socket, ESocketType SocketType, const FString& SocketDescription );

	// allow BSD sockets to use this when creating new sockets from accept() etc
	friend FSocketBSDIPv6;

private:

	// Used to prevent multiple threads accessing the shared data.
	FCriticalSection HostByNameSynch;
};
#endif
