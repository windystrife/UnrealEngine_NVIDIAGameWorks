// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	XAudio2Device.h: Unreal XAudio2 audio interface object.
=============================================================================*/

#pragma once

#ifndef WITH_XMA2
#define WITH_XMA2 0
#endif

/*------------------------------------------------------------------------------------
	XAudio2 system headers
------------------------------------------------------------------------------------*/
#include "CoreMinimal.h"
#include "AudioDecompress.h"
#include "AudioEffect.h"
#include "AudioDevice.h"

DECLARE_LOG_CATEGORY_EXTERN(LogXAudio2, Log, All);


/*------------------------------------------------------------------------------------
	Dependencies, helpers & forward declarations.
------------------------------------------------------------------------------------*/

enum ProcessingStages
{
	STAGE_SOURCE = 1,
	STAGE_RADIO,
	STAGE_REVERB,
	STAGE_EQPREMASTER,
	STAGE_OUTPUT
};

enum SourceDestinations
{
	DEST_DRY,
	DEST_REVERB,
	DEST_RADIO,
	DEST_COUNT
};

enum ChannelOutputs
{
	CHANNELOUT_FRONTLEFT = 0,
	CHANNELOUT_FRONTRIGHT,
	CHANNELOUT_FRONTCENTER,
	CHANNELOUT_LOWFREQUENCY,
	CHANNELOUT_LEFTSURROUND,
	CHANNELOUT_RIGHTSURROUND,

	CHANNELOUT_REVERB,
	CHANNELOUT_RADIO,
	CHANNELOUT_COUNT
};

// We are supporting spatializing non-mono assets, so our channel output will be a "matrix" 
// of channel-out mappings per input channel only supporting stereo 3d spatialization, but 
// if we support 3d spatialization of other N channel source files, we'd bump this up
#define MAX_INPUT_CHANNELS_SPATIALIZED (2)
#define CHANNEL_MATRIX_COUNT (MAX_INPUT_CHANNELS_SPATIALIZED*CHANNELOUT_COUNT)

#define SPEAKER_5POINT0          ( SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT )
#define SPEAKER_6POINT1          ( SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT | SPEAKER_BACK_CENTER )

class FXAudio2Device;

enum ESoundFormat
{
	SoundFormat_Invalid,
	SoundFormat_PCM,
	SoundFormat_PCMPreview,
	SoundFormat_PCMRT,
	SoundFormat_XMA2,
	SoundFormat_XWMA,
	SoundFormat_Streaming
};

class FXAudio2SoundBuffer;
class FXAudio2SoundSource;
class FSpatializationHelper;

/**
 * XAudio2 implementation of an Unreal audio device. Neither use XACT nor X3DAudio.
 */
class FXAudio2Device : public FAudioDevice
{
public:
	FXAudio2Device() : FAudioDevice()
	{
		CachedAudioClockStartTime = 0.0;
#if WITH_XMA2 
		bDisableAudioCaching = true;	// Do not allow DTYPE_Native buffers, only DTYPE_RealTime or DTYPE_Streaming since on the fly decompression is so cheap, it saves memory, and requires fewer code paths
#endif
	}

private:
	virtual void GetAudioDeviceList(TArray<FString>& OutAudioDeviceNames) const override;

	/** Starts up any platform specific hardware/APIs */
	virtual bool InitializeHardware() override;

	/** Shuts down any platform specific hardware/APIs */
	virtual void TeardownHardware() override;

	/** Lets the platform any tick actions */
	virtual void UpdateHardware() override;

	/** Get a sample-accurate time clock. */
	virtual void UpdateAudioClock() override;

	/** Creates a new platform specific sound source */
	virtual FAudioEffectsManager* CreateEffectsManager() override;

	/** Creates a new platform specific sound source */
	virtual FSoundSource* CreateSoundSource() override;

	virtual FName GetRuntimeFormat(USoundWave* SoundWave) override
	{
		static FName NAME_OPUS(TEXT("OPUS"));

		if (SoundWave->IsStreaming())
		{
			return NAME_OPUS;
		}
#if WITH_OGGVORBIS
		static FName NAME_OGG(TEXT("OGG"));
		static FName NAME_XMA(TEXT("XMA"));

#if WITH_XMA2
		if (SoundWave->NumChannels > 2)
		{
			// Use OGG for surround wave sources, until we can sort out the channel assignments properly
			return NAME_OGG;
		}
		else
		{
			return NAME_XMA;
		}
#else
		return NAME_OGG;
#endif

#else //WITH_OGGVORBIS
		static FName NAME_XMA(TEXT("XMA"));
		return NAME_XMA;
#endif //WITH_OGGVORBIS
	}

