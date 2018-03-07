// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "OnlineSubsystemSteamTypes.h"
#include "Containers/Ticker.h"
#include "OnlineSubsystemSteamPackage.h"

class Error;

/**
 * Windows specific socket subsystem implementation
 */
class FSocketSubsystemSteam : public ISocketSubsystem, public FTickerObjectBase
{
protected:

	/** Single instantiation of this subsystem */
	static FSocketSubsystemSteam* SocketSingleton;

	/** Tracks existing Steamworks sockets, for connection failure/timeout resolution */
	TArray<class FSocketSteam*> SteamSockets;

	/** Tracks existing Steamworks connections, for connection failure/timeout resolution */
	TArray<struct FWeakObjectPtr> SteamConnections;

    /** Keep track of Steam P2P connections */
	struct FSteamP2PConnectionInfo
	{
        /** Steam networking interface responsible for this connection */
		ISteamNetworking* SteamNetworkingPtr;
		/** Channel to close */
		int32 Channel;
        /** Last time P2P session had activity (RecvFrom, etc) */
		double LastReceivedTime;

		FSteamP2PConnectionInfo() :
			SteamNetworkingPtr(NULL),
			Channel(-1),
			LastReceivedTime(0.0)
		{
		}

		FSteamP2PConnectionInfo(ISteamNetworking* InSteamNetworkingPtr, double InTime, int32 InChannel=-1) :
			SteamNetworkingPtr(InSteamNetworkingPtr),
			Channel(InChannel),
			LastReceivedTime(InTime)
		{
		}
	};

    /** List of Steam P2P connections being tracked */
	TMap<class FUniqueNetIdSteam, FSteamP2PConnectionInfo> AcceptedConnections;
	/** List of Steam P2P connections to shutdown (dead connections remain around a few seconds longer to flush) */
	TMap<class FUniqueNetIdSteam, FSteamP2PConnectionInfo> DeadConnections;

	/**
	 * Should Steam P2P sockets all fall back to Steam servers relay if a direct connection fails
	 * read from [OnlineSubsystemSteam.bAllowP2PPacketRelay]
	 */
	bool bAllowP2PPacketRelay;
	/** 
     * Timeout period for any P2P session
	 * read from [OnlineSubsystemSteam.P2PConnectionTimeout]
     * (should be at least as long as NetDriver::ConnectionTimeout) 
     */
	float P2PConnectionTimeout;
	/** Accumulated time before next dump of connection info */
	double P2PDumpCounter;
	/** Connection info output interval */
	double P2PDumpInterval;

	/**
	 * Adds a steam socket for tracking
	 *
	 * @param InSocket	The socket to add for tracking
	 */
	void AddSocket(class FSocketSteam* InSocket)
	{
		SteamSockets.Add(InSocket);
	}

	/**
	 * Removes a steam socket from tracking
	 *
	 * @param InSocket	The socket to remove from tracking
	 */
	void RemoveSocket(class FSocketSteam* InSocket)
	{
		SteamSockets.RemoveSingleSwap(InSocket);
	}

PACKAGE_SCOPE:

	/** Last error set by the socket subsystem or one of its sockets */
	int32 LastSocketError;

	/** 
	 * Singleton interface for this subsystem 
	 * @return the only instance of this subsystem
	 */
	static FSocketSubsystemSteam* Create();

	/**
	 * Performs Windows specific socket clean up
	 */
	static void Destroy();

	/**
	 * Iterate through the pending dead connections and permanently remove any that have been around
	 * long enough to flush their contents
	 */
	void CleanupDeadConnections();

	/**
	 * Associate the game server steam id with any sockets that were created prior to successful login
	 *
	 * @param GameServerId id assigned to this game server
	 */
	void FixupSockets(const FUniqueNetIdSteam& GameServerId);

	/**
	 * Adds a steam connection for tracking
	 *
	 * @param Connection The connection to add for tracking
	 */
	void RegisterConnection(class USteamNetConnection* Connection);

	/**
	 * Removes a steam connection from tracking
	 *
	 * @param Connection The connection to remove from tracking
	 */
	void UnregisterConnection(class USteamNetConnection* Connection);

	/**
	 * Notification from the Steam event layer that a remote connection has completely failed
	 * 
	 * @param RemoteId failure address
	 */
	void ConnectFailure(class FUniqueNetIdSteam& RemoteId);

	/**
	 * Potentially accept an incoming connection from a Steam P2P request
	 * 
	 * @param SteamNetworkingPtr the interface for access the P2P calls (Client/GameServer)
	 * @param RemoteId the id of the incoming request
	 * 
	 * @return true if accepted, false otherwise
	 */
	bool AcceptP2PConnection(ISteamNetworking* SteamNetworkingPtr, FUniqueNetIdSteam& RemoteId);

