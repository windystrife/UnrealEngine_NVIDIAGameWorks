// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "SCurveEditor.h"

class FPaintArgs;
class FSlateWindowElementList;

/**
 * A Slate SpinBox resembles traditional spin boxes in that it is a widget that provides
 * keyboard-based and mouse-based manipulation of a numberic value.
 * Mouse-based manipulation: drag anywhere on the spinbox to change the value.
 * Keyboard-based manipulation: click on the spinbox to enter text mode.
 */
DECLARE_DELEGATE_TwoParams( FOnCropAnimSequence, bool, float )
DECLARE_DELEGATE_TwoParams( FOnAddAnimSequence, bool, int32 )
DECLARE_DELEGATE_TwoParams( FOnAppendAnimSequence, bool, int32 )
DECLARE_DELEGATE_TwoParams( FOnScrubBarDrag, int32, float)
DECLARE_DELEGATE_OneParam( FOnReZeroAnimSequence, int32 )

class KISMETWIDGETS_API SScrubWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SScrubWidget)
		: _Value(0)
		, _NumOfKeys(10)
		, _SequenceLength()
		, _bAllowZoom(false)
		, _DisplayDrag(true)
		, _OnValueChanged()
		, _OnBeginSliderMovement()
		, _OnEndSliderMovement()
		, _ViewInputMin()
		, _ViewInputMax()
		, _OnSetInputViewRange()
		, _OnCropAnimSequence()
		{}
		
		/** The value to display */
		SLATE_ATTRIBUTE( float, Value )
		SLATE_ATTRIBUTE( uint32, NumOfKeys )
		SLATE_ATTRIBUTE( float, SequenceLength )
		SLATE_ARGUMENT( bool, bAllowZoom )
		SLATE_ATTRIBUTE( bool, DisplayDrag )
		/** Called when the value is changed by slider or typing */
		SLATE_EVENT( FOnFloatValueChanged, OnValueChanged )
		/** Called right before the slider begins to move */
		SLATE_EVENT( FSimpleDelegate, OnBeginSliderMovement )
		/** Called right after the slider handle is released by the user */
		SLATE_EVENT( FOnFloatValueChanged, OnEndSliderMovement )
		/** View Input range **/
		SLATE_ATTRIBUTE( float, ViewInputMin )
		SLATE_ATTRIBUTE( float, ViewInputMax )
		SLATE_EVENT( FOnSetInputViewRange, OnSetInputViewRange )
		/** Called when an anim sequence is cropped before/after a selected frame */
		SLATE_EVENT( FOnCropAnimSequence, OnCropAnimSequence )
		/** Called when an frane is added before/after a selected frame */
		SLATE_EVENT( FOnAddAnimSequence, OnAddAnimSequence )
		/** Called when an frame is appended in the beginning or at the end */
		SLATE_EVENT(FOnAppendAnimSequence, OnAppendAnimSequence)
		/** Called to zero out selected frame's translation from origin */
		SLATE_EVENT( FOnReZeroAnimSequence, OnReZeroAnimSequence )
		/** Optional, additional values to draw on the timeline **/
		SLATE_ATTRIBUTE( TArray<float>, DraggableBars )
		SLATE_EVENT( FOnScrubBarDrag, OnBarDrag)
	SLATE_END_ARGS()


	/**
	 * Construct the widget
	 * 
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs );

	// SWidget interface
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;

	static int32 GetDivider(float InputMinX, float InputMaxX, FVector2D WidgetSize, float SequenceLength, int32 NumFrames);
protected:

	/**
	 * Call this method when the user's interaction has changed the value
	 *
	 * @param NewValue				Value resulting from the user's interaction
	 * @param bSliderClamp			true to clamp the slider value to the nearest delta value
	 * @param bCommittedFromText	true if the value was committed from the text box and not the slider	
	 */
	void CommitValue( float NewValue, bool bSliderClamp, bool bCommittedFromText );

private:

	/** Function to create context menu to display anim sequence editing options */
	void CreateContextMenu( float CurrentFrameTime, const FPointerEvent& MouseEvent );

	/** Function to crop animation sequence before/after selected frame */
	void OnSequenceCroppedCalled( bool bFromStart, float CurrentFrameTime );

	/** Function to crop animation sequence before/after selected frame */
	void OnSequenceAddedCalled(bool bBefore, int32 CurrentFrameNumber);

	/** Function to append frames in the beginning or at the end*/
	void OnSequenceAppendedCalled(const FText & InNewGroupText, ETextCommit::Type CommitInfo, bool bBegin);

	/** Function to ask how many frames to append */
	void OnShowPopupOfAppendAnimation(bool bBegin);

	/** Function to zero out translation of the selected frame */
	void OnReZeroCalled(int32 FrameIndex);

	TAttribute<float> ValueAttribute;
	FOnFloatValueChanged OnValueChanged;
	FSimpleDelegate OnBeginSliderMovement;
	FOnFloatValueChanged OnEndSliderMovement;

	TAttribute<float> ViewInputMin;
	TAttribute<float> ViewInputMax;
	FOnSetInputViewRange	OnSetInputViewRange;
	FOnCropAnimSequence		OnCropAnimSequence;
	FOnAddAnimSequence		OnAddAnimSequence;
	FOnAppendAnimSequence	OnAppendAnimSequence;
	FOnReZeroAnimSequence	OnReZeroAnimSequence;

	/** Dragagble bars are generic lines drawn on the scrub widget that can be dragged with the mouse. This is very bare bones and just represents drawing/moving float values */
	TAttribute<TArray<float>>	DraggableBars;
	FOnScrubBarDrag OnBarDrag;

	/** Distance Dragged **/
	float DistanceDragged;
	/** Number of Keys **/
	TAttribute<uint32> NumOfKeys;
	TAttribute<float> SequenceLength;
	/** The number of decimal places to display */
	bool bDragging;
	bool bAllowZoom;	
	TAttribute<bool> bDisplayDrag;
	/** If we are currently panning the panel*/
	bool bPanning;
	/** Has the mouse moved during panning - used to determine if we should open the context menu or not */
	bool bMouseMovedDuringPanning;
	/** Bar Dragging*/
	int32 DraggableBarIndex;
	bool DraggingBar;
};

