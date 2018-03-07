// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AudioEffect.cpp: Unreal base audio.
=============================================================================*/

#include "AudioEffect.h"
#include "Misc/App.h"
#include "Audio.h"
#include "Sound/AudioVolume.h"
#include "Sound/ReverbEffect.h"
#include "AudioDeviceManager.h"
#include "AudioDevice.h"

/** 
 * Default settings for a null reverb effect
 */
FAudioReverbEffect::FAudioReverbEffect( void )
{
	Time = 0.0;
	Volume = 0.0f;

	Density = 1.0f;					
	Diffusion = 1.0f;				
	Gain = 0.32f;					
	GainHF = 0.89f;					
	DecayTime = 1.49f;				
	DecayHFRatio = 0.83f;			
	ReflectionsGain = 0.05f;		
	ReflectionsDelay = 0.007f;		
	LateGain = 1.26f;				
	LateDelay = 0.011f;				
	AirAbsorptionGainHF = 0.994f;	
	RoomRolloffFactor = 0.0f;		
}

/** 
 * Construct generic reverb settings based in the I3DL2 standards
 */
FAudioReverbEffect::FAudioReverbEffect( float InRoom, 
								 		float InRoomHF, 
								 		float InRoomRolloffFactor,	
								 		float InDecayTime,			
								 		float InDecayHFRatio,		
								 		float InReflections,		
								 		float InReflectionsDelay,	
								 		float InReverb,				
								 		float InReverbDelay,		
								 		float InDiffusion,			
								 		float InDensity,			
								 		float InAirAbsorption )
{
	Time = 0.0;
	Volume = 0.0f;

	Density = InDensity;
	Diffusion = InDiffusion;
	Gain = InRoom;
	GainHF = InRoomHF;
	DecayTime = InDecayTime;
	DecayHFRatio = InDecayHFRatio;
	ReflectionsGain = InReflections;
	ReflectionsDelay = InReflectionsDelay;
	LateGain = InReverb;
	LateDelay = InReverbDelay;
	RoomRolloffFactor = InRoomRolloffFactor;
	AirAbsorptionGainHF = InAirAbsorption;
}

FAudioReverbEffect& FAudioReverbEffect::operator=(class UReverbEffect* InReverbEffect)
{
	if( InReverbEffect )
	{
		Density = InReverbEffect->Density;
		Diffusion = InReverbEffect->Diffusion;
		Gain = InReverbEffect->Gain;
		GainHF = InReverbEffect->GainHF;
		DecayTime = InReverbEffect->DecayTime;
		DecayHFRatio = InReverbEffect->DecayHFRatio;
		ReflectionsGain = InReverbEffect->ReflectionsGain;
		ReflectionsDelay = InReverbEffect->ReflectionsDelay;
		LateGain = InReverbEffect->LateGain;
		LateDelay = InReverbEffect->LateDelay;
		AirAbsorptionGainHF = InReverbEffect->AirAbsorptionGainHF;
		RoomRolloffFactor = InReverbEffect->RoomRolloffFactor;
	}

	return *this;
}

/** 
 * Get interpolated reverb parameters
 */
void FAudioReverbEffect::Interpolate( float InterpValue, const FAudioReverbEffect& Start, const FAudioReverbEffect& End )
{
	float InvInterpValue = 1.0f - InterpValue;
	Time = FApp::GetCurrentTime();
	Volume = ( Start.Volume * InvInterpValue ) + ( End.Volume * InterpValue );
	Density = ( Start.Density * InvInterpValue ) + ( End.Density * InterpValue );				
	Diffusion = ( Start.Diffusion * InvInterpValue ) + ( End.Diffusion * InterpValue );				
	Gain = ( Start.Gain * InvInterpValue ) + ( End.Gain * InterpValue );					
	GainHF = ( Start.GainHF * InvInterpValue ) + ( End.GainHF * InterpValue );					
	DecayTime = ( Start.DecayTime * InvInterpValue ) + ( End.DecayTime * InterpValue );				
	DecayHFRatio = ( Start.DecayHFRatio * InvInterpValue ) + ( End.DecayHFRatio * InterpValue );			
	ReflectionsGain = ( Start.ReflectionsGain * InvInterpValue ) + ( End.ReflectionsGain * InterpValue );		
	ReflectionsDelay = ( Start.ReflectionsDelay * InvInterpValue ) + ( End.ReflectionsDelay * InterpValue );		
	LateGain = ( Start.LateGain * InvInterpValue ) + ( End.LateGain * InterpValue );				
	LateDelay = ( Start.LateDelay * InvInterpValue ) + ( End.LateDelay * InterpValue );				
	AirAbsorptionGainHF = ( Start.AirAbsorptionGainHF * InvInterpValue ) + ( End.AirAbsorptionGainHF * InterpValue );	
	RoomRolloffFactor = ( Start.RoomRolloffFactor * InvInterpValue ) + ( End.RoomRolloffFactor * InterpValue );		
}

