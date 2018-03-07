// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
AudioMixerEffectsManager.h: Implementation of backwarwds compatible effects manager
for the multiplatform audio mixer
=============================================================================*/

#pragma once

#include "AudioEffect.h"

namespace Audio
{
	class FMixerDevice;

	class FAudioMixerEffectsManager : public FAudioEffectsManager
	{
	public:
		FAudioMixerEffectsManager(FAudioDevice* InDevice);
		~FAudioMixerEffectsManager() override;

		//~ Begin FAudioEffectsManager
		virtual void SetReverbEffectParameters(const FAudioReverbEffect& ReverbEffectParameters) override;
		virtual void SetEQEffectParameters(const FAudioEQEffect& ReverbEffectParameters) override;
		virtual void SetRadioEffectParameters(const FAudioRadioEffect& ReverbEffectParameters) override;
		//~ End FAudioEffectsManager

	protected:

		FRuntimeFloatCurve MasterReverbWetLevelCurve;
	};
}

