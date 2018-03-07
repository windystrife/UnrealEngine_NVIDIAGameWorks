// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "AudioDecompress.h"
#include "AudioDevice.h"
#include "Interfaces/IAudioFormat.h"
#include "ContentStreaming.h"
#include "HAL/LowLevelMemTracker.h"

IStreamedCompressedInfo::IStreamedCompressedInfo()
	: SrcBufferData(nullptr)
	, SrcBufferDataSize(0)
	, SrcBufferOffset(0)
	, AudioDataOffset(0)
	, SampleRate(0)
	, TrueSampleCount(0)
	, CurrentSampleCount(0)
	, NumChannels(0)
	, MaxFrameSizeSamples(0)
	, SampleStride(0)
	, LastPCMByteSize(0)
	, LastPCMOffset(0)
	, bStoringEndOfFile(false)
	, StreamingSoundWave(nullptr)
	, CurrentChunkIndex(0)
	, bPrintChunkFailMessage(true)
{
}

uint32 IStreamedCompressedInfo::Read(void *OutBuffer, uint32 DataSize)
{
	uint32 BytesToRead = FMath::Min(DataSize, SrcBufferDataSize - SrcBufferOffset);
	if (BytesToRead > 0)
	{
		FMemory::Memcpy(OutBuffer, SrcBufferData + SrcBufferOffset, BytesToRead);
		SrcBufferOffset += BytesToRead;
	}
	return BytesToRead;
}

bool IStreamedCompressedInfo::ReadCompressedInfo(const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, FSoundQualityInfo* QualityInfo)
{
	check(!SrcBufferData);

	SCOPE_CYCLE_COUNTER(STAT_AudioStreamedDecompressTime);

	// Parse the format header, this is done different for each format
	if (!ParseHeader(InSrcBufferData, InSrcBufferDataSize, QualityInfo))
	{
		return false;
	}

	// After parsing the header, the SrcBufferData should be none-null
	check(SrcBufferData != nullptr);

	// Sample Stride is 
	SampleStride = NumChannels * sizeof(int16);

	MaxFrameSizeSamples = GetMaxFrameSizeSamples();
		
	LastDecodedPCM.Empty(MaxFrameSizeSamples * SampleStride);
	LastDecodedPCM.AddUninitialized(MaxFrameSizeSamples * SampleStride);

	return CreateDecoder();
}

bool IStreamedCompressedInfo::ReadCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize)
{
	check(Destination);

	SCOPE_CYCLE_COUNTER(STAT_AudioStreamedDecompressTime);

	bool bFinished = false;
	uint32 TotalBytesDecoded = 0;

	while (TotalBytesDecoded < BufferSize)
	{
		const uint8* EncodedSrcPtr = SrcBufferData + SrcBufferOffset;
		const uint32 RemainingEncodedSrcSize = SrcBufferDataSize - SrcBufferOffset;

		const FDecodeResult DecodeResult = Decode(EncodedSrcPtr, RemainingEncodedSrcSize, Destination, BufferSize - TotalBytesDecoded);
		if (!DecodeResult.NumPcmBytesProduced)
		{
			bFinished = true;

			if (bLooping)
			{
				SrcBufferOffset = AudioDataOffset;
				CurrentSampleCount = 0;

				PrepareToLoop();
			}
			else
			{
				// Zero the remainder of the buffer
				FMemory::Memzero(Destination, BufferSize - TotalBytesDecoded);
				break;
			}
		}
		else if (DecodeResult.NumPcmBytesProduced < 0)
		{
			// Zero the remainder of the buffer
			FMemory::Memzero(Destination, BufferSize - TotalBytesDecoded);
			return true;
		}

		TotalBytesDecoded += DecodeResult.NumPcmBytesProduced;
		SrcBufferOffset += DecodeResult.NumCompressedBytesConsumed;
		Destination += DecodeResult.NumPcmBytesProduced;
	}

	return bFinished;
}

