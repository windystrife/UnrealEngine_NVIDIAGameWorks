// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchFileConstructor.h: Declares the BuildPatchFileConstructor class
	that handles creating files in a manifest from the chunks that make it.
=============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "HAL/Runnable.h"
#include "BuildPatchProgress.h"
#include "BuildPatchManifest.h"
#include "Installer/Controllable.h"

// Forward declarations
class FBuildPatchAppManifest;
struct FChunkPart;
namespace BuildPatchServices
{
	class IChunkSource;
	class IChunkReferenceTracker;
	class IInstallerError;
	class IInstallerAnalytics;
	class IFileConstructorStat;

	/**
	 * FBuildPatchFileConstructor
	 * This class controls a thread that constructs files from a file list, given install details, and chunk availability notifications
	 */
	class FBuildPatchFileConstructor
		: public FRunnable
		, public IControllable
	{
	private:
		// Hold a pointer to my thread for easier deleting
		FRunnableThread* Thread;

		// A flag marking that we a running
		bool bIsRunning;

		// A flag marking that we initialized correctly
		bool bIsInited;

		// A flag marking that our init returned a failure (true means failed)
		bool bInitFailed;

		// A flag marking that we told the chunk cache to queue required downloads
		bool bIsDownloadStarted;

		// A flag marking that we have made the initial disk space check following resume logic compelte
		bool bInitialDiskSizeCheck;

		// A flag marking whether we should be paused
		FThreadSafeBool bIsPaused;

		// A flag marking whether we should abort operations and exit
		FThreadSafeBool bShouldAbort;

		// A critical section to protect the flags and variables
		FCriticalSection ThreadLock;

		// The build manifest for the app we are installing
		FBuildPatchAppManifestRef BuildManifest;

		// The root installation directory 
		FString InstallDirectory;

		// The directory for staging files
		FString StagingDirectory;

		// A list of filenames for files that need to be constructed in this build
		TArray<FString> FilesToConstruct;

		// Pointer to chunk source.
		IChunkSource* ChunkSource;

		// Pointer to the chunk reference tracker.
		IChunkReferenceTracker* ChunkReferenceTracker;

		// Pointer to the installer error class.
		IInstallerError* InstallerError;

		// Pointer to the installer analytics handler.
		IInstallerAnalytics* InstallerAnalytics;

		// Pointer to the stat class.
		IFileConstructorStat* FileConstructorStat;

		// Total job size for tracking progress
		int64 TotalJobSize;

		// Byte processed so far for tracking progress
		int64 ByteProcessed;

		// A list of filenames for files that have been constructed
		TArray<FString> FilesConstructed;

	public:

		/**
		 * Constructor
		 * @param BuildManifest             The Manifest for the build we are installing.
		 * @param InstallDirectory          The root location where the installation is going.
		 * @param StageDirectory            The location where we will store temporary files.
		 * @param ConstructList             The list of files to be constructed, filename paths should match those contained in manifest.
		 * @param ChunkSource               Pointer to the chunk source.
		 * @param ChunkReferenceTracker     Pointer to the chunk reference tracker.
		 * @param InstallerError            Pointer to the installer error class for reporting fatal errors.
		 * @param InstallerAnalytics        Pointer to the installer analytics handler for reporting events.
		 * @param FileConstructorStat       Pointer to the stat class for receiving updates.
		 */
		FBuildPatchFileConstructor(FBuildPatchAppManifestRef BuildManifest, FString InstallDirectory, FString StageDirectory, TArray<FString> ConstructList, IChunkSource* ChunkSource, IChunkReferenceTracker* ChunkReferenceTracker, IInstallerError* InstallerError, IInstallerAnalytics* InstallerAnalytics, IFileConstructorStat* FileConstructorStat);

		/**
		 * Default Destructor, will delete the allocated Thread
		 */
		~FBuildPatchFileConstructor();

		// FRunnable interface begin.
		virtual bool Init() override;
		virtual uint32 Run() override;
		// FRunnable interface end.

		// IControllable interface begin.
		virtual void SetPaused(bool bInIsPaused) override
		{
			bIsPaused = bInIsPaused;
		}

		virtual void Abort() override
		{
			bShouldAbort = true;
		}
		// IControllable interface end.

		/**
		 * Blocks the calling thread until this one has completed
		 */
		void Wait();

		/**
		 * Get whether the thread has finished working
		 * @return	true if the thread completed
		 */
		bool IsComplete();

	private:

		/**
		 * Sets the bIsRunning flag
		 * @param bRunning	Whether the thread is running
		 */
		void SetRunning(bool bRunning);

		/**
		 * Sets the bIsInited flag
		 * @param bInited	Whether the thread successfully initialized
		 */
		void SetInited(bool bInited);

		/**
		 * Sets the bInitFailed flag
		 * @param bFailed	Whether the thread failed on init
		 */
		void SetInitFailed(bool bFailed);

		/**
		 * Count additional bytes processed, and set new install progress value
		 * @param ByteCount		Number of bytes to increment by
		 */
		void CountBytesProcessed(const int64& ByteCount);

		/**
		 * Function to fetch a chunk from the download list
		 * @param Filename		Receives the filename for the file to construct from the manifest
		 * @return true if there was a chunk guid in the list
		 */
		bool GetFileToConstruct(FString& Filename);

		/**
		 * @return the total bytes size of files not yet started construction
		 */
		int64 GetRemainingBytes();

		/**
		 * Constructs a particular file referenced by the given BuildManifest. The function takes an interface to a class that can provide availability information of chunks so that this
		 * file construction process can be ran alongside chunk acquisition threads. It will Sleep while waiting for chunks that it needs.
		 * @param Filename			The Filename for the file to construct, that matches an entry in the BuildManifest.
		 * @param bResumeExisting	Whether we should resume from an existing file
		 * @return	true if no file errors occurred
		 */
		bool ConstructFileFromChunks(const FString& Filename, bool bResumeExisting);

		/**
		 * Inserts the data data from a chunk into the destination file according to the chunk part info
		 * @param ChunkPart			The chunk part details.
		 * @param DestinationFile	The Filename for the file being constructed.
		 * @param HashState			An FSHA1 hash state to update with the data going into the destination file.
		 * @return true if no errors were detected
		 */
		bool InsertChunkData(const FChunkPartData& ChunkPart, FArchive& DestinationFile, FSHA1& HashState);

		/**
		 * Delete all contents of a directory
		 * @param RootDirectory	 	Directory to make empty
		 */
		void DeleteDirectoryContents(const FString& RootDirectory);
	};

