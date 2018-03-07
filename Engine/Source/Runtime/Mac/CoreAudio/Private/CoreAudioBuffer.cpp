// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 	CoreAudioBuffer.cpp: Unreal CoreAudio buffer interface object.
 =============================================================================*/

/*------------------------------------------------------------------------------------
 Audio includes.
 ------------------------------------------------------------------------------------*/

#include "CoreAudioDevice.h"
#include "AudioDecompress.h"
#include "AudioEffect.h"
#include "CoreAudioEffects.h"
#include "IAudioFormat.h"

/*------------------------------------------------------------------------------------
 FCoreAudioSoundBuffer.
 ------------------------------------------------------------------------------------*/

/** 
 * Constructor
 *
 * @param AudioDevice	audio device this sound buffer is going to be attached to.
 */
FCoreAudioSoundBuffer::FCoreAudioSoundBuffer( FAudioDevice* InAudioDevice, ESoundFormat InSoundFormat )
:   FSoundBuffer( InAudioDevice ),
    SoundFormat( InSoundFormat ),
	PCMData( NULL ),
	PCMDataSize( 0 ),
	DecompressionState( NULL ),
	bDynamicResource( false )
{
}

/**
 * Destructor 
 * 
 * Frees wave data and detaches itself from audio device.
 */
FCoreAudioSoundBuffer::~FCoreAudioSoundBuffer( void )
{
	if( bAllocationInPermanentPool )
	{
		UE_LOG(LogCoreAudio, Fatal, TEXT( "Can't free resource '%s' as it was allocated in permanent pool." ), *ResourceName );
	}

	if( DecompressionState )
	{
		delete DecompressionState;
	}

	if( PCMData )
	{
		FMemory::Free( PCMData );
	}
}

/**
 * Returns the size of this buffer in bytes.
 *
 * @return Size in bytes
 */
int32 FCoreAudioSoundBuffer::GetSize( void )
{
	int32 TotalSize = 0;
	
	switch( SoundFormat )
	{
		case SoundFormat_PCM:
			TotalSize = PCMDataSize;
			break;
			
		case SoundFormat_PCMPreview:
			TotalSize = PCMDataSize;
			break;
			
		case SoundFormat_PCMRT:
			TotalSize = (DecompressionState ? DecompressionState->GetSourceBufferSize() : 0) + ( MONO_PCM_BUFFER_SIZE * 2 * NumChannels );
			break;

		case SoundFormat_Streaming:
			TotalSize = ( MONO_PCM_BUFFER_SIZE * 2 * NumChannels );
			break;
	}
	
	return( TotalSize );
}

/** 
 * Setup an AudioStreamBasicDescription structure
 */
void FCoreAudioSoundBuffer::InitAudioStreamBasicDescription( UInt32 FormatID, USoundWave* Wave, bool bCheckPCMData )
{
	PCMFormat.mSampleRate = Wave->SampleRate;
	PCMFormat.mFormatID = FormatID;
	PCMFormat.mFormatID = kAudioFormatLinearPCM;
	PCMFormat.mFormatFlags = kLinearPCMFormatFlagIsPacked | kAudioFormatFlagsNativeEndian | kLinearPCMFormatFlagIsSignedInteger;
	PCMFormat.mFramesPerPacket = 1;
	PCMFormat.mChannelsPerFrame = Wave->NumChannels;
	PCMFormat.mBitsPerChannel = 16;
	PCMFormat.mBytesPerFrame = PCMFormat.mChannelsPerFrame * PCMFormat.mBitsPerChannel / 8;
	PCMFormat.mBytesPerPacket = PCMFormat.mBytesPerFrame * PCMFormat.mFramesPerPacket;
	
	// Set the number of channels - 0 channels means there has been an error
	NumChannels = Wave->NumChannels;

	if( bCheckPCMData )
	{
		if( PCMData == NULL || PCMDataSize == 0 )
		{
			NumChannels = 0;
			UE_LOG(LogCoreAudio, Warning, TEXT( "Failed to create audio buffer for '%s'" ), *Wave->GetFullName() );
		}
	}
}

/**
 * Decompresses a chunk of compressed audio to the destination memory
 *
 * @param Destination		Memory to decompress to
 * @param bLooping			Whether to loop the sound seamlessly, or pad with zeroes
 * @return					Whether the sound looped or not
 */
bool FCoreAudioSoundBuffer::ReadCompressedData( uint8* Destination, bool bLooping )
{
	const uint32 kPCMBufferSize = MONO_PCM_BUFFER_SIZE * NumChannels;
	if (SoundFormat == SoundFormat_Streaming)
	{
		return DecompressionState->StreamCompressedData(Destination, bLooping, kPCMBufferSize);
	}
	else
	{
		return DecompressionState->ReadCompressedData(Destination, bLooping, kPCMBufferSize);
	}
}

