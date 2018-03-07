// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSAudioBuffer.cpp: Unreal IOSAudio buffer interface object.
 =============================================================================*/

/*------------------------------------------------------------------------------------
	Includes
 ------------------------------------------------------------------------------------*/

#include "IOSAudioDevice.h"
#include "AudioEffect.h"
#include "IAudioFormat.h"
#include "Sound/SoundWave.h"
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include "AudioDecompress.h"
#include "ADPCMAudioInfo.h"

/*------------------------------------------------------------------------------------
	FIOSAudioSoundBuffer
 ------------------------------------------------------------------------------------*/

FIOSAudioSoundBuffer::FIOSAudioSoundBuffer(FIOSAudioDevice* InAudioDevice, USoundWave* InWave, bool InStreaming):
	FSoundBuffer(InAudioDevice),
	SampleData(NULL),
	BufferSize(0),
	bStreaming(InStreaming)
{
	DecompressionState = static_cast<FADPCMAudioInfo*>(InAudioDevice->CreateCompressedAudioInfo(InWave));

	if (!ReadCompressedInfo(InWave))
	{
		return;
	}
	
	SoundFormat = static_cast<ESoundFormat>(*DecompressionState->WaveInfo.pFormatTag);
	SampleRate = InWave->SampleRate;
	NumChannels = InWave->NumChannels;
	BufferSize = AudioCallbackFrameSize * sizeof(uint16) * NumChannels;
	SampleData = static_cast<int16*>(FMemory::Malloc(BufferSize));
	check(SampleData != nullptr);
	
	FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
	check(AudioDeviceManager != nullptr);

	// There is no need to track this resource with the AudioDeviceManager since there is a one to one mapping between FIOSAudioSoundBuffer objects and FIOSAudioSoundSource objects
	//	and this object will be deleted when the corresponding FIOSAudioSoundSource no longer needs it
}

FIOSAudioSoundBuffer::~FIOSAudioSoundBuffer(void)
{
	if (bAllocationInPermanentPool)
	{
		UE_LOG(LogIOSAudio, Fatal, TEXT("Can't free resource '%s' as it was allocated in permanent pool."), *ResourceName);
	}
	
	if(SampleData != NULL)
	{
		FMemory::Free(SampleData);
		SampleData = NULL;
	}
	
	if(DecompressionState != NULL)
	{
		delete DecompressionState;
		DecompressionState = NULL;
	}
}

int32 FIOSAudioSoundBuffer::GetSize(void)
{
	int32 TotalSize = 0;
	
	switch (SoundFormat)
	{
		case SoundFormat_LPCM:
		case SoundFormat_ADPCM:
			TotalSize = BufferSize;
			break;
	}
	
	return TotalSize;
}

int32 FIOSAudioSoundBuffer::GetCurrentChunkIndex() const
{
	if(DecompressionState == NULL)
	{
		return -1;
	}
	
	return DecompressionState->GetCurrentChunkIndex();
}

int32 FIOSAudioSoundBuffer::GetCurrentChunkOffset() const
{
	if(DecompressionState == NULL)
	{
		return -1;
	}
	
	return DecompressionState->GetCurrentChunkOffset();
}

bool FIOSAudioSoundBuffer::ReadCompressedInfo(USoundWave* InWave)
{
	FSoundQualityInfo QualityInfo = { 0 };
	if(bStreaming)
	{
		return DecompressionState->StreamCompressedInfo(InWave, &QualityInfo);
	}

	InWave->InitAudioResource(FName(TEXT("ADPCM")));
	if (!InWave->ResourceData || InWave->ResourceSize <= 0)
	{
		InWave->RemoveAudioResource();
		return false;
	}

	return DecompressionState->ReadCompressedInfo(InWave->ResourceData, InWave->ResourceSize, &QualityInfo);
}

bool FIOSAudioSoundBuffer::ReadCompressedData( uint8* Destination, bool bLooping )
{
	if(bStreaming)
	{
		return DecompressionState->StreamCompressedData(Destination, bLooping, RenderCallbackBufferSize);
	}
	return DecompressionState->ReadCompressedData(Destination, bLooping, RenderCallbackBufferSize);
}

FIOSAudioSoundBuffer* FIOSAudioSoundBuffer::Init(FIOSAudioDevice* IOSAudioDevice, USoundWave* InWave)
{
	// Can't create a buffer without any source data
	if (InWave == NULL || InWave->NumChannels == 0)
	{
		return NULL;
	}

	FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();

	FIOSAudioSoundBuffer *Buffer = NULL;
	bool	bIsStreaming = false;
	
	switch (static_cast<EDecompressionType>(InWave->DecompressionType))
	{
		case DTYPE_Setup:
			// Has circumvented pre-cache mechanism - pre-cache now
			IOSAudioDevice->Precache(InWave, true, false);
			
			// Recall this function with new decompression type
			return Init(IOSAudioDevice, InWave);
			
		case DTYPE_Streaming:
			bIsStreaming = true;
			// fall through to next case
						
		case DTYPE_RealTime:
			// Always create a new FIOSAudioSoundBuffer since positional information about the sound is kept track of in this object
			Buffer = new FIOSAudioSoundBuffer(IOSAudioDevice, InWave, bIsStreaming);
			break;
			
		case DTYPE_Native:
		case DTYPE_Invalid:
		case DTYPE_Preview:
		case DTYPE_Procedural:
		default:
			// Invalid will be set if the wave cannot be played
			UE_LOG( LogIOSAudio, Warning, TEXT("Init Buffer on unsupported sound type name = %s type = %d"), *InWave->GetName(), int32(InWave->DecompressionType));
			break;
	}

	return Buffer;
}

