// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VoiceModule.h"
#include "Misc/ConfigCacheIni.h"
#include "VoicePrivate.h"

IMPLEMENT_MODULE(FVoiceModule, Voice);

DEFINE_LOG_CATEGORY(LogVoice);
DEFINE_LOG_CATEGORY(LogVoiceEncode);
DEFINE_LOG_CATEGORY(LogVoiceDecode);
DEFINE_LOG_CATEGORY(LogVoiceCapture);

DEFINE_STAT(STAT_Voice_Encoding);
DEFINE_STAT(STAT_Voice_Decoding);
DEFINE_STAT(STAT_Encode_SampleRate);
DEFINE_STAT(STAT_Encode_NumChannels);
DEFINE_STAT(STAT_Encode_Bitrate);
DEFINE_STAT(STAT_Encode_CompressionRatio);
DEFINE_STAT(STAT_Encode_OutSize);
DEFINE_STAT(STAT_Decode_SampleRate);
DEFINE_STAT(STAT_Decode_NumChannels);
DEFINE_STAT(STAT_Decode_CompressionRatio);
DEFINE_STAT(STAT_Decode_OutSize);

#if PLATFORM_SUPPORTS_VOICE_CAPTURE
/** Implement these functions per platform to create the voice objects */
extern bool InitVoiceCapture();
extern void ShutdownVoiceCapture();
extern IVoiceCapture* CreateVoiceCaptureObject(const FString& DeviceName, int32 SampleRate, int32 NumChannels);
extern IVoiceEncoder* CreateVoiceEncoderObject(int32 SampleRate, int32 NumChannels, EAudioEncodeHint EncodeHint);
extern IVoiceDecoder* CreateVoiceDecoderObject(int32 SampleRate, int32 NumChannels);
#endif

void FVoiceModule::StartupModule()
{	
	bEnabled = false;
	if (!GConfig->GetBool(TEXT("Voice"), TEXT("bEnabled"), bEnabled, GEngineIni))
	{
		UE_LOG(LogVoice, Warning, TEXT("Missing bEnabled key in Voice of DefaultEngine.ini"));
	}

#if PLATFORM_SUPPORTS_VOICE_CAPTURE
	if (bEnabled)
	{
		if (!InitVoiceCapture())
		{
			UE_LOG(LogVoice, Warning, TEXT("Failed to initialize voice capture module"));
			ShutdownVoiceCapture();
		}
	}
#endif
}

void FVoiceModule::ShutdownModule()
{
#if PLATFORM_SUPPORTS_VOICE_CAPTURE
	if (bEnabled)
	{
		ShutdownVoiceCapture();
	}
#endif
}

bool FVoiceModule::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Ignore any execs that don't start with Voice
	if (FParse::Command(&Cmd, TEXT("Voice")))
	{
		return false;
	}

	return false;
}

TSharedPtr<IVoiceCapture> FVoiceModule::CreateVoiceCapture(const FString& DeviceName, int32 SampleRate, int32 NumChannels)
{
#if PLATFORM_SUPPORTS_VOICE_CAPTURE
	if (bEnabled)
	{
		// Create the platform specific instance
		return TSharedPtr<IVoiceCapture>(CreateVoiceCaptureObject(DeviceName, SampleRate, NumChannels));
	}
#endif
	return nullptr;
}

TSharedPtr<IVoiceEncoder> FVoiceModule::CreateVoiceEncoder(int32 SampleRate, int32 NumChannels, EAudioEncodeHint EncodeHint)
{
#if PLATFORM_SUPPORTS_VOICE_CAPTURE
	if (bEnabled)
	{
		// Create the platform specific instance
		return TSharedPtr<IVoiceEncoder>(CreateVoiceEncoderObject(SampleRate, NumChannels, EncodeHint));
	}
#endif

	return nullptr;
}

TSharedPtr<IVoiceDecoder> FVoiceModule::CreateVoiceDecoder(int32 SampleRate, int32 NumChannels)
{
#if PLATFORM_SUPPORTS_VOICE_CAPTURE
	if (bEnabled)
	{
		// Create the platform specific instance
		return TSharedPtr<IVoiceDecoder>(CreateVoiceDecoderObject(SampleRate, NumChannels));
	}
#endif

	return nullptr;
}