	/**
	 * This interface defines the statistics class required by the file constructor. It should be implemented in order to collect
	 * desired information which is being broadcast by the system.
	 */
	class IFileConstructorStat
	{
	public:
		virtual ~IFileConstructorStat() {}

		/**
		 * Called when the resume process begins.
		 */
		virtual void OnResumeStarted() = 0;

		/**
		 * Called when the resume process completes.
		 */
		virtual void OnResumeCompleted() = 0;

		/**
		 * Called when a file construction has started.
		 * @param Filename      The filename of the file.
		 * @param FileSize      The size of the file being constructed.
		 */
		virtual void OnFileStarted(const FString& Filename, int64 FileSize) = 0;

		/**
		 * Called during a file construction with the current progress.
		 * @param Filename      The filename of the file.
		 * @param TotalBytes    The number of bytes processed so far.
		 */
		virtual void OnFileProgress(const FString& Filename, int64 TotalBytes) = 0;

		/**
		 * Called when a file construction has completed.
		 * @param Filename      The filename of the file.
		 * @param bSuccess      True if the file construction succeeded.
		 */
		virtual void OnFileCompleted(const FString& Filename, bool bSuccess) = 0;

		/**
		 * Called to update the total amount of bytes which have been constructed.
		 * @param TotalBytes    The number of bytes constructed so far.
		 */
		virtual void OnProcessedDataUpdated(int64 TotalBytes) = 0;

		/**
		 * Called to update the total number of bytes to be constructed.
		 * @param TotalBytes    The total number of bytes to be constructed.
		 */
		virtual void OnTotalRequiredUpdated(int64 TotalBytes) = 0;
	};

}
