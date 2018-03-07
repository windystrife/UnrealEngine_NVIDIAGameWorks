// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/** 
 * Playable sound object for raw wave files
 */

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Async/AsyncWork.h"
#include "Sound/SoundBase.h"
#include "Serialization/BulkData.h"
#include "Sound/SoundGroups.h"
#include "AudioMixerTypes.h"

#include "SoundWave.generated.h"

class ITargetPlatform;
struct FActiveSound;
struct FSoundParseParameters;

UENUM()
enum EDecompressionType
{
	DTYPE_Setup,
	DTYPE_Invalid,
	DTYPE_Preview,
	DTYPE_Native,
	DTYPE_RealTime,
	DTYPE_Procedural,
	DTYPE_Xenon,
	DTYPE_Streaming,
	DTYPE_MAX,
};

/**
 * A chunk of streamed audio.
 */
struct FStreamedAudioChunk
{
	/** Size of the chunk of data in bytes */
	int32 DataSize;

	/** Bulk data if stored in the package. */
	FByteBulkData BulkData;

	/** Default constructor. */
	FStreamedAudioChunk()
	{
	}

	/** Serialization. */
	void Serialize(FArchive& Ar, UObject* Owner, int32 ChunkIndex);

#if WITH_EDITORONLY_DATA
	/** Key if stored in the derived data cache. */
	FString DerivedDataKey;

	/**
	 * Place chunk data in the derived data cache associated with the provided
	 * key.
	 */
	uint32 StoreInDerivedDataCache(const FString& InDerivedDataKey);
#endif // #if WITH_EDITORONLY_DATA
};

/**
 * Platform-specific data used streaming audio at runtime.
 */
USTRUCT()
struct FStreamedAudioPlatformData
{
	GENERATED_USTRUCT_BODY()

	/** Number of audio chunks. */
	int32 NumChunks;
	/** Format in which audio chunks are stored. */
	FName AudioFormat;
	/** audio data. */
	TIndirectArray<struct FStreamedAudioChunk> Chunks;

#if WITH_EDITORONLY_DATA
	/** The key associated with this derived data. */
	FString DerivedDataKey;
	/** Async cache task if one is outstanding. */
	struct FStreamedAudioAsyncCacheDerivedDataTask* AsyncTask;
#endif

	/** Default constructor. */
	FStreamedAudioPlatformData();

	/** Destructor. */
	~FStreamedAudioPlatformData();

	/**
	 * Try to load audio chunk from the derived data cache.
	 * @param ChunkIndex	The Chunk index to load.
	 * @param OutChunkData	Address of pointer that will store chunk data - should
	 *						either be NULL or have enough space for the chunk
	 * @returns true if requested chunk has been loaded.
	 */
	bool TryLoadChunk(int32 ChunkIndex, uint8** OutChunkData);

	/** Serialization. */
	void Serialize(FArchive& Ar, class USoundWave* Owner);

#if WITH_EDITORONLY_DATA
	void Cache(class USoundWave& InSoundWave, FName AudioFormatName, uint32 InFlags);
	void FinishCache();
	bool IsFinishedCache() const;
	ENGINE_API bool TryInlineChunkData();
	bool AreDerivedChunksAvailable() const;
#endif

};

UCLASS(hidecategories=Object, editinlinenew, BlueprintType)
class ENGINE_API USoundWave : public USoundBase
{
	GENERATED_UCLASS_BODY()

	/** Platform agnostic compression quality. 1..100 with 1 being best compression and 100 being best quality. */
	UPROPERTY(EditAnywhere, Category=Compression, meta=(ClampMin = "1", ClampMax = "100"), AssetRegistrySearchable)
	int32 CompressionQuality;

	/** If set, when played directly (not through a sound cue) the wave will be played looping. */
	UPROPERTY(EditAnywhere, Category=SoundWave, AssetRegistrySearchable)
	uint32 bLooping:1;

	/** Whether this sound can be streamed to avoid increased memory usage */
	UPROPERTY(EditAnywhere, Category=Streaming)
	uint32 bStreaming:1;

