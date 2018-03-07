// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorldCollision.h"
#include "Sound/SoundAttenuation.h"
#include "HAL/ThreadSafeBool.h"
#include "Audio.h"
#include "Sound/SoundConcurrency.h"
#include "Components/AudioComponent.h"
#include "Sound/AudioVolume.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundSourceBus.h"
#include "AudioDevice.h"

class FAudioDevice;
class USoundBase;
class USoundSubmix;
class USoundSourceBus;
struct FSoundSubmixSendInfo;
struct FSoundSourceBusSendInfo;
class USoundWave;
struct FListener;

/**
 *	Struct used for gathering the final parameters to apply to a wave instance
 */
struct FSoundParseParameters
{
	// A collection of 
	FNotifyBufferFinishedHooks NotifyBufferFinishedHooks;

	// The Sound Class to use the settings of
	USoundClass* SoundClass;
	
	// The transform of the sound (scale is not used)
	FTransform Transform;

	// The speed that the sound is moving relative to the listener
	FVector Velocity;
	
	// The volume product of the sound
	float Volume;

	// The attenuation of the sound due to distance attenuation
	float DistanceAttenuation;

	// A volume scalse on the sound specified by user
	float VolumeMultiplier;
	
	// Volume due to application-level volume scaling (tabbing, master volume)
	float VolumeApp;

	// The multiplier to apply if the sound class desires
	float InteriorVolumeMultiplier;

	// The priority of sound, which is the product of the component priority and the USoundBased priority
	float Priority;

	// The pitch scale factor of the sound
	float Pitch;

	// How far in to the sound the
	float StartTime;

	// At what distance from the source of the sound should spatialization begin
	float OmniRadius;

	// The distance over which the sound is attenuated 
	float AttenuationDistance;

	// The distance from the listener to the sound
	float ListenerToSoundDistance;

	// The absolute azimuth angle of the sound relative to the forward listener vector (359 degrees to left, 1 degrees to right)
	float AbsoluteAzimuth;

	// The sound submix to use for the wave instance
	USoundSubmix* SoundSubmix;

	// The submix sends to use
	TArray<FSoundSubmixSendInfo> SoundSubmixSends;

	// The source bus sends to use
	TArray<FSoundSourceBusSendInfo> SoundSourceBusSends;

	// Reverb wet-level parameters
	EReverbSendMethod ReverbSendMethod;
	FVector2D ReverbSendLevelRange;
	FVector2D ReverbSendLevelDistanceRange;
	float ManualReverbSendLevel;
	FRuntimeFloatCurve CustomReverbSendCurve;

	// The distance between left and right channels when spatializing stereo assets
	float StereoSpread;

	// Which spatialization algorithm to use
	ESoundSpatializationAlgorithm SpatializationMethod;

	// What occlusion plugin source settings to use
	USpatializationPluginSourceSettingsBase* SpatializationPluginSettings;

	// What occlusion plugin source settings to use
	UOcclusionPluginSourceSettingsBase* OcclusionPluginSettings;

	// What reverb plugin source settings to use
	UReverbPluginSourceSettingsBase* ReverbPluginSettings;

	// What source effect chain to use
	USoundEffectSourcePresetChain* SourceEffectChain;

	// The lowpass filter frequency to apply (if enabled)
	float LowPassFilterFrequency;

	// The lowpass filter frequency to apply due to distance attenuation
	float AttenuationLowpassFilterFrequency;

	// The highpass filter frequency to apply due to distance attenuation
	float AttenuationHighpassFilterFrequency;

	// The lowpass filter to apply if the sound is occluded
	float OcclusionFilterFrequency;

	// The lowpass filter to apply if the sound is inside an ambient zone
	float AmbientZoneFilterFrequency;

	// Whether or not to ouput this audio to buses only
	uint32 bOutputToBusOnly:1;

	// Whether the sound should be spatialized
	uint32 bUseSpatialization:1;

	// Whether the sound should be seamlessly looped
	uint32 bLooping:1;
	
	// Whether we have enabled low-pass filtering of this sound
	uint32 bEnableLowPassFilter:1;

	// Whether this sound is occluded
	uint32 bIsOccluded:1;

