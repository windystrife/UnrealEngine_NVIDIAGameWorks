// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/StringPropertySection.h"
#include "ISectionLayoutBuilder.h"
#include "StringCurveKeyArea.h"
#include "Sections/MovieSceneStringSection.h"


void FStringPropertySection::GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const
{
	UMovieSceneStringSection* StringSection = Cast<UMovieSceneStringSection>(&SectionObject);

	TAttribute<TOptional<FString>> ExternalValue;
	ExternalValue.Bind(TAttribute<TOptional<FString>>::FGetter::CreateLambda([&] { return GetPropertyValue<FString>(); }));

	TSharedRef<FStringCurveKeyArea> KeyArea = MakeShareable(new FStringCurveKeyArea(&StringSection->GetStringCurve(), ExternalValue, StringSection));
	LayoutBuilder.SetSectionAsKeyArea(KeyArea);
}
