// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/Controllable.h"
#include "BuildPatchManifest.h"

namespace BuildPatchServices
{
	class IVerifierStat;
	enum class EVerifyMode : uint32;

	/**
	 * An interface providing the functionality to verify a local installation.
	 */
	class IVerifier
		: public IControllable
	{
	public:
		/**
		 * Verifies a local directory structure against a given manifest.
		 * NOTE: This function is blocking and will not return until finished. Don't run on main thread.
		 * @param OutDatedFiles    OUT  The array of files that do not match or are locally missing.
		 * @return    true if no file errors occurred AND the verification was successful
		 */
		virtual bool Verify(TArray<FString>& OutDatedFiles) = 0;
	};

	class FVerifierFactory
	{
	public:
		/**
		 * Creates a verifier class that will verify a local directory structure against a given manifest, optionally
		 * taking account of a staging directory where alternative files are used.
		 * NOTE: This function is blocking and will not return until finished. Don't run on a UI thread.
		 * @param FileSystem            The file system interface.
		 * @param VerifierStat          Pointer to the class which will receive status updates.
		 * @param VerifyMode            The verify mode to run.
		 * @param TouchedFiles          The set of files that were touched by the installation, these will be verified.
		 * @param InstallTags           The install tags, will be used when verifying all files.
		 * @param Manifest              The manifest describing the build data.
		 * @param VerifyDirectory       The directory to analyze.
		 * @param StagedFileDirectory   A stage directory for updated files, ignored if empty string. If a file exists here, it will be checked instead of the one in VerifyDirectory.
		 * @return     Ref of an object that can be used to perform the operation.
		 */
		static IVerifier* Create(IFileSystem* FileSystem, IVerifierStat* VerifierStat, EVerifyMode VerifyMode, TSet<FString> TouchedFiles, TSet<FString> InstallTags, FBuildPatchAppManifestRef Manifest, FString VerifyDirectory, FString StagedFileDirectory);
	};

	/**
	 * This interface defines the statistics class required by the verifier system. It should be implemented in order to collect
	 * desired information which is being broadcast by the system.
	 */
	class IVerifierStat
	{
	public:
		virtual ~IVerifierStat() {}

		/**
		 * Called each time a file is going to be verified.
		 * @param Filename      The filename of the file.
		 * @param FileSize      The size of the file being verified.
		 */
		virtual void OnFileStarted(const FString& Filename, int64 FileSize) = 0;

		/**
		 * Called during a file verification with the current progress.
		 * @param Filename      The filename of the file.
		 * @param TotalBytes    The number of bytes processed so far.
		 */
		virtual void OnFileProgress(const FString& Filename, int64 TotalBytes) = 0;

		/**
		 * Called each time a file has finished being verified.
		 * @param Filename      The filename of the file.
		 * @param bSuccess      True if the file data was correct.
		 */
		virtual void OnFileCompleted(const FString& Filename, bool bSuccess) = 0;

		/**
		 * Called to update the total amount of bytes which have been processed.
		 * @param TotalBytes    The number of bytes processed so far.
		 */
		virtual void OnProcessedDataUpdated(int64 TotalBytes) = 0;

		/**
		 * Called to update the total number of bytes to be processed.
		 * @param TotalBytes    The total number of bytes to be processed.
		 */
		virtual void OnTotalRequiredUpdated(int64 TotalBytes) = 0;
	};
}
