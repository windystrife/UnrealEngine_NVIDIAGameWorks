// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/SWidget.h"
#include "IntegralKeyArea.h"

class ISequencer;

/**
 * A key area for displaying and editing byte curves.
 */
class FByteKeyArea
	: public FIntegralKeyArea<uint8>
{
public:

	/**
	* Creates a new key area for editing byte curves.
	*
	* @param InCurve The integral curve which has the byte keys.
	* @param InOwningSection The section which owns the curve which is being displayed and edited by this area.
	*/
	FByteKeyArea(FIntegralCurve& InCurve, UMovieSceneSection* InOwningSection)
		: FIntegralKeyArea(InCurve, InOwningSection)
	{ }

	/**
	* Creates a new key area for editing byte curves whose value can be overridden externally.
	*
	* @param InCurve The integral curve which has the byte keys.
	* @param InExternalValue An attribute which can provide an external value for this key area.  External values are
	*        useful for things like property tracks where the property value can change without changing the animation
	*        and we want to be able to key and update using the new property value.
	* @param InOwningSection The section which owns the curve which is being displayed and edited by this area.
	*/
	FByteKeyArea(FIntegralCurve& InCurve, TAttribute<TOptional<uint8>> ExternalValue, UMovieSceneSection* InOwningSection)
		: FIntegralKeyArea(InCurve, ExternalValue, InOwningSection)
	{ }

public:

	//~ FIntegralKeyArea interface

	virtual bool CanCreateKeyEditor() override;
	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override;

protected:

	//~ FIntegralKeyArea interface

	virtual uint8 ConvertCurveValueToIntegralType(int32 CurveValue) const override;
};
