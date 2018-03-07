// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SocketSubsystemSteam.h"
#include "Misc/ConfigCacheIni.h"
#include "SocketsSteam.h"
#include "IPAddressSteam.h"
#include "OnlineSessionInterfaceSteam.h"
#include "SocketSubsystemModule.h"
#include "SteamNetConnection.h"

FSocketSubsystemSteam* FSocketSubsystemSteam::SocketSingleton = NULL;

/**
 * Create the socket subsystem for the given platform service
 */
FName CreateSteamSocketSubsystem()
{
	// Create and register our singleton factory with the main online subsystem for easy access
	FSocketSubsystemSteam* SocketSubsystem = FSocketSubsystemSteam::Create();
	FString Error;
	if (SocketSubsystem->Init(Error))
	{
		FSocketSubsystemModule& SSS = FModuleManager::LoadModuleChecked<FSocketSubsystemModule>("Sockets");
		SSS.RegisterSocketSubsystem(STEAM_SUBSYSTEM, SocketSubsystem, true);
		return STEAM_SUBSYSTEM;
	}
	else
	{
		FSocketSubsystemSteam::Destroy();
		return NAME_None;
	}
}

/**
 * Tear down the socket subsystem for the given platform service
 */
void DestroySteamSocketSubsystem()
{
	FModuleManager& ModuleManager = FModuleManager::Get();

	if (ModuleManager.IsModuleLoaded("Sockets"))
	{
		FSocketSubsystemModule& SSS = FModuleManager::GetModuleChecked<FSocketSubsystemModule>("Sockets");
		SSS.UnregisterSocketSubsystem(STEAM_SUBSYSTEM);
	}
	FSocketSubsystemSteam::Destroy();
}

/** 
 * Singleton interface for this subsystem 
 * @return the only instance of this subsystem
 */
FSocketSubsystemSteam* FSocketSubsystemSteam::Create()
{
	if (SocketSingleton == NULL)
	{
		SocketSingleton = new FSocketSubsystemSteam();
	}

	return SocketSingleton;
}

/**
 * Performs Steam specific socket clean up
 */
void FSocketSubsystemSteam::Destroy()
{
	if (SocketSingleton != NULL)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = NULL;
	}
}

/**
 * Does Steam platform initialization of the sockets library
 *
 * @param Error a string that is filled with error information
 *
 * @return true if initialized ok, false otherwise
 */
bool FSocketSubsystemSteam::Init(FString& Error)
{
	if (GConfig)
	{
		if (!GConfig->GetBool(TEXT("OnlineSubsystemSteam"), TEXT("bAllowP2PPacketRelay"), bAllowP2PPacketRelay, GEngineIni))
		{
			UE_LOG_ONLINE(Warning, TEXT("Missing bAllowP2PPacketRelay key in OnlineSubsystemSteam of DefaultEngine.ini"));
		}

		if (!GConfig->GetFloat(TEXT("OnlineSubsystemSteam"), TEXT("P2PConnectionTimeout"), P2PConnectionTimeout, GEngineIni))
		{
			UE_LOG_ONLINE(Warning, TEXT("Missing P2PConnectionTimeout key in OnlineSubsystemSteam of DefaultEngine.ini"));
		}
	}

	if (SteamNetworking())
	{
		SteamNetworking()->AllowP2PPacketRelay(bAllowP2PPacketRelay);
	}

	if (SteamGameServerNetworking())
	{
		SteamGameServerNetworking()->AllowP2PPacketRelay(bAllowP2PPacketRelay);
	}

	return true;
}

/**
 * Performs platform specific socket clean up
 */
void FSocketSubsystemSteam::Shutdown()
{
	for (int32 ConnIdx=SteamConnections.Num()-1; ConnIdx>=0; ConnIdx--)
	{
		if (SteamConnections[ConnIdx].IsValid())
		{
			USteamNetConnection* SteamConn = CastChecked<USteamNetConnection>(SteamConnections[ConnIdx].Get());
			UnregisterConnection(SteamConn);
		}
	}

	// Cleanup any remaining sessions
	TArray<FUniqueNetIdSteam> SessionIds;
	AcceptedConnections.GenerateKeyArray(SessionIds);
	for (int SessionIdx=0; SessionIdx < SessionIds.Num(); SessionIdx++)
	{
		P2PRemove(SessionIds[SessionIdx]);
	}
	
	CleanupDeadConnections();

	// Cleanup sockets
	TArray<FSocketSteam*> TempArray = SteamSockets;
	for (int SocketIdx=0; SocketIdx < TempArray.Num(); SocketIdx++)
	{
		DestroySocket(TempArray[SocketIdx]);
	}

	SteamSockets.Empty();
	SteamConnections.Empty();
	AcceptedConnections.Empty();
	DeadConnections.Empty();
}

