// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"
#include "IAudioExtensionPlugin.generated.h"

class FAudioDevice;

enum class EAudioPlatform : uint8
{
	Windows,
	Mac,
	Linux,
	IOS,
	Android,
	XboxOne,
	Playstation4,
	Switch,
	HTML5,
	Unknown
};

/**
* Enumeration of audio plugin types
*
*/
enum class EAudioPlugin : uint8
{
	SPATIALIZATION = 0,
	REVERB = 1,
	OCCLUSION = 2
};

class IAudioSpatialization;
class IAudioOcclusion;
class IAudioReverb;
class IAudioPluginListener;

typedef TSharedPtr<IAudioSpatialization, ESPMode::ThreadSafe> TAudioSpatializationPtr;
typedef TSharedPtr<IAudioOcclusion, ESPMode::ThreadSafe> TAudioOcclusionPtr;
typedef TSharedPtr<IAudioReverb, ESPMode::ThreadSafe> TAudioReverbPtr;
typedef TSharedPtr<IAudioPluginListener, ESPMode::ThreadSafe> TAudioPluginListenerPtr;

/**
* FSpatializationParams
* Struct for retrieving parameters needed for computing spatialization and occlusion plugins.
*/
struct FSpatializationParams
{
	/** The listener position (is likely at the origin). */
	FVector ListenerPosition;

	/** The listener orientation. */
	FVector ListenerOrientation;

	/** The emitter position relative to listener. */
	FVector EmitterPosition;

	/** The emitter world position. */
	FVector EmitterWorldPosition;

	/** The emitter world rotation. */
	FQuat EmitterWorldRotation;

	/** The left channel position. */
	FVector LeftChannelPosition;

	/** The right channel position. */
	FVector RightChannelPosition;

	/** The distance between listener and emitter. */
	float Distance;

	/** The normalized omni radius, or the radius that will blend a sound to non-3d */
	float NormalizedOmniRadius;

	FSpatializationParams()
		: ListenerPosition(FVector::ZeroVector)
		, ListenerOrientation(FVector::ZeroVector)
		, EmitterPosition(FVector::ZeroVector)
		, EmitterWorldPosition(FVector::ZeroVector)
		, EmitterWorldRotation(FQuat::Identity)
		, LeftChannelPosition(FVector::ZeroVector)
		, RightChannelPosition(FVector::ZeroVector)
		, Distance(0.0f)
		, NormalizedOmniRadius(0.0f)
	{}
};

struct FAudioPluginInitializationParams
{
	//Maximum number of sources that can play simultaneously.
	uint32 NumSources;

	//Number of output channels.
	uint32 NumOutputChannels;

	//Sample rate.
	uint32 SampleRate;

	//Buffer length used for each callback.
	uint32 BufferLength;

	//Pointer to audio device owning this audio plugin.
	//IMPORTANT: This will be deprecated once the AudioMixer
	//           is taken out of the experimental branch.
	FAudioDevice* AudioDevicePtr;

	FAudioPluginInitializationParams()
		: NumSources(0)
		, NumOutputChannels(0)
		, SampleRate(0)
		, BufferLength(0)
		, AudioDevicePtr(nullptr)
	{}

};

struct FAudioPluginSourceInputData
{
	// The ID of the source voice
	int32 SourceId;

	// The ID of the audio component associated with the wave instance.
	int32 AudioComponentId;

	// The audio input buffer
	TArray<float>* AudioBuffer;

	// Number of channels of the source audio buffer.
	int32 NumChannels;

	// Spatialization parameters.
	const FSpatializationParams* SpatializationParams;
};

struct FAudioPluginSourceOutputData
{
	// The audio ouput buffer
	TArray<float> AudioBuffer;
};

/** This is a class which should be overridden to provide users with settings to use for individual sounds */
UCLASS(config = Engine, abstract, editinlinenew, BlueprintType)
class ENGINE_API USpatializationPluginSourceSettingsBase : public UObject
{
	GENERATED_BODY()
};


/************************************************************************/
/* IAudioPluginFactory                                             */
/* This interface is inherited by spatialization, reverb and occlusion  */
/* plugins to describe specifics of a plugin such as platform support,  */
/* and display names.                                                   */
/************************************************************************/
class IAudioPluginFactory
{
public:
	/*
	* Returns human-readable string representing the display name of this plugin.
	* This is the name that will be used in settings and .ini files.
	* If multiple IAudioPlugin implementations are found that return identical strings here,
	* The first one of these loaded will be used.
	*
	* @return FString of the display name of this plugin.
	*/
	virtual FString GetDisplayName()
	{
		static FString DisplayName = FString(TEXT("Generic Audio Plugin"));
		return DisplayName;
	}

	/*
	* Returns whether this plugin supports use on the specified platform.
	* @param Platform an enumerated platform (i.e. Windows, Playstation4, etc.)
	* @return true if this plugin supports use on Platform, false otherwise.
	*/
	virtual bool SupportsPlatform(EAudioPlatform Platform) = 0;

