// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineFriendsFacebook.h"
#include "OnlineSubsystemFacebookPrivate.h"

#if WITH_FACEBOOK

FOnlineFriendsFacebook::FOnlineFriendsFacebook(FOnlineSubsystemFacebook* InSubsystem) 
	: FOnlineFriendsFacebookCommon(InSubsystem)
{
}

FOnlineFriendsFacebook::FOnlineFriendsFacebook()
	: FOnlineFriendsFacebookCommon(nullptr)
{
}

FOnlineFriendsFacebook::~FOnlineFriendsFacebook()
{
}

#endif // WITH_FACEBOOK

