#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectBaseUtility.h"
#include "UObject/Object.h"
#include "BulkData.h"
#include "BlastAsset.generated.h"

struct NvBlastAsset;

UENUM()
enum class EBlastAssetChunkFlags : uint8
{
	None = 0x00,  // None
	Static = 0x01 // Static/Kinematic chunk
};
ENUM_CLASS_FLAGS(EBlastAssetChunkFlags);


/*
	This is the Blast Asset, which is the LL asset.

	Blast has chunks, which can have 1-N subchunks, these are graphics and physics only

*/
UCLASS()
class BLAST_API UBlastAsset : public UObject
{
	GENERATED_UCLASS_BODY()
public:

#if WITH_EDITOR
	/*
		Initialize with NvBlastAsset to keep. It will be serialized into buffer, pointer won't be stored.
	*/
	void CopyFromLoadedAsset(const NvBlastAsset* AssetToCopy, const FGuid& NewAssetGUID = FGuid::NewGuid());
#endif
	/*
		Gets the loaded asset
	*/
	NvBlastAsset* GetLoadedAsset() const;

	/*
		Get assets root chunks
	*/
	const TArray<uint32>& GetRootChunks() const;

	uint32	GetChunkCount() const;

	uint32	GetBondCount() const;

	const struct NvBlastChunk& GetChunkInfo(uint32 ChunkIndex) const;

	bool IsSupportChunk(uint32 ChunkIndex) const;

	uint32 GetChunkDepth(uint32 ChunkIndex) const;

	uint32 GetMaxChunkDepth() const
	{
		return MaxChunkDepth;
	}

	bool IsChunkStatic(uint32 ChunkIndex) const;

	// (!Note) This function also changes states of other chunks:
	// If 'true' passed chunk (ChunkIndex) and all parent chunks till the root become static
	// If 'false' passed chunk (ChunkIndex) and all children chunks become non-static
	void SetChunkStatic(uint32 ChunkIndex, bool IsStatic);


	virtual void PostLoad() override;

	virtual void Serialize(FArchive& Ar) override;

	inline const FGuid& GetAssetGUID() const { return AssetGUID; }

	// NvBlastAsset serialization wrappers
	static NvBlastAsset* DeserializeBlastAsset(const void* buffer, uint64_t bufferSize);
	static uint64 SerializeBlastAsset(void*& buffer, const NvBlastAsset* asset);

private:

	void	DeserializeRawAsset();
	void	Update();

	void	BuildChunkMaxDepth();

	// Per chunk flags
	UPROPERTY()
	TArray<EBlastAssetChunkFlags>			ChunksFlags;

	UPROPERTY()
	FGuid									AssetGUID;

	/*
	This is the raw, serialized data of the asset. It is deserialized into LoadedAsset on demand.
	It's stored in BulkData so we don't waste memory keeping it when it's not needed
	*/
	FByteBulkData							RawAssetData;

	/*
	List of asset's root chunks, updated when asset is loaded
	*/
	TArray<uint32>							RootChunks;

	TArray<uint32>							SupportChunks;

	TArray<uint32>							ChunksDepth;

	uint32									MaxChunkDepth;

	/*
		This is the pointer to the deserialized/loaded asset. This doesn't get populated until required
		or the user tells us to load it.

		This is wrapped in a shared_ptr so that we can call release when it leaves scope.
	*/
	TSharedPtr<NvBlastAsset>				LoadedAsset;

};

