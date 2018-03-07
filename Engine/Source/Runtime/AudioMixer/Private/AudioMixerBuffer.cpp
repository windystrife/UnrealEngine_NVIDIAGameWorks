// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerBuffer.h"
#include "AudioMixerDevice.h"
#include "Interfaces/IAudioFormat.h"

namespace Audio
{

	FMixerBuffer::FMixerBuffer(FAudioDevice* InAudioDevice, USoundWave* InWave, EBufferType::Type InBufferType)
		: FSoundBuffer(InAudioDevice)
		, RealtimeAsyncHeaderParseTask(nullptr)
		, DecompressionState(nullptr)
		, SoundWaveProcedural(nullptr)
		, BufferType(InBufferType)
		, SampleRate(InWave->SampleRate)
		, BitsPerSample(16) // TODO: support more bits, currently hard-coded to 16
		, Data(nullptr)
		, DataSize(0)
		, bIsRealTimeSourceReady(false)
		, bIsDynamicResource(false)
	{
		if (InWave->DecompressionType == EDecompressionType::DTYPE_Native || InWave->DecompressionType == EDecompressionType::DTYPE_Preview)
		{
			Data = InWave->RawPCMData;
			DataSize = InWave->RawPCMDataSize;
 			InWave->RawPCMData = nullptr;
		}

		// Set the base-class NumChannels to wave's NumChannels
		NumChannels = InWave->NumChannels;
	}

	FMixerBuffer::~FMixerBuffer()
	{
		if (bAllocationInPermanentPool)
		{
			UE_LOG(LogAudioMixer, Fatal, TEXT("Can't free resource '%s' as it was allocated in permanent pool."), *ResourceName);
		}

		if (DecompressionState)
		{
			delete DecompressionState;
			DecompressionState = nullptr;
		}

		switch (BufferType)
		{
			case EBufferType::PCM:
			{
				if (Data)
				{
					FMemory::Free((void*)Data);
				}
			}
			break;

			case EBufferType::PCMPreview:
			{
				if (bIsDynamicResource && Data)
				{
					FMemory::Free((void*)Data);
				}
			}
			break;

			case EBufferType::PCMRealTime:
			case EBufferType::Streaming:
			// Buffers are freed as part of the ~FSoundSource
			break;

			case EBufferType::Invalid:
			// nothing
			break;
		}

		// Mark the procedural sound wave as being ok to be destroyed now
		if (SoundWaveProcedural)
		{
			// This is actually a 'bIsNotReadyForDestroy', but we can't change headers for a hotfix release.
			// This should be renamed as soon as possible, or a USoundWaveProcedural(FVTableHelper& Helper) constructor
			// should be added which sets this to true.
			SoundWaveProcedural->bIsReadyForDestroy = false;
		}
	}

	int32 FMixerBuffer::GetSize()
	{
		switch (BufferType)
		{
			case EBufferType::PCM:
			case EBufferType::PCMPreview:
				return DataSize;

			case EBufferType::PCMRealTime:
				return (DecompressionState ? DecompressionState->GetSourceBufferSize() : 0) + (MONO_PCM_BUFFER_SIZE * NumChannels);
			
			case EBufferType::Streaming:
				return MONO_PCM_BUFFER_SIZE * NumChannels;

			case EBufferType::Invalid:
			break;
		}

		return 0;
	}

	int32 FMixerBuffer::GetCurrentChunkIndex() const
	{
		if (DecompressionState)
		{
			return DecompressionState->GetCurrentChunkIndex();
		}

		return 0;
	}

	int32 FMixerBuffer::GetCurrentChunkOffset() const
	{
		if (DecompressionState)
		{
			return DecompressionState->GetCurrentChunkOffset();
		}
		return 0;
	}

	bool FMixerBuffer::IsRealTimeSourceReady()
	{
		// If we have a realtime async header parse task, then we check if its done
		if (RealtimeAsyncHeaderParseTask)
		{
			bool bIsDone = RealtimeAsyncHeaderParseTask->IsDone();
			if (bIsDone)
			{
				delete RealtimeAsyncHeaderParseTask;
				RealtimeAsyncHeaderParseTask = nullptr;
			}
			return bIsDone;
		}

		// Otherwise, we weren't a real time decoding sound buffer (or we've already asked and it was ready)
		return true;
	}

