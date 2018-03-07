// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNiagaraEmitterSection.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandleViewModel.h"
#include "NiagaraEmitterViewModel.h"

#define LOCTEXT_NAMESPACE "MovieSceneNiagaraEmitterSection"

UMovieSceneNiagaraEmitterSection::UMovieSceneNiagaraEmitterSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BurstCurve = MakeShareable(new FBurstCurve(&Times, &BurstKeys));
}

TOptional<float> UMovieSceneNiagaraEmitterSection::GetKeyTime(FKeyHandle KeyHandle) const
{
	TOptional<float> Time;
	if (BurstCurve.IsValid())
	{
		auto Key = BurstCurve->GetKey(KeyHandle);
		Time = Key.IsSet()
			? TOptional<float>(Key->Time)
			: TOptional<float>();
	}
	return Time;
}

void UMovieSceneNiagaraEmitterSection::SetKeyTime(FKeyHandle KeyHandle, float Time)
{
	if (BurstCurve.IsValid())
	{
		BurstCurve->SetKeyTime(KeyHandle, Time);
	}
}

void UMovieSceneNiagaraEmitterSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (BurstCurve.IsValid())
	{
		for (TKeyTimeIterator<float> KeyIterator = BurstCurve->IterateKeys(); KeyIterator; ++KeyIterator)
		{
			if (TimeRange.Contains(*KeyIterator))
			{
				OutKeyHandles.Add(KeyIterator.GetKeyHandle());
			}
		}
	}
}

TSharedPtr<FNiagaraEmitterHandleViewModel> UMovieSceneNiagaraEmitterSection::GetEmitterHandle()
{
	return EmitterHandleViewModel;
}

void UMovieSceneNiagaraEmitterSection::SetEmitterHandle(TSharedRef<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel)
{
	EmitterHandleViewModel = InEmitterHandleViewModel;
}

TSharedPtr<FBurstCurve> UMovieSceneNiagaraEmitterSection::GetBurstCurve()
{
	return BurstCurve;
}

#undef LOCTEXT_NAMESPACE // MovieSceneNiagaraEmitterSection