// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertySection.h"

/**
 * A movie scene section for material parameters.
 */
class FParameterSection
	: public FPropertySection
{
public:

	FParameterSection(UMovieSceneSection& InSectionObject, const FText& SectionName)
		: FPropertySection(InSectionObject, SectionName)
	{ }

public:

	//~ ISequencerSection interface

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override;
	virtual bool RequestDeleteCategory(const TArray<FName>& CategoryNamePath) override;
	virtual bool RequestDeleteKeyArea(const TArray<FName>& KeyAreaNamePath) override;
};
