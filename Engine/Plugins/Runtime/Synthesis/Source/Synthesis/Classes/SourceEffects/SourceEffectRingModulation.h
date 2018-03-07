// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundEffectSource.h"
#include "DSP/RingModulation.h"
#include "SourceEffectRingModulation.generated.h"

UENUM(BlueprintType)
enum class ERingModulatorTypeSourceEffect : uint8
{
	Sine,
	Saw,
	Triangle,
	Square,
	Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectRingModulationSettings
{
	GENERATED_USTRUCT_BODY()

	// Ring modulation modulator oscillator type
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "5.0", UIMin = "10.0", UIMax = "5000.0"))
	ERingModulatorTypeSourceEffect ModulatorType;

	// Ring modulation frequency
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "5.0", UIMin = "10.0", UIMax = "5000.0"))
	float Frequency;

	// Ring modulation depth
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Depth;

	FSourceEffectRingModulationSettings()
		: ModulatorType(ERingModulatorTypeSourceEffect::Sine)
		, Frequency(100.0f)
		, Depth(0.5f)
	{}
};

class SYNTHESIS_API FSourceEffectRingModulation : public FSoundEffectSource
{
public:
	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InSampleRate) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) override;

protected:
	Audio::FRingModulation RingModulation;
};

UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectRingModulationPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SourceEffectRingModulation)

	virtual FColor GetPresetColor() const override { return FColor(122.0f, 125.0f, 195.0f); }

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectRingModulationSettings& InSettings);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Effects")
	FSourceEffectRingModulationSettings Settings;
};
