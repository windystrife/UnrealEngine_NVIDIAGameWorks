// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AITestsCommon.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "BTBuilder.h"
#include "MockAI_BT.h"

namespace FAITestHelpers
{
	uint64 UpdatesCounter = 0;
	
	void UpdateFrameCounter()
	{
		static uint64 PreviousFramesCounter = GFrameCounter;
		if (PreviousFramesCounter != GFrameCounter)
		{
			++UpdatesCounter;
			PreviousFramesCounter = GFrameCounter;
		}
	}

	uint64 FramesCounter()
	{
		return UpdatesCounter;
	}
}

bool FAITestCommand_WaitSeconds::Update()
{
	float NewTime = FPlatformTime::Seconds();
	if (NewTime - StartTime >= Duration)
	{
		return true;
	}
	return false;
}

bool FAITestCommand_WaitOneTick::Update()
{
	if (bAlreadyRun == false)
	{
		bAlreadyRun = true;
		return true;
	}
	return false;
}

bool FAITestCommand_SetUpTest::Update()
{
	if (AITest)
	{
		AITest->SetUp();
	}
	return true;
}

bool FAITestCommand_PerformTest::Update()
{
	return AITest == nullptr || AITest->Update();
}

bool FAITestCommand_TearDownTest::Update()
{
	if (AITest)
	{
		AITest->TearDown();
		delete AITest;
		AITest = nullptr;
	}
	return true;
}

namespace FAITestHelpers
{
	UWorld* GetWorld()
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			return GWorld;
		}
#endif // WITH_EDITOR
		return GEngine->GetWorldContexts()[0].World();
	}
}

//----------------------------------------------------------------------//
// FAITestBase
//----------------------------------------------------------------------//
FAITestBase::~FAITestBase()
{
	check(bTearedDown && "Super implementation of TearDown not called!");
}

void FAITestBase::Test(const FString& Description, bool bValue)
{
	if (TestRunner)
	{
		TestRunner->TestTrue(Description, bValue);
	}
#if ENSURE_FAILED_TESTS
	ensure(bValue);
#endif // ENSURE_FAILED_TESTS
}

void FAITestBase::TearDown()
{
	bTearedDown = true;
	for (auto AutoDestroyedObject : SpawnedObjects)
	{
		AutoDestroyedObject->RemoveFromRoot();
	}
	SpawnedObjects.Reset();
}

//----------------------------------------------------------------------//
// FAITest_SimpleBT
//----------------------------------------------------------------------//
FAITest_SimpleBT::FAITest_SimpleBT()
{
	bUseSystemTicking = false;
	
	BTAsset = &FBTBuilder::CreateBehaviorTree();
}

void FAITest_SimpleBT::SetUp()
{
	FAITestBase::SetUp();

	AIBTUser = NewAutoDestroyObject<UMockAI_BT>();

	UMockAI_BT::ExecutionLog.Reset();

	if (AIBTUser && BTAsset)
	{
		AIBTUser->RunBT(*BTAsset, EBTExecutionMode::SingleRun);
		AIBTUser->SetEnableTicking(bUseSystemTicking);
	}
}

bool FAITest_SimpleBT::Update()
{
	FAITestHelpers::UpdateFrameCounter();

	if (AIBTUser != NULL)
	{
		if (bUseSystemTicking == false)
		{
			AIBTUser->TickMe(FAITestHelpers::TickInterval);
		}

		if (AIBTUser->IsRunning())
		{
			return false;
		}
	}

	VerifyResults();
	return true;
}

void FAITest_SimpleBT::VerifyResults()
{
	const bool bMatch = (ExpectedResult == UMockAI_BT::ExecutionLog);
	//ensure(bMatch && "VerifyResults failed!");
	if (!bMatch)
	{
		FString DescriptionResult;
		for (int32 Idx = 0; Idx < UMockAI_BT::ExecutionLog.Num(); Idx++)
		{
			DescriptionResult += TTypeToString<int32>::ToString(UMockAI_BT::ExecutionLog[Idx]);
			if (Idx < (UMockAI_BT::ExecutionLog.Num() - 1))
			{
				DescriptionResult += TEXT(", ");
			}
		}

		FString DescriptionExpected;
		for (int32 Idx = 0; Idx < ExpectedResult.Num(); Idx++)
		{
			DescriptionExpected += TTypeToString<int32>::ToString(ExpectedResult[Idx]);
			if (Idx < (ExpectedResult.Num() - 1))
			{
				DescriptionExpected += TEXT(", ");
			}
		}

		UE_LOG(LogBehaviorTreeTest, Error, TEXT("Test scenario failed to produce expected results!\nExecution log: %s\nExpected values: %s"), *DescriptionResult, *DescriptionExpected);
	}
}
