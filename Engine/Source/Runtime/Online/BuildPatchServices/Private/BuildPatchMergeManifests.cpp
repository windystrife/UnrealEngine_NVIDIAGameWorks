// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchMergeManifests.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/ThreadSafeBool.h"
#include "Misc/Guid.h"
#include "BuildPatchManifest.h"
#include "Async/Future.h"
#include "Async/Async.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMergeManifests, Log, All);
DEFINE_LOG_CATEGORY(LogMergeManifests);

namespace MergeHelpers
{
	FBuildPatchAppManifestPtr LoadManifestFile(const FString& ManifestFilePath, FCriticalSection* UObjectAllocationLock)
	{
		check(UObjectAllocationLock != nullptr);
		UObjectAllocationLock->Lock();
		FBuildPatchAppManifestPtr Manifest = MakeShareable(new FBuildPatchAppManifest());
		UObjectAllocationLock->Unlock();
		if (Manifest->LoadFromFile(ManifestFilePath))
		{
			return Manifest;
		}
		return FBuildPatchAppManifestPtr();
	}

	bool CopyFileDataFromManifestToArray(const TSet<FString>& Filenames, const FBuildPatchAppManifestPtr& Source, TArray<FFileManifestData>& DestArray)
	{
		bool bSuccess = true;
		for (const FString& Filename : Filenames)
		{
			check(Source.IsValid());
			const FFileManifestData* FileManifest = Source->GetFileManifest(Filename);
			if (FileManifest == nullptr)
			{
				UE_LOG(LogMergeManifests, Error, TEXT("Could not find file in %s %s: %s"), *Source->GetAppName(), *Source->GetVersionString(), *Filename);
				bSuccess = false;
			}
			else
			{
				DestArray.Add(*FileManifest);
			}
		}
		return bSuccess;
	}
}

