// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "Sound/SoundEffectSource.h"
#include "DSP/EnvelopeFollower.h"
#include "Components/ActorComponent.h"
#include "SourceEffectEnvelopeFollower.generated.h"

UENUM(BlueprintType)
enum class EEnvelopeFollowerPeakMode : uint8
{
	MeanSquared = 0,
	RootMeanSquared,
	Peak,
	Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectEnvelopeFollowerSettings
{
	GENERATED_USTRUCT_BODY()

	// The attack time of the envelope follower in milliseconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackTime;

	// The release time of the envelope follower in milliseconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ReleaseTime;

	// The peak mode of the envelope follower
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	EEnvelopeFollowerPeakMode PeakMode;

	// Whether or not the envelope follower is in analog mode
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	bool bIsAnalogMode;

	FSourceEffectEnvelopeFollowerSettings()
		: AttackTime(10.0f)
		, ReleaseTime(100.0f)
		, PeakMode(EEnvelopeFollowerPeakMode::Peak)
		, bIsAnalogMode(true)
	{}
};

class SYNTHESIS_API FSourceEffectEnvelopeFollower : public FSoundEffectSource
{
public:
	~FSourceEffectEnvelopeFollower();

	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InSampleRate) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) override;

protected:

	Audio::FEnvelopeFollower EnvelopeFollower;
	float CurrentEnvelopeValue;
	uint32 OwningPresetUniqueId;
	uint32 InstanceId;
	int32 FrameCount;
	int32 FramesToNotify;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnvelopeFollowerUpdate, float, EnvelopeValue);

class IEnvelopeFollowerNotifier
{
public:
	virtual void UnregisterEnvelopeFollowerListener(uint32 PresetUniqueId, UEnvelopeFollowerListener* EnvFollowerListener) = 0;
};

UCLASS(ClassGroup = Synth, hidecategories = (Object, ActorComponent, Physics, Rendering, Mobility, LOD), meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API UEnvelopeFollowerListener : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnvelopeFollowerListener(const FObjectInitializer& ObjInit)
		: Super(ObjInit)
		, bRegistered(false)
		, PresetUniqueId(INDEX_NONE)
		, EnvelopeFollowerNotifier(nullptr)
	{
	}

	~UEnvelopeFollowerListener()
	{
		if (EnvelopeFollowerNotifier)
		{
			check(PresetUniqueId != INDEX_NONE);
			EnvelopeFollowerNotifier->UnregisterEnvelopeFollowerListener(PresetUniqueId, this);
		}
	}

	UPROPERTY(BlueprintAssignable)
	FOnEnvelopeFollowerUpdate OnEnvelopeFollowerUpdate;

	void Init(IEnvelopeFollowerNotifier* InNotifier, uint32 InPresetUniqueId)
	{
		if (PresetUniqueId != INDEX_NONE)
		{
			check(InNotifier);
			InNotifier->UnregisterEnvelopeFollowerListener(PresetUniqueId, this);
		}
		PresetUniqueId = InPresetUniqueId;
		EnvelopeFollowerNotifier = InNotifier;
	}

protected:
	bool bRegistered;
	uint32 PresetUniqueId;
	IEnvelopeFollowerNotifier* EnvelopeFollowerNotifier;
};

UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectEnvelopeFollowerPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SourceEffectEnvelopeFollower)

	virtual FColor GetPresetColor() const override { return FColor(248.0f, 218.0f, 78.0f); }

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectEnvelopeFollowerSettings& InSettings);

	/** Adds a submix effect preset to the master submix. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects", meta = (WorldContext = "WorldContextObject"))
	void RegisterEnvelopeFollowerListener(UEnvelopeFollowerListener* EnvelopeFollowerListener);

	/** Adds a submix effect preset to the master submix. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects", meta = (WorldContext = "WorldContextObject"))
	void UnregisterEnvelopeFollowerListener(UEnvelopeFollowerListener* EnvelopeFollowerListener);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Effects")
	FSourceEffectEnvelopeFollowerSettings Settings;
};

