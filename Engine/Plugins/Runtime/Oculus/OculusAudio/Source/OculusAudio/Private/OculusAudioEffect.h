// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XAudio2Device.h"
#include "AudioEffect.h"
#include "Runtime/Windows/XAudio2/Private/XAudio2Support.h"
#include "AudioDevice.h"

#include "AllowWindowsPlatformTypes.h"
#include "AllowWindowsPlatformAtomics.h"
THIRD_PARTY_INCLUDES_START
	#include <xapobase.h>
	#include <xapofx.h>
	#include <xaudio2fx.h>
THIRD_PARTY_INCLUDES_END
#include "HideWindowsPlatformAtomics.h"
#include "HideWindowsPlatformTypes.h"

#define AUDIO_HRTF_EFFECT_CLASS_ID __declspec( uuid( "{8E67E588-FFF5-4860-A323-5E89B325D5EF}" ) )


class AUDIO_HRTF_EFFECT_CLASS_ID FXAudio2HRTFEffect : public CXAPOBase
{
public:
	FXAudio2HRTFEffect(uint32 InVoiceId, FAudioDevice* InAudioDevice);
	~FXAudio2HRTFEffect();

	STDMETHOD(LockForProcess)(UINT32 InputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pInputLockedParameters, UINT32 OutputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pOutputLockedParameters);
	STDMETHOD_(void, Process)(UINT32 InputProcessParameterCount, const XAPO_PROCESS_BUFFER_PARAMETERS *pInputProcessParameters, UINT32 OutputProcessParameterCount, XAPO_PROCESS_BUFFER_PARAMETERS *pOutputProcessParameters, BOOL IsEnabled);

	// Override AddRef/Release because we will be doing our own lifetime management without COM
	STDMETHOD_(ULONG, AddRef)() override { return 0; }
	STDMETHOD_(ULONG, Release)() override { return 0; }

private:

	WAVEFORMATEX							WaveFormat;

	/* Which voice index this effect is attached to. */
	uint32									InputFrameCount;
	static XAPO_REGISTRATION_PROPERTIES		Registration;
	uint32									InputChannels;
	uint32									OutputChannels;
	uint32									VoiceId;
	FAudioDevice*							AudioDevice;
	bool									bPassThrough;
};

