// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 	CoreAudioDevice.h: Unreal CoreAudio audio interface object.
 =============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "AudioDecompress.h"
#include "AudioEffect.h"
#include "AudioDevice.h"

/*------------------------------------------------------------------------------------
 CoreAudio system headers
 ------------------------------------------------------------------------------------*/
#include <CoreAudio/AudioHardware.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>

DECLARE_LOG_CATEGORY_EXTERN(LogCoreAudio, Log, All);

/*------------------------------------------------------------------------------------
	Dependencies, helpers & forward declarations.
------------------------------------------------------------------------------------*/

/**
 * Maximum number of multichannel audio channels - used only for MatrixMixer setup
 */

#define CORE_AUDIO_MAX_CHANNELS (MAX_AUDIOCHANNELS + 1)
#define CORE_AUDIO_MAX_MULTICHANNEL_AUDIOCHANNELS 16

class FCoreAudioDevice;
class FCoreAudioEffectsManager;

enum ESoundFormat
{
	SoundFormat_Invalid,
	SoundFormat_PCM,
	SoundFormat_PCMPreview,
	SoundFormat_PCMRT,
	SoundFormat_Streaming
};

struct FCoreAudioBuffer
{
	uint8 *AudioData;
	int32 AudioDataSize;
	int32 ReadCursor;
};

/**
 * CoreAudio implementation of FSoundBuffer, containing the wave data and format information.
 */
class FCoreAudioSoundBuffer : public FSoundBuffer
{
public:
	/** 
	 * Constructor
	 *
	 * @param AudioDevice	audio device this sound buffer is going to be attached to.
	 */
	FCoreAudioSoundBuffer( FAudioDevice* AudioDevice, ESoundFormat SoundFormat );
	
	/**
	 * Destructor 
	 * 
	 * Frees wave data and detaches itself from audio device.
	 */
	~FCoreAudioSoundBuffer( void );
	
	/** 
	 * Setup an AudioStreamBasicDescription structure
	 */
	void InitAudioStreamBasicDescription( UInt32 FormatID, USoundWave* Wave, bool bCheckPCMData );
	
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
	void Seek( const float SeekTime );
		
	/**
	 * Static function used to create a CoreAudio buffer and dynamically upload decompressed ogg vorbis data to.
	 *
	 * @param InWave		USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FCoreAudioSoundBuffer* CreateQueuedBuffer( FCoreAudioDevice* CoreAudioDevice, USoundWave* Wave );
	
	/**
	 * Static function used to create a CoreAudio buffer and dynamically upload procedural data to.
	 *
	 * @param InWave		USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FCoreAudioSoundBuffer* CreateProceduralBuffer( FCoreAudioDevice* CoreAudioDevice, USoundWave* Wave );
	
	/**
	 * Static function used to create a CoreAudio buffer and upload raw PCM data to.
	 *
	 * @param InWave		USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FCoreAudioSoundBuffer* CreatePreviewBuffer( FCoreAudioDevice* CoreAudioDevice, USoundWave* Wave, FCoreAudioSoundBuffer* Buffer );
	
	/**
	 * Static function used to create a CoreAudio buffer and upload decompressed ogg vorbis data to.
	 *
	 * @param Wave			USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FCoreAudioSoundBuffer* CreateNativeBuffer( FCoreAudioDevice* CoreAudioDevice, USoundWave* Wave );
	
	/**
	 * Static function used to create an XAudio buffer and dynamically upload streaming data to.
	 *
	 * @param InWave		USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FCoreAudioSoundBuffer* CreateStreamingBuffer( FCoreAudioDevice* CoreAudio2Device, USoundWave* Wave );
	
	/**
	 * Static function used to create a buffer.
	 *
	 * @param InWave USoundWave to use as template and wave source
	 * @param AudioDevice audio device to attach created buffer to
	 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FCoreAudioSoundBuffer* Init( FAudioDevice* AudioDevice, USoundWave* InWave, bool bForceRealtime );
	
	/**
	 * Returns the size of this buffer in bytes.
	 *
	 * @return Size in bytes
	 */
	int32 GetSize( void );

	// These are used by the streaming engine to manage loading/unloading of chunks
	virtual int32 GetCurrentChunkIndex() const override;
	virtual int32 GetCurrentChunkOffset() const override;

	/** Audio device this buffer is attached to	*/
	FAudioDevice*				AudioDevice;

	/** Format of the sound referenced by this buffer */
	int32							SoundFormat;
	
	/** Format of the source PCM data */
	AudioStreamBasicDescription	PCMFormat;
	/** Address of PCM data in physical memory */
	uint8*						PCMData;
	/** Size of PCM data in physical memory */
	int32							PCMDataSize;
	
