// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertySection.h"

/** A property section for FPostMoveSettings properties. */
class FComposurePostMoveSettingsPropertySection
	: public FPropertySection
{
public:

	FComposurePostMoveSettingsPropertySection(ISequencer* InSequencer, FGuid InObjectBinding, FName InPropertyName, const FString& InPropertyPath, UMovieSceneSection& InSectionObject, const FText& SectionName);

	//~ ISequencerSection interface.
	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override;

private:

	TOptional<float> GetSizeReferenceXValue() const;
	TOptional<float> GetSizeReferenceYValue() const;

	TOptional<float> GetPivotXValue() const;
	TOptional<float> GetPivotYValue() const;

	TOptional<float> GetTranslationXValue() const;
	TOptional<float> GetTranslationYValue() const;

	TOptional<float> GetRotationValue() const;

	TOptional<float> GetScaleValue() const;
};