// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

/**
 * Stores the version information associated with a build
 */
class PROJECTS_API FBuildVersion
{
public:
	/**
	 * The major engine version (4 for UE4)
	 */
	int MajorVersion;

	/**
	 * The minor engine version
	 */
	int MinorVersion;

	/**
	 * The hotfix/patch version
	 */
	int PatchVersion;

	/**
	 * The changelist that the engine is being built from
	 */
	int Changelist;

	/**
	 * The changelist that the engine maintains compatibility with
	 */
	int CompatibleChangelist;

	/**
	 * Whether the changelist numbers are a licensee changelist
	 */
	int IsLicenseeVersion;

	/**
	 * Whether the current build is a promoted build, that is, built strictly from a clean sync of the given changelist
	 */
	int IsPromotedBuild;

	/**
	 * Name of the current branch, with '/' characters escaped as '+'
	 */
	FString BranchName;

	/**
	 * The current build id. This will be generated automatically whenever engine binaries change if not set in the default Engine/Build/Build.version.
	 */
	FString BuildId;

	/**
	 * Default constructor. Initializes the structure to empty.
	 */
	FBuildVersion();

	/// <summary>
	/// Get the default path to the build.version file on disk
	/// </summary>
	/// <returns>Path to the Build.version file</returns>
	static FString GetDefaultFileName();

	/// <summary>
	/// Get the path to the version file for the current executable.
	/// </summary>
	/// <returns>Path to the target's version file</returns>
	static FString GetFileNameForCurrentExecutable();
	
	/**
	 * Try to read a version file from disk
	 *
	 * @param FileName Path to the version file
	 * @param OutVersion The version information
	 * @return True if the version was read successfully, false otherwise
	 */
	static bool TryRead(const FString& FileName, FBuildVersion& OutVersion);

	/**
	 * Parses a build version from a FJsonObject
	 *
	 * @param Object object to parse
	 * @param OutVersion The version information
	 * @return True if the version was parsed successfully, false otherwise
	 */
	static bool TryParse(const FJsonObject& Object, FBuildVersion& OutVersion);
};
