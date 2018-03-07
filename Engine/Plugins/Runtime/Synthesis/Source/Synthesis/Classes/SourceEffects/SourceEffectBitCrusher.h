// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundEffectSource.h"
#include "DSP/BitCrusher.h"
#include "SourceEffectBitCrusher.generated.h"

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectBitCrusherSettings
{
	GENERATED_USTRUCT_BODY()

	// The reduced frequency to use for the audio stream. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "1.0", UIMin = "500.0", UIMax = "15000.0"))
	float CrushedSampleRate;

	// The reduced bit depth to use for the audio stream
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "1.0", ClampMax = "24.0", UIMin = "1.0", UIMax = "16.0"))
	float CrushedBits;

	FSourceEffectBitCrusherSettings()
		: CrushedSampleRate(8000.0f)
		, CrushedBits(8.0f)
	{}
};

class SYNTHESIS_API FSourceEffectBitCrusher : public FSoundEffectSource
{
public:
	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InSampleRate) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) override;

protected:
	Audio::FBitCrusher BitCrusher;
};

UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectBitCrusherPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SourceEffectBitCrusher)

	virtual FColor GetPresetColor() const override { return FColor(196.0f, 185.0f, 121.0f); }

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectBitCrusherSettings& InSettings);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Effects")
	FSourceEffectBitCrusherSettings Settings;
};
