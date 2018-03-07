// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AudioEffect.h"
#include "Delay.h"
#include "DSP/Dsp.h"
#include "Sound/SoundEffectSubmix.h"
#include "SubmixEffectDelay.generated.h"

// ========================================================================
// FSubmixEffectDelaySettings
// UStruct used to define user-exposed params for use with your effect.
// ========================================================================

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSubmixEffectDelaySettings
{
	GENERATED_USTRUCT_BODY()

	// Maximum possible length for a delay, in milliseconds. Changing this at runtime will reset the effect.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Initialization, meta = (ClampMin = "10.0", UIMin = "10.0", UIMax = "20000.0"))
	float MaximumDelayLength;

	// Number of milliseconds over which a tap will reach it's set length and gain. Smaller values are more responsive, while larger values will make pitching less dramatic.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Realtime, meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "20000.0"))
	float InterpolationTime;

	// Number of milliseconds of delay.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Realtime, meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "20000.0"))
	float DelayLength;

	FSubmixEffectDelaySettings()
		: MaximumDelayLength(2000.0f)
		, InterpolationTime(400.0f)
		, DelayLength(1000.0f)
	{
	}
};

class SYNTHESIS_API FSubmixEffectDelay : public FSoundEffectSubmix
{
public:
	FSubmixEffectDelay();
	~FSubmixEffectDelay();

	//~ Begin FSoundEffectSubmix
	virtual void Init(const FSoundEffectSubmixInitData& InData) override;
	virtual void OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData) override;
	//~ End FSoundEffectSubmix

	//~ Begin FSoundEffectBase
	virtual void OnPresetChanged() override;
	//~End FSoundEffectBase

	// Sets the reverb effect parameters based from audio thread code
	void SetEffectParameters(const FSubmixEffectDelaySettings& InTapEffectParameters);

	// Set the time it takes, in milliseconds, to arrive at a new parameter.
	void SetInterpolationTime(float Time);

	// Set how long the actualy delay is, in milliseconds.
	void SetDelayLineLength(float Length);

private:
	// This is called on the audio render thread to query the parameters.
	void UpdateParameters();

	// Called on the audio render thread when the number of channels is changed
	void OnNumChannelsChanged(int32 NumChannels);

	// Params struct used to pass parameters safely to the audio render thread.
	Audio::TParams<FSubmixEffectDelaySettings> Params;

	// Sample rate cached at initialization. Used to gage interpolation times.
	float SampleRate;

	// Current maximum delay line length, in milliseconds.
	float MaxDelayLineLength;

	// Current interpolation time, in seconds.
	float InterpolationTime;

	// Most recently set delay line length.
	float TargetDelayLineLength;

	Audio::FLinearEase InterpolationInfo;

	// Delay lines for each channel.
	TArray<Audio::FDelay> DelayLines;
};

// ========================================================================
// USubmixEffectDelayPreset
// Class which processes audio streams and uses parameters defined in the preset class.
// ========================================================================

UCLASS()
class SYNTHESIS_API USubmixEffectDelayPreset : public USoundEffectSubmixPreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SubmixEffectDelay)

	// Set all tap delay setting. This will replace any dynamically added or modified taps.
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects|Delay")
	void SetSettings(const FSubmixEffectDelaySettings& InSettings);

	// Get the maximum delay possible.
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects|Delay")
	float GetMaxDelayInMilliseconds() { return DynamicSettings.MaximumDelayLength;  };

	// Set the time it takes to interpolate between parameters, in milliseconds.
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects|Delay")
	void SetInterpolationTime(float Time);

	// Set how long the delay actually is, in milliseconds.
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects|Delay")
	void SetDelay(float Length);

public:
	virtual void OnInit() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixEffectPreset, Meta = (ShowOnlyInnerProperties))
	FSubmixEffectDelaySettings Settings;

	UPROPERTY(transient)
	FSubmixEffectDelaySettings DynamicSettings;
};