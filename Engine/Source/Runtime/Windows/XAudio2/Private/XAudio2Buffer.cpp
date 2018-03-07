// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	XeAudioDevice.cpp: Unreal XAudio2 Audio interface object.

	Unreal is RHS with Y and Z swapped (or technically LHS with flipped axis)

=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "XAudio2Device.h"
#include "AudioDecompress.h"
#include "AudioEffect.h"
#include "XAudio2Effects.h"
#include "XAudio2Support.h"
#include "Interfaces/IAudioFormat.h"

#if XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
/**
 * Helper structure to access information in raw XMA data.
 */
struct FXMAInfo
{
	/**
	 * Constructor, parsing passed in raw data.
	 *
	 * @param RawData		raw XMA data
	 * @param RawDataSize	size of raw data in bytes
	 */
	FXMAInfo( uint8* RawData, uint32 RawDataSize )
	{
		// Check out XeTools.cpp/dll.
		uint32 Offset = 0;
		FMemory::Memcpy( &EncodedBufferFormatSize, RawData + Offset, sizeof( uint32 ) );
		Offset += sizeof( uint32 );
		FMemory::Memcpy( &SeekTableSize, RawData + Offset, sizeof( uint32 ) );
		Offset += sizeof( uint32 );
		FMemory::Memcpy( &EncodedBufferSize, RawData + Offset, sizeof( uint32 ) );
		Offset += sizeof( uint32 );

		//@warning EncodedBufferFormat is NOT endian swapped.

		EncodedBufferFormat = ( XMA2WAVEFORMATEX* )( RawData + Offset );
		Offset += EncodedBufferFormatSize;
		SeekTable = ( uint32* )( RawData + Offset );
		Offset += SeekTableSize;
		EncodedBuffer = RawData + Offset;
		Offset += EncodedBufferSize;

		check( Offset == RawDataSize );
	}

	/** Encoded buffer data (allocated via malloc from within XMA encoder) */
	void*				EncodedBuffer;
	/** Size in bytes of encoded buffer */
	uint32				EncodedBufferSize;
	/** Encoded buffer format (allocated via malloc from within XMA encoder) */
	void*				EncodedBufferFormat;
	/** Size in bytes of encoded buffer format */
	uint32				EncodedBufferFormatSize;
	/** Seek table (allocated via malloc from within XMA encoder) */
	uint32*				SeekTable;
	/** Size in bytes of seek table */
	uint32				SeekTableSize;
};
#endif	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX

/*------------------------------------------------------------------------------------
	FXAudio2SoundBuffer.
------------------------------------------------------------------------------------*/

FXAudio2SoundBuffer::FXAudio2SoundBuffer( FAudioDevice* InAudioDevice, ESoundFormat InSoundFormat )
	: FSoundBuffer(InAudioDevice)
	, SoundFormat(InSoundFormat)
	, DecompressionState(nullptr)
	, RealtimeAsyncHeaderParseTask(nullptr)
	, bDynamicResource(false)
{
	PCM.PCMData = NULL;
	PCM.PCMDataSize = 0;

#if XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
	XMA2.XMA2Data = NULL;
	XMA2.XMA2DataSize = 0;
#endif	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX

	XWMA.XWMAData = NULL;
	XWMA.XWMADataSize = 0;
	XWMA.XWMASeekData = NULL;
	XWMA.XWMASeekDataSize = 0;
}

