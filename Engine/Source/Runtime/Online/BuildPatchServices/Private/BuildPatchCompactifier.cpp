// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
BuildPatchCompactifier.cpp: Implements the classes that clean up chunk and file
data that are no longer referenced by the manifests in a given cloud directory.
=============================================================================*/

#include "BuildPatchCompactifier.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "BuildPatchManifest.h"
#include "BuildPatchServicesModule.h"
#include "BuildPatchUtil.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDataCompactifier, Log, All);
DEFINE_LOG_CATEGORY(LogDataCompactifier);

/* Constructors
*****************************************************************************/

FBuildDataCompactifier::FBuildDataCompactifier(const FString& InCloudDir, bool bInPreview)
	: CloudDir(InCloudDir)
	, bPreview(bInPreview)
{
}

/* Public static methods
*****************************************************************************/

bool FBuildDataCompactifier::CompactifyCloudDirectory(const FString& CloudDir, float DataAgeThreshold, bool bPreview, const FString& DeletedChunkLogFile)
{
	FBuildDataCompactifier Compactifier(CloudDir, bPreview);
	return Compactifier.Compactify(DataAgeThreshold, DeletedChunkLogFile);
}

bool FBuildDataCompactifier::CompactifyCloudDirectory(float DataAgeThreshold, bool bPreview, const FString& DeletedChunkLogFile)
{
	return CompactifyCloudDirectory(FBuildPatchServicesModule::GetCloudDirectory(), DataAgeThreshold, bPreview, DeletedChunkLogFile);
}

/* Private methods
*****************************************************************************/

bool FBuildDataCompactifier::Compactify(float DataAgeThreshold, const FString& DeletedChunkLogFile) const
{
	UE_LOG(LogDataCompactifier, Log, TEXT("Running on %s%s"), *CloudDir, bPreview ? TEXT(". Preview mode. NO action will be taken.") : TEXT(""));
	UE_LOG(LogDataCompactifier, Log, TEXT("Minimum age of deleted chunks: %.3f days."), DataAgeThreshold);

	// We'll work out the date of the oldest unreferenced file we'll keep
	FDateTime Cutoff = FDateTime::UtcNow() - FTimespan::FromDays(DataAgeThreshold);

	const bool bLogDeletedChunks = !DeletedChunkLogFile.IsEmpty();

	// We'll get ALL files first, so we can use the count to preallocate space within the data filenames array to save excessive reallocs
	TArray<FString> AllFiles;
	const bool bFindFiles = true;
	const bool bFindDirectories = false;
	IFileManager::Get().FindFilesRecursive(AllFiles, *CloudDir, TEXT("*.*"), bFindFiles, bFindDirectories);

	TSet<FGuid> ReferencedGuids; // The master list of *ALL* referenced chunk / file data Guids

	TArray<FString> ManifestFilenames;
	TArray<FGuid> DataGuids; // The Guids associated with the data files from a single manifest
	int32 NumDataFiles = 0;
	
	// Preallocate enough storage in DataGuids, to stop repeatedly expanding the allocation
	DataGuids.Reserve(AllFiles.Num());

	EnumerateManifests(ManifestFilenames);

	// If we don't have any manifest files, notify that we'll continue to delete all mature chunks.
	if (ManifestFilenames.Num() == 0)
	{
		UE_LOG(LogDataCompactifier, Log, TEXT("Could not find any manifest files. Proceeding to delete all mature chunks."));
	}

	// Process each remaining manifest, and build up a list of all referenced files
	for (const auto& ManifestFilename : ManifestFilenames)
	{
		const FString ManifestPath = CloudDir / ManifestFilename;
		UE_LOG(LogDataCompactifier, Log, TEXT("Extracting chunk filenames from %s."), *ManifestFilename);

		// Load the manifest data from the manifest
		FBuildPatchAppManifest Manifest;
		if (Manifest.LoadFromFile(ManifestPath))
		{
			// Work out all data Guids referenced in the manifest, and add them to our list of files to keep
			DataGuids.Empty();
			Manifest.GetDataList(DataGuids);

			UE_LOG(LogDataCompactifier, Log, TEXT("Extracted %d chunks from %s. Unioning with %d existing chunks."), DataGuids.Num(), *ManifestFilename, NumDataFiles);
			NumDataFiles += DataGuids.Num();

			// We're going to keep all the Guids so we know which files to keep later
			ReferencedGuids.Append(DataGuids);
		}
		else
		{
			// We failed to read from the manifest file.  This is an error which should halt progress and return a non-zero exit code
			UE_LOG(LogDataCompactifier, Error, TEXT("Could not parse manifest file %s."), *ManifestFilename);
			return false;
		}
	}

	UE_LOG(LogDataCompactifier, Log, TEXT("Walking %s to remove all mature unreferenced chunks and compute statistics."), *CloudDir);

	uint32 FilesProcessed = 0;
	uint32 FilesSkipped = 0;
	uint32 NonPatchFilesProcessed = 0;
	uint32 FilesDeleted = 0;
	uint64 BytesProcessed = 0;
	uint64 BytesSkipped = 0;
	uint64 NonPatchBytesProcessed = 0;
	uint64 BytesDeleted = 0;
	int64 CurrentFileSize;
	FGuid FileGuid;
	TArray<FString> DeletedChunks;

	for (const auto& File : AllFiles)
	{
		CurrentFileSize = IFileManager::Get().FileSize(*File);
		if (CurrentFileSize >= 0)
		{
			++FilesProcessed;
			BytesProcessed += CurrentFileSize;

			if (!GetPatchDataGuid(File, FileGuid))
			{
				FString CleanFilename = FPaths::GetCleanFilename(File);
				if (!ManifestFilenames.Contains(CleanFilename))
				{
					++NonPatchFilesProcessed;
					NonPatchBytesProcessed += CurrentFileSize;
				}
				continue;
			}

			if (!ReferencedGuids.Contains(FileGuid))
			{
				if (IFileManager::Get().GetTimeStamp(*File) < Cutoff)
				{
					// This file is not referenced by any manifest, is a data file, and is older than we need to keep ...
					// Let's get rid of it!
					DeleteFile(File);
					++FilesDeleted;
					BytesDeleted += CurrentFileSize;
					if (bLogDeletedChunks)
					{
						// Make path relative to cloud directory and add to our set of deleted chunks, which we'll output later.
						DeletedChunks.Add(File.RightChop(CloudDir.Len() + 1));
					}
				}
				else
				{
					++FilesSkipped;
					BytesSkipped += CurrentFileSize;
				}
			}
		}
		else
		{
			UE_LOG(LogDataCompactifier, Warning, TEXT("Could not determine size of %s. Perhaps it has been removed by another process."), *File);
		}
	}

	if (bLogDeletedChunks)
	{
		FString FullList;
		for (const FString& DeletedChunk : DeletedChunks)
		{
			FullList += DeletedChunk + TEXT("\r\n");
		}
		if (FFileHelper::SaveStringToFile(FullList, *DeletedChunkLogFile))
		{
			UE_LOG(LogDataCompactifier, Log, TEXT("Saved list of deleted chunks out to %s"), *DeletedChunkLogFile);
		}
		else
		{
			UE_LOG(LogDataCompactifier, Error, TEXT("Failed to save list of deleted chunks out to %s"), *DeletedChunkLogFile);
		}
	}

	UE_LOG(LogDataCompactifier, Log, TEXT("Found %u files totalling %s."), FilesProcessed, *HumanReadableSize(BytesProcessed));
	UE_LOG(LogDataCompactifier, Log, TEXT("Of these, %u (totalling %s) were not chunk/manifest files."), NonPatchFilesProcessed, *HumanReadableSize(NonPatchBytesProcessed));
	UE_LOG(LogDataCompactifier, Log, TEXT("Deleted %u chunk files totalling %s."), FilesDeleted, *HumanReadableSize(BytesDeleted));
	UE_LOG(LogDataCompactifier, Log, TEXT("Skipped %u unreferenced chunk files (totalling %s) which have not yet aged out."), FilesSkipped, *HumanReadableSize(BytesSkipped));
	return true;
}

