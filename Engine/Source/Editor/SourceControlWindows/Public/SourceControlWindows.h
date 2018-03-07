// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "ISourceControlProvider.h"

class SOURCECONTROLWINDOWS_API FSourceControlWindows
{
public:
	/** Opens a dialog to choose packages to submit */
	static void ChoosePackagesToCheckIn();

	/** Determines whether we can choose packages to check in (we cant if an operation is already in progress) */
	static bool CanChoosePackagesToCheckIn();

	/**
	 * Display check in dialog for the specified packages
	 *
	 * @param	bUseSourceControlStateCache		Whether to use the cached source control status, or force the status to be updated
	 * @param	InPackageNames					Names of packages to check in
	 * @param	InPendingDeletePaths			Directories to check for files marked 'pending delete'
	 * @param	InConfigFiles					Config filenames to check in
	 */
	static bool PromptForCheckin(bool bUseSourceControlStateCache, const TArray<FString>& InPackageNames, const TArray<FString>& InPendingDeletePaths = TArray<FString>(), const TArray<FString>& InConfigFiles = TArray<FString>());

	/**
	 * Display file revision history for the provided packages
	 *
	 * @param	InPackageNames	Names of packages to display file revision history for
	 */
	static void DisplayRevisionHistory( const TArray<FString>& InPackagesNames );

	/**
	 * Prompt the user with a revert files dialog, allowing them to specify which packages, if any, should be reverted.
	 *
	 * @param	InPackageNames	Names of the packages to consider for reverting
	 *
	 * @return	true if the files were reverted; false if the user canceled out of the dialog
	 */
	static bool PromptForRevert(const TArray<FString>& InPackageNames );

protected:
	/** Callback for ChoosePackagesToCheckIn(), continues to bring up UI once source control operations are complete */
	static void ChoosePackagesToCheckInCallback(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);

	/** Called when the process has completed and we have packages to check in */
	static void ChoosePackagesToCheckInCompleted(const TArray<UPackage*>& LoadedPackages, const TArray<FString>& PackageNames, const TArray<FString>& ConfigFiles);

	/** Delegate called when the user has decided to cancel the check in process */
	static void ChoosePackagesToCheckInCancelled(FSourceControlOperationRef InOperation);

private:
	/** The notification in place while we choose packages to check in */
	static TWeakPtr<class SNotificationItem> ChoosePackagesToCheckInNotification;
};