FXAudio2SoundBuffer::~FXAudio2SoundBuffer( void )
{
	if( bAllocationInPermanentPool )
	{
		UE_LOG(LogXAudio2, Fatal, TEXT( "Can't free resource '%s' as it was allocated in permanent pool." ), *ResourceName );
	}

	check(RealtimeAsyncHeaderParseTask == nullptr);

	if( DecompressionState )
	{
		delete DecompressionState;
	}

	switch( SoundFormat )
	{
	case SoundFormat_PCM:
		if( PCM.PCMData )
		{
			FMemory::Free( ( void* )PCM.PCMData );
		}
		break;

	case SoundFormat_PCMPreview:
		if( bDynamicResource && PCM.PCMData )
		{
			FMemory::Free( ( void* )PCM.PCMData );
		}
		break;

	case SoundFormat_PCMRT:
	case SoundFormat_Streaming:
		// Buffers are freed as part of the ~FSoundSource
		break;

	case SoundFormat_XMA2:
#if XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
		if( XMA2.XMA2Data )
		{
			// Wave data was kept in pBuffer so we need to free it.
			FMemory::Free( ( void* )XMA2.XMA2Data );
		}
#endif	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
		break;

	case SoundFormat_XWMA:
		if( XWMA.XWMAData )
		{
			// Wave data was kept in pBuffer so we need to free it.
			FMemory::Free( ( void* )XWMA.XWMAData );
		}

		if( XWMA.XWMASeekData )
		{
			// Wave data was kept in pBuffer so we need to free it.
			FMemory::Free( ( void* )XWMA.XWMASeekData );
		}
		break;
	}
}

int32 FXAudio2SoundBuffer::GetSize( void )
{
	int32 TotalSize = 0;

	switch( SoundFormat )
	{
	case SoundFormat_PCM:
		TotalSize = PCM.PCMDataSize;
		break;

	case SoundFormat_PCMPreview:
		TotalSize = PCM.PCMDataSize;
		break;

	case SoundFormat_PCMRT:
		TotalSize = (DecompressionState ? DecompressionState->GetSourceBufferSize() : 0) + ( MONO_PCM_BUFFER_SIZE * 2 * NumChannels );
		break;

	case SoundFormat_XMA2:
#if XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
		TotalSize = XMA2.XMA2DataSize;
#else	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
		TotalSize = 0;
#endif	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
		break;

	case SoundFormat_XWMA:
		TotalSize = XWMA.XWMADataSize + XWMA.XWMASeekDataSize;
		break;

	case SoundFormat_Streaming:
		TotalSize = ( MONO_PCM_BUFFER_SIZE * 2 * NumChannels );
		break;
	}

	return( TotalSize );
}

int32 FXAudio2SoundBuffer::GetCurrentChunkIndex() const
{
	if (DecompressionState)
	{
		return DecompressionState->GetCurrentChunkIndex();
	}
	return INDEX_NONE;
}

int32 FXAudio2SoundBuffer::GetCurrentChunkOffset() const
{
	if (DecompressionState)
	{
		return DecompressionState->GetCurrentChunkOffset();
	}
	return INDEX_NONE;
}

bool FXAudio2SoundBuffer::IsRealTimeSourceReady()
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

void FXAudio2SoundBuffer::EnsureRealtimeTaskCompletion()
{
	if (RealtimeAsyncHeaderParseTask)
	{
		RealtimeAsyncHeaderParseTask->EnsureCompletion();
		delete RealtimeAsyncHeaderParseTask;
		RealtimeAsyncHeaderParseTask = nullptr;
	}
}


void FXAudio2SoundBuffer::InitWaveFormatEx( uint16 Format, USoundWave* Wave, bool bCheckPCMData )
{
	// Setup the format structure required for XAudio2
	PCM.PCMFormat.wFormatTag = Format;
	PCM.PCMFormat.nChannels = Wave->NumChannels;
	PCM.PCMFormat.nSamplesPerSec = Wave->SampleRate;
	PCM.PCMFormat.wBitsPerSample = 16;
	PCM.PCMFormat.cbSize = 0;

	// Set the number of channels - 0 channels means there has been an error
	NumChannels = Wave->NumChannels;

	if( bCheckPCMData )
	{
		if( PCM.PCMData == NULL || PCM.PCMDataSize == 0 )
		{
			NumChannels = 0;
			UE_LOG(LogXAudio2, Warning, TEXT( "Failed to create audio buffer for '%s'" ), *Wave->GetFullName() );
		}
	}

	PCM.PCMFormat.nBlockAlign = NumChannels * sizeof( int16 );
	PCM.PCMFormat.nAvgBytesPerSec = NumChannels * sizeof( int16 ) * Wave->SampleRate;
}

