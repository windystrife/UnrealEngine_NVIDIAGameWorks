// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "AudioThread.h"
#include "Sound/SoundEffectSource.h"

class FAudioDevice;
class FReferenceCollector;
class FSoundBuffer;
class IAudioDeviceModule;
class UAudioComponent;
class USoundClass;
class USoundMix;
class USoundSubmix;
class USoundWave;
struct FSourceEffectChainEntry;

/**
* Class for managing multiple audio devices.
*/
class ENGINE_API FAudioDeviceManager
{
public:

	/**
	* Constructor
	*/
	FAudioDeviceManager();

	/**
	* Destructor
	*/
	~FAudioDeviceManager();

	/**
	* Registers the audio device module to the audio device manager. The audio device
	* module is responsible for creating platform-dependent audio devices.
	*/
	void RegisterAudioDeviceModule(IAudioDeviceModule* AudioDeviceModuleInput);

	struct FCreateAudioDeviceResults
	{
		uint32 Handle;
		uint8  bNewDevice : 1;
		FAudioDevice* AudioDevice;

		FCreateAudioDeviceResults();
	};


	/**
	* Creates and audio device instance internally and returns a
	* handle to the audio device. Returns true on success.
	*/
	bool CreateAudioDevice(bool bCreateNewDevice, FCreateAudioDeviceResults& OutResults);

	/**
	* Returns whether the audio device handle is valid (i.e. points to
	* an actual audio device instance)
	*/
	bool IsValidAudioDeviceHandle(uint32 Handle) const;

	/**
	* Shutsdown the audio device associated with the handle. The handle
	* will become invalid after the audio device is shut down.
	*/
	bool ShutdownAudioDevice(uint32 Handle);

	/**
	* Shuts down all active audio devices
	*/
	bool ShutdownAllAudioDevices();

	/**
	* Returns a ptr to the audio device associated with the handle. If the
	* handle is invalid then a NULL device ptr will be returned.
	*/
	FAudioDevice* GetAudioDevice(uint32 Handle);

	/**
	* Returns a ptr to the active audio device. If there is no active 
	* device then it will return the main audio device.
	*/
	FAudioDevice* GetActiveAudioDevice();

	/** Returns the current number of active audio devices. */
	uint8 GetNumActiveAudioDevices() const;

	/** Returns the number of worlds (e.g. PIE viewports) using the main audio device. */
	uint8 GetNumMainAudioDeviceWorlds() const;

	/** Updates all active audio devices */
	void UpdateActiveAudioDevices(bool bGameTicking);

	/** Tracks objects in the active audio devices. */
	void AddReferencedObjects(FReferenceCollector& Collector);

	/** Stops sounds using the given resource on all audio devices. */
	void StopSoundsUsingResource(class USoundWave* InSoundWave, TArray<UAudioComponent*>* StoppedComponents = nullptr);

	/** Registers the Sound Class for all active devices. */
	void RegisterSoundClass(USoundClass* SoundClass);

	/** Unregisters the Sound Class for all active devices. */
	void UnregisterSoundClass(USoundClass* SoundClass);

	/** Initializes the sound class for all active devices. */
	void InitSoundClasses();

	/** Registers the Sound Mix for all active devices. */
	void RegisterSoundSubmix(USoundSubmix* SoundSubmix);

	/** Registers the Sound Mix for all active devices. */
	void UnregisterSoundSubmix(USoundSubmix* SoundSubmix);

	/** Initializes the sound mixes for all active devices. */
	void InitSoundSubmixes();

	/** Initialize all sound effect presets. */
	void InitSoundEffectPresets();

	/** Updates source effect chain on all sources currently using the source effect chain. */
	void UpdateSourceEffectChain(const uint32 SourceEffectChainId, const TArray<FSourceEffectChainEntry>& SourceEffectChain, const bool bPlayEffectChainTails);

	/** Sets which audio device is the active audio device. */
	void SetActiveDevice(uint32 InAudioDeviceHandle);

	/** Sets an audio device to be solo'd */
	void SetSoloDevice(uint32 InAudioDeviceHandle);

	/** Links up the resource data indices for looking up and cleaning up. */
	void TrackResource(USoundWave* SoundWave, FSoundBuffer* Buffer);

	/** Frees the given sound wave resource from the device manager */
	void FreeResource(USoundWave* SoundWave);

	/** Frees the sound buffer from the device manager. */
	void FreeBufferResource(FSoundBuffer* SoundBuffer);

	/** Stops using the given sound buffer. Called before freeing the buffer */
	void StopSourcesUsingBuffer(FSoundBuffer* Buffer);