	virtual bool HasCompressedAudioInfoClass(USoundWave* SoundWave) override;

	virtual bool SupportsRealtimeDecompression() const override
	{
		return true;
	}

	virtual class ICompressedAudioInfo* CreateCompressedAudioInfo(USoundWave* SoundWave) override;

	/** 
	 * Check for errors and output a human readable string 
	 */
	virtual bool ValidateAPICall(const TCHAR* Function, uint32 ErrorCode) override;

	/**
	 * Exec handler used to parse console commands.
	 *
	 * @param	Cmd		Command to parse
	 * @param	Ar		Output device to use in case the handler prints anything
	 * @return	true if command was handled, false otherwise
	 */
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;

protected:

	/** Creates a mastering voice. Returns false if fails. */
	struct IXAudio2MasteringVoice* CreateMasteringVoice();

	/**
     * Allocates memory from permanent pool. This memory will NEVER be freed.
	 *
	 * @param	Size	Size of allocation.
	 * @param	AllocatedInPool	(OUT) True if the allocation occurred from the pool; false if it was a regular physical allocation.
	 *
	 * @return pointer to a chunk of memory with size Size
	 */
	void* AllocatePermanentMemory( int32 Size, /*OUT*/ bool& AllocatedInPool );


	/** 
     * Derives the output matrix to use based on the channel mask and the number of channels
	 */
	static bool GetOutputMatrix( uint32 ChannelMask, uint32 NumChannels );

	/** Test decompress a vorbis file */
	void TestDecompressOggVorbis( USoundWave* Wave );
	/** Decompress a wav a number of times for profiling purposes */
	void TimeTest( FOutputDevice& Ar, const TCHAR* WaveAssetName );

	friend class FXAudio2SoundBuffer;
	friend class FXAudio2SoundSource;
	friend class FXAudio2EffectsManager;

private:
	struct FXAudioDeviceProperties* DeviceProperties;

	/** Whether or not audio hardware changed. */
	bool bHardwareChanged;

	/** Cached audio clock time for when devices are removed/swapped. */
	double CachedAudioClockStartTime;

#if PLATFORM_WINDOWS
	// We need to keep track whether com was successfully initialized so we can clean 
	// it up during shutdown
	bool bComInitialized;
#endif // PLATFORM_WINDOWS
};

class FXMPHelper
{
private:
	/** Count of current cinematic audio clips playing (used to turn on/off XMP background music, allowing for overlap) */
	int32							CinematicAudioCount;
	/** Count of current movies playing (used to turn on/off XMP background music, NOT allowing for overlap) */
	bool						MoviePlaying;
	/** Flag indicating whether or not XMP playback is enabled (defaults to true) */
	bool						XMPEnabled;
	/** Flag indicating whether or not XMP playback is blocked (defaults to false)
	 *	Updated when player enters single-play:
	 *		XMP is blocked if the player hasn't finished the game before
	 */
	bool						XMPBlocked;

public:
	/**
	 * Constructor, initializing all member variables.
	 */
	FXMPHelper( void );
	/**
	 * Destructor, performing final cleanup.
	 */
	~FXMPHelper( void );

	/**
	 * Accessor for getting the XMPHelper class
	 */
	static FXMPHelper* GetXMPHelper( void );

	/**
	 * Records that a cinematic audio track has started playing.
	 */
	void CinematicAudioStarted( void );
	/**
	 * Records that a cinematic audio track has stopped playing.
	 */
	void CinematicAudioStopped( void );
	/**
	 * Records that a movie has started playing.
	 */
	void MovieStarted( void );
	/**
	 * Records that a movie has stopped playing.
	 */
	void MovieStopped( void );
	/**
	 * Called with every movie/cinematic change to update XMP status if necessary
	 */
	void CountsUpdated( void );
	/**
	 * Called to block XMP playback (when the gamer hasn't yet finished the game and enters single-play)
	 */
	void BlockXMP( void );
	/**
	 * Called to unblock XMP playback (when the gamer has finished the game or exits single-play)
	 */
	void UnblockXMP( void );
};
