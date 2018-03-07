// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BSDSockets/SocketSubsystemBSD.h"
#include "SocketSubsystemPackage.h"

/**
 * Android specific socket subsystem implementation
 */
class FSocketSubsystemHTML5 : public FSocketSubsystemBSD
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
};