void FXAudio2SoundBuffer::InitXMA2( FXAudio2Device* XAudio2Device, USoundWave* Wave, FXMAInfo* XMAInfo )
{
#if XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
	SoundFormat = SoundFormat_XMA2;

	FMemory::Memcpy( &XMA2, XMAInfo->EncodedBufferFormat, XMAInfo->EncodedBufferFormatSize );

	NumChannels = XMA2.XMA2Format.wfx.nChannels;

	// Allocate the audio data in physical memory
	XMA2.XMA2DataSize = XMAInfo->EncodedBufferSize;

	if( Wave->IsRooted() )
	{
		// Allocate from permanent pool and mark buffer as non destructible.
		bool AllocatedInPool = true;
		XMA2.XMA2Data = ( uint8* )XAudio2Device->AllocatePermanentMemory( XMA2.XMA2DataSize, /*OUT*/ AllocatedInPool );
		bAllocationInPermanentPool = AllocatedInPool;
	}
	else
	{
		// Allocate via normal allocator.
		XMA2.XMA2Data = ( uint8* )FMemory::Malloc( XMA2.XMA2DataSize );
	}

	FMemory::Memcpy( ( void* )XMA2.XMA2Data, XMAInfo->EncodedBuffer, XMAInfo->EncodedBufferSize );
#else	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
	checkf(0, TEXT("XMA2 not supported!"));
#endif	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
}

void FXAudio2SoundBuffer::InitXWMA( USoundWave* Wave, FXMAInfo* XMAInfo )
{
#if XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
	SoundFormat = SoundFormat_XWMA;

	FMemory::Memcpy( &XWMA.XWMAFormat, XMAInfo->EncodedBufferFormat, XMAInfo->EncodedBufferFormatSize );

	NumChannels = XWMA.XWMAFormat.Format.nChannels;

	// Allocate the audio data in physical memory
	XWMA.XWMADataSize = XMAInfo->EncodedBufferSize;

	// Allocate via normal allocator.
	XWMA.XWMAData = ( uint8* )FMemory::Malloc( XWMA.XWMADataSize );
	FMemory::Memcpy( ( void* )XWMA.XWMAData, XMAInfo->EncodedBuffer, XMAInfo->EncodedBufferSize );

	XWMA.XWMASeekDataSize = XMAInfo->SeekTableSize;

	XWMA.XWMASeekData = ( UINT32* )FMemory::Malloc( XWMA.XWMASeekDataSize );
	FMemory::Memcpy( ( void* )XWMA.XWMASeekData, XMAInfo->SeekTable, XMAInfo->SeekTableSize );
#else	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
	checkf(0, TEXT("XMA2WAVEFORMATEX not supported!"));
#endif	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
}

bool FXAudio2SoundBuffer::ReadCompressedInfo(USoundWave* SoundWave)
{
	if (!DecompressionState)
	{
		UE_LOG(LogXAudio2, Warning, TEXT("Attempting to read compressed info without a compression state instance for resource '%s'"), *ResourceName);
		return false;
	}
	return DecompressionState->ReadCompressedInfo(SoundWave->ResourceData, SoundWave->ResourceSize, nullptr);
}

bool FXAudio2SoundBuffer::ReadCompressedData( uint8* Destination, bool bLooping )
{
	if (!DecompressionState)
	{
		UE_LOG(LogXAudio2, Warning, TEXT( "Attempting to read compressed data without a compression state instance for resource '%s'" ), *ResourceName );
		return false;
	}
	
	const uint32 kPCMBufferSize = MONO_PCM_BUFFER_SIZE * NumChannels;
	if (SoundFormat == SoundFormat_Streaming)
	{
		return DecompressionState->StreamCompressedData( Destination, bLooping, kPCMBufferSize );
	}
	else
	{
		return( DecompressionState->ReadCompressedData( Destination, bLooping, kPCMBufferSize ) );
	}
}

void FXAudio2SoundBuffer::Seek( const float SeekTime )
{
	if (DecompressionState)
	{
		DecompressionState->SeekToTime(SeekTime);
	}
}

