// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IBuildManifest.h"
#include "Misc/Variant.h"
#include "BuildPatchVerify.h"

namespace BuildPatchServices
{
	/**
	 * Defines a list of all the options of an installation task.
	 */
	struct FInstallerConfiguration
	{
		// The manifest that the current install was generated from (if applicable).
		IBuildManifestPtr CurrentManifest;
		// The manifest to be installed.
		IBuildManifestRef InstallManifest;
		// The directory to install to.
		FString InstallDirectory;
		// The directory for storing the intermediate files. This would usually be inside the InstallDirectory. Empty string will use module's global setting.
		FString StagingDirectory;
		// The directory for placing files that are believed to have local changes, before we overwrite them. Empty string will use module's global setting. If both empty, the feature disables.
		FString BackupDirectory;
		// The list of chunk database filenames that will be used to pull patch data from.
		TArray<FString> ChunkDatabaseFiles;
		// The list of cloud directory roots that will be used to pull patch data from. Empty array will use module's global setting.
		TArray<FString> CloudDirectories;
		// The set of tags that describe what to be installed. Empty set means full installation.
		TSet<FString> InstallTags;
		// The mode for verification.
		EVerifyMode VerifyMode;
		// Whether the operation is a repair to an existing installation only.
		bool bIsRepair;
		// Whether the operation should only produce the necessary staged data, without performing the final install stage yet.
		bool bStageOnly;
		// Whether to run the prerequisite installer provided if it hasn't been ran before on this machine.
		bool bRunRequiredPrereqs;

		/**
		 * Construct with install manifest, provides common defaults for other settings.
		 */
		FInstallerConfiguration(const IBuildManifestRef& InInstallManifest)
			: CurrentManifest(nullptr)
			, InstallManifest(InInstallManifest)
			, InstallDirectory()
			, StagingDirectory()
			, BackupDirectory()
			, ChunkDatabaseFiles()
			, CloudDirectories()
			, InstallTags()
			, VerifyMode(EVerifyMode::ShaVerifyAllFiles)
			, bIsRepair(false)
			, bStageOnly(false)
			, bRunRequiredPrereqs(true)
		{}

		/**
		 * RValue constructor to allow move semantics.
		 */
		FInstallerConfiguration(FInstallerConfiguration&& MoveFrom)
			: CurrentManifest(MoveTemp(MoveFrom.CurrentManifest))
			, InstallManifest(MoveTemp(MoveFrom.InstallManifest))
			, InstallDirectory(MoveTemp(MoveFrom.InstallDirectory))
			, StagingDirectory(MoveTemp(MoveFrom.StagingDirectory))
			, BackupDirectory(MoveTemp(MoveFrom.BackupDirectory))
			, ChunkDatabaseFiles(MoveTemp(MoveFrom.ChunkDatabaseFiles))
			, CloudDirectories(MoveTemp(MoveFrom.CloudDirectories))
			, InstallTags(MoveTemp(MoveFrom.InstallTags))
			, VerifyMode(MoveTemp(MoveFrom.VerifyMode))
			, bIsRepair(MoveTemp(MoveFrom.bIsRepair))
			, bStageOnly(MoveTemp(MoveFrom.bStageOnly))
			, bRunRequiredPrereqs(MoveTemp(MoveFrom.bRunRequiredPrereqs))
		{}

		/**
		 * Copy constructor.
		 */
		FInstallerConfiguration(const FInstallerConfiguration& CopyFrom)
			: CurrentManifest(CopyFrom.CurrentManifest)
			, InstallManifest(CopyFrom.InstallManifest)
			, InstallDirectory(CopyFrom.InstallDirectory)
			, StagingDirectory(CopyFrom.StagingDirectory)
			, BackupDirectory(CopyFrom.BackupDirectory)
			, ChunkDatabaseFiles(CopyFrom.ChunkDatabaseFiles)
			, CloudDirectories(CopyFrom.CloudDirectories)
			, InstallTags(CopyFrom.InstallTags)
			, VerifyMode(CopyFrom.VerifyMode)
			, bIsRepair(CopyFrom.bIsRepair)
			, bStageOnly(CopyFrom.bStageOnly)
			, bRunRequiredPrereqs(CopyFrom.bRunRequiredPrereqs)
		{}
	};

	/**
	 * Defines a list of all options for generation tasks.
	 */
	struct FGenerationConfiguration
	{
		// The directory to analyze
		FString RootDirectory;
		// The ID of the app of this build
		uint32 AppId;
		// The name of the app of this build
		FString AppName;
		// The version string for this build
		FString BuildVersion;
		// The local exe path that would launch this build
		FString LaunchExe;
		// The command line that would launch this build
		FString LaunchCommand;
		// The path to a file containing a \r\n separated list of RootDirectory relative files to ignore.
		FString IgnoreListFile;
		// The path to a file containing a \r\n separated list of RootDirectory relative files followed by attribute keywords.
		FString AttributeListFile;
		// The set of identifiers which the prerequisites satisfy
		TSet<FString> PrereqIds;
		// The display name of the prerequisites installer
		FString PrereqName;
		// The path to the prerequisites installer
		FString PrereqPath;
		// The command line arguments for the prerequisites installer
		FString PrereqArgs;
		// The maximum age (in days) of existing data files which can be reused in this build
		float DataAgeThreshold;
		// Indicates whether data age threshold should be honored. If false, ALL data files can be reused
		bool bShouldHonorReuseThreshold;
		// Map of custom fields to add to the manifest
		TMap<FString, FVariant> CustomFields;
		// The cloud directory that all patch data will be saved to. An empty value will use module's global setting.
		FString CloudDirectory;
		// The output manifest filename.
		FString OutputFilename;

		/**
		 * Default constructor
		 */
		FGenerationConfiguration()
			: RootDirectory()
			, AppId()
			, AppName()
			, BuildVersion()
			, LaunchExe()
			, LaunchCommand()
			, IgnoreListFile()
			, AttributeListFile()
			, PrereqIds()
			, PrereqName()
			, PrereqPath()
			, PrereqArgs()
			, DataAgeThreshold()
			, bShouldHonorReuseThreshold()
			, CustomFields()
			, CloudDirectory()
			, OutputFilename()
		{}
	};
}
