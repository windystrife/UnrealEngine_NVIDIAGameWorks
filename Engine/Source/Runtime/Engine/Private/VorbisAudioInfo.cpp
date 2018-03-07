// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "VorbisAudioInfo.h"
#include "Misc/Paths.h"
#include "Interfaces/IAudioFormat.h"
#include "ContentStreaming.h"
#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#endif

#if WITH_OGGVORBIS

// hack to get ogg types right for HTML5.
#if PLATFORM_HTML5
#define _WIN32
#define __MINGW32__
#endif
#pragma pack(push, 8)
#include "ogg/ogg.h"
#include "vorbis/vorbisenc.h"
#include "vorbis/vorbisfile.h"
#pragma pack(pop)
#endif

#if PLATFORM_HTML5
#undef  _WIN32
#undef __MINGW32__
#endif

#if PLATFORM_LITTLE_ENDIAN
#define VORBIS_BYTE_ORDER 0
#else
#define VORBIS_BYTE_ORDER 1
#endif

/**
 * Channel order expected for a multi-channel ogg vorbis file.
 * Ordering taken from http://xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-800004.3.9
 */
namespace VorbisChannelInfo
{
	const int32 Order[8][8] = {
		{ 0 },
		{ 0, 1 },
		{ 0, 2, 1 },
		{ 0, 1, 2, 3 },
		{ 0, 2, 1, 3, 4 },
		{ 0, 2, 1, 4, 5, 3 },
		{ 0, 2, 1, 4, 5, 6, 3 },
		{ 0, 2, 1, 4, 5, 6, 7, 3 }
	};
};

/*------------------------------------------------------------------------------------
	FVorbisFileWrapper. Hides Vorbis structs from public headers.
------------------------------------------------------------------------------------*/
struct FVorbisFileWrapper
{
	FVorbisFileWrapper()
	{
#if WITH_OGGVORBIS
		FMemory::Memzero( &vf, sizeof( OggVorbis_File ) );
#endif
	}

	~FVorbisFileWrapper()
	{
#if WITH_OGGVORBIS
		ov_clear( &vf );
#endif
	}

#if WITH_OGGVORBIS
	/** Ogg vorbis decompression state */
	OggVorbis_File	vf;
#endif
};

#if WITH_OGGVORBIS
/*------------------------------------------------------------------------------------
	FVorbisAudioInfo.
------------------------------------------------------------------------------------*/
FVorbisAudioInfo::FVorbisAudioInfo( void )
	: VFWrapper(new FVorbisFileWrapper())
	, SrcBufferData(NULL)
	, SrcBufferDataSize(0)
	, BufferOffset(0)
	, bPerformingOperation(false)
	, StreamingSoundWave(NULL)
	, StreamingChunksSize(0)
{
	// Make sure we have properly allocated a VFWrapper
	check(VFWrapper != NULL);
}

FVorbisAudioInfo::~FVorbisAudioInfo( void )
{
	// Make sure we're not deleting ourselves while performing an operation
	ensure(!bPerformingOperation);

	FScopeLock ScopeLock(&VorbisCriticalSection);
	check(VFWrapper != nullptr);
	delete VFWrapper;
	VFWrapper = nullptr;
}

/** Emulate read from memory functionality */
size_t FVorbisAudioInfo::ReadMemory( void *Ptr, uint32 Size )
{
	check(Ptr);
	size_t BytesToRead = FMath::Min(Size, SrcBufferDataSize - BufferOffset);
	FMemory::Memcpy( Ptr, SrcBufferData + BufferOffset, BytesToRead );
	BufferOffset += BytesToRead;
	return( BytesToRead );
}

static size_t OggReadMemory( void *ptr, size_t size, size_t nmemb, void *datasource )
{
	check(ptr);
	check(datasource);
	FVorbisAudioInfo* OggInfo = (FVorbisAudioInfo*)datasource;
	return( OggInfo->ReadMemory( ptr, size * nmemb ) );
}

int FVorbisAudioInfo::SeekMemory( uint32 offset, int whence )
{
	switch( whence )
	{
	case SEEK_SET:
		BufferOffset = offset;
		break;

	case SEEK_CUR:
		BufferOffset += offset;
		break;

	case SEEK_END:
		BufferOffset = SrcBufferDataSize - offset;
		break;

	default:
		checkf(false, TEXT("Uknown seek type"));
		break;
	}

	return( BufferOffset );
}

static int OggSeekMemory( void *datasource, ogg_int64_t offset, int whence )
{
	FVorbisAudioInfo* OggInfo = ( FVorbisAudioInfo* )datasource;
	return( OggInfo->SeekMemory( offset, whence ) );
}

int FVorbisAudioInfo::CloseMemory( void )
{
	return( 0 );
}