	/** Wrapper to handle the decompression of audio codecs */
	class ICompressedAudioInfo*		DecompressionState;
	/** Set to true when the PCM data should be freed when the buffer is destroyed */
	bool						bDynamicResource;
};

typedef FAsyncTask<class FAsyncRealtimeAudioTaskWorker<FCoreAudioSoundBuffer>> FAsyncRealtimeAudioTask;

/**
 * CoreAudio implementation of FSoundSource, the interface used to play, stop and update sources
 */
class FCoreAudioSoundSource : public FSoundSource
{
public:
	/**
	 * Constructor
	 *
	 * @param	InAudioDevice	audio device this source is attached to
	 */
	FCoreAudioSoundSource( FAudioDevice* InAudioDevice );
	
	/** 
	 * Destructor
	 */
	~FCoreAudioSoundSource( void );
	
	/**
	 * Initializes a source with a given wave instance and prepares it for playback.
	 *
	 * @param	WaveInstance	wave instance being primed for playback
	 * @return	true			if initialization was successful, false otherwise
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
	 * @return	true if the wave instance/ source has finished playback and false if it is 
	 *			currently playing or paused.
	 */
	virtual bool IsFinished( void );

	/** 
	 * Submit the relevant audio buffers to the system
	 */
	void SubmitPCMBuffers( void );
	
	/** 
	 * Submit the relevant audio buffers to the system
	 */
	void SubmitPCMRTBuffers( void );

	static OSStatus CoreAudioRenderCallback( void *InRefCon, AudioUnitRenderActionFlags *IOActionFlags,
											const AudioTimeStamp *InTimeStamp, UInt32 InBusNumber,
											UInt32 InNumberFrames, AudioBufferList *IOData );
	static OSStatus CoreAudioConvertCallback( AudioConverterRef converter, UInt32 *ioNumberDataPackets, AudioBufferList *ioData,
											 AudioStreamPacketDescription **outPacketDescription, void *inUserData );

protected:

	enum class EDataReadMode : uint8
	{
		Synchronous,
		Asynchronous,
		AsynchronousSkipFirstFrame
	};

	/**
	 * Handles feeding new data to a real time decompressed sound
	 */
	void HandleRealTimeSourceData(bool bLooped);

	/**
	 * Handles feeding new data to a real time decompressed sound
	 */
	void HandleRealTimeSource(bool bBlockForData);

	/** Decompress USoundWave procedure to generate more PCM data. Returns true/false: did audio loop? */
	bool ReadMorePCMData( const int32 BufferIndex, EDataReadMode DataReadMode );

	OSStatus CreateAudioUnit( OSType Type, OSType SubType, OSType Manufacturer, AudioStreamBasicDescription* InputFormat, AudioStreamBasicDescription* OutputFormat, AUNode* OutNode, AudioUnit* OutUnit );
	OSStatus ConnectAudioUnit( AUNode DestNode, uint32 DestInputNumber, AUNode OutNode, AudioUnit OutUnit );
	OSStatus CreateAndConnectAudioUnit( OSType Type, OSType SubType, OSType Manufacturer, AUNode DestNode, uint32 DestInputNumber, AudioStreamBasicDescription* InputFormat, AudioStreamBasicDescription* OutputFormat, AUNode* OutNode, AudioUnit* OutUnit );
	bool AttachToAUGraph();
	bool DetachFromAUGraph();

	/** Owning classes */
	FCoreAudioDevice*			AudioDevice;
	FCoreAudioEffectsManager*	Effects;

	/** Cached sound buffer associated with currently bound wave instance. */
	FCoreAudioSoundBuffer*		CoreAudioBuffer;

	AudioConverterRef			CoreAudioConverter;

	/** Asynchronous task for real time audio sources */
	FAsyncRealtimeAudioTask* RealtimeAsyncTask;

	/** Which sound buffer should be written to next - used for double buffering. */
	bool						bStreamedSound;
	/** A pair of sound buffers to allow notification when a sound loops. */
	FCoreAudioBuffer			CoreAudioBuffers[3];
	/** Set when we wish to let the buffers play themselves out */
	bool						bBuffersToFlush;

	AUNode						SourceNode;
	AudioUnit					SourceUnit;

	AUNode						EQNode;
	AudioUnit					EQUnit;

	AUNode						LowPassNode;
	AudioUnit					LowPassUnit;

	AUNode						RadioNode;
	AudioUnit					RadioUnit;
	bool						bRadioMuted;

	AUNode						ReverbNode;
	AudioUnit					ReverbUnit;
	bool						bReverbMuted;

	bool						bDryMuted;

	int32						AudioChannel;
	int32						BufferInUse;
	int32						NumActiveBuffers;
	
	int32						MixerInputNumber;

	FCriticalSection			CriticalSection;

private:

	void FreeResources();

