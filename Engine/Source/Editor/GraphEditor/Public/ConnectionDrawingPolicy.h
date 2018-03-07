// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "Layout/ArrangedWidget.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"
#include "GraphSplineOverlapResult.h"
#include "GraphEditorSettings.h"

class FSlateWindowElementList;

/////////////////////////////////////////////////////

GRAPHEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogConnectionDrawingPolicy, Log, All);

/////////////////////////////////////////////////////
// FGeometryHelper

class GRAPHEDITOR_API FGeometryHelper
{
public:
	static FVector2D VerticalMiddleLeftOf(const FGeometry& SomeGeometry);
	static FVector2D VerticalMiddleRightOf(const FGeometry& SomeGeometry);
	static FVector2D CenterOf(const FGeometry& SomeGeometry);
	static void ConvertToPoints(const FGeometry& Geom, TArray<FVector2D>& Points);

	/** Find the point on line segment from LineStart to LineEnd which is closest to Point */
	static FVector2D FindClosestPointOnLine(const FVector2D& LineStart, const FVector2D& LineEnd, const FVector2D& TestPoint);

	static FVector2D FindClosestPointOnGeom(const FGeometry& Geom, const FVector2D& TestPoint);
};

/////////////////////////////////////////////////////
// FConnectionParameters

struct GRAPHEDITOR_API FConnectionParams
{
	FLinearColor WireColor;
	UEdGraphPin* AssociatedPin1;
	UEdGraphPin* AssociatedPin2;

	float WireThickness;
	bool bDrawBubbles;
	bool bUserFlag1;
	bool bUserFlag2;

	EEdGraphPinDirection StartDirection;
	EEdGraphPinDirection EndDirection;

	FConnectionParams()
		: WireColor(FLinearColor::White)
		, AssociatedPin1(nullptr)
		, AssociatedPin2(nullptr)
		, WireThickness(1.0f)
		, bDrawBubbles(false)
		, bUserFlag1(false)
		, bUserFlag2(false)
		, StartDirection(EGPD_Output)
		, EndDirection(EGPD_Input)
	{
	}
};

/////////////////////////////////////////////////////
// FConnectionDrawingPolicy

// This class draws the connections for an UEdGraph composed of pins and nodes
class GRAPHEDITOR_API FConnectionDrawingPolicy
{
protected:
	int32 WireLayerID;
	int32 ArrowLayerID;

	const FSlateBrush* ArrowImage;
	const FSlateBrush* MidpointImage;
	const FSlateBrush* BubbleImage;

	const UGraphEditorSettings* Settings;

public:
	FVector2D ArrowRadius;
	FVector2D MidpointRadius;

	FGraphSplineOverlapResult SplineOverlapResult;

protected:
	float ZoomFactor; 
	float HoverDeemphasisDarkFraction;
	const FSlateRect& ClippingRect;
	FSlateWindowElementList& DrawElementsList;
	TMap< UEdGraphPin*, TSharedRef<SGraphPin> > PinToPinWidgetMap;
	TSet< FEdGraphPinReference > HoveredPins;
	TMap<TSharedRef<SWidget>, FArrangedWidget>* PinGeometries;
	double LastHoverTimeEvent;
	FVector2D LocalMousePosition;
public:
	virtual ~FConnectionDrawingPolicy() {}

	FConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements);

	// Update the drawing policy with the set of hovered pins (which can be empty)
	void SetHoveredPins(const TSet< FEdGraphPinReference >& InHoveredPins, const TArray< TSharedPtr<SGraphPin> >& OverridePins, double HoverTime);

	void SetMousePosition(const FVector2D& InMousePos);

	// Update the drawing policy with the marked pin (which may not be valid)
	void SetMarkedPin(TWeakPtr<SGraphPin> InMarkedPin);

	static float MakeSplineReparamTable(const FVector2D& P0, const FVector2D& P0Tangent, const FVector2D& P1, const FVector2D& P1Tangent, FInterpCurve<float>& OutReparamTable);

	virtual void DrawSplineWithArrow(const FVector2D& StartPoint, const FVector2D& EndPoint, const FConnectionParams& Params);
	
	virtual void DrawSplineWithArrow(const FGeometry& StartGeom, const FGeometry& EndGeom, const FConnectionParams& Params);

	virtual FVector2D ComputeSplineTangent(const FVector2D& Start, const FVector2D& End) const;
	virtual void DrawConnection(int32 LayerId, const FVector2D& Start, const FVector2D& End, const FConnectionParams& Params);
	virtual void DrawPreviewConnector(const FGeometry& PinGeometry, const FVector2D& StartPoint, const FVector2D& EndPoint, UEdGraphPin* Pin);

	// Give specific editor modes a chance to highlight this connection or darken non-interesting connections
	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params);

	virtual void Draw(TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries, FArrangedChildren& ArrangedNodes);

	virtual void DetermineLinkGeometry(
		FArrangedChildren& ArrangedNodes, 
		TSharedRef<SWidget>& OutputPinWidget,
		UEdGraphPin* OutputPin,
		UEdGraphPin* InputPin,
		/*out*/ FArrangedWidget*& StartWidgetGeometry,
		/*out*/ FArrangedWidget*& EndWidgetGeometry
		);

	virtual void SetIncompatiblePinDrawState(const TSharedPtr<SGraphPin>& StartPin, const TSet< TSharedRef<SWidget> >& VisiblePins);
	virtual void ResetIncompatiblePinDrawState(const TSet< TSharedRef<SWidget> >& VisiblePins);

	virtual void ApplyHoverDeemphasis(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ float& Thickness, /*inout*/ FLinearColor& WireColor);

	virtual bool IsConnectionCulled( const FArrangedWidget& StartLink, const FArrangedWidget& EndLink ) const;
};
