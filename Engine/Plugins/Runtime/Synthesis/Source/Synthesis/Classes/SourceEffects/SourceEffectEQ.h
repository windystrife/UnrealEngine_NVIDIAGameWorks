// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundEffectSource.h"
#include "DSP/EQ.h"
#include "SourceEffectEQ.generated.h"


USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectEQBand
{
	GENERATED_USTRUCT_BODY()

	// The cutoff frequency of the band
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "20000.0", UIMin = "0.0", UIMax = "15000.0"))
	float Frequency;

	// The bandwidth (in octaves) of the band
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "10.0"))
	float Bandwidth;

	// The gain in decibels to apply to the eq band
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "-90.0", ClampMax = "20.0", UIMin = "-90.0", UIMax = "20.0"))
	float GainDb;

	// Whether or not the band is enabled. Allows changing bands on the fly.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	uint32 bEnabled : 1;

	FSourceEffectEQBand()
		: Frequency(500.0f)
		, Bandwidth(2.0f)
		, GainDb(0.0f)
		, bEnabled(false)
	{
	}
};

// EQ source effect settings
USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectEQSettings
{
	GENERATED_USTRUCT_BODY()

	// The EQ bands to use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	TArray<FSourceEffectEQBand> EQBands;
};

class SYNTHESIS_API FSourceEffectEQ : public FSoundEffectSource
{
public:
	FSourceEffectEQ();

	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InSampleRate) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) override;

protected:

	// Bank of biquad filters
	TArray<Audio::FBiquadFilter> Filters;
	float InAudioFrame[2];
	float OutAudioFrame[2];
	float SampleRate;
};

UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectEQPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SourceEffectEQ)

	virtual FColor GetPresetColor() const override { return FColor(53.0f, 158.0f, 153.0f); }

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectEQSettings& InSettings);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Effects")
	FSourceEffectEQSettings Settings;
};
