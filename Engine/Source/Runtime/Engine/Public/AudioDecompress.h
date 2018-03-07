// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AudioDecompress.h: Unreal audio vorbis decompression interface object.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Async/AsyncWork.h"
#include "Sound/SoundWave.h"
#include "Misc/ScopeLock.h"
#include "HAL/LowLevelMemTracker.h"

// 186ms of 44.1KHz data
// 372ms of 22KHz data
#define MONO_PCM_BUFFER_SAMPLES		8192
#define MONO_PCM_BUFFER_SIZE		( MONO_PCM_BUFFER_SAMPLES * sizeof( int16 ) )

struct FSoundQualityInfo;

/**
 * Interface class to decompress various types of audio data
 */
class ICompressedAudioInfo
{
public:
	/**
	* Virtual destructor.
	*/
	virtual ~ICompressedAudioInfo() { }

	/**
	* Reads the header information of a compressed format
	*
	* @param	InSrcBufferData		Source compressed data
	* @param	InSrcBufferDataSize	Size of compressed data
	* @param	QualityInfo			Quality Info (to be filled out)
	*/
	ENGINE_API virtual bool ReadCompressedInfo(const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, struct FSoundQualityInfo* QualityInfo) = 0;

	/**
	* Decompresses data to raw PCM data.
	*
	* @param	Destination	where to place the decompressed sound
	* @param	bLooping	whether to loop the sound by seeking to the start, or pad the buffer with zeroes
	* @param	BufferSize	number of bytes of PCM data to create
	*
	* @return	bool		true if the end of the data was reached (for both single shot and looping sounds)
	*/
	ENGINE_API virtual bool ReadCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize) = 0;

	/**
	 * Seeks to time (Some formats might not be seekable)
	 */
	virtual void SeekToTime(const float SeekTime) = 0;

	/**
	* Decompress an entire data file to a TArray
	*/
	ENGINE_API virtual void ExpandFile(uint8* DstBuffer, struct FSoundQualityInfo* QualityInfo) = 0;

	/**
	* Sets decode to half-rate
	*
	* @param	HalfRate	Whether Half rate is enabled
	*/
	ENGINE_API virtual void EnableHalfRate(bool HalfRate) = 0;

	/**
	 * Gets the size of the source buffer originally passed to the info class (bytes)
	 */
	virtual uint32 GetSourceBufferSize() const = 0;

	/**
	 * Whether the decompressed audio will be arranged using Vorbis' channel ordering
	 * See http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-800004.3.9 for details
	 */
	virtual bool UsesVorbisChannelOrdering() const = 0;

	/**
	* Gets the preferred size for a streaming buffer for this decompression scheme
	*/
	virtual int GetStreamBufferSize() const = 0;

	////////////////////////////////////////////////////////////////
	// Following functions are optional if streaming is supported //
	////////////////////////////////////////////////////////////////

	/**
	 * Whether this decompression class supports streaming decompression
	 */
	virtual bool SupportsStreaming() const {return false;}

	/**
	* Streams the header information of a compressed format
	*
	* @param	Wave			Wave that will be read from to retrieve necessary chunk
	* @param	QualityInfo		Quality Info (to be filled out)
	*/
	virtual bool StreamCompressedInfo(USoundWave* Wave, struct FSoundQualityInfo* QualityInfo) {return false;}

	/**
	* Decompresses streamed data to raw PCM data.
	*
	* @param	Destination	where to place the decompressed sound
	* @param	bLooping	whether to loop the sound by seeking to the start, or pad the buffer with zeroes
	* @param	BufferSize	number of bytes of PCM data to create
	*
	* @return	bool		true if the end of the data was reached (for both single shot and looping sounds)
	*/
	virtual bool StreamCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize) {return false;}

	/**
	 * Gets the chunk index that was last read from (for Streaming Manager requests)
	 */
	virtual int32 GetCurrentChunkIndex() const {return -1;}

	/**
	 * Gets the offset into the chunk that was last read to (for Streaming Manager priority)
	 */
	virtual int32 GetCurrentChunkOffset() const {return -1;}
};

