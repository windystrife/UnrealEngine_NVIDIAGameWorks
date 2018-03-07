// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Audio.h: Unreal base audio.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/ThreadSafeBool.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundEffectSource.h"
#include "Sound/SoundSourceBusSend.h"
#include "IAudioExtensionPlugin.h"
#include "AudioPluginUtilities.h"

class FAudioDevice;
class USoundNode;
class USoundWave;
class USoundClass;
class USoundSubmix;
class USoundSourceBus;
struct FActiveSound;
struct FWaveInstance;
struct FSoundSourceBusSendInfo;

//#include "Sound/SoundConcurrency.h"

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogAudio, Warning, All);

// Special log category used for temporary programmer debugging code of audio
ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogAudioDebug, Display, All);
 
/** 
 * Maximum number of channels that can be set using the ini setting
 */
#define MAX_AUDIOCHANNELS				64


/** 
 * Length of sound in seconds to be considered as looping forever
 */
#define INDEFINITELY_LOOPING_DURATION	10000.0f

/**
 * Some defaults to help cross platform consistency
 */
#define SPEAKER_COUNT					6

#define DEFAULT_LOW_FREQUENCY			600.0f
#define DEFAULT_MID_FREQUENCY			1000.0f
#define DEFAULT_HIGH_FREQUENCY			2000.0f

#define MAX_VOLUME						4.0f
#define MIN_PITCH						0.4f
#define MAX_PITCH						2.0f

#define MIN_SOUND_PRIORITY				0.0f
#define MAX_SOUND_PRIORITY				100.0f

#define DEFAULT_SUBTITLE_PRIORITY		10000.0f

/**
 * Some filters don't work properly with extreme values, so these are the limits 
 */
#define MIN_FILTER_GAIN					0.126f
#define MAX_FILTER_GAIN					7.94f

#define MIN_FILTER_FREQUENCY			20.0f
#define MAX_FILTER_FREQUENCY			20000.0f

#define MIN_FILTER_BANDWIDTH			0.1f
#define MAX_FILTER_BANDWIDTH			2.0f

/**
 * Audio stats
 */
DECLARE_DWORD_COUNTER_STAT_EXTERN( TEXT( "Active Sounds" ), STAT_ActiveSounds, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Audio Evaluate Concurrency"), STAT_AudioEvaluateConcurrency, STATGROUP_Audio, );
DECLARE_DWORD_COUNTER_STAT_EXTERN( TEXT( "Audio Sources" ), STAT_AudioSources, STATGROUP_Audio , );
DECLARE_DWORD_COUNTER_STAT_EXTERN( TEXT( "Wave Instances" ), STAT_WaveInstances, STATGROUP_Audio , );
DECLARE_DWORD_COUNTER_STAT_EXTERN( TEXT( "Wave Instances Dropped" ), STAT_WavesDroppedDueToPriority, STATGROUP_Audio , );
DECLARE_DWORD_COUNTER_STAT_EXTERN( TEXT( "Audible Wave Instances Dropped" ), STAT_AudibleWavesDroppedDueToPriority, STATGROUP_Audio , );
DECLARE_DWORD_COUNTER_STAT_EXTERN( TEXT( "Finished delegates called" ), STAT_AudioFinishedDelegatesCalled, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Finished delegates time" ), STAT_AudioFinishedDelegates, STATGROUP_Audio , );
DECLARE_MEMORY_STAT_EXTERN( TEXT( "Audio Memory Used" ), STAT_AudioMemorySize, STATGROUP_Audio , );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN( TEXT( "Audio Buffer Time" ), STAT_AudioBufferTime, STATGROUP_Audio , );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN( TEXT( "Audio Buffer Time (w/ Channels)" ), STAT_AudioBufferTimeChannels, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Gathering WaveInstances" ), STAT_AudioGatherWaveInstances, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Processing Sources" ), STAT_AudioStartSources, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Updating Sources" ), STAT_AudioUpdateSources, STATGROUP_Audio , ENGINE_API);
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Updating Effects" ), STAT_AudioUpdateEffects, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Source Init" ), STAT_AudioSourceInitTime, STATGROUP_Audio , ENGINE_API);
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Source Create" ), STAT_AudioSourceCreateTime, STATGROUP_Audio , ENGINE_API);
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Submit Buffers" ), STAT_AudioSubmitBuffersTime, STATGROUP_Audio , ENGINE_API);
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Decompress Audio" ), STAT_AudioDecompressTime, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Decompress Vorbis" ), STAT_VorbisDecompressTime, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Prepare Audio Decompression" ), STAT_AudioPrepareDecompressionTime, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Prepare Vorbis Decompression" ), STAT_VorbisPrepareDecompressionTime, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Finding Nearest Location" ), STAT_AudioFindNearestLocation, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Decompress Streamed" ), STAT_AudioStreamedDecompressTime, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Buffer Creation" ), STAT_AudioResourceCreationTime, STATGROUP_Audio , );

