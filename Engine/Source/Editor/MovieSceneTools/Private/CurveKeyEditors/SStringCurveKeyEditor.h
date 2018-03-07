// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class ISequencer;
class UMovieSceneSection;
struct FStringCurve;

/**
 * A widget for editing a curve representing string keys.
 */
class SStringCurveKeyEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SStringCurveKeyEditor) {}

		/** The sequencer which is controlling this key editor. */
		SLATE_ARGUMENT(ISequencer*, Sequencer)

		/** The section that owns the data edited by this key editor. */
		SLATE_ARGUMENT(UMovieSceneSection*, OwningSection)

		/** The curve being edited by this curve editor. */
		SLATE_ARGUMENT(FStringCurve*, Curve)

		/** Allows the value displayed and edited by this key editor to be supplied from an external source.  This
		is useful for curves on property tracks who's property value can change without changing the animation. */
		SLATE_ATTRIBUTE(TOptional<FString>, ExternalValue)

	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

private:

	FText GetText() const;
	void OnTextCommitted(const FText& InText, ETextCommit::Type InCommitType);

	ISequencer* Sequencer;
	UMovieSceneSection* OwningSection;
	FStringCurve* Curve;
	TAttribute<TOptional<FString>> ExternalValue;
};
