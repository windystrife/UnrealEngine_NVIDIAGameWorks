// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/SWidget.h"
#include "IntegralKeyArea.h"

class ISequencer;

/**
 * A key area for displaying and editing integral curves representing Booleans.
 */
class FBoolKeyArea
	: public FIntegralKeyArea<bool>
{
public:

	/**
	* Creates a new key area for editing bool curves.
	*
	* @param InCurve The integral curve which has the bool keys.
	* @param InOwningSection The section which owns the curve which is being displayed and edited by this area.
	*/
	FBoolKeyArea(FIntegralCurve& InCurve, UMovieSceneSection* InOwningSection)
		: FIntegralKeyArea(InCurve, InOwningSection)
	{ }

	/**
	* Creates a new key area for editing bool curves whose value can be overridden externally.
	*
	* @param InCurve The integral curve which has the bool keys.
	* @param InExternalValue An attribute which can provide an external value for this key area.  External values are
	*        useful for things like property tracks where the property value can change without changing the animation
	*        and we want to be able to key and update using the new property value.
	* @param InOwningSection The section which owns the curve which is being displayed and edited by this area.
	*/
	FBoolKeyArea(FIntegralCurve& InCurve, TAttribute<TOptional<bool>> ExternalValue, UMovieSceneSection* InOwningSection)
		: FIntegralKeyArea(InCurve, ExternalValue, InOwningSection)
	{ }

public:

	//~ FIntegralKeyArea interface

	virtual bool CanCreateKeyEditor() override;
	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override;

protected:

	//~ FIntegralKeyArea interface

	virtual bool ConvertCurveValueToIntegralType(int32 CurveValue) const override;
};