	// Whether or not this sound is manually paused (i.e. not by application-wide pause)
	uint32 bIsPaused:1;

	// Whether or not to apply a =6 dB attenuation to stereo spatialization sounds
	uint32 bApplyNormalizationToStereoSounds:1;

	FSoundParseParameters()
		: SoundClass(nullptr)
		, Velocity(ForceInit)
		, Volume(1.f)
		, DistanceAttenuation(1.f)
		, VolumeMultiplier(1.f)
		, VolumeApp(1.f)
		, InteriorVolumeMultiplier(1.f)
		, Pitch(1.f)
		, StartTime(-1.f)
		, OmniRadius(0.0f)
		, AttenuationDistance(0.0f)
		, ListenerToSoundDistance(0.0f)
		, AbsoluteAzimuth(0.0f)
		, SoundSubmix(nullptr)
		, ReverbSendMethod(EReverbSendMethod::Linear)
		, ReverbSendLevelRange(0.0f, 0.0f)
		, ReverbSendLevelDistanceRange(0.0f, 0.0f)
		, ManualReverbSendLevel(0.2f)
		, StereoSpread(0.0f)
		, SpatializationMethod(ESoundSpatializationAlgorithm::SPATIALIZATION_Default)
		, SpatializationPluginSettings(nullptr)
		, OcclusionPluginSettings(nullptr)
		, ReverbPluginSettings(nullptr)
		, SourceEffectChain(nullptr)
		, LowPassFilterFrequency(MAX_FILTER_FREQUENCY)
		, AttenuationLowpassFilterFrequency(MAX_FILTER_FREQUENCY)
		, AttenuationHighpassFilterFrequency(MIN_FILTER_FREQUENCY)
		, OcclusionFilterFrequency(MAX_FILTER_FREQUENCY)
		, AmbientZoneFilterFrequency(MAX_FILTER_FREQUENCY)
		, bOutputToBusOnly(false)
		, bUseSpatialization(false)
		, bLooping(false)
		, bEnableLowPassFilter(false)
		, bIsOccluded(false)
		, bIsPaused(false)
		, bApplyNormalizationToStereoSounds(false)
	{
	}
};

struct ENGINE_API FActiveSound
{
public:

	FActiveSound();
	~FActiveSound();

private:
	TWeakObjectPtr<UWorld> World;
	uint32 WorldID;

	class USoundBase* Sound;

	uint64 AudioComponentID;
	FName AudioComponentUserID;
	uint32 OwnerID;
	
	FName AudioComponentName;
	FName OwnerName;

public:

	uint64 GetAudioComponentID() const { return AudioComponentID; }
	FName GetAudioComponentUserID() const { return AudioComponentUserID; }
	void SetAudioComponent(UAudioComponent* Component);
	void SetOwner(AActor* Owner);
	FString GetAudioComponentName() const;
	FString GetOwnerName() const;

	uint32 GetWorldID() const { return WorldID; }
	TWeakObjectPtr<UWorld> GetWeakWorld() const { return World; }
	UWorld* GetWorld() const 
	{ 
		check(IsInGameThread()); 
		return World.Get();
	}
	void SetWorld(UWorld* World);

	USoundBase* GetSound() const { return Sound; }
	void SetSound(USoundBase* InSound);

	void SetSoundClass(USoundClass* SoundClass);

	void SetAudioDevice(FAudioDevice* InAudioDevice)
	{
		AudioDevice = InAudioDevice;
	}

	/** Returns whether or not the active sound can be deleted. */
	bool CanDelete() const { return !bAsyncOcclusionPending; }

	FAudioDevice* AudioDevice;

	/** The group of active concurrent sounds that this sound is playing in. */
	FConcurrencyGroupID ConcurrencyGroupID;

	/** The generation of this sound in the concurrency group. */
	int32 ConcurrencyGeneration;

	/** Optional USoundConcurrency to override for the sound. */
	USoundConcurrency* ConcurrencySettings;

private:
	/** Optional SoundClass to override for the sound. */
	USoundClass* SoundClassOverride;

	/** Optional SoundSubmix to override for the sound. */
	USoundSubmix* SoundSubmixOverride;

	/** Optional override the submix sends for the sound. */
	TArray<FSoundSubmixSendInfo> SoundSubmixSendsOverride;

