// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "PropertySection.h"

class FSequencerSectionPainter;
class UMovieSceneColorSection;

/**
* A color section implementation
*/
class FColorPropertySection
	: public FPropertySection
{
public:

	/**
	* Creates a new color property section.
	*
	* @param InSequencer The sequencer which is controlling this property section.
	* @param InObjectBinding The object binding for the object which owns the property that this section is animating.
	* @param InPropertyName The name of the property which is animated by this section.
	* @param InPropertyPath A string representing the path to the property which is animated by this section.
	* @param InSectionObject The section object which is being displayed and edited.
	* @param InDisplayName A display name for the section being displayed and edited.
	*/
	FColorPropertySection(ISequencer* InSequencer, FGuid InObjectBinding, FName InPropertyName, const FString& InPropertyPath, UMovieSceneSection& InSectionObject, const FText& InDisplayName)
		: FPropertySection(InSequencer, InObjectBinding, InPropertyName, InPropertyPath, InSectionObject, InDisplayName)
	{
	}

public:

	// FPropertySection interface

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override;
	virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override;

protected:

	/** Consolidate color curves for all track sections. */
	void ConsolidateColorCurves(TArray< TKeyValuePair<float, FLinearColor> >& OutColorKeys, const UMovieSceneColorSection* Section) const;
	
	/** Gets current value of the property being edited as a linear color. */
	TOptional<FLinearColor> GetPropertyValueAsLinearColor() const;

	TOptional<float> GetColorRedValue() const;
	TOptional<float> GetColorGreenValue() const;
	TOptional<float> GetColorBlueValue() const;
	TOptional<float> GetColorAlphaValue() const;
};
