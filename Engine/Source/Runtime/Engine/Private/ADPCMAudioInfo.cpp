// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "ADPCMAudioInfo.h"
#include "Interfaces/IAudioFormat.h"
#include "Sound/SoundWave.h"
#include "Audio.h"
#include "ContentStreaming.h"

#define WAVE_FORMAT_LPCM  1
#define WAVE_FORMAT_ADPCM 2

namespace ADPCM
{
	void DecodeBlock(const uint8* EncodedADPCMBlock, int32 BlockSize, int16* DecodedPCMData);
	void DecodeBlockStereo(const uint8* EncodedADPCMBlockLeft, const uint8* EncodedADPCMBlockRight, int32 BlockSize, int16* DecodedPCMData);
}

FADPCMAudioInfo::FADPCMAudioInfo(void)
{
	UncompressedBlockData = NULL;
	SamplesPerBlock = 0;
}

FADPCMAudioInfo::~FADPCMAudioInfo(void)
{
	if(UncompressedBlockData != NULL)
	{
		FMemory::Free(UncompressedBlockData);
		UncompressedBlockData = NULL;
	}
}

void FADPCMAudioInfo::SeekToTime(const float SeekTime)
{
	if(SeekTime == 0.0f)
	{
		CurrentUncompressedBlockSampleIndex = UncompressedBlockSize / sizeof(uint16);
		CurrentCompressedBlockIndex = 0;

		CurrentUncompressedBlockSampleIndex = 0;
		CurrentChunkIndex = 0;
		CurrentChunkBufferOffset = 0;
		TotalSamplesStreamed = 0;
		CurCompressedChunkData = NULL;
	}
	else
	{
		if (Format == WAVE_FORMAT_LPCM)
		{
			// There are no "blocks" on LPCM, so only update the total samples streamed (which is based off sample rate).
			// Note that TotalSamplesStreamed is per-channel in the ReadCompressedInfo. Channels are takin into account there.
			TotalSamplesStreamed = (uint32)(SeekTime * (float)(*WaveInfo.pSamplesPerSec));
		}
		else
		{
			// Figure out the block index that the seek takes us to
			uint32 SeekedSamples = (uint32)(SeekTime * (float)(*WaveInfo.pSamplesPerSec));

			// Compute the block index that we're seeked to
			CurrentCompressedBlockIndex = SeekedSamples / SamplesPerBlock;

			// Update the samples streamed to the current block index and the samples per block
			TotalSamplesStreamed = CurrentCompressedBlockIndex * SamplesPerBlock;
		}
	}
}

bool FADPCMAudioInfo::ReadCompressedInfo(const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, struct FSoundQualityInfo* QualityInfo)
{
	check(InSrcBufferData);

	SrcBufferData = InSrcBufferData;
	SrcBufferDataSize = InSrcBufferDataSize;

	void*	FormatHeader;
	
	if (!WaveInfo.ReadWaveInfo((uint8*)SrcBufferData, SrcBufferDataSize, NULL, false, &FormatHeader))
	{
		UE_LOG(LogAudio, Warning, TEXT("WaveInfo.ReadWaveInfo Failed"));
		return false;
	}

	Format = *WaveInfo.pFormatTag;
	NumChannels = *WaveInfo.pChannels;

	if (Format == WAVE_FORMAT_ADPCM)
	{
		ADPCM::ADPCMFormatHeader* ADPCMHeader = (ADPCM::ADPCMFormatHeader*)FormatHeader;
		TotalSamplesPerChannel = ADPCMHeader->SamplesPerChannel;
		SamplesPerBlock = ADPCMHeader->wSamplesPerBlock;

		const uint32 PreambleSize = 7;
		BlockSize = *WaveInfo.pBlockAlign;

		// ADPCM starts with 2 uncompressed samples and then the remaining compressed sample data has 2 samples per byte
		UncompressedBlockSize = (2 + (BlockSize - PreambleSize) * 2) * sizeof(int16);
		CompressedBlockSize = BlockSize;

		const uint32 uncompressedBlockSamples = (2 + (BlockSize - PreambleSize) * 2);
		const uint32 targetBlocks = MONO_PCM_BUFFER_SAMPLES / uncompressedBlockSamples;
		StreamBufferSize = targetBlocks * UncompressedBlockSize;
		// Ensure TotalDecodedSize is a even multiple of the compressed block size so that the buffer is not over read on the last block
		TotalDecodedSize = ((WaveInfo.SampleDataSize + CompressedBlockSize - 1) / CompressedBlockSize) * UncompressedBlockSize;

		UncompressedBlockData = (uint8*)FMemory::Realloc(UncompressedBlockData, NumChannels * UncompressedBlockSize);
		check(UncompressedBlockData != NULL);
		TotalCompressedBlocksPerChannel = (WaveInfo.SampleDataSize + CompressedBlockSize - 1) / CompressedBlockSize / NumChannels;
	}
	else if(Format == WAVE_FORMAT_LPCM)
	{
		// There are no "blocks" in this case
		BlockSize = 0;
		UncompressedBlockSize = 0;
		CompressedBlockSize = 0;
		StreamBufferSize = 0;
		UncompressedBlockData = NULL;
		TotalCompressedBlocksPerChannel = 0;
		
		TotalDecodedSize = WaveInfo.SampleDataSize;
		
		TotalSamplesPerChannel = TotalDecodedSize / sizeof(uint16) / NumChannels;
	}
	else
	{
		return false;
	}
	
	if (QualityInfo)
	{
		QualityInfo->SampleRate = *WaveInfo.pSamplesPerSec;
		QualityInfo->NumChannels = *WaveInfo.pChannels;
		QualityInfo->SampleDataSize = TotalDecodedSize;
		QualityInfo->Duration = (float)TotalSamplesPerChannel / QualityInfo->SampleRate;
	}
	
	CurrentCompressedBlockIndex = 0;
	TotalSamplesStreamed = 0;
	// This is set to the max value to trigger the decompression of the first audio block
	CurrentUncompressedBlockSampleIndex = UncompressedBlockSize / sizeof(uint16);
	
	return true;
}

