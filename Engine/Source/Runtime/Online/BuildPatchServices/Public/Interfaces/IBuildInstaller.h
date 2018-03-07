// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IBuildInstaller.h: Declares the IBuildInstaller interface.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "BuildPatchState.h"
#include "BuildPatchMessage.h"

class IBuildInstaller;

typedef TSharedPtr< class IBuildInstaller, ESPMode::ThreadSafe > IBuildInstallerPtr;
typedef TSharedRef< class IBuildInstaller, ESPMode::ThreadSafe > IBuildInstallerRef;

/**
 * Declares the error type enum for use with the error system
 */
enum class EBuildPatchInstallError
{
	// There has been no registered error
	NoError = 0,

	// A download request failed and ran out of allowed retries
	DownloadError = 1,

	// A file failed to construct properly
	FileConstructionFail = 2,

	// An error occurred trying to move the file to the install location
	MoveFileToInstall = 3,

	// The installed build failed to verify
	BuildVerifyFail = 4,

	// The user or some process has closed the application
	ApplicationClosing = 5,

	// An application error, such as module fail to load.
	ApplicationError = 6,

	// User canceled download
	UserCanceled = 7,

	// A prerequisites installer failed
	PrerequisiteError = 8,

	// An initialization error
	InitializationError = 9,

	// An error occurred creating a file due to excessive path length
	PathLengthExceeded = 10,

	// An error occurred creating a file due to their not being enough space left on the disk
	OutOfDiskSpace = 11,

	// Used to help verify logic
	NumInstallErrors
};

/**
 * Declares the error code prefixes for each error type enum
 */
namespace InstallErrorPrefixes
{
	static const TCHAR* ErrorTypeStrings[] =
	{
		TEXT("OK"), // NoError
		TEXT("DL"), // DownloadError
		TEXT("FC"), // FileConstructionFail
		TEXT("MF"), // MoveFileToInstall
		TEXT("BV"), // BuildVerifyFail
		TEXT("SD"), // ApplicationClosing
		TEXT("FA"), // ApplicationError
		TEXT("UC"), // UserCanceled
		TEXT("PQ"), // PrerequisiteError
		TEXT("IZ"), // InitializationError
		TEXT("PL"), // PathLengthExceeded
		TEXT("DS"), // OutOfDiskSpace
	};
};

// Enum describing download health. The actual percentage values used are configurable in the engine ini.
enum class EBuildPatchDownloadHealth
{
	// All requests are in the retrying state. No progress currently. Possibly disconnected.
	Disconnected = 0,
	// More than 10% of requests are failing.
	Poor,
	// 10% or fewer requests are failing.
	OK,
	// 1% or fewer requests are failing.
	Good,
	// No requests are failing.
	Excellent,

	// Must be last value, only used for value counts.
	NUM_Values
};

/**
 * A struct to hold stats for the build process.
 */
struct FBuildInstallStats
{
	// Constructor
	FBuildInstallStats()
		: AppName()
		, AppInstalledVersion()
		, AppPatchVersion()
		, CloudDirectory()
		, NumFilesInBuild(0)
		, NumFilesOutdated(0)
		, NumFilesToRemove(0)
		, NumChunksRequired(0)
		, ChunksQueuedForDownload(0)
		, ChunksLocallyAvailable(0)
		, ChunksInChunkDbs(0)
		, NumChunksDownloaded(0)
		, NumChunksRecycled(0)
		, NumChunksReadFromChunkDbs(0)
		, NumChunksCacheBooted(0)
		, NumDriveCacheChunkLoads(0)
		, NumFailedDownloads(0)
		, NumBadDownloads(0)
		, NumAbortedDownloads(0)
		, NumRecycleFailures(0)
		, NumDriveCacheLoadFailures(0)
		, NumChunkDbChunksFailed(0)
		, TotalDownloadedData(0)
		, AverageDownloadSpeed(0.0)
		, FinalDownloadSpeed(-1.0)
		, TheoreticalDownloadTime(0.0f)
		, InitializeTime(0.0f)
		, ConstructTime(0.0f)
		, MoveFromStageTime(0.0f)
		, FileAttributesTime(0.0f)
		, VerifyTime(0.0f)
		, CleanUpTime(0.0f)
		, PrereqTime(0.0f)
		, ProcessPausedTime(0.0f)
		, ProcessActiveTime(0.0f)
		, ProcessExecuteTime(0.0f)
		, ProcessSuccess(false)
		, NumInstallRetries(0)
		, FailureType(EBuildPatchInstallError::InitializationError)
		, RetryFailureTypes()
		, ErrorCode()
		, RetryErrorCodes()
		, FailureReasonText()
		, FinalProgress(0.0f)
		, OverallRequestSuccessRate(0.0f)
		, ExcellentDownloadHealthTime(0.0f)
		, GoodDownloadHealthTime(0.0f)
		, OkDownloadHealthTime(0.0f)
		, PoorDownloadHealthTime(0.0f)
		, DisconnectedDownloadHealthTime(0.0f)
	{}

