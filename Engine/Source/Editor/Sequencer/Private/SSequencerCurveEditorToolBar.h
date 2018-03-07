// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Sequencer.h"

class USequencerSettings;

/** Wraps and builds a toolbar which works with the SSequencerCurveEditor. */
class SSequencerCurveEditorToolBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SSequencerCurveEditorToolBar )
	{}

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedRef<FSequencer> InSequencer, TSharedPtr<FUICommandList> CurveEditorCommandList );

private:
	/** Makes the curve editor view options menu for the toolbar. */
	TSharedRef<SWidget> MakeCurveEditorViewOptionsMenu(TSharedPtr<FUICommandList> CurveEditorCommandList);
	/** Makes the curve editor curve options menu for the toolbar. */
	TSharedRef<SWidget> MakeCurveEditorCurveOptionsMenu(TSharedPtr<FUICommandList> CurveEditorCommandList);

	float OnGetValueSnapInterval() const;
	void OnValueSnapIntervalChanged(float InInterval);

	/** The sequencer which owns this widget. */
	TWeakPtr<FSequencer> Sequencer;
	/** Cached user-supplied settings object */
	USequencerSettings* SequencerSettings; 
};
