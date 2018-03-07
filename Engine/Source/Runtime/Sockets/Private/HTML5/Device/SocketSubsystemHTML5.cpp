// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SocketSubsystemHTML5.h"
#include "SocketSubsystemModule.h"
#include "ModuleManager.h"


FSocketSubsystemHTML5* FSocketSubsystemHTML5::SocketSingleton = NULL;

FName CreateSocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
 	FName SubsystemName(TEXT("HTML5"));
	// Create and register our singleton factor with the main online subsystem for easy access
	FSocketSubsystemHTML5* SocketSubsystem = FSocketSubsystemHTML5::Create();
	FString Error;
	if (SocketSubsystem->Init(Error))
	{
		SocketSubsystemModule.RegisterSocketSubsystem(SubsystemName, SocketSubsystem);
		return SubsystemName;
	}
	else
	{
		FSocketSubsystemHTML5::Destroy();
		return NAME_None;
	}

}

void DestroySocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	SocketSubsystemModule.UnregisterSocketSubsystem(FName(TEXT("HTML5")));
	FSocketSubsystemHTML5::Destroy();
}

/** 
 * Singleton interface for the Android socket subsystem 
 * @return the only instance of the Android socket subsystem
 */
FSocketSubsystemHTML5* FSocketSubsystemHTML5::Create()
{
	if (SocketSingleton == NULL)
	{
		SocketSingleton = new FSocketSubsystemHTML5();
	}

	return SocketSingleton; 
}

/** 
 * Destroy the singleton Android socket subsystem
 */
void FSocketSubsystemHTML5::Destroy()
{
	if (SocketSingleton != NULL)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = NULL;
	}
}


bool FSocketSubsystemHTML5::Init(FString& Error)
{
	return true;
}

/**
 * Performs HTML5 specific socket clean up
 */
void FSocketSubsystemHTML5::Shutdown(void)
{
}


/**
 * @return Whether the device has a properly configured network device or not
 */
bool FSocketSubsystemHTML5::HasNetworkDevice()
{
	return true;
}
