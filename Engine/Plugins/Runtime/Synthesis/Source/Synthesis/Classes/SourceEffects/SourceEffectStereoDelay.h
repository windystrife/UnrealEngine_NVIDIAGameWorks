// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DSP/DelayStereo.h"
#include "Sound/SoundEffectSource.h"
#include "SourceEffectStereoDelay.generated.h"

UENUM(BlueprintType)
enum class EStereoDelaySourceEffect : uint8
{
	Normal = 0,
	Cross,
	PingPong,
	Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectStereoDelaySettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	EStereoDelaySourceEffect DelayMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "2000.0", UIMin = "0.0", UIMax = "2000.0"))
	float DelayTimeMsec;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Feedback;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float DelayRatio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float WetLevel;

	FSourceEffectStereoDelaySettings()
		: DelayMode(EStereoDelaySourceEffect::PingPong)
		, DelayTimeMsec(500.0f)
		, DelayRatio(0.2f)
		, WetLevel(0.4f)
	{}
};

class SYNTHESIS_API FSourceEffectStereoDelay : public FSoundEffectSource
{
public:
	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InSampleRate) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) override;

protected:
	Audio::FDelayStereo DelayStereo;
};



UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectStereoDelayPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:

	EFFECT_PRESET_METHODS(SourceEffectStereoDelay)

	virtual FColor GetPresetColor() const override { return FColor(23.0f, 121.0f, 225.0f); }

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectStereoDelaySettings& InSettings);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Effects")
	FSourceEffectStereoDelaySettings Settings;
};
