// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemSteamPrivate.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemSteamPackage.h"

/**
 * Base class that holds a delegate to fire when a given async task is complete
 */
class FOnlineAsyncTaskSteam : public FOnlineAsyncTaskBasic<class FOnlineSubsystemSteam>
{
PACKAGE_SCOPE:

	/** Unique handle for the Steam async call initiated */
	SteamAPICall_t CallbackHandle;

	/** Hidden on purpose */
	FOnlineAsyncTaskSteam() :
		FOnlineAsyncTaskBasic(NULL),
		CallbackHandle(k_uAPICallInvalid)
	{
	}

public:

	FOnlineAsyncTaskSteam(class FOnlineSubsystemSteam* InSteamSubsystem, SteamAPICall_t InCallbackHandle) :
		FOnlineAsyncTaskBasic(InSteamSubsystem),	
		CallbackHandle(InCallbackHandle)
	{
	}

	virtual ~FOnlineAsyncTaskSteam()
	{
	}
};

/**
 *	Steam version of the async task manager to register the various Steam callbacks with the engine
 */
class FOnlineAsyncTaskManagerSteam : public FOnlineAsyncTaskManager
{
private:

    /** Delegate registered with Steam to trigger when another Steam client makes a P2P call to this instance */
	STEAM_CALLBACK(FOnlineAsyncTaskManagerSteam, OnP2PSessionRequest, P2PSessionRequest_t, OnP2PSessionRequestCallback);
	/** Delegate registered with Steam to trigger when a connection between two steam P2P endpoints fails */
	STEAM_CALLBACK(FOnlineAsyncTaskManagerSteam, OnP2PSessionConnectFail, P2PSessionConnectFail_t, OnP2PSessionConnectFailCallback);
	/** Delegate registered with Steam to trigger when another Steam client makes a P2P call to this instance (gameserver API) */
	STEAM_GAMESERVER_CALLBACK(FOnlineAsyncTaskManagerSteam, OnP2PSessionRequestGS, P2PSessionRequest_t, OnP2PSessionRequestGSCallback);
	/** Delegate registered with Steam to trigger when a connection between two steam P2P endpoints fails (gameserver API) */
	STEAM_GAMESERVER_CALLBACK(FOnlineAsyncTaskManagerSteam, OnP2PSessionConnectFailGS, P2PSessionConnectFail_t, OnP2PSessionConnectFailGSCallback);
	
	/** Delegate registered with Steam to trigger when a user (client API) is connected to the Steam servers  (usually don't get this because we're already connected externally) */
	STEAM_CALLBACK(FOnlineAsyncTaskManagerSteam, OnSteamServersConnected, SteamServersConnected_t, OnSteamServersConnectedCallback);
	/** Delegate registered with Steam to trigger when a user (client API) is disconnected from the Steam servers.  All realtime services are disabled until a connect event above */
	STEAM_CALLBACK(FOnlineAsyncTaskManagerSteam, OnSteamServersDisconnected, SteamServersDisconnected_t, OnSteamServersDisconnectedCallback);
	/** Delegate registered with Steam to trigger when a server (gameserver API) is connected to the Steam servers.  Initiated with a LogOnAnonymous() call */
	STEAM_GAMESERVER_CALLBACK(FOnlineAsyncTaskManagerSteam, OnSteamServersConnectedGS, SteamServersConnected_t, OnSteamServersConnectedGSCallback);
	/** Delegate registered with Steam to trigger when a server (gameserver API) is disconnected from Steam.  Will occur on DestroySession() */
	STEAM_GAMESERVER_CALLBACK(FOnlineAsyncTaskManagerSteam, OnSteamServersDisconnectedGS, SteamServersDisconnected_t, OnSteamServersDisconnectedGSCallback);
	/** Delegate registered with Steam to trigger when a server (gameserver API) connection fails. */
	STEAM_GAMESERVER_CALLBACK(FOnlineAsyncTaskManagerSteam, OnSteamServersConnectFailureGS, SteamServerConnectFailure_t, OnSteamServersConnectFailureGSCallback);
	/** Delegate registered with Steam to trigger when a server (gameserver API) is properly connected to Steam servers, logon isn't complete until this occurs */
	STEAM_GAMESERVER_CALLBACK(FOnlineAsyncTaskManagerSteam, OnPolicyResponseGS, GSPolicyResponse_t, OnPolicyResponseGSCallback);

