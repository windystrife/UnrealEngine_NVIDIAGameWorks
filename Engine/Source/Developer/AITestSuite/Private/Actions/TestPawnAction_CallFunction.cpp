// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Actions/TestPawnAction_CallFunction.h"
#include "Engine/World.h"

UTestPawnAction_CallFunction::UTestPawnAction_CallFunction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FunctionToCall(nullptr)
{

}

UTestPawnAction_CallFunction* UTestPawnAction_CallFunction::CreateAction(UWorld& World, FTestLogger<int32>& InLogger, FFunctionToCall InFunctionToCall)
{
	UTestPawnAction_CallFunction* Action = UPawnAction::CreateActionInstance<UTestPawnAction_CallFunction>(World);
	if (Action)
	{
		Action->Logger = &InLogger;
		Action->FunctionToCall = InFunctionToCall;
	}

	return Action;
}

bool UTestPawnAction_CallFunction::Start()
{
	if (Super::Start())
	{
		(*FunctionToCall)(*GetOwnerComponent(), *this, ETestPawnActionMessage::Started);
		return true;
	}
	return false;
}

bool UTestPawnAction_CallFunction::Pause(const UPawnAction* PausedBy)
{
	if (Super::Pause(PausedBy))
	{
		(*FunctionToCall)(*GetOwnerComponent(), *this, ETestPawnActionMessage::Paused);
		return true;
	}
	return false;
}

bool UTestPawnAction_CallFunction::Resume()
{
	if (Super::Resume())
	{
		(*FunctionToCall)(*GetOwnerComponent(), *this, ETestPawnActionMessage::Resumed);
		return true;
	}
	return false;
}

void UTestPawnAction_CallFunction::OnFinished(EPawnActionResult::Type WithResult)
{
	Super::OnFinished(WithResult);
	(*FunctionToCall)(*GetOwnerComponent(), *this, ETestPawnActionMessage::Finished);
}

void UTestPawnAction_CallFunction::OnChildFinished(UPawnAction& Action, EPawnActionResult::Type WithResult)
{
	Super::OnChildFinished(Action, WithResult);
	(*FunctionToCall)(*GetOwnerComponent(), *this, ETestPawnActionMessage::ChildFinished);
}