/**
 * Channel definitions for multistream waves
 *
 * These are in the sample order OpenAL expects for a 7.1 sound
 * 
 */
enum EAudioSpeakers
{							//	4.0	5.1	6.1	7.1
	SPEAKER_FrontLeft,		//	*	*	*	*
	SPEAKER_FrontRight,		//	*	*	*	*
	SPEAKER_FrontCenter,	//		*	*	*
	SPEAKER_LowFrequency,	//		*	*	*
	SPEAKER_LeftSurround,	//	*	*	*	*
	SPEAKER_RightSurround,	//	*	*	*	*
	SPEAKER_LeftBack,		//			*	*		If there is no BackRight channel, this is the BackCenter channel
	SPEAKER_RightBack,		//				*
	SPEAKER_Count
};

namespace EAudioMixerChannel
{
	/** Enumeration values represent sound file or speaker channel types. */
	enum Type
	{
		FrontLeft,
		FrontRight,
		FrontCenter,
		LowFrequency,
		BackLeft,
		BackRight,
		FrontLeftOfCenter,
		FrontRightOfCenter,
		BackCenter,
		SideLeft,
		SideRight,
		TopCenter,
		TopFrontLeft,
		TopFrontCenter,
		TopFrontRight,
		TopBackLeft,
		TopBackCenter,
		TopBackRight,
		Unknown,
		ChannelTypeCount
	};

	static const int32 MaxSupportedChannel = EAudioMixerChannel::TopCenter;

	inline const TCHAR* ToString(EAudioMixerChannel::Type InType)
	{
		switch (InType)
		{
		case FrontLeft:				return TEXT("FrontLeft");
		case FrontRight:			return TEXT("FrontRight");
		case FrontCenter:			return TEXT("FrontCenter");
		case LowFrequency:			return TEXT("LowFrequency");
		case BackLeft:				return TEXT("BackLeft");
		case BackRight:				return TEXT("BackRight");
		case FrontLeftOfCenter:		return TEXT("FrontLeftOfCenter");
		case FrontRightOfCenter:	return TEXT("FrontRightOfCenter");
		case BackCenter:			return TEXT("BackCenter");
		case SideLeft:				return TEXT("SideLeft");
		case SideRight:				return TEXT("SideRight");
		case TopCenter:				return TEXT("TopCenter");
		case TopFrontLeft:			return TEXT("TopFrontLeft");
		case TopFrontCenter:		return TEXT("TopFrontCenter");
		case TopFrontRight:			return TEXT("TopFrontRight");
		case TopBackLeft:			return TEXT("TopBackLeft");
		case TopBackCenter:			return TEXT("TopBackCenter");
		case TopBackRight:			return TEXT("TopBackRight");
		case Unknown:				return TEXT("Unknown");

		default:
			return TEXT("UNSUPPORTED");
		}
	}
}


// Forward declarations.
class UAudioComponent;
class USoundNode;
struct FWaveInstance;
struct FReverbSettings;
struct FSampleLoop;

enum ELoopingMode
{
	/** One shot sound */
	LOOP_Never,
	/** Call the user callback on each loop for dynamic control */
	LOOP_WithNotification,
	/** Loop the sound forever */
	LOOP_Forever
};

struct ENGINE_API FNotifyBufferFinishedHooks
{
	void AddNotify(USoundNode* NotifyNode, UPTRINT WaveInstanceHash);
	UPTRINT GetHashForNode(USoundNode* NotifyNode) const;
	void AddReferencedObjects( FReferenceCollector& Collector );
	void DispatchNotifies(FWaveInstance* WaveInstance, const bool bStopped);

	friend FArchive& operator<<( FArchive& Ar, FNotifyBufferFinishedHooks& WaveInstance );

private:

