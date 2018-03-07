// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Actions/TestPawnAction_Log.h"
#include "TestLogger.h"
#include "Engine/World.h"

UTestPawnAction_Log::UTestPawnAction_Log(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
	, Logger(nullptr)
{

}

UTestPawnAction_Log* UTestPawnAction_Log::CreateAction(UWorld& World, FTestLogger<int32>& InLogger)
{
	UTestPawnAction_Log* Action = UPawnAction::CreateActionInstance<UTestPawnAction_Log>(World);
	if (Action)
	{
		Action->Logger = &InLogger;
	}

	return Action;
}

bool UTestPawnAction_Log::Start()
{
	Logger->Log(ETestPawnActionMessage::Started);

	return Super::Start();
}

bool UTestPawnAction_Log::Pause(const UPawnAction* PausedBy)
{
	Logger->Log(ETestPawnActionMessage::Paused);

	return Super::Pause(PausedBy);
}

bool UTestPawnAction_Log::Resume()
{
	Logger->Log(ETestPawnActionMessage::Resumed);

	return Super::Resume();
}

void UTestPawnAction_Log::OnFinished(EPawnActionResult::Type WithResult)
{
	Super::OnFinished(WithResult);
	Logger->Log(ETestPawnActionMessage::Finished);	
}

void UTestPawnAction_Log::OnChildFinished(UPawnAction& Action, EPawnActionResult::Type WithResult)
{
	Super::OnChildFinished(Action, WithResult);
	Logger->Log(ETestPawnActionMessage::ChildFinished);
}