void FBuildDataCompactifier::DeleteFile(const FString& FilePath) const
{
	FString LogMsg = FString::Printf(TEXT("Deprecated data %s"), *FilePath);
	if (!bPreview)
	{
		LogMsg = LogMsg.Append(TEXT(" ... deleted"));
		IFileManager::Get().Delete(*FilePath);
	}
	UE_LOG(LogDataCompactifier, Log, TEXT("%s"), *LogMsg);
}

void FBuildDataCompactifier::EnumerateManifests(TArray<FString>& OutManifests) const
{
	// Get a list of all manifest filenames by using IFileManager
	FString FilePattern = CloudDir / TEXT("*.manifest");
	IFileManager::Get().FindFiles(OutManifests, *FilePattern, true, false);
}

bool FBuildDataCompactifier::GetPatchDataGuid(const FString& FilePath, FGuid& OutGuid) const
{
	FString Extension = FPaths::GetExtension(FilePath);
	if (Extension != TEXT("chunk") && Extension != TEXT("file"))
	{
		// Our filename doesn't have one of the allowed file extensions, so it's not patch data
		return false;
	}

	FString BaseFileName = FPaths::GetBaseFilename(FilePath);
	if (BaseFileName.Contains(TEXT("_")))
	{
		FString Right;
		BaseFileName.Split(TEXT("_"), NULL, &Right);
		if (Right.Len() != 32)
		{
			return false; // Valid data files which contain an _ in their filename must have 32 characters to the right of the underscore
		}
	}
	else
	{
		if (BaseFileName.Len() != 32)
		{
			return false; // Valid data files whose names do not contain underscores must be 32 characters long
		}
	}

	return FBuildPatchUtils::GetGUIDFromFilename(FilePath, OutGuid);
}

FString FBuildDataCompactifier::HumanReadableSize(uint64 NumBytes, uint8 DecimalPlaces, bool bUseBase10) const
{
	static const TCHAR* const Suffixes[2][7] = { { TEXT("Bytes"), TEXT("kB"), TEXT("MB"), TEXT("GB"), TEXT("TB"), TEXT("PB"), TEXT("EB") },
	                                             { TEXT("Bytes"), TEXT("KiB"), TEXT("MiB"), TEXT("GiB"), TEXT("TiB"), TEXT("PiB"), TEXT("EiB") }};

	float DataSize = (float)NumBytes;

	const int32 Base = bUseBase10 ? 1000 : 1024;
	int32 Index = FMath::FloorToInt(FMath::LogX(Base, NumBytes));
	Index = FMath::Clamp<int32>(Index, 0, ARRAY_COUNT(Suffixes[0]) - 1);

	DecimalPlaces = FMath::Clamp<int32>(DecimalPlaces, 0, Index*3);

	return FString::Printf(TEXT("%.*f %s"), DecimalPlaces, DataSize / (FMath::Pow(Base, Index)), Suffixes[bUseBase10 ? 0 : 1][Index]);
}
