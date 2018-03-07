// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=================================================================================
BuildPatchCompactifier.h: Declares classes involved with compactifying build data.
===================================================================================*/

#pragma once

#include "CoreMinimal.h"

/**
 * Used to run compactify routines on cloud directories.
 * Compactification removes all data files not currently associated
 * with an active manifest file.
 */
class FBuildDataCompactifier
{
public:
	/**
	 * Processes the Cloud Directory to identify and delete any orphaned chunks or files.
	 * The CloudDir should already have been set using FBuildPatchServicesModule::SetCloudDirectory
	 * NOTE: THIS function is blocking and will not return until finished. Don't run on main thread.
	 * @param DataAgeThreshold       Chunks which are not referenced by a valid manifest, and which are older than this age (in days), will be deleted
	 * @param bPreview               If true, then no actual work will be done, but all operations which would be carried out will be logged.
	 * @param DeletedChunkLogFile    The full path to a file to which a list of all chunk files deleted by compactify will be written. The output filenames will be relative to the cloud directory
	 * @return                       true if no file errors occurred
	 */
	static bool CompactifyCloudDirectory(float DataAgeThreshold, bool bPreview, const FString& DeletedChunkLogFile);

	/**
	 * Processes the specified Cloud Directory to identify and delete any orphaned chunks or files.
	 * NOTE: THIS function is blocking and will not return until finished. Don't run on main thread.
	 * @param CloudDir               The path to the Cloud Dir to compactify
	 * @param DataAgeThreshold       Chunks which are not referenced by a valid manifest, and which are older than this age (in days), will be deleted
	 * @param bPreview               If true, then no actual work will be done, but all operations which would be carried out will be logged
	 * @param DeletedChunkLogFile    The full path to a file to which a list of all chunk files deleted by compactify will be written. The output filenames will be relative to the cloud directory
	 * @return                       true if no file errors occurred
	 */
	static bool CompactifyCloudDirectory(const FString& CloudDir, float DataAgeThreshold, bool bPreview, const FString& DeletedChunkLogFile);

private:
	const FString CloudDir;
	const bool bPreview;

	/**
	 * Constructor.
	 * @param CloudDir         The directory that this compactifier should operate on.
	 * @param bPreview         If true, then no actual work will be done, but all operations which would be carried out will be logged.
	 */
	FBuildDataCompactifier(const FString& CloudDir, bool bPreview);

	/**
	 * Processes the Cloud Directory to identify and delete any orphaned chunks or files.
	 * NOTE: THIS function is blocking and will not return until finished. Don't run on main thread.
	 * @param DataAgeThreshold       The minimum age (in days) of data before it is eligible to be deleted
	 * @param DeletedChunkLogFile    The full path to a file to which a list of all chunk files deleted by compactify will be written. The output filenames will be relative to the cloud directory
	 * @return                       true if no file errors occurred
	 */
	bool Compactify(float DataAgeThreshold, const FString& DeletedChunkLogFile) const;

	/**
	 * Deletes the specified file, and logs the deletion
	 * @param FilePath     The path to the file to delete
	 */
	void DeleteFile(const FString& FilePath) const;

	/**
	 * Obtains a list of manifest filenames, along with the manifest data, in the specified Cloud Directory
	 * @param Manifests    OUT  An array of filenames corresponding to manifest files within the Cloud Directory
	 */
	void EnumerateManifests(TArray<FString>& OutManifests) const;

	/**
	 * Determines whether or not the filename passed in contains patch data, indicated by a file extension of
	 * .chunk or .file, and obtains the file's data Guid.
	 * @param FilePath    IN  The filename to get the Guid from
	 * @param OutGuid     OUT The Guid of the filename (only set if result  is true)
	 * @return                True if the file is a patch and we could determine its Guid, false otherwise
	 */
	bool GetPatchDataGuid(const FString& FilePath, FGuid& OutGuid) const;

	/**
	 * Converts a data size (in bytes) into a more human readable form using kB, MB, GB etc.
	 * @param NumBytes       The value (in bytes) to convert
	 * @param DecimalPlaces  The number of decimal places of the size to show
	 * @param bUseBase10     If true, 1kB = 1000 bytes and so on, otherwise 1KiB = 1024 bytes ...
	 * @return               A string containing the human readable size
	 */
	FString HumanReadableSize(uint64 NumBytes, uint8 DecimalPlaces = 2, bool bUseBase10 = false) const;
};