	void InitSourceUnit(AudioStreamBasicDescription* Format, AUNode& HeadNode);
	void InitLowPassEffect(AudioStreamBasicDescription* Format, AUNode& HeadNode);
	void InitRadioSourceEffect(AudioStreamBasicDescription* Format, AUNode& HeadNode);
	void InitEqSourceEffect(AudioStreamBasicDescription* Format, AUNode& HeadNode);
	void InitReverbSourceEffect(AudioStreamBasicDescription* Format, AUNode& HeadNode);

	friend class FCoreAudioDevice;
	friend class FCoreAudioEffectsManager;
};

/**
 * CoreAudio implementation of an Unreal audio device.
 */
class FCoreAudioDevice : public FAudioDevice
{
	/** Starts up any platform specific hardware/APIs */
	virtual bool InitializeHardware() override;

	/** Shuts down any platform specific hardware/APIs */
	virtual void TeardownHardware() override;

	/** Lets the platform any tick actions */
	virtual void UpdateHardware() override;

	/** Creates a new platform specific sound source */
	virtual FAudioEffectsManager* CreateEffectsManager() override;

	/** Creates a new platform specific sound source */
	virtual FSoundSource* CreateSoundSource() override;

	/** Setup Matrix Mixer's input - enable input, input->output volumes */
	void SetupMatrixMixerInput( int32 Input, bool bIs6ChannelOGG );
	
	/** Set Matrix Mixer's input volume */
	void SetMatrixMixerInputVolume( int32 Input, float Volume );
	
	/** Get free 3D Mixer input number */
	int32 GetFreeMixer3DInput();
	
	/** Set free 3D Mixer input number */
	void SetFreeMixer3DInput( int32 Input );
	
	/** Get free Matrix Mixer input number */
	int32 GetFreeMatrixMixerInput();
	
	/** Set free Matrix Mixer input number */
	void SetFreeMatrixMixerInput( int32 Input );

	AUGraph GetAudioUnitGraph() const { return AudioUnitGraph; }
	AUNode GetMixer3DNode() const { return Mixer3DNode; }
	AudioUnit GetMixer3DUnit() const { return Mixer3DUnit; }
	AUNode GetMatrixMixerNode() const { return MatrixMixerNode; }
	AudioUnit GetMatrixMixerUnit() const { return MatrixMixerUnit; }
	
	virtual FName GetRuntimeFormat(USoundWave* SoundWave) override
	{
		static FName NAME_OPUS(TEXT("OPUS"));

		if (SoundWave->IsStreaming())
		{
			return NAME_OPUS;
		}
		static FName NAME_OGG(TEXT("OGG"));
		return NAME_OGG;
	}

	virtual bool HasCompressedAudioInfoClass(USoundWave* SoundWave) override;

	virtual bool SupportsRealtimeDecompression() const override
	{ 
		return true; 
	}

	virtual class ICompressedAudioInfo* CreateCompressedAudioInfo(USoundWave* SoundWave) override;

	FORCEINLINE uint32 CalculateMatrixElement( uint32 InputNum, uint32 OutputNum )
	{
		return ( ( InputNum << 16 ) | ( OutputNum & 0x0000FFFF ) );
	}

	int32 FindFreeAudioChannel()
	{
		for (int32 Index = 1; Index < CORE_AUDIO_MAX_CHANNELS; Index++)
		{
			if (AudioChannels[Index] == nullptr)
			{
				return Index;
			}
		}
		return 0;
	}

	void UpdateAUGraph();

protected:

	/** Inverse listener transformation, used for spatialization */
	FMatrix								InverseTransform;
	
private:

	AUGraph						AudioUnitGraph;
	AUNode						OutputNode;
	AudioUnit					OutputUnit;
	AUNode						Mixer3DNode;
	AudioUnit					Mixer3DUnit;
	AUNode						MatrixMixerNode;
	AudioUnit					MatrixMixerUnit;
	AudioStreamBasicDescription	Mixer3DFormat;
	AudioStreamBasicDescription	MatrixMixerInputFormat;
	AudioStreamBasicDescription	MatrixMixerOutputFormat;

	bool						Mixer3DInputStatus[CORE_AUDIO_MAX_CHANNELS];
	bool						MatrixMixerInputStatus[CORE_AUDIO_MAX_MULTICHANNEL_AUDIOCHANNELS];

	class FCoreAudioSoundSource* AudioChannels[CORE_AUDIO_MAX_CHANNELS];
	

	TSet<AudioConverterRef>		ConvertersToDispose;
	TArray<FCoreAudioSoundSource*> SourcesAttached;
	TArray<FCoreAudioSoundSource*> SourcesDetached;
	bool						bNeedsUpdate;
	bool						bAuGraphErrorLogged;

	friend class FCoreAudioSoundBuffer;
	friend class FCoreAudioSoundSource;
	friend class FCoreAudioEffectsManager;
};
