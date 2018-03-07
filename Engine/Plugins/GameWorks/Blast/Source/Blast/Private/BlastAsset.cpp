#include "BlastAsset.h"
#include "BlastModule.h"
#include "NvBlastTypes.h"
#include "NvBlastExtSerialization.h"
#include "NvBlastExtLlSerialization.h"
#include "NvBlast.h"
#include "NvBlastGlobals.h"

#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/CustomVersion.h"
#include "Containers/Queue.h"

UBlastAsset::UBlastAsset(const FObjectInitializer& ObjectInitializer):
	Super(ObjectInitializer)
{
	RawAssetData.SetBulkDataFlags(BULKDATA_SerializeCompressed);
}

NvBlastAsset* UBlastAsset::DeserializeBlastAsset(const void* buffer, uint64_t bufferSize)
{
	Nv::Blast::ExtSerialization* Serialization = NvBlastExtSerializationCreate();
	uint32_t objectTypeIDPtr = 0;
	NvBlastAsset* Asset = (NvBlastAsset*)Serialization->deserializeFromBuffer(buffer, bufferSize, &objectTypeIDPtr);
	Serialization->release();
	return Asset;
}

uint64 UBlastAsset::SerializeBlastAsset(void*& buffer, const NvBlastAsset* asset)
{
	Nv::Blast::ExtSerialization* Serialization = NvBlastExtSerializationCreate();
	uint64 size = Serialization->serializeIntoBuffer(buffer, asset, Nv::Blast::LlObjectTypeID::Asset);
	check(size > 0);
	Serialization->release();
	return size;
}

#if WITH_EDITOR

void UBlastAsset::CopyFromLoadedAsset(const NvBlastAsset* AssetToCopy, const FGuid& NewAssetGUID)
{
	// Serialize modified asset into buffer and put it into the new UBlastAsset
	void *Buffer;
	
	uint32 BufferSize = SerializeBlastAsset(Buffer, AssetToCopy);
	if (BufferSize)
	{
		RawAssetData.Lock(LOCK_READ_WRITE);
		void* DestMemory = RawAssetData.Realloc(BufferSize);
		FMemory::Memcpy(DestMemory, Buffer, BufferSize);
		RawAssetData.Unlock();

		//We can't lock the raw asset until we save so we can't call DeserializeRawAsset
		LoadedAsset = TSharedPtr<NvBlastAsset>(DeserializeBlastAsset(Buffer, BufferSize), [=](NvBlastAsset* asset)
		{
			NVBLAST_FREE((void*)asset);
		});

		NVBLAST_FREE(Buffer);
	}
	else
	{
		LoadedAsset.Reset();

		//Empty it out
		RawAssetData.Lock(LOCK_READ_WRITE);
		RawAssetData.Realloc(0);
		RawAssetData.Unlock();
	}

	//Our contents changed so assign a new asset GUID
	AssetGUID = NewAssetGUID;

	Update();
}
#endif 

void UBlastAsset::DeserializeRawAsset()
{
	// Load Asset from raw data
	int32 BulkDataSize = RawAssetData.GetBulkDataSize();
	if (BulkDataSize > 0)
	{
		const void* DataPtr = RawAssetData.LockReadOnly();
		if (DataPtr)
		{
			LoadedAsset = TSharedPtr<NvBlastAsset>(DeserializeBlastAsset(DataPtr, BulkDataSize), [=](NvBlastAsset* asset)
			{
				NVBLAST_FREE((void*)asset);
			});
		}
		else
		{
			LoadedAsset.Reset();
		}
		RawAssetData.Unlock();
		if (!LoadedAsset.IsValid())
		{
			RawAssetData.RemoveBulkData();
		}
	}
	else
	{
		LoadedAsset.Reset();
	}

	Update();
}


void UBlastAsset::Update()
{
	// Fill RootChunks with chunk indices
	RootChunks.Reset();

	if (LoadedAsset.IsValid())
	{
		const uint32 chunkCount = GetChunkCount();

		static_assert((int)EBlastAssetChunkFlags::None == 0, "ChunkFlags resize needs to have non-zero initialization");
		ChunksFlags.SetNumZeroed(chunkCount);

		const NvBlastChunk* chunks = NvBlastAssetGetChunks(LoadedAsset.Get(), Nv::Blast::logLL);
		for (uint32 i = 0; i < chunkCount; i++)
		{
			if (chunks[i].parentChunkIndex == 0xFFFFFFFF)
			{
				RootChunks.Add(i);
			}
		}

		auto& graph = NvBlastAssetGetSupportGraph(LoadedAsset.Get(), Nv::Blast::logLL);
		SupportChunks.Reset(graph.nodeCount);
		for (uint32_t i = 0; i < graph.nodeCount; i++)
		{
			SupportChunks.Add(graph.chunkIndices[i]);
		}
	}
	BuildChunkMaxDepth();
}

NvBlastAsset* UBlastAsset::GetLoadedAsset() const
{
	return LoadedAsset.Get();
}

const TArray<uint32>& UBlastAsset::GetRootChunks() const
{
	return RootChunks;
}

uint32 UBlastAsset::GetChunkCount()  const
{
	if (LoadedAsset.IsValid())
	{
		return NvBlastAssetGetChunkCount(LoadedAsset.Get(), Nv::Blast::logLL);
	}
	return 0;
}