	// The name of the app being installed.
	FString AppName;
	// The version string currently installed, or "NONE".
	FString AppInstalledVersion;
	// The version string patching to.
	FString AppPatchVersion;
	// The cloud directory used for this install.
	FString CloudDirectory;
	// The total number of files in the build.
	uint32 NumFilesInBuild;
	// The total number of files outdated.
	uint32 NumFilesOutdated;
	// The total number of files in the previous build that can be deleted.
	uint32 NumFilesToRemove;
	// The total number of chunks making up those files.
	uint32 NumChunksRequired;
	// The number of required chunks queued for download.
	uint32 ChunksQueuedForDownload;
	// The number of chunks locally available in the build.
	uint32 ChunksLocallyAvailable;
	// The number of chunks available in chunkdb files.
	uint32 ChunksInChunkDbs;
	// The total number of chunks that were downloaded.
	uint32 NumChunksDownloaded;
	// The number of chunks successfully recycled.
	uint32 NumChunksRecycled;
	// The number of chunks successfully read from chunkdbs.
	uint32 NumChunksReadFromChunkDbs;
	// The number of chunks that had to be booted from the cache.
	uint32 NumChunksCacheBooted;
	// The number of chunks that had to be loaded from the drive cache.
	uint32 NumDriveCacheChunkLoads;
	// The number of chunks we did not successfully receive.
	uint32 NumFailedDownloads;
	// The number of chunks we received but were determined bad data.
	uint32 NumBadDownloads;
	// The number of chunks we aborted as they were determined as taking too long.
	uint32 NumAbortedDownloads;
	// The number of chunks that failed to be recycled from existing build.
	uint32 NumRecycleFailures;
	// The number of chunks that failed to load from the drive cache.
	uint32 NumDriveCacheLoadFailures;
	// The number of chunks that were not successfully loaded from provided chunkdbs.
	uint32 NumChunkDbChunksFailed;
	// The total number of bytes downloaded.
	int64 TotalDownloadedData;
	// The average chunk download speed.
	double AverageDownloadSpeed;
	// The download speed registered at the end of the installation.
	double FinalDownloadSpeed;
	// The theoretical download time (data/speed).
	float TheoreticalDownloadTime;
	// The time spent during the initialization stage.
	float InitializeTime;
	// The time spent during the construction stage.
	float ConstructTime;
	// The time spent moving staged files into the installation location.
	float MoveFromStageTime;
	// The time spent during the file attribution stage.
	float FileAttributesTime;
	// The time spent during the verification stage.
	float VerifyTime;
	// The time spent during the clean up stage.
	float CleanUpTime;
	// The time spent during the prerequisite stage.
	float PrereqTime;
	// The amount of time that was spent paused.
	float ProcessPausedTime;
	// The amount of time that was spent active (un-paused).
	float ProcessActiveTime;
	// The total time that the install process took to complete.
	float ProcessExecuteTime;
	// Whether the process was successful.
	bool ProcessSuccess;
	// The number of times the system looped to retry.
	uint32 NumInstallRetries;
	// The failure type for the install.
	EBuildPatchInstallError FailureType;
	// If NumInstallRetries > 0, this will contain the list of retry reasons for retrying.
	TArray<EBuildPatchInstallError> RetryFailureTypes;
	// The error code. No error results in 'OK'.
	FString ErrorCode;
	// If NumInstallRetries > 0, this will contain the list of error codes for each retry.
	TArray<FString> RetryErrorCodes;
	// The localized, more generic failure reason.
	FText FailureReasonText;
	// Final progress state, this is the progress of the current retry attempt.
	float FinalProgress;
	// The overall rate of success for download requests.
	float OverallRequestSuccessRate;
	// The amount of time that was spent with Excellent download health.
	float ExcellentDownloadHealthTime;
	// The amount of time that was spent with Good download health.
	float GoodDownloadHealthTime;
	// The amount of time that was spent with OK download health.
	float OkDownloadHealthTime;
	// The amount of time that was spent with Poor download health.
	float PoorDownloadHealthTime;
	// The amount of time that was spent with Disconnected download health.
	float DisconnectedDownloadHealthTime;
};

