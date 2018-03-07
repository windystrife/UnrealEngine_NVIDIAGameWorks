// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/VisibilityPropertySection.h"
#include "BoolKeyArea.h"
#include "Sections/MovieSceneBoolSection.h"
#include "ISectionLayoutBuilder.h"

void FVisibilityPropertySection::GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const
{
	UMovieSceneBoolSection* BoolSection = Cast<UMovieSceneBoolSection>( &SectionObject );
	TAttribute<TOptional<bool>> ExternalValue;
	if (CanGetPropertyValue())
	{
		ExternalValue.Bind(TAttribute<TOptional<bool>>::FGetter::CreateLambda([&]
		{
			TOptional<bool> BoolValue = GetPropertyValue<bool>();
			return BoolValue.IsSet()
				? TOptional<bool>(!BoolValue.GetValue())
				: TOptional<bool>();
		}));
	}
	TSharedRef<FBoolKeyArea> KeyArea = MakeShareable( new FBoolKeyArea( BoolSection->GetCurve(), ExternalValue, BoolSection ) );
	LayoutBuilder.SetSectionAsKeyArea( KeyArea );
}
