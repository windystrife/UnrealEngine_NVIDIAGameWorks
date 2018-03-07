// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchDiffManifests.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "BuildPatchManifest.h"
#include "BuildPatchUtil.h"
#include "Async/Future.h"
#include "Async/Async.h"
#include "Algo/Sort.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDiffManifests, Log, All);
DEFINE_LOG_CATEGORY(LogDiffManifests);

// For the output file we'll use pretty json in debug, otherwise condensed.
#if UE_BUILD_DEBUG
typedef  TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FDiffJsonWriter;
typedef  TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FDiffJsonWriterFactory;
#else
typedef  TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FDiffJsonWriter;
typedef  TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FDiffJsonWriterFactory;
#endif //UE_BUILD_DEBUG

namespace DiffHelpers
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
}

bool FBuildDiffManifests::DiffManifests(const FString& ManifestFilePathA, const TSet<FString>& TagSetA, const FString& ManifestFilePathB, const TSet<FString>& TagSetB, const FString& OutputFilePath)
{
	bool bSuccess = true;
	FCriticalSection UObjectAllocationLock;

	TFunction<FBuildPatchAppManifestPtr()> TaskManifestA = [&UObjectAllocationLock, &ManifestFilePathA]()
	{
		return DiffHelpers::LoadManifestFile(ManifestFilePathA, &UObjectAllocationLock);
	};
	TFunction<FBuildPatchAppManifestPtr()> TaskManifestB = [&UObjectAllocationLock, &ManifestFilePathB]()
	{
		return DiffHelpers::LoadManifestFile(ManifestFilePathB, &UObjectAllocationLock);
	};

	TFuture<FBuildPatchAppManifestPtr> FutureManifestA = Async(EAsyncExecution::ThreadPool, MoveTemp(TaskManifestA));
	TFuture<FBuildPatchAppManifestPtr> FutureManifestB = Async(EAsyncExecution::ThreadPool, MoveTemp(TaskManifestB));

	FBuildPatchAppManifestPtr ManifestA = FutureManifestA.Get();
	FBuildPatchAppManifestPtr ManifestB = FutureManifestB.Get();

	// Flush any logs collected by tasks.
	GLog->FlushThreadedLogs();

	// We must have loaded our manifests.
	if (ManifestA.IsValid() == false)
	{
		UE_LOG(LogDiffManifests, Error, TEXT("Could not load manifest %s"), *ManifestFilePathA);
		return false;
	}
	if (ManifestB.IsValid() == false)
	{
		UE_LOG(LogDiffManifests, Error, TEXT("Could not load manifest %s"), *ManifestFilePathB);
		return false;
	}

	TSet<FString> TagsA, TagsB;
	ManifestA->GetFileTagList(TagsA);
	if (TagSetA.Num() > 0)
	{
		TagsA = TagsA.Intersect(TagSetA);
	}
	ManifestB->GetFileTagList(TagsB);
	if (TagSetB.Num() > 0)
	{
		TagsB = TagsB.Intersect(TagSetB);
	}

	int64 NewChunksCount = 0;
	int64 TotalChunkSize = 0;
	TSet<FString> TaggedFiles;
	TSet<FGuid> ChunkSetA;
	TSet<FGuid> ChunkSetB;
	ManifestA->GetTaggedFileList(TagsA, TaggedFiles);
	ManifestA->GetChunksRequiredForFiles(TaggedFiles, ChunkSetA);
	TaggedFiles.Reset();
	ManifestB->GetTaggedFileList(TagsB, TaggedFiles);
	ManifestB->GetChunksRequiredForFiles(TaggedFiles, ChunkSetB);
	TArray<FString> NewChunkPaths;
	for (FGuid& ChunkB : ChunkSetB)
	{
		if (ChunkSetA.Contains(ChunkB) == false)
		{
			++NewChunksCount;
			int32 ChunkFileSize = ManifestB->GetDataSize(ChunkB);
			TotalChunkSize += ChunkFileSize;
			NewChunkPaths.Add(FBuildPatchUtils::GetDataFilename(ManifestB.ToSharedRef(), TEXT("."), ChunkB));
			UE_LOG(LogDiffManifests, Verbose, TEXT("New chunk discovered: Size: %10lld, Path: %s"), ChunkFileSize, *NewChunkPaths.Last());
		}
	}

	UE_LOG(LogDiffManifests, Log, TEXT("New chunks:  %lld"), NewChunksCount);
	UE_LOG(LogDiffManifests, Log, TEXT("Total bytes: %lld"), TotalChunkSize);

	// Log download details.
	FNumberFormattingOptions SizeFormattingOptions;
	SizeFormattingOptions.MaximumFractionalDigits = 3;
	SizeFormattingOptions.MinimumFractionalDigits = 3;

	int64 DownloadSizeA = ManifestA->GetDownloadSize(TagsA);
	int64 BuildSizeA = ManifestA->GetBuildSize(TagsA);
	int64 DownloadSizeB = ManifestB->GetDownloadSize(TagsB);
	int64 BuildSizeB = ManifestB->GetBuildSize(TagsB);
	int64 DeltaDownloadSize = ManifestB->GetDeltaDownloadSize(TagsB, ManifestA.ToSharedRef(), TagsA);

	// Break down the sizes and delta into new chunks per tag.
	TMap<FString, int64> TagDownloadImpactA;
	TMap<FString, int64> TagBuildImpactA;
	TMap<FString, int64> TagDownloadImpactB;
	TMap<FString, int64> TagBuildImpactB;
	TMap<FString, int64> TagDeltaImpact;
	for (const FString& Tag : TagsA)
	{
		TSet<FString> TagSet;
		TagSet.Add(Tag);
		TagDownloadImpactA.Add(Tag, ManifestA->GetDownloadSize(TagSet));
		TagBuildImpactA.Add(Tag, ManifestA->GetBuildSize(TagSet));
	}
	for (const FString& Tag : TagsB)
	{
		TSet<FString> TagSet;
		TagSet.Add(Tag);
		TagDownloadImpactB.Add(Tag, ManifestB->GetDownloadSize(TagSet));
		TagBuildImpactB.Add(Tag, ManifestB->GetBuildSize(TagSet));
		TagDeltaImpact.Add(Tag, ManifestB->GetDeltaDownloadSize(TagSet, ManifestA.ToSharedRef(), TagsA));
	}

	// Log the information.
	TArray<FString> TagArrayB = TagsB.Array();
	Algo::Sort(TagArrayB);
	FString UntaggedLog(TEXT("(untagged)"));
	FString TagLogList = FString::Join(TagArrayB, TEXT(", "));
	if (TagLogList.IsEmpty() || TagLogList.StartsWith(TEXT(", ")))
	{
		TagLogList.InsertAt(0, UntaggedLog);
	}
	UE_LOG(LogDiffManifests, Log, TEXT("TagSet: %s"), *TagLogList);
	UE_LOG(LogDiffManifests, Log, TEXT("%s %s:"), *ManifestA->GetAppName(), *ManifestA->GetVersionString());
	UE_LOG(LogDiffManifests, Log, TEXT("    Download Size:  %10s"), *FText::AsMemory(DownloadSizeA, &SizeFormattingOptions).ToString());
	UE_LOG(LogDiffManifests, Log, TEXT("    Build Size:     %10s"), *FText::AsMemory(BuildSizeA, &SizeFormattingOptions).ToString())
	UE_LOG(LogDiffManifests, Log, TEXT("%s %s:"), *ManifestB->GetAppName(), *ManifestB->GetVersionString());
	UE_LOG(LogDiffManifests, Log, TEXT("    Download Size:  %10s"), *FText::AsMemory(DownloadSizeB, &SizeFormattingOptions).ToString());
	UE_LOG(LogDiffManifests, Log, TEXT("    Build Size:     %10s"), *FText::AsMemory(BuildSizeB, &SizeFormattingOptions).ToString());
	UE_LOG(LogDiffManifests, Log, TEXT("%s %s -> %s %s:"), *ManifestA->GetAppName(), *ManifestA->GetVersionString(), *ManifestB->GetAppName(), *ManifestB->GetVersionString());
	UE_LOG(LogDiffManifests, Log, TEXT("    Delta Size:     %10s"), *FText::AsMemory(DeltaDownloadSize, &SizeFormattingOptions).ToString());
	UE_LOG(LogDiffManifests, Log, TEXT(""));
	for (const FString& Tag : TagArrayB)
	{
		UE_LOG(LogDiffManifests, Log, TEXT("%s Impact:"), *(Tag.IsEmpty() ? UntaggedLog : Tag));
		UE_LOG(LogDiffManifests, Log, TEXT("    Individual Download Size: %10s"), *FText::AsMemory(TagDownloadImpactB[Tag], &SizeFormattingOptions).ToString());
		UE_LOG(LogDiffManifests, Log, TEXT("    Individual Build Size:    %10s"), *FText::AsMemory(TagBuildImpactB[Tag], &SizeFormattingOptions).ToString());
		UE_LOG(LogDiffManifests, Log, TEXT("    Individual Delta Size:    %10s"), *FText::AsMemory(TagDeltaImpact[Tag], &SizeFormattingOptions).ToString());
	}

	// Save the output.
	if (bSuccess && OutputFilePath.IsEmpty() == false)
	{
		FString JsonOutput;
		TSharedRef<FDiffJsonWriter> Writer = FDiffJsonWriterFactory::Create(&JsonOutput);
		Writer->WriteObjectStart();
		{
			Writer->WriteObjectStart(TEXT("ManifestA"));
			{
				Writer->WriteValue(TEXT("AppName"), ManifestA->GetAppName());
				Writer->WriteValue(TEXT("AppId"), static_cast<int32>(ManifestA->GetAppID()));
				Writer->WriteValue(TEXT("VersionString"), ManifestA->GetVersionString());
				Writer->WriteValue(TEXT("DownloadSize"), DownloadSizeA);
				Writer->WriteValue(TEXT("BuildSize"), BuildSizeA);
				Writer->WriteObjectStart(TEXT("IndividualTagDownloadSizes"));
				for (const TPair<FString, int64>& Pair : TagDownloadImpactA)
				{
					Writer->WriteValue(Pair.Key, Pair.Value);
				}
				Writer->WriteObjectEnd();
				Writer->WriteObjectStart(TEXT("IndividualTagBuildSizes"));
				for (const TPair<FString, int64>& Pair : TagBuildImpactA)
				{
					Writer->WriteValue(Pair.Key, Pair.Value);
				}
				Writer->WriteObjectEnd();
			}
			Writer->WriteObjectEnd();
			Writer->WriteObjectStart(TEXT("ManifestB"));
			{
				Writer->WriteValue(TEXT("AppName"), ManifestB->GetAppName());
				Writer->WriteValue(TEXT("AppId"), static_cast<int32>(ManifestB->GetAppID()));
				Writer->WriteValue(TEXT("VersionString"), ManifestB->GetVersionString());
				Writer->WriteValue(TEXT("DownloadSize"), DownloadSizeB);
				Writer->WriteValue(TEXT("BuildSize"), BuildSizeB);
				Writer->WriteObjectStart(TEXT("IndividualTagDownloadSizes"));
				for (const TPair<FString, int64>& Pair : TagDownloadImpactB)
				{
					Writer->WriteValue(Pair.Key, Pair.Value);
				}
				Writer->WriteObjectEnd();
				Writer->WriteObjectStart(TEXT("IndividualTagBuildSizes"));
				for (const TPair<FString, int64>& Pair : TagBuildImpactB)
				{
					Writer->WriteValue(Pair.Key, Pair.Value);
				}
				Writer->WriteObjectEnd();
			}
			Writer->WriteObjectEnd();
			Writer->WriteObjectStart(TEXT("Differential"));
			{
				Writer->WriteArrayStart(TEXT("NewChunkPaths"));
				for (const FString& NewChunkPath : NewChunkPaths)
				{
					Writer->WriteValue(NewChunkPath);
				}
				Writer->WriteArrayEnd();
				Writer->WriteValue(TEXT("TotalChunkSize"), TotalChunkSize);
				Writer->WriteValue(TEXT("DeltaDownloadSize"), DeltaDownloadSize);
				Writer->WriteObjectStart(TEXT("IndividualTagDeltaSizes"));
				for (const TPair<FString, int64>& Pair : TagDeltaImpact)
				{
					Writer->WriteValue(Pair.Key, Pair.Value);
				}
				Writer->WriteObjectEnd();
			}
			Writer->WriteObjectEnd();
		}
		Writer->WriteObjectEnd();
		Writer->Close();
		bSuccess = FFileHelper::SaveStringToFile(JsonOutput, *OutputFilePath);
		if (!bSuccess)
		{
			UE_LOG(LogDiffManifests, Error, TEXT("Could not save output to %s"), *OutputFilePath);
		}
	}

	return bSuccess;
}
