// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/BoolPropertySection.h"
#include "Rendering/DrawElements.h"
#include "Curves/IntegralCurve.h"
#include "BoolKeyArea.h"
#include "SequencerSectionPainter.h"
#include "ISectionLayoutBuilder.h"
#include "EditorStyleSet.h"
#include "CommonMovieSceneTools.h"
#include "Sections/MovieSceneBoolSection.h"


void FBoolPropertySection::GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const
{
	UMovieSceneBoolSection* BoolSection = Cast<UMovieSceneBoolSection>( &SectionObject );
	TAttribute<TOptional<bool>> ExternalValue;
	if (CanGetPropertyValue())
	{
		ExternalValue.Bind(TAttribute<TOptional<bool>>::FGetter::CreateLambda([&]
		{
			return GetPropertyValue<bool>();
		}));
	}
	TSharedRef<FBoolKeyArea> KeyArea = MakeShareable( new FBoolKeyArea( BoolSection->GetCurve(), ExternalValue, BoolSection ) );
	LayoutBuilder.SetSectionAsKeyArea( KeyArea );
}

int32 FBoolPropertySection::OnPaintSection( FSequencerSectionPainter& Painter ) const
{
	// custom drawing for bool curves
	UMovieSceneBoolSection* BoolSection = Cast<UMovieSceneBoolSection>( &SectionObject );

	TArray<float> SectionSwitchTimes;

	const FTimeToPixel& TimeConverter = Painter.GetTimeConverter();

	// Add the start time
	const float StartTime = TimeConverter.PixelToTime(0.f);
	const float EndTime = TimeConverter.PixelToTime(Painter.SectionGeometry.GetLocalSize().X);
	
	SectionSwitchTimes.Add(StartTime);

	FIntegralCurve& BoolCurve = BoolSection->GetCurve();
	for ( TArray<FIntegralKey>::TConstIterator It( BoolCurve.GetKeyIterator() ); It; ++It )
	{
		float Time = It->Time;
		if ( Time > StartTime && Time < EndTime )
		{
			SectionSwitchTimes.Add( Time );
		}
	}

	SectionSwitchTimes.Add(EndTime);

	int32 LayerId = Painter.PaintSectionBackground();

	const ESlateDrawEffect DrawEffects = Painter.bParentEnabled
		? ESlateDrawEffect::None
		: ESlateDrawEffect::DisabledEffect;

	static const int32 Height = 5;
	const float VerticalOffset = Painter.SectionGeometry.GetLocalSize().Y * .5f - Height * .5f;

	const FSlateBrush* BoolOverlayBrush = FEditorStyle::GetBrush("Sequencer.Section.StripeOverlay");

	for ( int32 i = 0; i < SectionSwitchTimes.Num() - 1; ++i )
	{
		float ThisTime = SectionSwitchTimes[i];
		
		const FColor Color = BoolSection->Eval(ThisTime, false) ? FColor(0, 255, 0, 125) : FColor(255, 0, 0, 125);
		
		FVector2D StartPos(TimeConverter.TimeToPixel(ThisTime), VerticalOffset);
		FVector2D Size(TimeConverter.TimeToPixel(SectionSwitchTimes[i+1]) - StartPos.X, Height);

		FSlateDrawElement::MakeBox(
			Painter.DrawElements,
			LayerId + 1,
			Painter.SectionGeometry.ToPaintGeometry(StartPos, Size),
			BoolOverlayBrush,
			DrawEffects,
			Color
			);
	}

	return LayerId + 1;
}
