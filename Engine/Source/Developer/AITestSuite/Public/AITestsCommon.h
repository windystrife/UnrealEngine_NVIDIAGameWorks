// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "TestLogger.h"
#include "Actions/PawnActionsComponent.h"

#define ENSURE_FAILED_TESTS 1

class UBehaviorTree;
class UMockAI_BT;

DECLARE_LOG_CATEGORY_EXTERN(LogAITestSuite, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogBehaviorTreeTest, Log, All);

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FAITestCommand_WaitSeconds, float, Duration);

class FAITestCommand_WaitOneTick : public IAutomationLatentCommand
{
public: 
	FAITestCommand_WaitOneTick()
		: bAlreadyRun(false)
	{} 
	virtual bool Update() override;
private: 
	bool bAlreadyRun;
};


namespace FAITestHelpers
{
	UWorld* GetWorld();
	static const float TickInterval = 1.f / 30;

	void UpdateFrameCounter();
	uint64 FramesCounter();
}

struct AITESTSUITE_API FAITestBase
{
private:
	// internals
	TArray<UObject*> SpawnedObjects;	
	uint32 bTearedDown : 1;
protected:
	FAutomationTestBase* TestRunner;

	FAITestBase() : bTearedDown(false), TestRunner(nullptr)
	{}

	template<typename ClassToSpawn>
	ClassToSpawn* NewAutoDestroyObject()
	{
		ClassToSpawn* ObjectInstance = NewObject<ClassToSpawn>();
		ObjectInstance->AddToRoot();
		SpawnedObjects.Add(ObjectInstance);
		return ObjectInstance;
	}

	UWorld& GetWorld() const
	{
		UWorld* World = FAITestHelpers::GetWorld();
		check(World);
		return *World;
	}

	// loggin helper
	void Test(const FString& Description, bool bValue);

public:

	virtual void SetTestInstance(FAutomationTestBase& AutomationTestInstance) { TestRunner = &AutomationTestInstance; }

	// interface
	virtual ~FAITestBase();
	virtual void SetUp() {}
	/** @return true to indicate that the test is done. */
	virtual bool Update() { return true; }
	virtual void InstantTest() {}
	// must be called!
	virtual void TearDown();
};

DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(AITESTSUITE_API, FAITestCommand_SetUpTest, FAITestBase*, AITest);
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(AITESTSUITE_API, FAITestCommand_PerformTest, FAITestBase*, AITest);
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(AITESTSUITE_API, FAITestCommand_TearDownTest, FAITestBase*, AITest);

// @note that TestClass needs to derive from FAITestBase
#define IMPLEMENT_AI_LATENT_TEST(TestClass, PrettyName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(TestClass##_Runner, PrettyName, (EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)) \
	bool TestClass##_Runner::RunTest(const FString& Parameters) \
	{ \
		/* spawn test instance. Setup should be done in test's constructor */ \
		TestClass* TestInstance = new TestClass(); \
		TestInstance->SetTestInstance(*this); \
		/* set up */ \
		ADD_LATENT_AUTOMATION_COMMAND(FAITestCommand_SetUpTest(TestInstance)); \
		/* run latent command to update */ \
		ADD_LATENT_AUTOMATION_COMMAND(FAITestCommand_PerformTest(TestInstance)); \
		/* run latent command to tear down */ \
		ADD_LATENT_AUTOMATION_COMMAND(FAITestCommand_TearDownTest(TestInstance)); \
		return true; \
	} 

#define IMPLEMENT_AI_INSTANT_TEST(TestClass, PrettyName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(TestClass##Runner, PrettyName, (EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)) \
	bool TestClass##Runner::RunTest(const FString& Parameters) \
	{ \
		/* spawn test instance. Setup should be done in test's constructor */ \
		TestClass* TestInstance = new TestClass(); \
		TestInstance->SetTestInstance(*this); \
		/* set up */ \
		TestInstance->SetUp(); \
		/* call the instant-test code */ \
		TestInstance->InstantTest(); \
		/* tear down */ \
		TestInstance->TearDown(); \
		return true; \
	} 

//----------------------------------------------------------------------//
// Specific test types
//----------------------------------------------------------------------//

struct FAITest_SimpleBT : public FAITestBase
{
	TArray<int32> ExpectedResult;
	UBehaviorTree* BTAsset;
	UMockAI_BT* AIBTUser;
	bool bUseSystemTicking;

	FAITest_SimpleBT();	
	
	virtual void SetUp() override;
	virtual bool Update() override;
	
	virtual void VerifyResults();
};

template<class TComponent>
struct FAITest_SimpleComponentBasedTest : public FAITestBase
{
	FTestLogger<int32> Logger;
	TComponent* Component;

	FAITest_SimpleComponentBasedTest()
	{
		Component = NewAutoDestroyObject<TComponent>();
	}

	virtual void SetTestInstance(FAutomationTestBase& AutomationTestInstance) override
	{ 
		FAITestBase::SetTestInstance(AutomationTestInstance);
		Logger.TestRunner = TestRunner;
	}

	virtual ~FAITest_SimpleComponentBasedTest()
	{
		Test(TEXT("Not all expected values has been logged"), Logger.ExpectedValues.Num() == 0 || Logger.ExpectedValues.Num() == Logger.LoggedValues.Num());
	}

	virtual void SetUp() override
	{
		UWorld* World = FAITestHelpers::GetWorld();
		Component->RegisterComponentWithWorld(World);
	}

	void TickComponent()
	{
		Component->TickComponent(FAITestHelpers::TickInterval, ELevelTick::LEVELTICK_All, nullptr);
	}

	//virtual bool Update() override;
};

typedef FAITest_SimpleComponentBasedTest<UPawnActionsComponent> FAITest_SimpleActionsTest;