FXAudio2SoundBuffer* FXAudio2SoundBuffer::CreateQueuedBuffer( FXAudio2Device* XAudio2Device, USoundWave* Wave )
{
	check(XAudio2Device);
	check(Wave);

	check(Wave->bIsPrecacheDone);
	// Always create a new buffer for real time decompressed sounds
	FXAudio2SoundBuffer* Buffer = new FXAudio2SoundBuffer( XAudio2Device, SoundFormat_PCMRT );

	// If the buffer was precached as native, the resource data will have been lost and we need to re-initialize it
	if (Wave->ResourceData == nullptr)
	{
		Wave->InitAudioResource(XAudio2Device->GetRuntimeFormat(Wave));
	}

	Buffer->DecompressionState = XAudio2Device->CreateCompressedAudioInfo(Wave);

	if (Buffer->DecompressionState)
	{
		// Start the async task that parses the decompressed asset header info
		// Doing this step synchronously causes huge main-thread hitches for large ogg-vorbis files
		check(Buffer->RealtimeAsyncHeaderParseTask == nullptr);
		Buffer->RealtimeAsyncHeaderParseTask = new FAsyncRealtimeAudioTask(Buffer, Wave);
		Buffer->RealtimeAsyncHeaderParseTask->StartBackgroundTask();

		// Clear out any dangling pointers
		Buffer->PCM.PCMData = NULL;
		Buffer->PCM.PCMDataSize = 0;

		Buffer->InitWaveFormatEx( WAVE_FORMAT_PCM, Wave, false );
	}
	else
	{
		Wave->DecompressionType = DTYPE_Invalid;
		Wave->NumChannels = 0;

		Wave->RemoveAudioResource();
	}

	return Buffer;
}

FXAudio2SoundBuffer* FXAudio2SoundBuffer::CreateProceduralBuffer( FXAudio2Device* XAudio2Device, USoundWave* Wave )
{
	// Always create a new buffer for real time decompressed sounds
	FXAudio2SoundBuffer* Buffer = new FXAudio2SoundBuffer( XAudio2Device, SoundFormat_PCMRT );

	// Clear out any dangling pointers
	Buffer->DecompressionState = NULL;
	Buffer->PCM.PCMData = NULL;
	Buffer->PCM.PCMDataSize = 0;
	Buffer->InitWaveFormatEx( WAVE_FORMAT_PCM, Wave, false );

	// No tracking of this resource as it's temporary
	Buffer->ResourceID = 0;
	Wave->ResourceID = 0;

	return( Buffer );
}

FXAudio2SoundBuffer* FXAudio2SoundBuffer::CreatePreviewBuffer( FXAudio2Device* XAudio2Device, USoundWave* Wave, FXAudio2SoundBuffer* Buffer )
{
	FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
	check(AudioDeviceManager != nullptr);

	if (Buffer)
	{
		AudioDeviceManager->FreeBufferResource(Buffer);
	}

	// Create new buffer.
	Buffer = new FXAudio2SoundBuffer( XAudio2Device, SoundFormat_PCMPreview );

	// Take ownership the PCM data
	Buffer->PCM.PCMData = Wave->RawPCMData;
	Buffer->PCM.PCMDataSize = Wave->RawPCMDataSize;

	Wave->RawPCMData = NULL;

	// Copy over whether this data should be freed on delete
	Buffer->bDynamicResource = Wave->bDynamicResource;

	Buffer->InitWaveFormatEx( WAVE_FORMAT_PCM, Wave, true );

	AudioDeviceManager->TrackResource(Wave, Buffer);

	return( Buffer );
}

FXAudio2SoundBuffer* FXAudio2SoundBuffer::CreateNativeBuffer( FXAudio2Device* XAudio2Device, USoundWave* Wave )
{
	check(Wave->bIsPrecacheDone);

	// Create new buffer.
	FXAudio2SoundBuffer* Buffer = new FXAudio2SoundBuffer( XAudio2Device, SoundFormat_PCM );

	// Take ownership the PCM data
	Buffer->PCM.PCMData = Wave->RawPCMData;
	Buffer->PCM.PCMDataSize = Wave->RawPCMDataSize;

	Wave->RawPCMData = NULL;

	// Keep track of associated resource name.
	Buffer->InitWaveFormatEx( WAVE_FORMAT_PCM, Wave, true );

	FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
	check(AudioDeviceManager != nullptr);
	AudioDeviceManager->TrackResource(Wave, Buffer);

	Wave->RemoveAudioResource();

	return( Buffer );
}

