// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "PropertySection.h"

/**
 * An implementation of byte property sections
 */
class FBytePropertySection
	: public FPropertySection
{
public:

	/**
	* Creates a new byte property section.
	*
	* @param InSequencer The sequencer which is controlling this property section.
	* @param InObjectBinding The object binding for the object which owns the property that this section is animating.
	* @param InPropertyName The name of the property which is animated by this section.
	* @param InPropertyPath A string representing the path to the property which is animated by this section.
	* @param InSectionObject The section object which is being displayed and edited.
	* @param InDisplayName A display name for the section being displayed and edited.
	* @param InEnum An optional enum parameter for byte tracks representing enum properties which is used display and populate the enum UI.
	*/
	FBytePropertySection(ISequencer* InSequencer, FGuid InObjectBinding, FName InPropertyName, const FString& InPropertyPath, UMovieSceneSection& InSectionObject, const FText& InDisplayName, UEnum* InEnum = nullptr)
		: FPropertySection(InSequencer, InObjectBinding, InPropertyName, InPropertyPath, InSectionObject, InDisplayName)
		, Enum(InEnum)
	{
	}

public:

	// FPropertySection interface

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override;

private:

	/** An optional UEnum for byte sections which animate an enum property. */
	UEnum* Enum;
};
