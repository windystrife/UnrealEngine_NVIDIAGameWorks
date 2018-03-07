// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "Templates/Casts.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "UpdateManager.generated.h"

class Error;
class UGameInstance;
enum class EHotfixResult : uint8;

/**
 * Various states the update manager flows through as it checks for patches/hotfixes
 */
UENUM(BlueprintType)
enum class EUpdateState : uint8
{
	/** No updates in progress */
	UpdateIdle,
	/** An update is waiting to be triggered at the right time */
	UpdatePending,
	/** Checking for an available patch */
	CheckingForPatch,
	/** Detect the console environment via an auth request */
	DetectingPlatformEnvironment,
	/** Checking with hotfix for available updates */
	CheckingForHotfix,
	/** Waiting for the async loading / preloading to complete */
	WaitingOnInitialLoad,
	/** Preloading complete */
	InitialLoadComplete,
	/** Last update check completed successfully */
	UpdateComplete
};

/**
 * Possible outcomes at the end of an update check
 */
UENUM(BlueprintType)
enum class EUpdateCompletionStatus : uint8
{
	/** Unknown update completion */
	UpdateUnknown,
	/** Update completed successfully, some changes applied */
	UpdateSuccess,
	/** Update completed successfully, no changed needed */
	UpdateSuccess_NoChange,
	/** Update completed successfully, need to reload the map */
	UpdateSuccess_NeedsReload,
	/** Update completed successfully, need to relaunch the game */
	UpdateSuccess_NeedsRelaunch,
	/** Update completed successfully, a patch must be download to continue */
	UpdateSuccess_NeedsPatch,
	/** Update failed in the patch check */
	UpdateFailure_PatchCheck,
	/** Update failed in the hotfix check */
	UpdateFailure_HotfixCheck,
	/** Update failed due to not being logged in */
	UpdateFailure_NotLoggedIn
};

/**
 * Possible outcomes at the end of just the patch check
 */
UENUM(BlueprintType)
enum class EPatchCheckResult : uint8
{
	/** No patch required */
	NoPatchRequired,
	/** Patch required to continue */
	PatchRequired,
	/** Logged in user required for a patch check */
	NoLoggedInUser,
	/** Patch check failed */
	PatchCheckFailure,
};

/**
 * Delegate fired when changes to the update progress have been made
 *
 * @param NewState newest state change while an update check is in progress
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUpdateStatusChanged, EUpdateState /** NewState */)
typedef FOnUpdateStatusChanged::FDelegate FOnUpdateStatusChangedDelegate;

/**
 * Delegate fired when a single update check has completed
 *
 * @param Result result of the update check operation 
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUpdateCheckComplete, EUpdateCompletionStatus /** Result */)
typedef FOnUpdateCheckComplete::FDelegate FOnUpdateCheckCompleteDelegate;

/**
 * Delegate fired when progress on a hotfix download is made
 *
 * @param NumDownloaded the number of files downloaded so far
 * @param TotalFiles the total number of files part of the hotfix
 * @param NumBytes the number of bytes processed so far
 * @param TotalBytes the total size of the hotfix data
 */
DECLARE_MULTICAST_DELEGATE_FourParams(FOnUpdateHotfixProgress, uint32 /** NumDownloaded */, uint32 /** TotalFiles */, uint64 /** NumBytes */, uint64 /** TotalBytes */)
typedef FOnUpdateHotfixProgress::FDelegate FOnUpdateHotfixProgressDelegate;

/**
 * Delegate fired when a single file hotfix is applied
 *
 * @param FriendlyName the human readable version of the file name (DefaultEngine.ini)
 * @param CachedFileName the full path to the file on disk
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUpdateHotfixProcessedFile, const FString& /** FriendlyName */, const FString& /** CachedFileName */);
typedef FOnUpdateHotfixProcessedFile::FDelegate FOnUpdateHotfixProcessedFileDelegate;

/**
 * Update manager
 *
 * Checks the system and/or backend for the possibility of a patch and hotfix
 * Will not apply a hotfix if a pending patch is available
 * Notifies the game of the result of the check
 * - possibly requires UI to prevent user from playing if a patch is available
 * - possibly requires UI to prevent user from player if a hotfix requires a reload of existing data
 */ 
UCLASS(Config=Engine)
class HOTFIX_API UUpdateManager : public UObject
{
	GENERATED_BODY()

private:

