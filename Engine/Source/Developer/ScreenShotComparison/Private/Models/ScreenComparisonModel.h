// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ImageComparer.h"
#include "AutomationWorkerMessages.h"
#include "Misc/FileHelper.h"
#include "JsonObjectConverter.h"
#include "AutomationWorkerMessages.h"
#include "Interfaces/IScreenShotManager.h"

class FScreenComparisonModel
{
public:
	FScreenComparisonModel(const FComparisonReport& InReport);

	DECLARE_MULTICAST_DELEGATE(FOnComplete);
	FOnComplete OnComplete;

	FComparisonReport Report;

	bool IsComplete() const;

	void Complete();

	bool AddNew(IScreenShotManagerPtr ScreenshotManager);
	bool Replace(IScreenShotManagerPtr ScreenshotManager);
	bool AddAlternative(IScreenShotManagerPtr ScreenshotManager);

	TOptional<FAutomationScreenshotMetadata> GetMetadata();

private:
	bool RemoveExistingApproved(IScreenShotManagerPtr ScreenshotManager);

private:
	bool bComplete;

	TOptional<FAutomationScreenshotMetadata> Metadata;

	struct FFileMapping
	{
		FFileMapping(const FString& InSourceFile, const FString& InDestinationFile)
			: SourceFile(InSourceFile)
			, DestinationFile(InDestinationFile)
		{
		}

		FString SourceFile;
		FString DestinationFile;
	};

	// 
	TArray<FFileMapping> FileImports;
};