	/** Priority of this sound when streaming (lower priority streams may not always play) */
	UPROPERTY(EditAnywhere, Category=Streaming, meta=(ClampMin=0))
	int32 StreamingPriority;

	/** Set to true for programmatically-generated, streamed audio. */
	uint32 bProcedural:1;

	/** Set to true of this is a bus sound source. This will result in the sound wave not generating audio for itself, but generate audio through instances. Used only in audio mixer. */
	uint32 bIsBus:1;

	/** Set to true for procedural waves that can be processed asynchronously. */
	uint32 bCanProcessAsync:1;

	/** Whether to free the resource data after it has been uploaded to the hardware */
	uint32 bDynamicResource:1;

	/** If set to true if this sound is considered to contain mature/adult content. */
	UPROPERTY(EditAnywhere, Category=Subtitles, AssetRegistrySearchable)
	uint32 bMature:1;

	/** If set to true will disable automatic generation of line breaks - use if the subtitles have been split manually. */
	UPROPERTY(EditAnywhere, Category=Subtitles )
	uint32 bManualWordWrap:1;

	/** If set to true the subtitles display as a sequence of single lines as opposed to multiline. */
	UPROPERTY(EditAnywhere, Category=Subtitles )
	uint32 bSingleLine:1;

	/** Allows sound to play at 0 volume, otherwise will stop the sound when the sound is silent. */
	UPROPERTY(EditAnywhere, Category=Sound)
	uint32 bVirtualizeWhenSilent:1;

	/** Whether this SoundWave was decompressed from OGG. */
	uint32 bDecompressedFromOgg:1;

	UPROPERTY(EditAnywhere, Category=Sound)
	TEnumAsByte<ESoundGroup> SoundGroup;

	/** A localized version of the text that is actually spoken phonetically in the audio. */
	UPROPERTY(EditAnywhere, Category=Subtitles )
	FString SpokenText;

	/** The priority of the subtitle. */
	UPROPERTY(EditAnywhere, Category=Subtitles)
	float SubtitlePriority;

	/** Playback volume of sound 0 to 1 - Default is 1.0. */
	UPROPERTY(Category=Sound, meta=(ClampMin = "0.0"), EditAnywhere)
	float Volume;

	/** Playback pitch for sound. */
	UPROPERTY(Category=Sound, meta=(ClampMin = "0.125", ClampMax = "4.0"), EditAnywhere)
	float Pitch;

	/** Number of channels of multichannel data; 1 or 2 for regular mono and stereo files */
	UPROPERTY(Category=Info, AssetRegistrySearchable, VisibleAnywhere)
	int32 NumChannels;

	/** Cached sample rate for displaying in the tools */
	UPROPERTY(Category=Info, AssetRegistrySearchable, VisibleAnywhere)
	int32 SampleRate;

#if WITH_EDITORONLY_DATA
	/** Offsets into the bulk data for the source wav data */
	UPROPERTY()
	TArray<int32> ChannelOffsets;

	/** Sizes of the bulk data for the source wav data */
	UPROPERTY()
	TArray<int32> ChannelSizes;

#endif // WITH_EDITORONLY_DATA

	/** Size of RawPCMData, or what RawPCMData would be if the sound was fully decompressed */
	UPROPERTY()
	int32 RawPCMDataSize;

	/**
	 * Subtitle cues.  If empty, use SpokenText as the subtitle.  Will often be empty,
	 * as the contents of the subtitle is commonly identical to what is spoken.
	 */
	UPROPERTY(EditAnywhere, Category=Subtitles)
	TArray<struct FSubtitleCue> Subtitles;

#if WITH_EDITORONLY_DATA
	/** Provides contextual information for the sound to the translator. */
	UPROPERTY(EditAnywhere, Category=Subtitles )
	FString Comment;

#endif // WITH_EDITORONLY_DATA

	/**
	 * The array of the subtitles for each language. Generated at cook time.
	 */
	UPROPERTY()
	TArray<struct FLocalizedSubtitle> LocalizedSubtitles;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FString SourceFilePath_DEPRECATED;
	UPROPERTY()
	FString SourceFileTimestamp_DEPRECATED;