	/** Retrieves the sound buffer for the given resource id */
	FSoundBuffer* GetSoundBufferForResourceID(uint32 ResourceID);

	/** Removes the sound buffer for the given resource id */
	void RemoveSoundBufferForResourceID(uint32 ResourceID);

	/** Removes sound mix from all audio devices */
	void RemoveSoundMix(USoundMix* SoundMix);

	/** Toggles playing audio for all active PIE sessions (and all devices). */
	void TogglePlayAllDeviceAudio();

	/** Gets whether or not all devices should play their audio. */
	bool IsPlayAllDeviceAudio() const { return bPlayAllDeviceAudio; }

	/** Is debug visualization of 3d sounds enabled */
	bool IsVisualizeDebug3dEnabled() const { return bVisualize3dDebug; }

	/** Toggles 3d visualization of 3d sounds on/off */
	void ToggleVisualize3dDebug();

	/** Toggles the given debug stat bitmask for all current audio devices. */
	void ToggleDebugStat(const uint8 StatBitMask);

	/** Debug solos the given sound class name. Sounds that play with this sound class will be solo'd */
	void SetDebugSoloSoundClass(const TCHAR* SoundClassName);

	/** Debug solos the given sound name. Sounds that play with this name will be solo'd */
	void SetDebugSoloSoundWave(const TCHAR* SoundClassName);

	void SetDebugSoloSoundCue(const TCHAR* SoundClassName);

	/** Enables debug logging and soloing of sounds that substring match the given sound name. Only works if built with AUDIO_MIXER_ENABLE_DEBUG_MODE is turned on. */
	void SetAudioMixerDebugSound(const TCHAR* SoundName);

	/** Gets the solo sound class used for debugging sounds */
	const FString& GetDebugSoloSoundClass() const;

	/** Gets the solo sound name used for debugging sound waves */
	const FString& GetDebugSoloSoundWave() const;

	/** Gets the solo sound name used for debugging sound cues */
	const FString& GetDebugSoloSoundCue() const;

	/** Gets the audio mixer debug sound name. Returns emptry string if AUDIO_MIXER_ENABLE_DEBUG_MODE is not enabled. */
	const FString& GetAudioMixerDebugSoundName() const;

public:

	/** Array of all created buffers */
	TArray<FSoundBuffer*>			Buffers;

	/** Look up associating a USoundWave's resource ID with sound buffers	*/
	TMap<int32, FSoundBuffer*>	WaveBufferMap;

	/** Returns all the audio devices managed by device manager. */
	TArray<FAudioDevice*>& GetAudioDevices() { return Devices; }

private:

	/** Struct which contains debug names for run-time debugging of sounds. */
	struct FDebugNames
	{
		FString DebugSoloSoundClass;
		FString DebugSoloSoundWave;
		FString DebugSoloSoundCue;
		FString DebugAudioMixerSoundName;
	};

	/** Call back for garbage collector, ensures no processing is happening on the thread before collecting resources */
	void OnPreGarbageCollect();

	/** Returns index of the given handle */
	uint32 GetIndex(uint32 Handle) const;

	/** Returns the generation of the given handle */
	uint32 GetGeneration(uint32 Handle) const;

	/** Creates a handle given the index and a generation value. */
	uint32 CreateHandle(uint32 DeviceIndex, uint8 Generation);

	/** Array for generation counts of audio devices in indices */
	TArray<uint8> Generations;

	/** Audio device module which creates audio devices. */
	IAudioDeviceModule* AudioDeviceModule;

	/** Count of the number of free slots in the audio device array. */
	uint32 FreeIndicesSize;

	/** Number of actively created audio device instances. */
	uint8 NumActiveAudioDevices;

	/** Number of worlds using the main audio device instance. */
	uint8 NumWorldsUsingMainAudioDevice;

	/** Queue for free indices */
	TQueue<uint32> FreeIndices;

	/**
	* Array of audio device pointers. If the device has been free, then
	* the device ptr at the array index will be null.
	*/
	TArray<FAudioDevice* > Devices;

	/** Next resource ID to assign out to a wave/buffer */
	int32 NextResourceID;

	/** Which audio device is solo'd */
	uint32 SoloDeviceHandle;

	/** Which audio device is currently active */
	uint32 ActiveAudioDeviceHandle;

	/** Instance of the debug names struct. */
	FDebugNames DebugNames;

	/** Whether or not to play all audio in all active audio devices. */
	bool bPlayAllDeviceAudio;

	/** Whether or not 3d debug visualization is enabled. */
	bool bVisualize3dDebug;

	/** Audio Fence to ensure that we don't allow the audio thread to drift never endingly behind. */
	FAudioCommandFence SyncFence;
};



