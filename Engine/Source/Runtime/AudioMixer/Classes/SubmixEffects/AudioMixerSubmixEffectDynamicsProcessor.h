// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DSP/DynamicsProcesser.h"
#include "Sound/SoundEffectSubmix.h"
#include "AudioMixerSubmixEffectDynamicsProcessor.generated.h"

UENUM(BlueprintType)
enum class ESubmixEffectDynamicsProcessorType : uint8
{
	Compressor = 0,
	Limiter,
	Expander,
	Gate,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ESubmixEffectDynamicsPeakMode : uint8
{
	MeanSquared = 0,
	RootMeanSquared,
	Peak,
	Count UMETA(Hidden)
};

// A submix dynamics processor
USTRUCT(BlueprintType)
struct AUDIOMIXER_API FSubmixEffectDynamicsProcessorSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	ESubmixEffectDynamicsProcessorType DynamicsProcessorType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	ESubmixEffectDynamicsPeakMode PeakMode;

	// The amount of time to look ahead of the current audio. Allows for transients to be included in dynamics processing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset",  meta = (ClampMin = "0.0", ClampMax = "50.0", UIMin = "0.0", UIMax = "50.0"))
	float LookAheadMsec;

	// The amount of time to ramp into any dynamics processing effect in milliseconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "1.0", ClampMax = "300.0", UIMin = "1.0", UIMax = "200.0"))
	float AttackTimeMsec;

	// The amount of time to release the dynamics processing effect in milliseconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "20.0", ClampMax = "5000.0", UIMin = "20.0", UIMax = "5000.0"))
	float ReleaseTimeMsec;

	// The threshold at which to perform a dynamics processing operation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "-60.0", ClampMax = "0.0", UIMin = "-60.0", UIMax = "0.0"))
	float ThresholdDb;

	// The dynamics processor ratio -- has different meaning depending on the processor type.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "1.0", ClampMax = "20.0", UIMin = "1.0", UIMax = "20.0"))
	float Ratio;

	// The knee bandwidth of the compressor to use in dB
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "20.0", UIMin = "0.0", UIMax = "20.0"))
	float KneeBandwidthDb;

	// The input gain of the dynamics processor in dB
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "-12.0", ClampMax = "20.0", UIMin = "-12.0", UIMax = "20.0"))
	float InputGainDb;

	// The output gain of the dynamics processor in dB
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "20.0", UIMin = "0.0", UIMax = "20.0"))
	float OutputGainDb;

	// Whether or not to average all channels of audio before inputing into the dynamics processor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	uint32 bChannelLinked : 1;

	// Toggles treating the attack and release envelopes as analog-style vs digital-style. Analog will respond a bit more naturally/slower.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	uint32 bAnalogMode : 1;

	FSubmixEffectDynamicsProcessorSettings()
		: DynamicsProcessorType(ESubmixEffectDynamicsProcessorType::Compressor)
		, PeakMode(ESubmixEffectDynamicsPeakMode::RootMeanSquared)
		, LookAheadMsec(3.0f)
		, AttackTimeMsec(10.0f)
		, ReleaseTimeMsec(100.0f)
		, ThresholdDb(-6.0f)
		, Ratio(1.5f)
		, KneeBandwidthDb(10.0f)
		, InputGainDb(0.0f)
		, OutputGainDb(0.0f)
		, bChannelLinked(true)
		, bAnalogMode(true)
	{
	}
};


class AUDIOMIXER_API FSubmixEffectDynamicsProcessor : public FSoundEffectSubmix
{
public:
	FSubmixEffectDynamicsProcessor();

	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSubmixInitData& InSampleRate) override;

	// Process the input block of audio. Called on audio thread.
	virtual void OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData) override;

	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

protected:
	TArray<float> AudioInputFrame;
	TArray<float> AudioOutputFrame;
	Audio::FDynamicsProcessor DynamicsProcessor;
};

UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class AUDIOMIXER_API USubmixEffectDynamicsProcessorPreset : public USoundEffectSubmixPreset
{
	GENERATED_BODY()

public:

	EFFECT_PRESET_METHODS(SubmixEffectDynamicsProcessor)

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSubmixEffectDynamicsProcessorSettings& InSettings);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixEffectPreset)
	FSubmixEffectDynamicsProcessorSettings Settings;
};