	struct FNotifyBufferDetails
	{
		USoundNode* NotifyNode;
		UPTRINT NotifyNodeWaveInstanceHash;

		FNotifyBufferDetails()
			: NotifyNode(NULL)
			, NotifyNodeWaveInstanceHash(0)
		{
		}

		FNotifyBufferDetails(USoundNode* InNotifyNode, UPTRINT InHash)
			: NotifyNode(InNotifyNode)
			, NotifyNodeWaveInstanceHash(InHash)
		{
		}
	};


	TArray<FNotifyBufferDetails> Notifies;
};

/** Queries if a plugin of the given type is enabled. */
ENGINE_API bool IsAudioPluginEnabled(EAudioPlugin PluginType);
ENGINE_API bool DoesAudioPluginHaveCustomSettings(EAudioPlugin PluginType);

/**
 * Structure encapsulating all information required to play a USoundWave on a channel/source. This is required
 * as a single USoundWave object can be used in multiple active cues or multiple times in the same cue.
 */
struct ENGINE_API FWaveInstance
{
	/** Static helper to create good unique type hashes */
	static uint32 TypeHashCounter;

	/** Wave data */
	USoundWave* WaveData;

	/** Sound class */
	USoundClass* SoundClass;

	/** Sound submix object to send audio to for mixing in audio mixer.  */
	USoundSubmix* SoundSubmix;

	/** Sound submix sends */
	TArray<FSoundSubmixSendInfo> SoundSubmixSends;

	/** The sound sourcebus sends. */
	TArray<FSoundSourceBusSendInfo> SoundSourceBusSends;

	/** Sound effect chain */
	USoundEffectSourcePresetChain* SourceEffectChain;

	/** Sound nodes to notify when the current audio buffer finishes */
	FNotifyBufferFinishedHooks NotifyBufferFinishedHooks;

	/** Active Sound this wave instance belongs to */
	struct FActiveSound* ActiveSound;

private:

	/** Current volume */
	float Volume;

	/** Volume attenuation due to distance. */
	float DistanceAttenuation;

	/** Current volume multiplier - used to zero the volume without stopping the source */
	float VolumeMultiplier;

	/** The volume of the wave instance due to application volume or tab-state */
	float VolumeApp;

public:

	/** An audio component priority value that scales with volume (post all gain stages) and is used to determine voice playback priority. */
	float Priority;

	/** Voice center channel volume */
	float VoiceCenterChannelVolume;

	/** Volume of the radio filter effect */
	float RadioFilterVolume;

	/** The volume at which the radio filter kicks in */
	float RadioFilterVolumeThreshold;

	/** The amount of stereo sounds to bleed to the rear speakers */
	float StereoBleed;

	/** The amount of a sound to bleed to the LFE channel */
	float LFEBleed;

	/** Looping mode - None, loop with notification, forever */
	ELoopingMode LoopingMode;

	/** An offset/seek time to play this wave instance. */
	float StartTime;

	/** Whether or not to only output this wave instances audio to buses only. Audio mixer only. */
	uint32 bOutputToBusOnly:1;

	/** Set to true if the sound nodes state that the radio filter should be applied */
	uint32 bApplyRadioFilter:1;

	/** Whether wave instanced has been started */
	uint32 bIsStarted:1;

	/** Whether wave instanced is finished */
	uint32 bIsFinished:1;

	/** Whether the notify finished hook has been called since the last update/parsenodes */
	uint32 bAlreadyNotifiedHook:1;

	/** Whether to use spatialization */
	uint32 bUseSpatialization:1;

	/** Whether or not to enable the low pass filter */
	uint32 bEnableLowPassFilter:1;

	/** Whether or not the sound is occluded. */
	uint32 bIsOccluded:1;

	/** Whether to apply audio effects */
	uint32 bEQFilterApplied:1;

	/** Whether or not this sound plays when the game is paused in the UI */
	uint32 bIsUISound:1;

	/** Whether or not this wave is music */
	uint32 bIsMusic:1;

	/** Whether or not this wave has reverb applied */
	uint32 bReverb:1;

	/** Whether or not this sound class forces sounds to the center channel */
	uint32 bCenterChannelOnly:1;

	/** Whether or not this sound is manually paused */
	uint32 bIsPaused:1;

