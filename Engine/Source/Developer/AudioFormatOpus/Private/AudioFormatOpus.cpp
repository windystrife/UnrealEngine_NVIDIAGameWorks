// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioFormatOpus.h"
#include "Serialization/MemoryWriter.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IAudioFormat.h"
#include "Interfaces/IAudioFormatModule.h"
#include "OpusAudioInfo.h"
#include "VorbisAudioInfo.h"

// Need to define this so that resampler.h compiles - probably a way around this somehow
#define OUTSIDE_SPEEX

THIRD_PARTY_INCLUDES_START
#include "opus_multistream.h"
#include "speex_resampler.h"
THIRD_PARTY_INCLUDES_END

/** Use UE4 memory allocation or Opus */
#define USE_UE4_MEM_ALLOC 1
#define SAMPLE_SIZE			( ( uint32 )sizeof( short ) )

static FName NAME_OPUS(TEXT("OPUS"));

/**
 * IAudioFormat, audio compression abstraction
**/
class FAudioFormatOpus : public IAudioFormat
{
	enum
	{
		/** Version for OPUS format, this becomes part of the DDC key. */
		UE_AUDIO_OPUS_VER = 3,
	};

public:
	virtual bool AllowParallelBuild() const override
	{
		return false;
	}

	virtual uint16 GetVersion(FName Format) const override
	{
		check(Format == NAME_OPUS);
		return UE_AUDIO_OPUS_VER;
	}


	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const override
	{
		OutFormats.Add(NAME_OPUS);
	}

	virtual bool Cook(FName Format, const TArray<uint8>& SrcBuffer, FSoundQualityInfo& QualityInfo, TArray<uint8>& CompressedDataStore) const override
	{
		check(Format == NAME_OPUS);

		// Get best compatible sample rate
		const uint16 kOpusSampleRate = GetBestOutputSampleRate(QualityInfo.SampleRate);
		// Frame size must be one of 2.5, 5, 10, 20, 40 or 60 ms
		const int32 kOpusFrameSizeMs = 60;
		// Calculate frame size required by Opus
		const int32 kOpusFrameSizeSamples = (kOpusSampleRate * kOpusFrameSizeMs) / 1000;
		const uint32 kSampleStride = SAMPLE_SIZE * QualityInfo.NumChannels;
		const int32 kBytesPerFrame = kOpusFrameSizeSamples * kSampleStride;

		// Check whether source has compatible sample rate
		TArray<uint8> SrcBufferCopy;
		if (QualityInfo.SampleRate != kOpusSampleRate)
		{
			if (!ResamplePCM(QualityInfo.NumChannels, SrcBuffer, QualityInfo.SampleRate, SrcBufferCopy, kOpusSampleRate))
			{
				return false;
			}
		}
		else
		{
			// Take a copy of the source regardless
			SrcBufferCopy = SrcBuffer;
		}

		// Initialise the Opus encoder
		OpusEncoder* Encoder = NULL;
		int32 EncError = 0;
#if USE_UE4_MEM_ALLOC
		int32 EncSize = opus_encoder_get_size(QualityInfo.NumChannels);
		Encoder = (OpusEncoder*)FMemory::Malloc(EncSize);
		EncError = opus_encoder_init(Encoder, kOpusSampleRate, QualityInfo.NumChannels, OPUS_APPLICATION_AUDIO);
#else
		Encoder = opus_encoder_create(kOpusSampleRate, QualityInfo.NumChannels, OPUS_APPLICATION_AUDIO, &EncError);
#endif
		if (EncError != OPUS_OK)
		{
			Destroy(Encoder);
			return false;
		}

		int32 BitRate = GetBitRateFromQuality(QualityInfo);
		opus_encoder_ctl(Encoder, OPUS_SET_BITRATE(BitRate));

		// Create a buffer to store compressed data
		CompressedDataStore.Empty();
		FMemoryWriter CompressedData(CompressedDataStore);
		int32 SrcBufferOffset = 0;

		// Calc frame and sample count
		int32 FramesToEncode = SrcBufferCopy.Num() / kBytesPerFrame;
		uint32 TrueSampleCount = SrcBufferCopy.Num() / kSampleStride;

		// Pad the end of data with zeroes if it isn't exactly the size of a frame.
		if (SrcBufferCopy.Num() % kBytesPerFrame != 0)
		{
			int32 FrameDiff = kBytesPerFrame - (SrcBufferCopy.Num() % kBytesPerFrame);
			SrcBufferCopy.AddZeroed(FrameDiff);
			FramesToEncode++;
		}

		check(QualityInfo.NumChannels <= MAX_uint8);
		check(FramesToEncode <= MAX_uint16);
		SerializeHeaderData(CompressedData, kOpusSampleRate, TrueSampleCount, QualityInfo.NumChannels, FramesToEncode);

		// Temporary storage with more than enough to store any compressed frame
		TArray<uint8> TempCompressedData;
		TempCompressedData.AddUninitialized(kBytesPerFrame);

		while (SrcBufferOffset < SrcBufferCopy.Num())
		{
			int32 CompressedLength = opus_encode(Encoder, (const opus_int16*)(SrcBufferCopy.GetData() + SrcBufferOffset), kOpusFrameSizeSamples, TempCompressedData.GetData(), TempCompressedData.Num());

			if (CompressedLength < 0)
			{
				const char* ErrorStr = opus_strerror(CompressedLength);
				UE_LOG(LogAudio, Warning, TEXT("Failed to encode: [%d] %s"), CompressedLength, ANSI_TO_TCHAR(ErrorStr));

				Destroy(Encoder);

				CompressedDataStore.Empty();
				return false;
			}
			else
			{
				// Store frame length and copy compressed data before incrementing pointers
				check(CompressedLength < MAX_uint16);
				SerialiseFrameData(CompressedData, TempCompressedData.GetData(), CompressedLength);
				SrcBufferOffset += kBytesPerFrame;
			}
		}

		Destroy(Encoder);

		return CompressedDataStore.Num() > 0;
	}

