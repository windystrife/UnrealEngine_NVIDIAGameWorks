// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraEmitter.h"
#include "MovieSceneSection.h"
#include "MovieSceneKeyStruct.h"
#include "Curves/CurveInterface.h"
#include "MovieSceneClipboard.h"
#include "MovieSceneNiagaraEmitterSection.generated.h"

class FNiagaraEmitterHandleViewModel;

/** Defines data for burst keys in this emitter section. */
USTRUCT()
struct FMovieSceneBurstKey
{
	GENERATED_BODY()

	FMovieSceneBurstKey()
		: TimeRange(0)
		, SpawnMinimum(0)
		, SpawnMaximum(0)
	{
	}

	/** The time range which will be used around the key time which is used for randomly bursting. */
	UPROPERTY(EditAnywhere, Category = "Burst")
	float TimeRange;

	/** The minimum number of particles to spawn with this burst. */
	UPROPERTY(EditAnywhere, Category = "Burst")
	uint32 SpawnMinimum;

	/** The maximum number of particles to span with this burst. */
	UPROPERTY(EditAnywhere, Category = "Burst")
	uint32 SpawnMaximum;
};

typedef TCurveInterface<FMovieSceneBurstKey, float> FBurstCurve;



namespace MovieSceneClipboard
{
	template<> inline FName GetKeyTypeName<FMovieSceneBurstKey>()
	{
		return "FMovieSceneBurstKey";
	}
}

/** 
 *	Niagara editor movie scene section; represents one emitter in the timeline
 */
UCLASS()
class UMovieSceneNiagaraEmitterSection : public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()
public:
	//~ UMovieSceneSection interface
	virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;

	/** Gets the emitter handle for the emitter which this section represents. */
	TSharedPtr<FNiagaraEmitterHandleViewModel> GetEmitterHandle();

	/** Sets the emitter handle for the emitter which this section represents. */
	void SetEmitterHandle(TSharedRef<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel);

	/** Gets the curve containing the burst keyframes. */
	TSharedPtr<FBurstCurve> GetBurstCurve();

private:
	/** The view model for the handle to the emitter this section represents. */
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel;

	/** Storage for the key times in the burst curve. */
	TArray<float> Times;

	/** Storage for the burst value times in the burst curve. */
	TArray<FMovieSceneBurstKey> BurstKeys;

	/** A curve containing the burst keyframes. */
	TSharedPtr<FBurstCurve> BurstCurve;
};