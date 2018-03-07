// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemFacebook.h"
#include "OnlineSubsystemFacebookPrivate.h"
#include "OnlineIdentityFacebook.h"
#include "OnlineFriendsFacebook.h"
#include "OnlineSharingFacebook.h"

FOnlineSubsystemFacebook::FOnlineSubsystemFacebook()
{
}

FOnlineSubsystemFacebook::FOnlineSubsystemFacebook(FName InInstanceName)
	: FOnlineSubsystemFacebookCommon(InInstanceName)
{
}

FOnlineSubsystemFacebook::~FOnlineSubsystemFacebook()
{
}

bool FOnlineSubsystemFacebook::Init()
{
#if WITH_FACEBOOK
	FacebookIdentity = MakeShareable(new FOnlineIdentityFacebook(this));
	FacebookFriends = MakeShareable(new FOnlineFriendsFacebook(this));
	FacebookSharing = MakeShareable(new FOnlineSharingFacebook(this));
	return true;
#else
	return false;
#endif
}

bool FOnlineSubsystemFacebook::Shutdown()
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineSubsystemFacebook::Shutdown()"));
	return FOnlineSubsystemFacebookCommon::Shutdown();
}

bool FOnlineSubsystemFacebook::IsEnabled() const
{
	return FOnlineSubsystemFacebookCommon::IsEnabled();
}