static int OggCloseMemory( void *datasource )
{
	FVorbisAudioInfo* OggInfo = ( FVorbisAudioInfo* )datasource;
	return( OggInfo->CloseMemory() );
}

long FVorbisAudioInfo::TellMemory( void )
{
	return( BufferOffset );
}

static long OggTellMemory( void *datasource )
{
	FVorbisAudioInfo *OggInfo = ( FVorbisAudioInfo* )datasource;
	return( OggInfo->TellMemory() );
}

/** Emulate read from memory functionality */
size_t FVorbisAudioInfo::ReadStreaming( void *Ptr, uint32 Size )
{
	size_t	BytesCopied = 0;

	while(Size > 0)
	{
		uint32	CurChunkSize = 0;

		uint8 const* ChunkData = IStreamingManager::Get().GetAudioStreamingManager().GetLoadedChunk(StreamingSoundWave, BufferOffset / StreamingChunksSize, &CurChunkSize);

		check(CurChunkSize >= (BufferOffset % StreamingChunksSize));
		size_t	BytesToCopy = FMath::Min<uint32>(CurChunkSize - (BufferOffset % StreamingChunksSize), Size);
		check((BufferOffset % StreamingChunksSize) + BytesToCopy <= CurChunkSize);

		if(ChunkData == NULL || BytesToCopy == 0)
		{
			return BytesCopied;
		}

		FMemory::Memcpy( Ptr, ChunkData + (BufferOffset % StreamingChunksSize), BytesToCopy );
		BufferOffset += BytesToCopy;
		BytesCopied += BytesToCopy;
		Size -= BytesToCopy;
		Ptr = (void*)((uint8*)Ptr + BytesToCopy);
	}

	return BytesCopied;
}

static size_t OggReadStreaming( void *ptr, size_t size, size_t nmemb, void *datasource )
{
	check(ptr);
	check(datasource);
	FVorbisAudioInfo* OggInfo = (FVorbisAudioInfo*)datasource;
	return( OggInfo->ReadStreaming( ptr, size * nmemb ) );
}

int FVorbisAudioInfo::CloseStreaming( void )
{
	return( 0 );
}

static int OggCloseStreaming( void *datasource )
{
	FVorbisAudioInfo* OggInfo = ( FVorbisAudioInfo* )datasource;
	return( OggInfo->CloseStreaming() );
}

bool FVorbisAudioInfo::GetCompressedInfoCommon(void* Callbacks, FSoundQualityInfo* QualityInfo)
{
	// Set up the read from memory variables
	int Result = ov_open_callbacks(this, &VFWrapper->vf, NULL, 0, (*(ov_callbacks*)Callbacks));
	if (Result < 0)
	{
		UE_LOG(LogAudio, Error, TEXT("FVorbisAudioInfo::ReadCompressedInfo, ov_open_callbacks error code: %d"), Result);
		return false;
	}

	if( QualityInfo )
	{
		// The compression could have resampled the source to make loopable
		vorbis_info* vi = ov_info( &VFWrapper->vf, -1 );
		QualityInfo->SampleRate = vi->rate;
		QualityInfo->NumChannels = vi->channels;
		ogg_int64_t PCMTotal = ov_pcm_total( &VFWrapper->vf, -1 );
		if (PCMTotal >= 0)
		{
			QualityInfo->SampleDataSize = PCMTotal * QualityInfo->NumChannels * sizeof( int16 );
			QualityInfo->Duration = ( float )ov_time_total( &VFWrapper->vf, -1 );
		}
		else if (PCMTotal == OV_EINVAL)
		{
			// indicates an error or that the bitstream is non-seekable
			QualityInfo->SampleDataSize = 0;
			QualityInfo->Duration = 0.0f;
		}
	}

	return true;
}

/**
 * Reads the header information of an ogg vorbis file
 *
 * @param	Resource		Info about vorbis data
 */
bool FVorbisAudioInfo::ReadCompressedInfo( const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, FSoundQualityInfo* QualityInfo )
{
	bPerformingOperation = true;

	SCOPE_CYCLE_COUNTER( STAT_VorbisPrepareDecompressionTime );

	FScopeLock ScopeLock(&VorbisCriticalSection);

	if (!VFWrapper)
	{
		bPerformingOperation = false;
		return false;
	}

	ov_callbacks Callbacks;

	SrcBufferData = InSrcBufferData;
	SrcBufferDataSize = InSrcBufferDataSize;
	BufferOffset = 0;

	Callbacks.read_func = OggReadMemory;
	Callbacks.seek_func = OggSeekMemory;
	Callbacks.close_func = OggCloseMemory;
	Callbacks.tell_func = OggTellMemory;

	bool result = GetCompressedInfoCommon(&Callbacks, QualityInfo);

	bPerformingOperation = false;

	return( result );
}