	/**
	 * Add/update a Steam P2P connection as being recently accessed
	 *
	 * @param SteamNetworkingPtr proper networking interface that this session is communicating on
	 * @param SessionId P2P session recently heard from
     *
     * @return true if the connection is active, false if this is in the dead connections list
	 */
	bool P2PTouch(ISteamNetworking* SteamNetworkingPtr, FUniqueNetIdSteam& SessionId);

	/**
	 * Remove a Steam P2P session from tracking and close the connection
	 *
	 * @param SessionId P2P session to remove
	 * @param Channel channel to close, -1 to close all communication
	 */
	void P2PRemove(FUniqueNetIdSteam& SessionId, int32 Channel = -1);

public:

	FSocketSubsystemSteam() :
	    bAllowP2PPacketRelay(false),
		P2PConnectionTimeout(45.0f),
		P2PDumpCounter(0.0),
		P2PDumpInterval(3.0),
		LastSocketError(0)
	{
	}

	/**
	 * Does Windows platform initialization of the sockets library
	 *
	 * @param Error a string that is filled with error information
	 *
	 * @return true if initialized ok, false otherwise
	 */
	virtual bool Init(FString& Error) override;

	/**
	 * Performs platform specific socket clean up
	 */
	virtual void Shutdown() override;

	/**
	 * Creates a socket
	 *
	 * @Param SocketType type of socket to create (DGram, Stream, etc)
 	 * @param SocketDescription debug description
	 * @param bForceUDP overrides any platform specific protocol with UDP instead
	 *
	 * @return the new socket or NULL if failed
	 */
	virtual class FSocket* CreateSocket(const FName& SocketType, const FString& SocketDescription, bool bForceUDP = false) override;

	/**
	 * Cleans up a socket class
	 *
	 * @param Socket the socket object to destroy
	 */
	virtual void DestroySocket(class FSocket* Socket) override;

	/**
	 * Does a DNS look up of a host name
	 *
	 * @param HostName the name of the host to look up
	 * @param OutAddr the address to copy the IP address to
	 */
	virtual ESocketErrors GetHostByName(const ANSICHAR* HostName, FInternetAddr& OutAddr) override;

	/**
	 * Some platforms require chat data (voice, text, etc.) to be placed into
	 * packets in a special way. This function tells the net connection
	 * whether this is required for this platform
	 */
	virtual bool RequiresChatDataBeSeparate() override
	{
		return false;
	}

	/**
	 * Some platforms require packets be encrypted. This function tells the
	 * net connection whether this is required for this platform
	 */
	virtual bool RequiresEncryptedPackets() override
	{
		return false;
	}

	/**
	 * Determines the name of the local machine
	 *
	 * @param HostName the string that receives the data
	 *
	 * @return true if successful, false otherwise
	 */
	virtual bool GetHostName(FString& HostName) override;

	/**
	 *	Create a proper FInternetAddr representation
	 * @param Address host address
	 * @param Port host port
	 */
	virtual TSharedRef<FInternetAddr> CreateInternetAddr(uint32 Address=0, uint32 Port=0) override;

	/**
	 * @return Whether the machine has a properly configured network device or not
	 */
	virtual bool HasNetworkDevice() override;

	/**
	 *	Get the name of the socket subsystem
	 * @return a string naming this subsystem
	 */
	virtual const TCHAR* GetSocketAPIName() const override;

	/**
	 * Returns the last error that has happened
	 */
	virtual ESocketErrors GetLastErrorCode() override;

	/**
	 * Translates the platform error code to a ESocketErrors enum
	 */
	virtual ESocketErrors TranslateErrorCode(int32 Code) override;

	/**
	 * Gets the list of addresses associated with the adapters on the local computer.
	 *
	 * @param OutAdresses - Will hold the address list.
	 *
	 * @return true on success, false otherwise.
	 */
	virtual bool GetLocalAdapterAddresses( TArray<TSharedPtr<FInternetAddr> >& OutAdresses ) override
	{
		bool bCanBindAll;

		OutAdresses.Add(GetLocalHostAddr(*GLog, bCanBindAll));

		return true;
	}

	/**
	 *	Get local IP to bind to
	 */
	virtual TSharedRef<FInternetAddr> GetLocalBindAddr(FOutputDevice& Out) override;

	/**
	 * Dumps the Steam P2P networking information for a given session state
	 *
	 * @param SessionInfo struct from Steam call to GetP2PSessionState
	 */
	void DumpSteamP2PSessionInfo(P2PSessionState_t& SessionInfo);

	/**
	 * Chance for the socket subsystem to get some time
	 *
	 * @param DeltaTime time since last tick
	 */
	virtual bool Tick(float DeltaTime) override;
};

/**
 * Create the socket subsystem for the given platform service
 */
FName CreateSteamSocketSubsystem();

/**
 * Tear down the socket subsystem for the given platform service
 */
void DestroySteamSocketSubsystem();