	/*
	* Returns whether this plugin sends audio to an external renderer.
	* if this returns true, the audio engine will not mix the result of the audio process callback
	* from the plugin into the audio output.
	*
	* @return true only if the plugin will handle sending audio to the DAC itself.
	*/
	virtual bool IsExternalSend()
	{
		return false;
	}
};

/************************************************************************/
/* IAudioSpatializationFactory                                          */
/* Implement this modular feature to make your Spatialialization plugin */
/* visible to the engine.                                               */
/************************************************************************/
class IAudioSpatializationFactory : public IAudioPluginFactory, public IModularFeature
{
public:
	/** Virtual destructor */
	virtual ~IAudioSpatializationFactory()
	{
	}

	// IModularFeature
	static FName GetModularFeatureName()
	{
		static FName AudioExtFeatureName = FName(TEXT("AudioSpatializationPlugin"));
		return AudioExtFeatureName;
	}

	/* Begin IAudioPluginWithMetadata implementation */
	virtual FString GetDisplayName() override
	{
		static FString DisplayName = FString(TEXT("Generic Audio Spatialization Plugin"));
		return DisplayName;
	}
	/* End IAudioPluginWithMetadata implementation */

	/**
	* @return a new instance of your spatialization plugin, owned by a shared pointer.
	*/
	virtual TAudioSpatializationPtr CreateNewSpatializationPlugin(FAudioDevice* OwningDevice) = 0;

	/*
	* @return true if this plugin uses a custom setting.
	*/
	virtual bool HasCustomSpatializationSetting() const { return false; }
};

/**
* IAudioSpatialization
*
* This class represents instances of a plugin that will process spatialization for a stream of audio.
* Currently used to process a mono-stream through an HRTF spatialization algorithm into a stereo stream.
* This algorithm contains an audio effect assigned to every VoiceId (playing sound instance). It assumes
* the effect is updated in the audio engine update loop with new position information.
*
*/
class IAudioSpatialization 
{
public:
	/** Virtual destructor */
	virtual ~IAudioSpatialization()
	{
	}

	/**
	* Shuts down the audio plugin.
	*
	*/
	virtual void Shutdown()
	{
	}

	virtual void OnDeviceShutdown(FAudioDevice* AudioDevice)
	{
	}

	/** DEPRECATED: sets the spatialization effect parameters. */
	virtual void SetSpatializationParameters(uint32 SourceId, const FSpatializationParams& Params)
	{
	}

	/** DEPRECATED: Gets the spatialization effect parameters. */
	virtual void GetSpatializationParameters(uint32 SourceId, FSpatializationParams& OutParams)
	{
	}
	
	/** DEPRECATED: Initializes the spatialization effect with the given buffer length. */
	virtual void InitializeSpatializationEffect(uint32 BufferLength)
	{
	}

	/** DEPRECATED: Uses the given HRTF algorithm to spatialize a mono audio stream. */
	virtual void ProcessSpatializationForVoice(uint32 SourceId, float* InSamples, float* OutSamples, const FVector& Position)
	{
	}

	/** DEPRECATED: Uses the given HRTF algorithm to spatialize a mono audio stream, assumes the parameters have already been set before processing. */
	virtual void ProcessSpatializationForVoice(uint32 SourceId, float* InSamples, float* OutSamples)
	{
	}

	/** Called when a source is assigned to a voice. */
	virtual void OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, USpatializationPluginSourceSettingsBase* InSettings)
	{
	}

	/** Called when a source is done playing and is released. */
	virtual void OnReleaseSource(const uint32 SourceId)
	{
	}

	/** Processes audio with the given input and output data structs.*/
	virtual void ProcessAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData)
	{
	}

	/** Called when all sources have finished processing. */
	virtual void OnAllSourcesProcessed()
	{
	}

	/** Returns whether or not the spatialization effect has been initialized */
	virtual bool IsSpatializationEffectInitialized() const
	{
		return false;
	}

	/** Initializes the spatialization plugin with the given buffer length. */
	virtual void Initialize(const FAudioPluginInitializationParams InitializationParams)
	{
	}

	/** Creates an audio spatialization effect. */
	virtual bool CreateSpatializationEffect(uint32 SourceId)
	{
		return true;
	}

	/**	Returns the spatialization effect for the given voice id. */
	virtual void* GetSpatializationEffect(uint32 SourceId)
	{
		return nullptr;
	}
};

/** This is a class which should be overridden to provide users with settings to use for individual sounds */
UCLASS(config = Engine, abstract, editinlinenew, BlueprintType)
class ENGINE_API UOcclusionPluginSourceSettingsBase : public UObject
{
	GENERATED_BODY()
};