void IStreamedCompressedInfo::ExpandFile(uint8* DstBuffer, struct FSoundQualityInfo* QualityInfo)
{
	check(DstBuffer);
	check(QualityInfo);

	// Ensure we're at the start of the audio data
	SrcBufferOffset = AudioDataOffset;

	uint32 RawPCMOffset = 0;

	while (RawPCMOffset < QualityInfo->SampleDataSize)
	{
		uint16 FrameSize = GetFrameSize();
		int32 DecodedSamples = DecompressToPCMBuffer(FrameSize);

		if (DecodedSamples < 0)
		{
			RawPCMOffset += ZeroBuffer(DstBuffer + RawPCMOffset, QualityInfo->SampleDataSize - RawPCMOffset);
		}
		else
		{
			LastPCMByteSize = IncrementCurrentSampleCount(DecodedSamples) * SampleStride;
			RawPCMOffset += WriteFromDecodedPCM(DstBuffer + RawPCMOffset, QualityInfo->SampleDataSize - RawPCMOffset);
		}
	}
}

bool IStreamedCompressedInfo::StreamCompressedInfo(USoundWave* Wave, struct FSoundQualityInfo* QualityInfo)
{
	StreamingSoundWave = Wave;

	// Get the first chunk of audio data (should always be loaded)
	CurrentChunkIndex = 0;
	const uint8* FirstChunk = IStreamingManager::Get().GetAudioStreamingManager().GetLoadedChunk(StreamingSoundWave, CurrentChunkIndex);

	if (FirstChunk)
	{
		return ReadCompressedInfo(FirstChunk, Wave->RunningPlatformData->Chunks[0].DataSize, QualityInfo);
	}

	return false;
}

bool IStreamedCompressedInfo::StreamCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize)
{
	check(Destination);
	SCOPED_NAMED_EVENT(IStreamedCompressedInfo_StreamCompressedData, FColor::Blue);

	SCOPE_CYCLE_COUNTER(STAT_AudioStreamedDecompressTime);

	UE_LOG(LogAudio, Log, TEXT("Streaming compressed data from SoundWave'%s' - Chunk %d, Offset %d"), *StreamingSoundWave->GetName(), CurrentChunkIndex, SrcBufferOffset);

	// Write out any PCM data that was decoded during the last request
	uint32 RawPCMOffset = WriteFromDecodedPCM(Destination, BufferSize);

	// If next chunk wasn't loaded when last one finished reading, try to get it again now
	if (SrcBufferData == NULL)
	{
		SrcBufferData = IStreamingManager::Get().GetAudioStreamingManager().GetLoadedChunk(StreamingSoundWave, CurrentChunkIndex);
		if (SrcBufferData)
		{
			bPrintChunkFailMessage = true;
			SrcBufferDataSize = StreamingSoundWave->RunningPlatformData->Chunks[CurrentChunkIndex].DataSize;
			SrcBufferOffset = CurrentChunkIndex == 0 ? AudioDataOffset : 0;
		}
		else
		{
			// Still not loaded, zero remainder of current buffer
			if (bPrintChunkFailMessage)
			{
				UE_LOG(LogAudio, Verbose, TEXT("Chunk %d not loaded from streaming manager for SoundWave '%s'. Likely due to stall on game thread."), CurrentChunkIndex, *StreamingSoundWave->GetName());
				bPrintChunkFailMessage = false;
			}
			ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
			return false;
		}
	}

	bool bLooped = false;

	if (bStoringEndOfFile && LastPCMByteSize > 0)
	{
		// delayed returning looped because we hadn't read the entire buffer
		bLooped = true;
		bStoringEndOfFile = false;
	}

	while (RawPCMOffset < BufferSize)
	{
		// Get the platform-dependent size of the current "frame" of encoded audio (note: frame is used here differently than audio frame/sample)
		uint16 FrameSize = GetFrameSize();

		// Decompress the next compression frame of audio (many samples) into the PCM buffer
		int32 DecodedSamples = DecompressToPCMBuffer(FrameSize);

		if (DecodedSamples < 0)
		{
			LastPCMByteSize = 0;
			ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
			return false;
		}
		else
		{
			LastPCMByteSize = IncrementCurrentSampleCount(DecodedSamples) * SampleStride;

			RawPCMOffset += WriteFromDecodedPCM(Destination + RawPCMOffset, BufferSize - RawPCMOffset);

			// Have we reached the end of buffer
			if (SrcBufferOffset >= SrcBufferDataSize)
			{
				// Special case for the last chunk of audio
				if (CurrentChunkIndex == StreamingSoundWave->RunningPlatformData->NumChunks - 1)
				{
					// check whether all decoded PCM was written
					if (LastPCMByteSize == 0)
					{
						bLooped = true;
					}
					else
					{
						bStoringEndOfFile = true;
					}

					if (bLooping)
					{
						CurrentChunkIndex = 0;
						SrcBufferOffset = AudioDataOffset;
						CurrentSampleCount = 0;

						// Prepare the decoder to begin looping.
						PrepareToLoop();
					}
					else
					{
						RawPCMOffset += ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
					}
				}
				else
				{
					CurrentChunkIndex++;
					SrcBufferOffset = 0;
				}

				SrcBufferData = IStreamingManager::Get().GetAudioStreamingManager().GetLoadedChunk(StreamingSoundWave, CurrentChunkIndex);
				if (SrcBufferData)
				{
					UE_LOG(LogAudio, Log, TEXT("Incremented current chunk from SoundWave'%s' - Chunk %d, Offset %d"), *StreamingSoundWave->GetName(), CurrentChunkIndex, SrcBufferOffset);
					SrcBufferDataSize = StreamingSoundWave->RunningPlatformData->Chunks[CurrentChunkIndex].DataSize;					
				}
				else
				{
					SrcBufferDataSize = 0;
					RawPCMOffset += ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
				}
			}
		}
	}

	return bLooped;
}