bool FADPCMAudioInfo::ReadCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize)
{
	// This correctly handles any BufferSize as long as its a multiple of sample size * number of channels
	
	check(Destination);
	check((BufferSize % (sizeof(uint16) * NumChannels)) == 0);
	
	int16*	OutData = (int16*)Destination;
	
	bool	reachedEndOfSamples = false;
	
	if(Format == WAVE_FORMAT_ADPCM)
	{
		// We need to loop over the number of samples requested since an uncompressed block will not match the number of frames requested
		while(BufferSize > 0)
		{
			if(CurrentUncompressedBlockSampleIndex >= UncompressedBlockSize / sizeof(uint16))
			{
				// we need to decompress another block of compressed data from the current chunk
				
				// Decompress one block for each channel and store it in UncompressedBlockData
				for(int32 channelItr = 0; channelItr < NumChannels; ++channelItr)
				{
					ADPCM::DecodeBlock(
						WaveInfo.SampleDataStart + (channelItr * TotalCompressedBlocksPerChannel + CurrentCompressedBlockIndex) * CompressedBlockSize,
						CompressedBlockSize,
						(int16*)(UncompressedBlockData + channelItr * UncompressedBlockSize));
				}
				
				// Update some bookkeeping
				CurrentUncompressedBlockSampleIndex = 0;
				++CurrentCompressedBlockIndex;
			}
			
			// Only copy over the number of samples we currently have available, we will loop around if needed
			uint32	decompressedSamplesToCopy = FMath::Min<uint32>(UncompressedBlockSize / sizeof(uint16) - CurrentUncompressedBlockSampleIndex, BufferSize / (sizeof(uint16) * NumChannels));
			check(decompressedSamplesToCopy > 0);
			
			// Ensure we don't go over the number of samples left in the audio data
			if(decompressedSamplesToCopy > TotalSamplesPerChannel - TotalSamplesStreamed)
			{
				decompressedSamplesToCopy = TotalSamplesPerChannel - TotalSamplesStreamed;
			}
			
			// Copy over the actual sample data
			for(uint32 sampleItr = 0; sampleItr < decompressedSamplesToCopy; ++sampleItr)
			{
				for(int32 channelItr = 0; channelItr < NumChannels; ++channelItr)
				{
					uint16	value = *(int16*)(UncompressedBlockData + channelItr * UncompressedBlockSize + (CurrentUncompressedBlockSampleIndex + sampleItr) * sizeof(int16));
					*OutData++ = value;
				}
			}
			
			// Update bookkeeping
			CurrentUncompressedBlockSampleIndex += decompressedSamplesToCopy;
			BufferSize -= decompressedSamplesToCopy * sizeof(uint16) * NumChannels;
			TotalSamplesStreamed += decompressedSamplesToCopy;
			
			// Check for the end of the audio samples and loop if needed
			if(TotalSamplesStreamed >= TotalSamplesPerChannel)
			{
				reachedEndOfSamples = true;
				// This is set to the max value to trigger the decompression of the first audio block
				CurrentUncompressedBlockSampleIndex = UncompressedBlockSize / sizeof(uint16);
				CurrentCompressedBlockIndex = 0;
				TotalSamplesStreamed = 0;
				if(!bLooping)
				{
					// Set the remaining buffer to 0
					memset(OutData, 0, BufferSize);
					return true;
				}
			}
		}
	}
	else
	{
		uint32	decompressedSamplesToCopy = BufferSize / (sizeof(uint16) * NumChannels);
			
		// Ensure we don't go over the number of samples left in the audio data
		if(decompressedSamplesToCopy > TotalSamplesPerChannel - TotalSamplesStreamed)
		{
			decompressedSamplesToCopy = TotalSamplesPerChannel - TotalSamplesStreamed;
		}

		memcpy(OutData, WaveInfo.SampleDataStart + TotalSamplesStreamed * sizeof(uint16) * NumChannels, decompressedSamplesToCopy * sizeof(uint16) * NumChannels);
		TotalSamplesStreamed += decompressedSamplesToCopy;
		BufferSize -= decompressedSamplesToCopy * sizeof(uint16) * NumChannels;
		
		// Check for the end of the audio samples and loop if needed
		if(TotalSamplesStreamed >= TotalSamplesPerChannel)
		{
			reachedEndOfSamples = true;
			TotalSamplesStreamed = 0;
			if(!bLooping)
			{
				// Set the remaining buffer to 0
				memset(OutData, 0, BufferSize);
				return true;
			}
		}
	}
	
	return reachedEndOfSamples;
}

