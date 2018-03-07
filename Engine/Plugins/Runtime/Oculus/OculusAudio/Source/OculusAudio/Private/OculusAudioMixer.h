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
/* OculusAudioSpatializationAudioMixer                                  */
/* This implementation of IAudioSpatialization uses the Oculus Audio    */
/* library to render audio sources with HRTF spatialization.            */
/************************************************************************/
class OculusAudioSpatializationAudioMixer : public IAudioSpatialization
{
public:
	OculusAudioSpatializationAudioMixer();
	~OculusAudioSpatializationAudioMixer();

	//~ Begin IAudioSpatialization 
	virtual void Initialize(const FAudioPluginInitializationParams InitializationParams) override;
	virtual void Shutdown() override;
	virtual bool IsSpatializationEffectInitialized() const override;
	virtual void SetSpatializationParameters(uint32 VoiceId, const FSpatializationParams& InParams) override;
	virtual void ProcessAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData) override;
	//~ End IAudioSpatialization 

private:
	// Helper function to convert from UE coords to OVR coords.
	FORCEINLINE FVector ToOVRVector(const FVector& InVec) const
	{
		return FVector(InVec.Y, -InVec.Z, -InVec.X);
	}

	// Whether or not the OVR audio context is initialized. We defer initialization until the first audio callback.
	bool bOvrContextInitialized;

	TArray<FSpatializationParams> Params;

	// The OVR Audio context initialized to "Fast" algorithm.
	ovrAudioContext OvrAudioContext;
};