// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "PropertySection.h"

/**
 * An implementation of vector property sections.
 */
class FTransformSection : public ISequencerSection
{
public:

	/**
	* Creates a new transform property section.
	*
	* @param InSection The section object which is being displayed and edited.
	* @param InSequencer The sequencer which is controlling this property section.
	* @param InObjectBinding The object binding for the object which owns the property that this section is animating.
	*/
	FTransformSection(TWeakObjectPtr<UMovieSceneSection> InSection, TWeakPtr<ISequencer> InSequencer, FGuid InObjectBinding)
		: Section(InSection), WeakSequencer(InSequencer), ObjectBinding(InObjectBinding)
	{
	}

	/** Assign a property to this section */
	void AssignProperty(FName PropertyName, const FString& PropertyPath);

public:

	virtual UMovieSceneSection* GetSectionObject() override { return Section.Get(); }
	virtual int32 OnPaintSection( FSequencerSectionPainter& InPainter ) const override;
	virtual void GenerateSectionLayout(ISectionLayoutBuilder& LayoutBuilder) const override;
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& InObjectBinding) override;

protected:

	virtual TOptional<FTransform> GetCurrentValue() const;

protected:

	TOptional<float> GetTranslationValue(EAxis::Type Axis) const;
	TOptional<float> GetRotationValue(EAxis::Type Axis) const;
	TOptional<float> GetScaleValue(EAxis::Type Axis) const;

protected:

	/** The section that we are editing */
	TWeakObjectPtr<UMovieSceneSection> Section;

	/** The sequencer which is controlling this section. */
	TWeakPtr<ISequencer> WeakSequencer;

	/** An object binding for the object which owns the property being animated by this section. */
	FGuid ObjectBinding;

	/** An object which is used to retrieve the value of a property based on it's name and path. */
	mutable TOptional<FTrackInstancePropertyBindings> PropertyBindings;
};