/**
 * Decompress an entire ogg vorbis data file to a TArray
 */
void FVorbisAudioInfo::ExpandFile( uint8* DstBuffer, FSoundQualityInfo* QualityInfo )
{
	bPerformingOperation = true;

	uint32		TotalBytesRead, BytesToRead;

	check( VFWrapper != NULL );
	check( DstBuffer );
	check( QualityInfo );

	FScopeLock ScopeLock(&VorbisCriticalSection);

	// A zero buffer size means decompress the entire ogg vorbis stream to PCM.
	TotalBytesRead = 0;
	BytesToRead = QualityInfo->SampleDataSize;

	char* Destination = ( char* )DstBuffer;
	while( TotalBytesRead < BytesToRead )
	{
		long BytesRead = ov_read( &VFWrapper->vf, Destination, BytesToRead - TotalBytesRead, 0, 2, 1, NULL );

		if (BytesRead < 0)
		{
			// indicates an error - fill remainder of buffer with zero
			FMemory::Memzero(Destination, BytesToRead - TotalBytesRead);
			bPerformingOperation = false;
			return;
		}

		TotalBytesRead += BytesRead;
		Destination += BytesRead;
	}

	bPerformingOperation = false;
}



/**
 * Decompresses ogg vorbis data to raw PCM data.
 *
 * @param	PCMData		where to place the decompressed sound
 * @param	bLooping	whether to loop the wav by seeking to the start, or pad the buffer with zeroes
 * @param	BufferSize	number of bytes of PCM data to create. A value of 0 means decompress the entire sound.
 *
 * @return	bool		true if the end of the data was reached (for both single shot and looping sounds)
 */
bool FVorbisAudioInfo::ReadCompressedData( uint8* InDestination, bool bLooping, uint32 BufferSize )
{
	SCOPED_NAMED_EVENT(FVorbisAudioInfo_ReadCompressedData, FColor::Blue);
	bPerformingOperation = true;

	bool		bLooped;
	uint32		TotalBytesRead;

	SCOPE_CYCLE_COUNTER( STAT_VorbisDecompressTime );

	FScopeLock ScopeLock(&VorbisCriticalSection);

	bLooped = false;

	// Work out number of samples to read
	TotalBytesRead = 0;
	char* Destination = ( char* )InDestination;

	check( VFWrapper != NULL );

	while( TotalBytesRead < BufferSize )
	{
		long BytesRead = ov_read( &VFWrapper->vf, Destination, BufferSize - TotalBytesRead, 0, 2, 1, NULL );
		if( !BytesRead )
		{
			// We've reached the end
			bLooped = true;
			if( bLooping )
			{
				int Result = ov_pcm_seek_page( &VFWrapper->vf, 0 );
				if (Result < 0)
				{
					// indicates an error - fill remainder of buffer with zero
					FMemory::Memzero(Destination, BufferSize - TotalBytesRead);
					bPerformingOperation = false;
					return true;
				}
			}
			else
			{
				int32 Count = ( BufferSize - TotalBytesRead );
				FMemory::Memzero( Destination, Count );

				BytesRead += BufferSize - TotalBytesRead;
			}
		}
		else if (BytesRead < 0)
		{
			// indicates an error - fill remainder of buffer with zero
			FMemory::Memzero(Destination, BufferSize - TotalBytesRead);
			bPerformingOperation = false;
			return false;
		}

		TotalBytesRead += BytesRead;
		Destination += BytesRead;
	}

	bPerformingOperation = false;
	return( bLooped );
}

void FVorbisAudioInfo::SeekToTime( const float SeekTime )
{
	bPerformingOperation = true;

	FScopeLock ScopeLock(&VorbisCriticalSection);

	const float TargetTime = FMath::Min(SeekTime, (float)ov_time_total(&VFWrapper->vf, -1));
	ov_time_seek( &VFWrapper->vf, TargetTime );

	bPerformingOperation = false;
}

void FVorbisAudioInfo::EnableHalfRate( bool HalfRate )
{

	bPerformingOperation = true;

	FScopeLock ScopeLock(&VorbisCriticalSection);

	ov_halfrate(&VFWrapper->vf, int32(HalfRate));

	bPerformingOperation = false;
}

