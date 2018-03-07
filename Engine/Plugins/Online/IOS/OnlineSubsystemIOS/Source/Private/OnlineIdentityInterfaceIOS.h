// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once


#include "OnlineSubsystemIOSTypes.h"
#include "OnlineIdentityInterface.h"
#include "OnlineSubsystemIOSPackage.h"


class FOnlineIdentityIOS :
	public IOnlineIdentity
{
private:
	/** UID for this identity */
	TSharedPtr< FUniqueNetIdString > UniqueNetId;
	FOnlineSubsystemIOS* Subsystem;

	FOnlineIdentityIOS();

PACKAGE_SCOPE:

	/**
	 * Default Constructor
	 */
	FOnlineIdentityIOS(FOnlineSubsystemIOS* InSubsystem);

	TSharedPtr<FUniqueNetIdString> GetLocalPlayerUniqueId() const;

	void SetLocalPlayerUniqueId(const TSharedPtr<FUniqueNetIdString>& UniqueId);

public:

	//~ Begin IOnlineIdentity Interface
	virtual bool Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials) override;
	virtual bool Logout(int32 LocalUserNum) override;
	virtual bool AutoLogin(int32 LocalUserNum) override;
	virtual TSharedPtr<FUserOnlineAccount> GetUserAccount(const FUniqueNetId& UserId) const override;
	virtual TArray<TSharedPtr<FUserOnlineAccount> > GetAllUserAccounts() const override;
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
	virtual FPlatformUserId GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const override;
	virtual FString GetAuthType() const override;
	//~ End IOnlineIdentity Interface


public:

	/**
	 * Destructor
	 */
	virtual ~FOnlineIdentityIOS() {};
	

	/**
	 * Get a reference to the GKLocalPlayer
	 *
	 * @return - The game center local player
	 */
	GKLocalPlayer* GetLocalGameCenterUser() const
	{
		return [GKLocalPlayer localPlayer];
	}
};


typedef TSharedPtr<FOnlineIdentityIOS, ESPMode::ThreadSafe> FOnlineIdentityIOSPtr;