/**
 * Creates a socket
 *
 * @Param SocketType type of socket to create (DGram, Stream, etc)
 * @param SocketDescription debug description
 * @param bForceUDP overrides any platform specific protocol with UDP instead
 *
 * @return the new socket or NULL if failed
 */
FSocket* FSocketSubsystemSteam::CreateSocket(const FName& SocketType, const FString& SocketDescription, bool bForceUDP)
{
	FSocket* NewSocket = NULL;
	if (SocketType == FName("SteamClientSocket"))
	{
		ISteamUser* SteamUserPtr = SteamUser();
		if (SteamUserPtr != NULL)
		{
			FUniqueNetIdSteam ClientId(SteamUserPtr->GetSteamID());
			NewSocket = new FSocketSteam(SteamNetworking(), ClientId, SocketDescription);

			if (NewSocket)
			{
				AddSocket((FSocketSteam*)NewSocket);
			}
		}
	}
	else if (SocketType == FName("SteamServerSocket"))
	{
		IOnlineSubsystem* SteamSubsystem = IOnlineSubsystem::Get(STEAM_SUBSYSTEM);
		FOnlineSessionSteamPtr SessionInt = StaticCastSharedPtr<FOnlineSessionSteam>(SteamSubsystem->GetSessionInterface());
		if (SessionInt.IsValid())
		{
			// If the GameServer connection hasn't been created yet, mark the socket as invalid for now
			if (SessionInt->bSteamworksGameServerConnected && SessionInt->GameServerSteamId->IsValid() && SessionInt->bPolicyResponseReceived)
			{
				NewSocket = new FSocketSteam(SteamGameServerNetworking(), *SessionInt->GameServerSteamId, SocketDescription);
			}
			else
			{
				FUniqueNetIdSteam InvalidId(uint64(0));
				NewSocket = new FSocketSteam(SteamGameServerNetworking(), InvalidId, SocketDescription);
			}

			if (NewSocket)
			{
				AddSocket((FSocketSteam*)NewSocket);
			}
		}
	}
	else
	{
		ISocketSubsystem* PlatformSocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (PlatformSocketSub)
		{
			NewSocket = PlatformSocketSub->CreateSocket(SocketType, SocketDescription, bForceUDP);
		}
	}
	
	if (!NewSocket)
	{
		UE_LOG(LogSockets, Warning, TEXT("Failed to create socket %s [%s]"), *SocketType.ToString(), *SocketDescription);
	}

	return NewSocket;
}

/**
 * Cleans up a socket class
 *
 * @param Socket the socket object to destroy
 */
void FSocketSubsystemSteam::DestroySocket(FSocket* Socket)
{
	// Possible non steam socket here PLATFORM_SOCKETSUBSYSTEM, but its just a pointer compare
	RemoveSocket((FSocketSteam*)Socket);
	delete Socket;
}

/**
 * Associate the game server steam id with any sockets that were created prior to successful login
 *
 * @param GameServerId id assigned to this game server
 */
void FSocketSubsystemSteam::FixupSockets(const FUniqueNetIdSteam& GameServerId)
{
	for (int32 SockIdx = 0; SockIdx < SteamSockets.Num(); SockIdx++)
	{
		FSocketSteam* Socket = SteamSockets[SockIdx];
		if (Socket->SteamNetworkingPtr == SteamGameServerNetworking() && !Socket->LocalSteamId.IsValid())
		{
			Socket->LocalSteamId = GameServerId;
		}
	}
}

/**
 * Adds a steam connection for tracking
 *
 * @param Connection The connection to add for tracking
 */