	virtual bool CookSurround(FName Format, const TArray<TArray<uint8> >& SrcBuffers, FSoundQualityInfo& QualityInfo, TArray<uint8>& CompressedDataStore) const override
	{
		check(Format == NAME_OPUS);

		// Get best compatible sample rate
		const uint16 kOpusSampleRate = GetBestOutputSampleRate(QualityInfo.SampleRate);
		// Frame size must be one of 2.5, 5, 10, 20, 40 or 60 ms
		const int32 kOpusFrameSizeMs = 60;
		// Calculate frame size required by Opus
		const int32 kOpusFrameSizeSamples = (kOpusSampleRate * kOpusFrameSizeMs) / 1000;
		const uint32 kSampleStride = SAMPLE_SIZE * QualityInfo.NumChannels;
		const int32 kBytesPerFrame = kOpusFrameSizeSamples * kSampleStride;

		// Check whether source has compatible sample rate
		TArray<TArray<uint8>> SrcBufferCopies;
		if (QualityInfo.SampleRate != kOpusSampleRate)
		{
			for (int32 Index = 0; Index < SrcBuffers.Num(); Index++)
			{
				TArray<uint8>& NewCopy = *new (SrcBufferCopies) TArray<uint8>;
				if (!ResamplePCM(1, SrcBuffers[Index], QualityInfo.SampleRate, NewCopy, kOpusSampleRate))
				{
					return false;
				}
			}
		}
		else
		{
			// Take a copy of the source regardless
			for (int32 Index = 0; Index < SrcBuffers.Num(); Index++)
			{
				SrcBufferCopies[Index] = SrcBuffers[Index];
			}
		}

		// Ensure that all channels are the same length
		int32 SourceSize = -1;
		for (int32 Index = 0; Index < SrcBufferCopies.Num(); Index++)
		{
			if (!Index)
			{
				SourceSize = SrcBufferCopies[Index].Num();
			}
			else
			{
				if (SourceSize != SrcBufferCopies[Index].Num())
				{
					return false;
				}
			}
		}
		if (SourceSize <= 0)
		{
			return false;
		}

		// Initialise the Opus multistream encoder
		OpusMSEncoder* Encoder = NULL;
		int32 EncError = 0;
		int32 streams = 0;
		int32 coupled_streams = 0;
		// mapping_family not documented but figured out: 0 = 1 or 2 channels, 1 = 1 to 8 channel surround sound, 255 = up to 255 channels with no surround processing
		int32 mapping_family = 1;
		TArray<uint8> mapping;
		mapping.AddUninitialized(QualityInfo.NumChannels);
#if USE_UE4_MEM_ALLOC
		int32 EncSize = opus_multistream_surround_encoder_get_size(QualityInfo.NumChannels, mapping_family);
		Encoder = (OpusMSEncoder*)FMemory::Malloc(EncSize);
		EncError = opus_multistream_surround_encoder_init(Encoder, kOpusSampleRate, QualityInfo.NumChannels, mapping_family, &streams, &coupled_streams, mapping.GetData(), OPUS_APPLICATION_AUDIO);
#else
		Encoder = opus_multistream_surround_encoder_create(kOpusSampleRate, QualityInfo.NumChannels, mapping_family, &streams, &coupled_streams, mapping.GetData(), OPUS_APPLICATION_AUDIO, &EncError);
#endif
		if (EncError != OPUS_OK)
		{
			Destroy(Encoder);
			return false;
		}

		int32 BitRate = GetBitRateFromQuality(QualityInfo);
		opus_multistream_encoder_ctl(Encoder, OPUS_SET_BITRATE(BitRate));

		// Create a buffer to store compressed data
		CompressedDataStore.Empty();
		FMemoryWriter CompressedData(CompressedDataStore);
		int32 SrcBufferOffset = 0;

		// Calc frame and sample count
		int32 FramesToEncode = SourceSize / (kOpusFrameSizeSamples * SAMPLE_SIZE);
		uint32 TrueSampleCount = SourceSize / SAMPLE_SIZE;

		// Add another frame if Source does not divide into an equal number of frames
		if (SourceSize % (kOpusFrameSizeSamples * SAMPLE_SIZE) != 0)
		{
			FramesToEncode++;
		}

		check(QualityInfo.NumChannels <= MAX_uint8);
		check(FramesToEncode <= MAX_uint16);
		SerializeHeaderData(CompressedData, kOpusSampleRate, TrueSampleCount, QualityInfo.NumChannels, FramesToEncode);

		// Temporary storage for source data in an interleaved format
		TArray<uint8> TempInterleavedSrc;
		TempInterleavedSrc.AddUninitialized(kBytesPerFrame);

		// Temporary storage with more than enough to store any compressed frame
		TArray<uint8> TempCompressedData;
		TempCompressedData.AddUninitialized(kBytesPerFrame);

		while (SrcBufferOffset < SourceSize)
		{
			// Read a frames worth of data from the source and pack it into interleaved temporary storage
			for (int32 SampleIndex = 0; SampleIndex < kOpusFrameSizeSamples; ++SampleIndex)
			{
				int32 CurrSrcOffset = SrcBufferOffset + SampleIndex*SAMPLE_SIZE;
				int32 CurrInterleavedOffset = SampleIndex*kSampleStride;
				if (CurrSrcOffset < SourceSize)
				{
					check(QualityInfo.NumChannels <= 8); // Static analysis fix: warning C6385: Reading invalid data from 'Order':  the readable size is '256' bytes, but '8160' bytes may be read.
					for (uint32 ChannelIndex = 0; ChannelIndex < QualityInfo.NumChannels; ++ChannelIndex)
					{
						// Interleave the channels in the Vorbis format, so that the correct channel is used for LFE
						int32 OrderedChannelIndex = VorbisChannelInfo::Order[QualityInfo.NumChannels - 1][ChannelIndex];
						int32 CurrInterleavedIndex = CurrInterleavedOffset + ChannelIndex*SAMPLE_SIZE;

						// Copy both bytes that make up a single sample
						TempInterleavedSrc[CurrInterleavedIndex] = SrcBufferCopies[OrderedChannelIndex][CurrSrcOffset];
						TempInterleavedSrc[CurrInterleavedIndex + 1] = SrcBufferCopies[OrderedChannelIndex][CurrSrcOffset + 1];
					}
				}
				else
				{
					// Zero the rest of the temp buffer to make it an exact frame
					FMemory::Memzero(TempInterleavedSrc.GetData() + CurrInterleavedOffset, kBytesPerFrame - CurrInterleavedOffset);
					SampleIndex = kOpusFrameSizeSamples;
				}
			}

			int32 CompressedLength = opus_multistream_encode(Encoder, (const opus_int16*)(TempInterleavedSrc.GetData()), kOpusFrameSizeSamples, TempCompressedData.GetData(), TempCompressedData.Num());

			if (CompressedLength < 0)
			{
				const char* ErrorStr = opus_strerror(CompressedLength);
				UE_LOG(LogAudio, Warning, TEXT("Failed to encode: [%d] %s"), CompressedLength, ANSI_TO_TCHAR(ErrorStr));

				Destroy(Encoder);

				CompressedDataStore.Empty();
				return false;
			}
			else
			{
				// Store frame length and copy compressed data before incrementing pointers
				check(CompressedLength < MAX_uint16);
				SerialiseFrameData(CompressedData, TempCompressedData.GetData(), CompressedLength);
				SrcBufferOffset += kOpusFrameSizeSamples * SAMPLE_SIZE;
			}
		}

		Destroy(Encoder);

		return CompressedDataStore.Num() > 0;
	}