void FADPCMAudioInfo::ExpandFile(uint8* DstBuffer, struct FSoundQualityInfo* QualityInfo)
{
	check(DstBuffer);

	ReadCompressedData(DstBuffer, false, TotalDecodedSize);
}

int FADPCMAudioInfo::GetStreamBufferSize() const
{
	return StreamBufferSize;
}

bool FADPCMAudioInfo::StreamCompressedInfo(USoundWave* Wave, struct FSoundQualityInfo* QualityInfo)
{
	check(QualityInfo);

	StreamingSoundWave = Wave;

	// Get the first chunk of audio data (should already be loaded)
	uint8 const* firstChunk = IStreamingManager::Get().GetAudioStreamingManager().GetLoadedChunk(Wave, 0, &CurrentChunkDataSize);

	if (firstChunk == NULL)
	{
		return false;
	}

	SrcBufferData = NULL;
	SrcBufferDataSize = 0;
	
	void*	FormatHeader;
	
	if (!WaveInfo.ReadWaveInfo((uint8*)firstChunk, Wave->RunningPlatformData->Chunks[0].DataSize, NULL, true, &FormatHeader))
	{
		UE_LOG(LogAudio, Warning, TEXT("WaveInfo.ReadWaveInfo Failed"));
		return false;
	}

	FirstChunkSampleDataOffset = WaveInfo.SampleDataStart - firstChunk;
	CurrentChunkBufferOffset = 0;
	CurCompressedChunkData = NULL;
	CurrentUncompressedBlockSampleIndex = 0;
	CurrentChunkIndex = 0;
	TotalSamplesStreamed = 0;
	Format = *WaveInfo.pFormatTag;
	NumChannels = *WaveInfo.pChannels;
	
	if (Format == WAVE_FORMAT_ADPCM)
	{
		ADPCM::ADPCMFormatHeader* APPCMHeader = (ADPCM::ADPCMFormatHeader*)FormatHeader;
		TotalSamplesPerChannel = APPCMHeader->SamplesPerChannel;

		const uint32 PreambleSize = 7;

		BlockSize = *WaveInfo.pBlockAlign;
		UncompressedBlockSize = (2 + (BlockSize - PreambleSize) * 2) * sizeof(int16);
		CompressedBlockSize = BlockSize;

		// Calculate buffer sizes and total number of samples
		const uint32 uncompressedBlockSamples = (2 + (BlockSize - PreambleSize) * 2);
		const uint32 targetBlocks = MONO_PCM_BUFFER_SAMPLES / uncompressedBlockSamples;
		StreamBufferSize = targetBlocks * UncompressedBlockSize;
		TotalDecodedSize = ((WaveInfo.SampleDataSize + CompressedBlockSize - 1) / CompressedBlockSize) * UncompressedBlockSize;
		
		UncompressedBlockData = (uint8*)FMemory::Realloc(UncompressedBlockData, NumChannels * UncompressedBlockSize);
		check(UncompressedBlockData != NULL);
	}
	else if (Format == WAVE_FORMAT_LPCM)
	{
		BlockSize = 0;
		UncompressedBlockSize = 0;
		CompressedBlockSize = 0;

		// This is uncompressed, so decoded size and buffer size are the same
		TotalDecodedSize = WaveInfo.SampleDataSize;
		StreamBufferSize = WaveInfo.SampleDataSize;
		TotalSamplesPerChannel = StreamBufferSize / sizeof(uint16) / NumChannels;
	}
	else
	{
		UE_LOG(LogAudio, Error, TEXT("Unsupported wave format"));
		return false;
	}
	
	if (QualityInfo)
	{
		QualityInfo->SampleRate = *WaveInfo.pSamplesPerSec;
		QualityInfo->NumChannels = *WaveInfo.pChannels;
		QualityInfo->SampleDataSize = TotalDecodedSize;
		QualityInfo->Duration = (float)TotalSamplesPerChannel / QualityInfo->SampleRate;
	}

	return true;
}

