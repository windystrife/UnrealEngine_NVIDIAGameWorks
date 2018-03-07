// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidAudioDevice.h"
#include "AudioEffect.h"
#include "IAudioFormat.h"
#include "AudioDecompress.h"
#include "ContentStreaming.h"

/*------------------------------------------------------------------------------------
	FSLESSoundBuffer.
------------------------------------------------------------------------------------*/
/** 
 * Constructor
 *
 * @param AudioDevice	audio device this sound buffer is going to be attached to.
 */
FSLESSoundBuffer::FSLESSoundBuffer( FSLESAudioDevice* InAudioDevice )
:	FSoundBuffer(InAudioDevice),
	AudioData(NULL),
	DecompressionState( NULL ),
	Format(SoundFormat_Invalid)
{
	
}

/**
 * Destructor 
 * 
 * Frees wave data and detaches itself from audio device.
 */
FSLESSoundBuffer::~FSLESSoundBuffer( void )
{
	FMemory::Free( AudioData);

	if( DecompressionState )
	{
		delete DecompressionState;
	}
}

FSLESSoundBuffer* FSLESSoundBuffer::CreateQueuedBuffer( FSLESAudioDevice* AudioDevice, USoundWave* InWave )
{
	check(InWave->bIsPrecacheDone);
	
	// Always create a new buffer for real time decompressed sounds
	FSLESSoundBuffer* Buffer = new FSLESSoundBuffer( AudioDevice);
	
	// Prime the first two buffers and prepare the decompression
	FSoundQualityInfo QualityInfo = { 0 };
	
	Buffer->DecompressionState = AudioDevice->CreateCompressedAudioInfo(InWave);

	// If the buffer was precached as native, the resource data will have been lost and we need to re-initialize it
	if (InWave->ResourceData == nullptr)
	{
		InWave->InitAudioResource(AudioDevice->GetRuntimeFormat(InWave));
	}

	if (Buffer->DecompressionState && Buffer->DecompressionState->ReadCompressedInfo(InWave->ResourceData, InWave->ResourceSize, &QualityInfo))
	{	
		// Clear out any dangling pointers
		Buffer->AudioData = NULL;
		Buffer->BufferSize = 0;
		
		// Keep track of associated resource name.
		Buffer->ResourceName	= InWave->GetPathName();		
		Buffer->NumChannels		= InWave->NumChannels;
		Buffer->SampleRate		= InWave->SampleRate;

		//Android can't handle more than 48kHz, so turn on halfrate decoding and adjust parameters
		if (Buffer->SampleRate > 48000)
		{
			UE_LOG(LogAndroidAudio, Log, TEXT( "Converting %s to halfrate from %d" ), *InWave->GetName(), Buffer->SampleRate );
			Buffer->DecompressionState->EnableHalfRate( true);
			Buffer->SampleRate = Buffer->SampleRate / 2;
			InWave->SampleRate = InWave->SampleRate / 2;
			uint32 SampleCount = QualityInfo.SampleDataSize / (QualityInfo.NumChannels * sizeof(uint16));
			SampleCount /= 2;
			InWave->RawPCMDataSize = SampleCount * QualityInfo.NumChannels * sizeof(uint16);;
		}

		Buffer->Format = SoundFormat_PCMRT;	
	}
	else
	{
		InWave->DecompressionType = DTYPE_Invalid;
		InWave->NumChannels = 0;
		
		InWave->RemoveAudioResource();
	}
	
	return Buffer;
}