void FSocketSubsystemSteam::RegisterConnection(USteamNetConnection* Connection)
{
	check(!Connection->bIsPassthrough);

	FWeakObjectPtr ObjectPtr = Connection;
	SteamConnections.Add(ObjectPtr);

	if (Connection->RemoteAddr.IsValid() && Connection->Socket)
	{
		FSocketSteam* SteamSocket = (FSocketSteam*)Connection->Socket;
		FInternetAddrSteam& SteamAddr = (FInternetAddrSteam&)(*Connection->RemoteAddr);
		P2PTouch(SteamSocket->SteamNetworkingPtr, SteamAddr.SteamId);
	}
}

/**
 * Removes a steam connection from tracking
 *
 * @param Connection The connection to remove from tracking
 */
void FSocketSubsystemSteam::UnregisterConnection(USteamNetConnection* Connection)
{
	check(!Connection->bIsPassthrough);

	FWeakObjectPtr ObjectPtr = Connection;
	int32 NumRemoved = SteamConnections.RemoveSingleSwap(ObjectPtr);

	// Don't call P2PRemove again if we didn't actually remove a connection. This 
	// will get called twice - once the connection is closed and when the connection
	// is garbage collected. It's possible that the player who left rejoined before garbage
	// collection runs (their connection object will be different), so P2PRemove would kick
	// them from the session when it shouldn't.
	if (NumRemoved > 0 && Connection->RemoteAddr.IsValid())
	{
		FInternetAddrSteam& SteamAddr = (FInternetAddrSteam&)(*Connection->RemoteAddr);
		P2PRemove(SteamAddr.SteamId, SteamAddr.SteamChannel);
	}
}

void FSocketSubsystemSteam::ConnectFailure(FUniqueNetIdSteam& RemoteId)
{
	// Remove any GC'd references
	for (int32 ConnIdx=SteamConnections.Num()-1; ConnIdx>=0; ConnIdx--)
	{
		if (!SteamConnections[ConnIdx].IsValid())
		{
			SteamConnections.RemoveAt(ConnIdx);
		}
	}

	// Find the relevant connections and shut them down
	for (int32 ConnIdx=0; ConnIdx<SteamConnections.Num(); ConnIdx++)
	{
		USteamNetConnection* SteamConn = CastChecked<USteamNetConnection>(SteamConnections[ConnIdx].Get());
		FInternetAddrSteam& RemoteAddrSteam = (FInternetAddrSteam&)*(SteamConn->RemoteAddr);

		// Only checking Id here because its a complete failure (channel doesn't matter)
		if (RemoteAddrSteam.SteamId == RemoteId)
		{
			SteamConn->Close();
		}
	}

	P2PRemove(RemoteId);
}

/**
 * Does a DNS look up of a host name
 *
 * @param HostName the name of the host to look up
 * @param OutAddr the address to copy the IP address to
 */
ESocketErrors FSocketSubsystemSteam::GetHostByName(const ANSICHAR* HostName, FInternetAddr& OutAddr)
{
	return SE_EADDRNOTAVAIL;
}

/**
 * Determines the name of the local machine
 *
 * @param HostName the string that receives the data
 *
 * @return true if successful, false otherwise
 */
bool FSocketSubsystemSteam::GetHostName(FString& HostName)
{
	return false;
}

/**
 *	Create a proper FInternetAddr representation
 * @param Address host address
 * @param Port host port
 */
TSharedRef<FInternetAddr> FSocketSubsystemSteam::CreateInternetAddr(uint32 Address, uint32 Port)
{
	FInternetAddrSteam* SteamAddr = new FInternetAddrSteam();
	return MakeShareable(SteamAddr);
}

/**
 * @return Whether the machine has a properly configured network device or not
 */
bool FSocketSubsystemSteam::HasNetworkDevice() 
{
	return true;
}

/**
 *	Get the name of the socket subsystem
 * @return a string naming this subsystem
 */
const TCHAR* FSocketSubsystemSteam::GetSocketAPIName() const 
{
	return TEXT("SteamSockets");
}

/**
 * Returns the last error that has happened
 */
ESocketErrors FSocketSubsystemSteam::GetLastErrorCode()
{
	return TranslateErrorCode(LastSocketError);
}

/**
 * Translates the platform error code to a ESocketErrors enum
 */
ESocketErrors FSocketSubsystemSteam::TranslateErrorCode(int32 Code)
{
	// @TODO ONLINE - This needs to be filled in (at present it is 1:1)
	return (ESocketErrors)LastSocketError;
}

/**
 *	Get local IP to bind to
 */