bool FVorbisAudioInfo::StreamCompressedInfo(USoundWave* Wave, struct FSoundQualityInfo* QualityInfo)
{
	bPerformingOperation = true;

	SCOPE_CYCLE_COUNTER( STAT_VorbisPrepareDecompressionTime );

	FScopeLock ScopeLock(&VorbisCriticalSection);

	if (!VFWrapper)
	{
		bPerformingOperation = false;
		return false;
	}

	ov_callbacks Callbacks;

	SrcBufferData = NULL;
	SrcBufferDataSize = 0;
	BufferOffset = 0;
	StreamingSoundWave = Wave;

	Callbacks.read_func = OggReadStreaming;
	Callbacks.close_func = OggCloseStreaming;
	Callbacks.seek_func = NULL;	// Force streaming
	Callbacks.tell_func = NULL;	// Force streaming

	// We need to start with a valid StreamingChunksSize so just use this
	StreamingChunksSize = MONO_PCM_BUFFER_SIZE * 2 * 2;

	bool result = GetCompressedInfoCommon(&Callbacks, QualityInfo);

	// Now we can set the real StreamingChunksSize
	StreamingChunksSize = MONO_PCM_BUFFER_SIZE * 2 * QualityInfo->NumChannels;

	bPerformingOperation = false;

	return( result );
}

bool FVorbisAudioInfo::StreamCompressedData(uint8* InDestination, bool bLooping, uint32 BufferSize)
{
	check( VFWrapper != NULL );
	bPerformingOperation = true;

	SCOPE_CYCLE_COUNTER( STAT_VorbisDecompressTime );

	FScopeLock ScopeLock(&VorbisCriticalSection);

	bool	bLooped = false;

	while( BufferSize > 0 )
	{
		long BytesActuallyRead = ov_read( &VFWrapper->vf, (char*)InDestination, (int)BufferSize, 0, 2, 1, NULL );

		if( BytesActuallyRead <= 0 )
		{
			// We've reached the end
			bLooped = true;

			BufferOffset = 0;

			// Since we can't tell a streaming file to go back to the start of the stream (there is no seek) we have to close and reopen it which is a bummer
			ov_clear(&VFWrapper->vf);
			FMemory::Memzero( &VFWrapper->vf, sizeof( OggVorbis_File ) );
			ov_callbacks Callbacks;
			Callbacks.read_func = OggReadStreaming;
			Callbacks.close_func = OggCloseStreaming;
			Callbacks.seek_func = NULL;	// Force streaming
			Callbacks.tell_func = NULL;	// Force streaming
			int Result = ov_open_callbacks(this, &VFWrapper->vf, NULL, 0, Callbacks);
			if (Result < 0)
			{
				UE_LOG(LogAudio, Error, TEXT("FVorbisAudioInfo::StreamCompressedData, ov_open_callbacks error code: %d"), Result);
				break;
			}

			if( !bLooping )
			{
				// Need to clear out the remainder of the buffer
				FMemory::Memzero(InDestination, BufferSize);
				break;
			}
			// else we start over to get the samples from the start of the compressed audio data
			continue;
		}

		InDestination += BytesActuallyRead;
		BufferSize -= BytesActuallyRead;
	}

	bPerformingOperation = false;

	return( bLooped );
}

void LoadVorbisLibraries()
{
	static bool bIsInitialized = false;
	if (!bIsInitialized)
	{
		bIsInitialized = true;
#if PLATFORM_WINDOWS  && WITH_OGGVORBIS
		//@todo if ogg is every ported to another platform, then use the platform abstraction to load these DLLs
		// Load the Ogg dlls
#  if _MSC_VER >= 1900
		FString VSVersion = TEXT("VS2015/");
#  else
#    error "Unsupported Visual Studio version."
#  endif
		FString PlatformString = TEXT("Win32");
		FString DLLNameStub = TEXT(".dll");
#if PLATFORM_64BITS
		PlatformString = TEXT("Win64");
		DLLNameStub = TEXT("_64.dll");
#endif

		FString RootOggPath = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/Ogg/") / PlatformString / VSVersion;
		FString RootVorbisPath = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/Vorbis/") / PlatformString / VSVersion;

		FString DLLToLoad = RootOggPath + TEXT("libogg") + DLLNameStub;
		verifyf(FPlatformProcess::GetDllHandle(*DLLToLoad), TEXT("Failed to load DLL %s"), *DLLToLoad);
		// Load the Vorbis dlls
		DLLToLoad = RootVorbisPath + TEXT("libvorbis") + DLLNameStub;
		verifyf(FPlatformProcess::GetDllHandle(*DLLToLoad), TEXT("Failed to load DLL %s"), *DLLToLoad);
		DLLToLoad = RootVorbisPath + TEXT("libvorbisfile") + DLLNameStub;
		verifyf(FPlatformProcess::GetDllHandle(*DLLToLoad), TEXT("Failed to load DLL %s"), *DLLToLoad);
#endif	//PLATFORM_WINDOWS
	}
}

#endif		// WITH_OGGVORBIS

