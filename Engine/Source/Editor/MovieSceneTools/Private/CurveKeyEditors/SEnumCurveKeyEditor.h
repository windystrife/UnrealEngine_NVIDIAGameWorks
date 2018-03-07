// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class ISequencer;
class UMovieSceneSection;
struct FIntegralCurve;

/**
 * A widget for editing a curve representing enum keys.
 */
class SEnumCurveKeyEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEnumCurveKeyEditor) {}

		/** The sequencer which is controlling this key editor. */
		SLATE_ARGUMENT(ISequencer*, Sequencer)

		/** The section that owns the data edited by this key editor. */
		SLATE_ARGUMENT(UMovieSceneSection*, OwningSection)

		/** The curve being edited by this curve editor. */
		SLATE_ARGUMENT(FIntegralCurve*, Curve)

		/** The UEnum type which provides options for the curve being edited. */
		SLATE_ARGUMENT(const UEnum*, Enum)

		/** Allows the value displayed and edited by this key editor to be supplied from an external source.  This
			is useful for curves on property tracks who's property value can change without changing the animation. */
		SLATE_ATTRIBUTE(TOptional<uint8>, ExternalValue)

	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

private:
	int32 OnGetCurrentValue() const;

	void OnComboSelectionChanged(int32 InSelectedItem, ESelectInfo::Type SelectInfo);

	ISequencer* Sequencer;
	UMovieSceneSection* OwningSection;
	FIntegralCurve* Curve;
	TAttribute<TOptional<uint8>> ExternalValue;
};