	/** Optional override for the source bus sends for the sound. */
	TArray<FSoundSourceBusSendInfo> SoundSourceBusSendsOverride;

public:
	/** Whether or not the sound has checked if it was occluded already. Used to initialize a sound as occluded and bypassing occlusion interpolation. */
	uint8 bHasCheckedOcclusion:1;

	/** Is this sound allowed to be spatialized? */
	uint8 bAllowSpatialization:1;

	/** Does this sound have attenuation settings specified. */
	uint8 bHasAttenuationSettings:1;

	/** Whether the wave instances should remain active if they're dropped by the prioritization code. Useful for e.g. vehicle sounds that shouldn't cut out. */
	uint8 bShouldRemainActiveIfDropped:1;

	/** Is the audio component currently fading out */
	uint8 bFadingOut:1;

	/** Whether the current component has finished playing */
	uint8 bFinished:1;

	/** Whether or not the active sound is paused. Independently set vs global pause or unpause. */
	uint8 bIsPaused:1;

	/** Whether or not to stop this active sound due to max concurrency */
	uint8 bShouldStopDueToMaxConcurrency:1;

	/** If true, the decision on whether to apply the radio filter has been made. */
	uint8 bRadioFilterSelected:1;

	/** If true, this sound will not be stopped when flushing the audio device. */
	uint8 bApplyRadioFilter:1;

	/** If true, the AudioComponent will be notified when a Wave is started to handle subtitles */
	uint8 bHandleSubtitles:1;

	/** If true, subtitles are being provided for the sound externally, so it still needs to make sure the sound plays to trigger the subtitles. */
	uint8 bHasExternalSubtitles:1;

	/** Whether the Location of the component is well defined */
	uint8 bLocationDefined:1;

	/** If true, this sound will not be stopped when flushing the audio device. */
	uint8 bIgnoreForFlushing:1;

	/** Whether audio effects are applied */
	uint8 bEQFilterApplied:1;

	/** Whether to artificially prioritize the component to play */
	uint8 bAlwaysPlay:1;

	/** Whether or not this sound plays when the game is paused in the UI */
	uint8 bIsUISound:1;

	/** Whether or not this audio component is a music clip */
	uint8 bIsMusic:1;

	/** Whether or not the audio component should be excluded from reverb EQ processing */
	uint8 bReverb:1;

	/** Whether or not this sound class forces sounds to the center channel */
	uint8 bCenterChannelOnly:1;

	/** Whether or not this active sound is a preview sound */
	uint8 bIsPreviewSound:1;

	/** Whether we have queried for the interior settings at least once */
	uint8 bGotInteriorSettings:1;

	/** Whether some part of this sound will want interior sounds to be applied */
	uint8 bApplyInteriorVolumes:1;

#if !(NO_LOGGING || UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** For debugging purposes, output to the log once that a looping sound has been orphaned */
	uint8 bWarnedAboutOrphanedLooping:1;
#endif

	/** Whether or not we have a low-pass filter enabled on this active sound. */
	uint8 bEnableLowPassFilter : 1;

	/** Whether or not this active sound will update play percentage. Based on set delegates on audio component. */
	uint8 bUpdatePlayPercentage:1;

public:
	uint8 UserIndex;

	/** whether we were occluded the last time we checked */
	FThreadSafeBool bIsOccluded;

	/** Whether or not there is an async occlusion trace pending */
	FThreadSafeBool bAsyncOcclusionPending;

	float PlaybackTime;
	float RequestedStartTime;

	float CurrentAdjustVolumeMultiplier;
	float TargetAdjustVolumeMultiplier;
	float TargetAdjustVolumeStopTime;

	float VolumeMultiplier;
	float PitchMultiplier;

	/** The low-pass filter frequency to apply if bEnableLowPassFilter is true. */
	float LowPassFilterFrequency;

	/** The interpolated parameter for the low-pass frequency due to occlusion. */
	FDynamicParameter CurrentOcclusionFilterFrequency;

	/** The interpolated parameter for the volume attenuation due to occlusion. */
	FDynamicParameter CurrentOcclusionVolumeAttenuation;