/** Struct used to store the results of a decode operation. **/
struct FDecodeResult
{
	// Number of bytes of compressed data consumed
	int32 NumCompressedBytesConsumed;
	// Number of bytes produced
	int32 NumPcmBytesProduced;
	// Number of frames produced.
	int32 NumAudioFramesProduced;

	FDecodeResult()
		: NumCompressedBytesConsumed(INDEX_NONE)
		, NumPcmBytesProduced(INDEX_NONE)
		, NumAudioFramesProduced(INDEX_NONE)
	{}
};

/** 
 * Default implementation of a streamed compressed audio format.
 * Can be subclassed to support streaming of a specific asset format. Handles all 
 * the platform independent aspects of file format streaming for you (dealing with UE4 streamed assets)
 */
class IStreamedCompressedInfo : public ICompressedAudioInfo
{
public:
	IStreamedCompressedInfo();
	virtual ~IStreamedCompressedInfo() {}

	//~ Begin ICompressedInfo Interface
	ENGINE_API virtual bool ReadCompressedInfo(const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, FSoundQualityInfo* QualityInfo) override;
	ENGINE_API virtual bool ReadCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize) override;
	ENGINE_API virtual void SeekToTime(const float SeekTime) override {};
	ENGINE_API virtual void ExpandFile(uint8* DstBuffer, struct FSoundQualityInfo* QualityInfo) override;
	ENGINE_API virtual void EnableHalfRate(bool HalfRate) override {};
	virtual uint32 GetSourceBufferSize() const override { return SrcBufferDataSize; }
	virtual bool UsesVorbisChannelOrdering() const override { return false; }
	virtual int GetStreamBufferSize() const override { return  MONO_PCM_BUFFER_SIZE; }
	virtual bool SupportsStreaming() const override { return true; }
	virtual bool StreamCompressedInfo(USoundWave* Wave, FSoundQualityInfo* QualityInfo) override;
	virtual bool StreamCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize) override;
	virtual int32 GetCurrentChunkIndex() const override { return CurrentChunkIndex; }
	virtual int32 GetCurrentChunkOffset() const override { return SrcBufferOffset; }
	//~ End ICompressedInfo Interface

	/** Parse the header information from the input source buffer data. This is dependent on compression format. */
	virtual bool ParseHeader(const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, FSoundQualityInfo* QualityInfo) = 0;

	/** Create the compression format dependent decoder object. */
	virtual bool CreateDecoder() = 0;

	/** Decode the input compressed frame data into output PCMData buffer. */
	virtual FDecodeResult Decode(const uint8* CompressedData, const int32 CompressedDataSize, uint8* OutPCMData, const int32 OutputPCMDataSize) = 0;

	/** Optional method to allow decoder to prepare to loop. */
	virtual void PrepareToLoop() {}

	/** Return the size of the current compression frame */
	virtual int32 GetFrameSize() = 0;

	/** The size of the decode PCM buffer size. */
	virtual uint32 GetMaxFrameSizeSamples() const = 0;

