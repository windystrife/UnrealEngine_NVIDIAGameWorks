// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "DSP/SampleBufferReader.h"
#include "DSP/SampleBuffer.h"
#include "Sound/SoundWave.h"
#include "SynthComponentWaveTable.generated.h"

UENUM(BlueprintType)
enum class ESamplePlayerSeekType : uint8
{
	FromBeginning,
	FromCurrentPosition,
	FromEnd,
	Count UMETA(Hidden)
};


// Called when a sample has finished loading into the sample player
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSampleLoaded);

// Called while a sample player is playing back. Indicates the playhead progress in percent and as absolute time value (within the file).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSamplePlaybackProgress, float, ProgressPercent, float, ProgressTimeSeconds);


UCLASS(ClassGroup = Synth, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USynthSamplePlayer : public USynthComponent
{
	GENERATED_BODY()

	USynthSamplePlayer(const FObjectInitializer& ObjInitializer);
	~USynthSamplePlayer();

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth|Preset")
	USoundWave* SoundWave;

	UPROPERTY(BlueprintAssignable, Category = "Synth|Components|Audio")
	FOnSampleLoaded OnSampleLoaded;

	UPROPERTY(BlueprintAssignable, Category = "Synth|Components|Audio")
	FOnSamplePlaybackProgress OnSamplePlaybackProgress;

	// This will override the current sound wave if one is set, stop audio, and reload the new sound wave
	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetSoundWave(USoundWave* InSoundWave);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetPitch(float InPitch, float TimeSec);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SeekToTime(float TimeSec, ESamplePlayerSeekType SeekType);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetScrubMode(bool bScrubMode);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetScrubTimeWidth(float InScrubTimeWidthSec);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	float GetSampleDuration() const;

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	float GetCurrentPlaybackProgressTime() const;

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	float GetCurrentPlaybackProgressPercent() const;

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	bool IsLoaded() const;

protected:
	Audio::FSampleBufferReader SampleBufferReader;
	Audio::FSampleBuffer SampleBuffer;
	Audio::FSoundWavePCMLoader SoundWaveLoader;

	float SampleDurationSec;
	float SamplePlaybackProgressSec;
};
