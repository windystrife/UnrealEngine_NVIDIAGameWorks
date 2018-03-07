// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterTrackEditor.h"
#include "NiagaraEmitterSection.h"
#include "NiagaraEmitter.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "Sequencer/NiagaraSequence/NiagaraSequence.h"

FNiagaraEmitterTrackEditor::FNiagaraEmitterTrackEditor(TSharedPtr<ISequencer> Sequencer) 
	: FMovieSceneTrackEditor(Sequencer.ToSharedRef())
{
}

TSharedRef<ISequencerTrackEditor> FNiagaraEmitterTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FNiagaraEmitterTrackEditor(InSequencer));
}

bool FNiagaraEmitterTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const 
{
	if (TrackClass == UMovieSceneNiagaraEmitterTrack::StaticClass())
	{
		return true;
	}
	return false;
}

TSharedRef<ISequencerSection> FNiagaraEmitterTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	return MakeShareable(new FNiagaraEmitterSection(SectionObject));
}

bool FNiagaraEmitterTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	UNiagaraEmitter* EmitterAsset = Cast<UNiagaraEmitter>(Asset);
	UNiagaraSequence* NiagaraSequence = Cast<UNiagaraSequence>(GetSequencer()->GetRootMovieSceneSequence());
	if (EmitterAsset != nullptr && NiagaraSequence != nullptr && NiagaraSequence->GetSystemViewModel().GetCanAddEmittersFromTimeline())
	{
		NiagaraSequence->GetSystemViewModel().AddEmitter(*EmitterAsset);
	}
	return false;
}