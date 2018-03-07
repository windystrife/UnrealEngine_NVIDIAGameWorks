// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/SlateRect.h"
#include "Layout/Geometry.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Rendering/RenderingCommon.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "Widgets/SLeafWidget.h"

class FCurveOwnerInterface;
class FPaintArgs;
class FSlateWindowElementList;

struct FGradientStopMark
{
public:

	FGradientStopMark();
	FGradientStopMark( float InTime, FKeyHandle InRedKeyHandle, FKeyHandle InGreenKeyHandle, FKeyHandle InBlueKeyHandle, FKeyHandle InAlphaKeyHandle = FKeyHandle() );


	bool operator==( const FGradientStopMark& Other ) const;
	bool IsValid( FCurveOwnerInterface& CurveOwner );
	bool IsValidAlphaMark( const TArray<FRichCurveEditInfo>& Curves ) const;
	bool IsValidColorMark( const TArray<FRichCurveEditInfo>& Curves ) const;
	FLinearColor GetColor( FCurveOwnerInterface& CurveOwner ) const;
	void SetColor( const FLinearColor& InColor, FCurveOwnerInterface& CurveOwner );
	void SetTime( float NewTime, FCurveOwnerInterface& CurveOwner );

public:
	float Time;
	FKeyHandle RedKeyHandle;
	FKeyHandle GreenKeyHandle;
	FKeyHandle BlueKeyHandle;
	FKeyHandle AlphaKeyHandle;

};

class SColorGradientEditor : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS( SColorGradientEditor ) 
		: _ViewMinInput(0.0f)
		, _ViewMaxInput(0.0f)
		, _IsEditingEnabled( true )
	{}
		SLATE_ATTRIBUTE( float, ViewMinInput )
		SLATE_ATTRIBUTE( float, ViewMaxInput )
		SLATE_ATTRIBUTE( bool, IsEditingEnabled )
	SLATE_END_ARGS()


	void Construct( const FArguments& InArgs );

	/** SWidget Interface */
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FVector2D ComputeDesiredSize(float) const override;

	/**
	 * Sets the curve that is being edited by this editor
	 *
	 * @param InCurveOwner Interface to the curves to edit
	 */
	void SetCurveOwner( FCurveOwnerInterface* InCurveOwner );

private:
	/**
	 * Opens a context menu with options for the selected gradient stop
	 *
	 * @param MouseEvent	The mouse event that raised this handler (containing the location and widget path of the click)
	 */
	void OpenGradientStopContextMenu(const FPointerEvent& MouseEvent);

	/**
	 * Opens a color picker to change the color of the selected stop
	 */
	void OpenGradientStopColorPicker();

	/**
	 * Called when the selected stop color changes from the color picker
	 *
	 * @param InNewColor	The new color
	 */
	void OnSelectedStopColorChanged( FLinearColor InNewColor );

	/**
	 * Called when canceling out of the color picker
	 *
	 * @param PreviousColor	The color before anything was changed
	 */
	void OnCancelSelectedStopColorChange( FLinearColor PreviousColor );

	/**
	 * Called right before a user begins using the slider to change the alpha value of a stop
	 */
	void OnBeginChangeAlphaValue();

	/**
	 * Called right after a user ends using the slider to change the alpha value of a stop
	 */
	void OnEndChangeAlphaValue( float NewValue );

	/**
	 * Called when the alpha value of a stop changes
	 */
	void OnAlphaValueChanged( float NewValue );

	/**
	 * Called when the alpha value of a stop changes by typing into the sliders text box
	 */
	void OnAlphaValueCommitted( float NewValue, ETextCommit::Type );

	/**
	 * Called to remove the selected gradient stop
	 */
	void OnRemoveSelectedGradientStop();

	/**
	 * Called when the gradient stop time is changed from the text entry popup
	 *
	 * @param NewText	The new time as text
	 * @param Type		How the value was committed 
	 */
	void OnSetGradientStopTimeFromPopup( const FText& NewText, ETextCommit::Type Type );

	/**
	 * Draws a single gradient stop
	 *
	 * @param Mark				The stop mark to draw
	 * @param Geometry			The geometry of the area to draw in
	 * @param XPos				The local X location where the stop should be placed at
	 * @param Color				The color of the stop
	 * @param DrawElements		List of draw elements to add to
	 * @param LayerId			The layer to start drawing on
	 * @param InClippingRect	The clipping rect 
	 * @param DrawEffects		Any draw effects to apply
	 * @param bColor			If true RGB of the Color param will be used, otherwise the A value will be used
	 */
	void DrawGradientStopMark( const FGradientStopMark& Mark, const FGeometry& Geometry, float XPos, const FLinearColor& Color, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FSlateRect& InClippingRect, ESlateDrawEffect DrawEffects, bool bColor, const FWidgetStyle& InWidgetStyle ) const;

	/**
	 * Calculates the geometry of the gradient stop color mark area
	 *
	 * @param InGeometry The parent geometry of the area
	 */
	FGeometry GetColorMarkAreaGeometry( const FGeometry& InGeometry ) const;

	/**
	 * Calculates the geometry of the gradient stop alpha mark area
	 *
	 * @param InGeometry The parent geometry of the area
	 */
	FGeometry GetAlphaMarkAreaGeometry( const FGeometry& InGeometry ) const;

	/**
	 * Gets the gradient stop (if any) of at the current mouse position
	 *
	 * @param MousePos		The screen space mouse position to check
	 * @param MyGeometry	The geometry that was clicked in
	 * @return				The stop mark at MousePos or invalid if none was found
	 */
	FGradientStopMark GetGradientStopAtPoint( const FVector2D& MousePos, const FGeometry& MyGeometry );

	/**
	 * Get all gradient stop marks on the curve
	 */
	void GetGradientStopMarks( TArray<FGradientStopMark>& OutColorMarks, TArray<FGradientStopMark>& OutAlphaMarks ) const;

	/**
	 * Removes a gradient stop
	 *
	 * @param InMark	The stop to remove
	 */
	void DeleteStop( const FGradientStopMark& InMark );

	/**
	 * Adds a gradient stop
	 *
	 * @param Position		The position to add the stop at
	 * @param MyGeometry	The geometry of the stop area
	 * @param bColorStop	If true a color stop is added, otherwise an alpha stop is added
	 */
	FGradientStopMark AddStop( const FVector2D& Position, const FGeometry& MyGeometry, bool bColorStop );

	/**
	 * Moves a gradient stop to a new time
	 *
	 * @param Mark		The gradient stop to move
	 * @param NewTime	The new time to move to
	 */
	void MoveStop( FGradientStopMark& Mark, float NewTime );

private:
	/** Local space rectangle of each gradient stop handle */
	static const FSlateRect HandleRect;

	/** The currently selected stop */
	FGradientStopMark SelectedStop;
	/** The color that was last modified. New stops are added with this color */
	FLinearColor LastModifiedColor;
	/** interface to the curves being edited */
	FCurveOwnerInterface* CurveOwner;
	/** Current min input value that is visible */
	TAttribute<float> ViewMinInput;
	/** Current max input value that is visible */
	TAttribute<float> ViewMaxInput;
	/** Whether or not the gradient is editable or just viewed */
	TAttribute<bool> IsEditingEnabled;
	/** Cached position where context menus should appear */
	FVector2D ContextMenuPosition;
	/** Current distance dragged since we captured the mouse */
	float DistanceDragged;
	/** True if an alpha value is being dragged */
	bool bDraggingAlphaValue;
	/** True if a gradient stop is being dragged */
	bool bDraggingStop;
};
