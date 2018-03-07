// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Misc/Attribute.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Curves/KeyHandle.h"
#include "PropertySection.h"
#include "Widgets/Layout/SSpacer.h"
#include "IntegralKeyArea.h"

/** A key area for displaying and editing an actor reference property. */
class FActorReferenceKeyArea
	: public FIntegralCurveKeyAreaBase
{
public:

	/**
	 * Creates a new actor reference key area.
	 *
	 * @param InCurve The integral curve which has the actor reference keys.
	 * @param InExternalValue An attribute which can provide an external value for this key area.  External values are
	 *        useful for things like property tracks where the property value can change without changing the animation
	 *        and we want to be able to key and update using the new property value.
	 * @param InOwningSection The section which owns the curve which is being displayed and edited by this area.
	 */
	FActorReferenceKeyArea(FIntegralCurve& InCurve, TAttribute<TOptional<FGuid>> InExternalValue, UMovieSceneSection* InOwningSection)
		: FIntegralCurveKeyAreaBase(InCurve, InOwningSection)
		, ExternalValue(InExternalValue)
	{ }

public:

	//~ IKeyArea overrides

	virtual bool CanCreateKeyEditor() override { return false; }
	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override { return SNew(SSpacer); }
	virtual void CopyKeys(FMovieSceneClipboardBuilder& ClipboardBuilder, const TFunctionRef<bool(FKeyHandle, const IKeyArea&)>& KeyMask) const override;
	virtual void PasteKeys(const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment) override;


protected:

	//~ FIntegralCurveKeyAreaBase interface

	virtual void EvaluateAndAddKey(float Time, float TimeToCopyFrom, FKeyHandle& CurrentKey) override;
	virtual void UpdateKeyWithExternalValue(float Time) override;

private:

	TAttribute<TOptional<FGuid>> ExternalValue;
};


/**
 * A property section for actor references.
 */
class FActorReferencePropertySection
	: public FPropertySection
{
public:

	/**
	* Creates a new actor reference property section.
	*
	* @param InSequencer The sequencer which is controlling this property section.
	* @param InObjectBinding The object binding for the object which owns the property that this section is animating.
	* @param InPropertyName The name of the property which is animated by this section.
	* @param InPropertyPath A string representing the path to the property which is animated by this section.
	* @param InSectionObject The section object which is being displayed and edited.
	* @param InDisplayName A display name for the section being displayed and edited.
	*/
	FActorReferencePropertySection(ISequencer* InSequencer, FGuid InObjectBinding, FName InPropertyName, const FString& InPropertyPath, UMovieSceneSection& InSectionObject, const FText& InDisplayName)
		: FPropertySection(InSequencer, InObjectBinding, InPropertyName, InPropertyPath, InSectionObject, InDisplayName)
	{
	}

public:

	//~ FPropertySection interface

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override;

private:

	TOptional<FGuid> GetActorGuid() const;
};