/** 
 * Validate all settings are in range
 */
void FAudioEQEffect::ClampValues( void )
{
	FrequencyCenter0 = FMath::Clamp(FrequencyCenter0, MIN_FILTER_FREQUENCY, MAX_FILTER_FREQUENCY);
	FrequencyCenter1 = FMath::Clamp(FrequencyCenter1, MIN_FILTER_FREQUENCY, MAX_FILTER_FREQUENCY);
	FrequencyCenter2 = FMath::Clamp(FrequencyCenter2, MIN_FILTER_FREQUENCY, MAX_FILTER_FREQUENCY);
	FrequencyCenter3 = FMath::Clamp(FrequencyCenter3, MIN_FILTER_FREQUENCY, MAX_FILTER_FREQUENCY);

	Gain0 = FMath::Clamp(Gain0, 0.0f, MAX_FILTER_GAIN);
	Gain1 = FMath::Clamp(Gain1, 0.0f, MAX_FILTER_GAIN);
	Gain2 = FMath::Clamp(Gain2, 0.0f, MAX_FILTER_GAIN);
	Gain3 = FMath::Clamp(Gain3, 0.0f, MAX_FILTER_GAIN);

	Bandwidth0 = FMath::Clamp(Bandwidth0, MIN_FILTER_BANDWIDTH, MAX_FILTER_BANDWIDTH);
	Bandwidth1 = FMath::Clamp(Bandwidth1, MIN_FILTER_BANDWIDTH, MAX_FILTER_BANDWIDTH);
	Bandwidth2 = FMath::Clamp(Bandwidth2, MIN_FILTER_BANDWIDTH, MAX_FILTER_BANDWIDTH);
	Bandwidth3 = FMath::Clamp(Bandwidth3, MIN_FILTER_BANDWIDTH, MAX_FILTER_BANDWIDTH);

}

/** 
 * Interpolate EQ settings based on time
 */
void FAudioEQEffect::Interpolate(float InterpValue, const FAudioEQEffect& Start, const FAudioEQEffect& End )
{
	RootTime = FApp::GetCurrentTime();

	FrequencyCenter0 = FMath::Lerp(Start.FrequencyCenter0, End.FrequencyCenter0, InterpValue);
	FrequencyCenter1 = FMath::Lerp(Start.FrequencyCenter1, End.FrequencyCenter1, InterpValue);
	FrequencyCenter2 = FMath::Lerp(Start.FrequencyCenter2, End.FrequencyCenter2, InterpValue);
	FrequencyCenter3 = FMath::Lerp(Start.FrequencyCenter3, End.FrequencyCenter3, InterpValue);

	Gain0 = FMath::Lerp(Start.Gain0, End.Gain0, InterpValue);
	Gain1 = FMath::Lerp(Start.Gain1, End.Gain1, InterpValue);
	Gain2 = FMath::Lerp(Start.Gain2, End.Gain2, InterpValue);
	Gain3 = FMath::Lerp(Start.Gain3, End.Gain3, InterpValue);

	Bandwidth0 = FMath::Lerp(Start.Bandwidth0, End.Bandwidth0, InterpValue);
	Bandwidth1 = FMath::Lerp(Start.Bandwidth1, End.Bandwidth1, InterpValue);
	Bandwidth2 = FMath::Lerp(Start.Bandwidth2, End.Bandwidth2, InterpValue);
	Bandwidth3 = FMath::Lerp(Start.Bandwidth3, End.Bandwidth3, InterpValue);
}