	/** Status update listeners */
	FOnUpdateStatusChanged UpdateStatusChangedDelegates;
	/** Check completion listeners */
	FOnUpdateCheckComplete UpdateCheckCompleteDelegates;
	/** Hotfix download progress listeners */
	FOnUpdateHotfixProgress UpdateHotfixProgressDelegate;
	/** Hotfix application process listeners */
	FOnUpdateHotfixProcessedFile UpdateHotfixProcessedFile;

public:

	UUpdateManager();

	/**
	 * Reset so you can call StartCheck again
	 */
	virtual void Reset();

	/**
	 * Start an patch and hotfix check
	 *
	 * @param bInCheckHotfixOnly check for the availability of a hotfix only (does not apply)
	 */
	virtual void StartCheck(bool bInCheckHotfixOnly = false);
	
	/**
	 *  @return the load progress (0..1)
	 */
	float GetLoadProgress() const;

	/**
	 * @return true if hotfixing is enabled
	 */
	bool IsHotfixingEnabled() const;

	/**
	 * @return true if blocking for initial load is enabled
	 */
	bool IsBlockingForInitialLoadEnabled() const;

	/**
	 * Put the update manager in a pending state so it can alert the game that a check is imminent
	 */
	void SetPending();

	/**
	 * @return true if at least one update been completed
	 */
	bool HasCompletedInitialUpdate() const { return bInitialUpdateFinished; }

	/**
	 * @return true if the update manager is actively checking or about to
	 */
	bool IsUpdating() const { return !bCheckHotfixAvailabilityOnly && (CurrentUpdateState != EUpdateState::UpdateIdle) && (CurrentUpdateState != EUpdateState::UpdateComplete); }

	/**
	* @return true if the update manager is actively checking
	*/
	bool IsActivelyUpdating() const { return IsUpdating() && (CurrentUpdateState != EUpdateState::UpdatePending); }

	/**
	 * @return the current state of the update check process
	 */
	EUpdateState GetUpdateState() const { return CurrentUpdateState; }

	/**
	 * @return the last result of the update manager
	 */
	EUpdateCompletionStatus GetCompletionResult() const { return LastCompletionResult[0]; }

	/**
	 * @return delegate triggered when update status has changed 
	 */
	FOnUpdateStatusChanged& OnUpdateStatusChanged() { return UpdateStatusChangedDelegates; }

	/**
	 * @return delegate triggered when update check is complete
	 */
	FOnUpdateCheckComplete& OnUpdateCheckComplete() { return UpdateCheckCompleteDelegates; }

	/**
	 * @return delegate triggered when hotfix file download progress has been made
	 */
	FOnUpdateHotfixProgress& OnUpdateHotfixProgress() { return UpdateHotfixProgressDelegate; }

	/**
	 * @return delegate triggered when hotfix file is applied
	 */
	FOnUpdateHotfixProcessedFile& OnUpdateHotfixProcessedFile() { return UpdateHotfixProcessedFile; }

protected:

	enum class EUpdateStartResult : uint8
	{
		/** Update did not start */
		None,
		/** Update has started */
		UpdateStarted,
		/** Cached result is going to be returned */
		UpdateCached
	};

	/** @return true if update checks are enabled */
	virtual bool ChecksEnabled() const;
	/** @return true if the backend environment requires update checks */
	virtual bool EnvironmentWantsPatchCheck() const;

	/** 
	 * Internal call for StartCheck
	 *
	 * @return whether or not the check started, returned a cached value, or did nothing (already in progress)
	 */
	virtual EUpdateStartResult StartCheckInternal(bool bInCheckHotfixOnly);

	/** Tick function during initial preload */
	virtual bool Tick(float DeltaTime);

	/** Amount of time to wait between the internal hotfix check completing and advancing to the next stage */
	UPROPERTY(Config)
	float HotfixCheckCompleteDelay;
	/** Amount of time to wait at the end of the entire check before notifying listening entities */
	UPROPERTY(Config)
	float UpdateCheckCompleteDelay;
	/** Amount of time to wait between the internal hotfix availability check completing and advancing to the next stage */
	UPROPERTY(Config)
	float HotfixAvailabilityCheckCompleteDelay;
	/** Amount of time to wait at the end of the entire check before notifying listening entities (availability check only) */
	UPROPERTY(Config)
	float UpdateCheckAvailabilityCompleteDelay;

