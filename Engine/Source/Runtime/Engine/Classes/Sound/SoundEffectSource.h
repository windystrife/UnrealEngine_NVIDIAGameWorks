// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Sound/SoundEffectPreset.h"
#include "Sound/SoundEffectBase.h"

#include "SoundEffectSource.generated.h"

class FSoundEffectSource;
class FSoundEffectBase;

/** This is here to make sure users don't mix up source and submix effects in the editor. Asset sorting, drag-n-drop, etc. */
UCLASS(config = Engine, abstract, editinlinenew, BlueprintType)
class ENGINE_API USoundEffectSourcePreset : public USoundEffectPreset
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ENGINE_API FSourceEffectChainEntry
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect")
	USoundEffectSourcePreset* Preset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect")
	uint32 bBypass : 1;
};


UCLASS(BlueprintType)
class ENGINE_API USoundEffectSourcePresetChain : public UObject
{
	GENERATED_BODY()

public:

	/** Chain of source effects to use for this sound source. */
	UPROPERTY(EditAnywhere, Category = "SourceEffect")
	TArray<FSourceEffectChainEntry> Chain;

	/** Whether to keep the source alive for the duration of the effect chain tails. */
	UPROPERTY(EditAnywhere, Category = Effects)
	uint32 bPlayEffectChainTails : 1;

protected:

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

};

/** Struct which has data needed to initialize the source effect. */
struct FSoundEffectSourceInitData
{
	float SampleRate;
	int32 NumSourceChannels;
	float SourceDuration;
	double AudioClock;

	// The object id of the parent preset
	uint32 ParentPresetUniqueId;

	FSoundEffectSourceInitData()
	: SampleRate(0.0f)
	, NumSourceChannels(0)
	, SourceDuration(0.0f)
	, AudioClock(0.0)
	, ParentPresetUniqueId(INDEX_NONE)
	{}
};

/** Struct which has data to initialize the source effect. */
struct FSoundEffectSourceInputData
{
	TArray<float> AudioFrame;
	FVector SourcePosition;
	FVector LeftChannelPosition;
	FVector RightChannelPosition;
	FVector ListenerPosition;
	float CurrentVolume;
	float CurrentPitch;
	float CurrentPlayTime;
	float Duration;
	float Distance;
	double AudioClock;
	int32 CurrentLoopCount;
	int32 MaxLoopCount;
	uint8 bLooping : 1;
	uint8 bIsSpatialized : 1;

	FSoundEffectSourceInputData()
	{
		FMemory::Memzero(this, sizeof(FSoundEffectSourceInputData));
	}
};

struct FSoundEffectSourceOutputData
{
	TArray<float> AudioFrame;
};

class ENGINE_API FSoundEffectSource : public FSoundEffectBase
{
public:
	virtual ~FSoundEffectSource() {}

	/** Called on an audio effect at initialization on main thread before audio processing begins. */
	virtual void Init(const FSoundEffectSourceInitData& InSampleRate) = 0;

	/** Process the input block of audio. Called on audio thread. */
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) = 0;
};