	virtual int32 Recompress(FName Format, const TArray<uint8>& SrcBuffer, FSoundQualityInfo& QualityInfo, TArray<uint8>& OutBuffer) const override
	{
		check(Format == NAME_OPUS);
		FOpusAudioInfo	AudioInfo;

		// Cannot quality preview multichannel sounds
		if( QualityInfo.NumChannels > 2 )
		{
			return 0;
		}

		TArray<uint8> CompressedDataStore;
		if( !Cook( Format, SrcBuffer, QualityInfo, CompressedDataStore ) )
		{
			return 0;
		}

		// Parse the opus header for the relevant information
		if( !AudioInfo.ReadCompressedInfo( CompressedDataStore.GetData(), CompressedDataStore.Num(), &QualityInfo ) )
		{
			return 0;
		}

		// Decompress all the sample data
		OutBuffer.Empty(QualityInfo.SampleDataSize);
		OutBuffer.AddZeroed(QualityInfo.SampleDataSize);
		AudioInfo.ExpandFile( OutBuffer.GetData(), &QualityInfo );

		return CompressedDataStore.Num();
	}

	virtual bool SplitDataForStreaming(const TArray<uint8>& SrcBuffer, TArray<TArray<uint8>>& OutBuffers) const override
	{
		// 16K chunks - don't really have an idea of what's best for loading from disc yet
		const int32 kMaxChunkSizeBytes = 16384;
		if (SrcBuffer.Num() == 0)
		{
			return false;
		}

		uint32 ReadOffset = 0;
		uint32 WriteOffset = 0;
		uint16 ProcessedFrames = 0;
		const uint8* LockedSrc = SrcBuffer.GetData();

		// Read Identifier, True Sample Count, Number of channels and Frames to Encode first
		if (FCStringAnsi::Strcmp((char*)LockedSrc, OPUS_ID_STRING) != 0)
		{
			return false;
		}
		ReadOffset += FCStringAnsi::Strlen(OPUS_ID_STRING) + 1;
		uint16 SampleRate = *((uint16*)(LockedSrc + ReadOffset));
		ReadOffset += sizeof(uint16);
		uint32 TrueSampleCount = *((uint32*)(LockedSrc + ReadOffset));
		ReadOffset += sizeof(uint32);
		uint8 NumChannels = *(LockedSrc + ReadOffset);
		ReadOffset += sizeof(uint8);
		uint16 SerializedFrames = *((uint16*)(LockedSrc + ReadOffset));
		ReadOffset += sizeof(uint16);

		// Should always be able to store basic info in a single chunk
		check(ReadOffset - WriteOffset < kMaxChunkSizeBytes)

		while (ProcessedFrames < SerializedFrames)
		{
			uint16 FrameSize = *((uint16*)(LockedSrc + ReadOffset));

			if ( (ReadOffset + sizeof(uint16) + FrameSize) - WriteOffset >= kMaxChunkSizeBytes)
			{
				WriteOffset += AddDataChunk(OutBuffers, LockedSrc + WriteOffset, ReadOffset - WriteOffset);
			}

			ReadOffset += sizeof(uint16) + FrameSize;
			ProcessedFrames++;
		}
		if (WriteOffset < ReadOffset)
		{
			WriteOffset += AddDataChunk(OutBuffers, LockedSrc + WriteOffset, ReadOffset - WriteOffset);
		}

		return true;
	}

