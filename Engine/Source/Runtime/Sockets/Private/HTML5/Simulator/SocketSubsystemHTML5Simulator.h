// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SocketSubsystem.h"
#include "SocketSubsystemPackage.h"

// This subsystem is not tested well, is incomplete and only used for debugging HTML5 Platform. Do not use to ship. 

/**
 * HTML5 VS tool chain specific socket subsystem implementation
 *
 */
class FSocketSubsystemHTML5 : public ISocketSubsystem
{
protected:

	/** Single instantiation of this subsystem */
	static FSocketSubsystemHTML5* SocketSingleton;

	/** Whether Init() has been called before or not */
	bool bTriedToInit;

public:

	/** 
	 * Singleton interface for this subsystem 
	 * @return the only instance of this subsystem
	 */
	static FSocketSubsystemHTML5* Create();

	/**
	 * Performs Android specific socket clean up
	 */
	static void Destroy();

public:

	FSocketSubsystemHTML5() 
		: bTriedToInit(false)
	{
	}

	virtual ~FSocketSubsystemHTML5()
	{
	}

	/**
	 * Does Android platform initialization of the sockets library
	 *
	 * @param Error a string that is filled with error information
	 *
	 * @return TRUE if initialized ok, FALSE otherwise
	 */
	virtual bool Init(FString& Error) override;

	/**
	 * Performs platform specific socket clean up
	 */
	virtual void Shutdown() override;

	/**
	 * @return Whether the device has a properly configured network device or not
	 */
	virtual bool HasNetworkDevice() override;

	virtual class FSocket* CreateSocket( const FName& SocketType, const FString& SocketDescription, bool bForceUDP = false );

	virtual void DestroySocket( class FSocket* Socket );

	virtual ESocketErrors GetHostByName( const ANSICHAR* HostName, FInternetAddr& OutAddr );

	virtual bool RequiresChatDataBeSeparate();

	virtual bool RequiresEncryptedPackets();

	virtual bool GetHostName( FString& HostName );

	virtual TSharedRef<FInternetAddr> CreateInternetAddr( uint32 Address=0, uint32 Port=0 );

	virtual const TCHAR* GetSocketAPIName() const;

	virtual ESocketErrors GetLastErrorCode();

	virtual ESocketErrors TranslateErrorCode( int32 Code );

	virtual bool GetLocalAdapterAddresses( TArray<TSharedPtr<FInternetAddr> >& OutAdresses );

};
