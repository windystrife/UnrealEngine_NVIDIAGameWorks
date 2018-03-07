// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "Generation/ManifestBuilder.h"


DECLARE_LOG_CATEGORY_EXTERN(LogManifestBuilder, Log, All);
DEFINE_LOG_CATEGORY(LogManifestBuilder);

namespace BuildPatchServices
{
	struct FFileBlock
	{
	public:
		FFileBlock(FGuid InChunkGuid, uint64 InFileOffset, uint64 InChunkOffset, uint64 InSize)
			: ChunkGuid(InChunkGuid)
			, FileOffset(InFileOffset)
			, ChunkOffset(InChunkOffset)
			, Size(InSize)
		{}

	public:
		FGuid ChunkGuid;
		uint64 FileOffset;
		uint64 ChunkOffset;
		uint64 Size;
	};

	class FManifestBuilder
		: public IManifestBuilder
	{
	public:
		FManifestBuilder(const FManifestDetails& Details);
		virtual ~FManifestBuilder();

		virtual void AddChunkMatch(const FGuid& ChunkGuid, const FBlockStructure& Structure) override;
		virtual bool FinalizeData(const TArray<FFileSpan>& FileSpans, TArray<FChunkInfoData> ChunkInfo) override;
		virtual bool SaveToFile(const FString& Filename) override;

	private:
		TArray<FChunkPartData> GetChunkPartsForFile(uint64 StartIdx, uint64 Size, TSet<FGuid>& ReferencedChunks);

		FBuildPatchAppManifestRef Manifest;
		TMap<FString, FFileAttributes> FileAttributesMap;
		FBlockStructure BuildStructureAdded;

		TMap<FGuid, TArray<FBlockStructure>> AllMatches;
	};

	FManifestBuilder::FManifestBuilder(const FManifestDetails& InDetails)
		: Manifest(MakeShareable(new FBuildPatchAppManifest()))
		, FileAttributesMap(InDetails.FileAttributesMap)
	{
		Manifest->bIsFileData = false;
		Manifest->AppID = InDetails.AppId;
		Manifest->AppName = InDetails.AppName;
		Manifest->BuildVersion = InDetails.BuildVersion;
		Manifest->LaunchExe = InDetails.LaunchExe;
		Manifest->LaunchCommand = InDetails.LaunchCommand;
		Manifest->PrereqIds = InDetails.PrereqIds;
		Manifest->PrereqName = InDetails.PrereqName;
		Manifest->PrereqPath = InDetails.PrereqPath;
		Manifest->PrereqArgs = InDetails.PrereqArgs;
		for (const auto& CustomField : InDetails.CustomFields)
		{
			int32 VarType = CustomField.Value.GetType();
			if (VarType == EVariantTypes::Float || VarType == EVariantTypes::Double)
			{
				Manifest->SetCustomField(CustomField.Key, (double)CustomField.Value);
			}
			else if (VarType == EVariantTypes::Int8 || VarType == EVariantTypes::Int16 || VarType == EVariantTypes::Int32 || VarType == EVariantTypes::Int64 ||
				VarType == EVariantTypes::UInt8 || VarType == EVariantTypes::UInt16 || VarType == EVariantTypes::UInt32 || VarType == EVariantTypes::UInt64)
			{
				Manifest->SetCustomField(CustomField.Key, (int64)CustomField.Value);
			}
			else if (VarType == EVariantTypes::String)
			{
				Manifest->SetCustomField(CustomField.Key, CustomField.Value.GetValue<FString>());
			}
		}
	}

	FManifestBuilder::~FManifestBuilder()
	{
	}

	void FManifestBuilder::AddChunkMatch(const FGuid& ChunkGuid, const FBlockStructure& Structure)
	{
		// Make sure there is no intersection as that is not allowed.
		check(BuildStructureAdded.Intersect(Structure).GetHead() == nullptr);
		// Track full build matched.
		BuildStructureAdded.Add(Structure);
		// Add match to map. One chunk can have multiple matches.
		AllMatches.FindOrAdd(ChunkGuid).Add(Structure);
		UE_LOG(LogManifestBuilder, Verbose, TEXT("Match added for chunk %s."), *ChunkGuid.ToString());
	}