/**
 * Converts and volume (0.0f to 1.0f) to a deciBel value
 */
int64 FAudioEffectsManager::VolumeToDeciBels( float Volume )
{
	int64 DeciBels = -100;

	if( Volume > 0.0f )
	{
		DeciBels = FMath::Clamp<int64>( ( int64 )( 20.0f * log10f( Volume ) ), -100, 0 ) ;
	}

	return( DeciBels );
}


/**
 * Converts and volume (0.0f to 1.0f) to a MilliBel value (a Hundredth of a deciBel)
 */
int64 FAudioEffectsManager::VolumeToMilliBels( float Volume, int32 MaxMilliBels )
{
	int64 MilliBels = -10000;

	if( Volume > 0.0f )
	{
		MilliBels = FMath::Clamp<int64>( ( int64)( 2000.0f * log10f( Volume ) ), -10000, MaxMilliBels );
	}

	return( MilliBels );
}

/** 
 * Gets the parameters for reverb based on settings and time
 */
bool FAudioEffectsManager::Interpolate( FAudioReverbEffect& Current, const FAudioReverbEffect& Start, const FAudioReverbEffect& End )
{
	float InterpValue = 1.0f;
	if( End.Time - Start.Time > 0.0 )
	{
		InterpValue = ( float )( ( FApp::GetCurrentTime() - Start.Time ) / ( End.Time - Start.Time ) );
	}

	if( InterpValue >= 1.0f )
	{
		Current = End;
		return true;
	}

	if( InterpValue <= 0.0f )
	{
		Current = Start;
		return false;
	}

	Current.Interpolate( InterpValue, Start, End );
	return false;
}

/** 
 * Gets the parameters for EQ based on settings and time
 */
bool FAudioEffectsManager::Interpolate( FAudioEQEffect& Current, const FAudioEQEffect& Start, const FAudioEQEffect& End )
{
	float InterpValue = 1.0f;
	if( End.RootTime - Start.RootTime > 0.0 )
	{
		InterpValue = ( float )( ( FApp::GetCurrentTime() - Start.RootTime ) / ( End.RootTime - Start.RootTime ) );
	}

	if( InterpValue >= 1.0f )
	{
		Current = End;
		return true;
	}

	if( InterpValue <= 0.0f )
	{
		Current = Start;
		return false;
	}

	Current.Interpolate( InterpValue, Start, End );
	return false;
}

/** 
 * Clear out any reverb and EQ settings
 */
FAudioEffectsManager::FAudioEffectsManager( FAudioDevice* InDevice )
	: AudioDevice(InDevice)
	, bEffectsInitialised(false)
	, CurrentReverbAsset(nullptr)
	, CurrentEQMix(nullptr)
	, bReverbActive(false)
	, bEQActive(false)
	, bReverbChanged(true) // Setting to true to catch the first default reverb setting
	, bEQChanged(false)
{
	InitAudioEffects();
}

void FAudioEffectsManager::ResetInterpolation()
{
	InitAudioEffects();
}

/** 
 * Sets up default reverb and eq settings
 */
void FAudioEffectsManager::InitAudioEffects( void )
{
	// Clear out the default reverb settings
	FReverbSettings ReverbSettings;
	ReverbSettings.ReverbEffect = NULL;
	ReverbSettings.Volume = 0.0f;
	ReverbSettings.FadeTime = 0.1f;
	SetReverbSettings( ReverbSettings );

	FMemory::Memzero(&PrevReverbEffect, sizeof(PrevReverbEffect));

	ClearMixSettings();
}

void FAudioEffectsManager::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject(CurrentReverbAsset);
}

/**
 * Called every tick from UGameViewportClient::Draw
 * 
 * Sets new reverb mode if necessary. Otherwise interpolates to the current settings and calls SetEffect to handle
 * the platform specific aspect.
 */
