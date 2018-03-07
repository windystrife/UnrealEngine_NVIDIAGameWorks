// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "ISequencerSection.h"
#include "MovieSceneCommonHelpers.h"
#include "ISequencer.h"

class FSequencerSectionPainter;

/**
 * A generic implementation for displaying simple property sections
 */
class MOVIESCENETOOLS_API FPropertySection
	: public ISequencerSection
{
public:

	/**
	* Creates a new property section for editing non-property sections.
	*
	* @param InSectionObject The section object which is being displayed and edited.
	* @param InDisplayName A display name for the section being displayed and edited.
	* @TODO Make another section which is a good base class for non-property sections.
	*/
	FPropertySection(UMovieSceneSection& InSectionObject, const FText& InDisplayName);

	/**
	* Creates a new property section.
	*
	* @param InSequencer The sequencer which is controlling this property section.
	* @param InObjectBinding The object binding for the object which owns the property that this section is animating.
	* @param InPropertyName The name of the property which is animated by this section.
	* @param InPropertyPath A string representing the path to the property which is animated by this section.
	* @param InSectionObject The section object which is being displayed and edited.
	* @param InDisplayName A display name for the section being displayed and edited.
	*/
	FPropertySection(ISequencer* InSequencer, FGuid InObjectBinding, FName InPropertyName, const FString& InPropertyPath, UMovieSceneSection& InSectionObject, const FText& InDisplayName);

public:

	// ISequencerSection interface

	virtual UMovieSceneSection* GetSectionObject() override;
	virtual FText GetSectionTitle() const override;
	virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override;

protected:

	/** Gets the sequencer which is controlling this section .*/
	ISequencer* GetSequencer() const { return Sequencer; }

	/** Gets the property being animated by this section.  Returns nullptr if this section was not constructed with the necessary
		data to get the property value, or if the runtime object or property can not be found. */
	UProperty* GetProperty() const;

	/** Gets the current value of the property which is being animated.  The optional return value will be unset if
		this section was not constructed with the necessary data, or is the runtime object or property can not be found. */
	template<typename ValueType>
	TOptional<ValueType> GetPropertyValue() const
	{
		if (!PropertyBindings.IsValid())
		{
			return TOptional<ValueType>();
		}

		{
			for (const TWeakObjectPtr<UObject>& WeakObject : Sequencer->FindBoundObjects(ObjectBinding, Sequencer->GetFocusedTemplateID()))
			{
				if (UObject* Object = WeakObject.Get())
				{
					return PropertyBindings->GetCurrentValue<ValueType>(*Object);
				}
			}
		}

		return TOptional<ValueType>();
	}

	/** Returns true when this section was constructed with the data necessary to query for the current property value. */
	bool CanGetPropertyValue() const;

protected:

	/** Display name of the section */
	FText DisplayName;

	/** The section we are visualizing */
	UMovieSceneSection& SectionObject;

private:

	/** The sequencer which is controlling this section. */
	ISequencer* Sequencer;

	/** An object binding for the object which owns the property being animated by this section. */
	FGuid ObjectBinding;

	/** An object which is used to retrieve the value of a property based on it's name and path. */
	mutable TSharedPtr<FTrackInstancePropertyBindings> PropertyBindings;
};