void FCoreAudioSoundBuffer::Seek( const float SeekTime )
{
	if (ensure(DecompressionState))
	{
		DecompressionState->SeekToTime(SeekTime);
	}
}

/**
 * Static function used to create a CoreAudio buffer and dynamically upload decompressed ogg vorbis data to.
 *
 * @param InWave		USoundWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FCoreAudioSoundBuffer* FCoreAudioSoundBuffer::CreateQueuedBuffer( FCoreAudioDevice* CoreAudioDevice, USoundWave* Wave )
{
	check(Wave->bIsPrecacheDone);

	// Always create a new buffer for real time decompressed sounds
	FCoreAudioSoundBuffer* Buffer = new FCoreAudioSoundBuffer( CoreAudioDevice, SoundFormat_PCMRT );
	
	// Prime the first two buffers and prepare the decompression
	FSoundQualityInfo QualityInfo = { 0 };

	Buffer->DecompressionState = CoreAudioDevice->CreateCompressedAudioInfo(Wave);

	// If the buffer was precached as native, the resource data will have been lost and we need to re-initialize it
	if (Wave->ResourceData == nullptr)
	{
		Wave->InitAudioResource(CoreAudioDevice->GetRuntimeFormat(Wave));
	}

	if (Buffer->DecompressionState->ReadCompressedInfo( Wave->ResourceData, Wave->ResourceSize, &QualityInfo ))
	{
		// Clear out any dangling pointers
		Buffer->PCMData = NULL;
		Buffer->PCMDataSize = 0;
		
		Buffer->InitAudioStreamBasicDescription( kAudioFormatLinearPCM, Wave, false );
	}
	else
	{
		Wave->DecompressionType = DTYPE_Invalid;
		Wave->NumChannels = 0;
		
		Wave->RemoveAudioResource();
	}
	
	return Buffer;
}

/**
 * Static function used to create an OpenAL buffer and dynamically upload procedural data to.
 *
 * @param InWave		USoundWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FCoreAudioSoundBuffer* FCoreAudioSoundBuffer::CreateProceduralBuffer( FCoreAudioDevice* CoreAudioDevice, USoundWave* Wave )
{
	// Always create a new buffer for real time decompressed sounds
	FCoreAudioSoundBuffer* Buffer = new FCoreAudioSoundBuffer( CoreAudioDevice, SoundFormat_PCMRT );
	
	// Clear out any dangling pointers
	Buffer->DecompressionState = NULL;
	Buffer->PCMData = NULL;
	Buffer->PCMDataSize = 0;
	Buffer->InitAudioStreamBasicDescription( kAudioFormatLinearPCM, Wave, false );
	
	// No tracking of this resource as it's temporary
	Buffer->ResourceID = 0;
	Wave->ResourceID = 0;
	
	return( Buffer );
}

/**
 * Static function used to create an OpenAL buffer and upload raw PCM data to.
 *
 * @param InWave		USoundWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FCoreAudioSoundBuffer* FCoreAudioSoundBuffer::CreatePreviewBuffer( FCoreAudioDevice* CoreAudioDevice, USoundWave* Wave, FCoreAudioSoundBuffer* Buffer )
{
    FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
    check(AudioDeviceManager != nullptr);
    
	if (Buffer)
	{
		AudioDeviceManager->FreeBufferResource( Buffer );
	}
	
	// Create new buffer.
	Buffer = new FCoreAudioSoundBuffer( CoreAudioDevice, SoundFormat_PCMPreview );
	
	// Take ownership the PCM data
	Buffer->PCMData = Wave->RawPCMData;
	Buffer->PCMDataSize = Wave->RawPCMDataSize;
	
	Wave->RawPCMData = NULL;
	
	// Copy over whether this data should be freed on delete
	Buffer->bDynamicResource = Wave->bDynamicResource;
	
	Buffer->InitAudioStreamBasicDescription( kAudioFormatLinearPCM, Wave, true );
	
	AudioDeviceManager->TrackResource( Wave, Buffer );
	
	return( Buffer );
}

/**
 * Static function used to create a CoreAudio buffer and upload decompressed ogg vorbis data to.
 *
 * @param Wave			USoundWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FCoreAudioSoundBuffer* FCoreAudioSoundBuffer::CreateNativeBuffer( FCoreAudioDevice* CoreAudioDevice, USoundWave* Wave )
{
	// Create new buffer.
	FCoreAudioSoundBuffer* Buffer = new FCoreAudioSoundBuffer( CoreAudioDevice, SoundFormat_PCM );

	// Take ownership the PCM data
	Buffer->PCMData = Wave->RawPCMData;
	Buffer->PCMDataSize = Wave->RawPCMDataSize;
	
	Wave->RawPCMData = NULL;

	// Keep track of associated resource name.
	Buffer->InitAudioStreamBasicDescription( kAudioFormatLinearPCM, Wave, true );
	
    FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
    check(AudioDeviceManager != nullptr);
    
	AudioDeviceManager->TrackResource( Wave, Buffer );
	
	Wave->RemoveAudioResource();
	
	return( Buffer );
}

FCoreAudioSoundBuffer* FCoreAudioSoundBuffer::CreateStreamingBuffer(FCoreAudioDevice* CoreAudio2Device, USoundWave* Wave)
{
	// Always create a new buffer for streaming sounds
	FCoreAudioSoundBuffer* Buffer = new FCoreAudioSoundBuffer(CoreAudio2Device, SoundFormat_Streaming);

	// Prime the first two buffers and prepare the decompression
	FSoundQualityInfo QualityInfo = { 0 };

	Buffer->DecompressionState = CoreAudio2Device->CreateCompressedAudioInfo(Wave);

	if (Buffer->DecompressionState->StreamCompressedInfo(Wave, &QualityInfo))
	{
		// Refresh the wave data
		Wave->SampleRate = QualityInfo.SampleRate;
		Wave->NumChannels = QualityInfo.NumChannels;
		Wave->RawPCMDataSize = QualityInfo.SampleDataSize;
		Wave->Duration = QualityInfo.Duration;

		// Clear out any dangling pointers
		Buffer->PCMData = NULL;
		Buffer->PCMDataSize = 0;

		Buffer->InitAudioStreamBasicDescription(kAudioFormatLinearPCM, Wave, false);
	}
	else
	{
		Wave->DecompressionType = DTYPE_Invalid;
		Wave->NumChannels = 0;

		Wave->RemoveAudioResource();
	}

	return Buffer;
}

/**
 * Static function used to create a buffer.
 *
 * @param InWave USoundWave to use as template and wave source
 * @param AudioDevice audio device to attach created buffer to
 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FCoreAudioSoundBuffer* FCoreAudioSoundBuffer::Init( FAudioDevice* AudioDevice, USoundWave* Wave, bool bForceRealtime )
{
	// Can't create a buffer without any source data
	if( Wave == NULL || Wave->NumChannels == 0 )
	{
		return NULL;
	}

    FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
    
	FCoreAudioDevice *CoreAudioDevice = ( FCoreAudioDevice *)AudioDevice;
	FCoreAudioSoundBuffer *Buffer = NULL;

	// Allow the precache to happen if necessary
	EDecompressionType DecompressionType = Wave->DecompressionType;
	if (bForceRealtime && DecompressionType != DTYPE_Setup && DecompressionType != DTYPE_Streaming)
	{
		DecompressionType = DTYPE_RealTime;
	}

	switch( DecompressionType )
	{
		case DTYPE_Setup:
			// Has circumvented precache mechanism - precache now
			AudioDevice->Precache(Wave, true, false);
			
			// Recall this function with new decompression type
			return( Init( AudioDevice, Wave, bForceRealtime ) );
			
		case DTYPE_Preview:
			// Find the existing buffer if any
			if( Wave->ResourceID )
			{
				Buffer = (FCoreAudioSoundBuffer*)AudioDeviceManager->WaveBufferMap.FindRef( Wave->ResourceID );
			}
			
			// Override with any new PCM data even if some already exists. 
			if( Wave->RawPCMData )
			{
				// Upload the preview PCM data to it
				Buffer = CreatePreviewBuffer( CoreAudioDevice, Wave, Buffer );
			}
			break;
			
		case DTYPE_Procedural:
			// Always create a new buffer for streaming procedural data
			Buffer = CreateProceduralBuffer( CoreAudioDevice, Wave );
			break;
			
		case DTYPE_RealTime:
			// Always create a new buffer for streaming ogg vorbis data
			Buffer = CreateQueuedBuffer( CoreAudioDevice, Wave );
			break;
			
		case DTYPE_Native:
			// Upload entire wav to CoreAudio
			if( Wave->ResourceID )
			{
				Buffer = (FCoreAudioSoundBuffer*)AudioDeviceManager->WaveBufferMap.FindRef( Wave->ResourceID );
			}

			if( Buffer == NULL )
			{
				Buffer = CreateNativeBuffer( CoreAudioDevice, Wave );
			}
			break;
			
		case DTYPE_Streaming:
			// Always create a new buffer for streaming sounds
			Buffer = CreateStreamingBuffer( CoreAudioDevice, Wave );
			break;

		case DTYPE_Invalid:
		default:
			// Invalid will be set if the wave cannot be played
			break;
	}

	return Buffer;
}

int32 FCoreAudioSoundBuffer::GetCurrentChunkIndex() const
{
	if (DecompressionState == NULL)
	{
		return -1;
	}

	return DecompressionState->GetCurrentChunkIndex();
}

int32 FCoreAudioSoundBuffer::GetCurrentChunkOffset() const
{
	if (DecompressionState == NULL)
	{
		return -1;
	}

	return DecompressionState->GetCurrentChunkOffset();
}
