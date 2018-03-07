// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "PropertySection.h"

class FSequencerSectionPainter;

/**
* An implementation of bool property sections.
*/
class FBoolPropertySection
	: public FPropertySection
{
public:

	/**
	* Creates a new bool property section for editing non-property bool sections.
	*
	* @param InSectionObject The section object which is being displayed and edited.
	* @param InDisplayName A display name for the section being displayed and edited.
	* @TODO Make another section which is a good base class for non-property bool sections.
	*/
	FBoolPropertySection(UMovieSceneSection& InSectionObject, const FText& InDisplayName)
		: FPropertySection(InSectionObject, InDisplayName)
	{
	}

	/**
	* Creates a new bool property section.
	*
	* @param InSequencer The sequencer which is controlling this property section.
	* @param InObjectBinding The object binding for the object which owns the property that this section is animating.
	* @param InPropertyName The name of the property which is animated by this section.
	* @param InPropertyPath A string representing the path to the property which is animated by this section.
	* @param InSectionObject The section object which is being displayed and edited.
	* @param InDisplayName A display name for the section being displayed and edited.
	*/
	FBoolPropertySection(ISequencer* InSequencer, FGuid InObjectBinding, FName InPropertyName, const FString& InPropertyPath, UMovieSceneSection& InSectionObject, const FText& InDisplayName)
		: FPropertySection(InSequencer, InObjectBinding, InPropertyName, InPropertyPath, InSectionObject, InDisplayName)
	{
	}

public:

	//~ FPropertySection interface

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override;
	virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override;
};
