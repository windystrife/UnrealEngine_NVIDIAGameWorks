// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Interfaces/VoiceCapture.h"
#include "Interfaces/VoiceCodec.h"

bool InitVoiceCapture()
{
	return false;
}

void ShutdownVoiceCapture()
{
}

IVoiceCapture* CreateVoiceCaptureObject(const FString& DeviceName, int32 SampleRate, int32 NumChannels)
{
	return nullptr; 
}

IVoiceEncoder* CreateVoiceEncoderObject(int32 SampleRate, int32 NumChannels, EAudioEncodeHint EncodeHint)
{
	return nullptr; 
}

IVoiceDecoder* CreateVoiceDecoderObject(int32 SampleRate, int32 NumChannels)
{
	return nullptr; 
}