FSLESSoundBuffer* FSLESSoundBuffer::CreateStreamBuffer( FSLESAudioDevice* AudioDevice, USoundWave* InWave )
{
	// Always create a new buffer for streaming
	FSLESSoundBuffer* Buffer = new FSLESSoundBuffer( AudioDevice);
	
	FSoundQualityInfo QualityInfo = { 0 };
	
	Buffer->DecompressionState = AudioDevice->CreateCompressedAudioInfo(InWave);

	if (Buffer->DecompressionState && Buffer->DecompressionState->StreamCompressedInfo(InWave, &QualityInfo))
	{	
		// Clear out any dangling pointers
		Buffer->AudioData = NULL;
		Buffer->BufferSize = 0;
		
		// Keep track of associated resource name.
		Buffer->ResourceName	= InWave->GetPathName();		
		Buffer->NumChannels		= InWave->NumChannels;
		Buffer->SampleRate		= InWave->SampleRate;

		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("DEBUG: FSLESSoundBuffer::CreateStreamBuffer Buffer->SampleRate = %d"), Buffer->SampleRate);

		//Android can't handle more than 48kHz, so turn on halfrate decoding and adjust parameters
		if (Buffer->SampleRate > 48000)
		{
			UE_LOG(LogAndroidAudio, Log, TEXT( "Converting %s to halfrate from %d" ), *InWave->GetName(), Buffer->SampleRate );
			Buffer->DecompressionState->EnableHalfRate( true);
			Buffer->SampleRate = Buffer->SampleRate / 2;
			InWave->SampleRate = InWave->SampleRate / 2;
			uint32 SampleCount = QualityInfo.SampleDataSize / (QualityInfo.NumChannels * sizeof(uint16));
			SampleCount /= 2;
			InWave->RawPCMDataSize = SampleCount * QualityInfo.NumChannels * sizeof(uint16);;
		}

		Buffer->Format = SoundFormat_Streaming;	
	}
	else
	{
		InWave->DecompressionType = DTYPE_Invalid;
		InWave->NumChannels = 0;
		
		InWave->RemoveAudioResource();
	}
	
	return Buffer;
}

/**
 * Static function used to create an OpenSL buffer and upload decompressed ogg vorbis data to.
 *
 * @param InWave			USoundWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @return FSLESSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FSLESSoundBuffer* FSLESSoundBuffer::CreateNativeBuffer( FSLESAudioDevice* AudioDevice, USoundWave* InWave )
{
	FSLESSoundBuffer* Buffer = NULL;

	// Create new buffer.
	Buffer = new FSLESSoundBuffer( AudioDevice );

	Buffer->DecompressionState = AudioDevice->CreateCompressedAudioInfo(InWave);

	FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
	check(AudioDeviceManager != nullptr);

	AudioDeviceManager->TrackResource(InWave, Buffer);

	Buffer->NumChannels		= InWave->NumChannels;
	Buffer->SampleRate		= InWave->SampleRate;

	// Take ownership the PCM data
	Buffer->AudioData = InWave->RawPCMData;
	Buffer->BufferSize = InWave->RawPCMDataSize;

	Buffer->Format = SoundFormat_PCM;
	
	InWave->RawPCMData = NULL;

	InWave->RemoveAudioResource();

	return Buffer;
}

/**
* Static function used to create an Audio buffer and dynamically upload procedural data to.
*
* @param InWave		USoundWave to use as template and wave source
* @param AudioDevice	audio device to attach created buffer to
* @return FSLESSoundBuffer pointer if buffer creation succeeded, NULL otherwise
*/
FSLESSoundBuffer* FSLESSoundBuffer::CreateProceduralBuffer(FSLESAudioDevice* AudioDevice, USoundWave* InWave)
{
	FSLESSoundBuffer* Buffer = new FSLESSoundBuffer(AudioDevice);

	// Setup any default information
	Buffer->DecompressionState = NULL;
	Buffer->AudioData = NULL;
	Buffer->BufferSize = 0;
	Buffer->Format = SoundFormat_PCMRT;
	Buffer->NumChannels = InWave->NumChannels;
	Buffer->SampleRate = InWave->SampleRate;
	
	InWave->RawPCMData = NULL;

	// No tracking of this resource as it's temporary
	Buffer->ResourceID = 0;
	InWave->ResourceID = 0;

	return Buffer;
}

/**
 * Static function used to create a buffer.
 *
 * @param InWave		USoundNodeWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @param	bIsPrecacheRequest	Whether this request is for precaching or not
 * @return	FSLESSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */

FSLESSoundBuffer* FSLESSoundBuffer::Init(  FSLESAudioDevice* AudioDevice ,USoundWave* InWave )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioResourceCreationTime );

	// Can't create a buffer without any source data
	if( InWave == NULL || InWave->NumChannels == 0 )
	{
		UE_LOG( LogAndroidAudio, Warning, TEXT("InitBuffer with Null sound data"));
		return( NULL );
	}
	
	FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
	FSLESSoundBuffer* Buffer = NULL;
	
	EDecompressionType DecompressionType = InWave->DecompressionType;

	UE_LOG(LogAndroidAudio, Verbose, TEXT("Init: Using decompression type: %d"), int32(DecompressionType));

	switch( DecompressionType )
	{
	case DTYPE_Setup:
		// Has circumvented precache mechanism - precache now
		AudioDevice->Precache(InWave, true, false);

		// if it didn't change, we will recurse forever
		check(InWave->DecompressionType != DTYPE_Setup);

		// Recall this function with new decompression type
		return( Init( AudioDevice, InWave ) );
		break;

	case DTYPE_Native:
		// Upload entire wav
		if( InWave->ResourceID )
		{
			Buffer = static_cast<FSLESSoundBuffer*>(AudioDeviceManager->WaveBufferMap.FindRef(InWave->ResourceID));
		}

		if( Buffer == NULL )
		{
			Buffer = CreateNativeBuffer( AudioDevice, InWave );
		}
		break;

	case DTYPE_RealTime:
		// Always create a new buffer for streaming ogg vorbis data
		Buffer = CreateQueuedBuffer( AudioDevice, InWave );
		break;

	case DTYPE_Streaming:
		// Always create a new buffer for streaming ogg vorbis data
		Buffer = CreateStreamBuffer( AudioDevice, InWave );
		break;
	
	case DTYPE_Procedural:
		// New buffer for procedural data
		Buffer = CreateProceduralBuffer(AudioDevice, InWave);
		break;

	case DTYPE_Invalid:
	case DTYPE_Preview:
	default:
		UE_LOG( LogAndroidAudio, Warning, TEXT("Init Buffer on unsupported sound type name = %s type = %d"), *InWave->GetName(), int32(DecompressionType));
		break;
	}
	
	return Buffer;
}


/**
 * Decompresses a chunk of compressed audio to the destination memory
 *
 * @param Destination		Memory to decompress to
 * @param bLooping			Whether to loop the sound seamlessly, or pad with zeroes
 * @return					Whether the sound looped or not
 */
bool FSLESSoundBuffer::ReadCompressedData( uint8* Destination, bool bLooping )
{
	ensure( DecompressionState);

	if(Format == SoundFormat_Streaming)
	{
		return(DecompressionState->StreamCompressedData(Destination, bLooping, DecompressionState->GetStreamBufferSize() * NumChannels));
	}

	return(DecompressionState->ReadCompressedData(Destination, bLooping, DecompressionState->GetStreamBufferSize() * NumChannels));
}

void FSLESSoundBuffer::Seek(const float SeekTime)
{
	if (ensure(DecompressionState))
	{
		DecompressionState->SeekToTime(SeekTime);
	}
}

int32 FSLESSoundBuffer::GetCurrentChunkIndex() const
{
	int32 result = -1;

	if (ensure(DecompressionState))
	{
		result = DecompressionState->GetCurrentChunkIndex();
	}

	return result;
}

int32 FSLESSoundBuffer::GetCurrentChunkOffset() const
{
	int32 result = -1;

	if (ensure(DecompressionState))
	{
		result = DecompressionState->GetCurrentChunkOffset();
	}

	return result;
}

/**
* Returns the size for a real time/streaming buffer based on decompressor
*
* @return Size of buffer in bytes for a single channel or 0 if no decompression state
*/
int FSLESSoundBuffer::GetRTBufferSize(void)
{
	return DecompressionState ? DecompressionState->GetStreamBufferSize() : MONO_PCM_BUFFER_SIZE;
}
