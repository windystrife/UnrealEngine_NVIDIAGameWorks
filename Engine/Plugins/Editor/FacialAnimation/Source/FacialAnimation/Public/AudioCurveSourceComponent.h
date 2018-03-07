// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/AudioComponent.h"
#include "Animation/CurveSourceInterface.h"

#include "AudioCurveSourceComponent.generated.h"

class UCurveTable;
class USoundWave;

/** An audio component that also provides curves to drive animation */
UCLASS(ClassGroup = Audio, Experimental, meta = (BlueprintSpawnableComponent))
class FACIALANIMATION_API UAudioCurveSourceComponent : public UAudioComponent, public ICurveSourceInterface
{
	GENERATED_BODY()

public:
	UAudioCurveSourceComponent();

	/** 
	 * Get the name that this curve source can be bound to by.
	 * Clients of this curve source will use this name to identify this source.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Curves)
	FName CurveSourceBindingName;

	/** Offset in time applied to audio position when evaluating curves */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Curves)
	float CurveSyncOffset;

public:
	/** UActorComponent interface */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** UAudioComponent interface */
	virtual void FadeIn(float FadeInDuration, float FadeVolumeLevel = 1.f, float StartTime = 0.f) override;
	virtual	void FadeOut(float FadeOutDuration, float FadeVolumeLevel) override;
	virtual void Play(float StartTime = 0.f) override;
	virtual void Stop() override;
	virtual bool IsPlaying() const override;

	/** ICurveSourceInterface interface */
	virtual FName GetBindingName_Implementation() const override;
	virtual float GetCurveValue_Implementation(FName CurveName) const override;
	virtual void GetCurves_Implementation(TArray<FNamedCurveValue>& OutCurve) const override;

private:
	/** Cache the curve parameters when playing back */
	void CacheCurveData();

	/** Internal handling of playback percentage */
	void HandlePlaybackPercent(const UAudioComponent* InComponent, const USoundWave* InSoundWave, const float InPlaybackPercentage);

private:
	/** Cached evaluation time from the last callback of OnPlaybackPercent */
	float CachedCurveEvalTime;

	/** Cached curve table from the last callback of OnPlaybackPercent */
	TWeakObjectPtr<class UCurveTable> CachedCurveTable;

	/** Preroll time we use to sync to curves */
	float CachedSyncPreRoll;

	/** Cached param for PlayInternal */
	float CachedStartTime;

	/** Cached param for PlayInternal */
	float CachedFadeInDuration;

	/** Cached param for PlayInternal */
	float CachedFadeVolumeLevel;

	/** Delay timer */
	float Delay;

	/** Cached duration */
	float CachedDuration;

	/** Cache looping flag */
	bool bCachedLooping;
};
