// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SteamNetConnection.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystem.h"
#include "SocketSubsystem.h"
#include "OnlineSubsystemSteamPrivate.h"
#include "IPAddressSteam.h"
#include "SocketSubsystemSteam.h"
#include "SocketsSteam.h"
#include "SteamNetDriver.h"

USteamNetConnection::USteamNetConnection(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	bIsPassthrough(false)
{
}

void USteamNetConnection::InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	bIsPassthrough = InURL.Host.StartsWith(STEAM_URL_PREFIX) ? false : true;
	
	Super::InitLocalConnection(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);
	if (!bIsPassthrough && RemoteAddr.IsValid())
	{
		FSocketSubsystemSteam* SocketSubsystem = (FSocketSubsystemSteam*)ISocketSubsystem::Get(STEAM_SUBSYSTEM);
		if (SocketSubsystem)
		{
			SocketSubsystem->RegisterConnection(this);
		}
	}
}

void USteamNetConnection::InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	bIsPassthrough = ((USteamNetDriver*)InDriver)->bIsPassthrough;

	Super::InitRemoteConnection(InDriver, InSocket, InURL, InRemoteAddr, InState, InMaxPacket, InPacketOverhead);
	if (!bIsPassthrough && RemoteAddr.IsValid())
	{
		FSocketSubsystemSteam* SocketSubsystem = (FSocketSubsystemSteam*)ISocketSubsystem::Get(STEAM_SUBSYSTEM);
		if (SocketSubsystem)
		{
			SocketSubsystem->RegisterConnection(this);
		}
	}
}

void USteamNetConnection::CleanUp()
{
	Super::CleanUp();

	if (!bIsPassthrough)
	{
		FSocketSubsystemSteam* SocketSubsystem = (FSocketSubsystemSteam*)ISocketSubsystem::Get(STEAM_SUBSYSTEM);
		if (SocketSubsystem)
		{
			// Unregister the connection AFTER the parent class has had a chance to close/flush connections
			SocketSubsystem->UnregisterConnection(this);
		}
	}
}

void USteamNetConnection::DumpSteamConnection()
{
	if (!bIsPassthrough && RemoteAddr.IsValid())
	{
		UE_LOG_ONLINE(Verbose, TEXT("%s: Dumping Steam P2P connection details:"), *GetName());
		FInternetAddrSteam& SteamAddr = (FInternetAddrSteam&)(*RemoteAddr); 
		FUniqueNetIdSteam& SessionId = SteamAddr.SteamId;
		UE_LOG_ONLINE(Verbose, TEXT("- Id: %s, IdleTime: %0.3f"), *SessionId.ToDebugString(), (Driver->Time - LastReceiveTime));
	
		if (Socket)
		{
			FSocketSteam* SteamSocket = (FSocketSteam*)Socket;

			FSocketSubsystemSteam* SteamSockets = (FSocketSubsystemSteam*)ISocketSubsystem::Get(STEAM_SUBSYSTEM);
			if (SteamSockets)
			{
				P2PSessionState_t SessionInfo;
				if (SteamSocket->SteamNetworkingPtr != NULL && SteamSocket->SteamNetworkingPtr->GetP2PSessionState(SessionId, &SessionInfo))
				{
					SteamSockets->DumpSteamP2PSessionInfo(SessionInfo);
				}
				else
				{
					UE_LOG_ONLINE(Verbose, TEXT("Failed to get Steam P2P session state for Id: %s"), *SessionId.ToDebugString());
				}
			}
		}
	}
}
