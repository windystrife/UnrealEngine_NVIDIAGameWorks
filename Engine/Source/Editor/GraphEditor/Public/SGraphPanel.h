// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Misc/Attribute.h"
#include "EdGraph/EdGraphPin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/Geometry.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Animation/CurveSequence.h"
#include "UObject/GCObject.h"
#include "GraphEditor.h"
#include "SNodePanel.h"
#include "SGraphNode.h"
#include "GraphEditAction.h"
#include "SGraphPin.h"
#include "GraphSplineOverlapResult.h"

class FActiveTimerHandle;
class FSlateWindowElementList;
class IToolTip;
class UEdGraph;

DECLARE_DELEGATE( FOnUpdateGraphPanel )

// Arguments when the graph panel wants to open a context menu
struct FGraphContextMenuArguments
{
	// The endpoint of the drag or the location of the right-click
	FVector2D NodeAddPosition;

	// The source node if there are any
	UEdGraphNode* GraphNode;

	// The source pin if there is one
	UEdGraphPin* GraphPin;

	// 
	TArray<UEdGraphPin*> DragFromPins;
};


class GRAPHEDITOR_API SGraphPanel : public SNodePanel, public FGCObject
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(FActionMenuContent, FOnGetContextMenuFor, const FGraphContextMenuArguments& /*SpawnInfo*/)

	SLATE_BEGIN_ARGS( SGraphPanel )
		: _OnGetContextMenuFor()
		, _OnSelectionChanged()
		, _OnNodeDoubleClicked()
		, _GraphObj( static_cast<UEdGraph*>(NULL) )
		, _GraphObjToDiff( static_cast<UEdGraph*>(NULL) )
		, _InitialZoomToFit( false )
		, _IsEditable( true )
		, _DisplayAsReadOnly( false )
		, _ShowGraphStateOverlay(true)
		, _OnUpdateGraphPanel()
		{
			_Clipping = EWidgetClipping::ClipToBounds;
		}

		SLATE_EVENT( FOnGetContextMenuFor, OnGetContextMenuFor )
		SLATE_EVENT( SGraphEditor::FOnSelectionChanged, OnSelectionChanged )
		SLATE_EVENT( FSingleNodeEvent, OnNodeDoubleClicked )
		SLATE_EVENT( SGraphEditor::FOnDropActor, OnDropActor )
		SLATE_EVENT( SGraphEditor::FOnDropStreamingLevel, OnDropStreamingLevel )
		SLATE_ARGUMENT( class UEdGraph*, GraphObj )
		SLATE_ARGUMENT( class UEdGraph*, GraphObjToDiff )
		SLATE_ARGUMENT( bool, InitialZoomToFit )
		SLATE_ATTRIBUTE( bool, IsEditable )
		SLATE_ATTRIBUTE( bool, DisplayAsReadOnly )
		/** Show overlay elements for the graph state such as the PIE and read-only borders and text */
		SLATE_ATTRIBUTE(bool, ShowGraphStateOverlay)
		SLATE_EVENT( FOnNodeVerifyTextCommit, OnVerifyTextCommit )
		SLATE_EVENT( FOnNodeTextCommitted, OnTextCommitted )
		SLATE_EVENT( SGraphEditor::FOnSpawnNodeByShortcut, OnSpawnNodeByShortcut )
		SLATE_EVENT( FOnUpdateGraphPanel, OnUpdateGraphPanel )
		SLATE_EVENT( SGraphEditor::FOnDisallowedPinConnection, OnDisallowedPinConnection )
		//SLATE_ATTRIBUTE( FGraphAppearanceInfo, Appearance )
	SLATE_END_ARGS()

	/**
	 * Construct a widget
	 *
	 * @param InArgs    The declaration describing how the widgets should be constructed.
	 */
	void Construct( const FArguments& InArgs );

	// Destructor
	~SGraphPanel();
