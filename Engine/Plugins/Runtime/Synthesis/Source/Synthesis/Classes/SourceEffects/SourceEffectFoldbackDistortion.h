// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundEffectSource.h"
#include "DSP/FoldbackDistortion.h"
#include "SourceEffectFoldbackDistortion.generated.h"

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectFoldbackDistortionSettings
{
	GENERATED_USTRUCT_BODY()

	// The amount of gain to add to input to allow forcing the triggering of the threshold
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "60.0", UIMin = "0.0", UIMax = "20.0"))
	float InputGainDb;

	// If the audio amplitude is higher than this, it will fold back
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "-90.0", ClampMax = "0.0", UIMin = "-60.0", UIMax = "0.0"))
	float ThresholdDb;

	// The amount of gain to apply to the output
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "-90.0", ClampMax = "20.0", UIMin = "-60.0", UIMax = "20.0"))
	float OutputGainDb;

	FSourceEffectFoldbackDistortionSettings()
		: InputGainDb(0.0f)
		, ThresholdDb(-6.0f)
		, OutputGainDb(-3.0f)
	{}
};

class SYNTHESIS_API FSourceEffectFoldbackDistortion : public FSoundEffectSource
{
public:
	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InSampleRate) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) override;

protected:
	Audio::FFoldbackDistortion FoldbackDistortion;
};

UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectFoldbackDistortionPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SourceEffectFoldbackDistortion)

	virtual FColor GetPresetColor() const override { return FColor(56.0f, 225.0f, 156.0f); }

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectFoldbackDistortionSettings& InSettings);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Effects")
	FSourceEffectFoldbackDistortionSettings Settings;
};