	/** A volume scale to apply to a sound based on the concurrency count of the active sound when it started. Will reduce volume of new sounds if many sounds are playing in concurrency group. */
	float ConcurrencyVolumeScale;

	/** A volume to apply to a sound based on the concurrency generation and the current generation count. Will reduce volume of older sounds as new sounds play in concurrency group. */
	float ConcurrencyDuckingVolumeScale;

	float SubtitlePriority;

	/** The product of the component priority and the USoundBase priority */
	float Priority;

	/** The amount priority is scaled due to focus */
	float FocusPriorityScale;

	/** The amount distance is scaled due to focus */
	float FocusDistanceScale;

	// The volume used to determine concurrency resolution for "quietest" active sound
	float VolumeConcurrency;

	/** The time in seconds with which to check for occlusion from its closest listener */
	float OcclusionCheckInterval;

	/** Last time we checked for occlusion */
	float LastOcclusionCheckTime;

	/** The max distance this sound will be audible. */
	float MaxDistance;

	FTransform Transform;

	/** Azimuth of the active sound relative to the listener. Used by sound focus. */
	float Azimuth;

	/** Absolute azimuth of the active sound relative to the listener. Used for 3d audio calculations. */
	float AbsoluteAzimuth;

	/** Location last time playback was updated */
	FVector LastLocation;

	FSoundAttenuationSettings AttenuationSettings;

	/** Cache what volume settings we had last time so we don't have to search again if we didn't move */
	FInteriorSettings InteriorSettings;

	uint32 AudioVolumeID;

	// To remember where the volumes are interpolating to and from
	double LastUpdateTime; 
	float SourceInteriorVolume;
	float SourceInteriorLPF;
	float CurrentInteriorVolume;
	float CurrentInteriorLPF;

	TMap<UPTRINT, struct FWaveInstance*> WaveInstances;

	TMap<UPTRINT,uint32> SoundNodeOffsetMap;
	TArray<uint8> SoundNodeData;

	TArray<FAudioComponentParam> InstanceParameters;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FName DebugOriginalSoundName;
#endif

	// Updates the wave instances to be played.
	void UpdateWaveInstances(TArray<FWaveInstance*> &OutWaveInstances, const float DeltaTime);

	/** 
	 * Find an existing waveinstance attached to this audio component (if any)
	 */
	FWaveInstance* FindWaveInstance(const UPTRINT WaveInstanceHash);

	/** 
	 * Check whether to apply the radio filter
	 */
	void ApplyRadioFilter(const struct FSoundParseParameters& ParseParams);

	/** Sets a float instance parameter for the ActiveSound */
	void SetFloatParameter(const FName InName, const float InFloat);

	/** Sets a wave instance parameter for the ActiveSound */
	void SetWaveParameter(const FName InName, class USoundWave* InWave);

	/** Sets a boolean instance parameter for the ActiveSound */
	void SetBoolParameter(const FName InName, const bool InBool);
	
	/** Sets an integer instance parameter for the ActiveSound */
	void SetIntParameter(const FName InName, const int32 InInt);

	/** Sets the audio component parameter on the active sound. Note: this can be set without audio components if they are set when active sound is created. */
	void SetSoundParameter(const FAudioComponentParam& Param);

	/**
	 * Try and find an Instance Parameter with the given name and if we find it return the float value.
	 * @return true if float for parameter was found, otherwise false
	 */
	bool GetFloatParameter(const FName InName, float& OutFloat) const;

	/**
	 *Try and find an Instance Parameter with the given name and if we find it return the USoundWave value.
	 * @return true if USoundWave for parameter was found, otherwise false
	 */
	bool GetWaveParameter(const FName InName, USoundWave*& OutWave) const;

	/**
	 *Try and find an Instance Parameter with the given name and if we find it return the boolean value.
	 * @return true if boolean for parameter was found, otherwise false
	 */
	bool GetBoolParameter(const FName InName, bool& OutBool) const;
	
	/**
	 *Try and find an Instance Parameter with the given name and if we find it return the integer value.
	 * @return true if boolean for parameter was found, otherwise false
	 */
	bool GetIntParameter(const FName InName, int32& OutInt) const;

