// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OculusAudioLegacy.h"

OculusAudioLegacySpatialization::OculusAudioLegacySpatialization()
	: bOvrContextInitialized(false)
	, OvrAudioContext(nullptr)
{
	
}

OculusAudioLegacySpatialization::~OculusAudioLegacySpatialization()
{
}

void OculusAudioLegacySpatialization::Initialize(const FAudioPluginInitializationParams InitializationParams)
{
	check(HRTFEffects.Num() == 0);
	check(Params.Num() == 0);

	for (int32 EffectIndex = 0; EffectIndex < (int32)InitializationParams.NumSources; ++EffectIndex)
	{
		/* Hack: grab main audio device. */
		FXAudio2HRTFEffect* NewHRTFEffect = new FXAudio2HRTFEffect(EffectIndex, InitializationParams.AudioDevicePtr);
		/* End hack. */
		NewHRTFEffect->Initialize(nullptr, 0);
		HRTFEffects.Add(NewHRTFEffect);

		Params.Add(FSpatializationParams());
	}

	if (bOvrContextInitialized)
	{
		return;
	}

	ovrAudioContextConfiguration ContextConfig;
	ContextConfig.acc_Size = sizeof(ovrAudioContextConfiguration);

	// First initialize the Fast algorithm context
	ContextConfig.acc_Provider = ovrAudioSpatializationProvider_OVR_OculusHQ;
	ContextConfig.acc_MaxNumSources = InitializationParams.NumSources;
	ContextConfig.acc_SampleRate = InitializationParams.SampleRate;
	//XAudio2 sets the buffer callback size to a 100th of the sample rate:
	ContextConfig.acc_BufferLength = InitializationParams.SampleRate / 100;

	check(OvrAudioContext == nullptr);
	// Create the OVR Audio Context with a given quality
	ovrResult Result = ovrAudio_CreateContext(&OvrAudioContext, &ContextConfig);
	OVR_AUDIO_CHECK(Result, "Failed to create simple context");

	// Now initialize the high quality algorithm context
	bOvrContextInitialized = true;
}

void OculusAudioLegacySpatialization::Shutdown()
{
	// Release all the effects for the oculus spatialization effect
	for (FXAudio2HRTFEffect* Effect : HRTFEffects)
	{
		if (Effect)
		{
			delete Effect;
		}
	}
	HRTFEffects.Empty();

	// Destroy the contexts if we created them
	if (bOvrContextInitialized)
	{
		if (OvrAudioContext != nullptr)
		{
			ovrAudio_DestroyContext(OvrAudioContext);
			OvrAudioContext = nullptr;
		}

		bOvrContextInitialized = false;
	}
}

bool OculusAudioLegacySpatialization::IsSpatializationEffectInitialized() const
{
	return bOvrContextInitialized;
}

void OculusAudioLegacySpatialization::ProcessSpatializationForVoice(uint32 VoiceIndex, float* InSamples, float* OutSamples, const FVector& Position)
{
	if (OvrAudioContext)
	{
		ProcessAudioInternal(OvrAudioContext, VoiceIndex, InSamples, OutSamples, Position);
	}
}

bool OculusAudioLegacySpatialization::CreateSpatializationEffect(uint32 VoiceId)
{
	// If an effect for this voice has already been created, then leave
	if ((int32)VoiceId >= HRTFEffects.Num())
	{
		return false;
	}
	return true;
}

void* OculusAudioLegacySpatialization::GetSpatializationEffect(uint32 VoiceId)
{
	if ((int32)VoiceId < HRTFEffects.Num())
	{
		return (void*)HRTFEffects[VoiceId];
	}
	return nullptr;
}

void OculusAudioLegacySpatialization::SetSpatializationParameters(uint32 VoiceId, const FSpatializationParams& InParams)
{
	check((int32)VoiceId < Params.Num());

	FScopeLock ScopeLock(&ParamCriticalSection);
	Params[VoiceId] = InParams;
}

void OculusAudioLegacySpatialization::GetSpatializationParameters(uint32 VoiceId, FSpatializationParams& OutParams)
{
	check((int32)VoiceId < Params.Num());

	FScopeLock ScopeLock(&ParamCriticalSection);
	OutParams = Params[VoiceId];
}

void OculusAudioLegacySpatialization::ProcessAudioInternal(ovrAudioContext AudioContext, uint32 VoiceIndex, float* InSamples, float* OutSamples, const FVector& Position)
{
	ovrResult Result = ovrAudio_SetAudioSourceAttenuationMode(AudioContext, VoiceIndex, ovrAudioSourceAttenuationMode_None, 1.0f);
	OVR_AUDIO_CHECK(Result, "Failed to set source attenuation mode");

	// Translate the input position to OVR coordinates
	FVector OvrPosition = ToOVRVector(Position);

	// Set the source position to current audio position
	Result = ovrAudio_SetAudioSourcePos(AudioContext, VoiceIndex, OvrPosition.X, OvrPosition.Y, OvrPosition.Z);
	OVR_AUDIO_CHECK(Result, "Failed to set audio source position");

	// Perform the processing
	uint32 Status;
	Result = ovrAudio_SpatializeMonoSourceInterleaved(AudioContext, VoiceIndex, ovrAudioSpatializationFlag_None, &Status, OutSamples, InSamples);
	OVR_AUDIO_CHECK(Result, "Failed to spatialize mono source interleaved");
}

/** Helper function to convert from UE coords to OVR coords. */
FVector OculusAudioLegacySpatialization::ToOVRVector(const FVector& InVec) const
{
	return FVector(float(InVec.Y), float(-InVec.Z), float(-InVec.X));
}