protected:

	/** Reads from the internal source audio buffer stream of the given data size. */
	uint32	Read(void *Outbuffer, uint32 DataSize);

	/**
	* Decompresses a frame of data to PCM buffer
	*
	* @param FrameSize Size of the frame in bytes
	* @return The amount of samples that were decompressed (< 0 indicates error)
	*/
	int32 DecompressToPCMBuffer(uint16 FrameSize);

	/**
	* Adds to the count of samples that have currently been decoded
	*
	* @param NewSamples	How many samples have been decoded
	* @return How many samples were actually part of the true sample count
	*/
	uint32 IncrementCurrentSampleCount(uint32 NewSamples);

	/**
	* Writes data from decoded PCM buffer, taking into account whether some PCM has been written before
	*
	* @param Destination	Where to place the decoded sound
	* @param BufferSize	Size of the destination buffer in bytes
	* @return				How many bytes were written
	*/
	uint32	WriteFromDecodedPCM(uint8* Destination, uint32 BufferSize);

	/**
	* Zeroes the contents of a buffer
	*
	* @param Destination	Buffer to zero
	* @param BufferSize	Size of the destination buffer in bytes
	* @return				How many bytes were zeroed
	*/
	uint32	ZeroBuffer(uint8* Destination, uint32 BufferSize);

	/** Ptr to the current streamed chunk. */
	const uint8* SrcBufferData;
	/** Size of the current streamed chunk. */
	uint32 SrcBufferDataSize;
	/** What byte we're currently reading in the streamed chunk. */
	uint32 SrcBufferOffset;
	/** Where the actual audio data starts in the current streamed chunk. Accounts for header offset. */
	uint32 AudioDataOffset;
	/** Sample rate of the source file */
	uint16 SampleRate;
	/** The total sample count of the source file. */
	uint32 TrueSampleCount;
	/** How many samples we've currently read in the source file. */
	uint32 CurrentSampleCount;
	/** Number of channels (left/right) in th esource file. */
	uint8 NumChannels;
	/** The maximum number of samples per decode frame. */
	uint32 MaxFrameSizeSamples;
	/** The number of bytes per interleaved sample (NumChannels * sizeof(int16)). */
	uint32 SampleStride;
	/** The decoded PCM byte array from the last decoded frame. */
	TArray<uint8> LastDecodedPCM;
	/** The amount of PCM data in bytes was decoded last. */
	uint32 LastPCMByteSize;
	/** The current offset in the last decoded PCM buffer. */
	uint32 LastPCMOffset;
	/** If we're currently reading the final buffer. */
	bool bStoringEndOfFile;
    /** Ptr to the sound wave currently streaming. */
	USoundWave* StreamingSoundWave;
	/** The current chunk index in the streamed chunks. */
	int32 CurrentChunkIndex;
	/** Whether or not to print the chunk fail message. */
	bool bPrintChunkFailMessage;
};


/**
 * Asynchronous audio decompression
 */
class FAsyncAudioDecompressWorker : public FNonAbandonableTask
{
protected:
	class USoundWave*		Wave;

	ICompressedAudioInfo*	AudioInfo;

public:
	/**
	 * Async decompression of audio data
	 *
	 * @param	InWave		Wave data to decompress
	 */
	ENGINE_API FAsyncAudioDecompressWorker(USoundWave* InWave);

	/**
	 * Performs the async audio decompression
	 */
	ENGINE_API void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncAudioDecompressWorker, STATGROUP_ThreadPoolAsyncTasks);
	}
};

typedef FAsyncTask<FAsyncAudioDecompressWorker> FAsyncAudioDecompress;

enum class ERealtimeAudioTaskType : uint8
{
	/** Parses the wave compressed asset header file info */
	CompressedInfo,

	/** Decompresses a chunk */
	Decompress,

	/** Processes a procedural buffer to generate more audio */
	Procedural
};

template<class T>
class FAsyncRealtimeAudioTaskWorker : public FNonAbandonableTask
{
protected:
	T* AudioBuffer;
	USoundWave* WaveData;
	uint8* AudioData;
	int32 MaxSamples;
	int32 BytesWritten;
	ERealtimeAudioTaskType TaskType;
	uint32 bSkipFirstBuffer:1;
	uint32 bLoopingMode:1;
	uint32 bLooped:1;

public:
	FAsyncRealtimeAudioTaskWorker(T* InAudioBuffer, USoundWave* InWaveData)
		: AudioBuffer(InAudioBuffer)
		, WaveData(InWaveData)
		, AudioData(nullptr)
		, MaxSamples(0)
		, BytesWritten(0)
		, TaskType(ERealtimeAudioTaskType::CompressedInfo)
		, bSkipFirstBuffer(false)
		, bLoopingMode(false)
		, bLooped(false)
	{
		check(AudioBuffer);
		check(WaveData);
	}

	FAsyncRealtimeAudioTaskWorker(T* InAudioBuffer, uint8* InAudioData, bool bInLoopingMode, bool bInSkipFirstBuffer)
		: AudioBuffer(InAudioBuffer)
		, AudioData(InAudioData)
		, TaskType(ERealtimeAudioTaskType::Decompress)
		, bSkipFirstBuffer(bInSkipFirstBuffer)
		, bLoopingMode(bInLoopingMode)
		, bLooped(false)
	{
		check(AudioBuffer);
		check(AudioData);
	}

