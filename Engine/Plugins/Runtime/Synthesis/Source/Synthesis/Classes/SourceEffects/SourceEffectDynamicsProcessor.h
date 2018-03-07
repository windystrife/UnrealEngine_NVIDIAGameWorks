// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundEffectSource.h"
#include "DSP/DynamicsProcesser.h"
#include "SourceEffectDynamicsProcessor.generated.h"

UENUM(BlueprintType)
enum class ESourceEffectDynamicsProcessorType : uint8
{
	Compressor = 0,
	Limiter,
	Expander,
	Gate,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ESourceEffectDynamicsPeakMode : uint8
{
	MeanSquared = 0,
	RootMeanSquared,
	Peak,
	Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectDynamicsProcessorSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	ESourceEffectDynamicsProcessorType DynamicsProcessorType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	ESourceEffectDynamicsPeakMode PeakMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset",  meta = (ClampMin = "0.0", ClampMax = "50.0", UIMin = "0.0", UIMax = "50.0"))
	float LookAheadMsec;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "1.0", ClampMax = "300.0", UIMin = "1.0", UIMax = "200.0"))
	float AttackTimeMsec;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "20.0", ClampMax = "5000.0", UIMin = "20.0", UIMax = "5000.0"))
	float ReleaseTimeMsec;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "-60.0", ClampMax = "0.0", UIMin = "-60.0", UIMax = "0.0"))
	float ThresholdDb;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "1.0", ClampMax = "20.0", UIMin = "1.0", UIMax = "20.0"))
	float Ratio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "20.0", UIMin = "0.0", UIMax = "20.0"))
	float KneeBandwidthDb;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "-12.0", ClampMax = "20.0", UIMin = "-12.0", UIMax = "20.0"))
	float InputGainDb;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "20.0", UIMin = "0.0", UIMax = "20.0"))
	float OutputGainDb;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	uint32 bStereoLinked : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	uint32 bAnalogMode : 1;

	FSourceEffectDynamicsProcessorSettings()
		: DynamicsProcessorType(ESourceEffectDynamicsProcessorType::Compressor)
		, PeakMode(ESourceEffectDynamicsPeakMode::RootMeanSquared)
		, LookAheadMsec(3.0f)
		, AttackTimeMsec(10.0f)
		, ReleaseTimeMsec(100.0f)
		, ThresholdDb(-6.0f)
		, Ratio(1.5f)
		, KneeBandwidthDb(10.0f)
		, InputGainDb(0.0f)
		, OutputGainDb(0.0f)
		, bStereoLinked(true)
		, bAnalogMode(true)
	{
	}
};

class SYNTHESIS_API FSourceEffectDynamicsProcessor : public FSoundEffectSource
{
public:
	FSourceEffectDynamicsProcessor();

	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InSampleRate) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) override;

protected:

	Audio::FDynamicsProcessor DynamicsProcessor;
};



UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectDynamicsProcessorPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SourceEffectDynamicsProcessor)

	virtual FColor GetPresetColor() const override { return FColor(218.0f, 199.0f, 11.0f); }

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectDynamicsProcessorSettings& InSettings);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Effects")
	FSourceEffectDynamicsProcessorSettings Settings;
};
