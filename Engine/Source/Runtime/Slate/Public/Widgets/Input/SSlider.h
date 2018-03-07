// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/SLeafWidget.h"

class FPaintArgs;
class FSlateWindowElementList;

/**
 * A Slate slider control is a linear scale and draggable handle.
 */
class SLATE_API SSlider
	: public SLeafWidget
{
public:

	SLATE_BEGIN_ARGS(SSlider)
		: _IndentHandle(true)
		, _Locked(false)
		, _Orientation(EOrientation::Orient_Horizontal)
		, _SliderBarColor(FLinearColor::White)
		, _SliderHandleColor(FLinearColor::White)
		, _Style(&FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("Slider"))
		, _StepSize(0.01f)
		, _Value(1.f)
		, _IsFocusable(true)
		, _OnMouseCaptureBegin()
		, _OnMouseCaptureEnd()
		, _OnValueChanged()
		{ }

		/** Whether the slidable area should be indented to fit the handle. */
		SLATE_ATTRIBUTE( bool, IndentHandle )

		/** Whether the handle is interactive or fixed. */
		SLATE_ATTRIBUTE( bool, Locked )

		/** The slider's orientation. */
		SLATE_ARGUMENT( EOrientation, Orientation)

		/** The color to draw the slider bar in. */
		SLATE_ATTRIBUTE( FSlateColor, SliderBarColor )

		/** The color to draw the slider handle in. */
		SLATE_ATTRIBUTE( FSlateColor, SliderHandleColor )

		/** The style used to draw the slider. */
		SLATE_STYLE_ARGUMENT( FSliderStyle, Style )

		/** The input mode while using the controller. */
		SLATE_ATTRIBUTE(float, StepSize)

		/** A value that drives where the slider handle appears. Value is normalized between 0 and 1. */
		SLATE_ATTRIBUTE( float, Value )

		/** Sometimes a slider should only be mouse-clickable and never keyboard focusable. */
		SLATE_ARGUMENT(bool, IsFocusable)

		/** Invoked when the mouse is pressed and a capture begins. */
		SLATE_EVENT(FSimpleDelegate, OnMouseCaptureBegin)

		/** Invoked when the mouse is released and a capture ends. */
		SLATE_EVENT(FSimpleDelegate, OnMouseCaptureEnd)

		/** Invoked when the Controller is pressed and capture begins. */
		SLATE_EVENT(FSimpleDelegate, OnControllerCaptureBegin)

		/** Invoked when the controller capture is released.  */
		SLATE_EVENT(FSimpleDelegate, OnControllerCaptureEnd)

		/** Called when the value is changed by the slider. */
		SLATE_EVENT( FOnFloatValueChanged, OnValueChanged )

	SLATE_END_ARGS()

	/**
	 * Construct the widget.
	 * 
	 * @param InDeclaration A declaration from which to construct the widget.
	 */
	void Construct( const SSlider::FArguments& InDeclaration );

	/** See the Value attribute */
	float GetValue() const;

	/** See the Value attribute */
	void SetValue(const TAttribute<float>& InValueAttribute);
	
	/** See the IndentHandle attribute */
	void SetIndentHandle(const TAttribute<bool>& InIndentHandle);
	
	/** See the Locked attribute */
	void SetLocked(const TAttribute<bool>& InLocked);

	/** See the Orientation attribute */
	void SetOrientation(EOrientation InOrientation);
	
	/** See the SliderBarColor attribute */
	void SetSliderBarColor(FSlateColor InSliderBarColor);
	
	/** See the SliderHandleColor attribute */
	void SetSliderHandleColor(FSlateColor InSliderHandleColor);

	/** Get the StepSize attribute */
	float GetStepSize() const;

	/** See the StepSize attribute */
	void SetStepSize(const TAttribute<float>& InStepSize);

public:

	// SWidget overrides

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;

	virtual bool SupportsKeyboardFocus() const override;
	virtual bool IsInteractable() const override;

	/** @return Is the handle locked or not? Defaults to false */
	bool IsLocked() const;

protected:

	/**
	 * Commits the specified slider value.
	 *
	 * @param NewValue The value to commit.
	 */
	virtual void CommitValue(float NewValue);

	/**
	 * Calculates the new value based on the given absolute coordinates.
	 *
	 * @param MyGeometry The slider's geometry.
	 * @param AbsolutePosition The absolute position of the slider.
	 * @return The new value.
	 */
	float PositionToValue( const FGeometry& MyGeometry, const FVector2D& AbsolutePosition );

protected:

	// Holds the style passed to the widget upon construction.
	const FSliderStyle* Style;

	// Holds a flag indicating whether the slideable area should be indented to fit the handle.
	TAttribute<bool> IndentHandle;

	// Holds a flag indicating whether the slider is locked.
	TAttribute<bool> LockedAttribute;

	// Holds the slider's orientation.
	EOrientation Orientation;

	// Holds the color of the slider bar.
	TAttribute<FSlateColor> SliderBarColor;

	// Holds the color of the slider handle.
	TAttribute<FSlateColor> SliderHandleColor;

	// Holds the slider's current value.
	TAttribute<float> ValueAttribute;

	/** Holds the amount to adjust the value by when using a controller or keyboard */
	TAttribute<float> StepSize;

	// Holds a flag indicating whether a controller/keyboard is manipulating the slider's value. 
	// When true, navigation away from the widget is prevented until a new value has been accepted or canceled. 
	bool bControllerInputCaptured;

	/** When true, this slider will be keyboard focusable. Defaults to false. */
	bool bIsFocusable;

private:

	// Resets controller input state. Fires delegates.
	void ResetControllerState();

	// Holds a delegate that is executed when the mouse is pressed and a capture begins.
	FSimpleDelegate OnMouseCaptureBegin;

	// Holds a delegate that is executed when the mouse is let up and a capture ends.
	FSimpleDelegate OnMouseCaptureEnd;

	// Holds a delegate that is executed when capture begins for controller or keyboard.
	FSimpleDelegate OnControllerCaptureBegin;

	// Holds a delegate that is executed when capture ends for controller or keyboard.
	FSimpleDelegate OnControllerCaptureEnd;

	// Holds a delegate that is executed when the slider's value changed.
	FOnFloatValueChanged OnValueChanged;
};