FXAudio2SoundBuffer* FXAudio2SoundBuffer::CreateStreamingBuffer( FXAudio2Device* XAudio2Device, USoundWave* Wave )
{
	// Always create a new buffer for streaming sounds
	FXAudio2SoundBuffer* Buffer = new FXAudio2SoundBuffer(XAudio2Device, SoundFormat_Streaming);

	// Prime the first two buffers and prepare the decompression
	FSoundQualityInfo QualityInfo = { 0 };

	Buffer->DecompressionState = XAudio2Device->CreateCompressedAudioInfo(Wave);

	if (Buffer->DecompressionState->StreamCompressedInfo(Wave, &QualityInfo))
	{
		// Refresh the wave data
		Wave->SampleRate = QualityInfo.SampleRate;
		Wave->NumChannels = QualityInfo.NumChannels;
		Wave->RawPCMDataSize = QualityInfo.SampleDataSize;
		Wave->Duration = QualityInfo.Duration;

		Buffer->InitWaveFormatEx(WAVE_FORMAT_PCM, Wave, false);

		// Clear out any dangling pointers
		Buffer->PCM.PCMData = NULL;
		Buffer->PCM.PCMDataSize = 0;

		return Buffer;
	}
	else
	{
		delete Buffer;
		Buffer = nullptr;

		return nullptr;
	}
}

FXAudio2SoundBuffer* FXAudio2SoundBuffer::Init( FAudioDevice* AudioDevice, USoundWave* Wave, bool bForceRealTime )
{
	// Can't create a buffer without any source data
	if( Wave == NULL || Wave->NumChannels == 0 )
	{
		return( NULL );
	}

	FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();

	FXAudio2Device* XAudio2Device = ( FXAudio2Device* )AudioDevice;
	FXAudio2SoundBuffer* Buffer = NULL;

	// Allow the precache to happen if necessary
	EDecompressionType DecompressionType = Wave->DecompressionType;
	if (bForceRealTime && DecompressionType != DTYPE_Setup && DecompressionType != DTYPE_Streaming)
	{
		DecompressionType = DTYPE_RealTime;
	}

	switch( DecompressionType )
	{
	case DTYPE_Setup:
		// Has circumvented precache mechanism - precache now
		AudioDevice->Precache(Wave, true, false);

		// if it didn't change, we will recurse forever
		check(Wave->DecompressionType != DTYPE_Setup);

		// Recall this function with new decompression type
		return( Init( AudioDevice, Wave, bForceRealTime ) );

	case DTYPE_Preview:
		// Find the existing buffer if any
		if( Wave->ResourceID )
		{
			Buffer = (FXAudio2SoundBuffer*)AudioDeviceManager->GetSoundBufferForResourceID(Wave->ResourceID);
		}

		// Override with any new PCM data even if some already exists. 
		if( Wave->RawPCMData )
		{
			// Upload the preview PCM data to it
			Buffer = CreatePreviewBuffer( XAudio2Device, Wave, Buffer );
		}
		break;

	case DTYPE_Procedural:
		// Always create a new buffer for streaming procedural data
		Buffer = CreateProceduralBuffer( XAudio2Device, Wave );
		break;

	case DTYPE_RealTime:
		// Always create a new buffer for streaming ogg vorbis data
		Buffer = CreateQueuedBuffer( XAudio2Device, Wave );
		break;

	case DTYPE_Native:
		// Upload entire wav to XAudio2
		if( Wave->ResourceID )
		{
			Buffer = (FXAudio2SoundBuffer*)AudioDeviceManager->GetSoundBufferForResourceID(Wave->ResourceID);
		}

		if( Buffer == NULL )
		{
			Buffer = CreateNativeBuffer( XAudio2Device, Wave );
		}
		break;

	case DTYPE_Streaming:
		// Always create a new buffer for streaming sounds
		Buffer = CreateStreamingBuffer( XAudio2Device, Wave );
		break;

	case DTYPE_Invalid:
	default:
		// Invalid will be set if the wave cannot be played
		break;
	}

	return( Buffer );
}