public:
	// SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual TSharedPtr<IToolTip> GetToolTip() override;
	// End of SWidget interface

	// SNodePanel interface
	virtual TSharedPtr<SWidget> OnSummonContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual bool OnHandleLeftMouseRelease(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void AddGraphNode(const TSharedRef<SNode>& NodeToAdd) override;
	virtual void RemoveAllNodes() override;
	// End of SNodePanel interface

	// FGCObject interface.
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	// End of FGCObject interface.

	void ArrangeChildrenForContextMenuSummon(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const;
	TSharedPtr<SWidget> SummonContextMenu(const FVector2D& WhereToSummon, const FVector2D& WhereToAddNode, UEdGraphNode* ForNode, UEdGraphPin* ForPin, const TArray<UEdGraphPin*>& DragFromPins);

	void OnBeginMakingConnection(UEdGraphPin* InOriginatingPin);
	void OnBeginMakingConnection(FGraphPinHandle PinHandle);
	void OnStopMakingConnection(bool bForceStop = false);
	void PreservePinPreviewUntilForced();

	/** Update this GraphPanel to match the data that it is observing. Expected to be called during ticking. */
	void Update();

	/** Purges the existing visual representation (typically followed by an Update call in the next tick) */
	void PurgeVisualRepresentation();

	/** Use to determine if a comment title is currently visible */
	bool IsNodeTitleVisible(const class UEdGraphNode* Node, bool bRequestRename);

	/** Use to determine if a rectangle is currently visible */
	bool IsRectVisible(const FVector2D &TopLeft, const FVector2D &BottomRight);

	/** Focuses the view on rectangle, zooming if neccesary */
	bool JumpToRect(const FVector2D &BottomLeft, const FVector2D &TopRight);

	void JumpToNode(const class UEdGraphNode* JumpToMe, bool bRequestRename, bool bSelectNode);

	void JumpToPin(const class UEdGraphPin* JumptToMe);

	void GetAllPins(TSet< TSharedRef<SWidget> >& AllPins);

	void AddPinToHoverSet(UEdGraphPin* HoveredPin);
	void RemovePinFromHoverSet(UEdGraphPin* UnhoveredPin);

	SGraphEditor::EPinVisibility GetPinVisibility() const { return PinVisibility; }
	void SetPinVisibility(SGraphEditor::EPinVisibility InVisibility) { PinVisibility = InVisibility; }

	UEdGraph* GetGraphObj() const { return GraphObj; }

	/** helper to attach graph events to sub node, which won't be placed directly on the graph */
	void AttachGraphEvents(TSharedPtr<SGraphNode> CreatedSubNode);

	/** Returns if this graph is editable */
	bool IsGraphEditable() const { return IsEditable.Get(); }

	/** Attempt to retrieve the bounds for the specified node */
	bool GetBoundsForNode(const UObject* InNode, FVector2D& MinCorner, FVector2D& MaxCorner, float Padding = 0.0f) const;

	/** Straighten all connections between the selected nodes */
	void StraightenConnections();
	
	/** Straighten any connections attached to the specified pin, optionally limiting to the specified pin to align */
	void StraightenConnections(UEdGraphPin* SourcePin, UEdGraphPin* PinToAlign = nullptr);

	/** When the graph panel needs to be dynamically refreshing for animations, this function is registered to tick and invalidate the UI. */
	EActiveTimerReturnType InvalidatePerTick(double InCurrentTime, float InDeltaTime);

protected:

	void NotifyGraphChanged ( const struct FEdGraphEditAction& InAction);

	const TSharedRef<SGraphNode> GetChild(int32 ChildIndex);

	/** Flag to control AddNode, more readable than a bool:*/
	enum AddNodeBehavior
	{
		CheckUserAddedNodesList,
		WasUserAdded,
		NotUserAdded
	};

	/** Helper method to add a new node to the panel */
	void AddNode(class UEdGraphNode* Node, AddNodeBehavior Behavior);

	/** Helper method to remove a node from the panel */
	void RemoveNode(const UEdGraphNode* Node);
public:
	/** Pin marked via shift-clicking */
	TWeakPtr<SGraphPin> MarkedPin;

	/** Get a graph node widget from the specified GUID, if it applies to any nodes in this graph */
	TSharedPtr<SGraphNode> GetNodeWidgetFromGuid(FGuid Guid) const;

private:

	/** A map of guid -> graph nodes */
	TMap<FGuid, TWeakPtr<SGraphNode>> NodeGuidMap;

protected:
	UEdGraph* GraphObj;
	UEdGraph* GraphObjToDiff;//if it exists, this is 

	// Should we ignore the OnStopMakingConnection unless forced?
	bool bPreservePinPreviewConnection;

	/** Pin visibility mode */
	SGraphEditor::EPinVisibility PinVisibility;

	/** List of pins currently being hovered over */
	TSet< FEdGraphPinReference > CurrentHoveredPins;

	/** Time since the last mouse enter/exit on a pin */
	double TimeWhenMouseEnteredPin;
	double TimeWhenMouseLeftPin;

	/** Sometimes the panel draws a preview connector; e.g. when the user is connecting pins */
	TArray< FGraphPinHandle > PreviewConnectorFromPins;
	FVector2D PreviewConnectorEndpoint;

	/** Last mouse position seen, used for paint-centric highlighting */
	FVector2D SavedMousePosForOnPaintEventLocalSpace;
	
	/** The overlap results from the previous OnPaint call */
	FVector2D PreviousFrameSavedMousePosForSplineOverlap;
	FGraphSplineOverlapResult PreviousFrameSplineOverlap;

	/** The mouse state from the last mouse move event, used to synthesize pin actions when hovering over a spline on the panel */
	FGeometry LastPointerGeometry;
	FPointerEvent LastPointerEvent;

	/** Invoked when we need to summon a context menu */
	FOnGetContextMenuFor OnGetContextMenuFor;

	/** Invoked when an actor is dropped onto the panel */
	SGraphEditor::FOnDropActor OnDropActor;

	/** Invoked when a streaming level is dropped onto the panel */
	SGraphEditor::FOnDropStreamingLevel OnDropStreamingLevel;

	/** What to do when a node is double-clicked */
	FSingleNodeEvent OnNodeDoubleClicked;

	/** Bouncing curve */
	FCurveSequence BounceCurve;

	/** Geometry cache */
	mutable FVector2D CachedAllottedGeometryScaledSize;

	/** Invoked when text is being committed on panel to verify it */
	FOnNodeVerifyTextCommit OnVerifyTextCommit;

	/** Invoked when text is committed on panel */
	FOnNodeTextCommitted OnTextCommitted;

	/** Invoked when the panel is updated */
	FOnUpdateGraphPanel OnUpdateGraphPanel;

	/** Called when the user generates a warning tooltip because a connection was invalid */
	SGraphEditor::FOnDisallowedPinConnection OnDisallowedPinConnection;

	/** Whether to draw the overlay indicating we're in PIE */
	bool bShowPIENotification;

	/** Whether to draw decorations for graph state (PIE / ReadOnly etc.) */
	TAttribute<bool> ShowGraphStateOverlay;

private:
	/** Ordered list of user actions, as they came in */
	TArray<FEdGraphEditAction> UserActions;

	/** Map of recently added nodes for the panel (maps from added nodes to UserActions indices) */
	TMap<const class UEdGraphNode*, int32> UserAddedNodes;

	/** Should the graph display all nodes in a read-only state (grayed)? This does not affect functionality of using them (IsEditable) */
	TAttribute<bool> DisplayAsReadOnly;

	FOnGraphChanged::FDelegate MyRegisteredGraphChangedDelegate;
	FDelegateHandle            MyRegisteredGraphChangedDelegateHandle;
private:
	/** Called when PIE begins */
	void OnBeginPIE( const bool bIsSimulating );

	/** Called when PIE ends */
	void OnEndPIE( const bool bIsSimulating );

	/** Called when watched graph changes */
	void OnGraphChanged( const FEdGraphEditAction& InAction );

	/** Update all selected nodes position by provided vector2d */
	void UpdateSelectedNodesPositions(FVector2D PositionIncrement);

	/** Handle updating the spline hover state */
	void OnSplineHoverStateChanged(const FGraphSplineOverlapResult& NewSplineHoverState);

	/** Returns the pin that we're considering as hovered if we are hovering over a spline; may be null */
	class SGraphPin* GetBestPinFromHoveredSpline() const;

	/** Handle to timer callback that allows the UI to refresh it's arrangement each tick, allows animations to occur within the UI */
	TWeakPtr<FActiveTimerHandle> ActiveTimerHandleInvalidatePerTick;

	/** Amount of time left to invalidate the UI per tick */
	float TimeLeftToInvalidatePerTick;
};
