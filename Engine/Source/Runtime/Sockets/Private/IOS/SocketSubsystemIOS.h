// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SocketSubsystem.h"
#include "BSDIPv6Sockets/SocketSubsystemBSDIPv6.h"
#include "SocketSubsystemPackage.h"

/**
 * Windows specific socket subsystem implementation
 */
class FSocketSubsystemIOS : public FSocketSubsystemBSDIPv6
{
protected:
	virtual TSharedRef<FInternetAddr> CreateInternetAddr(uint32 Address = 0, uint32 Port = 0) override;
	/** Single instantiation of this subsystem */
	static FSocketSubsystemIOS* SocketSingleton;

	/** Whether Init() has been called before or not */
	bool bTriedToInit;

	// @todo ios: This is kind of hacky, since there's no UBT that should set PACKAGE_SCOPE
// PACKAGE_SCOPE:
public:

	/** 
	 * Singleton interface for this subsystem 
	 * @return the only instance of this subsystem
	 */
	static FSocketSubsystemIOS* Create();

	/**
	 * Performs Windows specific socket clean up
	 */
	static void Destroy();

public:

	FSocketSubsystemIOS() 
		: bTriedToInit(false)
	{
	}

	virtual ~FSocketSubsystemIOS()
	{
	}

	virtual bool Init(FString& Error) override;
	virtual void Shutdown() override;
	virtual bool HasNetworkDevice() override;
	virtual FSocket* CreateSocket(const FName& SocketType, const FString& SocketDescription, bool bForceUDP) override;

	virtual TSharedRef<FInternetAddr> GetLocalHostAddr(FOutputDevice& Out, bool& bCanBindAll) override;
	virtual class FSocketBSDIPv6* InternalBSDSocketFactory(SOCKET Socket, ESocketType SocketType, const FString& SocketDescription) override;
};