void FAudioEffectsManager::SetReverbSettings( const FReverbSettings& ReverbSettings, bool bForce)
{
	/** Update the settings if the reverb type has changed */
	if( ReverbSettings.bApplyReverb && (ReverbSettings.ReverbEffect != CurrentReverbAsset || bForce))
	{
		FString CurrentReverbName = CurrentReverbAsset ? CurrentReverbAsset->GetName() : TEXT("None");
		FString NextReverbName = ReverbSettings.ReverbEffect ? ReverbSettings.ReverbEffect->GetName() : TEXT("None");
		UE_LOG(LogAudio, Log, TEXT( "FAudioDevice::SetReverbSettings(): Old - %s  New - %s:%f (%f)" ),
			*CurrentReverbName, *NextReverbName, ReverbSettings.Volume, ReverbSettings.FadeTime );

		if( ReverbSettings.Volume > 1.0f )
		{
			UE_LOG(LogAudio, Warning, TEXT( "FAudioDevice::SetReverbSettings(): Illegal volume %g (should be 0.0f <= Volume <= 1.0f)" ), ReverbSettings.Volume );
		}

		CurrentReverbSettings = ReverbSettings;

		SourceReverbEffect = CurrentReverbEffect;
		SourceReverbEffect.Time = FApp::GetCurrentTime();

		DestinationReverbEffect = ReverbSettings.ReverbEffect;
		bReverbChanged = true;

		if (bForce)
		{
			DestinationReverbEffect.Time = FApp::GetCurrentTime();
		}
		else
		{
			DestinationReverbEffect.Time = FApp::GetCurrentTime() + ReverbSettings.FadeTime;
		}
		DestinationReverbEffect.Volume = ReverbSettings.Volume;
		if( !ReverbSettings.ReverbEffect )
		{
			DestinationReverbEffect.Volume = 0.0f;
		}

		CurrentReverbAsset = ReverbSettings.ReverbEffect;
	}
}

/**
 * Sets new EQ mix if necessary. Otherwise interpolates to the current settings and calls SetEffect to handle
 * the platform specific aspect.
 */
void FAudioEffectsManager::SetMixSettings(USoundMix* NewMix, bool bIgnorePriority, bool bForce)
{
	if (NewMix && (NewMix != CurrentEQMix || bForce))
	{
		// Check whether the priority of this SoundMix is higher than existing one
		if (CurrentEQMix == NULL || bIgnorePriority || NewMix->EQPriority > CurrentEQMix->EQPriority)
		{
			UE_LOG(LogAudio, Log, TEXT( "FAudioEffectsManager::SetMixSettings(): %s" ), *NewMix->GetName() );

			SourceEQEffect = CurrentEQEffect;
			SourceEQEffect.RootTime = FApp::GetCurrentTime();

			if( NewMix->bApplyEQ )
			{
				DestinationEQEffect = NewMix->EQSettings;
			}
			else
			{
				// it doesn't have EQ settings, so interpolate back to default
				DestinationEQEffect = FAudioEQEffect();
			}

			DestinationEQEffect.RootTime = FApp::GetCurrentTime() + NewMix->FadeInTime;
			DestinationEQEffect.ClampValues();

			bEQChanged = true;

			CurrentEQMix = NewMix;
		}
	}
}

/**
 * If there is an active SoundMix, clear it and any EQ settings it applied
 */
void FAudioEffectsManager::ClearMixSettings()
{
	if (CurrentEQMix)
	{
		UE_LOG(LogAudio, Log, TEXT( "FAudioEffectsManager::ClearMixSettings(): %s" ), *CurrentEQMix->GetName() );

		SourceEQEffect = CurrentEQEffect;
		double CurrentTime = FApp::GetCurrentTime();
		SourceEQEffect.RootTime = CurrentTime;

		// interpolate back to default
		DestinationEQEffect = FAudioEQEffect();
		DestinationEQEffect.RootTime = CurrentTime + CurrentEQMix->FadeOutTime;

		CurrentEQMix = NULL;
	}
}


