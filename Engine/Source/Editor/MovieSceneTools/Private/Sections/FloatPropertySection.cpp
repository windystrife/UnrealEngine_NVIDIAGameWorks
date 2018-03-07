// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/FloatPropertySection.h"
#include "ISectionLayoutBuilder.h"
#include "FloatCurveKeyArea.h"
#include "Sections/MovieSceneFloatSection.h"

void FFloatPropertySection::GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const
{
	UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(&SectionObject);
	TAttribute<TOptional<float>> ExternalValue;
	if (CanGetPropertyValue())
	{
		ExternalValue.Bind(TAttribute<TOptional<float>>::FGetter::CreateLambda([&]
		{
			return GetPropertyValue<float>();
		}));
	}
	TSharedRef<FFloatCurveKeyArea> KeyArea = MakeShareable(
		new FFloatCurveKeyArea(&FloatSection->GetFloatCurve(), ExternalValue, FloatSection));
	LayoutBuilder.SetSectionAsKeyArea(KeyArea);
}
