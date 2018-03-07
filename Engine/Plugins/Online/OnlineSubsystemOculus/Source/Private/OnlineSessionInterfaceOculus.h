// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemOculus.h"
#include "OnlineSessionInterface.h"
#include "OnlineSubsystemOculusTypes.h"
#include "OnlineSubsystemOculusPackage.h"

#define SETTING_OCULUS_POOL FName(TEXT("OCULUSPOOL"))
#define SETTING_OCULUS_BUILD_UNIQUE_ID FName(TEXT("OCULUSSESSIONBUILDUNIQUEID"))

#define SEARCH_OCULUS_MODERATED_ROOMS_ONLY FName(TEXT("OCULUSMODERATEDROOMSONLY"))

/**
 * Interface definition for the online services session services
 * Session services are defined as anything related managing a session
 * and its state within a platform service
 */
class FOnlineSessionOculus : public IOnlineSession
{
private:

	/** Reference to the main Oculus subsystem */
	FOnlineSubsystemOculus& OculusSubsystem;

	/**
	 * Current session settings
	 * This really should be unique ptrs, but TMap has a limitation that makes TUniquePtr impossible to store as values here
	 */
	TMap<FName, TSharedPtr<FNamedOnlineSession>> Sessions;

	/**
	 * The matchmaking search in progress
	 */
	TSharedPtr<FOnlineSessionSearch> InProgressMatchmakingSearch;
	/**
	 * The SessionName passed into StartMatchmaking
	 */
	FName InProgressMatchmakingSearchName;

	ovrID GetOvrIDFromSession(const FNamedOnlineSession& Session) const;

	TArray<TSharedRef<const FOnlineSessionSearchResult>> PendingInviteAcceptedSessions;

	int32 GetRoomBuildUniqueId(const ovrRoomHandle Room);

PACKAGE_SCOPE:

	FDelegateHandle OnRoomNotificationUpdateHandle;
	void OnRoomNotificationUpdate(ovrMessageHandle Message, bool bIsError);

	FDelegateHandle OnRoomNotificationInviteAcceptedHandle;
	void OnRoomInviteAccepted(ovrMessageHandle Message, bool bIsError);

	FDelegateHandle OnMatchmakingNotificationMatchFoundHandle;
	void OnMatchmakingNotificationMatchFound(ovrMessageHandle Message, bool bIsError);

	TSharedRef<FOnlineSession> CreateSessionFromRoom(ovrRoomHandle Room) const;

	void UpdateSessionFromRoom(FNamedOnlineSession& Session, ovrRoomHandle Room) const;
	void UpdateSessionSettingsFromDataStore(FOnlineSessionSettings& SessionSettings, ovrDataStoreHandle DataStore) const;

	void TickPendingInvites(float DeltaTime);

	bool CreateRoomSession(FNamedOnlineSession& Session, ovrRoomJoinPolicy JoinPolicy);
	bool CreateMatchmakingSession(FNamedOnlineSession& Session, ovrRoomJoinPolicy JoinPolicy);
	void OnCreateRoomComplete(ovrMessageHandle Message, bool bIsError, FName SessionName);

	bool FindModeratedRoomSessions(const TSharedRef<FOnlineSessionSearch>& SearchSettings);
	bool FindMatchmakingSessions(const FString Pool, const TSharedRef<FOnlineSessionSearch>& SearchSettings);

	bool UpdateMatchmakingRoom(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings);
	bool UpdateRoomDataStore(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings);

public:

	FOnlineSessionOculus(FOnlineSubsystemOculus& InSubsystem);
	virtual ~FOnlineSessionOculus();

	// IOnlineSession
	virtual bool CreateSession(int32 HostingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;
	virtual bool CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;
	virtual bool StartSession(FName SessionName) override;
	virtual bool UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData = true) override;
	virtual bool EndSession(FName SessionName) override;
	virtual bool DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate = FOnDestroySessionCompleteDelegate()) override;
	virtual bool IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId) override;
	virtual bool StartMatchmaking(const TArray< TSharedRef<const FUniqueNetId> >& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName) override;
	virtual bool CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName) override;
	virtual bool FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegate) override;
	virtual bool CancelFindSessions() override;
	virtual bool PingSearchResults(const FOnlineSessionSearchResult& SearchResult) override;
	virtual bool JoinSession(int32 PlayerNum, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;
	virtual bool JoinSession(const FUniqueNetId& PlayerId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;
	virtual bool FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend) override;
	virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend) override;
	virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<TSharedRef<const FUniqueNetId>>& FriendList) override;
	virtual bool SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend) override;
	virtual bool SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend) override;
	virtual bool SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends) override;
	virtual bool SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends) override;
	virtual bool GetResolvedConnectString(FName SessionName, FString& ConnectInfo, FName PortType = NAME_GamePort) override;
	virtual bool GetResolvedConnectString(const class FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo) override;
	virtual FOnlineSessionSettings* GetSessionSettings(FName SessionName) override;
	virtual bool RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited) override;
	virtual bool RegisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players, bool bWasInvited = false) override;
	virtual bool UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId) override;
	virtual bool UnregisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players) override;
	virtual void RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate) override;
	virtual void UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate) override;
	virtual int32 GetNumSessions() override;
	virtual void DumpSessionState() override;

	FNamedOnlineSession* GetNamedSession(FName SessionName) override;
	virtual void RemoveNamedSession(FName SessionName) override;
	virtual EOnlineSessionState::Type GetSessionState(FName SessionName) const override;
	virtual bool HasPresenceSession() override;
	class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings) override;
	class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSession& Session) override;

};

typedef TSharedPtr<FOnlineSessionOculus, ESPMode::ThreadSafe> FOnlineSessionOculusPtr;