	FAsyncRealtimeAudioTaskWorker(USoundWave* InWaveData, uint8* InAudioData, int32 InMaxSamples)
		: WaveData(InWaveData)
		, AudioData(InAudioData)
		, MaxSamples(InMaxSamples)
		, BytesWritten(0)
		, TaskType(ERealtimeAudioTaskType::Procedural)
	{
		check(WaveData);
		check(AudioData);
	}

	void DoWork()
	{
		LLM_SCOPE(ELLMTag::Audio);

		switch(TaskType)
		{
		case ERealtimeAudioTaskType::CompressedInfo:
			AudioBuffer->ReadCompressedInfo(WaveData);
			break;

		case ERealtimeAudioTaskType::Decompress:
			if (bSkipFirstBuffer)
			{
#if PLATFORM_ANDROID
				// Only skip one buffer on Android
				AudioBuffer->ReadCompressedData( ( uint8* )AudioData, bLoopingMode );
#else
				// If we're using cached data we need to skip the first two reads from the data
				AudioBuffer->ReadCompressedData( ( uint8* )AudioData, bLoopingMode );
				AudioBuffer->ReadCompressedData( ( uint8* )AudioData, bLoopingMode );
#endif
			}
			bLooped = AudioBuffer->ReadCompressedData( ( uint8* )AudioData, bLoopingMode );
			break;

		case ERealtimeAudioTaskType::Procedural:
			BytesWritten = WaveData->GeneratePCMData( (uint8*)AudioData, MaxSamples );
			break;

		default:
			check(false);
		}
	}

	FORCEINLINE TStatId GetStatId() const
	{
		if (TaskType == ERealtimeAudioTaskType::Procedural)
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncRealtimeAudioProceduralWorker, STATGROUP_ThreadPoolAsyncTasks);
		}
		else
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncRealtimeAudioDecompressWorker, STATGROUP_ThreadPoolAsyncTasks);
		}
	}

	ERealtimeAudioTaskType GetTaskType() const
	{
		return TaskType;
	}

	bool GetBufferLooped() const
	{ 
		check(TaskType == ERealtimeAudioTaskType::Decompress);
		return bLooped;
	}

	int32 GetBytesWritten() const
	{ 
		check(TaskType == ERealtimeAudioTaskType::Procedural);
		return BytesWritten;
	}
};

template<class T>
class FAsyncRealtimeAudioTaskProxy
{
public:
	FAsyncRealtimeAudioTaskProxy(T* InAudioBuffer, USoundWave* InWaveData)
	{
		Task = new FAsyncTask<FAsyncRealtimeAudioTaskWorker<T>>(InAudioBuffer, InWaveData);
	}

	FAsyncRealtimeAudioTaskProxy(T* InAudioBuffer, uint8* InAudioData, bool bInLoopingMode, bool bInSkipFirstBuffer)
	{
		Task = new FAsyncTask<FAsyncRealtimeAudioTaskWorker<T>>(InAudioBuffer, InAudioData, bInLoopingMode, bInSkipFirstBuffer);
	}
	FAsyncRealtimeAudioTaskProxy(USoundWave* InWaveData, uint8* InAudioData, int32 InMaxSamples)
	{
		Task = new FAsyncTask<FAsyncRealtimeAudioTaskWorker<T>>(InWaveData, InAudioData, InMaxSamples);
	}

	~FAsyncRealtimeAudioTaskProxy()
	{
		check(IsDone());
		delete Task;
	}

	bool IsDone()
	{
		FScopeLock Lock(&CritSect);
		return Task->IsDone();
	}

	void EnsureCompletion(bool bDoWorkOnThisThreadIfNotStarted = true)
	{
		FScopeLock Lock(&CritSect);
		Task->EnsureCompletion(bDoWorkOnThisThreadIfNotStarted);
	}

	void StartBackgroundTask()
	{
		FScopeLock Lock(&CritSect);
		Task->StartBackgroundTask();
	}

	FAsyncRealtimeAudioTaskWorker<T>& GetTask()
	{
		FScopeLock Lock(&CritSect);
		return Task->GetTask();
	}

private:
	FCriticalSection CritSect;
	FAsyncTask<FAsyncRealtimeAudioTaskWorker<T>>* Task;
};