	/** Prevent spamming of spatialization of surround sounds by tracking if the warning has already been emitted */
	uint32 bReportedSpatializationWarning:1;

	/** Which spatialization method to use to spatialize 3d sounds. */
	ESoundSpatializationAlgorithm SpatializationMethod;

	/** The occlusion plugin settings to use for the wave instance. */
	USpatializationPluginSourceSettingsBase* SpatializationPluginSettings;

	/** The occlusion plugin settings to use for the wave instance. */
	UOcclusionPluginSourceSettingsBase* OcclusionPluginSettings;

	/** The occlusion plugin settings to use for the wave instance. */
	UReverbPluginSourceSettingsBase* ReverbPluginSettings;

	/** Which output target the sound should play on. */
	EAudioOutputTarget::Type OutputTarget;

	/** The low pass filter frequency to use */
	float LowPassFilterFrequency;

	/** The low pass filter frequency to use if the sound is occluded. */
	float OcclusionFilterFrequency;

	/** The low pass filter frequency to use due to ambient zones. */
	float AmbientZoneFilterFrequency;

	/** The low pass filter frequency to use due to distance attenuation. */
	float AttenuationLowpassFilterFrequency;

	/** The high pass filter frequency to use due to distance attenuation. (using in audio mixer only) */
	float AttenuationHighpassFilterFrequency;

	/** Current pitch scale. */
	float Pitch;

	/** Current location */
	FVector Location;

	/** At what distance we start transforming into omnidirectional soundsource */
	float OmniRadius;

	/** Amount of spread for 3d multi-channel asset spatialization */
	float StereoSpread;

	/** Distance over which the sound is attenuated. */
	float AttenuationDistance;

	/** The distance from this wave instance to the closest listener. */
	float ListenerToSoundDistance;

	/** The absolute position of the wave instance relative to forward vector of listener. */
	float AbsoluteAzimuth; 

	/** The reverb send method to use. */
	EReverbSendMethod ReverbSendMethod;

	/** Reverb distance-based wet-level amount range. */
	FVector2D ReverbSendLevelRange;

	/** Reverb distance-based wet-level distance/radial range. */
	FVector2D ReverbSendLevelDistanceRange;

	/** Custom reverb send curve. */
	FRuntimeFloatCurve CustomRevebSendCurve;

	/** The manual send level to use if the sound is set to use manual send level. */
	float ManualReverbSendLevel;

	/** Cached type hash */
	uint32 TypeHash;

	/** Hash value for finding the wave instance based on the path through the cue to get to it */
	UPTRINT WaveInstanceHash;

	/** User / Controller index that owns the sound */
	uint8 UserIndex;

	/** Constructor, initializing all member variables. */
	FWaveInstance(FActiveSound* ActiveSound);

	/** Stops the wave instance without notifying NotifyWaveInstanceFinishedHook. */
	void StopWithoutNotification();

	/** Notifies the wave instance that the current playback buffer has finished. */
	void NotifyFinished(const bool bStopped = false);

	/** Friend archive function used for serialization. */
	friend FArchive& operator<<(FArchive& Ar, FWaveInstance* WaveInstance);

	/** Function used by the GC. */
	void AddReferencedObjects(FReferenceCollector& Collector);

	/** Returns the actual volume the wave instance will play at */
	bool ShouldStopDueToMaxConcurrency() const;

	/** Setters for various volume values on wave instances. */
	void SetVolume(const float InVolume) { Volume = InVolume; }
	void SetDistanceAttenuation(const float InDistanceAttenuation) { DistanceAttenuation = InDistanceAttenuation; }
	void SetVolumeApp(const float InVolumeApp) { VolumeApp = InVolumeApp; }
	void SetVolumeMultiplier(const float InVolumeMultiplier) { VolumeMultiplier = InVolumeMultiplier; }

	/** Returns the volume multiplier on the wave instance. */
	float GetVolumeMultiplier() const { return VolumeMultiplier; }

	/** Returns the actual volume the wave instance will play at, including all gain stages. */
	float GetActualVolume() const;

	/** Returns the volume of the sound including distance attenuation. */
	float GetVolumeWithDistanceAttenuation() const;

	/** Returns the distance attenuation of the source voice. */
	float GetDistanceAttenuation() const;

	/** Returns the volume of the wave instance (ignoring application muting) */
	float GetVolume() const;

