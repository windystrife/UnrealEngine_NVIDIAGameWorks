// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Sequencer.h"

class USequencerSettings;

class SSequencerGotoBox
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSequencerGotoBox) { }
	SLATE_END_ARGS()

	/** Construct the widget. */
	void Construct(const FArguments& InArgs, const TSharedRef<FSequencer>& InSequencer, USequencerSettings& InSettings, const TSharedRef<INumericTypeInterface<float>>& InNumericTypeInterface);

	/** Toggle the widget's visibility. */
	void ToggleVisibility();

private:

	/** Callback for committed values in the entry box. */
	void HandleEntryBoxValueCommitted(float Value, ETextCommit::Type CommitType);

private:

	/** The border widget. */
	TSharedPtr<SWidget> Border;

	/** The entry box widget. */
	TSharedPtr<SNumericEntryBox<float>> EntryBox;

	/** The widget that focused prior to this goto box. */
	TWeakPtr<SWidget> LastFocusedWidget;

	/** Numeric type interface used for converting parsing and generating strings from numbers. */
	TSharedPtr<INumericTypeInterface<float>> NumericTypeInterface;

	/** The main sequencer interface. */
	TWeakPtr<FSequencer> SequencerPtr;

	/** Cached settings provided to the sequencer itself on creation. */
	USequencerSettings* Settings;
};