	UPROPERTY(VisibleAnywhere, Instanced, Category=ImportSettings)
	class UAssetImportData* AssetImportData;
	
#endif // WITH_EDITORONLY_DATA

	/** Curves associated with this sound wave */
	UPROPERTY(EditAnywhere, Category = SoundWave, AdvancedDisplay)
	class UCurveTable* Curves;

	/** Hold a reference to our internal curve so we can switch back to it if we want to */
	UPROPERTY()
	class UCurveTable* InternalCurves;

public:	
	/** Async worker that decompresses the audio data on a different thread */
	typedef FAsyncTask< class FAsyncAudioDecompressWorker > FAsyncAudioDecompress;	// Forward declare typedef
	FAsyncAudioDecompress*		AudioDecompressor;

	/** Whether or not the the precache task has finished. */
	FThreadSafeBool				bIsPrecacheDone;

	/** Pointer to 16 bit PCM data - used to avoid synchronous operation to obtain first block of the realtime decompressed buffer */
	uint8*						CachedRealtimeFirstBuffer;

	/** Pointer to 16 bit PCM data - used to decompress data to and preview sounds */
	uint8*						RawPCMData;

	/** Memory containing the data copied from the compressed bulk data */
	uint8*						ResourceData;

	/** Uncompressed wav data 16 bit in mono or stereo - stereo not allowed for multichannel data */
	FByteBulkData				RawData;

	/** GUID used to uniquely identify this node so it can be found in the DDC */
	FGuid						CompressedDataGuid;

	FFormatContainer			CompressedFormatData;

	/** Type of buffer this wave uses. Set once on load */
	TEnumAsByte<enum EDecompressionType> DecompressionType;

	/** Resource index to cross reference with buffers */
	int32 ResourceID;

	/** Size of resource copied from the bulk data */
	int32 ResourceSize;

	/** Cache the total used memory recorded for this SoundWave to keep INC/DEC consistent */
	int32 TrackedMemoryUsage;

	/** The streaming derived data for this sound on this platform. */
	FStreamedAudioPlatformData *RunningPlatformData;

	/** cooked streaming platform data for this sound */
	TMap<FString, FStreamedAudioPlatformData*> CookedPlatformData;

	//~ Begin UObject Interface. 
	virtual void Serialize( FArchive& Ar ) override;
	virtual void PostInitProperties() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual void FinishDestroy() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;	
#endif // WITH_EDITOR
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual FName GetExporterName() override;
	virtual FString GetDesc() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	//~ End UObject Interface. 

	//~ Begin USoundBase Interface.
	virtual bool IsPlayable() const override;
	virtual void Parse( class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances ) override;
	virtual float GetMaxAudibleDistance() override;
	virtual float GetDuration() override;
	virtual float GetSubtitlePriority() const override;
	//~ End USoundBase Interface.

	/**
	 *	@param		Format		Format to check
	 *
	 *	@return		Sum of the size of waves referenced by this cue for the given platform.
	 */
	virtual int32 GetResourceSizeForFormat(FName Format);

	/**
	 * Frees up all the resources allocated in this class
	 */
	void FreeResources();

	/** 
	 * Copy the compressed audio data from the bulk data
	 */
	virtual void InitAudioResource( FByteBulkData& CompressedData );

	/** 
	 * Copy the compressed audio data from derived data cache
	 *
	 * @param Format to get the compressed audio in
	 * @return true if the resource has been successfully initialized or it was already initialized.
	 */
	virtual bool InitAudioResource(FName Format);

	/** 
	 * Remove the compressed audio data associated with the passed in wave
	 */
	void RemoveAudioResource();

	/** 
	 * Prints the subtitle associated with the SoundWave to the console
	 */
	void LogSubtitle( FOutputDevice& Ar );

	/** 
	 * Handle any special requirements when the sound starts (e.g. subtitles)
	 */
	FWaveInstance* HandleStart( FActiveSound& ActiveSound, const UPTRINT WaveInstanceHash ) const;

