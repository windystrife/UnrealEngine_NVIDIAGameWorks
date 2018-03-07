// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Layout/Geometry.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "VisualizerEvents.h"

class FPaintArgs;
class FSlateWindowElementList;
class SButton;
class SRealtimeProfilerVisualizer;
struct FRealtimeProfilerFPSChartFrame;

//////////////////////////////////////////////////////////////////////////
// SRealtimeProfilerLineGraph


class SRealtimeProfilerLineGraph : public SCompoundWidget
{
public:

	/** Delegate used allotted gemetry changes */
	DECLARE_DELEGATE_OneParam( FOnGeometryChanged, FGeometry );

private:

	/** Max view range */
	TAttribute<float>	MaxValue;
	/** Number of frames to buffer */
	TAttribute<int32>	MaxFrames;

	/** Delegate called when the geometry changes */
	FOnGeometryChanged OnGeometryChanged;

	/** Pointer to the visualizer */
	TAttribute< SRealtimeProfilerVisualizer * > Visualizer;


public:


	SLATE_BEGIN_ARGS( SRealtimeProfilerLineGraph )
		: _MaxValue(100.0f)
		, _MaxFrames(500)
		, _Visualizer()
		{}

		SLATE_ATTRIBUTE( float, MaxValue )
		SLATE_ATTRIBUTE( int32, MaxFrames)
		SLATE_ATTRIBUTE( SRealtimeProfilerVisualizer *, Visualizer )
		SLATE_EVENT( FOnGeometryChanged, OnGeometryChanged )
		

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	int32 GetMaxFrames() const;
	void AppendData(TSharedPtr< FVisualizerEvent > ProfileData, FRealtimeProfilerFPSChartFrame * InFPSChartFrame);

	/**
	 * Sets the graph's zoom level
	 *
	 * @param NewValue	Zoom value, minimum is 1.0
	 */
	void SetZoom( float InZoom )
	{
		Zoom = FMath::Max(InZoom, 1.0f);
	}

	/**
	 * Sets the graph's offset by which all graph bars should be moved
	 *
	 * @param NewValue	Offset value
	 */
	void SetOffset( float InOffset )
	{
		Offset = InOffset;
	} 

	/**
	 * Gets the graph's offset value
	 *
	 * @return Offset value
	 */
	float GetOffset() const
	{
		return Offset;
	}

	bool bIsProfiling;

private:


	FVector2D GetWidgetPosition(float X, float Y, const FGeometry& Geom) const;
	void DisplayFrameDetailAtMouse(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	// SWidget overrides
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	/** Current zoom of the graph */
	float Zoom;

	/** Current offset of the graph */
	float Offset;

	/** Last allotted geometry */
	FGeometry LastGeometry;

	/** Current mouse position to draw the graph cursor */
	FVector2D MousePosition;

	TArray<TSharedPtr< FVisualizerEvent > > ProfileDataArray;
	TArray<FRealtimeProfilerFPSChartFrame> FPSChartDataArray;

	TSharedPtr< SButton >  StartButton;
	TSharedPtr< SButton >  PauseButton;

	FReply OnStartButtonDown();
	FReply OnPauseButtonDown();
	FReply OnStopButtonDown();
	FReply OnSwitchViewButtonDown();

	EVisibility GetStartButtonVisibility() const;
	EVisibility GetPauseButtonVisibility() const;

	bool bDisplayFPSChart;
	bool bIsLeftMouseButtonDown;
};


