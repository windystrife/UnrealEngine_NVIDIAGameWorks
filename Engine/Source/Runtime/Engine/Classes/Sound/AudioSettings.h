// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/DeveloperSettings.h"
#include "AudioSettings.generated.h"

struct ENGINE_API FAudioPlatformSettings
{
	/** Sample rate to use on the platform for the mixing engine. Higher sample rates will incur more CPU cost. */
	int32 SampleRate;

	/** The amount of audio to compute each callback block. Lower values decrease latency but may increase CPU cost. */
	int32 CallbackBufferFrameSize;

	/** The number of buffers to keep enqueued. More buffers increases latency, but can compensate for variable compute availability in audio callbacks on some platforms. */
	int32 NumBuffers;

	/** The max number of channels to limit for this platform. The max channels used will be the minimum of this value and the global audio quality settings. A value of 0 will not apply a platform channel count max. */
	int32 MaxChannels;

	/** The number of workers to use to compute source audio. Will only use up to the max number of sources. Will evenly divide sources to each source worker. */
	int32 NumSourceWorkers;

	static FAudioPlatformSettings GetPlatformSettings(const TCHAR* PlatformSettingsConfigFile);

	FAudioPlatformSettings()
		: SampleRate(48000)
		, CallbackBufferFrameSize(1024)
		, NumBuffers(2)
		, MaxChannels(0)
		, NumSourceWorkers(0)
	{
	}
};


USTRUCT()
struct ENGINE_API FAudioQualitySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Quality")
	FText DisplayName;

	// The number of audio channels that can be used at once
	// NOTE: Some platforms may cap this value to a lower setting regardless of what the settings request
	UPROPERTY(EditAnywhere, Category="Quality", meta=(ClampMin="1"))
	int32 MaxChannels;

	FAudioQualitySettings()
		: MaxChannels(32)
	{
	}
};

/**
 * Audio settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Audio"))
class ENGINE_API UAudioSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeChainProperty( struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	/** The SoundClass assigned to newly created sounds */
	UPROPERTY(config, EditAnywhere, Category="Audio", meta=(AllowedClasses="SoundClass", DisplayName="Default Sound Class"))
	FSoftObjectPath DefaultSoundClassName;

	/** The SoundConcurrency assigned to newly created sounds */
	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (AllowedClasses = "SoundConcurrency", DisplayName = "Default Sound Concurrency"))
	FSoftObjectPath DefaultSoundConcurrencyName;

	/** The SoundMix to use as base when no other system has specified a Base SoundMix */
	UPROPERTY(config, EditAnywhere, Category="Audio", meta=(AllowedClasses="SoundMix"))
	FSoftObjectPath DefaultBaseSoundMix;
	
	/** Sound class to be used for the VOIP audio component */
	UPROPERTY(config, EditAnywhere, Category="Audio", meta=(AllowedClasses="SoundClass"))
	FSoftObjectPath VoiPSoundClass;

	/** The amount of audio to send to reverb submixes if no reverb send is setup for the source through attenuation settings. Only used in audio mixer. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", AdvancedDisplay, meta = (ClampMin = 0.0, ClampMax = 1.0))
	float DefaultReverbSendLevel;

	UPROPERTY(config, EditAnywhere, Category="Audio", AdvancedDisplay, meta=(ClampMin=0.1,ClampMax=1.5))
	float LowPassFilterResonance;

	/** How many streaming sounds can be played at the same time (if more are played they will be sorted by priority) */
	UPROPERTY(config, EditAnywhere, Category="Audio", meta=(ClampMin=0))
	int32 MaximumConcurrentStreams;

	UPROPERTY(config, EditAnywhere, Category="Quality")
	TArray<FAudioQualitySettings> QualityLevels;

	/** Allows sounds to play at 0 volume. */
	UPROPERTY(config, EditAnywhere, Category = "Quality")
	uint32 bAllowVirtualizedSounds:1;

	/** Disables master EQ effect in the audio DSP graph. */
	UPROPERTY(config, EditAnywhere, Category = "Quality")
	uint32 bDisableMasterEQ : 1;

	/** Disables master reverb effect in the audio DSP graph. */
	UPROPERTY(config, EditAnywhere, Category = "Quality")
	uint32 bDisableMasterReverb : 1;

	/** Enables the surround sound spatialization calculations to include the center channel. */
	UPROPERTY(config, EditAnywhere, Category = "Quality")
	uint32 bAllowCenterChannel3DPanning : 1;

	/**
	 * The format string to use when generating the filename for contexts within dialogue waves. This must generate unique names for your project.
	 * Available format markers:
	 *   * {DialogueGuid} - The GUID of the dialogue wave. Guaranteed to be unique and stable against asset renames.
	 *   * {DialogueHash} - The hash of the dialogue wave. Not guaranteed to be unique or stable against asset renames, however may be unique enough if combined with the dialogue name.
	 *   * {DialogueName} - The name of the dialogue wave. Not guaranteed to be unique or stable against asset renames, however may be unique enough if combined with the dialogue hash.
	 *   * {ContextId}    - The ID of the context. Guaranteed to be unique within its dialogue wave. Not guaranteed to be stable against changes to the context.
	 *   * {ContextIndex} - The index of the context within its parent dialogue wave. Guaranteed to be unique within its dialogue wave. Not guaranteed to be stable against contexts being removed.
	 */
	UPROPERTY(config, EditAnywhere, Category="Dialogue")
	FString DialogueFilenameFormat;

	const FAudioQualitySettings& GetQualityLevelSettings(int32 QualityLevel) const;

	// Sets whether audio mixer is enabled. Set once an audio mixer platform modu le is loaded.
	void SetAudioMixerEnabled(const bool bInAudioMixerEnabled);

	// Returns if the audio mixer is currently enabled
	const bool IsAudioMixerEnabled() const;

	/** Returns the highest value for MaxChannels among all quality levels */
	int32 GetHighestMaxChannels() const;

private:

#if WITH_EDITOR
	TArray<FAudioQualitySettings> CachedQualityLevels;
#endif

	void AddDefaultSettings();

	// Whether or not the audio mixer is loaded/enabled. Used to toggle visibility of editor features.
	bool bIsAudioMixerEnabled;
};