TSharedRef<FInternetAddr> FSocketSubsystemSteam::GetLocalBindAddr(FOutputDevice& Out)
{
	FInternetAddrSteam* SteamAddr = NULL;
	CSteamID SteamId;
	if (SteamUser())
	{
		// Prefer the steam user
		SteamId = SteamUser()->GetSteamID();
		SteamAddr = new FInternetAddrSteam(FUniqueNetIdSteam(SteamId));
	}
	else if (SteamGameServer() && SteamGameServer()->BLoggedOn())
	{
		// Dedicated server 
		SteamId = SteamGameServer()->GetSteamID();
		SteamAddr = new FInternetAddrSteam(FUniqueNetIdSteam(SteamId));
	}
	else
	{
		// Empty/invalid case
		SteamAddr = new FInternetAddrSteam();
	}

	return MakeShareable(SteamAddr);
}

/**
 * Potentially accept an incoming connection from a Steam P2P request
 * 
 * @param SteamNetworkingPtr the interface for access the P2P calls (Client/GameServer)
 * @param RemoteId the id of the incoming request
 * 
 * @return true if accepted, false otherwise
 */
bool FSocketSubsystemSteam::AcceptP2PConnection(ISteamNetworking* SteamNetworkingPtr, FUniqueNetIdSteam& RemoteId)
{
	if (SteamNetworkingPtr && RemoteId.IsValid() && DeadConnections.Find(RemoteId) == NULL)
	{
		// Blindly accept connections (but only if P2P enabled)
		SteamNetworkingPtr->AcceptP2PSessionWithUser(RemoteId);
		AcceptedConnections.Add(RemoteId, FSteamP2PConnectionInfo(SteamNetworkingPtr, FPlatformTime::Seconds()));
		return true;
	}

	return false;
}

/**
 * Add/update a Steam P2P connection as being recently accessed
 *
 * @param SteamNetworkingPtr proper networking interface that this session is communicating on
 * @param SessionId P2P session recently heard from
 *
 * @return true if the connection is active, false if this is in the dead connections list
 */
bool FSocketSubsystemSteam::P2PTouch(ISteamNetworking* SteamNetworkingPtr, FUniqueNetIdSteam& SessionId)
{
    // Don't update any sessions coming from pending disconnects
	if (DeadConnections.Find(SessionId) == NULL)
	{
		AcceptedConnections.Add(SessionId, FSteamP2PConnectionInfo(SteamNetworkingPtr, FPlatformTime::Seconds()));
		return true;
	}

	return false;
}
	
/**
 * Remove a Steam P2P session from tracking and close the connection
 *
 * @param SessionId P2P session to remove
 * @param Channel channel to close, -1 to close all communication
 */
void FSocketSubsystemSteam::P2PRemove(FUniqueNetIdSteam& SessionId, int32 Channel)
{
	FSteamP2PConnectionInfo* ConnectionInfo = AcceptedConnections.Find(SessionId);
	if (ConnectionInfo)
	{
        // Move active connections to the dead list so they can be removed (giving Steam a chance to flush connection)
		DeadConnections.Add(SessionId, FSteamP2PConnectionInfo(ConnectionInfo->SteamNetworkingPtr, FPlatformTime::Seconds(), Channel));
	}

	UE_LOG_ONLINE(Log, TEXT("Removing P2P Session Id: %s, IdleTime: %0.3f"), *SessionId.ToDebugString(), ConnectionInfo ? (FPlatformTime::Seconds() - ConnectionInfo->LastReceivedTime) : 9999.f);
	AcceptedConnections.Remove(SessionId);
}

/**
 * Chance for the socket subsystem to get some time
 *
 * @param DeltaTime time since last tick
 */
