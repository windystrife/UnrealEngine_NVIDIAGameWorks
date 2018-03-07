// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "Async.h"

#if WITH_XGE_CONTROLLER

struct FXGETaskResult
{
	int32 ReturnCode;
	bool bCompleted;
};

class IXGEController : public IModuleInterface
{
public:
	virtual bool SupportsDynamicReloading() override { return false; }

	// Returns true if the XGE controller may be used.
	virtual bool IsSupported() abstract;

	// Returns a new file path to be used for writing input data to.
	virtual FString CreateUniqueFilePath() abstract;

	// Launches a task within XGE. Returns a future which can be waited on for the results.
	virtual TFuture<FXGETaskResult> EnqueueTask(const FString& Command, const FString& CommandArgs) abstract;

	static XGECONTROLLER_API IXGEController& Get();
};

#endif // WITH_XGE_CONTROLLER
