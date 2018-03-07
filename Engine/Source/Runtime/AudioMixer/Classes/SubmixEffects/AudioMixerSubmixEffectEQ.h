// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DSP/EQ.h"
#include "Sound/SoundEffectSubmix.h"
#include "Sound/SoundMix.h"
#include "AudioMixerSubmixEffectEQ.generated.h"

// A multiband EQ submix effect.
USTRUCT(BlueprintType)
struct AUDIOMIXER_API FSubmixEffectEQBand
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubmixEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "20000.0", UIMin = "0.0", UIMax = "15000.0"))
	float Frequency;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubmixEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "10.0"))
	float Bandwidth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubmixEffect|Preset", meta = (ClampMin = "-90.0", ClampMax = "20.0", UIMin = "-90.0", UIMax = "20.0"))
	float GainDb;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubmixEffect|Preset")
	uint32 bEnabled : 1;

	FSubmixEffectEQBand()
		: Frequency(500.0f)
		, Bandwidth(2.0f)
		, GainDb(0.0f)
		, bEnabled(false)
	{
	}
};

// EQ submix effect
USTRUCT(BlueprintType)
struct FSubmixEffectSubmixEQSettings
{
	GENERATED_USTRUCT_BODY()

	// The EQ bands to use. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SubmixEffect|Preset")
	TArray<FSubmixEffectEQBand> EQBands;
};

class AUDIOMIXER_API FSubmixEffectSubmixEQ : public FSoundEffectSubmix
{
public:
	FSubmixEffectSubmixEQ();

	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSubmixInitData& InSampleRate) override;

	// Process the input block of audio. Called on audio thread.
	virtual void OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData) override;

	// Sets the effect parameters using the old audio engine preset setting object
	void SetEffectParameters(const FAudioEQEffect& InEQEffectParameters);

	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

protected:
	void UpdateParameters(const int32 NumOutputChannels);

	// An EQ effect is a bank of biquad filters
	struct FEQ
	{
		bool bEnabled;
		TArray<Audio::FBiquadFilter> Bands;

		FEQ()
			: bEnabled(true)
		{}
	};

	// Each of these filters is a 2 channel biquad filter. 1 for each stereo pair
	TArray<FEQ> FiltersPerChannel;

	float ScratchInBuffer[2];
	float ScratchOutBuffer[2];
	float SampleRate;
	float NumOutputChannels;
	bool bEQSettingsSet;

	// A pending eq setting change
	Audio::TParams<FSubmixEffectSubmixEQSettings> PendingSettings;

	// Game thread copy of the eq setting
	FSubmixEffectSubmixEQSettings GameThreadEQSettings;

	// Audio render thread copy of the eq setting
	FSubmixEffectSubmixEQSettings RenderThreadEQSettings;
};

UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class AUDIOMIXER_API USubmixEffectSubmixEQPreset : public USoundEffectSubmixPreset
{
	GENERATED_BODY()

public:

	EFFECT_PRESET_METHODS(SubmixEffectSubmixEQ)

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSubmixEffectSubmixEQSettings& InSettings);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixEffectPreset)
	FSubmixEffectSubmixEQSettings Settings;
};