	/**
	 * Calculate the best sample rate for the output opus data
	 */
	static uint16 GetBestOutputSampleRate(int32 SampleRate)
	{
		static const uint16 ValidSampleRates[] = 
		{
			0, // not really valid, but simplifies logic below
			8000,
			12000,
			16000,
			24000,
			48000,
		};

		// look for the next highest valid rate
		for (int32 Index = ARRAY_COUNT(ValidSampleRates) - 2; Index >= 0; Index--)
		{
			if (SampleRate > ValidSampleRates[Index])
			{
				return ValidSampleRates[Index + 1];
			}
		}
		// this should never get here!
		check(0);
		return 0;
	}

	bool ResamplePCM(uint32 NumChannels, const TArray<uint8>& InBuffer, uint32 InSampleRate, TArray<uint8>& OutBuffer, uint32 OutSampleRate) const
	{
		// Initialize resampler to convert to desired rate for Opus
		int32 err = 0;
		SpeexResamplerState* resampler = speex_resampler_init(NumChannels, InSampleRate, OutSampleRate, SPEEX_RESAMPLER_QUALITY_DESKTOP, &err);
		if (err != RESAMPLER_ERR_SUCCESS)
		{
			speex_resampler_destroy(resampler);
			return false;
		}

		// Calculate extra space required for sample rate
		const uint32 SampleStride = SAMPLE_SIZE * NumChannels;
		const float Duration = (float)InBuffer.Num() / (InSampleRate * SampleStride);
		const int32 SafeCopySize = (Duration + 1) * OutSampleRate * SampleStride;
		OutBuffer.Empty(SafeCopySize);
		OutBuffer.AddUninitialized(SafeCopySize);
		uint32 InSamples = InBuffer.Num() / SampleStride;
		uint32 OutSamples = OutBuffer.Num() / SampleStride;

		// Do resampling and check results
		if (NumChannels == 1)
		{
			err = speex_resampler_process_int(resampler, 0, (const short*)(InBuffer.GetData()), &InSamples, (short*)(OutBuffer.GetData()), &OutSamples);
		}
		else
		{
			err = speex_resampler_process_interleaved_int(resampler, (const short*)(InBuffer.GetData()), &InSamples, (short*)(OutBuffer.GetData()), &OutSamples);
		}

		speex_resampler_destroy(resampler);
		if (err != RESAMPLER_ERR_SUCCESS)
		{
			return false;
		}

		// reduce the size of Out Buffer if more space than necessary was allocated
		const int32 WrittenBytes = (int32)(OutSamples * SampleStride);
		if (WrittenBytes < OutBuffer.Num())
		{
			OutBuffer.SetNum(WrittenBytes, true);
		}

		return true;
	}

