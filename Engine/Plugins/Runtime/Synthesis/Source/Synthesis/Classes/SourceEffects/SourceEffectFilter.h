// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundEffectSource.h"
#include "DSP/Filter.h"
#include "SourceEffectFilter.generated.h"

UENUM(BlueprintType)
enum class ESourceEffectFilterCircuit : uint8
{
	OnePole = 0,
	StateVariable,
	Ladder,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ESourceEffectFilterType : uint8
{
	LowPass = 0,
	HighPass,
	BandPass,
	BandStop,
	Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectFilterSettings
{
	GENERATED_USTRUCT_BODY()

	// The type of filter circuit to use.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	ESourceEffectFilterCircuit FilterCircuit;

	// The type of filter to use.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	ESourceEffectFilterType FilterType;

	// The filter cutoff frequency
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "20.0", UIMin = "20.0", UIMax = "12000.0"))
	float CutoffFrequency;

	// The filter resonance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.5", ClampMax = "10.0", UIMin = "0.5", UIMax = "10.0"))
	float FilterQ;

	FSourceEffectFilterSettings()
		: FilterCircuit(ESourceEffectFilterCircuit::StateVariable)
		, FilterType(ESourceEffectFilterType::LowPass)
		, CutoffFrequency(800.0f)
		, FilterQ(2.0f)
	{
	}
};

class SYNTHESIS_API FSourceEffectFilter : public FSoundEffectSource
{
public:
	FSourceEffectFilter();

	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InSampleRate) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) override;

protected:
	void UpdateFilter();

	Audio::FStateVariableFilter StateVariableFilter;
	Audio::FLadderFilter LadderFilter;
	Audio::FOnePoleFilter OnePoleFilter;
	Audio::IFilter* CurrentFilter;

	float CutoffFrequency;
	float FilterQ;
	ESourceEffectFilterCircuit CircuitType;
	ESourceEffectFilterType FilterType;

	float AudioInput[2];
	float AudioOutput[2];
};

UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectFilterPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SourceEffectFilter)

	virtual FColor GetPresetColor() const override { return FColor(139.0f, 152.0f, 98.0f); }

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectFilterSettings& InSettings);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Effects")
	FSourceEffectFilterSettings Settings;
};
