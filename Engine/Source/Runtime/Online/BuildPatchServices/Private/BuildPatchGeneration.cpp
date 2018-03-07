// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchGeneration.h"
#include "Templates/ScopedPointer.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/SecureHash.h"
#include "UniquePtr.h"
#include "BuildPatchManifest.h"
#include "BuildPatchServicesModule.h"
#include "BuildPatchHash.h"
#include "Core/BlockStructure.h"
#include "Data/ChunkData.h"
#include "Generation/DataScanner.h"
#include "Generation/BuildStreamer.h"
#include "Generation/CloudEnumeration.h"
#include "Generation/ManifestBuilder.h"
#include "Generation/FileAttributesParser.h"
#include "Generation/ChunkWriter.h"

using namespace BuildPatchServices;

DECLARE_LOG_CATEGORY_EXTERN(LogPatchGeneration, Log, All);
DEFINE_LOG_CATEGORY(LogPatchGeneration);

namespace BuildPatchServices
{
	struct FScannerDetails
	{
	public:
		FScannerDetails(int32 InLayer, uint64 InLayerOffset, bool bInIsFinalScanner, uint64 InPaddingSize, TArray<uint8> InData, FBlockStructure InStructure, const ICloudEnumerationRef& CloudEnumeration, const FStatsCollectorRef& StatsCollector)
			: Layer(InLayer)
			, LayerOffset(InLayerOffset)
			, bIsFinalScanner(bInIsFinalScanner)
			, PaddingSize(InPaddingSize)
			, Data(MoveTemp(InData))
			, Structure(MoveTemp(InStructure))
			, Scanner(FDataScannerFactory::Create(Data, CloudEnumeration, StatsCollector))
		{}

	public:
		int32 Layer;
		uint64 LayerOffset;
		bool bIsFinalScanner;
		uint64 PaddingSize;
		TArray<uint8> Data;
		FBlockStructure Structure;
		IDataScannerRef Scanner;
	};
}

namespace PatchGenerationHelpers
{
	bool SerializeIntersection(const FBlockStructure& ByteStructure, const FBlockStructure& Intersection, FBlockStructure& SerialRanges)
	{
		FBlockStructure ActualIntersection = ByteStructure.Intersect(Intersection);
		uint64 ByteOffset = 0;
		const FBlockEntry* ByteBlock = ByteStructure.GetHead();
		const FBlockEntry* IntersectionBlock = ActualIntersection.GetHead();
		while (ByteBlock != nullptr && IntersectionBlock != nullptr)
		{
			uint64 ByteBlockEnd = ByteBlock->GetOffset() + ByteBlock->GetSize();
			if (ByteBlockEnd <= IntersectionBlock->GetOffset())
			{
				ByteOffset += ByteBlock->GetSize();
				ByteBlock = ByteBlock->GetNext();
				continue;
			}
			else
			{
				// Intersection must overlap.
				check(IntersectionBlock->GetOffset() >= ByteBlock->GetOffset());
				check(ByteBlockEnd >= (IntersectionBlock->GetOffset() + IntersectionBlock->GetSize()));
				uint64 Chop = IntersectionBlock->GetOffset() - ByteBlock->GetOffset();
				ByteOffset += Chop;
				SerialRanges.Add(ByteOffset, IntersectionBlock->GetSize());
				ByteOffset += ByteBlock->GetSize() - Chop;
				IntersectionBlock = IntersectionBlock->GetNext();
				ByteBlock = ByteBlock->GetNext();
			}
		}
		return true;
	}

	uint64 CountSerialBytes(const FBlockStructure& Structure)
	{
		uint64 Count = 0;
		const FBlockEntry* Block = Structure.GetHead();
		while (Block != nullptr)
		{
			Count += Block->GetSize();
			Block = Block->GetNext();
		}
		return Count;
	}

	int32 GetMaxScannerBacklogCount()
	{
		int32 MaxScannerBacklogCount = 75;
		GConfig->GetInt(TEXT("BuildPatchServices"), TEXT("MaxScannerBacklog"), MaxScannerBacklogCount, GEngineIni);
		MaxScannerBacklogCount = FMath::Clamp<int32>(MaxScannerBacklogCount, 5, 500);
		return MaxScannerBacklogCount;
	}

	bool ScannerArrayFull(const TArray<TUniquePtr<FScannerDetails>>& Scanners)
	{
		static int32 MaxScannerBacklogCount = GetMaxScannerBacklogCount();
		return (FDataScannerCounter::GetNumIncompleteScanners() > FDataScannerCounter::GetNumRunningScanners()) || (Scanners.Num() >= MaxScannerBacklogCount);
	}
}

