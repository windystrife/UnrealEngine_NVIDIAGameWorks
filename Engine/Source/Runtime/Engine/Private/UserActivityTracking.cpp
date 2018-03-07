// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UserActivityTracking.h"
#include "Misc/CoreDelegates.h"

FUserActivityTracking::FOnActivityChanged FUserActivityTracking::OnActivityChanged;

FUserActivity FUserActivityTracking::UserActivity;
EUserActivityContext FUserActivityTracking::ContextFilter = EUserActivityContext::Game;		// default to expecting (and firing delegates for) Game activities

void FUserActivityTracking::SetContextFilter(EUserActivityContext InContext)
{
	ContextFilter = InContext;
}

void FUserActivityTracking::SetActivity(const FUserActivity& InUserActivity)
{ 
	if (InUserActivity.Context == ContextFilter)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FUserActivityTracking_SetActivity);
		UserActivity = InUserActivity;
		OnActivityChanged.Broadcast(UserActivity);
		FCoreDelegates::UserActivityStringChanged.Broadcast(UserActivity.ActionName);
	}
}