bool FSocketSubsystemSteam::Tick(float DeltaTime)
{	
	double CurSeconds = FPlatformTime::Seconds();

	// Debug connection state information
	bool bDumpSessionInfo = false;
	if ((CurSeconds - P2PDumpCounter) >= P2PDumpInterval)
	{
		P2PDumpCounter = CurSeconds;
		bDumpSessionInfo = true;
	}

	TArray<FUniqueNetIdSteam> ExpiredSessions;
	for (TMap<FUniqueNetIdSteam, FSteamP2PConnectionInfo>::TConstIterator It(AcceptedConnections); It; ++It)
	{
		const FUniqueNetIdSteam& SessionId = It.Key();
		const FSteamP2PConnectionInfo& ConnectionInfo = It.Value();

		bool bExpiredSession = true;
		if (CurSeconds - ConnectionInfo.LastReceivedTime < P2PConnectionTimeout)
		{
			P2PSessionState_t SessionInfo;
			if (ConnectionInfo.SteamNetworkingPtr != NULL && ConnectionInfo.SteamNetworkingPtr->GetP2PSessionState(SessionId, &SessionInfo))
			{
				bExpiredSession = false;

				if (bDumpSessionInfo)
				{
					UE_LOG_ONLINE(Verbose, TEXT("Dumping Steam P2P socket details:"));
					UE_LOG_ONLINE(Verbose, TEXT("- Id: %s, IdleTime: %0.3f"), *SessionId.ToDebugString(), (CurSeconds - ConnectionInfo.LastReceivedTime));

					DumpSteamP2PSessionInfo(SessionInfo);
				}
			}
			else
			{
				UE_LOG_ONLINE(Verbose, TEXT("Failed to get Steam P2P session state for Id: %s, IdleTime: %0.3f"), *SessionId.ToDebugString(), (CurSeconds - ConnectionInfo.LastReceivedTime));
			}
		}

		if (bExpiredSession)
		{
			ExpiredSessions.Add(SessionId);
		}
	}

	// Cleanup any closed/invalid sessions
	for (int32 SessionIdx=0; SessionIdx < ExpiredSessions.Num(); SessionIdx++)
	{
		FUniqueNetIdSteam& SessionId = ExpiredSessions[SessionIdx];
		P2PRemove(SessionId);
	}

	CleanupDeadConnections();

	return true;
}

/**
 * Iterate through the pending dead connections and permanently remove any that have been around
 * long enough to flush their contents
 */
void FSocketSubsystemSteam::CleanupDeadConnections()
{
	double CurSeconds = FPlatformTime::Seconds();
	TArray<FUniqueNetIdSteam> ExpiredSessions;
	for (TMap<FUniqueNetIdSteam, FSteamP2PConnectionInfo>::TConstIterator It(DeadConnections); It; ++It)
	{
		const FUniqueNetIdSteam& SessionId = It.Key();
		const FSteamP2PConnectionInfo& ConnectionInfo = It.Value();
		if (CurSeconds - ConnectionInfo.LastReceivedTime >= 3.0f)
		{
			if (ConnectionInfo.Channel == -1)
			{
				ConnectionInfo.SteamNetworkingPtr->CloseP2PSessionWithUser(SessionId);
			}
			else
			{
				ConnectionInfo.SteamNetworkingPtr->CloseP2PChannelWithUser(SessionId, ConnectionInfo.Channel);
			}

			ExpiredSessions.Add(SessionId);
		}
	}

	for (int32 SessionIdx=0; SessionIdx < ExpiredSessions.Num(); SessionIdx++)
	{
		FUniqueNetIdSteam& SessionId = ExpiredSessions[SessionIdx];
		DeadConnections.Remove(SessionId);
	}
}

/**
 * Dumps the Steam P2P networking information for a given session id
 *
 * @param SessionInfo struct from Steam call to GetP2PSessionState
 */
void FSocketSubsystemSteam::DumpSteamP2PSessionInfo(P2PSessionState_t& SessionInfo)
{
	TSharedRef<FInternetAddr> IpAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr(SessionInfo.m_nRemoteIP, SessionInfo.m_nRemotePort);
	UE_LOG_ONLINE(Verbose, TEXT("- Detailed P2P session info:"));
	UE_LOG_ONLINE(Verbose, TEXT("-- IPAddress: %s"), *IpAddr->ToString(true));
	UE_LOG_ONLINE(Verbose, TEXT("-- ConnectionActive: %i, Connecting: %i, SessionError: %i, UsingRelay: %i"),
		SessionInfo.m_bConnectionActive, SessionInfo.m_bConnecting, SessionInfo.m_eP2PSessionError,
		SessionInfo.m_bUsingRelay);
	UE_LOG_ONLINE(Verbose, TEXT("-- QueuedBytes: %i, QueuedPackets: %i"), SessionInfo.m_nBytesQueuedForSend,
		SessionInfo.m_nPacketsQueuedForSend);
}
