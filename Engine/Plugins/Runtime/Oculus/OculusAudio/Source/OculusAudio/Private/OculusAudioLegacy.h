// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OVR_Audio.h"
#include "XAudio2Device.h"
#include "AudioEffect.h"
#include "XAudio2Effects.h"
#include "OculusAudioEffect.h"
#include "Runtime/Windows/XAudio2/Private/XAudio2Support.h"
#include "Misc/Paths.h"
#include "OculusAudioDllManager.h"

/************************************************************************/
/* OculusAudioLegacySpatialization                                      */
/* This spatialization plugin is used in the non-audiomixer engine,     */
/* XAudio2's HRTFEffect plugin system directly.                         */
/************************************************************************/
class OculusAudioLegacySpatialization : public IAudioSpatialization
{
public:
	OculusAudioLegacySpatialization();
	~OculusAudioLegacySpatialization();

	/** Begin IAudioSpatialization implementation */
	virtual void Initialize(const FAudioPluginInitializationParams InitializationParams) override;
	virtual void Shutdown() override;

	virtual bool IsSpatializationEffectInitialized() const override;
	virtual void ProcessSpatializationForVoice(uint32 VoiceIndex, float* InSamples, float* OutSamples, const FVector& Position) override;
	virtual bool CreateSpatializationEffect(uint32 VoiceId) override;
	virtual void* GetSpatializationEffect(uint32 VoiceId) override;
	virtual void SetSpatializationParameters(uint32 VoiceId, const FSpatializationParams& InParams) override;
	virtual void GetSpatializationParameters(uint32 VoiceId, FSpatializationParams& OutParams) override;
	/** End IAudioSpatialization implementation */

private:
	void ProcessAudioInternal(ovrAudioContext AudioContext, uint32 VoiceIndex, float* InSamples, float* OutSamples, const FVector& Position);
	FVector ToOVRVector(const FVector& InVec) const;
	
private:
	/* Whether or not the OVR audio context is initialized. We defer initialization until the first audio callback.*/
	bool bOvrContextInitialized;

	/** The OVR Audio context initialized to "Fast" algorithm. */
	ovrAudioContext OvrAudioContext;

	/** Xaudio2 effects for the oculus plugin */
	TArray<class FXAudio2HRTFEffect*> HRTFEffects;
	TArray<FSpatializationParams> Params;

	FCriticalSection ParamCriticalSection;
};