uint32 UBlastAsset::GetBondCount()  const
{
	if (LoadedAsset.IsValid())
	{
		return NvBlastAssetGetBondCount(LoadedAsset.Get(), Nv::Blast::logLL);
	}
	return 0;
}

void UBlastAsset::BuildChunkMaxDepth()
{
	MaxChunkDepth = 0;
	ChunksDepth.Reset();
	if (LoadedAsset.IsValid())
	{
		NvBlastAsset* LLBlastAsset = GetLoadedAsset();
		const NvBlastChunk* chunks = NvBlastAssetGetChunks(LLBlastAsset, Nv::Blast::logLL);
		const uint32 chunkCount = GetChunkCount();

		if ((uint32)ChunksDepth.Num() < chunkCount)
		{
			ChunksDepth.SetNum(chunkCount);
		}

		for (uint32 ChunkIndex = 0; ChunkIndex < chunkCount; ++ChunkIndex)
		{
			uint32 ChunkDepth = 0;
			const NvBlastChunk* chunk = &(chunks[ChunkIndex]);
			while (chunk->parentChunkIndex != UINT32_MAX)
			{
				chunk = &(chunks[chunk->parentChunkIndex]);
				ChunkDepth++;
			}
			MaxChunkDepth = FMath::Max(MaxChunkDepth, ChunkDepth);
			ChunksDepth[ChunkIndex] = ChunkDepth;
		}
	}
}

const NvBlastChunk& UBlastAsset::GetChunkInfo(uint32 ChunkIndex) const
{
	check(ChunkIndex < GetChunkCount());
	const NvBlastChunk* chunks = NvBlastAssetGetChunks(LoadedAsset.Get(), Nv::Blast::logLL);
	return chunks[ChunkIndex];
}

bool UBlastAsset::IsSupportChunk(uint32 ChunkIndex) const
{
	return SupportChunks.Find(ChunkIndex) != INDEX_NONE;
}

uint32 UBlastAsset::GetChunkDepth(uint32 ChunkIndex) const
{
	check(ChunkIndex < GetChunkCount());
	return ChunksDepth[ChunkIndex];
}

bool UBlastAsset::IsChunkStatic(uint32 ChunkIndex) const
{
	check(ChunkIndex < GetChunkCount());
	return (!!(ChunksFlags[ChunkIndex] & EBlastAssetChunkFlags::Static));
}

void UBlastAsset::SetChunkStatic(uint32 ChunkIndex, bool IsStatic)
{
	if (IsChunkStatic(ChunkIndex) == IsStatic)
		return;

	const NvBlastAsset* LLBlastAsset = GetLoadedAsset();
	const NvBlastChunk* chunks = NvBlastAssetGetChunks(LLBlastAsset, Nv::Blast::logLL);

	if (IsStatic)
	{
		// Mark this chunk and all parent chunks till the root as static
		for(uint32 index = ChunkIndex; index != UINT32_MAX; index = chunks[index].parentChunkIndex)
		{
			ChunksFlags[index] |= EBlastAssetChunkFlags::Static;
		}
	}
	else
	{
		// Traverse and remove static flag recursively for all children chunks
		TQueue<uint32> ChunkQueue;
		ChunkQueue.Enqueue(ChunkIndex);
		uint32 index;
		while (ChunkQueue.Dequeue(index))
		{
			ChunksFlags[index] &= ~EBlastAssetChunkFlags::Static;

			for (uint32 i = chunks[index].firstChildIndex; i < chunks[index].childIndexStop; i++)
			{
				ChunkQueue.Enqueue(i);
			}
		}
	}
}


void UBlastAsset::PostLoad()
{
	Super::PostLoad();
	DeserializeRawAsset();
}

//The value of this is not important, it's just used to tag our version code
static FGuid BlastAssetDataFormatGUID(0x648A6305, 0x343D4537, 0x98F6EF84, 0xE044E371);

enum EBlastAssetDataFormatVersion 
{
	EBlastAssetDataFormatVersion_Initial = 1,
	EBlastAssetDataFormatVersion_AddedAssetGUID,
};

FCustomVersionRegistration GRegisterUBlastAssetDataFormat(BlastAssetDataFormatGUID, EBlastAssetDataFormatVersion_AddedAssetGUID, TEXT("BlastAssetVer"));

void UBlastAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	
	//Not used for anything not crashing when reading old files, but be future proof
	Ar.UsingCustomVersion(BlastAssetDataFormatGUID);

	int32 ArchiveVersion = EBlastAssetDataFormatVersion_Initial;
	if (Ar.IsLoading())
	{
		ArchiveVersion = Ar.CustomVer(BlastAssetDataFormatGUID);

		if (ArchiveVersion < EBlastAssetDataFormatVersion_AddedAssetGUID)
		{
			//We don't have an existing one so make one
			AssetGUID = FGuid::NewGuid();
		}

		if (ArchiveVersion >= EBlastAssetDataFormatVersion_Initial)
		{
			RawAssetData.Serialize(Ar, this);
		}
	}
	else
	{
		if (Ar.IsCooking())
		{
			//We are writing a cooked asset, the code will only ever call DeserializeRawAsset once during post load
			RawAssetData.SetBulkDataFlags(BULKDATA_ForceInlinePayload | BULKDATA_SingleUse);
		}
		RawAssetData.Serialize(Ar, this);
	}
}
