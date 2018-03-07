// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma  once 

/*------------------------------------------------------------------------------------
	Dependencies, helpers & forward declarations.
------------------------------------------------------------------------------------*/

class FSLESAudioDevice;

#include "CoreMinimal.h"
#include "AudioDecompress.h"
#include "AudioEffect.h"
#include "AudioDevice.h"

#include <SLES/OpenSLES.h>
#include "SLES/OpenSLES_Android.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAndroidAudio,Log,VeryVerbose);

enum ESoundFormat
{
	SoundFormat_Invalid,
	SoundFormat_PCM,
	SoundFormat_PCMRT,
	SoundFormat_Streaming
};

struct SLESAudioBuffer
{
	uint8 *AudioData;
	int32 AudioDataSize;
	int32 ReadCursor;
};

/**
 * OpenSLES implementation of FSoundBuffer, containing the wave data and format information.
 */
class FSLESSoundBuffer : public FSoundBuffer 
{
public:
	/** 
	 * Constructor
	 *
	 * @param AudioDevice	audio device this sound buffer is going to be attached to.
	 */
	FSLESSoundBuffer( FSLESAudioDevice* AudioDevice );
	
	/**
	 * Destructor 
	 * 
	 * Frees wave data and detaches itself from audio device.
 	 */
	~FSLESSoundBuffer( void );

	/**
	 * Static function used to create a buffer and dynamically upload decompressed ogg vorbis data to.
	 *
	 * @param InWave		USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FSLESSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FSLESSoundBuffer* CreateQueuedBuffer( FSLESAudioDevice* AudioDevice, USoundWave* Wave );

	/**
	 * Static function used to create a buffer and stream data directly off disk as needed
	 *
	 * @param InWave		USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FSLESSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FSLESSoundBuffer* CreateStreamBuffer( FSLESAudioDevice* AudioDevice, USoundWave* Wave );

	/**
	 * Static function used to create an Audio buffer and upload decompressed ogg vorbis data to.
	 *
	 * @param Wave			USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FSLESSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FSLESSoundBuffer* CreateNativeBuffer( FSLESAudioDevice* AudioDevice, USoundWave* Wave );

	/**
	* Static function used to create an Audio buffer and dynamically upload procedural data to.
	*
	* @param InWave		USoundWave to use as template and wave source
	* @param AudioDevice	audio device to attach created buffer to
	* @return FSLESSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	*/
	static FSLESSoundBuffer* CreateProceduralBuffer(FSLESAudioDevice* AudioDevice, USoundWave* Wave);

	/**
	 * Static function used to create a buffer.
	 *
	 * @param	InWave				USoundWave to use as template and wave source
	 * @param	AudioDevice			Audio device to attach created buffer to
	 * @return	FSLESSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FSLESSoundBuffer* Init(  FSLESAudioDevice* ,USoundWave* );

	/**
	 * Decompresses a chunk of compressed audio to the destination memory
	 *
	 * @param Destination		Memory to decompress to
	 * @param bLooping			Whether to loop the sound seamlessly, or pad with zeroes
	 * @return					Whether the sound looped or not
	 */
	bool ReadCompressedData( uint8* Destination, bool bLooping );

	/**
	 * Sets the point in time within the buffer to the specified time
	 * If the time specified is beyond the end of the sound, it will be set to the end
	 *
	 * @param SeekTime		Time in seconds from the beginning of sound to seek to
	 */
	void Seek(const float SeekTime);

	/**
	 * Returns the size of this buffer in bytes.
	 *
	 * @return Size in bytes
	 */
	int GetSize( void ) 
	{ 
		return( BufferSize ); 
	}
	
	// These are used by the streaming engine to manage loading/unloading of chunks
	virtual int32 GetCurrentChunkIndex() const override;
	virtual int32 GetCurrentChunkOffset() const override;

	/**
	 * Returns the size for a real time/streaming buffer based on decompressor
	 */
	int GetRTBufferSize(void);
		
	/** Audio device this buffer is attached to */
	FSLESAudioDevice*			AudioDevice;
	/** Data */
	uint8*					AudioData;

	/** Number of bytes stored in OpenAL, or the size of the ogg vorbis data */
	int						BufferSize;
	/** Sample rate of the ogg vorbis data - typically 44100 or 22050 */
	int						SampleRate;

	/** Wrapper to handle the decompression of audio codecs */
	class ICompressedAudioInfo*		DecompressionState;

	/** Format of data to be received by the source */
	ESoundFormat Format;
	
};

typedef FAsyncTask<FAsyncRealtimeAudioTaskWorker<FSLESSoundBuffer>> FAsyncRealtimeAudioTask;

/**
 * OpenSLES implementation of FSoundSource, the interface used to play, stop and update sources
 */
class FSLESSoundSource : public FSoundSource
{
public:
	/**
	 * Constructor
	 *
	 * @param	InAudioDevice	audio device this source is attached to
	 */
	FSLESSoundSource( class FAudioDevice* InAudioDevice );

	/** 
	 * Destructor
	 */
	~FSLESSoundSource( void );

