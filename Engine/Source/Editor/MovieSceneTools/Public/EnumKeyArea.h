// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/SWidget.h"
#include "ByteKeyArea.h"

class ISequencer;

/**
 * A key area for displaying and editing integral curves representing enums.
 */
class FEnumKeyArea
	: public FByteKeyArea
{
public:

	/**
	* Creates a new key area for editing enum curves.
	*
	* @param InCurve The integral curve which has the enum keys.
	* @param InOwningSection The section which owns the curve which is being displayed and edited by this area.
	* @param InEnum The enum which is used to generate display values for this area.
	*/
	FEnumKeyArea(FIntegralCurve& InCurve, UMovieSceneSection* InOwningSection, const UEnum* InEnum)
		: FByteKeyArea(InCurve, InOwningSection)
		, Enum(InEnum)
	{ }

	/**
	* Creates a new key area for editing enum curves whose value can be overridden externally.
	*
	* @param InCurve The integral curve which has the enum keys.
	* @param InExternalValue An attribute which can provide an external value for this key area.  External values are
	*        useful for things like property tracks where the property value can change without changing the animation
	*        and we want to be able to key and update using the new property value.
	* @param InOwningSection The section which owns the curve which is being displayed and edited by this area.
	* @param InEnum The enum which is used to generate display values for this area.
	*/
	FEnumKeyArea(FIntegralCurve& InCurve, TAttribute<TOptional<uint8>> ExternalValue, UMovieSceneSection* InOwningSection, const UEnum* InEnum)
		: FByteKeyArea(InCurve, ExternalValue, InOwningSection)
		, Enum(InEnum)
	{ }

public:

	//~ FByteKeyArea interface

	virtual bool CanCreateKeyEditor() override;
	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override;

private:

	/** The enum which provides available integral values for this key area. */
	const UEnum* Enum;
};
