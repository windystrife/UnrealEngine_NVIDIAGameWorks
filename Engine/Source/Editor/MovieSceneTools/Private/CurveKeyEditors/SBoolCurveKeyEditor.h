// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class ISequencer;
class UMovieSceneSection;
struct FIntegralCurve;
enum class ECheckBoxState : uint8;

/**
 * A widget for editing a curve representing bool keys.
 */
class SBoolCurveKeyEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBoolCurveKeyEditor) {}

		/** The sequencer which is controlling this key editor. */
		SLATE_ARGUMENT(ISequencer*, Sequencer)

		/** The section that owns the data edited by this key editor. */
		SLATE_ARGUMENT(UMovieSceneSection*, OwningSection)

		/** The curve being edited by this curve editor. */
		SLATE_ARGUMENT(FIntegralCurve*, Curve)

		/** Allows the value displayed and edited by this key editor to be supplied from an external source.  This
			is useful for curves on property tracks who's property value can change without changing the animation. */
		SLATE_ATTRIBUTE(TOptional<bool>, ExternalValue)

	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

private:

	ECheckBoxState IsChecked() const;
	void OnCheckStateChanged(ECheckBoxState);

	ISequencer* Sequencer;
	UMovieSceneSection* OwningSection;
	FIntegralCurve* Curve;
	TAttribute<TOptional<bool>> ExternalValue;
};