int32 IStreamedCompressedInfo::DecompressToPCMBuffer(uint16 FrameSize)
{
	if (SrcBufferOffset + FrameSize > SrcBufferDataSize)
	{
		// if frame size is too large, something has gone wrong
		return -1;
	}

	const uint8* SrcPtr = SrcBufferData + SrcBufferOffset;
	SrcBufferOffset += FrameSize;
	LastPCMOffset = 0;
	
	const FDecodeResult DecodeResult = Decode(SrcPtr, FrameSize, LastDecodedPCM.GetData(), LastDecodedPCM.Num());
	return DecodeResult.NumAudioFramesProduced;
}

uint32 IStreamedCompressedInfo::IncrementCurrentSampleCount(uint32 NewSamples)
{
	if (CurrentSampleCount + NewSamples > TrueSampleCount)
	{
		NewSamples = TrueSampleCount - CurrentSampleCount;
		CurrentSampleCount = TrueSampleCount;
	}
	else
	{
		CurrentSampleCount += NewSamples;
	}
	return NewSamples;
}

uint32 IStreamedCompressedInfo::WriteFromDecodedPCM(uint8* Destination, uint32 BufferSize)
{
	uint32 BytesToCopy = FMath::Min(BufferSize, LastPCMByteSize - LastPCMOffset);
	if (BytesToCopy > 0)
	{
		check(BytesToCopy <= LastDecodedPCM.Num() - LastPCMOffset);
		FMemory::Memcpy(Destination, LastDecodedPCM.GetData() + LastPCMOffset, BytesToCopy);
		LastPCMOffset += BytesToCopy;
		if (LastPCMOffset >= LastPCMByteSize)
		{
			LastPCMOffset = 0;
			LastPCMByteSize = 0;
		}
	}
	return BytesToCopy;
}

