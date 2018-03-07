// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerSection.h"

class UMovieSceneNiagaraEmitterSection;

/**
*	Visual representation of UMovieSceneNiagaraEmitterSection
*/
class FNiagaraEmitterSection : public ISequencerSection
{
public:
	FNiagaraEmitterSection(UMovieSceneSection &SectionObject);

	//~ ISequencerSection interface
	virtual UMovieSceneSection* GetSectionObject(void) override;
	virtual int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override;
	virtual FText GetSectionTitle(void) const override;
	virtual float GetSectionHeight() const override { return 20.0f; }
	virtual void GenerateSectionLayout(ISectionLayoutBuilder &) const override;

private:
	UMovieSceneNiagaraEmitterSection* EmitterSection;
};