/************************************************************************/
/* IAudioOcclusionFactory                                               */
/*                                                                      */
/************************************************************************/
class IAudioOcclusionFactory : public IAudioPluginFactory, public IModularFeature
{
public:
	/** Virtual destructor */
	virtual ~IAudioOcclusionFactory()
	{
	}

	// IModularFeature
	static FName GetModularFeatureName()
	{
		static FName AudioExtFeatureName = FName(TEXT("AudioOcclusionPlugin"));
		return AudioExtFeatureName;
	}

	/* Begin IAudioPluginWithMetadata implementation */
	virtual FString GetDisplayName() override
	{
		static FString DisplayName = FString(TEXT("Generic Audio Occlusion Plugin"));
		return DisplayName;
	}
	/* End IAudioPluginWithMetadata implementation */

	virtual TAudioOcclusionPtr CreateNewOcclusionPlugin(FAudioDevice* OwningDevice) = 0;

	virtual bool HasCustomOcclusionSetting() const { return false; }
};

class IAudioOcclusion
{
public:
	/** Virtual destructor */
	virtual ~IAudioOcclusion()
	{
	}

	/** Initialize the occlusion plugin with the same rate and number of sources. */
	virtual void Initialize(const FAudioPluginInitializationParams InitializationParams)
	{
	}

	/** Called when a source is assigned to a voice. */
	virtual void OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, const uint32 NumChannels, UOcclusionPluginSourceSettingsBase* InSettings)
	{
	}

	/** Called when a source is done playing and is released. */
	virtual void OnReleaseSource(const uint32 SourceId)
	{
	}

	/** Processes audio with the given input and output data structs.*/
	virtual void ProcessAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData)
	{
	}
};

/** This is a class which should be overridden to provide users with settings to use for individual sounds */
UCLASS(config = Engine, abstract, editinlinenew, BlueprintType)
class ENGINE_API UReverbPluginSourceSettingsBase : public UObject
{
	GENERATED_BODY()
};
 
class IAudioReverbFactory : public IAudioPluginFactory, public IModularFeature
{
public:
	/** Virtual destructor */
	virtual ~IAudioReverbFactory()
	{
	}

	// IModularFeature
	static FName GetModularFeatureName()
	{
		static FName AudioExtFeatureName = FName(TEXT("AudioReverbPlugin"));
		return AudioExtFeatureName;
	}

	/* Begin IAudioPluginWithMetadata implementation */
	virtual FString GetDisplayName() override
	{
		static FString DisplayName = FString(TEXT("Generic Audio Reverb Plugin"));
		return DisplayName;
	}
	/* End IAudioPluginWithMetadata implementation */

	virtual TAudioReverbPtr CreateNewReverbPlugin(FAudioDevice* OwningDevice) = 0;

	virtual bool HasCustomReverbSetting() const { return false; }
};

class IAudioReverb
{
public:
	/** Virtual destructor */
	virtual ~IAudioReverb()
	{
	}

	/** Initialize the reverb plugin with the same rate and number of sources. */
	virtual void Initialize(const FAudioPluginInitializationParams InitializationParams)
	{
	}

	/**
	* Shuts down the audio plugin.
	*
	*/
	virtual void Shutdown()
	{
	}

	virtual void OnDeviceShutdown(FAudioDevice* AudioDevice)
	{
	}

	/** Called when a source is assigned to a voice. */
	virtual void OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, const uint32 NumChannels, UReverbPluginSourceSettingsBase* InSettings) = 0;

	/** Called when a source is done playing and is released. */
	virtual void OnReleaseSource(const uint32 SourceId) = 0;

	virtual class FSoundEffectSubmix* GetEffectSubmix(class USoundSubmix* Submix) = 0;

	/** Processes audio with the given input and output data structs.*/
	virtual void ProcessSourceAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData)
	{
	}
};

/************************************************************************/
/* IAudioPluginListener                                                 */
/* Implementations of this interface can receive updates about the      */
/* audio listener's position in the game world, as well as other data.  */
/* to use this, register a ListenerObserver to an audio device using    */
/* FAudioDevice::RegisterPluginListener().                              */
/************************************************************************/
class IAudioPluginListener
{
public:
	virtual ~IAudioPluginListener()
	{
	}

	virtual void OnDeviceShutdown(FAudioDevice* AudioDevice)
	{
	}

	//This function is called when a game world initializes a listener with an audio device this
	//IAudioPluginListener is registered to. Please note that it is possible to miss this event
	//if you register this IAudioPluginListener after the listener is initialized.
	virtual void OnListenerInitialize(FAudioDevice* AudioDevice, UWorld* ListenerWorld)
	{
	}

	// Called when the listener is updated on the given audio device.
	virtual void OnListenerUpdated(FAudioDevice* AudioDevice, const int32 ViewportIndex, const FTransform& ListenerTransform, const float InDeltaSeconds)
	{
	}

	//Called when the listener is shutdown.
	virtual void OnListenerShutdown(FAudioDevice* AudioDevice)
	{
	}
};