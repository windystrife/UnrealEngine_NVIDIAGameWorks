// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "AITestsCommon.h"
#include "Actions/TestPawnAction_Log.h"
#include "Actions/TestPawnAction_CallFunction.h"

#define LOCTEXT_NAMESPACE "AITestSuite_PawnActionTest"

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_PawnActions_Push : public FAITest_SimpleActionsTest
{	
	UTestPawnAction_Log* Action;

	void SetUp()
	{
		FAITest_SimpleActionsTest::SetUp();

		UWorld& World = GetWorld();
		Action = UTestPawnAction_Log::CreateAction(World, Logger);
		Component->PushAction(*Action, EAIRequestPriority::Logic);

		Test(TEXT("No action should be active at this point"), Component->GetCurrentAction() == nullptr);
	}

	bool Update()
	{
		TickComponent();
		Test(TEXT("After one tick created action should be the active one"), Component->GetCurrentAction() == Action);
		Test(TEXT("After one tick created action should have been started"), Logger.LoggedValues.Top() == ETestPawnActionMessage::Started);
		return true;
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_PawnActions_Push, "System.Engine.AI.Pawn Actions.Pushing Single Action")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_PawnActions_PushingSameActionWithDelay : public FAITest_SimpleActionsTest
{
	UTestPawnAction_Log* Action;

	void SetUp()
	{
		FAITest_SimpleActionsTest::SetUp();

		UWorld& World = GetWorld();
		Action = UTestPawnAction_Log::CreateAction(World, Logger);
		Component->PushAction(*Action, EAIRequestPriority::Logic);
	}

	bool Update()
	{
		TickComponent();

		Test(TEXT("Addind an action for a second time should fail"), Component->PushAction(*Action, EAIRequestPriority::Logic) == false);
		Test(TEXT("Addind an action for a second time, but with different priority, should fail"), Component->PushAction(*Action, EAIRequestPriority::Ultimate) == false);
		
		return true;
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_PawnActions_PushingSameActionWithDelay, "System.Engine.AI.Pawn Actions.Pusihng action that has already been pushed should fail")


//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_PawnActions_Pause : public FAITest_SimpleActionsTest
{
	UTestPawnAction_Log* Action;

	void SetUp()
	{
		FAITest_SimpleActionsTest::SetUp();

		Logger.ExpectedValues.Add(ETestPawnActionMessage::Started);
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Paused);
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Started);

		UWorld& World = GetWorld();
		Action = UTestPawnAction_Log::CreateAction(World, Logger);
		Component->PushAction(*Action, EAIRequestPriority::Logic);
	}
		
	bool Update()
	{
		UWorld& World = GetWorld();

		TickComponent();
		UTestPawnAction_Log* AnotherAction = UTestPawnAction_Log::CreateAction(World, Logger);
		Component->PushAction(*AnotherAction, EAIRequestPriority::Logic);

		TickComponent();

		Test(TEXT("Second pushed action should be the active one"), Component->GetCurrentAction() == AnotherAction);
		Test(TEXT("First actionshould be paused"), Action->IsPaused() == true);

		return true; 
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_PawnActions_Pause, "System.Engine.AI.Pawn Actions.Pausing Action by younger Action of same priority")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_PawnActions_SamePriorityOrder : public FAITest_SimpleActionsTest
{
	UTestPawnAction_Log* FirstAction;

	void SetUp()
	{
		FAITest_SimpleActionsTest::SetUp();

		Logger.ExpectedValues.Add(ETestPawnActionMessage::Started);

		UWorld& World = GetWorld();
		FirstAction = UTestPawnAction_Log::CreateAction(World, Logger);
		Component->PushAction(*FirstAction, EAIRequestPriority::Logic);
		UTestPawnAction_Log* SecondAction = UTestPawnAction_Log::CreateAction(World, Logger);
		Component->PushAction(*SecondAction, EAIRequestPriority::Logic);
	}

	bool Update()
	{
		TickComponent();

		Test(TEXT("Second pushed action should be the active one"), Component->GetCurrentAction() != FirstAction);
		
		return true;
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_PawnActions_SamePriorityOrder, "System.Engine.AI.Pawn Actions.Respecting push order")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_PawnActions_DoublePushingAction : public FAITest_SimpleActionsTest
{
	bool Update()
	{
		UWorld& World = GetWorld();
		UTestPawnAction_Log* Action = UTestPawnAction_Log::CreateAction(World, Logger);
		Component->PushAction(*Action, EAIRequestPriority::Logic);
		Test(TEXT("Pushing same action for the second time should fail"), Component->PushAction(*Action, EAIRequestPriority::Logic) == false);
		Test(TEXT("There should be exactly one ActionEvent awaiting processing"), Component->GetActionEventsQueueSize() == 1);
	
		TickComponent();

		Test(TEXT("There should be only one action on stack now."), Component->GetActionStackSize(EAIRequestPriority::Logic) == 1);
		Test(TEXT("Action queue should be empty."), Component->GetActionEventsQueueSize() == 0);

		Test(TEXT("Pushing already active action should"), Component->PushAction(*Action, EAIRequestPriority::Logic) == false);
		Test(TEXT("Action queue should be empty."), Component->GetActionEventsQueueSize() == 0);

		return true;
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_PawnActions_DoublePushingAction, "System.Engine.AI.Pawn Actions.Pushing same action twice")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_PawnActions_SimplePriority : public FAITest_SimpleActionsTest
{
	bool Update()
	{
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Started);
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Paused);
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Started);

		UWorld& World = GetWorld();

		UTestPawnAction_Log* LowPriorityAction = UTestPawnAction_Log::CreateAction(World, Logger);
		Component->PushAction(*LowPriorityAction, EAIRequestPriority::Logic);
		TickComponent();

		UTestPawnAction_Log* HighPriorityAction = UTestPawnAction_Log::CreateAction(World, Logger);
		Component->PushAction(*HighPriorityAction, EAIRequestPriority::Reaction);
		TickComponent();

		Test(TEXT("There should be exactly one action on Logic stack now."), Component->GetActionStackSize(EAIRequestPriority::Logic) == 1);
		Test(TEXT("There should be exactly one action on Reaction stack now."), Component->GetActionStackSize(EAIRequestPriority::Reaction) == 1);
		Test(TEXT("The higher priority action should be the active one"), Component->GetCurrentAction() == HighPriorityAction);

		return true;
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_PawnActions_SimplePriority, "System.Engine.AI.Pawn Actions.Pushing different priority actions")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_PawnActions_HighPriorityKeepRunning : public FAITest_SimpleActionsTest
{
	bool Update()
	{
		// only this one event should get logged
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Started);

		UWorld& World = GetWorld();

		UTestPawnAction_Log* HighPriorityAction = UTestPawnAction_Log::CreateAction(World, Logger);
		Component->PushAction(*HighPriorityAction, EAIRequestPriority::Reaction);
		TickComponent();

		UTestPawnAction_Log* LowPriorityAction = UTestPawnAction_Log::CreateAction(World, Logger);
		Component->PushAction(*LowPriorityAction, EAIRequestPriority::Logic);
		TickComponent();

		Test(TEXT("There should be exactly one action on Logic stack now."), Component->GetActionStackSize(EAIRequestPriority::Logic) == 1);
		Test(TEXT("There should be exactly one action on Reaction stack now."), Component->GetActionStackSize(EAIRequestPriority::Reaction) == 1);
		Test(TEXT("The higher priority action should still be the active"), Component->GetCurrentAction() == HighPriorityAction);

		return true;
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_PawnActions_HighPriorityKeepRunning, "System.Engine.AI.Pawn Actions.High priority action still running after pushing lower priority action")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_PawnActions_SamePriorityActionsPushing : public FAITest_SimpleActionsTest
{
	bool Update()
	{
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Started);
		UWorld& World = GetWorld();
		
		Component->PushAction(*UTestPawnAction_Log::CreateAction(World, Logger), EAIRequestPriority::Logic);
		Component->PushAction(*UTestPawnAction_Log::CreateAction(World, Logger), EAIRequestPriority::Logic);
		Component->PushAction(*UTestPawnAction_Log::CreateAction(World, Logger), EAIRequestPriority::Logic);
		UPawnAction* LastAction = UTestPawnAction_Log::CreateAction(World, Logger);
		Component->PushAction(*LastAction, EAIRequestPriority::Logic);
		
		Test(TEXT("Action queue should be empty."), Component->GetActionEventsQueueSize() == 4);

		TickComponent();

		Test(TEXT("Last action pushed should the one active"), Component->GetCurrentAction() == LastAction);

		return true;
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_PawnActions_SamePriorityActionsPushing, "System.Engine.AI.Pawn Actions.Pushing multiple actions of same priority")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_PawnActions_Aborting : public FAITest_SimpleActionsTest
{
	bool Update()
	{
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Started);
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Finished);

		UWorld& World = GetWorld();
		UTestPawnAction_Log* Action = UTestPawnAction_Log::CreateAction(World, Logger);
		Component->PushAction(*Action, EAIRequestPriority::Logic);
		TickComponent();

		Component->AbortAction(*Action);
		TickComponent();

		Test(TEXT("There should be no actions on the stack."), Component->GetActionStackSize(EAIRequestPriority::Logic) == 0);
		Test(TEXT("There should be no current action"), Component->GetCurrentAction() == nullptr);

		return true;
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_PawnActions_Aborting, "System.Engine.AI.Pawn Actions.Basic aborting mechanics")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_PawnActions_PushAndAbort : public FAITest_SimpleActionsTest
{
	bool Update()
	{
		UWorld& World = GetWorld();
		UTestPawnAction_Log* Action = UTestPawnAction_Log::CreateAction(World, Logger);
		Component->PushAction(*Action, EAIRequestPriority::Logic);
		Component->AbortAction(*Action);

		TickComponent();

		Test(TEXT("There should be no actions on the stack."), Component->GetActionStackSize(EAIRequestPriority::Logic) == 0);
		Test(TEXT("There should be no current action"), Component->GetCurrentAction() == nullptr);
		Test(TEXT("No actuall work should have been done"), Logger.LoggedValues.Num() == 0);

		return true;
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_PawnActions_PushAndAbort, "System.Engine.AI.Pawn Actions.Push and Abort same frame")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_PawnActions_AbortAfterPushingNewAction : public FAITest_SimpleActionsTest
{
	bool Update()
	{
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Started);
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Finished);
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Started);

		UWorld& World = GetWorld();
		UTestPawnAction_Log* FirstAction = UTestPawnAction_Log::CreateAction(World, Logger);
		Component->PushAction(*FirstAction, EAIRequestPriority::Logic);
		TickComponent();
		
		UTestPawnAction_Log* SecondAction = UTestPawnAction_Log::CreateAction(World, Logger);
		Component->PushAction(*SecondAction, EAIRequestPriority::Logic);
		Component->AbortAction(*FirstAction);

		TickComponent();

		Test(TEXT("There should be exactly one action on stack."), Component->GetActionStackSize(EAIRequestPriority::Logic) == 1);
		Test(TEXT("Last pushed action should be the active one"), Component->GetCurrentAction() == SecondAction);

		return true;
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_PawnActions_AbortAfterPushingNewAction, "System.Engine.AI.Pawn Actions.Abort action after a newer action has been pushed")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_PawnActions_ActionPushingActions : public FAITest_SimpleActionsTest
{
	static void CreateNewAction(UPawnActionsComponent& ActionsComponent, UTestPawnAction_CallFunction& Caller, ETestPawnActionMessage::Type Message)
	{
		if (Message == ETestPawnActionMessage::Started)
		{
			UTestPawnAction_Log* NextAction = UTestPawnAction_Log::CreateAction(*Caller.GetWorld(), *Caller.Logger);
			ActionsComponent.PushAction(*NextAction, Caller.GetPriority());
		}
	}

	bool Update()
	{
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Started);
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Paused);
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Started);

		UWorld& World = GetWorld();
		UTestPawnAction_CallFunction* RootAction = UTestPawnAction_CallFunction::CreateAction(World, Logger, &FAITest_PawnActions_ActionPushingActions::CreateNewAction);
		Component->PushAction(*RootAction, EAIRequestPriority::Logic);

		TickComponent();

		Test(TEXT("There should be exactly one action on stack."), Component->GetActionStackSize(EAIRequestPriority::Logic) == 1);
		Test(TEXT("Root action action be the active one"), Component->GetCurrentAction() == RootAction);
		Test(TEXT("There should be exactly one action event pending"), Component->GetActionEventsQueueSize() == 1);

		TickComponent();

		Test(TEXT("There should be exactly one action on stack."), Component->GetActionStackSize(EAIRequestPriority::Logic) == 2);
		Test(TEXT("Root action action be the active one"), Component->GetCurrentAction() != RootAction);
		Test(TEXT("There should be exactly one action event pending"), Component->GetActionEventsQueueSize() == 0);

		return true;
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_PawnActions_ActionPushingActions, "System.Engine.AI.Pawn Actions.Action pushing Actions")

// scenarios to test
// * Push, wait for Start and Abort instantly. Implement a special action that will finish in X tick with result Y

#undef LOCTEXT_NAMESPACE