	int32 GetBitRateFromQuality(FSoundQualityInfo& QualityInfo) const
	{
		// There is no perfect way to map Vorbis' Quality setting to an Opus bitrate but this 
		// will use it as a multiplier to decide how much smaller than the original the
		// compressed data should be
		int32 OriginalBitRate = QualityInfo.SampleRate * QualityInfo.NumChannels * SAMPLE_SIZE * 8;
		return (float)OriginalBitRate * FMath::GetMappedRangeValueClamped(FVector2D(1, 100), FVector2D(0.04, 0.25), QualityInfo.Quality);
	}

	void SerializeHeaderData(FMemoryWriter& CompressedData, uint16 SampleRate, uint32 TrueSampleCount, uint8 NumChannels, uint16 NumFrames) const
	{
		const char* OpusIdentifier = OPUS_ID_STRING;
		CompressedData.Serialize((void*)OpusIdentifier, FCStringAnsi::Strlen(OpusIdentifier) + 1);
		CompressedData.Serialize(&SampleRate, sizeof(uint16));
		CompressedData.Serialize(&TrueSampleCount, sizeof(uint32));
		CompressedData.Serialize(&NumChannels, sizeof(uint8));
		CompressedData.Serialize(&NumFrames, sizeof(uint16));
	}

	void SerialiseFrameData(FMemoryWriter& CompressedData, uint8* FrameData, uint16 FrameSize) const
	{
		CompressedData.Serialize(&FrameSize, sizeof(uint16));
		CompressedData.Serialize(FrameData, FrameSize);
	}

	void Destroy(OpusEncoder* Encoder) const
	{
#if USE_UE4_MEM_ALLOC
		FMemory::Free(Encoder);
#else
		opus_encoder_destroy(Encoder);
#endif
	}

	void Destroy(OpusMSEncoder* Encoder) const
	{
#if USE_UE4_MEM_ALLOC
		FMemory::Free(Encoder);
#else
		opus_multistream_encoder_destroy(Encoder);
#endif
	}

	/**
	 * Adds a new chunk of data to the array
	 *
	 * @param	OutBuffers	Array of buffers to add to
	 * @param	ChunkData	Pointer to chunk data
	 * @param	ChunkSize	How much data to write
	 * @return	How many bytes were written
	 */
	int32 AddDataChunk(TArray<TArray<uint8>>& OutBuffers, const uint8* ChunkData, int32 ChunkSize) const
	{
		TArray<uint8>& NewBuffer = *new (OutBuffers) TArray<uint8>;
		NewBuffer.Empty(ChunkSize);
		NewBuffer.AddUninitialized(ChunkSize);
		FMemory::Memcpy(NewBuffer.GetData(), ChunkData, ChunkSize);
		return ChunkSize;
	}
};


/**
 * Module for opus audio compression
 */

static IAudioFormat* Singleton = NULL;

class FAudioPlatformOpusModule : public IAudioFormatModule
{
public:
	virtual ~FAudioPlatformOpusModule()
	{
		delete Singleton;
		Singleton = NULL;
	}
	virtual IAudioFormat* GetAudioFormat()
	{
		if (!Singleton)
		{
			Singleton = new FAudioFormatOpus();
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE( FAudioPlatformOpusModule, AudioFormatOpus);