bool FADPCMAudioInfo::StreamCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize)
{
	// Destination samples are interlaced by channel, BufferSize is in bytes
	
	// Ensure that BuffserSize is a multiple of the sample size times the number of channels
	check((BufferSize % (sizeof(uint16) * NumChannels)) == 0);
	
	int16*	OutData = (int16*)Destination;
	
	bool	reachedEndOfSamples = false;

	if(Format == WAVE_FORMAT_ADPCM)
	{
		// We need to loop over the number of samples requested since an uncompressed block will not match the number of frames requested
		while(BufferSize > 0)
		{
			if(CurCompressedChunkData == NULL || CurrentUncompressedBlockSampleIndex >= UncompressedBlockSize / sizeof(uint16))
			{
				// we need to decompress another block of compressed data from the current chunk
				
				if(CurCompressedChunkData == NULL || CurrentChunkBufferOffset >= CurrentChunkDataSize)
				{
					// Get another chunk of compressed data from the streaming engine
					
					// CurrentChunkIndex is used to keep track of the current chunk for loading/unloading by the streaming engine but chunk 0 is preloaded so we don't want to increment this when getting chunk 0, only later chunks
					// Also, if we failed to get a previous chunk because it was not ready we don't want to re-increment the CurrentChunkIndex
					if(CurCompressedChunkData != NULL)
					{
						++CurrentChunkIndex;
					}
					
					// Request the next chunk of data from the streaming engine
					CurCompressedChunkData = IStreamingManager::Get().GetAudioStreamingManager().GetLoadedChunk(StreamingSoundWave, CurrentChunkIndex, &CurrentChunkDataSize);

					if(CurCompressedChunkData == NULL)
					{
						// If we did not get it then just bail, CurrentChunkIndex will not get incremented on the next callback so in effect another attempt will be made to fetch the chunk
						// Since audio streaming depends on the general data streaming mechanism used by other parts of the engine and new data is prefectched on the game tick thread its possible a game hickup can cause this
						UE_LOG(LogAudio, Warning, TEXT("Missed Deadline chunk %d"), CurrentChunkIndex);
						
						// zero out remaining data and bail
						memset(OutData, 0, BufferSize);
						return false;
					}
					
					// Set the current buffer offset accounting for the header in the first chunk
					CurrentChunkBufferOffset = CurrentChunkIndex == 0 ? FirstChunkSampleDataOffset : 0;
				}
				
				// Decompress one block for each channel and store it in UncompressedBlockData
				for(int32 channelItr = 0; channelItr < NumChannels; ++channelItr)
				{
					ADPCM::DecodeBlock(
						CurCompressedChunkData + CurrentChunkBufferOffset + channelItr * CompressedBlockSize,
						CompressedBlockSize,
						(int16*)(UncompressedBlockData + channelItr * UncompressedBlockSize));
				}
				
				// Update some bookkeeping
				CurrentUncompressedBlockSampleIndex = 0;
				CurrentChunkBufferOffset += NumChannels * CompressedBlockSize;
			}
			
			// Only copy over the number of samples we currently have available, we will loop around if needed
			uint32	decompressedSamplesToCopy = FMath::Min<uint32>(UncompressedBlockSize / sizeof(uint16) - CurrentUncompressedBlockSampleIndex, BufferSize / (sizeof(uint16) * NumChannels));
			check(decompressedSamplesToCopy > 0);
			
			// Ensure we don't go over the number of samples left in the audio data
			if(decompressedSamplesToCopy > TotalSamplesPerChannel - TotalSamplesStreamed)
			{
				decompressedSamplesToCopy = TotalSamplesPerChannel - TotalSamplesStreamed;
			}
			
			// Copy over the actual sample data
			for(uint32 sampleItr = 0; sampleItr < decompressedSamplesToCopy; ++sampleItr)
			{
				for(int32 channelItr = 0; channelItr < NumChannels; ++channelItr)
				{
					uint16	value = *(int16*)(UncompressedBlockData + channelItr * UncompressedBlockSize + (CurrentUncompressedBlockSampleIndex + sampleItr) * sizeof(int16));
					*OutData++ = value;
				}
			}
			
			// Update bookkeeping
			CurrentUncompressedBlockSampleIndex += decompressedSamplesToCopy;
			BufferSize -= decompressedSamplesToCopy * sizeof(uint16) * NumChannels;
			TotalSamplesStreamed += decompressedSamplesToCopy;
			
			// Check for the end of the audio samples and loop if needed
			if(TotalSamplesStreamed >= TotalSamplesPerChannel)
			{
				reachedEndOfSamples = true;
				CurrentUncompressedBlockSampleIndex = 0;
				CurrentChunkIndex = 0;
				CurrentChunkBufferOffset = 0;
				TotalSamplesStreamed = 0;
				CurCompressedChunkData = NULL;
				if(!bLooping)
				{
					// Set the remaining buffer to 0
					memset(OutData, 0, BufferSize);
					return true;
				}
			}
		}
	}
	else
	{
		while(BufferSize > 0)
		{
			if(CurCompressedChunkData == NULL || CurrentChunkBufferOffset >= CurrentChunkDataSize)
			{
				// Get another chunk of compressed data from the streaming engine
				
				// CurrentChunkIndex is used to keep track of the current chunk for loading/unloading by the streaming engine but chunk 0 is preloaded so we don't want to increment this when getting chunk 0, only later chunks
				// Also, if we failed to get a previous chunk because it was not ready we don't want to re-increment the CurrentChunkIndex
				if(CurCompressedChunkData != NULL)
				{
					++CurrentChunkIndex;
				}
				
				// Request the next chunk of data from the streaming engine
				CurCompressedChunkData = IStreamingManager::Get().GetAudioStreamingManager().GetLoadedChunk(StreamingSoundWave, CurrentChunkIndex, &CurrentChunkDataSize);

				if(CurCompressedChunkData == NULL)
				{
					// If we did not get it then just bail, CurrentChunkIndex will not get incremented on the next callback so in effect another attempt will be made to fetch the chunk
					// Since audio streaming depends on the general data streaming mechanism used by other parts of the engine and new data is prefectched on the game tick thread its possible a game hickup can cause this
					UE_LOG(LogAudio, Warning, TEXT("Missed Deadline chunk %d"), CurrentChunkIndex);
					
					// zero out remaining data and bail
					memset(OutData, 0, BufferSize);
					return false;
				}
				
				// Set the current buffer offset accounting for the header in the first chunk
				CurrentChunkBufferOffset = CurrentChunkIndex == 0 ? FirstChunkSampleDataOffset : 0;
			}

			uint32	decompressedSamplesToCopy = FMath::Min<uint32>((CurrentChunkDataSize - CurrentChunkBufferOffset) / (sizeof(uint16) * NumChannels), BufferSize / (sizeof(uint16) * NumChannels));
			check(decompressedSamplesToCopy > 0);
			
			// Ensure we don't go over the number of samples left in the audio data
			if(decompressedSamplesToCopy > TotalSamplesPerChannel - TotalSamplesStreamed)
			{
				decompressedSamplesToCopy = TotalSamplesPerChannel - TotalSamplesStreamed;
			}
			
			memcpy(OutData, CurCompressedChunkData + CurrentChunkBufferOffset, decompressedSamplesToCopy * (sizeof(uint16) * NumChannels));
			
			OutData += decompressedSamplesToCopy * NumChannels;
			CurrentChunkBufferOffset += decompressedSamplesToCopy * (sizeof(uint16) * NumChannels);
			BufferSize -= decompressedSamplesToCopy * (sizeof(uint16) * NumChannels);
			TotalSamplesStreamed += decompressedSamplesToCopy;
			
			// Check for the end of the audio samples and loop if needed
			if(TotalSamplesStreamed >= TotalSamplesPerChannel)
			{
				reachedEndOfSamples = true;
				CurrentChunkIndex = 0;
				CurrentChunkBufferOffset = 0;
				TotalSamplesStreamed = 0;
				CurCompressedChunkData = NULL;
				if(!bLooping)
				{
					// Set the remaining buffer to 0
					memset(OutData, 0, BufferSize);
					return true;
				}
			}
		}
	}
	
	return reachedEndOfSamples;
}