	bool FManifestBuilder::FinalizeData(const TArray<FFileSpan>& FileSpans, TArray<FChunkInfoData> ChunkInfo)
	{
		// Keep track of referenced chunks so we can trim the list down.
		TSet<FGuid> ReferencedChunks;
		// For each file create its manifest.
		for (const FFileSpan& FileSpan : FileSpans)
		{
			FFileAttributes FileAttributes = FileAttributesMap.FindRef(FileSpan.Filename);
			Manifest->FileManifestList.AddDefaulted();
			FFileManifestData& FileManifest = Manifest->FileManifestList.Last();
			FileManifest.Filename = FileSpan.Filename;
			FMemory::Memcpy(FileManifest.FileHash.Hash, FileSpan.SHAHash.Hash, FSHA1::DigestSize);
			FileManifest.InstallTags = FileAttributes.InstallTags.Array();
			FileManifest.bIsUnixExecutable = FileAttributes.bUnixExecutable || FileSpan.IsUnixExecutable;
			FileManifest.SymlinkTarget = FileSpan.SymlinkTarget;
			FileManifest.bIsReadOnly = FileAttributes.bReadOnly;
			FileManifest.bIsCompressed = FileAttributes.bCompressed;
			FileManifest.FileChunkParts = GetChunkPartsForFile(FileSpan.StartIdx, FileSpan.Size, ReferencedChunks);
			FileManifest.Init();
			check(FileManifest.GetFileSize() == FileSpan.Size);
		}
		UE_LOG(LogManifestBuilder, Verbose, TEXT("Manifest references %d chunks."), ReferencedChunks.Num());

		// Setup chunk list, removing all that were not referenced.
		Manifest->ChunkList = MoveTemp(ChunkInfo);
		int32 TotalChunkListNum = Manifest->ChunkList.Num();
		Manifest->ChunkList.RemoveAll([&](FChunkInfoData& Candidate){ return ReferencedChunks.Contains(Candidate.Guid) == false; });
		UE_LOG(LogManifestBuilder, Verbose, TEXT("Chunk info list trimmed from %d to %d."), TotalChunkListNum, Manifest->ChunkList.Num());

		// Init the manifest, and we are done.
		Manifest->InitLookups();

		// Sanity check all chunk info was provided
		bool bHasAllInfo = true;
		for (const FGuid& ReferencedChunk : ReferencedChunks)
		{
			uint64 ChunkHash;
			if(Manifest->GetChunkHash(ReferencedChunk, ChunkHash) == false)
			{
				UE_LOG(LogManifestBuilder, Error, TEXT("Generated manifest is missing ChunkInfo for chunk %s."), *ReferencedChunk.ToString());
				bHasAllInfo = false;
			}
		}
		if (bHasAllInfo == false)
		{
			return false;
		}

		// Insert the legacy SHA-based prereq id if we have a prereq path specified but no prereq id.
		if (Manifest->PrereqIds.Num() == 0 && !Manifest->PrereqPath.IsEmpty())
		{
			UE_LOG(LogManifestBuilder, Log, TEXT("Setting PrereqIds to be the SHA hash of the PrereqPath."));
			FSHAHashData PrereqHash;
			Manifest->GetFileHash(Manifest->PrereqPath, PrereqHash);
			Manifest->PrereqIds.Add(PrereqHash.ToString());
		}

		// Some sanity checks for build integrity.
		if (BuildStructureAdded.GetHead() == nullptr || BuildStructureAdded.GetHead()->GetNext() != nullptr)
		{
			UE_LOG(LogManifestBuilder, Error, TEXT("Build structure added was not whole or complete."));
			return false;
		}
		if (BuildStructureAdded.GetHead()->GetSize() != Manifest->GetBuildSize())
		{
			UE_LOG(LogManifestBuilder, Error, TEXT("Generated manifest build size did not equal build structure added."));
			return false;
		}

		// Everything seems fine.
		return true;
	}

