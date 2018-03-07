// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/IntegerPropertySection.h"
#include "IntegerKeyArea.h"
#include "Sections/MovieSceneIntegerSection.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "ISectionLayoutBuilder.h"

void FIntegerPropertySection::GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const
{
	UMovieSceneIntegerSection* IntegerSection = Cast<UMovieSceneIntegerSection>(&SectionObject);
	TAttribute<TOptional<int32>> ExternalValue;
	if (CanGetPropertyValue())
	{
		ExternalValue.Bind(TAttribute<TOptional<int32>>::FGetter::CreateLambda([&]
		{
			return GetPropertyValue<int32>();
		}));
	}
	TSharedRef<FIntegerKeyArea> KeyArea = MakeShareable(
		new FIntegerKeyArea(IntegerSection->GetCurve(), ExternalValue, IntegerSection));
	LayoutBuilder.SetSectionAsKeyArea(KeyArea);
}