bool FBuildDataGenerator::GenerateChunksManifestFromDirectory(const BuildPatchServices::FGenerationConfiguration& Settings)
{
	uint64 StartTime = FStatsCollector::GetCycles();

	// Create a manifest details.
	FManifestDetails ManifestDetails;
	ManifestDetails.AppId = Settings.AppId;
	ManifestDetails.AppName = Settings.AppName;
	ManifestDetails.BuildVersion = Settings.BuildVersion;
	ManifestDetails.LaunchExe = Settings.LaunchExe;
	ManifestDetails.LaunchCommand = Settings.LaunchCommand;
	ManifestDetails.PrereqIds = Settings.PrereqIds;
	ManifestDetails.PrereqName = Settings.PrereqName;
	ManifestDetails.PrereqPath = Settings.PrereqPath;
	ManifestDetails.PrereqArgs = Settings.PrereqArgs;
	ManifestDetails.CustomFields = Settings.CustomFields;

	// Use the cloud directory passed in if present, otherwise fall back to module default
	FString CloudDirectory = Settings.CloudDirectory;
	if (CloudDirectory.IsEmpty())
	{
		CloudDirectory = FBuildPatchServicesModule::GetCloudDirectory();
	}

	// Check for the required output filename.
	if (Settings.OutputFilename.IsEmpty())
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("Manifest OutputFilename was not provided"));
		return false;
	}

	// Ensure that cloud directory exists, and create it if not.
	IFileManager::Get().MakeDirectory(*CloudDirectory, true);
	if (!IFileManager::Get().DirectoryExists(*CloudDirectory))
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("Unable to create specified cloud directory %s"), *CloudDirectory);
		return false;
	}

	// Load the required file attributes.
	if (!Settings.AttributeListFile.IsEmpty())
	{
		FFileAttributesParserRef FileAttributesParser = FFileAttributesParserFactory::Create();
		if (!FileAttributesParser->ParseFileAttributes(Settings.AttributeListFile, ManifestDetails.FileAttributesMap))
		{
			UE_LOG(LogPatchGeneration, Error, TEXT("Attributes list file did not parse %s"), *Settings.AttributeListFile);
			return false;
		}
	}

	// Create stat collector.
	FStatsCollectorRef StatsCollector = FStatsCollectorFactory::Create();

	// Enumerate Chunks.
	const FDateTime Cutoff = Settings.bShouldHonorReuseThreshold ? FDateTime::UtcNow() - FTimespan::FromDays(Settings.DataAgeThreshold) : FDateTime::MinValue();
	ICloudEnumerationRef CloudEnumeration = FCloudEnumerationFactory::Create(CloudDirectory, Cutoff, StatsCollector);

	// Create a build streamer.
	FBuildStreamerRef BuildStream = FBuildStreamerFactory::Create(Settings.RootDirectory, Settings.IgnoreListFile, StatsCollector);

	// Create a chunk writer.
	FChunkWriter ChunkWriter(CloudDirectory, StatsCollector);

	// Output to log for builder info.
	UE_LOG(LogPatchGeneration, Log, TEXT("Running Chunks Patch Generation for: %u:%s %s"), Settings.AppId, *Settings.AppName, *Settings.BuildVersion);

	// Create the manifest builder.
	IManifestBuilderRef ManifestBuilder = FManifestBuilderFactory::Create(ManifestDetails);

	TArray<FString> EnumeratedFiles = BuildStream->GetAllFilenames();

	// Check existence of prereq exe, if specified.
	if (!Settings.PrereqPath.IsEmpty() && !EnumeratedFiles.Contains(FPaths::Combine(Settings.RootDirectory, Settings.PrereqPath)))
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("Provided prerequisite executable file was not found within the build root. %s"), *Settings.PrereqPath);
		return false;
	}

	// Used to store data read lengths.
	uint32 ReadLen = 0;

	// The last time we logged out data processed.
	double LastProgressLog = FPlatformTime::Seconds();
	const double TimeGenStarted = LastProgressLog;

	// Load settings from config.
	float GenerationScannerSizeMegabytes = 32.5f;
	float StatsLoggerTimeSeconds = 10.0f;
	GConfig->GetFloat(TEXT("BuildPatchServices"), TEXT("GenerationScannerSizeMegabytes"), GenerationScannerSizeMegabytes, GEngineIni);
	GConfig->GetFloat(TEXT("BuildPatchServices"), TEXT("StatsLoggerTimeSeconds"), StatsLoggerTimeSeconds, GEngineIni);
	GenerationScannerSizeMegabytes = FMath::Clamp<float>(GenerationScannerSizeMegabytes, 10.0f, 500.0f);
	StatsLoggerTimeSeconds = FMath::Clamp<float>(StatsLoggerTimeSeconds, 1.0f, 60.0f);
	const uint64 ScannerDataSize = GenerationScannerSizeMegabytes * 1048576;
	const uint64 ScannerOverlapSize = BuildPatchServices::ChunkDataSize - 1;
	TArray<uint8> DataBuffer;

	// Setup Generation stats.
	volatile int64* StatTotalTime = StatsCollector->CreateStat(TEXT("Generation: Total Time"), EStatFormat::Timer);
	volatile int64* StatLayers = StatsCollector->CreateStat(TEXT("Generation: Layers"), EStatFormat::Value);
	volatile int64* StatNumScanners = StatsCollector->CreateStat(TEXT("Generation: Scanner Backlog"), EStatFormat::Value);
	volatile int64* StatUnknownDataAlloc = StatsCollector->CreateStat(TEXT("Generation: Unmatched Buffers Allocation"), EStatFormat::DataSize);
	volatile int64* StatUnknownDataNum = StatsCollector->CreateStat(TEXT("Generation: Unmatched Buffers Use"), EStatFormat::DataSize);
	int64 MaxLayer = 0;

	// List of created scanners.
	TArray<TUniquePtr<FScannerDetails>> Scanners;

	// Tracking info per layer for rescanning.
	TMap<int32, FChunkMatch> LayerToLastChunkMatch;
	TMap<int32, uint64> LayerToProcessedCount;
	TMap<int32, uint64> LayerToScannerCount;
	TMap<int32, uint64> LayerToDataOffset;
	TMap<int32, TArray<uint8>> LayerToUnknownLayerData;
	TMap<int32, FBlockStructure> LayerToUnknownLayerStructure;
	TMap<int32, FBlockStructure> LayerToUnknownBuildStructure;

	// For future investigation.
	TArray<FChunkMatch> RejectedChunkMatches;
	FBlockStructure BuildSpaceRejectedStructure;

	// Keep a copy of the new chunk inventory.
	TMap<uint64, TSet<FGuid>> ChunkInventory;
	TMap<FGuid, FSHAHash> ChunkShaHashes;

	// Loop through all data.
	bool bHasUnknownData = true;
	while (!BuildStream->IsEndOfData() || Scanners.Num() > 0 || bHasUnknownData)
	{
		if (!PatchGenerationHelpers::ScannerArrayFull(Scanners))
		{
			// Create a scanner from new build data?
			if (!BuildStream->IsEndOfData())
			{
				// Keep the overlap data from previous scanner.
				int32 PreviousSize = DataBuffer.Num();
				uint64& DataOffset = LayerToDataOffset.FindOrAdd(0);
				if (PreviousSize > 0)
				{
					check(PreviousSize > ScannerOverlapSize);
					uint8* CopyTo = DataBuffer.GetData();
					uint8* CopyFrom = CopyTo + (PreviousSize - ScannerOverlapSize);
					FMemory::Memcpy(CopyTo, CopyFrom, ScannerOverlapSize);
					DataBuffer.SetNum(ScannerOverlapSize, false);
					DataOffset += ScannerDataSize - ScannerOverlapSize;
				}

				// Grab some data from the build stream.
				PreviousSize = DataBuffer.Num();
				DataBuffer.SetNumUninitialized(ScannerDataSize);
				const bool bWaitForData = true;
				ReadLen = BuildStream->DequeueData(DataBuffer.GetData() + PreviousSize, ScannerDataSize - PreviousSize, bWaitForData);
				DataBuffer.SetNum(PreviousSize + ReadLen, false);

				// Only make a scanner if we are getting new data.
				if (ReadLen > 0)
				{
					// Pad scanner data if end of build
					uint64 PadSize = BuildStream->IsEndOfData() ? ScannerOverlapSize : 0;
					DataBuffer.AddZeroed(PadSize);

					// Create data processor.
					FBlockStructure Structure;
					Structure.Add(DataOffset, DataBuffer.Num() - PadSize);
					UE_LOG(LogPatchGeneration, Verbose, TEXT("Creating scanner on layer 0 at %llu. IsFinal:%d."), DataOffset, BuildStream->IsEndOfData());
					Scanners.Emplace(new FScannerDetails(0, DataOffset, BuildStream->IsEndOfData(), PadSize, DataBuffer, MoveTemp(Structure), CloudEnumeration, StatsCollector));
					LayerToScannerCount.FindOrAdd(0)++;
				}
			}
		}

		// Grab any completed scanners?
		FStatsCollector::Set(StatNumScanners, Scanners.Num());
		while (Scanners.Num() > 0 && Scanners[0]->Scanner->IsComplete())
		{
			FScannerDetails& Scanner = *Scanners[0];
			UE_LOG(LogPatchGeneration, Verbose, TEXT("Scanner on layer %d completed. IsFinal:%d."), Scanner.Layer, Scanner.bIsFinalScanner);

			// Check that we are able to process this scanner, there is a 2GB limit on our TArrays of data.
			if (LayerToUnknownLayerData.Contains(Scanner.Layer))
			{
				int32 CurrentUnknownLayerDataNum = LayerToUnknownLayerData[Scanner.Layer].Num();
				const int32 OneGigabyte = 1073741824;
				if (CurrentUnknownLayerDataNum >= OneGigabyte)
				{
					UE_LOG(LogPatchGeneration, Verbose, TEXT("Ignoring completed scanners in order to process accumulated unknown data."));
					break;
				}
			}

			// Get the scan result.
			TArray<FChunkMatch> ChunkMatches = Scanner.Scanner->GetResultWhenComplete();

			// Create structures to track results in required spaces.
			FBlockStructure LayerSpaceUnknownStructure;
			FBlockStructure LayerSpaceKnownStructure;
			FBlockStructure BuildSpaceUnknownStructure;
			FBlockStructure BuildSpaceKnownStructure;
			BuildSpaceUnknownStructure.Add(Scanner.Structure);
			LayerSpaceUnknownStructure.Add(Scanner.LayerOffset, Scanner.Data.Num() - Scanner.PaddingSize);

			// Grab the last match from previous layer scanner, to handle to overlap.
			FChunkMatch* LayerLastChunkMatch = LayerToLastChunkMatch.Find(Scanner.Layer);
			if (LayerLastChunkMatch != nullptr)
			{
				// Track the last match in this range's layer structure.
				LayerSpaceUnknownStructure.Remove(LayerLastChunkMatch->DataOffset, BuildPatchServices::ChunkDataSize);
				LayerSpaceKnownStructure.Add(LayerLastChunkMatch->DataOffset, BuildPatchServices::ChunkDataSize);
				LayerSpaceKnownStructure.Remove(0, Scanner.LayerOffset);

				// Assert there is one or zero blocks
				check(LayerSpaceKnownStructure.GetHead() == LayerSpaceKnownStructure.GetFoot());

				// Track the last match in this range's build structure.
				if (LayerSpaceKnownStructure.GetHead() != nullptr)
				{
					uint64 FirstByte = LayerSpaceKnownStructure.GetHead()->GetOffset() - Scanner.LayerOffset;
					uint64 Count = LayerSpaceKnownStructure.GetHead()->GetSize();
					FBlockStructure LastChunkBuildSpace;
					if (Scanner.Structure.SelectSerialBytes(FirstByte, Count, LastChunkBuildSpace) != Count)
					{
						// Fatal error, translated last chunk should be in range.
						UE_LOG(LogPatchGeneration, Error, TEXT("Translated last chunk match was not within scanner's range."));
						return false;
					}
					BuildSpaceUnknownStructure.Remove(LastChunkBuildSpace);
					BuildSpaceKnownStructure.Add(LastChunkBuildSpace);
				}
			}

			// Process each chunk that this scanner matched.
			for (int32 MatchIdx = 0; MatchIdx < ChunkMatches.Num(); ++MatchIdx)
			{
				FChunkMatch& Match = ChunkMatches[MatchIdx];

				// Translate to build space.
				FBlockStructure BuildSpaceChunkStructure;
				const uint64 BytesFound = Scanner.Structure.SelectSerialBytes(Match.DataOffset, BuildPatchServices::ChunkDataSize, BuildSpaceChunkStructure);
				const bool bFoundOk = Scanner.bIsFinalScanner || BytesFound == BuildPatchServices::ChunkDataSize;
				if (!bFoundOk)
				{
					// Fatal error if the scanner returned a matched range that doesn't fit inside it's data.
					UE_LOG(LogPatchGeneration, Error, TEXT("Chunk match was not within scanner's data structure."));
					return false;
				}

				uint64 LayerOffset = Scanner.LayerOffset + Match.DataOffset;
				if (LayerLastChunkMatch == nullptr || ((LayerLastChunkMatch->DataOffset + BuildPatchServices::ChunkDataSize) <= LayerOffset))
				{
					// Accept the match.
					LayerLastChunkMatch = &LayerToLastChunkMatch.Add(Scanner.Layer, FChunkMatch(LayerOffset, Match.ChunkGuid));

					// Track data from this scanner.
					LayerSpaceUnknownStructure.Remove(LayerOffset, BuildPatchServices::ChunkDataSize);
					LayerSpaceKnownStructure.Add(LayerOffset, BuildPatchServices::ChunkDataSize);

					// Process the chunk in build space.
					BuildSpaceUnknownStructure.Remove(BuildSpaceChunkStructure);
					BuildSpaceKnownStructure.Add(BuildSpaceChunkStructure);
					ManifestBuilder->AddChunkMatch(Match.ChunkGuid, BuildSpaceChunkStructure);
					UE_LOG(LogPatchGeneration, Verbose, TEXT("Accepted a chunk match with %s on layer %d. Mapping:%s"), *Match.ChunkGuid.ToString(), Scanner.Layer, *BuildSpaceChunkStructure.ToString());
				}
				else
				{
					// Currently we don't use overlapping chunks, but we should save that info to drive improvement investigation.
					RejectedChunkMatches.Add(Match);
					BuildSpaceRejectedStructure.Add(BuildSpaceChunkStructure);
					UE_LOG(LogPatchGeneration, Verbose, TEXT("Rejected an overlapping chunk match with %s on layer %d. Mapping:%s"), *Match.ChunkGuid.ToString(), Scanner.Layer, *BuildSpaceChunkStructure.ToString());
				}
			}
			// Remove padding from known structure.
			LayerSpaceKnownStructure.Remove(Scanner.LayerOffset + Scanner.Data.Num() - Scanner.PaddingSize, Scanner.PaddingSize);

			// Collect unknown data into buffers and spaces.
			TArray<uint8>& UnknownLayerData = LayerToUnknownLayerData.FindOrAdd(Scanner.Layer);
			FBlockStructure& UnknownLayerStructure = LayerToUnknownLayerStructure.FindOrAdd(Scanner.Layer);
			FBlockStructure& UnknownBuildStructure = LayerToUnknownBuildStructure.FindOrAdd(Scanner.Layer);

			// Check sanity of tracked data
			check(UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownLayerStructure)
				&& UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownBuildStructure));

			// Remove from unknown data buffer what we now know. This covers newly recognized data from previous overlaps.
			FBlockStructure NowKnownDataStructure;
			if (PatchGenerationHelpers::SerializeIntersection(UnknownLayerStructure, LayerSpaceKnownStructure, NowKnownDataStructure))
			{
				const FBlockEntry* NowKnownDataBlock = NowKnownDataStructure.GetFoot();
				while (NowKnownDataBlock != nullptr)
				{
					UnknownLayerData.RemoveAt(NowKnownDataBlock->GetOffset(), NowKnownDataBlock->GetSize(), false);
					NowKnownDataBlock = NowKnownDataBlock->GetPrevious();
				}
			}
			else
			{
				// Fatal error if we could not convert to serial space.
				UE_LOG(LogPatchGeneration, Error, TEXT("Could not convert new blocks to serial space."));
				return false;
			}
			UnknownLayerStructure.Remove(LayerSpaceKnownStructure);
			UnknownBuildStructure.Remove(BuildSpaceKnownStructure);

			// Check sanity of tracked data
			check(UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownLayerStructure)
				&& UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownBuildStructure));

			// Mark the number of bytes we know to be accurate. This stays one scanner behind.
			LayerToProcessedCount.FindOrAdd(Scanner.Layer) = UnknownLayerData.Num();

			// Add new unknown data to buffer and structures.
			LayerSpaceUnknownStructure.Remove(UnknownLayerStructure);
			const FBlockEntry* LayerSpaceUnknownBlock = LayerSpaceUnknownStructure.GetHead();
			while (LayerSpaceUnknownBlock != nullptr)
			{
				const int32 ScannerDataOffset = LayerSpaceUnknownBlock->GetOffset() - Scanner.LayerOffset;
				const int32 BlockSize = LayerSpaceUnknownBlock->GetSize();
				check(ScannerDataOffset >= 0 && (ScannerDataOffset + BlockSize) <= (Scanner.Data.Num() - Scanner.PaddingSize));
				UnknownLayerData.Append(&Scanner.Data[ScannerDataOffset], BlockSize);
				LayerSpaceUnknownBlock = LayerSpaceUnknownBlock->GetNext();
			}
			UnknownLayerStructure.Add(LayerSpaceUnknownStructure);
			UnknownBuildStructure.Add(BuildSpaceUnknownStructure);

			// Check sanity of tracked data
			check(UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownLayerStructure)
				&& UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownBuildStructure));

			// If this is the final scanner then mark all unknown data ready for processing.
			if (Scanner.bIsFinalScanner)
			{
				LayerToProcessedCount.FindOrAdd(Scanner.Layer) = UnknownLayerData.Num();
			}

			// Remove scanner from list.
			LayerToScannerCount.FindOrAdd(Scanner.Layer)--;
			Scanners.RemoveAt(0);
		}

		// Process some unknown data for each layer
		for (TPair<int32, TArray<uint8>>& UnknownLayerDataPair : LayerToUnknownLayerData)
		{
			// Gather info for the layer.
			const int32 Layer = UnknownLayerDataPair.Key;
			uint64& ProcessedCount = LayerToProcessedCount[Layer];
			TArray<uint8>& UnknownLayerData = UnknownLayerDataPair.Value;
			FBlockStructure& UnknownLayerStructure = LayerToUnknownLayerStructure[Layer];
			FBlockStructure& UnknownBuildStructure = LayerToUnknownBuildStructure[Layer];

			// Check if this final data for the layer.
			bool bIsFinalData = BuildStream->IsEndOfData();
			for (int32 LayerIdx = 0; LayerIdx < Layer; ++LayerIdx)
			{
				bIsFinalData = bIsFinalData && LayerToUnknownLayerData.FindRef(LayerIdx).Num() == 0;
				bIsFinalData = bIsFinalData && LayerToScannerCount.FindRef(LayerIdx) == 0;
			}
			bIsFinalData = bIsFinalData && LayerToScannerCount.FindRef(Layer) == 0;

			// Use large enough unknown data runs to make new chunks.
			if (UnknownLayerData.Num() > 0)
			{
				const FBlockEntry* UnknownLayerBlock = UnknownLayerStructure.GetHead();
				bool bSingleBlock = UnknownLayerBlock != nullptr && UnknownLayerBlock->GetNext() == nullptr;
				uint64 ByteOffset = 0;
				while (UnknownLayerBlock != nullptr)
				{
					uint64 ByteEnd = ByteOffset + UnknownLayerBlock->GetSize();
					if (ByteEnd > ProcessedCount)
					{
						// Clamp size by setting end to processed count or offset. Unsigned subtraction is unsafe.
						ByteEnd = FMath::Max<uint64>(ProcessedCount, ByteOffset);
					}
					// Make a new chunk if large enough block, or it's a final single block.
					if ((ByteEnd - ByteOffset) >= BuildPatchServices::ChunkDataSize || (bSingleBlock && bIsFinalData))
					{
						// Chunk needs padding?
						check(UnknownLayerData.Num() > ByteOffset);
						uint64 SizeOfData = FMath::Min<uint64>(BuildPatchServices::ChunkDataSize, UnknownLayerData.Num() - ByteOffset);
						if (SizeOfData < BuildPatchServices::ChunkDataSize)
						{
							UnknownLayerData.SetNumZeroed(ByteOffset + BuildPatchServices::ChunkDataSize);
						}

						// Create data for new chunk.
						uint8* NewChunkData = &UnknownLayerData[ByteOffset];
						FGuid NewChunkGuid = FGuid::NewGuid();
						uint64 NewChunkHash = FRollingHash<BuildPatchServices::ChunkDataSize>::GetHashForDataSet(NewChunkData);
						FSHAHash NewChunkSha;
						FSHA1::HashBuffer(NewChunkData, BuildPatchServices::ChunkDataSize, NewChunkSha.Hash);

						// Save it out.
						ChunkWriter.QueueChunk(NewChunkData, NewChunkGuid, NewChunkHash);
						ChunkShaHashes.Add(NewChunkGuid, NewChunkSha);
						ChunkInventory.FindOrAdd(NewChunkHash).Add(NewChunkGuid);

						// Add to manifest builder.
						FBlockStructure BuildSpaceChunkStructure;
						if (UnknownBuildStructure.SelectSerialBytes(ByteOffset, SizeOfData, BuildSpaceChunkStructure) != SizeOfData)
						{
							// Fatal error if the scanner returned a matched range that doesn't fit inside it's data.
							UE_LOG(LogPatchGeneration, Error, TEXT("New chunk was not within build space structure."));
							return false;
						}
						ManifestBuilder->AddChunkMatch(NewChunkGuid, BuildSpaceChunkStructure);
						UE_LOG(LogPatchGeneration, Verbose, TEXT("Created a new chunk %s with hash %016llX on layer %d. Mapping:%s"), *NewChunkGuid.ToString(), NewChunkHash, Layer, *BuildSpaceChunkStructure.ToString());

						// Remove from tracking.
						UnknownLayerData.RemoveAt(ByteOffset, BuildPatchServices::ChunkDataSize, false);
						FBlockStructure LayerSpaceChunkStructure;
						if (UnknownLayerStructure.SelectSerialBytes(ByteOffset, SizeOfData, LayerSpaceChunkStructure) != SizeOfData)
						{
							// Fatal error if the scanner returned a matched range that doesn't fit inside it's data.
							UE_LOG(LogPatchGeneration, Error, TEXT("New chunk was not within layer space structure."));
							return false;
						}
						UnknownLayerStructure.Remove(LayerSpaceChunkStructure);
						UnknownBuildStructure.Remove(BuildSpaceChunkStructure);
						check(ProcessedCount >= SizeOfData);
						ProcessedCount -= SizeOfData;

						// Check sanity of tracked data
						check(UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownLayerStructure)
							&& UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownBuildStructure));
						check(SizeOfData >= BuildPatchServices::ChunkDataSize || UnknownLayerData.Num() == 0);

						// UnknownLayerBlock is now potentially invalid due to mutation of UnknownLayerStructure so we must loop from beginning.
						UnknownLayerBlock = UnknownLayerStructure.GetHead();
						ByteOffset = 0;
						continue;
					}
					ByteOffset += UnknownLayerBlock->GetSize();
					UnknownLayerBlock = UnknownLayerBlock->GetNext();
				}
			}

			// Use unknown data to make new scanners.
			if (UnknownLayerData.Num() > 0)
			{
				// Make sure there are enough bytes available for a scanner, plus a chunk, so that we know no more chunks will get
				// made from this sequential unknown data.
				uint64 RequiredScannerBytes = ScannerDataSize + BuildPatchServices::ChunkDataSize;
				// We make a scanner when there's enough data, or if we are at the end of data for this layer.
				bool bShouldMakeScanner = (ProcessedCount >= RequiredScannerBytes) || bIsFinalData;
				if (!PatchGenerationHelpers::ScannerArrayFull(Scanners) && bShouldMakeScanner)
				{
					// Create data processor.
					const uint64 SizeToTake = FMath::Min<uint64>(ScannerDataSize, UnknownLayerData.Num());
					bool bIsFinalScanner = bIsFinalData && SizeToTake == UnknownLayerData.Num();
					TArray<uint8> ScannerData;
					ScannerData.Append(UnknownLayerData.GetData(), SizeToTake);
					// Pad scanner data if end of build.
					uint64 PadSize = bIsFinalScanner ? ScannerOverlapSize : 0;
					ScannerData.AddZeroed(PadSize);
					// Grab structure for scanner.
					FBlockStructure BuildStructure;
					if (UnknownBuildStructure.SelectSerialBytes(0, SizeToTake, BuildStructure) != SizeToTake)
					{
						// Fatal error if the tracked results do not align correctly.
						UE_LOG(LogPatchGeneration, Error, TEXT("Tracked unknown build data inconsistent."));
						return false;
					}
					const int32 NextLayer = Layer + 1;
					uint64& DataOffset = LayerToDataOffset.FindOrAdd(NextLayer);
					Scanners.Emplace(new FScannerDetails(NextLayer, DataOffset, bIsFinalScanner, PadSize, MoveTemp(ScannerData), BuildStructure, CloudEnumeration, StatsCollector));
					LayerToScannerCount.FindOrAdd(NextLayer)++;
					UE_LOG(LogPatchGeneration, Verbose, TEXT("Creating scanner on layer %d at %llu. IsFinal:%d."), NextLayer, DataOffset, bIsFinalScanner);
					MaxLayer = FMath::Max<int64>(MaxLayer, NextLayer);
					FStatsCollector::Set(StatLayers, MaxLayer);

					// Remove data we just pulled out from tracking, minus the overlap.
					uint64 RemoveSize = bIsFinalScanner ? SizeToTake : SizeToTake - ScannerOverlapSize;
					DataOffset += RemoveSize;
					FBlockStructure LayerStructure;
					if (UnknownLayerStructure.SelectSerialBytes(0, RemoveSize, LayerStructure) != RemoveSize)
					{
						// Fatal error if the tracked results do not align correctly.
						UE_LOG(LogPatchGeneration, Error, TEXT("Tracked unknown layer data inconsistent."));
						return false;
					}
					BuildStructure.Empty();
					if (UnknownBuildStructure.SelectSerialBytes(0, RemoveSize, BuildStructure) != RemoveSize)
					{
						// Fatal error if the tracked results do not align correctly.
						UE_LOG(LogPatchGeneration, Error, TEXT("Tracked unknown build data inconsistent."));
						return false;
					}
					UnknownLayerData.RemoveAt(0, RemoveSize, false);
					UnknownLayerStructure.Remove(LayerStructure);
					UnknownBuildStructure.Remove(BuildStructure);
					check(ProcessedCount >= RemoveSize);
					ProcessedCount -= RemoveSize;

					// Check sanity of tracked data.
					check(UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownLayerStructure)
						&& UnknownLayerData.Num() == PatchGenerationHelpers::CountSerialBytes(UnknownBuildStructure));
					// Final scanner must have emptied data.
					check(PadSize == 0 || UnknownLayerData.Num() == 0);
				}
			}
		}

		// Set whether we are still processing unknown data.
		int64 UnknownDataAlloc = 0;
		int64 UnknownDataNum = 0;
		for (TPair<int32, TArray<uint8>>& UnknownLayerDataPair : LayerToUnknownLayerData)
		{
			UnknownDataNum += UnknownLayerDataPair.Value.Num();
			UnknownDataAlloc += UnknownLayerDataPair.Value.GetAllocatedSize();
		}
		bHasUnknownData = UnknownDataNum > 0;
		FStatsCollector::Set(StatUnknownDataAlloc, UnknownDataAlloc);
		FStatsCollector::Set(StatUnknownDataNum, UnknownDataNum);

		// Log collected stats.
		GLog->FlushThreadedLogs();
		FStatsCollector::Set(StatTotalTime, FStatsCollector::GetCycles() - StartTime);
		StatsCollector->LogStats(StatsLoggerTimeSeconds);

		// Sleep to allow other threads.
		FPlatformProcess::Sleep(0.01f);
	}
	UE_LOG(LogPatchGeneration, Verbose, TEXT("Scanning complete, waiting for writer thread."));

	// Check that we read some build data
	if (BuildStream->GetBuildSize() == 0)
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("There was no data to process. Please check path: %s."), *Settings.RootDirectory);
		return false;
	}

	// Inform no more chunks.
	ChunkWriter.NoMoreChunks();
	// Wait for chunk writer.
	ChunkWriter.WaitForThread();

	// Collect chunk info for the manifest builder.
	TMap<FGuid, FChunkInfoData> ChunkInfoMap;
	TMap<FGuid, int64> ChunkFileSizes = CloudEnumeration->GetChunkFileSizes();
	ChunkWriter.GetChunkFilesizes(ChunkFileSizes);
	for (const TPair<uint64, TSet<FGuid>>& ChunkInventoryPair : CloudEnumeration->GetChunkInventory())
	{
		TSet<FGuid>& ChunkSet = ChunkInventory.FindOrAdd(ChunkInventoryPair.Key);
		ChunkSet = ChunkSet.Union(ChunkInventoryPair.Value);
	}
	ChunkShaHashes.Append(CloudEnumeration->GetChunkShaHashes());
	for (const TPair<uint64, TSet<FGuid>>& ChunkInventoryPair : ChunkInventory)
	{
		for (const FGuid& ChunkGuid : ChunkInventoryPair.Value)
		{
			if (ChunkShaHashes.Contains(ChunkGuid) && ChunkFileSizes.Contains(ChunkGuid))
			{
				FChunkInfoData& ChunkInfo = ChunkInfoMap.FindOrAdd(ChunkGuid);
				ChunkInfo.Guid = ChunkGuid;
				ChunkInfo.Hash = ChunkInventoryPair.Key;
				FMemory::Memcpy(ChunkInfo.ShaHash.Hash, ChunkShaHashes[ChunkGuid].Hash, FSHA1::DigestSize);
				ChunkInfo.FileSize = ChunkFileSizes[ChunkGuid];
				ChunkInfo.GroupNumber = FCrc::MemCrc32(&ChunkGuid, sizeof(FGuid)) % 100;
			}
		}
	}

	// Finalize the manifest data.
	TArray<FChunkInfoData> ChunkInfoList;
	ChunkInfoMap.GenerateValueArray(ChunkInfoList);
	if (ManifestBuilder->FinalizeData(BuildStream->GetAllFiles(), MoveTemp(ChunkInfoList)) == false)
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("Finalizing manifest failed."));
	}

	// Produce final stats log.
	FStatsCollector::Set(StatTotalTime, FStatsCollector::GetCycles() - StartTime);
	StatsCollector->LogStats();
	uint64 EndTime = FStatsCollector::GetCycles();
	UE_LOG(LogPatchGeneration, Log, TEXT("Completed in %s."), *FPlatformTime::PrettyTime(FStatsCollector::CyclesToSeconds(EndTime - StartTime)));

	// Save manifest out to the cloud directory.
	FString OutputFilename = CloudDirectory / Settings.OutputFilename;
	if (ManifestBuilder->SaveToFile(OutputFilename) == false)
	{
		UE_LOG(LogPatchGeneration, Error, TEXT("Saving manifest failed."));
		return false;
	}
	UE_LOG(LogPatchGeneration, Log, TEXT("Saved manifest to %s."), *OutputFilename);

	return true;
}