	/**
	 * Patch check
	 */
	virtual void StartPatchCheck();
	virtual void OnCheckForPatchComplete(const FUniqueNetId& UniqueId, EUserPrivileges::Type Privilege, uint32 PrivilegeResult, bool bConsoleCheck);
	virtual void PatchCheckComplete(EPatchCheckResult PatchResult);

	/**
	 * Hotfix check
	 */
	virtual void StartHotfixCheck();
	virtual void OnHotfixProgress(uint32 NumDownloaded, uint32 TotalFiles, uint64 NumBytes, uint64 TotalBytes);
	virtual void OnHotfixProcessedFile(const FString& FriendlyName, const FString& CachedName);
	virtual void OnHotfixCheckComplete(EHotfixResult Result);

	/** Check for the availability of changed hotfix files only */
	virtual void StartHotfixAvailabilityCheck();
	/** Availability check complete */
	virtual void HotfixAvailabilityCheckComplete(EHotfixResult Result);

	/**
	 * Query for platform environment before continuing with hotfixing
	 * This configures the Mcp backend correctly
	 */
	virtual void StartPlatformEnvironmentCheck();
	/**
	 * Callback for loging in on console which is needed for the environment check
	 */
	virtual void PlatformEnvironmentCheck_OnLoginConsoleComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

	/**
	 * Preload game assets after patch/hotfix check is complete but before game is alerted
	 */
	virtual void StartInitialPreload();

	/**
	 * Initial preload of assets is complete
	 */
	virtual void InitialPreloadComplete();

	/** 
	 * Announce that the update check has completed
	 *
	 * @param Result result of the entire update check (patch/hotfix/etc)
	 * @param bUpdateTimestamp whether or not to update the cache timer
	 */
	virtual void CheckComplete(EUpdateCompletionStatus Result, bool bUpdateTimestamp = true);

	/** Change the state of the update manager */
	virtual void SetUpdateState(EUpdateState NewState);

protected:

	/** true if we've already detected the backend environment */
	UPROPERTY()
	bool bPlatformEnvironmentDetected;

	/** Has the first update completed */
	UPROPERTY()
	bool bInitialUpdateFinished;

	/** Is this run only checking and not applying */
	UPROPERTY()
	bool bCheckHotfixAvailabilityOnly;

	/** Current state of the update */
	UPROPERTY()
	EUpdateState CurrentUpdateState;

	/** What was the maximum number of pending async loads we've seen so far */
	UPROPERTY()
	int32 WorstNumFilesPendingLoadViewed;

	/** Result of the last patch check */
	UPROPERTY()
	EPatchCheckResult LastPatchCheckResult;

	/** Result of the last hotfix */
	UPROPERTY()
	EHotfixResult LastHotfixResult;
	/** Delegates to hotfix updates */
	FDelegateHandle HotfixCompleteDelegateHandle;
	FDelegateHandle HotfixProgressDelegateHandle;
	FDelegateHandle HotfixProcessedFileDelegateHandle;
	FDelegateHandle OnLoginConsoleCompleteHandle;

	/** The time at which we started the initial load after updates completed */
	double LoadStartTime;

	/** Timestamp of last update check (0:normal, 1:availability only) */
	UPROPERTY()
	FDateTime LastUpdateCheck[2];
	/** Last update check result (0:normal, 1:availability only) */
	UPROPERTY()
	EUpdateCompletionStatus LastCompletionResult[2];

	FDelegateHandle TickerHandle;
	FTimerHandle StartCheckInternalTimerHandle;

private:

	/**
	 * Helpers
	 */

	/** String output */
	UPROPERTY()
	UEnum* UpdateStateEnum;
	UPROPERTY()
	UEnum* UpdateCompletionEnum;

	/** Fire a delegate after a given amount of time */
	typedef TFunction<void(void)> DelayCb;
	FTimerHandle DelayResponse(DelayCb&& Delegate, float Delay);

	/** Helper function to check if a timer is running. */
	bool IsTimerHandleActive(const FTimerHandle& TimerHandle) const;

	friend bool SkipPatchCheck(UUpdateManager* UpdateManager);

protected:

	/** @return a pointer to the hotfix manager */
	template< typename THotfixManager >
	THotfixManager* GetHotfixManager() const
	{
		return Cast<THotfixManager>(THotfixManager::Get(GetWorld()));
	}

	/** @return a pointer to the world */
	UWorld* GetWorld() const;

	/** @return a pointer to the game instance */
	UGameInstance* GetGameInstance() const;
};
