// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "DSP/Granulator.h"
#include "Sound/SoundWave.h"
#include "SynthComponentGranulator.generated.h"


UENUM(BlueprintType)
enum class EGranularSynthEnvelopeType : uint8
{
	Rectangular,
	Triangle,
	DownwardTriangle,
	UpwardTriangle,
	ExponentialDecay,
	ExponentialIncrease,
	Gaussian,
	Hanning,
	Lanczos,
	Cosine,
	CosineSquared,
	Welch,
	Blackman,
	BlackmanHarris,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EGranularSynthSeekType : uint8
{
	FromBeginning,
	FromCurrentPosition,
	Count UMETA(Hidden)
};


UCLASS(ClassGroup = Synth, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API UGranularSynth : public USynthComponent
{
	GENERATED_BODY()

	UGranularSynth(const FObjectInitializer& ObjInitializer);
	~UGranularSynth();

	// Initialize the synth component
	virtual void Init(const int32 SampleRate) override;

	// Called to generate more audio
	virtual void OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

	//~ Begin ActorComponent Interface.
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	//~ End ActorComponent Interface

public:

	// This will override the current sound wave if one is set, stop audio, and reload the new sound wave
	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetSoundWave(USoundWave* InSoundWave);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetAttackTime(const float AttackTimeMsec);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetDecayTime(const float DecayTimeMsec);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetSustainGain(const float SustainGain);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetReleaseTimeMsec(const float ReleaseTimeMsec);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void NoteOn(const float Note, const int32 Velocity, const float Duration = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void NoteOff(const float Note, const bool bKill = false);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetGrainsPerSecond(const float InGrainsPerSecond);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetGrainProbability(const float InGrainProbability);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetGrainEnvelopeType(const EGranularSynthEnvelopeType EnvelopeType);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetPlaybackSpeed(const float InPlayheadRate);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetGrainPitch(const float BasePitch, const FVector2D PitchRange = FVector2D::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetGrainVolume(const float BaseVolume, const FVector2D VolumeRange = FVector2D::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetGrainPan(const float BasePan, const FVector2D PanRange = FVector2D::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetGrainDuration(const float BaseDurationMsec, const FVector2D DurationRange = FVector2D::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	float GetSampleDuration() const;

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetScrubMode(const bool bScrubMode);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetPlayheadTime(const float InPositionSec, const float LerpTimeSec = 0.0f, EGranularSynthSeekType SeekType = EGranularSynthSeekType::FromBeginning);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	float GetCurrentPlayheadTime() const;

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	bool IsLoaded() const;

protected:

	TQueue<USoundWave*> PendingStoppingSoundWaves;

	Audio::FGranularSynth GranularSynth;
	Audio::FSoundWavePCMLoader SoundWaveLoader;

	bool bIsLoaded;
	bool bRegistered;

	bool bIsLoading;
};