/**
 * Interface to a Build Manifest.
 */
class IBuildInstaller
{
public:
	/**
	 * Virtual destructor.
	 */
	virtual ~IBuildInstaller() { }

	/**
	 * Get whether the install has complete
	 * @return	true if the thread completed
	 */
	virtual bool IsComplete() const = 0;

	/**
	 * Get whether the install was canceled. Only valid if complete.
	 * @return	true if installation was canceled
	 */
	virtual bool IsCanceled() const = 0;

	/**
	 * Get whether the install is currently paused.
	 * @return	true if installation is paused
	 */
	virtual bool IsPaused() const = 0;

	/**
	 * Get whether the install can be resumed.
	 * @return	true if installation is resumable
	 */
	virtual bool IsResumable() const = 0;

	/**
	 * Get whether the install failed. Only valid if complete.
	 * @return	true if installation was a failure
	 */
	virtual bool HasError() const = 0;

	/**
	 * Get the type of error for a failure that has occurred.
	 * @return	the enum representing the type of error
	 */
	virtual EBuildPatchInstallError GetErrorType() const = 0;

	/**
	 * This is deprecated and shouldn't be used anymore [6/4/2014 justin.sargent]
	 *
	 * Get the percentage complete text for the current process
	 * @return	percentage complete text progress
	 */
	virtual FText GetPercentageText() const = 0;

	/**
	 * This is deprecated and shouldn't be used anymore [6/4/2014 justin.sargent]
	 *
	 * Get the download speed text for the current process
	 * @return	download speed text progress
	 */
	virtual FText GetDownloadSpeedText() const = 0;

	/**
	 * Get the download speed for the current process
	 * @return	download speed progress
	 */
	virtual double GetDownloadSpeed() const = 0;

	/**
	 * Get the initial download size
	 * @return	the initial download size
	 */
	virtual int64 GetInitialDownloadSize() const = 0;

	/**
	 * Get the total currently downloaded
	 * @return	the total currently downloaded
	 */
	virtual int64 GetTotalDownloaded() const = 0;

	/**
	 * Get the status of the install process.
	 * @return Status of the install process.
	 */
	virtual BuildPatchServices::EBuildPatchState GetState() const = 0;

	/**
	 * This is deprecated and redundant [2016/06/12 leigh.swift]
	 * Instead, you can BuildPatchServices::StateToText(Installer->GetState()).
	 *
	 * Get the text for status of the install process.
	 * @return Status of the install process.
	 */
	virtual FText GetStatusText() const = 0;

	/**
	 * Get the update progress
	 * @return	A float describing progress: Between 0 and 1 for known progress, or less than 0 for unknown progress.
	 */
	virtual float GetUpdateProgress() const = 0;

	/**
	 * Get the build stats for the process. This should only be called after the install has completed
	 * @return	A struct containing information about the build
	 */
	virtual FBuildInstallStats GetBuildStatistics() const = 0;

	/**
	 * Get the current download health rating.
	 * @return	the enum representing the download health
	 */
	virtual EBuildPatchDownloadHealth GetDownloadHealth() const = 0;

	/**
	 * Get the display text for the error that occurred. Only valid to call after completion
	 * @return	display error text
	 */
	virtual FText GetErrorText() const = 0;

	/**
	 * Get the installation error code. This includes the failure type as well as specific code associated. The value is alphanumeric.
	 * This is only guaranteed to be set once the installation has completed.
	 * @returns the code as a string.
	 */
	virtual FString GetErrorCode() const = 0;

	/**
	 * Cancel the current install
	 */
	virtual void CancelInstall() = 0;

	/**
	 * Toggle the install paused state
	 * @return true if the installer is now paused
	 */
	virtual bool TogglePauseInstall() = 0;

	/**
	 * Registers a message handler with the installer.
	 * @param MessageHandler    Ptr to the message handler to add. Must not be null.
	 */
	virtual void RegisterMessageHandler(BuildPatchServices::FMessageHandler* MessageHandler) = 0;

	/**
	 * Unregisters a message handler, will no longer receive HandleMessage calls.
	 * @param MessageHandler    Ptr to the message handler to remove.
	 */
	virtual void UnregisterMessageHandler(BuildPatchServices::FMessageHandler* MessageHandler) = 0;
};
