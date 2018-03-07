// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNiagaraEmitterTrack.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitterHandleViewModel.h"
#include "NiagaraEmitterViewModel.h"
#include "MovieSceneNiagaraEmitterSection.h"
#include "MovieSceneNiagaraEmitterTrackInstance.h"

UMovieSceneNiagaraEmitterTrack::UMovieSceneNiagaraEmitterTrack(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

TSharedPtr<FNiagaraEmitterHandleViewModel> UMovieSceneNiagaraEmitterTrack::GetEmitterHandle()
{
	return EmitterHandleViewModel;
}

void UMovieSceneNiagaraEmitterTrack::SetEmitterHandle(TSharedRef<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel)
{
	EmitterHandleViewModel = InEmitterHandleViewModel;

	if (Sections.Num() == 0)
	{
		Sections.Add(NewObject<UMovieSceneNiagaraEmitterSection>(this));
	}
}

bool UMovieSceneNiagaraEmitterTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}

void UMovieSceneNiagaraEmitterTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}

bool UMovieSceneNiagaraEmitterTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}

const TArray<UMovieSceneSection*>& UMovieSceneNiagaraEmitterTrack::GetAllSections() const
{
	return Sections;
}

TRange<float> UMovieSceneNiagaraEmitterTrack::GetSectionBoundaries() const
{
	return Sections.Num() == 1
		? Sections[0]->GetRange()
		: TRange<float>(0, 0);
}

FName UMovieSceneNiagaraEmitterTrack::GetTrackName() const
{
	return EmitterHandleViewModel.IsValid()
		? EmitterHandleViewModel->GetName()
		: NAME_None;
}

