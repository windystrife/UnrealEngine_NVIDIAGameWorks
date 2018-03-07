// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class ISequencer;
class UMovieSceneSection;
struct FRichCurve;

/**
 * A widget for editing a curve representing float keys.
 */
class SFloatCurveKeyEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFloatCurveKeyEditor) {}

		/** The sequencer which is controlling this key editor. */
		SLATE_ARGUMENT(ISequencer*, Sequencer)

		/** The section that owns the data edited by this key editor. */
		SLATE_ARGUMENT(UMovieSceneSection*, OwningSection)

		/** The curve being edited by this curve editor. */
		SLATE_ARGUMENT(FRichCurve*, Curve)

		/** Allows the value displayed and edited by this key editor to be supplied from an external source.  This
			is useful for curves on property tracks who's property value can change without changing the animation. */
		SLATE_ATTRIBUTE(TOptional<float>, ExternalValue)

	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

private:
	void OnBeginSliderMovement();

	void OnEndSliderMovement(float Value);

	float OnGetKeyValue() const;

	void OnValueChanged(float Value);
	void OnValueCommitted(float Value, ETextCommit::Type CommitInfo);

	void SetValue(float Value);

	ISequencer* Sequencer;
	UMovieSceneSection* OwningSection;
	FRichCurve* Curve;
	TAttribute<TOptional<float>> ExternalValue;
};
