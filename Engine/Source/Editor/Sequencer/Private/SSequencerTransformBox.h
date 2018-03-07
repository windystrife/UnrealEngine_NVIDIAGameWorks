// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SNumericEntryBox.h"

class FSequencer;
class USequencerSettings;

class SSequencerTransformBox
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSequencerTransformBox) { }
	SLATE_END_ARGS()

	/** Construct the widget. */
	void Construct(const FArguments& InArgs, const TSharedRef<FSequencer>& InSequencer, USequencerSettings& InSettings, const TSharedRef<INumericTypeInterface<float>>& InNumericTypeInterface);

	/** Toggle the widget's visibility. */
	void ToggleVisibility();

private:

	/** Callback for transform operations. */
	FReply OnPlusButtonClicked();
	FReply OnMinusButtonClicked();
	FReply OnMultiplyButtonClicked();
	FReply OnDivideButtonClicked();
	void OnDeltaChanged(float Value, ETextCommit::Type CommitType);
	void OnScaleChanged(float Value, ETextCommit::Type CommitType);

	/** Callback for when the close button is clicked. */
	FReply OnCloseButtonClicked();

private:

	/** The border widget. */
	TSharedPtr<SWidget> Border;

	/** The entry box widget. */
	TSharedPtr<SNumericEntryBox<float>> EntryBox;

	/** The widget that focused prior to this transform box. */
	TWeakPtr<SWidget> LastFocusedWidget;

	/** Numeric type interface used for converting parsing and generating strings from numbers. */
	TSharedPtr<INumericTypeInterface<float>> NumericTypeInterface;

	/** The main sequencer interface. */
	TWeakPtr<FSequencer> SequencerPtr;

	/** Cached settings provided to the sequencer itself on creation. */
	USequencerSettings* Settings;

	/** Cached delta time. */
	float DeltaTime;

	/** Cached scale factor. */
	float ScaleFactor;
};
