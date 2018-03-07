// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundEffectSource.h"
#include "DSP/Chorus.h"
#include "SourceEffectChorus.generated.h"

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectChorusSettings
{
	GENERATED_USTRUCT_BODY()

	// The depth of the chorus effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Depth;

	// The frequency of the chorus effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float Frequency;

	// The feedback of the chorus effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Feedback;

	// The wetlevel of the chorus effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float WetLevel;

	// The spread of the effect (larger means greater difference between left and right delay lines)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Spread;

	FSourceEffectChorusSettings()
		: Depth(0.2f)
		, Frequency(2.0f)
		, Feedback(0.3f)
		, WetLevel(0.5f)
		, Spread(0.0f)
	{}
};

class SYNTHESIS_API FSourceEffectChorus : public FSoundEffectSource
{
public:
	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InSampleRate) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) override;

protected:
	Audio::FChorus Chorus;
};

UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectChorusPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SourceEffectChorus)

	virtual FColor GetPresetColor() const override { return FColor(102.0f, 85.0f, 121.0f); }

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectChorusSettings& InSettings);

	// The depth of the chorus effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	FSourceEffectChorusSettings Settings;
};