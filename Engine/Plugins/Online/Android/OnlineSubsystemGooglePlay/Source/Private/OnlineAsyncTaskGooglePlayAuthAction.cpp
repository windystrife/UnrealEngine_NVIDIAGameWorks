// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncTaskGooglePlayAuthAction.h"

THIRD_PARTY_INCLUDES_START
#include "gpg/builder.h"
#include "gpg/debug.h"
THIRD_PARTY_INCLUDES_END

FOnlineAsyncTaskGooglePlayAuthAction::FOnlineAsyncTaskGooglePlayAuthAction(
	FOnlineSubsystemGooglePlay* InSubsystem)
	: FOnlineAsyncTaskBasic(InSubsystem)
	, bInit(false)
{
	check(InSubsystem);
}

void FOnlineAsyncTaskGooglePlayAuthAction::Tick()
{
	if ( !bInit)
	{
		Start_OnTaskThread();
		bInit = true;
	}
}
