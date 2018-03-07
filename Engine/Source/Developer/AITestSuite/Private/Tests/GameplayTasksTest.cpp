// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "GameplayTask.h"
#include "AITestsCommon.h"
#include "MockGameplayTasks.h"
//#include "MockAI_BT.h"
//#include "BehaviorTree/TestBTDecorator_CantExecute.h"
//#include "Actions/TestPawnAction_CallFunction.h"

#define LOCTEXT_NAMESPACE "AITestSuite_GameplayTasksTest"

typedef FAITest_SimpleComponentBasedTest<UMockGameplayTasksComponent> FAITest_GameplayTasksTest;

static const FGameplayResourceSet::FResourceID ResourceMovement = 0;
static const FGameplayResourceSet::FResourceID ResourceLogic = 1;
static const FGameplayResourceSet::FResourceID ResourceAnimation = 2;

static const FGameplayResourceSet MovementResourceSet = FGameplayResourceSet().AddID(ResourceMovement);
static const FGameplayResourceSet LogicResourceSet = FGameplayResourceSet().AddID(ResourceLogic);
static const FGameplayResourceSet AnimationResourceSet = FGameplayResourceSet().AddID(ResourceAnimation);
static const FGameplayResourceSet MoveAndAnimResourceSet = FGameplayResourceSet().AddSet(MovementResourceSet).AddSet(AnimationResourceSet);
static const FGameplayResourceSet MoveAndLogicResourceSet = FGameplayResourceSet((1 << ResourceMovement) | (1 << ResourceLogic));
static const FGameplayResourceSet MoveAnimLogicResourceSet = FGameplayResourceSet((1 << ResourceMovement) | (1 << ResourceLogic) | (1 << ResourceAnimation));

static const uint8 LowPriority = 1;
static const uint8 HighPriority = 255;

