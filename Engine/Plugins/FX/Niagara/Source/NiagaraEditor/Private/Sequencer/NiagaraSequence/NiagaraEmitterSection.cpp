// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterSection.h"
#include "MovieSceneNiagaraEmitterSection.h"
#include "NiagaraEmitterHandleViewModel.h"
#include "NiagaraEmitterViewModel.h"

#include "SequencerSectionPainter.h"
#include "GenericKeyArea.h"
#include "ISectionLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "NiagaraEmitterSection"

FNiagaraEmitterSection::FNiagaraEmitterSection(UMovieSceneSection &SectionObject)
{
	UMovieSceneNiagaraEmitterSection *NiagaraSectionObject = Cast<UMovieSceneNiagaraEmitterSection>(&SectionObject);
	check(NiagaraSectionObject);

	EmitterSection = NiagaraSectionObject;
}

UMovieSceneSection* FNiagaraEmitterSection::GetSectionObject(void)
{
	return EmitterSection;
}

int32 FNiagaraEmitterSection::OnPaintSection(FSequencerSectionPainter& InPainter) const
{
	// draw the first run of the emitter
	FSlateDrawElement::MakeBox
	(
		InPainter.DrawElements,
		InPainter.LayerId,
		InPainter.SectionGeometry.ToPaintGeometry(),
		FEditorStyle::GetBrush("CurveEd.TimelineArea"),
		ESlateDrawEffect::None,
		FLinearColor(0.3f, 0.3f, 0.6f)
	);

	// draw all loops of the emitter as 'ghosts' of the original section
	float X = InPainter.SectionGeometry.AbsolutePosition.X;
	float GeomW = InPainter.SectionGeometry.GetDrawSize().X;
	int32 NumLoops = EmitterSection->GetEmitterHandle()->GetEmitterViewModel()->GetNumLoops() - 1;
	for (int32 Loop = 0; Loop < NumLoops; Loop++)
	{
		FSlateDrawElement::MakeBox
		(
			InPainter.DrawElements,
			InPainter.LayerId,
			InPainter.SectionGeometry.ToPaintGeometry(FVector2D(GeomW*(Loop + 1), 0.0f), InPainter.SectionGeometry.GetDrawSize(), 1.0f),
			FEditorStyle::GetBrush("CurveEd.TimelineArea"),
			ESlateDrawEffect::None,
			FLinearColor(0.3f, 0.3f, 0.6f, 0.25f)
		);
	}

	return InPainter.LayerId;
}

FText FNiagaraEmitterSection::GetSectionTitle(void) const
{
	return EmitterSection->GetEmitterHandle()->GetNameText();
}

void FNiagaraEmitterSection::GenerateSectionLayout(ISectionLayoutBuilder &LayoutBuilder) const
{
	if (EmitterSection->GetBurstCurve().IsValid())
	{
		auto KeyArea = MakeShared<TGenericKeyArea<FMovieSceneBurstKey, float>>(MoveTemp(*EmitterSection->GetBurstCurve()), EmitterSection);
		LayoutBuilder.SetSectionAsKeyArea(KeyArea);
	}
}

#undef LOCTEXT_NAMESPACE