	/** 
	 * This is only used for DTYPE_Procedural audio. It's recommended to use USynthComponent base class
	 * for procedurally generated sound vs overriding this function. If a new component is not feasible,
	 * consider using USoundWaveProcedural base class vs USoundWave base class since as it implements
	 * GeneratePCMData for you and you only need to return PCM data.
	 */
	virtual int32 GeneratePCMData(uint8* PCMData, const int32 SamplesNeeded) { ensure(false); return 0; }

	/** 
	* Return the format of the generated PCM data type. Used in audio mixer to allow generating float buffers and avoid unnecessary format conversions. 
	* This feature is only supported in audio mixer. If your procedural sound wave needs to be used in both audio mixer and old audio engine,
	* it's best to generate int16 data as old audio engine only supports int16 formats. Or check at runtime if the audio mixer is enabled.
	* Audio mixer will convert from int16 to float internally.
	*/
	virtual Audio::EAudioMixerStreamDataFormat::Type GetGeneratedPCMDataFormat() const { return Audio::EAudioMixerStreamDataFormat::Int16; }

	/** 
	 * Gets the compressed data size from derived data cache for the specified format
	 * 
	 * @param Format	format of compressed data
	 * @return			compressed data size, or zero if it could not be obtained
	 */ 
	int32 GetCompressedDataSize(FName Format)
	{
		FByteBulkData* Data = GetCompressedData(Format);
		return Data ? Data->GetBulkDataSize() : 0;
	}

	virtual bool HasCompressedData(FName Format) const;

	/** 
	 * Gets the compressed data from derived data cache for the specified platform
	 * Warning, the returned pointer isn't valid after we add new formats
	 * 
	 * @param Format	format of compressed data
	 * @return	compressed data, if it could be obtained
	 */ 
	virtual FByteBulkData* GetCompressedData(FName Format);

	/** 
	 * Change the guid and flush all compressed data
	 */ 
	void InvalidateCompressedData();

	/**
	 * Checks whether sound has been categorised as streaming
	 */
	bool IsStreaming() const;

	/**
	 * Attempts to update the cached platform data after any changes that might affect it
	 */
	void UpdatePlatformData();

	void CleanupCachedRunningPlatformData();

	/**
	 * Serializes cooked platform data.
	 */
	void SerializeCookedPlatformData(class FArchive& Ar);

#if WITH_EDITORONLY_DATA
	
#if WITH_EDITOR
	/**
	 * Begins caching platform data in the background for the platform requested
	 */
	virtual void BeginCacheForCookedPlatformData(  const ITargetPlatform *TargetPlatform ) override;

	virtual bool IsCachedCookedPlatformDataLoaded( const ITargetPlatform* TargetPlatform ) override;

	/**
	 * Clear all the cached cooked platform data which we have accumulated with BeginCacheForCookedPlatformData calls
	 * The data can still be cached again using BeginCacheForCookedPlatformData again
	 */
	virtual void ClearAllCachedCookedPlatformData() override;

	virtual void ClearCachedCookedPlatformData( const ITargetPlatform* TargetPlatform ) override;

	virtual void WillNeverCacheCookedPlatformDataAgain() override;

	uint32 bNeedsThumbnailGeneration:1;
#endif
	
	/**
	 * Caches platform data for the sound.
	 */
	void CachePlatformData(bool bAsyncCache = false);

	/**
	 * Begins caching platform data in the background.
	 */
	void BeginCachePlatformData();

	/**
	 * Blocks on async cache tasks and prepares platform data for use.
	 */
	void FinishCachePlatformData();

	/**
	 * Forces platform data to be rebuilt.
	 */
	void ForceRebuildPlatformData();
#endif

	/**
	 * Get Chunk data for a specified chunk index.
	 * @param ChunkIndex	The Chunk index to cache.
	 * @param OutChunkData	Address of pointer that will store data.
	 */
	bool GetChunkData(int32 ChunkIndex, uint8** OutChunkData);

private:

	enum class ESoundWaveResourceState
	{
		NeedsFree,
		Freeing,
		Freed
	};

	ESoundWaveResourceState ResourceState;

};