bool FBuildMergeManifests::MergeManifests(const FString& ManifestFilePathA, const FString& ManifestFilePathB, const FString& ManifestFilePathC, const FString& NewVersionString, const FString& SelectionDetailFilePath)
{
	bool bSuccess = true;
	FCriticalSection UObjectAllocationLock;

	TFunction<FBuildPatchAppManifestPtr()> TaskManifestA = [&UObjectAllocationLock, &ManifestFilePathA]()
	{
		return MergeHelpers::LoadManifestFile(ManifestFilePathA, &UObjectAllocationLock);
	};
	TFunction<FBuildPatchAppManifestPtr()> TaskManifestB = [&UObjectAllocationLock, &ManifestFilePathB]()
	{
		return MergeHelpers::LoadManifestFile(ManifestFilePathB, &UObjectAllocationLock);
	};
	typedef TPair<TSet<FString>,TSet<FString>> FStringSetPair;
	FThreadSafeBool bSelectionDetailSuccess = false;
	TFunction<FStringSetPair()> TaskSelectionInfo = [&SelectionDetailFilePath, &bSelectionDetailSuccess]()
	{
		bSelectionDetailSuccess = true;
		FStringSetPair StringSetPair;
		if(SelectionDetailFilePath.IsEmpty() == false)
		{
			FString SelectionDetailFileData;
			bSelectionDetailSuccess = FFileHelper::LoadFileToString(SelectionDetailFileData, *SelectionDetailFilePath);
			if (bSelectionDetailSuccess)
			{
				TArray<FString> SelectionDetailLines;
				SelectionDetailFileData.ParseIntoArrayLines(SelectionDetailLines);
				for (int32 LineIdx = 0; LineIdx < SelectionDetailLines.Num(); ++LineIdx)
				{
					FString Filename, Source;
					SelectionDetailLines[LineIdx].Split(TEXT("\t"), &Filename, &Source, ESearchCase::CaseSensitive);
					Filename = Filename.TrimStartAndEnd().TrimQuotes();
					FPaths::NormalizeDirectoryName(Filename);
					Source = Source.TrimStartAndEnd().TrimQuotes();
					if (Source == TEXT("A"))
					{
						StringSetPair.Key.Add(Filename);
					}
					else if (Source == TEXT("B"))
					{
						StringSetPair.Value.Add(Filename);
					}
					else
					{
						UE_LOG(LogMergeManifests, Error, TEXT("Could not parse line %d from %s"), LineIdx + 1, *SelectionDetailFilePath);
						bSelectionDetailSuccess = false;
					}
				}
			}
			else
			{
				UE_LOG(LogMergeManifests, Error, TEXT("Could not load selection detail file %s"), *SelectionDetailFilePath);
			}
		}
		return MoveTemp(StringSetPair);
	};

	TFuture<FBuildPatchAppManifestPtr> FutureManifestA = Async(EAsyncExecution::ThreadPool, MoveTemp(TaskManifestA));
	TFuture<FBuildPatchAppManifestPtr> FutureManifestB = Async(EAsyncExecution::ThreadPool, MoveTemp(TaskManifestB));
	TFuture<FStringSetPair> FutureSelectionDetail = Async(EAsyncExecution::ThreadPool, MoveTemp(TaskSelectionInfo));

	FBuildPatchAppManifestPtr ManifestA = FutureManifestA.Get();
	FBuildPatchAppManifestPtr ManifestB = FutureManifestB.Get();
	FStringSetPair SelectionDetail = FutureSelectionDetail.Get();

	// Flush any logs collected by tasks
	GLog->FlushThreadedLogs();

	// We must have loaded our manifests
	if (ManifestA.IsValid() == false)
	{
		UE_LOG(LogMergeManifests, Error, TEXT("Could not load manifest %s"), *ManifestFilePathA);
		return false;
	}
	if (ManifestB.IsValid() == false)
	{
		UE_LOG(LogMergeManifests, Error, TEXT("Could not load manifest %s"), *ManifestFilePathB);
		return false;
	}

	// Check if the selection detail had an error
	if (bSelectionDetailSuccess == false)
	{
		return false;
	}

	// If we have no selection detail, then we take the union of all files, preferring the version from B
	if (SelectionDetail.Key.Num() == 0 && SelectionDetail.Value.Num() == 0)
	{
		TSet<FString> ManifestFilesA(ManifestA->GetBuildFileList());
		SelectionDetail.Value.Append(ManifestB->GetBuildFileList());
		SelectionDetail.Key = ManifestFilesA.Difference(SelectionDetail.Value);
	}

	// Create the new manifest
	FBuildPatchAppManifest MergedManifest;

	// Copy basic info from B
	MergedManifest.ManifestFileVersion = ManifestB->ManifestFileVersion;
	MergedManifest.bIsFileData = ManifestB->bIsFileData;
	MergedManifest.AppID = ManifestB->AppID;
	MergedManifest.AppName = ManifestB->AppName;
	MergedManifest.LaunchExe = ManifestB->LaunchExe;
	MergedManifest.LaunchCommand = ManifestB->LaunchCommand;
	MergedManifest.PrereqIds = ManifestB->PrereqIds;
	MergedManifest.PrereqName = ManifestB->PrereqName;
	MergedManifest.PrereqPath = ManifestB->PrereqPath;
	MergedManifest.PrereqArgs = ManifestB->PrereqArgs;
	MergedManifest.CustomFields = ManifestB->CustomFields;

	// Set the new version string
	MergedManifest.BuildVersion = NewVersionString;

	// Copy the file manifests required from A
	bSuccess = MergeHelpers::CopyFileDataFromManifestToArray(SelectionDetail.Key, ManifestA, MergedManifest.FileManifestList) && bSuccess;

	// Copy the file manifests required from B
	bSuccess = MergeHelpers::CopyFileDataFromManifestToArray(SelectionDetail.Value, ManifestB, MergedManifest.FileManifestList) && bSuccess;

	// Sort the file manifests before entering chunk info
	MergedManifest.FileManifestList.Sort();

	// Fill out the chunk list in order of reference
	TSet<FGuid> ReferencedChunks;
	for (const FFileManifestData& FileManifest: MergedManifest.FileManifestList)
	{
		for (const FChunkPartData& FileChunkPart : FileManifest.FileChunkParts)
		{
			if (ReferencedChunks.Contains(FileChunkPart.Guid) == false)
			{
				ReferencedChunks.Add(FileChunkPart.Guid);

				// Find the chunk info
				FChunkInfoData** ChunkInfo = ManifestB->ChunkInfoLookup.Find(FileChunkPart.Guid);
				if (ChunkInfo == nullptr)
				{
					ChunkInfo = ManifestA->ChunkInfoLookup.Find(FileChunkPart.Guid);
				}
				if (ChunkInfo == nullptr)
				{
					UE_LOG(LogMergeManifests, Error, TEXT("Failed to copy chunk meta for %s used by %s. Possible damaged manifest file as input."), *FileChunkPart.Guid.ToString(), *FileManifest.Filename);
					bSuccess = false;
				}
				else
				{
					MergedManifest.ChunkList.Add(**ChunkInfo);
				}
			}
		}
	}

	// Save the new manifest out if we didn't register a failure
	if (bSuccess)
	{
		MergedManifest.InitLookups();
		if (!MergedManifest.SaveToFile(ManifestFilePathC, false))
		{
			UE_LOG(LogMergeManifests, Error, TEXT("Failed to save new manifest %s"), *ManifestFilePathC);
			bSuccess = false;
		}
	}
	else
	{
		UE_LOG(LogMergeManifests, Error, TEXT("Not saving new manifest due to previous errors."));
	}

	return bSuccess;
}
