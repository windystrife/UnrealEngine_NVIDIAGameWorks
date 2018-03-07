// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineIdentityInterfaceGooglePlay.h"
#include "OnlineSubsystemGooglePlay.h"

FOnlineIdentityGooglePlay::FPendingConnection FOnlineIdentityGooglePlay::PendingConnectRequest;

FOnlineIdentityGooglePlay::FOnlineIdentityGooglePlay(FOnlineSubsystemGooglePlay* InSubsystem)
	: bPrevLoggedIn(false)
	, bLoggedIn(false)
	, PlayerAlias("NONE")
	, AuthToken("NONE")
	, MainSubsystem(InSubsystem)
	, bRegisteringUser(false)
	, bLoggingInUser(false)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineIdentityGooglePlay::FOnlineIdentityGooglePlay()"));
	check(MainSubsystem != nullptr);
	PendingConnectRequest.ConnectionInterface = this;
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityGooglePlay::GetUserAccount(const FUniqueNetId& UserId) const
{
	return nullptr;
}


TArray<TSharedPtr<FUserOnlineAccount> > FOnlineIdentityGooglePlay::GetAllUserAccounts() const
{
	TArray<TSharedPtr<FUserOnlineAccount> > Result;

	return Result;
}

bool FOnlineIdentityGooglePlay::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials) 
{
	bool bStartedLogin = false;
	if (bLoggedIn)
	{
		// already logged in so just report all is ok!
		// Now logged in
		bStartedLogin = true;

		static const int32 MAX_TEXT_LINE_LEN = 32;
		TCHAR Line[MAX_TEXT_LINE_LEN + 1] = { 0 };
		int32 Len = FCString::Snprintf(Line, MAX_TEXT_LINE_LEN, TEXT("%d"), LocalUserNum);

		const FString PlayerId(Line);
		UniqueNetId = MakeShareable(new FUniqueNetIdString(PlayerId));
		TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UniqueNetId, TEXT(""));
	}
	else if (!PendingConnectRequest.IsConnectionPending)
	{
		// Kick the login sequence...
		bStartedLogin = true;
		PendingConnectRequest.IsConnectionPending = true;
	}
	else
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdString(TEXT("")), FString("Already trying to login"));
	}
	
	return bStartedLogin;
}


bool FOnlineIdentityGooglePlay::Logout(int32 LocalUserNum)
{
	MainSubsystem->StartLogoutTask(LocalUserNum);
	return true;
}


bool FOnlineIdentityGooglePlay::AutoLogin(int32 LocalUserNum)
{
	return Login(LocalUserNum, FOnlineAccountCredentials());
}


ELoginStatus::Type FOnlineIdentityGooglePlay::GetLoginStatus(int32 LocalUserNum) const
{
	if (LocalUserNum < MAX_LOCAL_PLAYERS && MainSubsystem->GetGameServices() != nullptr && MainSubsystem->GetGameServices()->IsAuthorized())
	{
		return ELoginStatus::LoggedIn;
	}

	return ELoginStatus::NotLoggedIn;
}


ELoginStatus::Type FOnlineIdentityGooglePlay::GetLoginStatus(const FUniqueNetId& UserId) const
{
	if (MainSubsystem->GetGameServices() != nullptr && MainSubsystem->GetGameServices()->IsAuthorized())
	{
		return ELoginStatus::LoggedIn;
	}

	return ELoginStatus::NotLoggedIn;
}


TSharedPtr<const FUniqueNetId> FOnlineIdentityGooglePlay::GetUniquePlayerId(int32 LocalUserNum) const
{
	if (UniqueNetId.IsValid())
	{
		return UniqueNetId;
	}

	TSharedPtr<const FUniqueNetId> NewID = MakeShareable(new FUniqueNetIdString(""));
	return NewID;
}


TSharedPtr<const FUniqueNetId> FOnlineIdentityGooglePlay::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
	if( Bytes && Size == sizeof(uint64) )
	{
		int32 StrLen = FCString::Strlen((TCHAR*)Bytes);
		if (StrLen > 0)
		{
			FString StrId((TCHAR*)Bytes);
			return MakeShareable(new FUniqueNetIdString(StrId));
		}
	}
	return NULL;
}


TSharedPtr<const FUniqueNetId> FOnlineIdentityGooglePlay::CreateUniquePlayerId(const FString& Str)
{
	return MakeShareable(new FUniqueNetIdString(Str));
}


FString FOnlineIdentityGooglePlay::GetPlayerNickname(int32 LocalUserNum) const
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineIdentityGooglePlay::GetPlayerNickname"));
	return PlayerAlias;
}

FString FOnlineIdentityGooglePlay::GetPlayerNickname(const FUniqueNetId& UserId) const
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineIdentityGooglePlay::GetPlayerNickname"));
	return PlayerAlias;
}

FString FOnlineIdentityGooglePlay::GetAuthToken(int32 LocalUserNum) const
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineIdentityGooglePlay::GetAuthToken"));
	return AuthToken;
}

void FOnlineIdentityGooglePlay::RevokeAuthToken(const FUniqueNetId& UserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineIdentityGooglePlay::RevokeAuthToken not implemented"));
	TSharedRef<const FUniqueNetId> UserIdRef(UserId.AsShared());
	MainSubsystem->ExecuteNextTick([UserIdRef, Delegate]()
	{
		Delegate.ExecuteIfBound(*UserIdRef, FOnlineError(FString(TEXT("RevokeAuthToken not implemented"))));
	});
}

void FOnlineIdentityGooglePlay::Tick(float DeltaTime)
{
}

void FOnlineIdentityGooglePlay::OnLoginCompleted(const int playerID, const gpg::AuthStatus errorCode)
{
	static const int32 MAX_TEXT_LINE_LEN = 32;
	TCHAR Line[MAX_TEXT_LINE_LEN + 1] = { 0 };
	int32 Len = FCString::Snprintf(Line, MAX_TEXT_LINE_LEN, TEXT("%d"), playerID);

	UniqueNetId = MakeShareable(new FUniqueNetIdString(Line));
	bLoggedIn = errorCode == gpg::AuthStatus::VALID;
	TriggerOnLoginCompleteDelegates(playerID, bLoggedIn, *UniqueNetId, TEXT(""));

	PendingConnectRequest.IsConnectionPending = false;
}

void FOnlineIdentityGooglePlay::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate)
{
	Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::NoFailures);
}

FPlatformUserId FOnlineIdentityGooglePlay::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& NetId) const
{
	for (int i = 0; i < MAX_LOCAL_PLAYERS; ++i)
	{
		auto CurrentUniqueId = GetUniquePlayerId(i);
		if (CurrentUniqueId.IsValid() && (*CurrentUniqueId == NetId))
		{
			return i;
		}
	}

	return PLATFORMUSERID_NONE;
}

FString FOnlineIdentityGooglePlay::GetAuthType() const
{
	return TEXT("");
}

void FOnlineIdentityGooglePlay::SetPlayerDataFromFetchSelfResponse(const gpg::Player& PlayerData)
{
	FString PlayerId(PlayerData.Id().c_str());
	UniqueNetId = MakeShareable(new FUniqueNetIdString(PlayerId));
	PlayerAlias = PlayerData.Name().c_str();
}

void FOnlineIdentityGooglePlay::SetAuthTokenFromGoogleConnectResponse(const FString& NewAuthToken)
{
	AuthToken = NewAuthToken;
}