	/** Delegate registered with Steam to trigger when a user accepts an invite request to a non lobby session */
	STEAM_CALLBACK(FOnlineAsyncTaskManagerSteam, OnInviteAccepted, GameRichPresenceJoinRequested_t, OnInviteAcceptedCallback);
	/** Delegate registered with Steam to trigger when a user attempts JIP (via Steam client) or accepts an invite request (via Steam client) to a lobby session */
	STEAM_CALLBACK(FOnlineAsyncTaskManagerSteam, OnLobbyInviteAccepted, GameLobbyJoinRequested_t, OnLobbyInviteAcceptedCallback);
	/** Delegate registered with Steam to trigger when a user joins a lobby (successfully or not) */
	STEAM_CALLBACK(FOnlineAsyncTaskManagerSteam, OnLobbyEnter, LobbyEnter_t, OnLobbyEnterCallback);
	/** Delegate registered with Steam to trigger when a lobby chat update occurs (usually users joining/leaving lobby) */
	STEAM_CALLBACK(FOnlineAsyncTaskManagerSteam, OnLobbyChatUpdate, LobbyChatUpdate_t, OnLobbyChatUpdateCallback);
	/** 
     * Delegate registered with Steam to trigger when a lobby update occurs
	 * As host (or any client joined to the lobby), any call to SetLobbyData will trigger this
	 * As client searching for lobbies, once per call to RequestLobbyData
     */
	STEAM_CALLBACK(FOnlineAsyncTaskManagerSteam, OnLobbyDataUpdate, LobbyDataUpdate_t, OnLobbyDataUpdateCallback);
	/** Delegate registered with Steam to trigger when the currently logged in user receives their stats from RequestStats() */
	STEAM_CALLBACK(FOnlineAsyncTaskManagerSteam, OnUserStatsReceived, UserStatsReceived_t, OnUserStatsReceivedCallback);
	/** Delegate registered with Steam to trigger when the currently logged in user has their stats stored */
	STEAM_CALLBACK(FOnlineAsyncTaskManagerSteam, OnUserStatsStored, UserStatsStored_t, OnUserStatsStoredCallback);
	/** Delegate registered with Steam to trigger when any previously requested user's stat are unloaded by the system */
	STEAM_CALLBACK(FOnlineAsyncTaskManagerSteam, OnUserStatsUnloaded, UserStatsUnloaded_t, OnUserStatsUnloadedCallback);
	/** Delegate registered with Steam to trigger when the Steam Overlay is activated */
	STEAM_CALLBACK(FOnlineAsyncTaskManagerSteam, OnExternalUITriggered, GameOverlayActivated_t, OnExternalUITriggeredCallback);
	/** Delegate registered with Steam to trigger when Steam is shutting down */
	STEAM_CALLBACK(FOnlineAsyncTaskManagerSteam, OnSteamShutdown, SteamShutdown_t, OnSteamShutdownCallback);

	//GameServerChangeRequested_t

protected:

	/** Cached reference to the main online subsystem */
	class FOnlineSubsystemSteam* SteamSubsystem;

public:

	FOnlineAsyncTaskManagerSteam(class FOnlineSubsystemSteam* InOnlineSubsystem) :
	    OnP2PSessionRequestCallback(this, &FOnlineAsyncTaskManagerSteam::OnP2PSessionRequest),
		OnP2PSessionConnectFailCallback(this, &FOnlineAsyncTaskManagerSteam::OnP2PSessionConnectFail),
		OnP2PSessionRequestGSCallback(this, &FOnlineAsyncTaskManagerSteam::OnP2PSessionRequestGS),
		OnP2PSessionConnectFailGSCallback(this, &FOnlineAsyncTaskManagerSteam::OnP2PSessionConnectFailGS),
		OnSteamServersConnectedCallback(this, &FOnlineAsyncTaskManagerSteam::OnSteamServersConnected),
		OnSteamServersDisconnectedCallback(this, &FOnlineAsyncTaskManagerSteam::OnSteamServersDisconnected),
		OnSteamServersConnectedGSCallback(this, &FOnlineAsyncTaskManagerSteam::OnSteamServersConnectedGS),
		OnSteamServersDisconnectedGSCallback(this, &FOnlineAsyncTaskManagerSteam::OnSteamServersDisconnectedGS),
		OnSteamServersConnectFailureGSCallback(this, &FOnlineAsyncTaskManagerSteam::OnSteamServersConnectFailureGS),
		OnPolicyResponseGSCallback(this, &FOnlineAsyncTaskManagerSteam::OnPolicyResponseGS),
		OnInviteAcceptedCallback(this, &FOnlineAsyncTaskManagerSteam::OnInviteAccepted),
		OnLobbyInviteAcceptedCallback(this, &FOnlineAsyncTaskManagerSteam::OnLobbyInviteAccepted),
		OnLobbyEnterCallback(this, &FOnlineAsyncTaskManagerSteam::OnLobbyEnter),
		OnLobbyChatUpdateCallback(this, &FOnlineAsyncTaskManagerSteam::OnLobbyChatUpdate),
		OnLobbyDataUpdateCallback(this, &FOnlineAsyncTaskManagerSteam::OnLobbyDataUpdate),
		OnUserStatsReceivedCallback(this, &FOnlineAsyncTaskManagerSteam::OnUserStatsReceived),
		OnUserStatsStoredCallback(this, &FOnlineAsyncTaskManagerSteam::OnUserStatsStored),
		OnUserStatsUnloadedCallback(this, &FOnlineAsyncTaskManagerSteam::OnUserStatsUnloaded),
		OnExternalUITriggeredCallback(this, &FOnlineAsyncTaskManagerSteam::OnExternalUITriggered),
		OnSteamShutdownCallback(this, &FOnlineAsyncTaskManagerSteam::OnSteamShutdown),
		SteamSubsystem(InOnlineSubsystem)
	{
	}

	~FOnlineAsyncTaskManagerSteam() 
	{
	}

	// FOnlineAsyncTaskManager
	virtual void OnlineTick() override;

	// FOnlineAsyncTaskManagerSteam
};


