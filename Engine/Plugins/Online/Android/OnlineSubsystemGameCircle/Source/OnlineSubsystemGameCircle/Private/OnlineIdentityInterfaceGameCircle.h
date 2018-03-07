// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineIdentityInterface.h"
#include "OnlineSubsystemGameCirclePackage.h"

#include "OnlineAGSPlayerClientCallbacks.h"

class FOnlineIdentityGameCircle :
	public IOnlineIdentity
{
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
	void Tick(float DeltaTime);
	//~ End IOnlineIdentity Interface

PACKAGE_SCOPE:

	/**
	 * Default Constructor
	 */
	FOnlineIdentityGameCircle(FOnlineSubsystemGameCircle* InSubsystem);

	/** Allow individual interfaces to access the currently signed-in user's id */
	TSharedPtr<const FUniqueNetIdString> GetCurrentUserId() const { return UniqueNetId; }

	void SetCurrentUserId(TSharedPtr<const FUniqueNetIdString> InUniqueNetId) { UniqueNetId = InUniqueNetId; }

	void SetSignedInState(bool bNewState) { bIsLoggedIn = bNewState; }

	void RequestLocalPlayerInfo();

	void OnGetLocalPlayerPlayerCallback(AmazonGames::ErrorCode InErrorCode, const AmazonGames::PlayerInfo *const InPlayerInfo);

private:

	class FOnlineSubsystemGameCircle* MainSubsystem;

	struct TAmazonPlayerInfoCache
	{
		FString PlayerId;
		FString Alias;
		FString AvatarURL;

	} LocalPlayerInfo;

	/** UID for this identity */
	TSharedPtr<const FUniqueNetIdString> UniqueNetId;

	FOnlineSignedInStateChangedListener	SignedInStateChangeListener;

	bool bIsLoggedIn;
	bool bLocalPlayerInfoRequested;
};


typedef TSharedPtr<FOnlineIdentityGameCircle, ESPMode::ThreadSafe> FOnlineIdentityGameCirclePtr;

