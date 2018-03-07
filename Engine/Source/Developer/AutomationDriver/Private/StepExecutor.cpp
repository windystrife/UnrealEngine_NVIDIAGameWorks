// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StepExecutor.h"
#include "IStepExecutor.h"
#include "IAutomationDriver.h"

#include "DriverConfiguration.h"

#include "ScopeLock.h"
#include "Ticker.h"
#include "Async.h"


class FStepExecutor
	: public IStepExecutor
	, public TSharedFromThis<FStepExecutor, ESPMode::ThreadSafe>
{
public:

	virtual ~FStepExecutor()
	{
		if (Promise.IsValid())
		{
			Promise->SetValue(false);
		}
	}

	virtual void Add(const FExecuteStepDelegate& Step) override
	{
		check(!Promise.IsValid());
		FScopeLock StateLock(&StepsCS);
		Steps.Add(Step);
	}

	virtual void Add(const TFunction<FStepResult(const FTimespan&)>& Step) override
	{
		check(!Promise.IsValid());
		FScopeLock StateLock(&StepsCS);
		Steps.Add(FExecuteStepDelegate::CreateLambda(Step));
	}

	virtual void InsertNext(const FExecuteStepDelegate& Step) override
	{
		check(Promise.IsValid());
		FScopeLock StateLock(&StepsCS);
		Steps.Insert(Step, CurrentStepIndex + 1);
	}

	virtual void InsertNext(const TFunction<FStepResult(const FTimespan&)>& Step) override
	{
		check(Promise.IsValid());
		FScopeLock StateLock(&StepsCS);
		Steps.Insert(FExecuteStepDelegate::CreateLambda(Step), CurrentStepIndex + 1);
	}

	virtual TAsyncResult<bool> Execute() override
	{
		check(!Promise.IsValid());
		CurrentStepIndex = 0;

		// Queue result back on main thread
		TWeakPtr<FStepExecutor, ESPMode::ThreadSafe> LocalWeakThis(SharedThis(this));
		AsyncTask(
			ENamedThreads::GameThread,
			[LocalWeakThis]()
			{
				TSharedPtr<FStepExecutor, ESPMode::ThreadSafe> Executor = LocalWeakThis.Pin();

				if (Executor.IsValid())
				{
					const int32 StepIndex = 0;
					FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateThreadSafeSP(Executor.ToSharedRef(), &FStepExecutor::ExecuteStep, StepIndex), 0);
				}
			}
		);

		Promise = MakeShareable(new TPromise<bool>());
		return TAsyncResult<bool>(Promise->GetFuture(), nullptr, nullptr);
	}

	virtual bool IsExecuting() const override
	{
		return Promise.IsValid();
	}

private:

	FStepExecutor(
		const TSharedRef<FDriverConfiguration, ESPMode::ThreadSafe>& InConfiguration)
		: Configuration(InConfiguration)
		, Steps()
		, CurrentStepIndex(0)
		, Promise()
		, StepTotalProcessTime(FTimespan::Zero())
		, LastDelay(0)
	{ }

	bool ExecuteStep(float Delta, int32 StepIndex)
	{
		check(IsInGameThread());

		FStepResult Result = FStepResult(FStepResult::EState::FAILED, 0);
		{
			FScopeLock StateLock(&StepsCS);

			// If we've encountered an invalid step that's greater then zero then we were just waiting
			// a little bit after the last step completed before signaling completion.
			if (StepIndex > 0 && !Steps.IsValidIndex(StepIndex))
			{
				Promise->SetValue(true);
				Promise.Reset();
				StepTotalProcessTime = FTimespan::Zero();
				return false;
			}

			check(Steps.IsValidIndex(StepIndex));

			Result = Steps[StepIndex].Execute(StepTotalProcessTime);
		}

		if (Result.State == FStepResult::EState::FAILED)
		{
			Promise->SetValue(false);
			Promise.Reset();
			StepTotalProcessTime = FTimespan::Zero();
			return false;
		}

		if (Result.State == FStepResult::EState::DONE)
		{
			StepTotalProcessTime = FTimespan::Zero();
			++StepIndex;
		}

		CurrentStepIndex = StepIndex;
		float Milliseconds = (float)(Result.NextWait.GetTicks()) / ETimespan::TicksPerMillisecond;
		float Delay = FMath::Max(SMALL_NUMBER, (Milliseconds / 1000) * Configuration->ExecutionSpeedMultiplier);
		
		if (LastDelay < KINDA_SMALL_NUMBER)
		{
			StepTotalProcessTime += FTimespan::FromSeconds(Delta);
		}

		StepTotalProcessTime += FTimespan::FromSeconds(Delay);
		LastDelay = Delay;
		FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateThreadSafeSP(this, &FStepExecutor::ExecuteStep, StepIndex), Delay);

		return false;
	}

private:

	const TSharedRef<FDriverConfiguration, ESPMode::ThreadSafe> Configuration;

	TArray<FExecuteStepDelegate> Steps;
	int32 CurrentStepIndex;
	TSharedPtr<TPromise<bool>> Promise;
	FTimespan StepTotalProcessTime;
	float LastDelay;

	mutable FCriticalSection StepsCS;

	friend FStepExecutorFactory;
};

TSharedRef<IStepExecutor, ESPMode::ThreadSafe> FStepExecutorFactory::Create(
	const TSharedRef<FDriverConfiguration, ESPMode::ThreadSafe>& Configuration)
{
	return MakeShareable(new FStepExecutor(Configuration));
}
