// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Templates/Function.h"
#include "Misc/AutomationTest.h"
#include "Async/Async.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncGraphTest, "System.Core.Async.Async (Task Graph)", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncThreadedTaskTest, "System.Core.Async.Async (Thread)", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncThreadedPoolTest, "System.Core.Async.Async (Thread Pool)", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncVoidTaskTest, "System.Core.Async.Async (Void)", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncCompletionCallbackTest, "System.Core.Async.Async (Completion Callback)", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)


/** Helper methods used in the test cases. */
namespace AsyncTestUtils
{
	TFunction<int()> Task = []() {
		return 123;
	};

	bool bHasVoidTaskFinished = false;

	TFunction<void()> VoidTask = []() {
		bHasVoidTaskFinished = true;
	};
}


/** Test that task graph tasks return correctly. */
bool FAsyncGraphTest::RunTest(const FString& Parameters)
{
	auto Future = Async(EAsyncExecution::TaskGraph, AsyncTestUtils::Task);
	int Result = Future.Get();

	TestEqual(TEXT("Task graph task must return expected value"), Result, 123);

	return true;
}


/** Test that threaded tasks return correctly. */
bool FAsyncThreadedTaskTest::RunTest(const FString& Parameters)
{
	auto Future = Async(EAsyncExecution::Thread, AsyncTestUtils::Task);
	int Result = Future.Get();

	TestEqual(TEXT("Threaded task must return expected value"), Result, 123);

	return true;
}


/** Test that threaded pool tasks return correctly. */
bool FAsyncThreadedPoolTest::RunTest(const FString& Parameters)
{
	auto Future = Async(EAsyncExecution::ThreadPool, AsyncTestUtils::Task);
	int Result = Future.Get();

	TestEqual(TEXT("Thread pool task must return expected value"), Result, 123);

	return true;
}


/** Test that void tasks run without errors or warnings. */
bool FAsyncVoidTaskTest::RunTest(const FString& Parameters)
{
	// Reset test variable before running
	AsyncTestUtils::bHasVoidTaskFinished = false;
	auto Future = Async(EAsyncExecution::TaskGraph, AsyncTestUtils::VoidTask);
	Future.Get();

	// Check that the variable state was updated by task
	TestTrue(TEXT("Void tasks should run"), AsyncTestUtils::bHasVoidTaskFinished);

	return true;
}


/** Test that asynchronous tasks have their completion callback called. */
bool FAsyncCompletionCallbackTest::RunTest(const FString& Parameters)
{
	bool Completed = false;
	auto Future = Async(EAsyncExecution::TaskGraph, AsyncTestUtils::Task, [&]() {
		Completed = true;
	});
	int Result = Future.Get();

	TestEqual(TEXT("Async Result"), Result, 123);
	TestTrue(TEXT("Completion callack to be called"), Completed);

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
