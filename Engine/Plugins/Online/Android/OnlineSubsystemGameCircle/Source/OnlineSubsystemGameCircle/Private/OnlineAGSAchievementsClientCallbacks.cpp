// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineAGSAchievementsClientCallbacks.h"
#include "OnlineSubsystemGameCircle.h"
#include "Async/TaskGraphInterfaces.h"

FOnlineGetAchievementsCallback::FOnlineGetAchievementsCallback(FOnlineSubsystemGameCircle *const InSubsystem, 
															   const FUniqueNetIdString& InUserID, 
															   const FOnQueryAchievementsCompleteDelegate& InDelegate)
	: GameCircleSubsystem(InSubsystem)
	, UserID(InUserID)
	, Delegate(InDelegate)
{
	check(GameCircleSubsystem && GameCircleSubsystem->GetCallbackManager());
	GameCircleSubsystem->GetCallbackManager()->AddActiveCallback(this);
}

void FOnlineGetAchievementsCallback::onGetAchievementsCb(
	AmazonGames::ErrorCode errorCode, 
	const AmazonGames::AchievementsData* responseStruct, 
	int developerTag)
{
	if(AmazonGames::ErrorCode::NO_ERROR == errorCode &&
		GameCircleSubsystem->GetAchievementsGameCircle().Get())
	{
		GameCircleSubsystem->GetAchievementsGameCircle()->SaveGetAchievementsCallbackResponse(responseStruct);
	}

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.onGetAchievementsCb"), STAT_FSimpleDelegateGraphTask_onGetAchievementsCb, STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=](){

			Delegate.ExecuteIfBound(UserID, AmazonGames::ErrorCode::NO_ERROR == errorCode);

			GameCircleSubsystem->GetCallbackManager()->CallbackCompleted(this);
		}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_onGetAchievementsCb), 
		nullptr, 
		ENamedThreads::GameThread
	);
}


FOnlineUpdateProgressCallback::FOnlineUpdateProgressCallback(FOnlineSubsystemGameCircle *const InSubsystem)
	: GameCircleSubsystem(InSubsystem)
{
	check(GameCircleSubsystem && GameCircleSubsystem->GetCallbackManager());
	GameCircleSubsystem->GetCallbackManager()->AddActiveCallback(this);
}

void FOnlineUpdateProgressCallback::onUpdateProgressCb(
	AmazonGames::ErrorCode errorCode, 
	const AmazonGames::UpdateProgressResponse* responseStruct, 
	int developerTag)
{
	if(AmazonGames::ErrorCode::NO_ERROR != errorCode)
	{
		UE_LOG(LogOnline, Error, TEXT("FOnlineUpdateProgressCallback Returned ErrorCode %d"), errorCode);
	}

	GameCircleSubsystem->GetCallbackManager()->CallbackCompleted(this);
}