	/** Returns the volume due to application behavior for the wave instance. */
	float GetVolumeApp() const { return VolumeApp; }

	/** Returns the weighted priority of the wave instance. */
	float GetVolumeWeightedPriority() const;

	/** Checks whether wave is streaming and streaming is supported */
	bool IsStreaming() const;

	/** Returns the name of the contained USoundWave */
	FString GetName() const;
};

inline uint32 GetTypeHash(FWaveInstance* A) { return A->TypeHash; }

/*-----------------------------------------------------------------------------
	FSoundBuffer.
-----------------------------------------------------------------------------*/

class FSoundBuffer
{
public:
	FSoundBuffer(class FAudioDevice * InAudioDevice)
		: ResourceID(0)
		, NumChannels(0)
		, bAllocationInPermanentPool(false)
		, AudioDevice(InAudioDevice)
	{

	}

	ENGINE_API virtual ~FSoundBuffer();

	virtual int32 GetSize() PURE_VIRTUAL(FSoundBuffer::GetSize,return 0;);

	/**
	 * Describe the buffer (platform can override to add to the description, but should call the base class version)
	 * 
	 * @param bUseLongNames If TRUE, this will print out the full path of the sound resource, otherwise, it will show just the object name
	 */
	ENGINE_API virtual FString Describe(bool bUseLongName);

	/**
	 * Return the name of the sound class for this buffer 
	 */
	FName GetSoundClassName();

	/**
	 * Turn the number of channels into a string description
	 */
	FString GetChannelsDesc();

	/**
	 * Reads the compressed info of the given sound wave. Not implemented on all platforms.
	 */
	virtual bool ReadCompressedInfo(USoundWave* SoundWave) { return true; }

	/** Reads the next compressed data chunk */
	virtual bool ReadCompressedData(uint8* Destination, bool bLooping) { return true; }
	
	/** Seeks the buffer to the given seek time */
	virtual void Seek(const float SeekTime) {}

	/**
	 * Gets the chunk index that was last read from (for Streaming Manager requests)
	 */
	virtual int32 GetCurrentChunkIndex() const {return -1;}

	/**
	 * Gets the offset into the chunk that was last read to (for Streaming Manager priority)
	 */
	virtual int32 GetCurrentChunkOffset() const {return -1;}

	/** Returns whether or not a real-time decoding buffer is ready for playback */
	virtual bool IsRealTimeSourceReady() { return true; }

	/** Forces any pending async realtime source tasks to finish for the buffer */
	virtual void EnsureRealtimeTaskCompletion() { }

	/** Unique ID that ties this buffer to a USoundWave */
	int32	ResourceID;
	/** Cumulative channels from all streams */
	int32	NumChannels;
	/** Human readable name of resource, most likely name of UObject associated during caching.	*/
	FString	ResourceName;
	/** Whether memory for this buffer has been allocated from permanent pool. */
	bool	bAllocationInPermanentPool;
	/** Parent Audio Device used when creating the sound buffer. Used to remove tracking info on this sound buffer when its done. */
	class FAudioDevice * AudioDevice;
};

/*-----------------------------------------------------------------------------
FSoundSource.
-----------------------------------------------------------------------------*/

class FSoundSource
{
public:
	/** Constructor */
	FSoundSource(FAudioDevice* InAudioDevice)
		: AudioDevice(InAudioDevice)
		, WaveInstance(nullptr)
		, Buffer(nullptr)
		, StereoBleed(0.0f)
		, LFEBleed(0.5f)
		, LPFFrequency(MAX_FILTER_FREQUENCY)
		, HPFFrequency(MIN_FILTER_FREQUENCY)
		, LastLPFFrequency(MAX_FILTER_FREQUENCY)
		, LastHPFFrequency(MIN_FILTER_FREQUENCY)
		, PlaybackTime(0.0f)
		, Pitch(1.0f)
		, LastUpdate(0)
		, LastHeardUpdate(0)
		, LeftChannelSourceLocation(0)
		, RightChannelSourceLocation(0)
		, NumFramesPlayed(0)
		, NumTotalFrames(1)
		, StartFrame(0)
		, VoiceId(-1)
		, Playing(false)
		, bReverbApplied(false)
		, bIsPausedByGame(false)
		, bIsManuallyPaused(false)
		, Paused(false)
		, bInitialized(true) // Note: this is defaulted to true since not all platforms need to deal with async initialization.
		, bIsPreviewSound(false)
		, bIsVirtual(false)
	{
	}

