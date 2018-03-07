// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundEffectSource.h"
#include "DSP/Phaser.h"
#include "SourceEffectPhaser.generated.h"

UENUM(BlueprintType)
enum class EPhaserLFOType : uint8
{
	Sine = 0,
	UpSaw,
	DownSaw,
	Square,
	Triangle,
	Exponential,
	RandomSampleHold,
	Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectPhaserSettings
{
	GENERATED_USTRUCT_BODY()

	// The wet level of the phaser effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float WetLevel;

	// The LFO frequency of the phaser effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float Frequency;

	// The feedback of the phaser effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Feedback;

	// The phaser LFO type
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	EPhaserLFOType LFOType;

	// Whether or not to use quadtrature phase for the LFO modulation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	bool UseQuadraturePhase;

	FSourceEffectPhaserSettings()
		: WetLevel(0.2f)
		, Frequency(2.0f)
		, Feedback(0.3f)
		, LFOType(EPhaserLFOType::Sine)
	{}
};

class SYNTHESIS_API FSourceEffectPhaser : public FSoundEffectSource
{
public:
	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InSampleRate) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) override;

protected:
	Audio::FPhaser Phaser;
};

UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectPhaserPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SourceEffectPhaser)

	virtual FColor GetPresetColor() const override { return FColor(140.0f, 4.0f, 4.0f); }

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectPhaserSettings& InSettings);

	// The depth of the chorus effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	FSourceEffectPhaserSettings Settings;
};