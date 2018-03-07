// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Audio.h"
#include "ALAudioDevice.h"
#include "AudioDecompress.h"

/*------------------------------------------------------------------------------------
	FALSoundBuffer.
------------------------------------------------------------------------------------*/
/**
 * Constructor
 *
 * @param AudioDevice	audio device this sound buffer is going to be attached to.
 */
FALSoundBuffer::FALSoundBuffer( FALAudioDevice* InAudioDevice )
	: FSoundBuffer(InAudioDevice),
	  BufferId(0)
{
}

/**
 * Destructor
 *
 * Frees wave data and detaches itself from audio device.
 */
FALSoundBuffer::~FALSoundBuffer( void )
{
	// Delete AL buffers.
	alDeleteBuffers(1, &BufferId);
}

/**
 * Static function used to create a buffer.
 *
 * @param InWave		USoundNodeWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @param	bIsPrecacheRequest	Whether this request is for precaching or not
 * @return	FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */

FALSoundBuffer* FALSoundBuffer::Init(FALAudioDevice* AudioDevice, USoundWave* InWave)
{
	// Can't create a buffer without any source data
	if (InWave == nullptr || InWave->NumChannels == 0)
	{
		return nullptr;
	}

	FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
	check(AudioDeviceManager != nullptr);

	FALSoundBuffer *Buffer = nullptr;

	switch (static_cast<EDecompressionType>(InWave->DecompressionType))
	{
	case DTYPE_Setup:
		// Has circumvented pre-cache mechanism - pre-cache now
		AudioDevice->Precache(InWave, true, false);

		// Recall this function with new decompression type
		return Init(AudioDevice, InWave);

	case DTYPE_Native:
		if (InWave->ResourceID)
		{
			Buffer = static_cast<FALSoundBuffer*>(AudioDeviceManager->WaveBufferMap.FindRef(InWave->ResourceID));
		}

		if (!Buffer || !Buffer->BufferId)
		{
			CreateNativeBuffer(AudioDevice, InWave, Buffer);
		}
		break;

		// Always create a new buffer for streaming ogg vorbis data
		//Buffer = CreateQueuedBuffer(AudioDevice, InWave);
		//break;

	case DTYPE_Invalid:
	case DTYPE_Preview:
	case DTYPE_Procedural:
	case DTYPE_RealTime:
	default:
		// Invalid will be set if the wave cannot be played
		UE_LOG(LogALAudio, Warning, TEXT("ALSoundBuffer wave '%s' has an invalid decompression type %d."),
			*InWave->GetName(), static_cast<EDecompressionType>(InWave->DecompressionType));
		break;
	}

	if (Buffer == nullptr)
	{
		UE_LOG(LogALAudio, Warning, TEXT("ALSoundBuffer init failed for wave '%s', decompression type %d."),
			*InWave->GetName(), static_cast<EDecompressionType>(InWave->DecompressionType));
	}

	return Buffer;
}

void FALSoundBuffer::CreateNativeBuffer(FALAudioDevice* AudioDevice, USoundWave* Wave, FALSoundBuffer* &Buffer)
{
	// Check if Buffer already exists
	if (Buffer)
	{
		// Assign the new AudioDevice
		Buffer->AudioDevice = AudioDevice;
	}
	else
	{
		SCOPE_CYCLE_COUNTER( STAT_AudioResourceCreationTime );

		check(Wave->bIsPrecacheDone);

		// Create new buffer.
		Buffer = new FALSoundBuffer(AudioDevice);

		Buffer->InternalFormat = AudioDevice->GetInternalFormat(Wave->NumChannels);
		Buffer->NumChannels = Wave->NumChannels;
		Buffer->SampleRate = Wave->SampleRate;

		FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
		check(AudioDeviceManager != nullptr);
		AudioDeviceManager->TrackResource(Wave, Buffer);
	}

	// Generate the new OpenAL buffer
	alGenBuffers(1, &Buffer->BufferId);
	AudioDevice->alError(TEXT("RegisterSound"));

	if (Wave->RawPCMData)
	{
		// upload it
		Buffer->BufferSize = Wave->RawPCMDataSize;
		alBufferData(Buffer->BufferId, Buffer->InternalFormat, Wave->RawPCMData, Wave->RawPCMDataSize, Buffer->SampleRate);

		// Free up the data if necessary
		if (Wave->bDynamicResource)
		{
			FMemory::Free( Wave->RawPCMData );
			Wave->RawPCMData = nullptr;
			Wave->bDynamicResource = false;
		}
	}
	else
	{
		// get the raw data
		uint8* SoundData = reinterpret_cast<uint8*>(Wave->RawData.Lock(LOCK_READ_ONLY));
		// it's (possibly) a pointer to a wave file, so skip over the header
		int SoundDataSize = Wave->RawData.GetBulkDataSize();

		// is there a wave header?
		FWaveModInfo WaveInfo;
		if (WaveInfo.ReadWaveInfo(SoundData, SoundDataSize))
		{
			// if so, modify the location and size of the sound data based on header
			SoundData = WaveInfo.SampleDataStart;
			SoundDataSize = WaveInfo.SampleDataSize;
		}
		// let the Buffer know the final size
		Buffer->BufferSize = SoundDataSize;

		// upload it
		alBufferData( Buffer->BufferId, Buffer->InternalFormat, SoundData, Buffer->BufferSize, Buffer->SampleRate );
		// unload it
		Wave->RawData.Unlock();
	}

	if (AudioDevice->alError(TEXT( "RegisterSound (buffer data)")) || (Buffer->BufferSize == 0))
	{
		Buffer->InternalFormat = 0;
	}

	if (Buffer->InternalFormat == 0)
	{
		UE_LOG(LogAudio, Log,TEXT( "Audio: sound format not supported for '%s' (%d)" ), *Wave->GetName(), Wave->NumChannels);
		UE_LOG(LogALAudio, Warning, TEXT("ALSoundBuffer: sound format not supported for wave '%s'"), *Wave->GetName());

		delete Buffer;
		Buffer = nullptr;
	}
}
