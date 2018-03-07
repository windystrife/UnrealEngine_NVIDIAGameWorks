// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	XAudio2Effects.h: Unreal XAudio2 audio effects interface object.
=============================================================================*/

#pragma once

#include "AudioEffect.h"
#include "XAudio2Device.h"

/** 
 * XAudio2 effects manager
 */
class FXAudio2EffectsManager : public FAudioEffectsManager
{
public:
	FXAudio2EffectsManager( FXAudio2Device* InDevice );
	~FXAudio2EffectsManager( void );

	/** 
     * Create voices that pipe the dry or EQ'd sound to the master output
	 */
	bool CreateEQPremasterVoices();

	/** 
     * Create a voice that pipes the reverb sounds to the premastering voices
	 */
	bool CreateReverbVoice();

	/** 
     * Create a voice that pipes the radio sounds to the master output
	 *
	 * @param	XAudio2Device	The audio device used by the engine.
	 */
	bool CreateRadioVoice();

	/** 
	 * Calls the platform specific code to set the parameters that define reverb
	 */
	virtual void SetReverbEffectParameters( const FAudioReverbEffect& ReverbEffectParameters );

	/** 
	 * Calls the platform specific code to set the parameters that define EQ
	 */
	virtual void SetEQEffectParameters( const FAudioEQEffect& ReverbEffectParameters );

	/** 
	 * Calls the platform specific code to set the parameters that define a radio effect.
	 * 
	 * @param	RadioEffectParameters	The new parameters for the radio distortion effect. 
	 */
	virtual void SetRadioEffectParameters( const FAudioRadioEffect& RadioEffectParameters );

private:
	/** 
	 * Creates the submix voice that handles the reverb effect
	 */
	bool CreateEffectsSubmixVoices( void );

	/** Cache the XAudio2 device pointer */
	FXAudio2Device*					XAudio2Device;

	/** Reverb effect */
	struct IUnknown*				ReverbEffect;
	/** EQ effect */
	struct IUnknown*				EQEffect;
	/** Radio effect */
	struct IUnknown*				RadioEffect;

	/** For receiving 6 channels of audio that will have no EQ applied */
	struct IXAudio2SubmixVoice*		DryPremasterVoice;
	/** For receiving 6 channels of audio that can have EQ applied */
	struct IXAudio2SubmixVoice*		EQPremasterVoice;
	/** For receiving audio that will have reverb applied */
	struct IXAudio2SubmixVoice*		ReverbEffectVoice;
	/** For receiving audio that will have radio effect applied */
	struct IXAudio2SubmixVoice*		RadioEffectVoice;

	friend class FXAudio2Device;
	friend class FXAudio2SoundSource;
};
