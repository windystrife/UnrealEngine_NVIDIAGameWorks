// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SocketSubsystemMac.h"
#include "SocketSubsystemModule.h"
#include "ModuleManager.h"


FSocketSubsystemMac* FSocketSubsystemMac::SocketSingleton = NULL;

FName CreateSocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	FName SubsystemName(TEXT("MAC"));
	// Create and register our singleton factor with the main online subsystem for easy access
	FSocketSubsystemMac* SocketSubsystem = FSocketSubsystemMac::Create();
	FString Error;
	if (SocketSubsystem->Init(Error))
	{
		SocketSubsystemModule.RegisterSocketSubsystem(SubsystemName, SocketSubsystem);
		return SubsystemName;
	}
	else
	{
		FSocketSubsystemMac::Destroy();
		return NAME_None;
	}
}

void DestroySocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	SocketSubsystemModule.UnregisterSocketSubsystem(FName(TEXT("MAC")));
	FSocketSubsystemMac::Destroy();
}

FSocketSubsystemMac* FSocketSubsystemMac::Create()
{
	if (SocketSingleton == NULL)
	{
		SocketSingleton = new FSocketSubsystemMac();
	}

	return SocketSingleton;
}

void FSocketSubsystemMac::Destroy()
{
	if (SocketSingleton != NULL)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = NULL;
	}
}

bool FSocketSubsystemMac::Init(FString& Error)
{
	return true;
}

void FSocketSubsystemMac::Shutdown(void)
{
}


bool FSocketSubsystemMac::HasNetworkDevice()
{
	return true;
}

class FSocketBSD* FSocketSubsystemMac::InternalBSDSocketFactory(SOCKET Socket, ESocketType SocketType, const FString& SocketDescription)
{
	// return a new socket object
	FSocketMac* MacSock = new FSocketMac(Socket, SocketType, SocketDescription, this);
	if (MacSock)
	{
		// disable the SIGPIPE exception
		int bAllow = 1;
		setsockopt(Socket, SOL_SOCKET, SO_NOSIGPIPE, &bAllow, sizeof(bAllow));
	}
	return MacSock;
}
