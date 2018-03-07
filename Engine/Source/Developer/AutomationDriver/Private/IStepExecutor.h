// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AsyncResult.h"
#include "Misc/Timespan.h"

struct FStepResult
{
public:

	enum class EState : uint8
	{
		DONE,
		FAILED,
		REPEAT,
	};

	FStepResult(EState InState, FTimespan InNextWait)
		: NextWait(MoveTemp(InNextWait))
		, State(InState)
	{ }

	// How long the executor should wait before either executing this same step, the next step or before declaring all steps complete
	FTimespan NextWait;

	// Whether the step that just completed is completely finished or should be rescheduled again for execution
	EState State;
};

class IStepExecutor
{
public:

	DECLARE_DELEGATE_RetVal_OneParam(FStepResult, FExecuteStepDelegate, const FTimespan& /*TotalProcessTime*/);
	virtual void Add(const FExecuteStepDelegate& Step) = 0;

	virtual void Add(const TFunction<FStepResult(const FTimespan&)>& Step) = 0;

	virtual void InsertNext(const FExecuteStepDelegate& Step) = 0;

	virtual void InsertNext(const TFunction<FStepResult(const FTimespan&)>& Step) = 0;

	virtual TAsyncResult<bool> Execute() = 0;

	virtual bool IsExecuting() const = 0;
};
