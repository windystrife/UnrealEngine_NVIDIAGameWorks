// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemTwitchPrivate.h"
#include "OnlineAccountTwitch.h"
#include "TwitchTokenValidationResponse.h"

TSharedRef<const FUniqueNetId> FUserOnlineAccountTwitch::GetUserId() const
{
	return UserId;
}

FString FUserOnlineAccountTwitch::GetRealName() const
{
	FString Result;
	GetAccountData(TEXT("name"), Result);
	return Result;
}

FString FUserOnlineAccountTwitch::GetDisplayName(const FString& Platform) const
{
	FString Result;
	GetAccountData(TEXT("displayName"), Result);
	return Result;
}

bool FUserOnlineAccountTwitch::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	return GetAccountData(AttrName, OutAttrValue);
}

bool FUserOnlineAccountTwitch::SetUserAttribute(const FString& AttrName, const FString& AttrValue)
{
	return SetAccountData(AttrName, AttrValue);
}

FString FUserOnlineAccountTwitch::GetAccessToken() const
{
	return AuthTicket;
}

bool FUserOnlineAccountTwitch::GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	return false;
}

bool FUserOnlineAccountTwitch::Parse(const FString& InAuthTicket, FTwitchTokenValidationResponse&& ValidationResponse)
{
	bool bResult = false;

	if (!InAuthTicket.IsEmpty())
	{
		if (ValidationResponse.bTokenIsValid)
		{
			UserId = MakeShared<FUniqueNetIdString>(ValidationResponse.UserId);
			if (!ValidationResponse.UserName.IsEmpty())
			{
				SetAccountData(TEXT("displayName"), ValidationResponse.UserName);
			}
			AuthTicket = InAuthTicket;
			ScopePermissions = MoveTemp(ValidationResponse.Authorization.Scopes);
			bResult = true;
		}
		else
		{
			UE_LOG_ONLINE(Log, TEXT("FUserOnlineAccountTwitch::Parse: Twitch token is not valid"));
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("FUserOnlineAccountTwitch::Parse: Empty Auth Ticket"));
	}
	return bResult;
}
