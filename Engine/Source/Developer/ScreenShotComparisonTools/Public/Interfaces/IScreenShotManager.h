// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IScreenShotComparisonModule.h: Declares the IScreenShotComparisonModule interface.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "ImageComparer.h"

class IScreenShotManager;

/**
 * Type definition for shared pointers to instances of IScreenShotManager.
 */
typedef TSharedPtr<class IScreenShotManager> IScreenShotManagerPtr;

/**
 * Type definition for shared references to instances of IScreenShotManager.
 */
typedef TSharedRef<class IScreenShotManager> IScreenShotManagerRef;

struct FScreenshotExportResults
{
	bool Success;
	FString ExportPath;

	FScreenshotExportResults()
		: Success(false)
	{
	}
};

/**
 * Interface for screen manager module.
 */
class IScreenShotManager
{
public:
	virtual ~IScreenShotManager(){ }

	/**
	 * Compares a specific screenshot, the shot path must be relative from the incoming unapproved directory.
	 */
	virtual TFuture<FImageComparisonResult> CompareScreensotAsync(FString RelativeImagePath) = 0;

	/**
	 * Exports the screenshots to the export location specified
	 */
	virtual TFuture<FScreenshotExportResults> ExportComparisonResultsAsync(FString ExportPath = TEXT("")) = 0;

	/**
	 * Imports screenshot comparison data from a given path.
	 */
	virtual bool OpenComparisonReports(FString ImportPath, TArray<FComparisonReport>& OutReports) = 0;

	/**
	 * 
	 */
	virtual FString GetLocalUnapprovedFolder() const = 0;

	/**
	 * 
	 */
	virtual FString GetLocalApprovedFolder() const = 0;

	/**
	 * 
	 */
	virtual FString GetLocalComparisonFolder() const = 0;
};
