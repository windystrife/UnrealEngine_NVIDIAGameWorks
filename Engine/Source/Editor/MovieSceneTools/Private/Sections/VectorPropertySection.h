// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "PropertySection.h"

/**
 * An implementation of vector property sections.
 */
class FVectorPropertySection
	: public FPropertySection
{
public:

	/**
	* Creates a new vector property section.
	*
	* @param InSequencer The sequencer which is controlling this property section.
	* @param InObjectBinding The object binding for the object which owns the property that this section is animating.
	* @param InPropertyName The name of the property which is animated by this section.
	* @param InPropertyPath A string representing the path to the property which is animated by this section.
	* @param InSectionObject The section object which is being displayed and edited.
	* @param InDisplayName A display name for the section being displayed and edited.
	*/
	FVectorPropertySection(ISequencer* InSequencer, FGuid InObjectBinding, FName InPropertyName, const FString& InPropertyPath, UMovieSceneSection& InSectionObject, const FText& InDisplayName)
		: FPropertySection(InSequencer, InObjectBinding, InPropertyName, InPropertyPath, InSectionObject, InDisplayName)
	{
	}

public:

	//~ FPropertySectionInterface

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override;

private:

	TOptional<FVector4> GetPropertyValueAsVector4() const;

	TOptional<float> GetVectorXValue() const;
	TOptional<float> GetVectorYValue() const;
	TOptional<float> GetVectorZValue() const;
	TOptional<float> GetVectorWValue() const;

private:

	mutable int32 ChannelsUsed;
};
