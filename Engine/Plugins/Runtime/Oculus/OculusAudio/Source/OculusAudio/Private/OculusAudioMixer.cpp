// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OculusAudioMixer.h"

OculusAudioSpatializationAudioMixer::OculusAudioSpatializationAudioMixer()
	: bOvrContextInitialized(false)
	, OvrAudioContext(nullptr)
{
}

OculusAudioSpatializationAudioMixer::~OculusAudioSpatializationAudioMixer()
{
	if (bOvrContextInitialized)
	{
		Shutdown();
	}
}

void OculusAudioSpatializationAudioMixer::Initialize(const FAudioPluginInitializationParams InitializationParams)
{
	if (bOvrContextInitialized)
	{
		return;
	}

	Params.AddDefaulted(InitializationParams.NumSources);

	ovrAudioContextConfiguration ContextConfig;
	ContextConfig.acc_Size = sizeof(ovrAudioContextConfiguration);

	// First initialize the Fast algorithm context
	ContextConfig.acc_Provider = ovrAudioSpatializationProvider_OVR_OculusHQ;
	ContextConfig.acc_MaxNumSources = InitializationParams.NumSources;
	ContextConfig.acc_SampleRate = InitializationParams.SampleRate;
	ContextConfig.acc_BufferLength = InitializationParams.BufferLength;

	check(OvrAudioContext == nullptr);
	// Create the OVR Audio Context with a given quality
	ovrResult Result = ovrAudio_CreateContext(&OvrAudioContext, &ContextConfig);
	OVR_AUDIO_CHECK(Result, "Failed to create simple context");

	for (int32 SourceId = 0; SourceId < (int32)InitializationParams.NumSources; ++SourceId)
	{
		Result = ovrAudio_SetAudioSourceAttenuationMode(OvrAudioContext, SourceId, ovrAudioSourceAttenuationMode_None, 1.0f);
		OVR_AUDIO_CHECK(Result, "Failed to set source attenuation mode");
	}

	// Now initialize the high quality algorithm context
	bOvrContextInitialized = true;
}

void OculusAudioSpatializationAudioMixer::Shutdown()
{
	ovrAudio_DestroyContext(OvrAudioContext);
	bOvrContextInitialized = false;
}

bool OculusAudioSpatializationAudioMixer::IsSpatializationEffectInitialized() const
{
	return bOvrContextInitialized;
}

void OculusAudioSpatializationAudioMixer::SetSpatializationParameters(uint32 VoiceId, const FSpatializationParams& InParams)
{
	Params[VoiceId] = InParams;
}

void OculusAudioSpatializationAudioMixer::ProcessAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData)
{
	if (InputData.SpatializationParams && OvrAudioContext)
	{
		Params[InputData.SourceId] = *InputData.SpatializationParams;

		// Translate the input position to OVR coordinates
		FVector OvrPosition = ToOVRVector(Params[InputData.SourceId].EmitterPosition);

		// Set the source position to current audio position
		ovrResult Result = ovrAudio_SetAudioSourcePos(OvrAudioContext, InputData.SourceId, OvrPosition.X, OvrPosition.Y, OvrPosition.Z);
		OVR_AUDIO_CHECK(Result, "Failed to set audio source position");

		// Perform the processing
		uint32 Status;
		Result = ovrAudio_SpatializeMonoSourceInterleaved(OvrAudioContext, InputData.SourceId, ovrAudioSpatializationFlag_None, &Status, OutputData.AudioBuffer.GetData(), InputData.AudioBuffer->GetData());
		OVR_AUDIO_CHECK(Result, "Failed to spatialize mono source interleaved");
	}
}