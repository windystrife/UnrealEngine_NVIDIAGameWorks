// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineIdentityInterfaceGameCircle.h"
#include "OnlineSubsystemGameCircle.h"
#include "OnlineError.h"

FOnlineIdentityGameCircle::FOnlineIdentityGameCircle(FOnlineSubsystemGameCircle* InSubsystem)
	: MainSubsystem(InSubsystem)
	, bIsLoggedIn(false)
	, bLocalPlayerInfoRequested(false)
	, SignedInStateChangeListener(InSubsystem)

{
	check(MainSubsystem != nullptr);
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityGameCircle::GetUserAccount(const FUniqueNetId& UserId) const
{
	return nullptr;
}


TArray<TSharedPtr<FUserOnlineAccount> > FOnlineIdentityGameCircle::GetAllUserAccounts() const
{
	TArray<TSharedPtr<FUserOnlineAccount> > Result;

	return Result;
}


bool FOnlineIdentityGameCircle::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials) 
{
	return MainSubsystem->GetExternalUIInterface()->ShowLoginUI(LocalUserNum, false, false);
}


bool FOnlineIdentityGameCircle::Logout(int32 LocalUserNum)
{
	return false;
}


bool FOnlineIdentityGameCircle::AutoLogin(int32 LocalUserNum)
{
	return Login(LocalUserNum, FOnlineAccountCredentials());
}


ELoginStatus::Type FOnlineIdentityGameCircle::GetLoginStatus(int32 LocalUserNum) const
{
	return bIsLoggedIn ? ELoginStatus::LoggedIn : ELoginStatus::NotLoggedIn;
}


ELoginStatus::Type FOnlineIdentityGameCircle::GetLoginStatus(const FUniqueNetId& UserId) const
{
	return GetLoginStatus(0);
}


TSharedPtr<const FUniqueNetId> FOnlineIdentityGameCircle::GetUniquePlayerId(int32 LocalUserNum) const
{
	return UniqueNetId;
}


TSharedPtr<const FUniqueNetId> FOnlineIdentityGameCircle::CreateUniquePlayerId(uint8* Bytes, int32 Size)
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


TSharedPtr<const FUniqueNetId> FOnlineIdentityGameCircle::CreateUniquePlayerId(const FString& Str)
{
	return MakeShareable(new FUniqueNetIdString(Str));
}


FString FOnlineIdentityGameCircle::GetPlayerNickname(int32 LocalUserNum) const
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineIdentityGameCircle::GetPlayerNickname returning \"%s\""), *LocalPlayerInfo.Alias);
	return LocalPlayerInfo.Alias;
}

FString FOnlineIdentityGameCircle::GetPlayerNickname(const FUniqueNetId& UserId) const
{
	return GetPlayerNickname(0);
}

FString FOnlineIdentityGameCircle::GetAuthToken(int32 LocalUserNum) const
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineIdentityGameCircle::GetAuthToken not implemented"));
	FString ResultToken;
	return ResultToken;
}

void FOnlineIdentityGameCircle::RevokeAuthToken(const FUniqueNetId& UserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineIdentityGameCircle::RevokeAuthToken not implemented"));
	TSharedRef<const FUniqueNetId> UserIdRef(UserId.AsShared());
	MainSubsystem->ExecuteNextTick([UserIdRef, Delegate]()
	{
		Delegate.ExecuteIfBound(*UserIdRef, FOnlineError(FString(TEXT("RevokeAuthToken not implemented"))));
	});
}

void FOnlineIdentityGameCircle::Tick(float DeltaTime)
{

}

void FOnlineIdentityGameCircle::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate)
{
	Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::NoFailures);
}

FPlatformUserId FOnlineIdentityGameCircle::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& NetId) const
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

FString FOnlineIdentityGameCircle::GetAuthType() const
{
	return TEXT("");
}

void FOnlineIdentityGameCircle::OnGetLocalPlayerPlayerCallback(AmazonGames::ErrorCode InErrorCode, const AmazonGames::PlayerInfo *const InPlayerInfo)
{
	switch (InErrorCode)
	{
	case AmazonGames::ErrorCode::NO_ERROR:
		{
			LocalPlayerInfo.PlayerId = InPlayerInfo->playerId;
			LocalPlayerInfo.Alias = InPlayerInfo->alias;
			LocalPlayerInfo.AvatarURL = InPlayerInfo->avatarUrl;

			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Received player info for local player with playerId \"%s\""), *LocalPlayerInfo.PlayerId);
			UniqueNetId = MakeShareable(new FUniqueNetIdString(LocalPlayerInfo.PlayerId));

			AmazonGames::PlayerClientInterface::setSignedInStateChangedListener(&SignedInStateChangeListener);
			bIsLoggedIn = AmazonGames::PlayerClientInterface::isSignedIn();
		}
		break;
	case AmazonGames::ErrorCode::SERVICE_NOT_READY:
		RequestLocalPlayerInfo();
		break;
	default:
		UE_LOG(LogOnline, Error, TEXT("GetLocalPlayer Callback recieved error code - %d"), InErrorCode);
		break;
	}
}

void FOnlineIdentityGameCircle::RequestLocalPlayerInfo()
{
	UE_LOG(LogOnline, Display, TEXT("Requesting local player info from Amazon"));
	AmazonGames::PlayerClientInterface::getLocalPlayer(new FOnlineGetLocalPlayerCallback(MainSubsystem));
}
