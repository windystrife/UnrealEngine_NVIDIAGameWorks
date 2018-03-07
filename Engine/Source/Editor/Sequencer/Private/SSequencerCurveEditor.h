// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "ITimeSlider.h"
#include "Sequencer.h"
#include "SCurveEditor.h"

class FSequencerCurveOwner;
class USequencerSettings;

/** A widget for displaying and managing an SCurveEditor in sequencer. */
class SSequencerCurveEditor : public SCurveEditor
{
public:
	SLATE_BEGIN_ARGS( SSequencerCurveEditor )
	{
		_Clipping = EWidgetClipping::ClipToBounds;
	}

		/** The range of time being viewed */
		SLATE_ATTRIBUTE( FAnimatedRange, ViewRange )

		/** Event that is dispatched when the view range needs to be changed */
		SLATE_EVENT( FOnViewRangeChanged, OnViewRangeChanged )

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedRef<FSequencer> InSequencer, TSharedRef<class ITimeSliderController> InTimeSliderController );

	/** Sets the sequencer node tree which supplies the curves. */
	void SetSequencerNodeTree( TSharedPtr<FSequencerNodeTree> InSequencerNodeTree );

	~SSequencerCurveEditor();

protected:

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

private:
	/** Builds and assigns a new curve owner to the FCurveEditor. */
	void UpdateCurveOwner();

	/** Gets whether or not snapping is enabled for the input of the curve. */
	bool GetInputCurveSnapEnabled() const;

	/** Gets time snapping interval. */
	float GetCurveTimeSnapInterval() const;

	/** Gets the value snapping interval. */
	float GetCurveValueSnapInterval() const;

	/** Gets whether to show time in frames.*/
	bool GetShowTimeInFrames() const;

	/** Run whenever the selection on the FSequencerNodeTree changes. */
	void NodeTreeSelectionChanged();

	/** Run whenever the selected curve visibility changes. */
	void OnCurveEditorCurveVisibilityChanged();

	/** Update the curve view model selection */
	void UpdateCurveViewModelSelection();

	/* Get the curves to will be used during a fit operation */
	virtual TArray<FRichCurve*> GetCurvesToFit()const override;

private:

	/** The sequencer which owns this widget. */
	TWeakPtr<FSequencer> Sequencer;
	/** Cached user-supplied settings object */
	USequencerSettings* SequencerSettings; 
	/** The class responsible for time sliding on the curve editor */
	TSharedPtr<class ITimeSliderController> TimeSliderController;
	/** The sequencer node tree which contains the key area nodes which supply the curves to edit. */
	TSharedPtr<FSequencerNodeTree> SequencerNodeTree;
	/** The sequencer curve owner implementation which is visualized by the SCurveEditor. */
	TSharedPtr<FSequencerCurveOwner> CurveOwner;
	/** A handle to remove the node tree selection changed delegate. */
	FDelegateHandle NodeTreeSelectionChangedHandle;
};
