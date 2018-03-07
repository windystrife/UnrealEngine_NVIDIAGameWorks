// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "AutomationScreenshotOptions.h"
#include "LatentActions.h"

struct FLatentActionInfo;

class FTakeScreenshotAfterTimeLatentAction : public FPendingLatentAction
{
public:
	FTakeScreenshotAfterTimeLatentAction(const FLatentActionInfo& LatentInfo, const FString& InScreenshotName, FAutomationScreenshotOptions InOptions);
	virtual ~FTakeScreenshotAfterTimeLatentAction();

	virtual void UpdateOperation(FLatentResponse& Response) override;

#if WITH_EDITOR
	// Returns a human readable description of the latent operation's current state
	virtual FString GetDescription() const override;
#endif

private:
	void OnScreenshotTakenAndCompared();

private:
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;
	FString ScreenshotName;
	float SecondsRemaining;
	bool IssuedScreenshotCapture;
	bool TakenScreenshot;
	FAutomationScreenshotOptions Options;
};

class FWaitForScreenshotComparisonLatentAction : public FPendingLatentAction
{
public:
	FWaitForScreenshotComparisonLatentAction(const FLatentActionInfo& LatentInfo);
	virtual ~FWaitForScreenshotComparisonLatentAction();

	virtual void UpdateOperation(FLatentResponse& Response) override;

#if WITH_EDITOR
	// Returns a human readable description of the latent operation's current state
	virtual FString GetDescription() const override;
#endif

private:
	void OnScreenshotTakenAndCompared();

private:
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;
	bool TakenScreenshot;
};