	/**
	 * Initializes a source with a given wave instance and prepares it for playback.
	 *
	 * @param	WaveInstance	wave instance being primed for playback
	 * @return	TRUE			if initialization was successful, FALSE otherwise
	 */
	virtual bool Init( FWaveInstance* WaveInstance );

	/**
	 * Updates the source specific parameter like e.g. volume and pitch based on the associated
	 * wave instance.	
	 */
	virtual void Update( void );

	/**
	 * Plays the current wave instance.	
	 */
	virtual void Play( void );

	/**
	 * Stops the current wave instance and detaches it from the source.	
	 */
	virtual void Stop( void );

	/**
	 * Pauses playback of current wave instance.
	 */
	virtual void Pause( void );

	/**
	 * Queries the status of the currently associated wave instance.
	 *
	 * @return	TRUE if the wave instance/ source has finished playback and FALSE if it is 
	 *			currently playing or paused.
	 */
	virtual bool IsFinished( void );

	/** 
	 * Returns TRUE if source has finished playing
	 */
	bool IsSourceFinished( void );

	/**
	 * Used to requeue a looping sound when the completion callback fires.
	 *
	 */
	void OnRequeueBufferCallback( SLAndroidSimpleBufferQueueItf InQueueInterface );


protected:

	enum class EDataReadMode : uint8
	{
		Synchronous,
		Asynchronous,
		AsynchronousSkipFirstFrame
	};

	friend class FSLESAudioDevice;
	// Do not shadow the Buffer member variable defined in the parent class
	FSLESSoundBuffer*		SLESBuffer;
	FSLESAudioDevice*		Device;
	
	// OpenSL interface objects
	SLObjectItf						SL_PlayerObject;
	SLPlayItf						SL_PlayerPlayInterface;
	SLAndroidSimpleBufferQueueItf	SL_PlayerBufferQueue;
	SLVolumeItf						SL_VolumeInterface;

	FAsyncRealtimeAudioTask* RealtimeAsyncTask;

	/** Which sound buffer should be written to next - used for double buffering. */
	bool						bStreamedSound;
	/** A pair of sound buffers for real-time decoding */
	SLESAudioBuffer				AudioBuffers[2];
	/** Set when we wish to let the buffers play themselves out */
	bool						bBuffersToFlush;

	uint32						BufferSize;

	int32						BufferInUse;
	float						VolumePreviousUpdate;
	bool						bHasLooped;
	/** Lets us know if calls to SL_PlayerPlayInterface->GetPosition have progressed past 0 at some point so we can determine when a non-looping sound has finished */
	bool						bHasPositionUpdated;

	bool CreatePlayer();
	void DestroyPlayer();
	void ReleaseResources();
	bool EnqueuePCMBuffer( bool bLoop);
	bool EnqueuePCMRTBuffer( bool bLoop);

	/** Decompress through FSLESSoundBuffer, or call USoundWave procedure to generate more PCM data. Returns true/false: did audio loop? */
	bool ReadMorePCMData( const int32 BufferIndex, EDataReadMode DataReadMode );
};

/**
 * OpenSLES implementation of an Unreal audio device.
 */
class FSLESAudioDevice : public FAudioDevice
{
public: 
	FSLESAudioDevice() {} 
	virtual ~FSLESAudioDevice() {} 

	virtual FName GetRuntimeFormat(USoundWave* SoundWave) override
	{
#if WITH_OGGVORBIS
		static FName NAME_OGG(TEXT("OGG"));		//@todo android: probably not ogg
		if (SoundWave->HasCompressedData(NAME_OGG))
		{
			return NAME_OGG;
		}
#endif
		static FName NAME_ADPCM(TEXT("ADPCM"));
		if (SoundWave->HasCompressedData(NAME_ADPCM))
		{
			return NAME_ADPCM;
		}

#if WITH_OGGVORBIS
		return NAME_OGG;
#else
		return NAME_ADPCM;
#endif
	}

	virtual bool HasCompressedAudioInfoClass(USoundWave* SoundWave) override;

	virtual bool SupportsRealtimeDecompression() const override
	{
		return true;
	}

	virtual class ICompressedAudioInfo* CreateCompressedAudioInfo(USoundWave* SoundWave) override;

	/** Starts up any platform specific hardware/APIs */
	virtual bool InitializeHardware() override;
 
	/** Check if any background music or sound is playing through the audio device */
	virtual bool IsExernalBackgroundSoundActive() override;

protected:

	virtual FSoundSource* CreateSoundSource() override; 


	/**
	 * Tears down audio device by stopping all sounds, removing all buffers,  destroying all sources, ... Called by both Destroy and ShutdownAfterError
	 * to perform the actual tear down.
	 */
	void Teardown( void );

	// Variables.

	/** The name of the OpenSL Device to open - defaults to "Generic Software" */
	FString										DeviceName;

	// SL specific

	
	// engine interfaces
	SLObjectItf									SL_EngineObject;
	SLEngineItf									SL_EngineEngine;
	SLObjectItf									SL_OutputMixObject;
	
	SLint32										SL_VolumeMax;
	SLint32										SL_VolumeMin;
	
	friend class FSLESSoundBuffer;
	friend class FSLESSoundSource;
};
