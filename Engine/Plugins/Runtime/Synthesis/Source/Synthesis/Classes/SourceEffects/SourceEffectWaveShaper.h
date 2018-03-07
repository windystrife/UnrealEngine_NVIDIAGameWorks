// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundEffectSource.h"
#include "DSP/WaveShaper.h"
#include "SourceEffectWaveShaper.generated.h"

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectWaveShaperSettings
{
	GENERATED_USTRUCT_BODY()

	// The amount of wave shaping. 0.0 = no wave shaping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0"))
	float Amount;

	// The amount of wave shaping. 0.0 = no wave shaping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "-60.0", ClampMax = "20.0", UIMin = "-60.0", UIMax = "20.0"))
	float OutputGainDb;

	FSourceEffectWaveShaperSettings()
		: Amount(1.0f)
		, OutputGainDb(0.0f)
	{}
};

class SYNTHESIS_API FSourceEffectWaveShaper : public FSoundEffectSource
{
public:
	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InSampleRate) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) override;

protected:
	Audio::FWaveShaper WaveShaper;
};



UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectWaveShaperPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SourceEffectWaveShaper)

	virtual FColor GetPresetColor() const override { return FColor(218.0f, 248.0f, 78.0f); }

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectWaveShaperSettings& InSettings);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Effects")
	FSourceEffectWaveShaperSettings Settings;
};