	/** Destructor */
	virtual ~FSoundSource() {}

	/* Prepares the source voice for initialization. This may parse a compressed asset header on some platforms */
	virtual bool PrepareForInitialization(FWaveInstance* InWaveInstance) { return true; }

	/** Returns if the source voice is prepared to initialize. */
	virtual bool IsPreparedToInit() { return true; }

	/** Initializes the sound source. */
	virtual bool Init(FWaveInstance* InWaveInstance) = 0;

	/** Returns whether or not the sound source has initialized. */
	bool IsInitialized() const { return bInitialized; };

	/** Updates the sound source. */
	virtual void Update() = 0;

	/** Plays the sound source. */
	virtual void Play() = 0;

	/** Stops the sound source. */
	ENGINE_API virtual void Stop();

	/** Returns true if the sound source has finished playing. */
	virtual	bool IsFinished() = 0;
	
	/** Pause the source from game pause */
	void SetPauseByGame(bool bInIsPauseByGame);

	/** Pause the source manually */
	void SetPauseManually(bool bInIsPauseManually);

	/** Returns a string describing the source (subclass can override, but it should call the base and append). */
	ENGINE_API virtual FString Describe(bool bUseLongName);

	/** Returns source is an in-game only. Will pause when in UI. */
	bool IsGameOnly() const;

	/** Returns the wave instance of the sound source. */
	const FWaveInstance* GetWaveInstance() const { return WaveInstance; }

	/** Returns whether or not the sound source is playing. */
	bool IsPlaying() const { return Playing; }

	/**  Returns true if the sound is paused. */
	bool IsPaused() const { return Paused; }
	
	/**  Returns true if the sound is paused. */
	bool IsPausedByGame() const { return bIsPausedByGame; }

	bool IsPausedManually() const { return bIsManuallyPaused; }

	/** Returns true if reverb should be applied. */
	bool IsReverbApplied() const { return bReverbApplied; }

	/** Returns true if EQ should be applied. */
	bool IsEQFilterApplied() const  { return WaveInstance->bEQFilterApplied; }

	/** Set the bReverbApplied variable. */
	ENGINE_API bool SetReverbApplied(bool bHardwareAvailable);

	/** Set the StereoBleed variable. */
	ENGINE_API float SetStereoBleed();

	/** Updates and sets the LFEBleed variable. */
	ENGINE_API float SetLFEBleed();

	/** Updates the FilterFrequency value. */
	ENGINE_API void SetFilterFrequency();

	/** Updates the stereo emitter positions of this voice. */
	ENGINE_API void UpdateStereoEmitterPositions();

	/** Draws debug info about this source voice if enabled. */
	ENGINE_API void DrawDebugInfo();

	/** Gets parameters necessary for computing 3d spatialization of sources. */
	ENGINE_API FSpatializationParams GetSpatializationParams();

	/** Returns the contained sound buffer object. */
	const FSoundBuffer* GetBuffer() const { return Buffer; }

	/** Initializes any source effects for this sound source. */
	virtual void InitializeSourceEffects(uint32 InEffectVoiceId)
	{
	}

	/** Sets if this voice is virtual. */
	void SetVirtual()
	{
		bIsVirtual = true;
	}

	/** Returns the source's playback percent. */
	ENGINE_API virtual float GetPlaybackPercent() const;

	void NotifyPlaybackPercent();

protected:

	/** Initializes common data for all sound source types. */
	ENGINE_API void InitCommon();

	/** Updates common data for all sound source types. */
	ENGINE_API void UpdateCommon();

	/** Pauses the sound source. */
	virtual void Pause() = 0;

	/** Updates this source's pause state */
	void UpdatePause();

	/** Returns the volume of the sound source after evaluating debug commands */
	ENGINE_API float GetDebugVolume(const float InVolume);

	/** Owning audio device. */
	FAudioDevice* AudioDevice;

	/** Contained wave instance. */
	FWaveInstance* WaveInstance;

	/** Cached sound buffer associated with currently bound wave instance. */
	FSoundBuffer* Buffer;

	/** The amount of stereo sounds to bleed to the rear speakers */
	float StereoBleed;

	/** The amount of a sound to bleed to the LFE speaker */
	float LFEBleed;