uint32 IStreamedCompressedInfo::ZeroBuffer(uint8* Destination, uint32 BufferSize)
{
	check(Destination);

	if (BufferSize > 0)
	{
		FMemory::Memzero(Destination, BufferSize);
		return BufferSize;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
// Copied from IOS - probably want to split and share
//////////////////////////////////////////////////////////////////////////
#define NUM_ADAPTATION_TABLE 16
#define NUM_ADAPTATION_COEFF 7
#define SOUND_SOURCE_FREE 0
#define SOUND_SOURCE_LOCKED 1

namespace
{
	template <typename T, uint32 B>
	inline T SignExtend(const T ValueToExtend)
	{
		struct { T ExtendedValue : B; } SignExtender;
		return SignExtender.ExtendedValue = ValueToExtend;
	}

	template <typename T>
	inline T ReadFromByteStream(const uint8* ByteStream, int32& ReadIndex, bool bLittleEndian = true)
	{
		T ValueRaw = 0;

		if (bLittleEndian)
		{
#if PLATFORM_LITTLE_ENDIAN
			for (int32 ByteIndex = 0; ByteIndex < sizeof(T); ++ByteIndex)
#else
			for (int32 ByteIndex = sizeof(T) - 1; ByteIndex >= 0; --ByteIndex)
#endif // PLATFORM_LITTLE_ENDIAN
			{
				ValueRaw |= ByteStream[ReadIndex++] << 8 * ByteIndex;
			}
		}
		else
		{
#if PLATFORM_LITTLE_ENDIAN
			for (int32 ByteIndex = sizeof(T) - 1; ByteIndex >= 0; --ByteIndex)
#else
			for (int32 ByteIndex = 0; ByteIndex < sizeof(T); ++ByteIndex)
#endif // PLATFORM_LITTLE_ENDIAN
			{
				ValueRaw |= ByteStream[ReadIndex++] << 8 * ByteIndex;
			}
		}

		return ValueRaw;
	}

	template <typename T>
	inline void WriteToByteStream(T Value, uint8* ByteStream, int32& WriteIndex, bool bLittleEndian = true)
	{
		if (bLittleEndian)
		{
#if PLATFORM_LITTLE_ENDIAN
			for (int32 ByteIndex = 0; ByteIndex < sizeof(T); ++ByteIndex)
#else
			for (int32 ByteIndex = sizeof(T) - 1; ByteIndex >= 0; --ByteIndex)
#endif // PLATFORM_LITTLE_ENDIAN
			{
				ByteStream[WriteIndex++] = (Value >> (8 * ByteIndex)) & 0xFF;
			}
		}
		else
		{
#if PLATFORM_LITTLE_ENDIAN
			for (int32 ByteIndex = sizeof(T) - 1; ByteIndex >= 0; --ByteIndex)
#else
			for (int32 ByteIndex = 0; ByteIndex < sizeof(T); ++ByteIndex)
#endif // PLATFORM_LITTLE_ENDIAN
			{
				ByteStream[WriteIndex++] = (Value >> (8 * ByteIndex)) & 0xFF;
			}
		}
	}

	template <typename T>
	inline T ReadFromArray(const T* ElementArray, int32& ReadIndex, int32 NumElements, int32 IndexStride = 1)
	{
		T OutputValue = 0;

		if (ReadIndex >= 0 && ReadIndex < NumElements)
		{
			OutputValue = ElementArray[ReadIndex];
			ReadIndex += IndexStride;
		}

		return OutputValue;
	}

	inline bool LockSourceChannel(int32* ChannelLock)
	{
		check(ChannelLock != NULL);
		return FPlatformAtomics::InterlockedCompareExchange(ChannelLock, SOUND_SOURCE_LOCKED, SOUND_SOURCE_FREE) == SOUND_SOURCE_FREE;
	}

	inline void UnlockSourceChannel(int32* ChannelLock)
	{
		check(ChannelLock != NULL);
		int32 Result = FPlatformAtomics::InterlockedCompareExchange(ChannelLock, SOUND_SOURCE_FREE, SOUND_SOURCE_LOCKED);

		check(Result == SOUND_SOURCE_LOCKED);
	}

} // end namespace

namespace ADPCM
{
	template <typename T>
	static void GetAdaptationTable(T(&OutAdaptationTable)[NUM_ADAPTATION_TABLE])
	{
		// Magic values as specified by standard
		static T AdaptationTable[] =
		{
			230, 230, 230, 230, 307, 409, 512, 614,
			768, 614, 512, 409, 307, 230, 230, 230
		};

		FMemory::Memcpy(&OutAdaptationTable, AdaptationTable, sizeof(AdaptationTable));
	}

	template <typename T>
	static void GetAdaptationCoefficients(T(&OutAdaptationCoefficient1)[NUM_ADAPTATION_COEFF], T(&OutAdaptationCoefficient2)[NUM_ADAPTATION_COEFF])
	{
		// Magic values as specified by standard
		static T AdaptationCoefficient1[] =
		{
			256, 512, 0, 192, 240, 460, 392
		};
		static T AdaptationCoefficient2[] =
		{
			0, -256, 0, 64, 0, -208, -232
		};

		FMemory::Memcpy(&OutAdaptationCoefficient1, AdaptationCoefficient1, sizeof(AdaptationCoefficient1));
		FMemory::Memcpy(&OutAdaptationCoefficient2, AdaptationCoefficient2, sizeof(AdaptationCoefficient2));
	}

	struct FAdaptationContext
	{
	public:
		// Adaptation constants
		int32 AdaptationTable[NUM_ADAPTATION_TABLE];
		int32 AdaptationCoefficient1[NUM_ADAPTATION_COEFF];
		int32 AdaptationCoefficient2[NUM_ADAPTATION_COEFF];

		int32 AdaptationDelta;
		int32 Coefficient1;
		int32 Coefficient2;
		int32 Sample1;
		int32 Sample2;

		FAdaptationContext() :
			AdaptationDelta(0),
			Coefficient1(0),
			Coefficient2(0),
			Sample1(0),
			Sample2(0)
		{
			GetAdaptationTable(AdaptationTable);
			GetAdaptationCoefficients(AdaptationCoefficient1, AdaptationCoefficient2);
		}
	};

	int16 DecodeNibble(FAdaptationContext& Context, uint8 EncodedNibble)
	{
		int32 PredictedSample = (Context.Sample1 * Context.Coefficient1 + Context.Sample2 * Context.Coefficient2) / 256;
		PredictedSample += SignExtend<int8, 4>(EncodedNibble) * Context.AdaptationDelta;
		PredictedSample = FMath::Clamp(PredictedSample, -32768, 32767);

		// Shuffle samples for the next iteration
		Context.Sample2 = Context.Sample1;
		Context.Sample1 = static_cast<int16>(PredictedSample);
		Context.AdaptationDelta = (Context.AdaptationDelta * Context.AdaptationTable[EncodedNibble]) / 256;
		Context.AdaptationDelta = FMath::Max(Context.AdaptationDelta, 16);

		return Context.Sample1;
	}

	void DecodeBlock(const uint8* EncodedADPCMBlock, int32 BlockSize, int16* DecodedPCMData)
	{
		FAdaptationContext Context;
		int32 ReadIndex = 0;
		int32 WriteIndex = 0;

		uint8 CoefficientIndex = ReadFromByteStream<uint8>(EncodedADPCMBlock, ReadIndex);
		Context.AdaptationDelta = ReadFromByteStream<int16>(EncodedADPCMBlock, ReadIndex);
		Context.Sample1 = ReadFromByteStream<int16>(EncodedADPCMBlock, ReadIndex);
		Context.Sample2 = ReadFromByteStream<int16>(EncodedADPCMBlock, ReadIndex);
		Context.Coefficient1 = Context.AdaptationCoefficient1[CoefficientIndex];
		Context.Coefficient2 = Context.AdaptationCoefficient2[CoefficientIndex];

		// The first two samples are sent directly to the output in reverse order, as per the standard
		DecodedPCMData[WriteIndex++] = Context.Sample2;
		DecodedPCMData[WriteIndex++] = Context.Sample1;

		uint8 EncodedNibblePair = 0;
		uint8 EncodedNibble = 0;
		while (ReadIndex < BlockSize)
		{
			// Read from the byte stream and advance the read head.
			EncodedNibblePair = ReadFromByteStream<uint8>(EncodedADPCMBlock, ReadIndex);

			EncodedNibble = (EncodedNibblePair >> 4) & 0x0F;
			DecodedPCMData[WriteIndex++] = DecodeNibble(Context, EncodedNibble);

			EncodedNibble = EncodedNibblePair & 0x0F;
			DecodedPCMData[WriteIndex++] = DecodeNibble(Context, EncodedNibble);
		}
	}

	// Decode two PCM streams and interleave as stereo data
	void DecodeBlockStereo(const uint8* EncodedADPCMBlockLeft, const uint8* EncodedADPCMBlockRight, int32 BlockSize, int16* DecodedPCMData)
	{
		FAdaptationContext ContextLeft;
		FAdaptationContext ContextRight;
		int32 ReadIndexLeft = 0;
		int32 ReadIndexRight = 0;
		int32 WriteIndex = 0;

		uint8 CoefficientIndexLeft = ReadFromByteStream<uint8>(EncodedADPCMBlockLeft, ReadIndexLeft);
		ContextLeft.AdaptationDelta = ReadFromByteStream<int16>(EncodedADPCMBlockLeft, ReadIndexLeft);
		ContextLeft.Sample1 = ReadFromByteStream<int16>(EncodedADPCMBlockLeft, ReadIndexLeft);
		ContextLeft.Sample2 = ReadFromByteStream<int16>(EncodedADPCMBlockLeft, ReadIndexLeft);
		ContextLeft.Coefficient1 = ContextLeft.AdaptationCoefficient1[CoefficientIndexLeft];
		ContextLeft.Coefficient2 = ContextLeft.AdaptationCoefficient2[CoefficientIndexLeft];

		uint8 CoefficientIndexRight = ReadFromByteStream<uint8>(EncodedADPCMBlockRight, ReadIndexRight);
		ContextRight.AdaptationDelta = ReadFromByteStream<int16>(EncodedADPCMBlockRight, ReadIndexRight);
		ContextRight.Sample1 = ReadFromByteStream<int16>(EncodedADPCMBlockRight, ReadIndexRight);
		ContextRight.Sample2 = ReadFromByteStream<int16>(EncodedADPCMBlockRight, ReadIndexRight);
		ContextRight.Coefficient1 = ContextRight.AdaptationCoefficient1[CoefficientIndexRight];
		ContextRight.Coefficient2 = ContextRight.AdaptationCoefficient2[CoefficientIndexRight];

		// The first two samples from each stream are sent directly to the output in reverse order, as per the standard
		DecodedPCMData[WriteIndex++] = ContextLeft.Sample2;
		DecodedPCMData[WriteIndex++] = ContextRight.Sample2;
		DecodedPCMData[WriteIndex++] = ContextLeft.Sample1;
		DecodedPCMData[WriteIndex++] = ContextRight.Sample1;

		uint8 EncodedNibblePairLeft = 0;
		uint8 EncodedNibbleLeft = 0;
		uint8 EncodedNibblePairRight = 0;
		uint8 EncodedNibbleRight = 0;

		while (ReadIndexLeft < BlockSize)
		{
			// Read from the byte stream and advance the read head.
			EncodedNibblePairLeft = ReadFromByteStream<uint8>(EncodedADPCMBlockLeft, ReadIndexLeft);
			EncodedNibblePairRight = ReadFromByteStream<uint8>(EncodedADPCMBlockRight, ReadIndexRight);

			EncodedNibbleLeft = (EncodedNibblePairLeft >> 4) & 0x0F;
			DecodedPCMData[WriteIndex++] = DecodeNibble(ContextLeft, EncodedNibbleLeft);

			EncodedNibbleRight = (EncodedNibblePairRight >> 4) & 0x0F;
			DecodedPCMData[WriteIndex++] = DecodeNibble(ContextRight, EncodedNibbleRight);

			EncodedNibbleLeft = EncodedNibblePairLeft & 0x0F;
			DecodedPCMData[WriteIndex++] = DecodeNibble(ContextLeft, EncodedNibbleLeft);

			EncodedNibbleRight = EncodedNibblePairRight & 0x0F;
			DecodedPCMData[WriteIndex++] = DecodeNibble(ContextRight, EncodedNibbleRight);
		}

	}
} // end namespace ADPCM

//////////////////////////////////////////////////////////////////////////
// End of copied section
//////////////////////////////////////////////////////////////////////////

/**
 * Worker for decompression on a separate thread
 */
FAsyncAudioDecompressWorker::FAsyncAudioDecompressWorker(USoundWave* InWave)
	: Wave(InWave)
	, AudioInfo(NULL)
{
	if (GEngine && GEngine->GetMainAudioDevice())
	{
		AudioInfo = GEngine->GetMainAudioDevice()->CreateCompressedAudioInfo(Wave);
	}
}

void FAsyncAudioDecompressWorker::DoWork()
{
	LLM_SCOPE(ELLMTag::Audio);

	if (AudioInfo)
	{
		FSoundQualityInfo QualityInfo = { 0 };

		// Parse the audio header for the relevant information
		if (AudioInfo->ReadCompressedInfo(Wave->ResourceData, Wave->ResourceSize, &QualityInfo))
		{
			FScopeCycleCounterUObject WaveObject( Wave );

#if PLATFORM_ANDROID
			// Handle resampling
			if (QualityInfo.SampleRate > 48000)
			{
				UE_LOG( LogAudio, Warning, TEXT("Resampling file %s from %d"), *(Wave->GetName()), QualityInfo.SampleRate);
				UE_LOG( LogAudio, Warning, TEXT("  Size %d"), QualityInfo.SampleDataSize);
				uint32 SampleCount = QualityInfo.SampleDataSize / (QualityInfo.NumChannels * sizeof(uint16));
				QualityInfo.SampleRate = QualityInfo.SampleRate / 2;
				SampleCount /= 2;
				QualityInfo.SampleDataSize = SampleCount * QualityInfo.NumChannels * sizeof(uint16);
				AudioInfo->EnableHalfRate(true);
			}
#endif
			// Extract the data
			Wave->SampleRate = QualityInfo.SampleRate;
			Wave->NumChannels = QualityInfo.NumChannels;
			if (QualityInfo.Duration > 0.0f)
			{ 
				Wave->Duration = QualityInfo.Duration;
			}

			if (Wave->DecompressionType == DTYPE_RealTime)
			{
#if PLATFORM_NUM_AUDIODECOMPRESSION_PRECACHE_BUFFERS > 0
				const uint32 PCMBufferSize = MONO_PCM_BUFFER_SIZE * Wave->NumChannels * PLATFORM_NUM_AUDIODECOMPRESSION_PRECACHE_BUFFERS;
				check(Wave->CachedRealtimeFirstBuffer == nullptr);
				Wave->CachedRealtimeFirstBuffer = (uint8*)FMemory::Malloc(PCMBufferSize);
				AudioInfo->ReadCompressedData(Wave->CachedRealtimeFirstBuffer, Wave->bLooping, PCMBufferSize);
#endif
			}
			else
			{
				check(Wave->DecompressionType == DTYPE_Native || Wave->DecompressionType == DTYPE_Procedural);

				Wave->RawPCMDataSize = QualityInfo.SampleDataSize;
				check(Wave->RawPCMData == nullptr);
				Wave->RawPCMData = ( uint8* )FMemory::Malloc( Wave->RawPCMDataSize );

				// Decompress all the sample data into preallocated memory
				AudioInfo->ExpandFile(Wave->RawPCMData, &QualityInfo);

				// Only track the RawPCMDataSize at this point since we haven't yet
				// removed the compressed asset from memory and GetResourceSize() would add that at this point
				Wave->TrackedMemoryUsage += Wave->RawPCMDataSize;
				INC_DWORD_STAT_BY(STAT_AudioMemorySize, Wave->RawPCMDataSize);
				INC_DWORD_STAT_BY(STAT_AudioMemory, Wave->RawPCMDataSize);
			}
		}
		else if (Wave->DecompressionType == DTYPE_RealTime)
		{
			Wave->DecompressionType = DTYPE_Invalid;
			Wave->NumChannels = 0;

			Wave->RemoveAudioResource();
		}

		if (Wave->DecompressionType == DTYPE_Native)
		{
			// Delete the compressed data
			Wave->RemoveAudioResource();
		}

		delete AudioInfo;

		// Flag that we've finished this precache decompress task.
		Wave->bIsPrecacheDone = true;
	}
}

// end