	void CollectAttenuationShapesForVisualization(TMultiMap<EAttenuationShape::Type, FBaseAttenuationSettings::AttenuationShapeDetails>& ShapeDetailsMap) const;

	/**
	 * Friend archive function used for serialization.
	 */
	friend FArchive& operator<<( FArchive& Ar, FActiveSound* ActiveSound );

	void AddReferencedObjects( FReferenceCollector& Collector );

	/**
	 * Get the sound class to apply on this sound instance
	 */
	USoundClass* GetSoundClass() const;

	/**
	* Get the sound submix to use for this sound instance
	*/
	USoundSubmix* GetSoundSubmix() const;

	/** Gets the sound submix sends to use for this sound instance. */
	void GetSoundSubmixSends(TArray<FSoundSubmixSendInfo>& OutSends) const;

	/** Gets the sound source bus sends to use for this sound instance. */
	void GetSoundSourceBusSends(TArray<FSoundSourceBusSendInfo>& OutSends) const;

	/* Determines which listener is the closest to the sound */
	int32 FindClosestListener( const TArray<struct FListener>& InListeners ) const;
	
	/** Returns the unique ID of the active sound's owner if it exists. Returns 0 if the sound doesn't have an owner. */
	uint32 GetOwnerID() const { return OwnerID; }

	/** Gets the sound concurrency to apply on this active sound instance */
	const FSoundConcurrencySettings* GetSoundConcurrencySettingsToApply() const;

	/** Delegate callback function when an async occlusion trace completes */
	static void OcclusionTraceDone(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum);

	/** Returns the sound concurrency object ID if it exists. If it doesn't exist, returns 0. */
	uint32 GetSoundConcurrencyObjectID() const;

	/** Applies the active sound's attenuation settings to the input parse params using the given listener */
	void ApplyAttenuation(FSoundParseParameters& ParseParams, const FListener& Listener, const FSoundAttenuationSettings* SettingsAttenuationNode = nullptr);

	/** Returns the effective priority of the active sound */
	float GetPriority() const { return Priority * FocusPriorityScale; }

	/** Sets the amount of audio from this active sound to send to the submix. */
	void SetSubmixSend(const FSoundSubmixSendInfo& SubmixSendInfo);

	/** Sets the amount of audio from this active sound to send to the source bus. */
	void SetSourceBusSend(const FSoundSourceBusSendInfo& SourceBusSendInfo);

private:
	
	struct FAsyncTraceDetails
	{
		uint32 AudioDeviceID;
		FActiveSound* ActiveSound;
	};

	static TMap<FTraceHandle, FAsyncTraceDetails> TraceToActiveSoundMap;

	static FTraceDelegate ActiveSoundTraceDelegate;

	/** Cached ptr to the closest listener. So we don't have to do the work to find it twice. */
	const FListener* ClosestListenerPtr;

	/** This is a friend so the audio device can call Stop() on the active sound. */
	friend class FAudioDevice;

	/** Stops the active sound. Can only be called from the owning audio device. */
	void Stop();

	/** Sets the target volume multiplier to achieve over the specified time period */
	void UpdateAdjustVolumeMultiplier(const float DeltaTime);

	/** if OcclusionCheckInterval > 0.0, checks if the sound has become (un)occluded during playback
	 * and calls eventOcclusionChanged() if so
	 * primarily used for gameplay-relevant ambient sounds
	 * CurrentLocation is the location of this component that will be used for playback
	 * @param ListenerLocation location of the closest listener to the sound
	 */
	void CheckOcclusion(const FVector ListenerLocation, const FVector SoundLocation, const FSoundAttenuationSettings* AttenuationSettingsPtr);
	 
	/** Apply the interior settings to the ambient sound as appropriate */
	void HandleInteriorVolumes( const FListener& Listener, struct FSoundParseParameters& ParseParams );

	/** Helper function which retrieves attenuation frequency value for HPF and LPF distance-based filtering. */
	float GetAttenuationFrequency(const FSoundAttenuationSettings* InSettings, const FAttenuationListenerData& ListenerData, const FVector2D& FrequencyRange, const FRuntimeFloatCurve& CustomCurve);

	/** Internal Focus Factor value used to allow smooth interpolation in/out of Focus */
	float InternalFocusFactor;
};
