// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncTaskGooglePlayQueryAchievements.h"
#include "OnlineSubsystemGooglePlay.h"
#include "OnlineAchievementsInterfaceGooglePlay.h"

THIRD_PARTY_INCLUDES_START
#include "gpg/achievement_manager.h"
THIRD_PARTY_INCLUDES_END

FOnlineAsyncTaskGooglePlayQueryAchievements::FOnlineAsyncTaskGooglePlayQueryAchievements(
	FOnlineSubsystemGooglePlay* InSubsystem,
	const FUniqueNetIdString& InUserId,
	const FOnQueryAchievementsCompleteDelegate& InDelegate)
	: FOnlineAsyncTaskBasic(InSubsystem)
	, UserId(InUserId)
	, Delegate(InDelegate)
{
}

void FOnlineAsyncTaskGooglePlayQueryAchievements::Finalize()
{
	if (bWasSuccessful)
	{
		Subsystem->GetAchievementsGooglePlay()->UpdateCache(Response);
	}
	else
	{
		Subsystem->GetAchievementsGooglePlay()->ClearCache();
	}
}

void FOnlineAsyncTaskGooglePlayQueryAchievements::TriggerDelegates()
{
	Delegate.ExecuteIfBound(UserId, bWasSuccessful);
}

void FOnlineAsyncTaskGooglePlayQueryAchievements::Tick()
{
	// We're already running on the online thread. Using the blocking version of the API function
	// won't block the main thread and simplifies things a bit.
	// Try a 10 second timeout.
	Response = Subsystem->GetGameServices()->Achievements().FetchAllBlocking(gpg::DataSource::CACHE_OR_NETWORK, gpg::Timeout(10000));
	
	bWasSuccessful = Response.status == gpg::ResponseStatus::VALID || Response.status == gpg::ResponseStatus::VALID_BUT_STALE;
	bIsComplete = true;
}
