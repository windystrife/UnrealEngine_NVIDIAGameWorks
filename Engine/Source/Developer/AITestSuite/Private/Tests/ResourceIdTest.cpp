// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "AITypes.h"
#include "AITestsCommon.h"
#include "Actions/TestPawnAction_Log.h"

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_ResourceIDBasic : public FAITestBase
{
	void InstantTest()
	{
		Test(TEXT("There are always some resources as long as AIModule is present"), FAIResources::GetResourcesCount() > 0);

		const FAIResourceID& MovementID = FAIResources::GetResource(FAIResources::Movement);
		Test(TEXT("Resource ID's indexes are broken!"), FAIResources::Movement == MovementID);
	}
};
IMPLEMENT_AI_INSTANT_TEST(FAITest_ResourceIDBasic, "System.Engine.AI.Resource ID.Basic operations")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_ResourceLock : public FAITestBase
{
	void InstantTest()
	{
		FAIResourceLock MockLock;

		// basic locking
		MockLock.SetLock(EAIRequestPriority::HardScript);
		Test(TEXT("Resource should be locked"), MockLock.IsLocked());
		Test(TEXT("Resource should be locked with specified priority"), MockLock.IsLockedBy(EAIRequestPriority::HardScript));
		Test(TEXT("Resource should not be available for lower priorities"), MockLock.IsAvailableFor(EAIRequestPriority::Logic) == false);
		Test(TEXT("Resource should be available for higher priorities"), MockLock.IsAvailableFor(EAIRequestPriority::Reaction));

		// clearing lock:
		// try clearing with lower priority
		MockLock.ClearLock(EAIRequestPriority::Logic);
		Test(TEXT("Resource should be still locked"), MockLock.IsLocked());
		Test(TEXT("Resource should still not be available for lower priorities"), MockLock.IsAvailableFor(EAIRequestPriority::Logic) == false);
		Test(TEXT("Resource should still be available for higher priorities"), MockLock.IsAvailableFor(EAIRequestPriority::Reaction));

		// releasing the actual lock
		MockLock.ClearLock(EAIRequestPriority::HardScript);
		Test(TEXT("Resource should be available now"), MockLock.IsLocked() == false);

		// clearing all locks at one go
		MockLock.SetLock(EAIRequestPriority::HardScript);
		MockLock.SetLock(EAIRequestPriority::Logic);
		MockLock.SetLock(EAIRequestPriority::Reaction);
		bool bWasLocked = MockLock.IsLocked();
		MockLock.ForceClearAllLocks();
		Test(TEXT("Resource should no longer be locked"), bWasLocked == true && MockLock.IsLocked() == false);

		// merging
		FAIResourceLock MockLock2;
		MockLock.SetLock(EAIRequestPriority::HardScript);
		MockLock2.SetLock(EAIRequestPriority::Logic);
		// merge
		MockLock2 += MockLock;
		Test(TEXT("Resource should be locked on both priorities"), MockLock2.IsLockedBy(EAIRequestPriority::Logic) && MockLock2.IsLockedBy(EAIRequestPriority::HardScript));
		MockLock2.ClearLock(EAIRequestPriority::Logic);
		Test(TEXT("At this point both locks should be identical"), MockLock == MockLock2);
	}
};
IMPLEMENT_AI_INSTANT_TEST(FAITest_ResourceLock, "System.Engine.AI.Resource ID.Resource locking")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_ResourceSet : public FAITestBase
{
	void InstantTest()
	{
		{
			FAIResourcesSet ResourceSet;
			Test(TEXT("Resource Set should be empty by default"), ResourceSet.IsEmpty());
			for (uint8 FlagIndex = 0; FlagIndex < FAIResourcesSet::MaxFlags; ++FlagIndex)
			{
				Test(TEXT("Resource Set should not contain any resources when empty"), ResourceSet.ContainsResourceIndex(FlagIndex) == false);
			}
		}

		{
			FAIResourcesSet ResourceSet(FAIResourcesSet::AllResources);
			Test(TEXT("Resource Set should be empty by default"), ResourceSet.IsEmpty() == false);
			for (uint8 FlagIndex = 0; FlagIndex < FAIResourcesSet::MaxFlags; ++FlagIndex)
			{
				Test(TEXT("Full Resource Set should contain every resource"), ResourceSet.ContainsResourceIndex(FlagIndex) == true);
			}
		}

		{
			const FAIResourceID& MovementResource = FAIResources::GetResource(FAIResources::Movement);
			const FAIResourceID& PerceptionResource = FAIResources::GetResource(FAIResources::Perception);

			FAIResourcesSet ResourceSet;
			ResourceSet.AddResource(PerceptionResource);
			Test(TEXT("Resource Set should contain added resource"), ResourceSet.ContainsResource(PerceptionResource));
			Test(TEXT("Resource Set should contain added resource given by Index"), ResourceSet.ContainsResourceIndex(PerceptionResource.Index));
			for (uint8 FlagIndex = 0; FlagIndex < FAIResourcesSet::MaxFlags; ++FlagIndex)
			{
				if (FlagIndex != PerceptionResource.Index)
				{
					Test(TEXT("Resource Set should not contain any other resources"), ResourceSet.ContainsResourceIndex(FlagIndex) == false);
				}
			}
			Test(TEXT("Resource Set should not be empty after adding a resource"), ResourceSet.IsEmpty() == false);

			ResourceSet.AddResourceIndex(MovementResource.Index);
			Test(TEXT("Resource Set should contain second added resource"), ResourceSet.ContainsResource(MovementResource));
			Test(TEXT("Resource Set should contain second added resource given by Index"), ResourceSet.ContainsResourceIndex(MovementResource.Index));

			ResourceSet.RemoveResource(MovementResource);
			Test(TEXT("Resource Set should no longer contain second added resource"), ResourceSet.ContainsResource(MovementResource) == false);
			Test(TEXT("Resource Set should still be not empty after removing one resource"), ResourceSet.IsEmpty() == false);

			ResourceSet.RemoveResourceIndex(PerceptionResource.Index);
			Test(TEXT("Resource Set should be empty after removing last resource"), ResourceSet.IsEmpty() == true);
		}
	}
};
IMPLEMENT_AI_INSTANT_TEST(FAITest_ResourceSet, "System.Engine.AI.Resource ID.Resource locking")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_PawnActions_PausingActionsOfSameResource : public FAITest_SimpleActionsTest
{
	void InstantTest()
	{
		/*Logger.ExpectedValues.Add(ETestPawnActionMessage::Started);
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Paused);
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Started);*/

		UWorld& World = GetWorld();
		UTestPawnAction_Log* MoveAction = UTestPawnAction_Log::CreateAction(World, Logger);
		MoveAction->GetRequiredResourcesSet() = FAIResourcesSet(FAIResources::Movement);
		Component->PushAction(*MoveAction, EAIRequestPriority::Logic);

		Component->TickComponent(FAITestHelpers::TickInterval, ELevelTick::LEVELTICK_All, nullptr);

		UTestPawnAction_Log* AnotherMoveAction = UTestPawnAction_Log::CreateAction(World, Logger);
		AnotherMoveAction->GetRequiredResourcesSet() = FAIResourcesSet(FAIResources::Movement);
		Component->PushAction(*AnotherMoveAction, EAIRequestPriority::Logic);

		Component->TickComponent(FAITestHelpers::TickInterval, ELevelTick::LEVELTICK_All, nullptr);

		Test(TEXT("First MoveAction should get paused"), MoveAction->IsPaused() == true);
	}
};
IMPLEMENT_AI_INSTANT_TEST(FAITest_PawnActions_PausingActionsOfSameResource, "System.Engine.AI.Pawn Actions.Pausing actions of same resource")

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
struct FAITest_PawnActions_NotPausingActionsOfDifferentResources : public FAITest_SimpleActionsTest
{
	void InstantTest()
	{
		/*Logger.ExpectedValues.Add(ETestPawnActionMessage::Started);
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Paused);
		Logger.ExpectedValues.Add(ETestPawnActionMessage::Started);*/

		UWorld& World = GetWorld();
		UTestPawnAction_Log* MoveAction = UTestPawnAction_Log::CreateAction(World, Logger);
		MoveAction->SetRequiredResourcesSet(FAIResourcesSet(FAIResources::Movement));
		Component->PushAction(*MoveAction, EAIRequestPriority::Logic);

		Component->TickComponent(FAITestHelpers::TickInterval, ELevelTick::LEVELTICK_All, nullptr);

		UTestPawnAction_Log* PerceptionAction = UTestPawnAction_Log::CreateAction(World, Logger);
		PerceptionAction->SetRequiredResourcesSet(FAIResourcesSet(FAIResources::Perception));
		Component->PushAction(*PerceptionAction, EAIRequestPriority::Logic);

		Component->TickComponent(FAITestHelpers::TickInterval, ELevelTick::LEVELTICK_All, nullptr);

		// @todo test temporarily disabled
		//Test(TEXT("First MoveAction should get paused"), MoveAction->IsPaused() == false && PerceptionAction->IsPaused() == false);
	}
};
IMPLEMENT_AI_INSTANT_TEST(FAITest_PawnActions_NotPausingActionsOfDifferentResources, "System.Engine.AI.Pawn Actions.Not pausing actions of different resources")
