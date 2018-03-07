// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SNodePanel.h"
#include "BehaviorTreeEditorTypes.h"
#include "SGraphNode.h"
#include "SGraphPin.h"
#include "SGraphNodeAI.h"

class SHorizontalBox;
class SToolTip;
class SVerticalBox;
class UBehaviorTreeGraphNode;

class SGraphNode_BehaviorTree : public SGraphNodeAI
{
public:
	SLATE_BEGIN_ARGS(SGraphNode_BehaviorTree){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UBehaviorTreeGraphNode* InNode);

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	virtual void CreatePinWidgets() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	virtual TSharedPtr<SToolTip> GetComplexTooltip() override;
	virtual void GetOverlayBrushes(bool bSelected, const FVector2D WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const override;
	virtual TArray<FOverlayWidgetInfo> GetOverlayWidgets(bool bSelected, const FVector2D& WidgetSize) const override;
	virtual TSharedRef<SGraphNode> GetNodeUnderMouse(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter) override;
	// End of SGraphNode interface

	/** handle double click */
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;

	/**
	 * Ticks this widget.  Override in derived classes, but always call the parent implementation.
	 *
	 * @param  AllottedGeometry The space allotted for this widget
	 * @param  InCurrentTime  Current absolute real time
	 * @param  InDeltaTime  Real time passed since last tick
	 */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	
	/** adds decorator widget inside current node */
	void AddDecorator(TSharedPtr<SGraphNode> DecoratorWidget);

	/** adds service widget inside current node */
	void AddService(TSharedPtr<SGraphNode> ServiceWidget);

	/** shows red marker when search failed*/
	EVisibility GetDebuggerSearchFailedMarkerVisibility() const;

	FVector2D GetCachedPosition() const { return CachedPosition; }

protected:
	uint32 bSuppressDebuggerColor : 1;
	uint32 bSuppressDebuggerTriggers : 1;
	
	/** time spent in current state */
	float DebuggerStateDuration;

	/** currently displayed state */
	int32 DebuggerStateCounter;

	/** debugger colors */
	FLinearColor FlashColor;
	float FlashAlpha;

	/** height offsets for search triggers */
	TArray<FNodeBounds> TriggerOffsets;

	/** cached draw position */
	FVector2D CachedPosition;

	TArray< TSharedPtr<SGraphNode> > DecoratorWidgets;
	TArray< TSharedPtr<SGraphNode> > ServicesWidgets;
	TSharedPtr<SVerticalBox> DecoratorsBox;
	TSharedPtr<SVerticalBox> ServicesBox;
	TSharedPtr<SHorizontalBox> OutputPinBox;

	/** The widget we use to display the index of the node */
	TSharedPtr<SWidget> IndexOverlay;

	/** The node body widget, cached here so we can determine its size when we want ot position our overlays */
	TSharedPtr<SBorder> NodeBody;

	FSlateColor GetBorderBackgroundColor() const;
	FSlateColor GetBackgroundColor() const;

	virtual const FSlateBrush* GetNameIcon() const override;
	virtual EVisibility GetBlueprintIconVisibility() const;

	/** Get the visibility of the index overlay */
	EVisibility GetIndexVisibility() const;

	/** Get the text to display in the index overlay */
	FText GetIndexText() const;

	/** Get the tooltip for the index overlay */
	FText GetIndexTooltipText() const;

	/** Get the color to display for the index overlay. This changes on hover state of sibling nodes */
	FSlateColor GetIndexColor(bool bHovered) const;

	/** Handle hover state changing for the index widget - we use this to highlight sibling nodes */
	void OnIndexHoverStateChanged(bool bHovered);

	FText GetPinTooltip(UEdGraphPin* GraphPinObj) const;
};
