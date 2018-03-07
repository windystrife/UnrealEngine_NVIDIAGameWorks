// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineIdentityInterface.h"
#include "OnlineSubsystemGooglePlayPackage.h"

THIRD_PARTY_INCLUDES_START
#include "gpg/status.h"
#include "gpg/player.h"
THIRD_PARTY_INCLUDES_END

class FOnlineIdentityGooglePlay :
	public IOnlineIdentity
{
private:
	
	bool bPrevLoggedIn;
	bool bLoggedIn;
	FString PlayerAlias;
	FString AuthToken;
	int32 CurrentLoginUserNum;
	class FOnlineSubsystemGooglePlay* MainSubsystem;

	bool bLoggingInUser;
	bool bRegisteringUser;

	/** UID for this identity */
	TSharedPtr< const FUniqueNetIdString > UniqueNetId;

	struct FPendingConnection
	{
		FOnlineIdentityGooglePlay * ConnectionInterface;
		bool IsConnectionPending;
	};

	/** Instance of the connection context data */
	static FPendingConnection PendingConnectRequest;

PACKAGE_SCOPE:

	/**
	 * Default Constructor
	 */
	FOnlineIdentityGooglePlay(FOnlineSubsystemGooglePlay* InSubsystem);

	/** Allow individual interfaces to access the currently signed-in user's id */
	TSharedPtr<const FUniqueNetIdString> GetCurrentUserId() const { return UniqueNetId; }

	void SetCurrentUserId(TSharedPtr<const FUniqueNetIdString> InUniqueNetId) { UniqueNetId = InUniqueNetId; }

	/** Called from ExternalUIInterface to set UniqueId and PlayerAlias after authentication */
	void SetPlayerDataFromFetchSelfResponse(const gpg::Player& PlayerData);

	/** Called from ExternalUIInterface to set AuthToken after authentication */
	void SetAuthTokenFromGoogleConnectResponse(const FString& NewAuthToken);

public:

	//~ Begin IOnlineIdentity Interface
	virtual TSharedPtr<FUserOnlineAccount> GetUserAccount(const FUniqueNetId& UserId) const override;
	virtual TArray<TSharedPtr<FUserOnlineAccount> > GetAllUserAccounts() const override;
	virtual bool Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials) override;
	virtual bool Logout(int32 LocalUserNum) override;
	virtual bool AutoLogin(int32 LocalUserNum) override;
	virtual TSharedPtr<const FUniqueNetId> GetUniquePlayerId(int32 LocalUserNum) const override;
	virtual TSharedPtr<const FUniqueNetId> CreateUniquePlayerId(uint8* Bytes, int32 Size) override;
	virtual TSharedPtr<const FUniqueNetId> CreateUniquePlayerId(const FString& Str) override;
	virtual ELoginStatus::Type GetLoginStatus(int32 LocalUserNum) const override;
	virtual ELoginStatus::Type GetLoginStatus(const FUniqueNetId& UserId) const override;
	virtual FString GetPlayerNickname(int32 LocalUserNum) const override;
	virtual FString GetPlayerNickname(const FUniqueNetId& UserId) const override;
	virtual FString GetAuthToken(int32 LocalUserNum) const override;
	virtual void RevokeAuthToken(const FUniqueNetId& UserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate) override;
	virtual void GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate) override;
	virtual FPlatformUserId GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& NetId) const override;
	virtual FString GetAuthType() const override;
	//~ End IOnlineIdentity interface

public:

	//~ Begin IOnlineIdentity Interface
	void Tick(float DeltaTime);
	//~ End IOnlineIdentity Interface
	
	//platform specific
	void OnLogin(const bool InLoggedIn,  const FString& InPlayerId, const FString& InPlayerAlias);
	void OnLogout(const bool InLoggedIn);

	void OnLoginCompleted(const int playerID, gpg::AuthStatus errorCode);

};


typedef TSharedPtr<FOnlineIdentityGooglePlay, ESPMode::ThreadSafe> FOnlineIdentityGooglePlayPtr;

