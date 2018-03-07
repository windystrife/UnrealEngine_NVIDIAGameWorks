// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Layout/SlateRect.h"
#include "Layout/Geometry.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Animation/CurveSequence.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Templates/ScopedPointer.h"
#include "SNodePanel.h"
#include "UniquePtr.h"

class FActiveTimerHandle;
class FSlateWindowElementList;

class SDesignSurface : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SDesignSurface )
		: _AllowContinousZoomInterpolation(false)
	{ }

		/** Slot for this designers content (optional) */
		SLATE_DEFAULT_SLOT(FArguments, Content)

		SLATE_ATTRIBUTE(bool, AllowContinousZoomInterpolation)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent) override;
	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	// End of Swidget interface

	/** Gets the current zoom factor. */
	float GetZoomAmount() const;

protected:
	virtual void OnPaintBackground(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	void PaintBackgroundAsLines(const FSlateBrush* BackgroundImage, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32& DrawLayerId) const;

	void ChangeZoomLevel(int32 ZoomLevelDelta, const FVector2D& WidgetSpaceZoomOrigin, bool bOverrideZoomLimiting);
	
	void PostChangedZoom();

	bool ScrollToLocation(const FGeometry& MyGeometry, FVector2D DesiredCenterPosition, const float InDeltaTime);

	bool ZoomToLocation(const FVector2D& CurrentSizeWithoutZoom, const FVector2D& DesiredSize, bool bDoneScrolling);

	void ZoomToFit(bool bInstantZoom);

	FText GetZoomText() const;
	FSlateColor GetZoomTextColorAndOpacity() const;

	FVector2D GetViewOffset() const;

	FSlateRect ComputeSensibleBounds() const;

	FVector2D GraphCoordToPanelCoord(const FVector2D& GraphSpaceCoordinate) const;
	FVector2D PanelCoordToGraphCoord(const FVector2D& PanelSpaceCoordinate) const;

protected:
	virtual FSlateRect ComputeAreaBounds() const;
	virtual float GetGridScaleAmount() const;
	virtual int32 GetGraphRulePeriod() const;
	virtual int32 GetSnapGridSize() const = 0;

protected:
	/** The position within the graph at which the user is looking */
	FVector2D ViewOffset;

	/** The position in the grid to begin drawing at. */
	FVector2D GridOrigin;

	/** Should we render the grid lines? */
	bool bDrawGridLines;

	/** Previous Zoom Level */
	int32 PreviousZoomLevel;

	/** How zoomed in/out we are. e.g. 0.25f results in quarter-sized nodes. */
	int32 ZoomLevel;

	/** Are we panning the view at the moment? */
	bool bIsPanning;

	/** Are we zooming the view with trackpad at the moment? */
	bool bIsZoomingWithTrackpad;

	/** Allow continuous zoom interpolation? */
	TAttribute<bool> AllowContinousZoomInterpolation;

	/** Fade on zoom for graph */
	FCurveSequence ZoomLevelGraphFade;

	/** Curve that handles fading the 'Zoom +X' text */
	FCurveSequence ZoomLevelFade;

	// The interface for mapping ZoomLevel values to actual node scaling values
	TUniquePtr<FZoomLevelsContainer> ZoomLevels;

	bool bAllowContinousZoomInterpolation;

	bool bTeleportInsteadOfScrollingWhenZoomingToFit;

	FVector2D ZoomTargetTopLeft;
	FVector2D ZoomTargetBottomRight;
	FVector2D ZoomToFitPadding;

	/** The Y component of mouse drag (used when zooming) */
	float TotalMouseDeltaY;

	/** Offset in the panel the user started the LMB+RMB zoom from */
	FVector2D ZoomStartOffset;

	/**  */
	FVector2D ViewOffsetStart;

	/**  */
	FVector2D MouseDownPositionAbsolute;

	/** Cumulative magnify delta from trackpad gesture */
	float TotalGestureMagnify;

	/** Does the user need to press Control in order to over-zoom. */
	bool bRequireControlToOverZoom;

private:
	/** Active timer that handles deferred zooming until the target zoom is reached */
	EActiveTimerReturnType HandleZoomToFit(double InCurrentTime, float InDeltaTime);

	/** The handle to the active timer */
	TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;

	// A flag noting if we have a pending zoom to extents operation to perform next tick.
	bool bDeferredZoomToExtents;
};
