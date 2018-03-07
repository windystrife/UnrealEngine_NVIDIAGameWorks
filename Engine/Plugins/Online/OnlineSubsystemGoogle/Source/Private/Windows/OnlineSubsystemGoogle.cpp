// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemGoogle.h"
#include "OnlineSubsystemGooglePrivate.h"
#include "OnlineIdentityGoogle.h"
#include "OnlineExternalUIInterfaceGoogle.h"

FOnlineSubsystemGoogle::FOnlineSubsystemGoogle()
{
}

FOnlineSubsystemGoogle::FOnlineSubsystemGoogle(FName InInstanceName)
	: FOnlineSubsystemGoogleCommon(InInstanceName)
{
}

FOnlineSubsystemGoogle::~FOnlineSubsystemGoogle()
{
}

bool FOnlineSubsystemGoogle::Init()
{
	if (FOnlineSubsystemGoogleCommon::Init())
	{
		GoogleIdentity = MakeShareable(new FOnlineIdentityGoogle(this));
		GoogleExternalUI = MakeShareable(new FOnlineExternalUIGoogle(this));
		return true;
	}

	return false;
}

bool FOnlineSubsystemGoogle::Shutdown()
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineSubsystemGoogle::Shutdown()"));
	return FOnlineSubsystemGoogleCommon::Shutdown();
}

bool FOnlineSubsystemGoogle::IsEnabled() const
{
	return FOnlineSubsystemGoogleCommon::IsEnabled();
}
