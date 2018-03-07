// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/Timespan.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformProcess.h"
#include "Stats/Stats.h"
#include "Misc/AutomationTest.h"
#include "Async/TaskGraphInterfaces.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTaskGraphTest, "System.Core.Async.TaskGraph", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)


namespace TaskGraphTestTask
{
	const FTimespan MaxWaitTime(0, 0, 5);
	const int32 NumTasks = 10000;
	int32 CompletedTasks = 0;
}


/**
 * Implements a test task.
 */
class FTaskGraphTestTask
{
public:

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FPlatformAtomics::InterlockedIncrement(&TaskGraphTestTask::CompletedTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyThread;
	}

	TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FTaskGraphTestTask, STATGROUP_TaskGraphTasks);
	}

	static ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::FireAndForget; 
	}
};


bool FTaskGraphTest::RunTest(const FString& Parameters)
{
	using namespace TaskGraphTestTask;

	CompletedTasks = 0;

	for (int32 TaskIndex = 0; TaskIndex < TaskGraphTestTask::NumTasks; ++TaskIndex)
	{
		TGraphTask<FTaskGraphTestTask>::CreateTask().ConstructAndDispatchWhenReady();
	}

	FDateTime StartTime = FDateTime::UtcNow();

	while ((CompletedTasks < NumTasks) &&((FDateTime::UtcNow() - StartTime) < MaxWaitTime))
	{
		FPlatformProcess::Sleep(0.0f);
	}

	TestEqual(TEXT("The number of completed tasks must equal the total number of tasks"), CompletedTasks, NumTasks);

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
