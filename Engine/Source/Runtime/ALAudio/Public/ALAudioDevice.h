// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ALAudioDevice.h: Unreal OpenAL audio interface object.
=============================================================================*/

#pragma  once

#include "CoreMinimal.h"
#include "Audio.h"
#include "AudioDevice.h"

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif
#define AL_NO_PROTOTYPES 1
#define ALC_NO_PROTOTYPES 1
#include "AL/al.h"
#include "AL/alc.h"
#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

class FALAudioDevice;
class USoundWave;

/*------------------------------------------------------------------------------------
	Dependencies, helpers & forward declarations.
------------------------------------------------------------------------------------*/

class FALAudioDevice;

DECLARE_LOG_CATEGORY_EXTERN(LogALAudio, Log, All);

//     2 UU == 1"
// <=> 1 UU == 0.0127 m
#define AUDIO_DISTANCE_FACTOR ( 0.0127f )

/**
 * OpenAL implementation of FSoundBuffer, containing the wave data and format information.
 */
class FALSoundBuffer : public FSoundBuffer
{
public:
	/**
	 * Constructor
	 *
	 * @param AudioDevice	audio device this sound buffer is going to be attached to.
	 */
	FALSoundBuffer( FALAudioDevice* AudioDevice );

	/**
	 * Destructor
	 *
	 * Frees wave data and detaches itself from audio device.
	 */
	~FALSoundBuffer( void );

	/**
	 * Static function used to create a buffer.
	 *
	 * @param	InWave				USoundWave to use as template and wave source
	 * @param	AudioDevice			Audio device to attach created buffer to
	 * @return	FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FALSoundBuffer* Init(  FALAudioDevice* ,USoundWave* );

	/**
	 * Static function used to create a IOSAudio buffer and upload decompressed ogg vorbis data to.
	 *
	 * @param Wave			USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FIOSAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static void CreateNativeBuffer(FALAudioDevice* AudioDevice, USoundWave* InWave, FALSoundBuffer* &Buffer);

	/**
	 * Returns the size of this buffer in bytes.
	 *
	 * @return Size in bytes
	 */
	int GetSize( void )
	{
		return( BufferSize );
	}

	/** Buffer id used to reference the data stored in AL. */
	ALuint					BufferId;
	/** Format of the data internal to OpenAL */
	ALuint					InternalFormat;

	/** Number of bytes stored in OpenAL, or the size of the ogg vorbis data */
	int						BufferSize;
	/** Sample rate of the ogg vorbis data - typically 44100 or 22050 */
	int						SampleRate;
};

/**
 * OpenAL implementation of FSoundSource, the interface used to play, stop and update sources
 */
class FALSoundSource : public FSoundSource
{
public:
	/**
	 * Constructor
	 *
	 * @param	InAudioDevice	audio device this source is attached to
	 */
	FALSoundSource( class FAudioDevice* InAudioDevice )
	:	FSoundSource( InAudioDevice ),
		SourceId( 0 ),
		Buffer( NULL )
	{}

	/**
	 * Destructor
	 */
	~FALSoundSource( void );

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
	 * Access function for the source id
	 */
	ALuint GetSourceId( void ) const { return( SourceId ); }

	/**
	 * Returns TRUE if an OpenAL source has finished playing
	 */
	bool IsSourceFinished( void );

	/**
	 * Handle dequeuing and requeuing of a single buffer
	 */
	void HandleQueuedBuffer( void );

protected:
	/** OpenAL source voice associated with this source/ channel. */
	ALuint				SourceId;
	/** Cached sound buffer associated with currently bound wave instance. */
	FALSoundBuffer*		Buffer;

	friend class FALAudioDevice;

	/** Convenience function to avoid casting in multiple places */
	FORCEINLINE FALAudioDevice * GetALDevice();
};

/**
 * OpenAL implementation of an Unreal audio device.
 */
class FALAudioDevice : public FAudioDevice
{
public:
	FALAudioDevice();

	virtual ~FALAudioDevice() {}

	virtual FName GetRuntimeFormat(USoundWave* SoundWave) override
	{
		static FName NAME_OGG(TEXT("OGG"));
		return NAME_OGG;
	}

	/** Starts up any platform specific hardware/APIs */
	virtual bool InitializeHardware() override;

	/**
	 * Tears down audio device by stopping all sounds, removing all buffers,  destroying all sources, ... Called by both Destroy and ShutdownAfterError
	 * to perform the actual tear down.
	 */
	virtual void TeardownHardware() override;

	/**
	 * Update the audio device and calculates the cached inverse transform later
	 * on used for spatialization.
	 */
	virtual void UpdateHardware() override;

	virtual bool HasCompressedAudioInfoClass(USoundWave* SoundWave) override;

	virtual class ICompressedAudioInfo* CreateCompressedAudioInfo(USoundWave* SoundWave) override;

	// Error checking.
	bool alError( const TCHAR* Text, bool Log = true );

	/**
	 * Makes sure correct context is set.
	 *
	 * @param CallSiteIdentifier identifies the call site for debugging/error reporting purposes
	 */
	void MakeCurrent(const TCHAR * CallSiteIdentifier = nullptr);

protected:

	virtual FSoundSource* CreateSoundSource() override;

	/** Returns the enum for the internal format for playing a sound with this number of channels. */
	ALuint GetInternalFormat( int NumChannels );


	// Variables.

	// AL specific

	/** Device/context used to play back sounds (static so it can be initialized early) */
	ALCdevice*									HardwareDevice;
	ALCcontext*									SoundContext;
	void*										DLLHandle;

	/** Formats for multichannel sounds */
	ALenum										Surround40Format;
	ALenum										Surround51Format;
	ALenum										Surround61Format;
	ALenum										Surround71Format;

	/** Inverse listener transformation, used for spatialization */
	FMatrix										InverseTransform;

	friend class FALSoundBuffer;
	friend class FALSoundSource;
};

FORCEINLINE FALAudioDevice * FALSoundSource::GetALDevice()
{
	return static_cast<FALAudioDevice *>(AudioDevice);
}