struct FAITest_GameplayTask_ComponentState : public FAITest_GameplayTasksTest
{
	void InstantTest()
	{
		FAITest_GameplayTasksTest::SetUp();

		Test(TEXT("Initially UGameplayTasksComponent should not want to tick"), Component->GetShouldTick() == false);

		/*UWorld& World = GetWorld();
		Task = UMockTask_Log::CreateTask(*Component, Logger);

		Test(TEXT("Task should be \'uninitialized\' before Activate is called on it"), Task->GetState() == EGameplayTaskState::AwaitingActivation);

		Component->

			Task->ReadyForActivation();
		Test(TEXT("Task should be \'uninitialized\' before Activate is called on it"), Task->GetState() == EGameplayTaskState::Active);


		Test(TEXT("Task should be \'uninitialized\' before Activate is called on it"), Task->GetState() == EGameplayTaskState::Active);*/
	}
};
IMPLEMENT_AI_INSTANT_TEST(FAITest_GameplayTask_ComponentState, "System.Engine.AI.Gameplay Tasks.Component\'s basic behavior")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_GameplayTask_ExternalCancelWithTick : public FAITest_GameplayTasksTest
{	
	UMockTask_Log* Task;

	void SetUp()
	{
		FAITest_GameplayTasksTest::SetUp();

		Logger.ExpectedValues.Add(ETestTaskMessage::Activate);
		Logger.ExpectedValues.Add(ETestTaskMessage::Tick);
		Logger.ExpectedValues.Add(ETestTaskMessage::ExternalCancel);
		Logger.ExpectedValues.Add(ETestTaskMessage::Ended);

		UWorld& World = GetWorld();
		Task = UMockTask_Log::CreateTask(*Component, Logger);
		Task->EnableTick();
		
		Test(TEXT("Task should be \'uninitialized\' before Activate is called on it"), Task->GetState() == EGameplayTaskState::AwaitingActivation);
		
		Task->ReadyForActivation();
		Test(TEXT("Task should be \'Active\' after basic call to ReadyForActivation"), Task->GetState() == EGameplayTaskState::Active);
		Test(TEXT("Component should want to tick in this scenario"), Component->GetShouldTick() == true);
	}

	bool Update()
	{
		TickComponent();

		Task->ExternalCancel();
		
		return true;
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_GameplayTask_ExternalCancelWithTick, "System.Engine.AI.Gameplay Tasks.External Cancel with Tick")

//----------------------------------------------------------------------//
// In this test the task should get properly created, acticated and end 
// during update without any ticking
//----------------------------------------------------------------------//
struct FAITest_GameplayTask_SelfEnd : public FAITest_GameplayTasksTest
{
	UMockTask_Log* Task;

	void SetUp()
	{
		FAITest_GameplayTasksTest::SetUp();

		Logger.ExpectedValues.Add(ETestTaskMessage::Activate);
		Logger.ExpectedValues.Add(ETestTaskMessage::Ended);

		UWorld& World = GetWorld();
		Task = UMockTask_Log::CreateTask(*Component, Logger);
		Task->EnableTick();
		Task->ReadyForActivation();
	}

	bool Update()
	{
		Task->EndTask();

		return true;
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_GameplayTask_SelfEnd, "System.Engine.AI.Gameplay Tasks.Self End")

//----------------------------------------------------------------------//
// Testing multiple simultaneously ticking tasks
//----------------------------------------------------------------------//
struct FAITest_GameplayTask_SimulatanousTick : public FAITest_GameplayTasksTest
{
	UMockTask_Log* Tasks[3];

	void SetUp()
	{
		FAITest_GameplayTasksTest::SetUp();

		Logger.ExpectedValues.Add(ETestTaskMessage::Activate);
		Logger.ExpectedValues.Add(ETestTaskMessage::Activate); 
		Logger.ExpectedValues.Add(ETestTaskMessage::Activate); 
		Logger.ExpectedValues.Add(ETestTaskMessage::Tick);
		Logger.ExpectedValues.Add(ETestTaskMessage::Tick);
		Logger.ExpectedValues.Add(ETestTaskMessage::Tick);
		Logger.ExpectedValues.Add(ETestTaskMessage::ExternalCancel);
		Logger.ExpectedValues.Add(ETestTaskMessage::Ended);
		Logger.ExpectedValues.Add(ETestTaskMessage::ExternalCancel);
		Logger.ExpectedValues.Add(ETestTaskMessage::Ended);
		Logger.ExpectedValues.Add(ETestTaskMessage::ExternalCancel);
		Logger.ExpectedValues.Add(ETestTaskMessage::Ended);

		for (int32 Index = 0; Index < sizeof(Tasks) / sizeof(UMockTask_Log*); ++Index)
		{
			Tasks[Index] = UMockTask_Log::CreateTask(*Component, Logger);
			Tasks[Index]->EnableTick();
			Tasks[Index]->ReadyForActivation();
		}
		Test(TEXT("Component should want to tick in this scenario"), Component->GetShouldTick() == true);
	}

	bool Update()
	{
		TickComponent();
		for (int32 Index = 0; Index < sizeof(Tasks) / sizeof(UMockTask_Log*); ++Index)
		{
			Tasks[Index]->ExternalCancel();
		}
		Test(TEXT("Component should want to tick in this scenario"), Component->GetShouldTick() == false);

		return true;
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_GameplayTask_SimulatanousTick, "System.Engine.AI.Gameplay Tasks.Simultanously ticking tasks")

//----------------------------------------------------------------------//
// Testing multiple simultaneously ticking tasks UGameplayTaskResource
//----------------------------------------------------------------------//
struct FAITest_GameplayTask_ResourceSet : public FAITestBase
{
	void InstantTest()
	{
		FGameplayResourceSet ResourcesSet;
		Test(TEXT("New FGameplayResourceSet should be empty initialy"), ResourcesSet.IsEmpty() == true);
		ResourcesSet.AddID(ResourceLogic);
		Test(TEXT("Added one ID, ResourcesSet should not be perceived as empty now"), ResourcesSet.IsEmpty() == false);
		Test(TEXT("Added one ID, ResourcesSet should not be perceived as empty now"), ResourcesSet.IsEmpty() == false);
		ResourcesSet.RemoveID(ResourceAnimation);
		Test(TEXT("Removed ID not previously added, ResourcesSet should not be perceived as empty now"), ResourcesSet.IsEmpty() == false);
		ResourcesSet.RemoveID(ResourceLogic);
		Test(TEXT("Removed ID previously added, ResourcesSet should be empty now"), ResourcesSet.IsEmpty());

		Test(TEXT("Single ID checking, not present ID"), MoveAndAnimResourceSet.HasAnyID(LogicResourceSet) == false);
		Test(TEXT("Single ID checking"), MoveAndAnimResourceSet.HasAnyID(MovementResourceSet));
		Test(TEXT("Single ID checking"), MoveAndAnimResourceSet.HasAnyID(AnimationResourceSet));

		Test(TEXT("Multiple ID checking - has all, self test"), MoveAndAnimResourceSet.HasAllIDs(MoveAndAnimResourceSet));
		Test(TEXT("Multiple ID checking - has all, other identical"), MoveAndAnimResourceSet.HasAllIDs(FGameplayResourceSet((1 << ResourceMovement) | (1 << ResourceAnimation))));
		Test(TEXT("Multiple ID checking - has all, other different"), MoveAndAnimResourceSet.HasAllIDs(MoveAndLogicResourceSet) == false);
		Test(TEXT("Multiple ID checking - overlap"), MoveAndAnimResourceSet.GetOverlap(MoveAndLogicResourceSet) == MovementResourceSet);
		Test(TEXT("Multiple ID checking - substraction"), MoveAndAnimResourceSet.GetDifference(MoveAndLogicResourceSet) == AnimationResourceSet);
		
		Test(TEXT("FGameplayResourceSet containing 0-th ID is not empty"), MovementResourceSet.IsEmpty() == false);
		Test(TEXT("FGameplayResourceSet has 0-th ID"), MovementResourceSet.HasID(ResourceMovement));
	}
};
IMPLEMENT_AI_INSTANT_TEST(FAITest_GameplayTask_ResourceSet, "System.Engine.AI.Gameplay Tasks.Resource Set")

//----------------------------------------------------------------------//
// Running tasks requiring non-overlapping resources
//----------------------------------------------------------------------//
struct FAITest_GameplayTask_NonOverlappingResources : public FAITest_GameplayTasksTest
{
	UMockTask_Log* Tasks[2];

	void InstantTest()
	{
		UWorld& World = GetWorld();
		
		Tasks[0] = UMockTask_Log::CreateTask(*Component, Logger, MoveAndAnimResourceSet);
		Tasks[1] = UMockTask_Log::CreateTask(*Component, Logger, LogicResourceSet);

		Tasks[0]->ReadyForActivation();
		Test(TEXT("TasksComponent should claim it's using 0th task's resources"), Component->GetCurrentlyUsedResources() == Tasks[0]->GetClaimedResources());
		
		Tasks[1]->ReadyForActivation();		
		Test(TEXT("Both tasks should be \'Active\' since their resources do not overlap"), Tasks[0]->GetState() == EGameplayTaskState::Active && Tasks[1]->GetState() == EGameplayTaskState::Active);
		Test(TEXT("TasksComponent should claim it's using only latter task's resources"), Component->GetCurrentlyUsedResources() == MoveAnimLogicResourceSet);

		Tasks[0]->ExternalCancel();
		Test(TEXT("Only index 1 task's resources should be relevant now"), Component->GetCurrentlyUsedResources() == Tasks[1]->GetClaimedResources());

		Tasks[1]->ExternalCancel();
		Test(TEXT("No resources should be occupied now"), Component->GetCurrentlyUsedResources().IsEmpty());
	}
};
IMPLEMENT_AI_INSTANT_TEST(FAITest_GameplayTask_NonOverlappingResources, "System.Engine.AI.Gameplay Tasks.Non-overlapping resources")

//----------------------------------------------------------------------//
// Running tasks requiring overlapping resources
//----------------------------------------------------------------------//
struct FAITest_GameplayTask_OverlappingResources : public FAITest_GameplayTasksTest
{
	UMockTask_Log* Tasks[2];

	void InstantTest()
	{
		UWorld& World = GetWorld();

		Tasks[0] = UMockTask_Log::CreateTask(*Component, Logger, MoveAndAnimResourceSet);
		Tasks[1] = UMockTask_Log::CreateTask(*Component, Logger, MoveAndLogicResourceSet);

		Tasks[0]->ReadyForActivation();
		Tasks[1]->ReadyForActivation();

		Test(TEXT("Only the latter task should be active since it shadows the other one in terms of required resources"), Tasks[1]->GetState() == EGameplayTaskState::Active);
		Test(TEXT("The first task should be paused at this moment"), Tasks[0]->GetState() == EGameplayTaskState::Paused);
		Test(TEXT("TasksComponent should claim it's using only latter task's resources"), Component->GetCurrentlyUsedResources() == Tasks[1]->GetClaimedResources());

		Tasks[1]->ExternalCancel();
		Test(TEXT("Now the latter task should be marked as Finished"), Tasks[1]->GetState() == EGameplayTaskState::Finished);
		Test(TEXT("And the first task should be resumed"), Tasks[0]->GetState() == EGameplayTaskState::Active);
		Test(TEXT("TasksComponent should claim it's using only first task's resources"), Component->GetCurrentlyUsedResources() == Tasks[0]->GetClaimedResources());
	}
};
IMPLEMENT_AI_INSTANT_TEST(FAITest_GameplayTask_OverlappingResources, "System.Engine.AI.Gameplay Tasks.Overlapping resources")

//----------------------------------------------------------------------//
// Pausing a task overlapping a lower priority task should not resume the low priority task
//----------------------------------------------------------------------//
struct FAITest_GameplayTask_PausingTasksBlockingOtherTasks : public FAITest_GameplayTasksTest
{
	UMockTask_Log* Tasks[3];

	void InstantTest()
	{
		UWorld& World = GetWorld();

		Tasks[0] = UMockTask_Log::CreateTask(*Component, Logger, MovementResourceSet);
		Tasks[1] = UMockTask_Log::CreateTask(*Component, Logger, MoveAndLogicResourceSet);
		Tasks[2] = UMockTask_Log::CreateTask(*Component, Logger, LogicResourceSet);
		 
		Tasks[0]->ReadyForActivation();
		Tasks[1]->ReadyForActivation();

		Test(TEXT("First task should be paused since it's resources get overlapped"), Tasks[0]->IsActive() == false);
		Test(TEXT("Second task should on top and active"), Tasks[1]->IsActive() == true);

		Tasks[2]->ReadyForActivation();
		Test(TEXT("Second task should get paused since its resources got overlapped"), Tasks[1]->IsActive() == false);
		Test(TEXT("First task should remain paused since it's resources get overlapped by the paused task"), Tasks[0]->IsActive() == false);

		Tasks[2]->ExternalCancel();
		Test(TEXT("Nothing shoud change for the first task"), Tasks[0]->IsActive() == false);
		Test(TEXT("Second task should be active again"), Tasks[1]->IsActive() == true);
	}
};
IMPLEMENT_AI_INSTANT_TEST(FAITest_GameplayTask_PausingTasksBlockingOtherTasks, "System.Engine.AI.Gameplay Tasks.Pausing tasks blocking other tasks")

//----------------------------------------------------------------------//
// Priority handling
//----------------------------------------------------------------------//
struct FAITest_GameplayTask_Priorities : public FAITest_GameplayTasksTest
{
	UMockTask_Log* Tasks[3];

	void InstantTest()
	{
		UWorld& World = GetWorld();

		// all tasks use same resources, have different priorities
		// let's do some tick testing as well
		Tasks[0] = UMockTask_Log::CreateTask(*Component, Logger, MoveAndAnimResourceSet, LowPriority);
		Tasks[0]->EnableTick();
		Tasks[1] = UMockTask_Log::CreateTask(*Component, Logger, MoveAndAnimResourceSet);
		Tasks[1]->EnableTick();
		Tasks[2] = UMockTask_Log::CreateTask(*Component, Logger, MoveAndAnimResourceSet, HighPriority);		

		Tasks[1]->ReadyForActivation();
		Tasks[0]->ReadyForActivation();

		Test(TEXT("Task at index 1 should be active at this point since it's higher priority"), Tasks[1]->IsActive() && !Tasks[0]->IsActive());
		Test(TEXT("TasksComponent should claim it's using only resources of task 1"), Component->GetCurrentlyUsedResources() == Tasks[1]->GetClaimedResources());
		Test(TEXT("Current top action wants to tick so Component should want that as well"), Component->GetShouldTick() == true); 

		Tasks[2]->ReadyForActivation();
		Test(TEXT("Now the last pushed, highest priority task should be active"), Tasks[2]->IsActive() && !Tasks[0]->IsActive() && !Tasks[1]->IsActive());
		Test(TEXT("No ticking task is active so Component should not want to tick"), Component->GetShouldTick() == false);
		
		Tasks[1]->ExternalCancel();
		Test(TEXT("Canceling mid-priority inactive task should not influence what's active"), Tasks[2]->IsActive() && !Tasks[0]->IsActive() && !Tasks[1]->IsActive());
		Test(TEXT("Current top action still doesn't want to tick, so neither should the Component"), Component->GetShouldTick() == false);

		Tasks[2]->ExternalCancel();
		Test(TEXT("After canceling the top-priority task the lowest priority task remains to be active"), !Tasks[2]->IsActive() && Tasks[0]->IsActive() && !Tasks[1]->IsActive());
		Test(TEXT("New top action wants tick, so should Component"), Component->GetShouldTick() == true);

		Tasks[0]->ExternalCancel();
		Test(TEXT("Task-less component should not want to tick"), Component->GetShouldTick() == false);
	}
};
IMPLEMENT_AI_INSTANT_TEST(FAITest_GameplayTask_Priorities, "System.Engine.AI.Gameplay Tasks.Priorities")

//----------------------------------------------------------------------//
// Internal ending, by task ending itself or owner finishing 
//----------------------------------------------------------------------//
struct FAITest_GameplayTask_InternalEnding : public FAITest_GameplayTasksTest
{
	static const int32 TasksCount = 4;
	UMockTask_Log* Task_SelfEndNoResources;
	UMockTask_Log* Task_OwnerEndNoResources;
	UMockTask_Log* Task_SelfEndWithResources;
	UMockTask_Log* Task_OwnerEndWithResources;
	UMockTask_Log* Tasks[TasksCount];

	void InstantTest()
	{
		UWorld& World = GetWorld();

		// all tasks use same resources, have different priorities
		Tasks[0] = Task_SelfEndNoResources = UMockTask_Log::CreateTask(*Component, Logger);
		Tasks[1] = Task_OwnerEndNoResources = UMockTask_Log::CreateTask(*Component, Logger);
		// not using overlapping resources set on purpose - want to test them independently
		Tasks[2] = Task_SelfEndWithResources = UMockTask_Log::CreateTask(*Component, Logger, MovementResourceSet);
		Tasks[3] = Task_OwnerEndWithResources = UMockTask_Log::CreateTask(*Component, Logger, LogicResourceSet);
		
		for (int32 Index = 0; Index < TasksCount; ++Index)
		{
			Tasks[Index]->ReadyForActivation();
			Test(TEXT("Trivial activation should succeed"), Tasks[Index]->IsActive());
		}
		Test(TEXT("Resources should sum up"), Component->GetCurrentlyUsedResources() == MoveAndLogicResourceSet);

		Task_SelfEndNoResources->EndTask();
		Test(TEXT("Task_SelfEndNoResources should be \'done\' now"), Task_SelfEndNoResources->IsActive() == false);

		Task_OwnerEndNoResources->TaskOwnerEnded();
		Test(TEXT("Task_SelfEndNoResources should be \'done\' now"), Task_OwnerEndNoResources->IsActive() == false);

		Task_SelfEndWithResources->EndTask();
		Test(TEXT("Task_SelfEndWithResources should be \'done\' now"), Task_SelfEndWithResources->IsActive() == false);
		Test(TEXT("Only the other task's resources should matter now"), Component->GetCurrentlyUsedResources() == LogicResourceSet);
		Test(TEXT("There should be only one active task in the priority queue"), Component->GetTaskPriorityQueueSize() == 1);

		Task_OwnerEndWithResources->TaskOwnerEnded();
		Test(TEXT("Task_SelfEndWithResources should be \'done\' now"), Task_OwnerEndWithResources->IsActive() == false);
		Test(TEXT("No resources should be locked at this moment"), Component->GetCurrentlyUsedResources().IsEmpty());
		Test(TEXT("Priority Task Queue should be empty"), Component->GetTaskPriorityQueueSize() == 0);
	}
};
IMPLEMENT_AI_INSTANT_TEST(FAITest_GameplayTask_InternalEnding, "System.Engine.AI.Gameplay Tasks.Self and Owner ending")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_GameplayTask_MultipleOwners : public FAITest_GameplayTasksTest
{
	static const int32 TasksCount = 3;
	UMockTask_Log* Tasks[TasksCount];
	UMockTask_Log* LowPriorityTask;
	UMockGameplayTaskOwner* OtherOwner;

	void InstantTest()
	{
		OtherOwner = NewObject<UMockGameplayTaskOwner>();
		OtherOwner->GTComponent = Component;
		Tasks[0] = UMockTask_Log::CreateTask(*Component, Logger, MovementResourceSet);
		Tasks[1] = UMockTask_Log::CreateTask(*OtherOwner, Logger, MovementResourceSet);
		Tasks[2] = UMockTask_Log::CreateTask(*Component, Logger, MovementResourceSet);

		for (int32 Index = 0; Index < TasksCount; ++Index)
		{
			Tasks[Index]->ReadyForActivation();
		}
		// This part tests what happens if "other owner" task is in the middle of the queue and
		// not active
		Test(TEXT("Last pushed task should be active now"), Tasks[2]->IsActive() == true);
		Component->EndAllResourceConsumingTasksOwnedBy(*Component);
		Test(TEXT("There should be only one task in the queue now"), Component->GetTaskPriorityQueueSize() == 1);
		Test(TEXT("The last remaining task should be active now"), Tasks[1]->IsActive() == true);

		// this part tests what happens during pruning if the "other owner" task is active at the
		// moment of performing the action
		LowPriorityTask = UMockTask_Log::CreateTask(*Component, Logger, MovementResourceSet, LowPriority);
		LowPriorityTask->ReadyForActivation();
		Test(TEXT("There should be 2 tasks in the queue now"), Component->GetTaskPriorityQueueSize() == 2);
		Component->EndAllResourceConsumingTasksOwnedBy(*Component);
		Test(TEXT("There should be only one task in the queue after second pruning"), Component->GetTaskPriorityQueueSize() == 1);
		Test(TEXT("The last remaining task should be still active"), Tasks[1]->IsActive() == true);
	}
};
IMPLEMENT_AI_INSTANT_TEST(FAITest_GameplayTask_MultipleOwners, "System.Engine.AI.Gameplay Tasks.Handling multiple task owners")

//----------------------------------------------------------------------//
// Claimed vs Required resources test
//----------------------------------------------------------------------//
struct FAITest_GameplayTask_ClaimedResources : public FAITest_GameplayTasksTest
{
	static const int32 TasksCount = 4;
	UMockTask_Log* Tasks[TasksCount];

	void InstantTest()
	{
		UWorld& World = GetWorld();

		// create three tasks
		// first one has a resource we're going to overlap with the extra-claimed resource of the next task
		Tasks[0] = UMockTask_Log::CreateTask(*Component, Logger, MovementResourceSet);
		Tasks[0]->ReadyForActivation();

		// second task requires non-overlapping resources to first task
		Tasks[1] = UMockTask_Log::CreateTask(*Component, Logger, AnimationResourceSet);
		// but declared an overlapping resource as "claimed"
		Tasks[1]->AddClaimedResourceSet(MovementResourceSet);
		
		Tasks[1]->ReadyForActivation();
		// at this point first task should get paused since it's required resource is claimed, or shadowed, by the newer task
		Test(TEXT("The first task should get paused since its required resource is claimed, or shadowed, by the newer task"), Tasks[0]->IsActive() == false);
		Test(TEXT("The second task should be running, nothing obstructing it"), Tasks[1]->IsActive() == true);
		
		// a new low-priority task should not be allowed to run neither
		Tasks[2] = UMockTask_Log::CreateTask(*Component, Logger, MovementResourceSet, LowPriority);
		Tasks[2]->ReadyForActivation();
		Test(TEXT("The new low-priority task should not be allowed to run neither"), Tasks[2]->IsActive() == false);
		Test(TEXT("The second task should be still running"), Tasks[1]->IsActive() == true);

		// however, a new task, that's using the overlapped claimed resource 
		// should run without any issues
		// note, this doesn't have to be "high priority" task - new tasks with same priority as "current"
		// are treated like higher priority anyway
		Tasks[3] = UMockTask_Log::CreateTask(*Component, Logger, MovementResourceSet, HighPriority);
		Tasks[3]->ReadyForActivation();
		Test(TEXT("The new high-priority task should be allowed to run"), Tasks[3]->IsActive() == true);
		// but active task, that declared the active resources should not get paused neither
		Test(TEXT("The second task should be still running, it's required resources are not being overlapped"), Tasks[1]->IsActive() == true);
	}
};
IMPLEMENT_AI_INSTANT_TEST(FAITest_GameplayTask_ClaimedResources, "System.Engine.AI.Gameplay Tasks.Claimed resources")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_GameplayTask_ClaimedResourcesAndInstantFinish : public FAITest_GameplayTasksTest
{
	static const int32 TasksCount = 4;
	UMockTask_Log* Task;

	void InstantTest()
	{
		UWorld& World = GetWorld();

		Task = UMockTask_Log::CreateTask(*Component, Logger, MovementResourceSet);
		Task->SetInstaEnd(true);
		Task->ReadyForActivation();

		Test(TEXT("No claimed resources should be left behind"), Component->GetCurrentlyUsedResources().IsEmpty() == true);
		Test(TEXT("There should no active tasks when task auto-insta-ended"), Component->GetTaskPriorityQueueSize() == 0);
	}
};
IMPLEMENT_AI_INSTANT_TEST(FAITest_GameplayTask_ClaimedResourcesAndInstantFinish, "System.Engine.AI.Gameplay Tasks.Claimed resources vs Insta-finish tasks")

// add tests if component wants ticking at while aborting/reactivating tasks
// add test for re-adding/re-activating a finished task

#undef LOCTEXT_NAMESPACE