	bool FManifestBuilder::SaveToFile(const FString& Filename)
	{
		// Previous validation from FinaliseData, but this time we assert if fail as the error should have been picked up.
		checkf(BuildStructureAdded.GetHead() != nullptr && BuildStructureAdded.GetHead()->GetNext() == nullptr, TEXT("Build integrity check failed. No structure was added."));
		checkf(BuildStructureAdded.GetHead()->GetSize() == Manifest->GetBuildSize(), TEXT("Build integrity check failed. Structure added is not the same size as the manifest data setup; did you call FinalizeData?"));

		// Currently we only save out in JSON format
		Manifest->ManifestFileVersion = EBuildPatchAppManifestVersion::GetLatestJsonVersion();
		return Manifest->SaveToFile(Filename, false);
	}

	TArray<FChunkPartData> FManifestBuilder::GetChunkPartsForFile(uint64 FileStart, uint64 FileSize, TSet<FGuid>& ReferencedChunks)
	{
		TArray<FChunkPartData> FileChunkParts;
		// Collect all matching blocks.
		TArray<FFileBlock> MatchingBlocks;
		uint64 FileEnd = FileStart + FileSize;
		uint64 SizeCountCheck = 0;
		for (const TPair<FGuid, TArray<FBlockStructure>>& Match : AllMatches)
		{
			for (const FBlockStructure& BlockStructure : Match.Value)
			{
				const FBlockEntry* BlockEntry = BlockStructure.GetHead();
				uint64 ChunkOffset = 0;
				while (BlockEntry != nullptr)
				{
					uint64 BlockEnd = BlockEntry->GetOffset() + BlockEntry->GetSize();
					if (BlockEntry->GetOffset() < FileEnd && BlockEnd > FileStart)
					{
						uint64 IntersectStart = FMath::Max<uint64>(BlockEntry->GetOffset(), FileStart);
						uint64 IntersectEnd = FMath::Min<uint64>(BlockEnd, FileEnd);
						uint64 IntersectSize = IntersectEnd - IntersectStart;
						ChunkOffset += IntersectStart - BlockEntry->GetOffset();
						check(IntersectSize > 0);
						SizeCountCheck += IntersectSize;
						MatchingBlocks.Emplace(Match.Key, IntersectStart, ChunkOffset, IntersectSize);
						ReferencedChunks.Add(Match.Key);
						ChunkOffset += BlockEntry->GetSize() - (IntersectStart - BlockEntry->GetOffset());
					}
					else
					{
						ChunkOffset += BlockEntry->GetSize();
					}
					BlockEntry = BlockEntry->GetNext();
				}
			}
		}
		check(SizeCountCheck == FileSize);

		// Sort the matches by file position.
		struct FFileBlockSort
		{
			FORCEINLINE bool operator()(const FFileBlock& A, const FFileBlock& B) const
			{
				return A.FileOffset < B.FileOffset;
			}
		};
		MatchingBlocks.Sort(FFileBlockSort());

		// Add the info to the return array.
		for (const FFileBlock& MatchingBlock : MatchingBlocks)
		{
			FileChunkParts.AddDefaulted();
			FChunkPartData& ChunkPart = FileChunkParts.Last();
			ChunkPart.Guid = MatchingBlock.ChunkGuid;
			ChunkPart.Offset = MatchingBlock.ChunkOffset;
			ChunkPart.Size = MatchingBlock.Size;
		}
		return FileChunkParts;
	}

	IManifestBuilderRef FManifestBuilderFactory::Create(const FManifestDetails& Details)
	{
		return MakeShareable(new FManifestBuilder(Details));
	}
}
