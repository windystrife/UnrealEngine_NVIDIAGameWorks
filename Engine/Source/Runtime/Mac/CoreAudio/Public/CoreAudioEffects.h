// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CodeAudioEffects.h: Unreal CoreAudio audio effects interface object.
=============================================================================*/

#pragma once

#include "CoreAudioDevice.h"
#include "AudioEffect.h"

#define CORE_AUDIO_LOWPASS_ENABLED 1
#define CORE_AUDIO_REVERB_ENABLED 0
#define CORE_AUDIO_EQ_ENABLED 1
#define CORE_AUDIO_RADIO_ENABLED 1

/** 
 * CoreAudio effects manager
 */
class FCoreAudioEffectsManager : public FAudioEffectsManager
{
public:
	FCoreAudioEffectsManager( FAudioDevice* InDevice );
	~FCoreAudioEffectsManager( void );

	/** 
	 * Calls the platform specific code to set the parameters that define reverb
	 */
	virtual void SetReverbEffectParameters( const FAudioReverbEffect& ReverbEffectParameters );

	/** 
	 * Calls the platform specific code to set the parameters that define EQ
	 */
	virtual void SetEQEffectParameters( const FAudioEQEffect& EQEffectParameters );

	/** 
	 * Calls the platform specific code to set the parameters that define a radio effect.
	 * 
	 * @param	RadioEffectParameters	The new parameters for the radio distortion effect. 
	 */
	virtual void SetRadioEffectParameters( const FAudioRadioEffect& RadioEffectParameters );

private:

	bool bRadioAvailable;
	CFBundleRef RadioBundle;

	/** Console variables to tweak the radio effect output */
	static TConsoleVariableData<float>*	Radio_ChebyshevPowerMultiplier;
	static TConsoleVariableData<float>*	Radio_ChebyshevPower;
	static TConsoleVariableData<float>*	Radio_ChebyshevCubedMultiplier;
	static TConsoleVariableData<float>*	Radio_ChebyshevMultiplier;

	friend class FCoreAudioDevice;
	friend class FCoreAudioSoundSource;
};