static bool operator==(const FAudioReverbEffect& EffectA, const FAudioReverbEffect& EffectB)
{
	bool bIsEqual = true;

	bIsEqual = bIsEqual && FMath::IsNearlyEqual(EffectA.Volume, EffectB.Volume, KINDA_SMALL_NUMBER);
	bIsEqual = bIsEqual && FMath::IsNearlyEqual(EffectA.Density, EffectB.Density, KINDA_SMALL_NUMBER);
	bIsEqual = bIsEqual && FMath::IsNearlyEqual(EffectA.Diffusion, EffectB.Diffusion, KINDA_SMALL_NUMBER);
	bIsEqual = bIsEqual && FMath::IsNearlyEqual(EffectA.Gain, EffectB.Gain, KINDA_SMALL_NUMBER);
	bIsEqual = bIsEqual && FMath::IsNearlyEqual(EffectA.GainHF, EffectB.GainHF, KINDA_SMALL_NUMBER);
	bIsEqual = bIsEqual && FMath::IsNearlyEqual(EffectA.DecayTime, EffectB.DecayTime, KINDA_SMALL_NUMBER);
	bIsEqual = bIsEqual && FMath::IsNearlyEqual(EffectA.DecayHFRatio, EffectB.DecayHFRatio, KINDA_SMALL_NUMBER);
	bIsEqual = bIsEqual && FMath::IsNearlyEqual(EffectA.ReflectionsGain, EffectB.ReflectionsGain, KINDA_SMALL_NUMBER);
	bIsEqual = bIsEqual && FMath::IsNearlyEqual(EffectA.ReflectionsDelay, EffectB.ReflectionsDelay, KINDA_SMALL_NUMBER);
	bIsEqual = bIsEqual && FMath::IsNearlyEqual(EffectA.LateGain, EffectB.LateGain, KINDA_SMALL_NUMBER);
	bIsEqual = bIsEqual && FMath::IsNearlyEqual(EffectA.LateDelay, EffectB.LateDelay, KINDA_SMALL_NUMBER);
	bIsEqual = bIsEqual && FMath::IsNearlyEqual(EffectA.AirAbsorptionGainHF, EffectB.AirAbsorptionGainHF, KINDA_SMALL_NUMBER);
	bIsEqual = bIsEqual && FMath::IsNearlyEqual(EffectA.RoomRolloffFactor, EffectB.RoomRolloffFactor, KINDA_SMALL_NUMBER);

	return bIsEqual;
}

static bool operator!=(const FAudioReverbEffect& EffectA, const FAudioReverbEffect& EffectB)
{
	return !(EffectA == EffectB);
}

/** 
 * Feed in new settings to the audio effect system
 */
void FAudioEffectsManager::Update()
{
	// Check for changes to the mix so we can hear EQ changes in real-time
#if WITH_EDITORONLY_DATA
	if (CurrentEQMix && CurrentEQMix->bChanged)
	{
		CurrentEQMix->bChanged = false;
		SetMixSettings(CurrentEQMix, true, true);
	}

	if (CurrentReverbAsset && CurrentReverbAsset->bChanged)
	{
		CurrentReverbAsset->bChanged = false;
		class FAudioDeviceManager* DeviceManager = GEngine->GetAudioDeviceManager();
		TArray<FAudioDevice*>& AudioDevices = DeviceManager->GetAudioDevices();
		for (int32 i = 0; i < AudioDevices.Num(); ++i)
		{
			if (AudioDevices[i])
			{
				FAudioEffectsManager* EffectsManager = AudioDevices[i]->GetEffects();
				EffectsManager->SetReverbSettings(CurrentReverbSettings, true);
			}
		}
		
	}

#endif

	const bool bIsReverbDone = Interpolate(CurrentReverbEffect, SourceReverbEffect, DestinationReverbEffect);
	if (!bIsReverbDone || bReverbActive || bReverbChanged)
	{
		bReverbChanged = false;
		PrevReverbEffect = CurrentReverbEffect;
		bReverbActive = !bIsReverbDone;
		SetReverbEffectParameters(CurrentReverbEffect);
	}

	const bool bIsEQDone = Interpolate(CurrentEQEffect, SourceEQEffect, DestinationEQEffect);
	if (!bIsEQDone || bEQActive || bEQChanged)
	{
		bEQChanged = false;
		bEQActive = !bIsEQDone;
		SetEQEffectParameters(CurrentEQEffect);
	}
}

// end 
