// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BSDSockets/SocketSubsystemBSD.h"
#include "SocketSubsystemPackage.h"

class Error;
class FInternetAddr;

/**
 * Android specific socket subsystem implementation
 */
class FSocketSubsystemLinux : public FSocketSubsystemBSD
{
protected:

	/** Single instantiation of this subsystem */
	static FSocketSubsystemLinux* SocketSingleton;

	/** Whether Init() has been called before or not */
	bool bTriedToInit;

	// @todo linux: (inherited from iOS) This is kind of hacky, since there's no UBT that should set PACKAGE_SCOPE
// PACKAGE_SCOPE:
public:

	/** 
	 * Singleton interface for this subsystem 
	 * @return the only instance of this subsystem
	 */
	static FSocketSubsystemLinux* Create();

	/**
	 * Performs Android specific socket clean up
	 */
	static void Destroy();

public:

	FSocketSubsystemLinux() 
		: bTriedToInit(false)
	{
	}

	virtual ~FSocketSubsystemLinux()
	{
	}

	// ISocketSubsystem
	virtual bool Init(FString& Error) override;
	virtual void Shutdown() override;
	virtual bool HasNetworkDevice() override;
	virtual TSharedRef<FInternetAddr> GetLocalHostAddr(FOutputDevice& Out, bool& bCanBindAll) override;
};