	bool FMixerBuffer::ReadCompressedInfo(USoundWave* SoundWave)
	{
		if (!DecompressionState)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("Attempting to read compressed info without a compression state instance for resource '%s'"), *ResourceName);
			return false;
		}
		return DecompressionState->ReadCompressedInfo(SoundWave->ResourceData, SoundWave->ResourceSize, nullptr);
	}

	bool FMixerBuffer::ReadCompressedData(uint8* Destination, bool bLooping)
	{
		return ReadCompressedData(Destination, MONO_PCM_BUFFER_SAMPLES, bLooping);
	}

	bool FMixerBuffer::ReadCompressedData(uint8* Destination, int32 NumFrames, bool bLooping)
	{
		if (!DecompressionState)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("Attempting to read compressed data without a compression state instance for resource '%s'"), *ResourceName);
			return false;
		}

		const int32 kPCMBufferSize = NumChannels * NumFrames * sizeof(int16);

		if (BufferType == EBufferType::Streaming)
		{
			return DecompressionState->StreamCompressedData(Destination, bLooping, kPCMBufferSize);
		}
		else
		{
			return DecompressionState->ReadCompressedData(Destination, bLooping, kPCMBufferSize);
		}
	}

	void FMixerBuffer::Seek(const float SeekTime)
	{
		if (ensure(DecompressionState))
		{
			DecompressionState->SeekToTime(SeekTime);
		}
	}

	FMixerBuffer* FMixerBuffer::Init(FAudioDevice* InAudioDevice, USoundWave* InWave, bool bForceRealtime)
	{
		// Can't create a buffer without any source data
		if (InWave == nullptr || InWave->NumChannels == 0)
		{
			return nullptr;
		}

		FAudioDeviceManager* AudioDeviceManager = FAudioDevice::GetAudioDeviceManager();

		FMixerDevice* Mixer = (FMixerDevice*)InAudioDevice;
		FMixerBuffer* Buffer = nullptr;

		EDecompressionType DecompressionType = InWave->DecompressionType;

		if (bForceRealtime  && DecompressionType != DTYPE_Setup && DecompressionType != DTYPE_Streaming)
		{
			DecompressionType = DTYPE_RealTime;
		}

		switch (DecompressionType)
		{
			case DTYPE_Setup:
			{
				// We've circumvented the level-load precache mechanism, precache synchronously // TODO: support async loading here?
				const bool bSynchronous = true;
				InAudioDevice->Precache(InWave, bSynchronous, false);
				check(InWave->DecompressionType != DTYPE_Setup);
				Buffer = Init(InAudioDevice, InWave, bForceRealtime);
			}
			break;

			case DTYPE_Preview:
			{
				// Find any existing buffers
				if (InWave->ResourceID)
				{
					Buffer = (FMixerBuffer*)AudioDeviceManager->GetSoundBufferForResourceID(InWave->ResourceID);
				}

				// Override with any new PCM data even if the buffer already exists
				if (InWave->RawPCMData)
				{
					// If we already have a buffer for this wave resource, free it
					if (Buffer)
					{
						AudioDeviceManager->FreeBufferResource(Buffer);
					}

					// Create a new preview buffer
					Buffer = FMixerBuffer::CreatePreviewBuffer(Mixer, InWave);

					// Track the new created buffer
					AudioDeviceManager->TrackResource(InWave, Buffer);
				}
			}
			break;

			case DTYPE_Procedural:
			{
				// Always create a new buffer for procedural or bus buffers
				Buffer = FMixerBuffer::CreateProceduralBuffer(Mixer, InWave);
			}
			break;

			case DTYPE_RealTime:
			{
				// Always create a new buffer for real-time buffers
				Buffer = FMixerBuffer::CreateRealTimeBuffer(Mixer, InWave);
			}
			break;

			case DTYPE_Native:
			{
				if (InWave->ResourceID)
				{
					Buffer = (FMixerBuffer*)AudioDeviceManager->GetSoundBufferForResourceID(InWave->ResourceID);
				}

				if (Buffer == nullptr)
				{
					Buffer = FMixerBuffer::CreateNativeBuffer(Mixer, InWave);

					// Track the resource with the audio device manager
					AudioDeviceManager->TrackResource(InWave, Buffer);
					InWave->RemoveAudioResource();
				}
			}
			break;

			case DTYPE_Streaming:
			{
				Buffer = FMixerBuffer::CreateStreamingBuffer(Mixer, InWave);
			}
			break;

			case DTYPE_Invalid:
			default:
			{
				// Invalid will be set if the wave cannot be played.
			}
			break;
		}

		return Buffer;
	}

	FMixerBuffer* FMixerBuffer::CreatePreviewBuffer(FMixerDevice* InMixer, USoundWave* InWave)
	{
		// Create a new buffer
		FMixerBuffer* Buffer = new FMixerBuffer(InMixer, InWave, EBufferType::PCMPreview);

		Buffer->bIsDynamicResource = InWave->bDynamicResource;
		return Buffer;
	}

	FMixerBuffer* FMixerBuffer::CreateProceduralBuffer(FMixerDevice* InMixer, USoundWave* InWave)
	{
		FMixerBuffer* Buffer = new FMixerBuffer(InMixer, InWave, EBufferType::PCMRealTime);

		// No tracking of this resource needed
		Buffer->ResourceID = 0;
		InWave->ResourceID = 0;

		// Don't allow the procedural sound wave to be destroyed until we're done with it
		Buffer->SoundWaveProcedural = Cast<USoundWaveProcedural>(InWave);

		// This is actually a 'bIsNotReadyForDestroy', but we can't change headers for a hotfix release.
		// This should be renamed as soon as possible, or a USoundWaveProcedural(FVTableHelper& Helper) constructor
		// should be added which sets this to true.
		if (Buffer->SoundWaveProcedural)
		{
			Buffer->SoundWaveProcedural->bIsReadyForDestroy = true;
		}

		return Buffer;
	}

	FMixerBuffer* FMixerBuffer::CreateNativeBuffer(FMixerDevice* InMixer, USoundWave* InWave)
	{
		check(InWave->bIsPrecacheDone);

		FMixerBuffer* Buffer = new FMixerBuffer(InMixer, InWave, EBufferType::PCM);
		return Buffer;
	}

	FMixerBuffer* FMixerBuffer::CreateStreamingBuffer(FMixerDevice* InMixer, USoundWave* InWave)
	{
		FMixerBuffer* Buffer = new FMixerBuffer(InMixer, InWave, EBufferType::Streaming);
		
		FSoundQualityInfo QualityInfo = { 0 };

		Buffer->DecompressionState = InMixer->CreateCompressedAudioInfo(InWave);

		// Get the header information of our compressed format
		if (Buffer->DecompressionState->StreamCompressedInfo(InWave, &QualityInfo))
		{
			// Refresh the wave data
			InWave->SampleRate = QualityInfo.SampleRate;
			InWave->NumChannels = QualityInfo.NumChannels;
			InWave->RawPCMDataSize = QualityInfo.SampleDataSize;
			InWave->Duration = QualityInfo.Duration;
		}
		else
		{
			InWave->DecompressionType = DTYPE_Invalid;
			InWave->NumChannels = 0;

			InWave->RemoveAudioResource();
		}

		return Buffer;
	}

	FMixerBuffer* FMixerBuffer::CreateRealTimeBuffer(FMixerDevice* InMixer, USoundWave* InWave)
	{
		check(InMixer);
		check(InWave);
		check(InWave->bIsPrecacheDone);

		// Create a new buffer for real-time sounds
		FMixerBuffer* Buffer = new FMixerBuffer(InMixer, InWave, EBufferType::PCMRealTime);

		if (InWave->ResourceData == nullptr)
		{
			InWave->InitAudioResource(InMixer->GetRuntimeFormat(InWave));
		}

		Buffer->DecompressionState = InMixer->CreateCompressedAudioInfo(InWave);

		if (Buffer->DecompressionState)
		{
			FHeaderParseAudioTaskData NewTaskData;
			NewTaskData.MixerBuffer = Buffer;
			NewTaskData.SoundWave = InWave;

			check(Buffer->RealtimeAsyncHeaderParseTask == nullptr);
			Buffer->RealtimeAsyncHeaderParseTask = CreateAudioTask(NewTaskData);

			Buffer->NumChannels = InWave->NumChannels;
		}
		else
		{
			InWave->DecompressionType = DTYPE_Invalid;
			InWave->NumChannels = 0;

			InWave->RemoveAudioResource();
		}

		return Buffer;
	}

	EBufferType::Type FMixerBuffer::GetType() const
	{
		return BufferType;
	}

	bool FMixerBuffer::IsRealTimeBuffer() const
	{
		return BufferType == EBufferType::PCMRealTime || BufferType == EBufferType::Streaming;
	}

	void FMixerBuffer::GetPCMData(uint8** OutData, uint32* OutDataSize)
	{
		*OutData = Data;
		*OutDataSize = DataSize;
	}

	void FMixerBuffer::EnsureHeaderParseTaskFinished()
	{
		if (RealtimeAsyncHeaderParseTask)
		{
			RealtimeAsyncHeaderParseTask->EnsureCompletion();
			delete RealtimeAsyncHeaderParseTask;
			RealtimeAsyncHeaderParseTask = nullptr;
		}
	}


}