	/** What frequency to set the LPF filter to. Note this could be caused by occlusion, manual LPF application, or LPF distance attenuation. */
	float LPFFrequency;

	/** What frequency to set the HPF filter to. Note this could be caused by HPF distance attenuation. */
	float HPFFrequency;

	/** The last LPF frequency set. Used to avoid making API calls when parameter doesn't changing. */
	float LastLPFFrequency;

	/** The last HPF frequency set. Used to avoid making API calls when parameter doesn't changing. */
	float LastHPFFrequency;

	/** The virtual current playback time. Used to trigger notifications when finished. */
	float PlaybackTime;

	/** The pitch of the sound source. */
	float Pitch;

	/** Last tick when this source was active */
	int32 LastUpdate;

	/** Last tick when this source was active *and* had a hearable volume */
	int32 LastHeardUpdate;

	/** The location of the left-channel source for stereo spatialization. */
	FVector LeftChannelSourceLocation;

	/** The location of the right-channel source for stereo spatialization. */
	FVector RightChannelSourceLocation;

	/** The number of frames (Samples / NumChannels) played by the sound source. */
	int32 NumFramesPlayed;

	/** The total number of frames of audio for the sound wave */
	int32 NumTotalFrames;

	/** The frame we started on. */
	int32 StartFrame;

	/** Effect ID of this sound source in the audio device sound source array. */
	uint32 VoiceId;

	/** Whether we are playing or not. */
	FThreadSafeBool Playing;

	/** Cached sound mode value used to detect when to switch outputs. */
	uint8 bReverbApplied : 1;

	/** Whether we are paused by game state or not. */
	uint8 bIsPausedByGame : 1;

	/** Whether or not we were paused manually. */
	uint8 bIsManuallyPaused : 1;

	/** Whether or not we are actually paused. */
	uint8 Paused : 1;

	/** Whether or not the sound source is initialized. */
	uint8 bInitialized : 1;

	/** Whether or not the sound is a preview sound. */
	uint8 bIsPreviewSound : 1;

	/** True if this isn't a real hardware voice */
	uint32 bIsVirtual : 1;

	friend class FAudioDevice;
};


/*-----------------------------------------------------------------------------
	FWaveModInfo. 
-----------------------------------------------------------------------------*/

//
// Structure for in-memory interpretation and modification of WAVE sound structures.
//
class FWaveModInfo
{
public:

	// Pointers to variables in the in-memory WAVE file.
	uint32* pSamplesPerSec;
	uint32* pAvgBytesPerSec;
	uint16* pBlockAlign;
	uint16* pBitsPerSample;
	uint16* pChannels;
	uint16* pFormatTag;

	uint32* pWaveDataSize;
	uint32* pMasterSize;
	uint8*  SampleDataStart;
	uint8*  SampleDataEnd;
	uint32  SampleDataSize;
	uint8*  WaveDataEnd;

	uint32  NewDataSize;

	// Constructor.
	FWaveModInfo()
	{
	}
	
	// 16-bit padding.
	static uint32 Pad16Bit( uint32 InDW )
	{
		return ((InDW + 1)& ~1);
	}

	// Read headers and load all info pointers in WaveModInfo. 
	// Returns 0 if invalid data encountered.
	ENGINE_API bool ReadWaveInfo( uint8* WaveData, int32 WaveDataSize, FString* ErrorMessage = NULL, bool InHeaderDataOnly = false, void** OutFormatHeader = NULL );
	
	/**
	 * Read a wave file header from bulkdata
	 */
	ENGINE_API bool ReadWaveHeader(uint8* RawWaveData, int32 Size, int32 Offset );

	ENGINE_API void ReportImportFailure() const;
};

/**
 * Brings loaded sounds up to date for the given platforms (or all platforms), and also sets persistent variables to cover any newly loaded ones.
 *
 * @param	Platform				Name of platform to cook for, or NULL if all platforms
 */
ENGINE_API void SetCompressedAudioFormatsToBuild(const TCHAR* Platform = NULL);

/**
 * Brings loaded sounds up to date for the given platforms (or all platforms), and also sets persistent variables to cover any newly loaded ones.
 *
 * @param	Platform				Name of platform to cook for, or NULL if all platforms
 */
ENGINE_API const TArray<FName>& GetCompressedAudioFormatsToBuild();


