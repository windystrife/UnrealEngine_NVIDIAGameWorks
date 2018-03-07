// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/VoiceCapture.h"
#include "Interfaces/VoiceCodec.h"

/** Name of default capture device */
#define DEFAULT_DEVICE_NAME TEXT("Default Device")
/** Default voice chat sample rate */
#define DEFAULT_VOICE_SAMPLE_RATE 16000
/** Deprecated value, use DEFAULT_VOICE_SAMPLE_RATE */
#define VOICE_SAMPLE_RATE DEFAULT_VOICE_SAMPLE_RATE
/** Default voice chat number of channels (mono) */
#define DEFAULT_NUM_VOICE_CHANNELS 1

class IVoiceCapture;
class IVoiceEncoder;
class IVoiceDecoder;

/** Logging related to general voice chat flow (muting/registration/etc) */
VOICE_API DECLARE_LOG_CATEGORY_EXTERN(LogVoice, Display, All);
/** Logging related to encoding of local voice packets */
VOICE_API DECLARE_LOG_CATEGORY_EXTERN(LogVoiceEncode, Display, All);
/** Logging related to decoding of remote voice packets */
VOICE_API DECLARE_LOG_CATEGORY_EXTERN(LogVoiceDecode, Display, All);

/** Internal voice capture logging */
DECLARE_LOG_CATEGORY_EXTERN(LogVoiceCapture, Warning, All);

/**
 * Module for Voice capture/compression/decompression implementations
 */
class FVoiceModule : 
	public IModuleInterface, public FSelfRegisteringExec
{

public:

	// FSelfRegisteringExec
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FVoiceModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FVoiceModule>("Voice");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("Voice");
	}

	/**
	 * Instantiates a new voice capture object
	 *
	 * @param DeviceName name of device to capture audio data with, empty for default device
	 * @param SampleRate sampling rate of voice capture
	 * @param NumChannels number of channels to capture
	 *
	 * @return new voice capture object, possibly NULL
	 */
	virtual TSharedPtr<IVoiceCapture> CreateVoiceCapture(const FString& DeviceName = DEFAULT_DEVICE_NAME, int32 SampleRate = DEFAULT_VOICE_SAMPLE_RATE, int32 NumChannels = DEFAULT_NUM_VOICE_CHANNELS);

	/**
	 * Instantiates a new voice encoder object
	 *
	 * @param SampleRate sampling rate of voice capture
	 * @param NumChannels number of channels to capture
	 * @param EncodeHint hint to describe type of audio quality desired
	 *
	 * @return new voice encoder object, possibly NULL
	 */
	virtual TSharedPtr<IVoiceEncoder> CreateVoiceEncoder(int32 SampleRate = DEFAULT_VOICE_SAMPLE_RATE, int32 NumChannels = DEFAULT_NUM_VOICE_CHANNELS, EAudioEncodeHint EncodeHint = EAudioEncodeHint::VoiceEncode_Voice);

	/**
	 * Instantiates a new voice decoder object
	 *
	 * @param SampleRate sampling rate of voice capture
	 * @param NumChannels number of channels to capture
	 *
	 * @return new voice decoder object, possibly NULL
	 */
	virtual TSharedPtr<IVoiceDecoder> CreateVoiceDecoder(int32 SampleRate = DEFAULT_VOICE_SAMPLE_RATE, int32 NumChannels = DEFAULT_NUM_VOICE_CHANNELS);

	/**
	 * @return true if voice is enabled
	 */
	inline bool IsVoiceEnabled() const
	{
		return bEnabled;
	}

private:

	// IModuleInterface

	/**
	 * Called when voice module is loaded
	 * Initialize platform specific parts of voice handling
	 */
	virtual void StartupModule() override;
	
	/**
	 * Called when voice module is unloaded
	 * Shutdown platform specific parts of voice handling
	 */
	virtual void ShutdownModule() override;

	/** Is voice interface enabled */
	bool bEnabled;
};

