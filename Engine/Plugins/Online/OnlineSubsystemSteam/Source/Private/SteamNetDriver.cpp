// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SteamNetDriver.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystem.h"
#include "SocketSubsystem.h"
#include "OnlineSubsystemSteamPrivate.h"
#include "SocketsSteam.h"
#include "SteamNetConnection.h"

USteamNetDriver::USteamNetDriver(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	bIsPassthrough(false)
{
}

void USteamNetDriver::PostInitProperties()
{
	Super::PostInitProperties();
	ConnectionDumpInterval = 10.0f;
	ConnectionDumpCounter = 0.0f;
}

bool USteamNetDriver::IsAvailable() const
{
	// Net driver won't work if the online and socket subsystems don't exist
	IOnlineSubsystem* SteamSubsystem = IOnlineSubsystem::Get(STEAM_SUBSYSTEM);
	if (SteamSubsystem)
	{
		ISocketSubsystem* SteamSockets = ISocketSubsystem::Get(STEAM_SUBSYSTEM);
		if (SteamSockets)
		{
			return true;
		}
	}

	return false;
}

ISocketSubsystem* USteamNetDriver::GetSocketSubsystem()
{
	return ISocketSubsystem::Get(bIsPassthrough ? PLATFORM_SOCKETSUBSYSTEM : STEAM_SUBSYSTEM);
}

bool USteamNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error)
{
	if (bIsPassthrough)
	{
		return UIpNetDriver::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error);
	}

	// Skip UIpNetDriver implementation
	if (!UNetDriver::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();
	if (SocketSubsystem == NULL)
	{
		UE_LOG(LogNet, Warning, TEXT("Unable to find socket subsystem"));
		Error = TEXT("Unable to find socket subsystem");
		return false;
	}

	if(Socket == NULL)
	{
		Socket = 0;
		Error = FString::Printf( TEXT("SteamSockets: socket failed (%i)"), (int32)SocketSubsystem->GetLastErrorCode() );
		return false;
	}

	// Bind socket to our port.
	LocalAddr = SocketSubsystem->GetLocalBindAddr(*GLog);

	// Set the Steam channel (port) to communicate on (both Client/Server communicate on same "channel")
	LocalAddr->SetPort(URL.Port);

	int32 AttemptPort = LocalAddr->GetPort();
	int32 BoundPort = SocketSubsystem->BindNextPort(Socket, *LocalAddr, MaxPortCountToTry + 1, 1);
	UE_LOG(LogNet, Display, TEXT("%s bound to port %d"), *GetName(), BoundPort);
	// Success.
	return true;
}

bool USteamNetDriver::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	ISocketSubsystem* SteamSockets = ISocketSubsystem::Get(STEAM_SUBSYSTEM);
	if (SteamSockets)
	{
		// If we are opening a Steam URL, create a Steam client socket
		if (ConnectURL.Host.StartsWith(STEAM_URL_PREFIX))
		{
			Socket = SteamSockets->CreateSocket(FName(TEXT("SteamClientSocket")), TEXT("Unreal client (Steam)"));
		}
		else
		{
			bIsPassthrough = true;
		}
	}

	return Super::InitConnect(InNotify, ConnectURL, Error);
}

bool USteamNetDriver::InitListen(FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error)
{
	ISocketSubsystem* SteamSockets = ISocketSubsystem::Get(STEAM_SUBSYSTEM);
	if (SteamSockets && !ListenURL.HasOption(TEXT("bIsLanMatch")))
	{
		FName SocketTypeName = IsRunningDedicatedServer() ? FName(TEXT("SteamServerSocket")) : FName(TEXT("SteamClientSocket"));
		Socket = SteamSockets->CreateSocket(SocketTypeName, TEXT("Unreal server (Steam)"));
	}
	else
	{
        // Socket will be created in base class
		bIsPassthrough = true;
	}

	return Super::InitListen(InNotify, ListenURL, bReuseAddressAndPort, Error);
}

void USteamNetDriver::Shutdown()
{
	if (!bIsPassthrough)
	{
		FSocketSteam* SteamSocket = (FSocketSteam*)Socket;
		if (SteamSocket)
		{
			SteamSocket->SetSteamSendMode(k_EP2PSendUnreliableNoDelay);
		}
	}

	Super::Shutdown();
}

void USteamNetDriver::TickFlush(float DeltaSeconds)
{
	Super::TickFlush(DeltaSeconds);

	if (!bIsPassthrough)
	{
		// Debug connection state information
		double CurSeconds = FPlatformTime::Seconds();
		if ((CurSeconds - ConnectionDumpCounter) >= ConnectionDumpInterval)
		{
			ConnectionDumpCounter = CurSeconds;

			USteamNetConnection* ServerConn = Cast<USteamNetConnection>(ServerConnection);
			if (ServerConn != NULL)
			{
				ServerConn->DumpSteamConnection();	
			}

			USteamNetConnection* ClientConn = NULL;
			for (int32 ConnIdx=0; ConnIdx < ClientConnections.Num(); ConnIdx++)
			{
				ClientConn = Cast<USteamNetConnection>(ClientConnections[ConnIdx]);
				if (ClientConn != NULL)
				{
					ClientConn->DumpSteamConnection();
				}
			}
		}
	}
}

bool USteamNetDriver::IsNetResourceValid()
{
	bool bIsValidSteamSocket = !bIsPassthrough && (Socket != NULL) && ((FSocketSteam*)Socket)->LocalSteamId.IsValid();
	bool bIsValidPassthroughSocket = bIsPassthrough && UIpNetDriver::IsNetResourceValid();
	return bIsValidSteamSocket || bIsValidPassthroughSocket;
}
