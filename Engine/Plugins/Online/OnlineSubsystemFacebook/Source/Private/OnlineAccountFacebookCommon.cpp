// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineAccountFacebookCommon.h"
#include "OnlineSubsystemFacebookPrivate.h"

bool FUserOnlineAccountFacebookCommon::Parse(const FString& InAuthTicket, const FString& JsonStr)
{
	bool bResult = false;
	if (!InAuthTicket.IsEmpty())
	{
		if (!JsonStr.IsEmpty())
		{
			TSharedPtr<FJsonObject> JsonUser;
			TSharedRef< TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(JsonStr);

			if (FJsonSerializer::Deserialize(JsonReader, JsonUser) &&
				JsonUser.IsValid())
			{
				if (FromJson(JsonUser))
				{
					if (!UserId.IsEmpty())
					{
						UserIdPtr = MakeShared<FUniqueNetIdString>(UserId);

						AddUserAttributes(JsonUser);

						// update the access token
						AuthTicket = InAuthTicket;

						bResult = true;
					}
					else
					{
						UE_LOG_ONLINE(Warning, TEXT("FUserOnlineAccountFacebookCommon: Missing user id. payload=%s"), *JsonStr);
					}
				}
				else
				{
					UE_LOG_ONLINE(Warning, TEXT("FUserOnlineAccountFacebookCommon: Invalid response payload=%s"), *JsonStr);
				}
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("FUserOnlineAccountFacebookCommon: Can't deserialize payload=%s"), *JsonStr);
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("FUserOnlineAccountFacebookCommon: Empty Json string"));
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("FUserOnlineAccountFacebookCommon: Empty auth ticket string"));
	}

	return bResult;
}

TSharedRef<const FUniqueNetId> FUserOnlineAccountFacebookCommon::GetUserId() const
{
	return UserIdPtr;
}

FString FUserOnlineAccountFacebookCommon::GetRealName() const
{
	return RealName;
}

FString FUserOnlineAccountFacebookCommon::GetDisplayName(const FString& Platform) const
{
	return RealName;
}

bool FUserOnlineAccountFacebookCommon::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	return GetAccountData(AttrName, OutAttrValue);
}

bool FUserOnlineAccountFacebookCommon::SetUserAttribute(const FString& AttrName, const FString& AttrValue)
{
	return SetAccountData(AttrName, AttrValue);
}

FString FUserOnlineAccountFacebookCommon::GetAccessToken() const
{
	return AuthTicket;
}

bool FUserOnlineAccountFacebookCommon::GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	return false;
}

void FUserOnlineAccountFacebookCommon::AddUserAttributes(const TSharedPtr<FJsonObject>& JsonUser)
{
	for (auto It = JsonUser->Values.CreateConstIterator(); It; ++It)
	{
		if (It.Value().IsValid())
		{
			if (It.Value()->Type == EJson::String)
			{
				AccountData.Add(It.Key(), It.Value()->AsString());
			}
			else if (It.Value()->Type == EJson::Boolean)
			{
				AccountData.Add(It.Key(), It.Value()->AsBool() ? TEXT("true") : TEXT("false"));
			}
			else if (It.Value()->Type == EJson::Number)
			{
				AccountData.Add(It.Key(), FString::Printf(TEXT("%f"), (double)It.Value()->AsNumber()));
			}
		}
	}
}

