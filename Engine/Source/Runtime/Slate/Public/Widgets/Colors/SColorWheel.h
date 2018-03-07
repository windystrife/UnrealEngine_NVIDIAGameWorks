// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "Framework/SlateDelegates.h"

class FPaintArgs;
class FSlateWindowElementList;
struct FSlateBrush;

/**
 * Implements the color wheel widget.
 */
class SLATE_API SColorWheel
	: public SLeafWidget
{
public:

	SLATE_BEGIN_ARGS(SColorWheel)
		: _SelectedColor()
		, _OnMouseCaptureBegin()
		, _OnMouseCaptureEnd()
		, _OnValueChanged()
	{ }
	
		/** The current color selected by the user. */
		SLATE_ATTRIBUTE(FLinearColor, SelectedColor)
		
		/** Invoked when the mouse is pressed and a capture begins. */
		SLATE_EVENT(FSimpleDelegate, OnMouseCaptureBegin)

		/** Invoked when the mouse is released and a capture ends. */
		SLATE_EVENT(FSimpleDelegate, OnMouseCaptureEnd)

		/** Invoked when a new value is selected on the color wheel. */
		SLATE_EVENT(FOnLinearColorValueChanged, OnValueChanged)

	SLATE_END_ARGS()
	
public:

	/**
	 * Construct this widget.
	 *
	 * @param InArgs The declaration data for this widget.
	 */
	void Construct(const FArguments& InArgs);

public:

	//~ SWidget overrides

	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	
protected:

	/**
	 * Calculates the position of the color selection indicator.
	 *
	 * @return The position relative to the widget.
	 */
	FVector2D CalcRelativePositionFromCenter() const;

	/**
	 * Performs actions according to mouse click / move
	 *
	 * @return	True if the mouse action occurred within the color wheel radius
	 */
	bool ProcessMouseAction(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bProcessWhenOutsideColorWheel);

private:

	/** The color wheel image to show. */
	const FSlateBrush* Image;
	
	/** The current color selected by the user. */
	TAttribute< FLinearColor > SelectedColor;

	/** The color selector image to show. */
	const FSlateBrush* SelectorImage;

private:

	/** Invoked when the mouse is pressed and a capture begins. */
	FSimpleDelegate OnMouseCaptureBegin;

	/** Invoked when the mouse is let up and a capture ends. */
	FSimpleDelegate OnMouseCaptureEnd;

	/** Invoked when a new value is selected on the color wheel. */
	FOnLinearColorValueChanged OnValueChanged;
};
