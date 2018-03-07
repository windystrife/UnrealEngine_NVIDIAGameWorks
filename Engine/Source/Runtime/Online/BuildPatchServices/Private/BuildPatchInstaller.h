// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchInstaller.h: Declares the FBuildPatchInstaller class which
	controls the process of installing a build described by a build manifest.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "BuildPatchManifest.h"
#include "Interfaces/IBuildInstaller.h"
#include "Interfaces/IBuildPatchServicesModule.h"
#include "HAL/ThreadSafeBool.h"
#include "BuildPatchProgress.h"
#include "Core/Platform.h"
#include "Core/ProcessTimer.h"
#include "Common/HttpManager.h"

namespace BuildPatchServices
{
	struct FChunkDbSourceConfig;
	struct FCloudSourceConfig;
	struct FInstallSourceConfig;
	class IInstallerError;
	class IInstallerStatistics;
	class IInstallerAnalytics;
	class IMachineConfig;
	class IDownloadService;
	class IMessagePump;

	/**
	 * FBuildPatchInstaller
	 * This class controls a thread that wraps the code to install/patch an app from manifests.
	 */
	class FBuildPatchInstaller
		: public IBuildInstaller
		, public FRunnable
	{
	private:
		// Hold a pointer to my thread for easier deleting.
		FRunnableThread* Thread;

		// The delegates that we will be calling.
		FBuildPatchBoolManifestDelegate OnCompleteDelegate;

		// The installer configuration.
		FInstallerConfiguration Configuration;

		// The manifest for the build we have installed (if applicable).
		FBuildPatchAppManifestPtr CurrentBuildManifest;

		// The manifest for the build we want to install.
		FBuildPatchAppManifestRef NewBuildManifest;

		// The directory created in staging, to store local patch data.
		FString DataStagingDir;

		// The directory created in staging to construct install files to.
		FString InstallStagingDir;

		// The filename used to mark a previous install that did not complete but moved staged files into the install directory.
		FString PreviousMoveMarker;

		// A critical section to protect variables.
		mutable FCriticalSection ThreadLock;

		// A flag to store if we are installing file data.
		const bool bIsFileData;

		// A flag to store if we are installing chunk data (to help readability).
		const bool bIsChunkData;

		// A flag storing whether the process was a success.
		FThreadSafeBool bSuccess;

		// A flag marking that we a running.
		FThreadSafeBool bIsRunning;

		// A flag marking that we initialized correctly.
		FThreadSafeBool bIsInited;

		// A flag that stores whether we are on the first install iteration.
		FThreadSafeBool bFirstInstallIteration;

		// Keep track of build stats.
		FBuildInstallStats BuildStats;

		// Keep track of install progress.
		FBuildPatchProgress BuildProgress;

		// Whether we are currently paused.
		bool bIsPaused;

		// Whether we are needing to abort.
		bool bShouldAbort;

		// Holds a list of files that have been placed into the install directory.
		TArray<FString> FilesInstalled;

		// Holds the files which are all required.
		TSet<FString> TaggedFiles;

		// The files that the installation process required to construct.
		TSet<FString> FilesToConstruct;

		// Map of registered installations.
		TMap<FString, FBuildPatchAppManifestRef> InstallationInfo;

		// The file which contains per machine configuration information.
		FString LocalMachineConfigFile;

		// HTTP manager used to make requests for download data.
		TUniquePtr<IHttpManager> HttpManager;

		// File systems for classes requiring disk access.
		TUniquePtr<IFileSystem> FileSystem;

		// Platform abstraction.
		TUniquePtr<IPlatform> Platform;

		// Installer error tracking system.
		TUniquePtr<IInstallerError> InstallerError;

		// The analytics provider interface.
		TSharedPtr<IAnalyticsProvider> Analytics;

		// The HTTP tracker service interface.
		TSharedPtr<FHttpServiceTracker> HttpTracker;

		// Installer analytics handler.
		TUniquePtr<IInstallerAnalytics> InstallerAnalytics;

		// Installer statistics tracking.
		TUniquePtr<IInstallerStatistics> InstallerStatistics;

		// Download service.
		TUniquePtr<IDownloadService> DownloadService;

		// The message pump controller.
		TUniquePtr<IMessagePump> MessagePump;

		// List of controllable classes that have been constructed.
		TArray<IControllable*> Controllables;

		// List of message handlers that have been registered.
		TArray<FMessageHandler*> MessageHandlers;

		// Stage timers for build stats.
		FProcessTimer InitializeTimer;
		FProcessTimer ConstructTimer;
		FProcessTimer MoveFromStageTimer;
		FProcessTimer FileAttributesTimer;
		FProcessTimer VerifyTimer;
		FProcessTimer CleanUpTimer;
		FProcessTimer PrereqTimer;
		FProcessTimer ProcessPausedTimer;
		FProcessTimer ProcessActiveTimer;
		FProcessTimer ProcessExecuteTimer;

	public:
		/**
		 * Constructor takes configuration and dependencies.
		 * @param Configuration             The installer configuration structure.
		 * @param InstallationInfo          Map of locally installed apps for use as chunk sources.
		 * @param LocalMachineConfigFile    Filename for the local machine's config. This is used for per-machine configuration rather than shipped or user config.
		 * @param Analytics                 Optionally valid ptr to an analytics provider for sending events.
		 * @param HttpTracker               Optionally valid ptr to HTTP tracker to collect HTTP requests.
		 * @param OnCompleteDelegate        Delegate for when the process has completed.
		 */
		FBuildPatchInstaller(FInstallerConfiguration Configuration, TMap<FString, FBuildPatchAppManifestRef> InstallationInfo, const FString& LocalMachineConfigFile, TSharedPtr<IAnalyticsProvider> Analytics, TSharedPtr<FHttpServiceTracker> HttpTracker, FBuildPatchBoolManifestDelegate OnCompleteDelegate);

		/**
		 * Default Destructor, will delete the allocated Thread.
		 */
		~FBuildPatchInstaller();

		// FRunnable interface begin.
		uint32 Run() override;
		// FRunnable interface end.

		// IBuildInstaller interface begin.
		virtual bool IsComplete() const override;
		virtual bool IsCanceled() const override;
		virtual bool IsPaused() const override;
		virtual bool IsResumable() const override;
		virtual bool HasError() const override;
		virtual EBuildPatchInstallError GetErrorType() const override;
		virtual FString GetErrorCode() const override;
		//@todo this is deprecated and shouldn't be used anymore [6/4/2014 justin.sargent]
		virtual FText GetPercentageText() const override;
		//@todo this is deprecated and shouldn't be used anymore [6/4/2014 justin.sargent]
		virtual FText GetDownloadSpeedText() const override;
		virtual double GetDownloadSpeed() const override;
		virtual int64 GetInitialDownloadSize() const override;
		virtual int64 GetTotalDownloaded() const override;
		virtual EBuildPatchState GetState() const override;
		virtual FText GetStatusText() const override;
		virtual float GetUpdateProgress() const override;
		virtual FBuildInstallStats GetBuildStatistics() const override;
		virtual EBuildPatchDownloadHealth GetDownloadHealth() const override;
		virtual FText GetErrorText() const override;
		virtual void CancelInstall() override;
		virtual bool TogglePauseInstall() override;
		virtual void RegisterMessageHandler(FMessageHandler* MessageHandler) override;
		virtual void UnregisterMessageHandler(FMessageHandler* MessageHandler) override;
		// IBuildInstaller interface end.

		/**
		 * Begin the installation process.
		 * @return true if the installation started successfully, or is already running.
		 */
		bool StartInstallation();

		/**
		 * Executes the on complete delegate. This should only be called when completed, and is separated out
		 * to allow control to make this call on the main thread.
		 */
		void ExecuteCompleteDelegate();

		/**
		 * Pumps all queued messages to registered handlers.
		 */
		void PumpMessages();

		/**
		 * Only returns once the thread has finished running.
		 */
		void WaitForThread() const;

		/**
		 * Called by the module during shutdown.
		 */
		void PreExit();

	private:

		/**
		 * Initialise the installer.
		 * @return Whether initialization was successful.
		 */
		bool Initialize();

		/**
		 * Checks the installation directory for any already existing files of the correct size, with may account for manual
		 * installation. Should be used for new installation detecting existing files.
		 * NB: Not useful for patches, where we'd expect existing files anyway.
		 * @return    Returns true if there were potentially already installed files.
		 */
		bool CheckForExternallyInstalledFiles();

		/**
		 * Runs the installation process.
		 * @param   CorruptFiles    A list of files that were corrupt, to only install those.
		 * @return  Returns true if there were no errors blocking installation.
		 */
		bool RunInstallation(TArray<FString>& CorruptFiles);

		/**
		 * Runs the prerequisite installation process.
		 */
		bool RunPrerequisites();

		/**
		 * Runs the backup process for locally changed files, and then moves new files into installation directory.
		 * @return    Returns true if there were no errors.
		 */
		bool RunBackupAndMove();

		/**
		 * Runs the process to setup all file attributes required.
		 * @param bForce            Set true if also removing attributes to force the API calls to be made.
		 * @return    Returns true if there were no errors.
		 */
		bool RunFileAttributes(bool bForce = false);

		/**
		 * Runs the verification process.
		 * @param CorruptFiles  OUT     Receives the list of files that failed verification.
		 * @return    Returns true if there were no corrupt files.
		 */
		bool RunVerification(TArray<FString>& CorruptFiles);

		/**
		 * Checks a particular file in the install directory to see if it needs backing up, and does so if necessary.
		 * @param Filename                      The filename to check, which should match a filename in a manifest.
		 * @param bDiscoveredByVerification     Optional, whether the file was detected changed already by verification stage. Default: false.
		 * @return    Returns true if there were no errors.
		 */
		bool BackupFileIfNecessary(const FString& Filename, bool bDiscoveredByVerification = false);

		/**
		 * Delete empty directories from an installation.
		 * @param RootDirectory     Root Directory for search.
		 */
		void CleanupEmptyDirectories(const FString& RootDirectory);

		/**
		 * Builds the chunkdb source configuration struct.
		 */
		FChunkDbSourceConfig BuildChunkDbSourceConfig();

		/**
		 * Builds the installation source configuration struct.
		 * @param ChunkIgnoreSet    A set of chunks to initially not consider for loading, use for chunks that are knowingly available in higher priority sources.
		 */
		FInstallSourceConfig BuildInstallSourceConfig(TSet<FGuid> ChunkIgnoreSet);

		/**
		 * Builds the cloud source configuration struct.
		 */
		FCloudSourceConfig BuildCloudSourceConfig();
	};
}

typedef TSharedPtr< BuildPatchServices::FBuildPatchInstaller, ESPMode::ThreadSafe > FBuildPatchInstallerPtr;
typedef TSharedRef< BuildPatchServices::FBuildPatchInstaller, ESPMode::ThreadSafe > FBuildPatchInstallerRef;
typedef TWeakPtr< BuildPatchServices::FBuildPatchInstaller, ESPMode::ThreadSafe > FBuildPatchInstallerWeakPtr;
