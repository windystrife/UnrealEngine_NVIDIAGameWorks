// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Layout/SlateRect.h"
#include "Layout/Geometry.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Styling/SlateColor.h"
#include "Layout/ArrangedWidget.h"
#include "Layout/Margin.h"
#include "Animation/CurveSequence.h"
#include "SlotBase.h"
#include "Layout/Children.h"
#include "Widgets/SPanel.h"
#include "Styling/CoreStyle.h"
#include "Framework/Commands/InputChord.h"
#include "GraphEditor.h"
#include "Templates/ScopedPointer.h"
#include "Layout/ArrangedChildren.h"
#include "Types/PaintArgs.h"
#include "EditorStyleSet.h"
#include "Layout/LayoutUtils.h"
#include "MarqueeOperation.h"
#include "UniquePtr.h"

class FActiveTimerHandle;
class FScopedTransaction;
class FSlateWindowElementList;
struct FMarqueeOperation;
struct Rect;

//@TODO: Too generic of a name to expose at this scope
typedef class UObject* SelectedItemType;

// Level of detail for graph rendering (lower numbers are 'further away' with fewer details)
namespace EGraphRenderingLOD
{
	enum Type
	{
		// Detail level when zoomed all the way out (all performance optimizations enabled)
		LowestDetail,

		// Detail level that text starts being disabled because it is unreadable
		LowDetail,

		// Detail level at which text starts to get hard to read but is still drawn
		MediumDetail,

		// Detail level when zoomed in at 1:1
		DefaultDetail,

		// Detail level when fully zoomed in (past 1:1)
		FullyZoomedIn,
	};
}

// Context passed in when getting popup info
struct FNodeInfoContext
{
public:
	bool bSelected;
};

// Entry for an overlay brush in the node panel
struct FOverlayBrushInfo
{
public:
	/** Brush to draw */
	const FSlateBrush* Brush;
	/** Scale of animation to apply */
	FVector2D AnimationEnvelope;
	/** Offset origin of the overlay from the widget */
	FVector2D OverlayOffset;

public:
	FOverlayBrushInfo()
		: Brush(NULL)
		, AnimationEnvelope(0.0f, 0.0f)
		, OverlayOffset(0.f, 0.f)
	{
	}

	FOverlayBrushInfo(const FSlateBrush* InBrush)
		: Brush(InBrush)
		, AnimationEnvelope(0.0f, 0.0f)
		, OverlayOffset(0.f, 0.f)
	{
	}

	FOverlayBrushInfo(const FSlateBrush* InBrush, float HorizontalBounce)
		: Brush(InBrush)
		, AnimationEnvelope(HorizontalBounce, 0.0f)
		, OverlayOffset(0.f, 0.f)
	{
	}
};

// Entry for an overlay widget in the node panel
struct FOverlayWidgetInfo
{
public:
	/** Widget to use */
	TSharedPtr<SWidget> Widget;

	/** Offset origin of the overlay from the widget */
	FVector2D OverlayOffset;

public:
	FOverlayWidgetInfo()
		: Widget(nullptr)
		, OverlayOffset(0.f, 0.f)
	{
	}

	FOverlayWidgetInfo(TSharedPtr<SWidget> InWidget)
		: Widget(InWidget)
		, OverlayOffset(0.f, 0.f)
	{
	}
};

// Entry for an information popup in the node panel
struct FGraphInformationPopupInfo
{
public:
	const FSlateBrush* Icon;
	FLinearColor BackgroundColor;
	FString Message;
public:
	FGraphInformationPopupInfo(const FSlateBrush* InIcon, FLinearColor InBackgroundColor, const FString& InMessage)
		: Icon(InIcon)
		, BackgroundColor(InBackgroundColor)
		, Message(InMessage)
	{
	}
};

/**
 * Interface for ZoomLevel values
 * Provides mapping for a range of virtual ZoomLevel values to actual node scaling values
 */
struct FZoomLevelsContainer
{
	/** 
	 * @param InZoomLevel virtual zoom level value
	 * 
	 * @return associated scaling value
	 */
	virtual float						GetZoomAmount(int32 InZoomLevel) const = 0;
	
	/** 
	 * @param InZoomAmount scaling value
	 * 
	 * @return nearest ZoomLevel mapping for provided scale value
	 */
	virtual int32						GetNearestZoomLevel(float InZoomAmount) const = 0;
	
	/** 
	 * @param InZoomLevel virtual zoom level value
	 * 
	 * @return associated friendly name
	 */
	virtual FText						GetZoomText(int32 InZoomLevel) const = 0;
	
	/** 
	 * @return count of supported zoom levels
	 */
	virtual int32						GetNumZoomLevels() const = 0;
	
	/** 
	 * @return the optimal(1:1) zoom level value, default zoom level for the graph
	 */
	virtual int32						GetDefaultZoomLevel() const = 0;
	
	/** 
	 * @param InZoomLevel virtual zoom level value
	 *
	 * @return associated LOD value
	 */
	virtual EGraphRenderingLOD::Type	GetLOD(int32 InZoomLevel) const = 0;

	// Necessary for Mac OS X to compile 'delete <pointer_to_this_object>;'
	virtual ~FZoomLevelsContainer( void ) {};
};

struct GRAPHEDITOR_API FGraphSelectionManager
{
	FGraphPanelSelectionSet SelectedNodes;

	/** Invoked when the selected graph nodes have changed. */
	SGraphEditor::FOnSelectionChanged OnSelectionChanged;
public:
	/** @return the set of selected nodes */
	const FGraphPanelSelectionSet& GetSelectedNodes() const;

	/** Select just the specified node */
	void SelectSingleNode(SelectedItemType Node);

	/** Reset the selection state of all nodes */
	void ClearSelectionSet();

	/** Returns true if any nodes are selected */
	bool AreAnyNodesSelected() const
	{
		return SelectedNodes.Num() > 0;
	}

	/** Changes the selection set to contain exactly all of the passed in nodes */
	void SetSelectionSet(FGraphPanelSelectionSet& NewSet);

	/**
	 * Add or remove a node from the selection set
	 *
	 * @param Node      Node the affect.
	 * @param bSelect   true to select the node; false to unselect.
	 */
	void SetNodeSelection(SelectedItemType Node, bool bSelect);

	/** @return true if Node is selected; false otherwise */
	bool IsNodeSelected(SelectedItemType Node) const;

	// Handle the selection mechanics of starting to drag a node
	void StartDraggingNode(SelectedItemType NodeBeingDragged, const FPointerEvent& MouseEvent);

	// Handle the selection mechanics when a node is clicked on
	void ClickedOnNode(SelectedItemType Node, const FPointerEvent& MouseEvent);
};

/**
 * This class is designed to serve as the base class for a panel/canvas that contains interactive widgets
 * which can be selected and moved around by the user.  It also manages zooming and panning, allowing a larger
 * virtual space to be used for the widget placement.
 *
 * The user is responsible for creating widgets (which must be derived from SNode) and any custom drawing
 * code desired.  The other main restriction is that each SNode instance must have a unique UObject* associated
 * with it.
 */
namespace ENodeZone
{
	enum Type
	{
		TopLeft,
		TopCenter,
		TopRight,

		Left,
		Center,
		Right,

		BottomLeft,
		BottomCenter,
		BottomRight,

		Count
	};
}

class GRAPHEDITOR_API SNodePanel : public SPanel
{
public:

	class SNode : public SPanel
	{
	public:

		/** A slot that support alignment of content and padding and z-order */
		class FNodeSlot : public TSlotBase<FNodeSlot>
		{
		public:
			FNodeSlot()
				: TSlotBase<FNodeSlot>()
				, HAlignment(HAlign_Fill)
				, VAlignment(VAlign_Fill)
				, SlotPadding(0.0f)
				, Offset( FVector2D::ZeroVector )
				, AllowScale( true )
			{ }

			FNodeSlot& HAlign( EHorizontalAlignment InHAlignment )
			{
				HAlignment = InHAlignment;
				return *this;
			}

			FNodeSlot& VAlign( EVerticalAlignment InVAlignment )
			{
				VAlignment = InVAlignment;
				return *this;
			}

			FNodeSlot& Padding( const TAttribute<FMargin> InPadding )
			{
				SlotPadding = InPadding;
				return *this;
			}

			FNodeSlot& SlotOffset( const TAttribute<FVector2D> InOffset )
			{
				Offset = InOffset;
				return *this;
			}

			FNodeSlot& SlotSize( const TAttribute<FVector2D> InSize )
			{
				Size = InSize;
				return *this;
			}

			FNodeSlot& AllowScaling( const TAttribute<bool> InAllowScale )
			{
				AllowScale = InAllowScale;
				return *this;
			}

		public:

			/** The child widget contained in this slot. */
			ENodeZone::Type Zone;
			EHorizontalAlignment HAlignment;
			EVerticalAlignment VAlignment;
			TAttribute<FMargin> SlotPadding;
			TAttribute<FVector2D> Offset;
			TAttribute<FVector2D> Size;
			TAttribute<bool> AllowScale;
		};

		typedef TSet<TWeakPtr<SNodePanel::SNode>> FNodeSet;

		// SPanel Interface
		virtual FChildren* GetChildren() override
		{
			return &Children;
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			for( int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex )
			{
				if( Children[ ChildIndex ].Zone == ENodeZone::Center )
				{
					const FNodeSlot& CenterZone = Children[ ChildIndex ];
					const EVisibility ChildVisibility = CenterZone.GetWidget()->GetVisibility();
					if( ChildVisibility != EVisibility::Collapsed )
					{
						return ( CenterZone.GetWidget()->GetDesiredSize() + CenterZone.SlotPadding.Get().GetDesiredSize() ) * DesiredSizeScale.Get();
					}
				}
			}
			return FVector2D::ZeroVector;
		}

		virtual float GetRelativeLayoutScale(const FSlotBase& Child, float LayoutScaleMultiplier) const override
		{
			const FNodeSlot& ThisSlot = static_cast<const FNodeSlot&>(Child);
			if ( !ThisSlot.AllowScale.Get() )
			{
				// Child slots that do not allow zooming should scale themselves to negate the node panel's zoom.
				TSharedPtr<SNodePanel> ParentPanel = GetParentPanel();
				if (ParentPanel.IsValid())
				{					
					return 1.0f/ParentPanel->GetZoomAmount();
				}
			}

			return 1.0f;
		}

		virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override
		{
			for( int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex )
			{
				const FNodeSlot& CurChild = Children[ChildIndex];
				const EVisibility ChildVisibility = CurChild.GetWidget()->GetVisibility();
				if ( ArrangedChildren.Accepts(ChildVisibility) )
				{
					const FMargin SlotPadding(CurChild.SlotPadding.Get());
					// If this child is not allowed to scale, its scale relative to its parent should undo the parent widget's scaling.
					FVector2D Size;

					if( CurChild.Size.IsSet() )
					{
						Size = CurChild.Size.Get();
					}
					else
					{
						AlignmentArrangeResult XResult = AlignChild<Orient_Horizontal>(AllottedGeometry.GetLocalSize().X, CurChild, SlotPadding);
						AlignmentArrangeResult YResult = AlignChild<Orient_Vertical>(AllottedGeometry.GetLocalSize().Y, CurChild, SlotPadding);
						Size = FVector2D( XResult.Size, YResult.Size );
					}
					const FArrangedWidget ChildGeom =
						AllottedGeometry.MakeChild(
						CurChild.GetWidget(),
						CurChild.Offset.Get(),
						Size,
						GetRelativeLayoutScale(CurChild, AllottedGeometry.Scale)
					);
					ArrangedChildren.AddWidget( ChildVisibility, ChildGeom );
				}
			}
		}

		virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override
		{
			FArrangedChildren ArrangedChildren( EVisibility::Visible );
			{
				ArrangeChildren( AllottedGeometry, ArrangedChildren );
			}

			int32 MaxLayerId = LayerId;
			for( int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ++ChildIndex )
			{
				const FArrangedWidget& CurWidget = ArrangedChildren[ ChildIndex ];

				if (!IsChildWidgetCulled(MyCullingRect, CurWidget))
				{
					const int32 CurWidgetsMaxLayerId = CurWidget.Widget->Paint(Args.WithNewParent(this), CurWidget.Geometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, ShouldBeEnabled(bParentEnabled));
					MaxLayerId = FMath::Max(MaxLayerId, CurWidgetsMaxLayerId);
				}
			}
			return MaxLayerId;
		}
		// End of SPanel Interface

		FNodeSlot& GetOrAddSlot( const ENodeZone::Type SlotId )
		{
			// Return existing
			for( int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex )
			{
				if( Children[ ChildIndex ].Zone == SlotId )
				{
					return Children[ ChildIndex ];
				}
			}
			// Add Zone
			FNodeSlot& NewSlot = *new FNodeSlot();
			NewSlot.Zone = SlotId;
			Children.Add( &NewSlot );

			return NewSlot;
		}

		FNodeSlot* GetSlot( const ENodeZone::Type SlotId )
		{
			FNodeSlot* Result = nullptr;
			// Return existing
			for( int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex )
			{
				if( Children[ ChildIndex ].Zone == SlotId )
				{
					Result = &Children[ ChildIndex ];
					break;
				}
			}
			return Result;
		}

		void RemoveSlot( const ENodeZone::Type SlotId )
		{
			for( int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex )
			{
				if( Children[ ChildIndex ].Zone == SlotId )
				{
					Children.RemoveAt( ChildIndex );
					break;
				}
			}
		}

		/**
		* @param NewPosition	The Node should be relocated to this position in the graph panel
		* @param NodeFilter		Set of nodes to prevent movement on, after moving successfully a node is added to this set.
		*/
		virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter )
		{
		}

		/** @return the Node's position within the graph */
		virtual FVector2D GetPosition() const
		{
			return FVector2D(0.0f, 0.0f);
		}

		/** @return a user-specified comment on this node; the comment gets drawn in a bubble above the node */
		virtual FString GetNodeComment() const
		{
			return FString();
		}

		/** @return The backing object, used as a unique identifier in the selection set, etc... */
		virtual UObject* GetObjectBeingDisplayed() const
		{
			return NULL;
		}

		/** @return The brush to use for drawing the shadow for this node */
		virtual const FSlateBrush* GetShadowBrush(bool bSelected) const
		{
			return bSelected ? FEditorStyle::GetBrush(TEXT("Graph.Node.ShadowSelected")) : FEditorStyle::GetBrush(TEXT("Graph.Node.Shadow"));
		}

		/** Populate the brushes array with any overlay brushes to render */
		virtual void GetOverlayBrushes(bool bSelected, const FVector2D WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const
		{
		}

		/** Populate the widgets array with any overlay widgets to render */
		virtual TArray<FOverlayWidgetInfo> GetOverlayWidgets(bool bSelected, const FVector2D& WidgetSize) const
		{
			return TArray<FOverlayWidgetInfo>();
		}

		/** Populate the popups array with any popups to render */
		virtual void GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const
		{
		}	

		/** Returns true if this node is dependent on the location of other nodes (it can only depend on the location of first-pass only nodes) */
		virtual bool RequiresSecondPassLayout() const
		{
			return false;
		}

		/** Performs second pass layout; only called if RequiresSecondPassLayout returned true */
		virtual void PerformSecondPassLayout(const TMap< UObject*, TSharedRef<SNode> >& InNodeToWidgetLookup) const
		{
		}

		/** Return false if this node should not be culled. Useful for potentially large nodes that may be improperly culled. */
		virtual bool ShouldAllowCulling() const
		{
			return true;
		}

		/** return if the node can be selected, by pointing given location */
		virtual bool CanBeSelected(const FVector2D& MousePositionInNode) const
		{
			return true;
		}

		/** Called when user interaction has completed */
		virtual void EndUserInteraction() const {}

		/** 
		 *	override, when area used to select node, should be different, than it's size
		 *	e.g. comment node - only title bar is selectable
		 *	return size of node used for Marquee selecting
		 */
		virtual FVector2D GetDesiredSizeForMarquee() const
		{
			return GetDesiredSize();
		}

		// Returns node sort depth, defaults to and is generally 0 for most nodes
		virtual int32 GetSortDepth() const { return 0; }

		// Node Sort Operator
		bool operator < ( const SNodePanel::SNode& NodeIn ) const
		{
			return GetSortDepth() < NodeIn.GetSortDepth();
		}

		void SetParentPanel(const TSharedPtr<SNodePanel>& InParent)
		{
			ParentPanelPtr = InParent;
		}

	protected:
		SNode()
		: BorderImage( FCoreStyle::Get().GetBrush( "NoBorder" ) )
		, BorderBackgroundColor( FEditorStyle::GetColor("Graph.ForegroundColor"))
		, DesiredSizeScale(FVector2D(1,1))
		{
		}

	protected:

		// SBorder Begin
		TAttribute<const FSlateBrush*> BorderImage;
		TAttribute<FSlateColor> BorderBackgroundColor;
		TAttribute<FVector2D> DesiredSizeScale;
		/** Whether or not to show the disabled effect when this border is disabled */
		TAttribute<bool> ShowDisabledEffect;
		/** Mouse event handlers */
		FPointerEventHandler MouseButtonDownHandler;
		FPointerEventHandler MouseButtonUpHandler;
		FPointerEventHandler MouseMoveHandler;
		FPointerEventHandler MouseDoubleClickHandler;
		// SBorder End

		// SPanel Begin
		/** The layout scale to apply to this widget's contents; useful for animation. */
		TAttribute<FVector2D> ContentScale;
		/** The color and opacity to apply to this widget and all its descendants. */
		TAttribute<FLinearColor> ColorAndOpacity;
		/** Optional foreground color that will be inherited by all of this widget's contents */
		TAttribute<FSlateColor> ForegroundColor;
		// SPanel End

	private:

		TSharedPtr<SNodePanel> GetParentPanel() const
		{
			return ParentPanelPtr.Pin();
		}

		TPanelChildren<FNodeSlot> Children;
		TWeakPtr<SNodePanel> ParentPanelPtr;

	};

	SNodePanel();

	// SPanel interface
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FChildren* GetChildren() override;
	// End of SPanel interface

	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual void OnFocusLost( const FFocusEvent& InFocusEvent ) override;
	virtual FReply OnTouchGesture( const FGeometry& MyGeometry, const FPointerEvent& GestureEvent ) override;
	virtual FReply OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override;
	virtual float GetRelativeLayoutScale(const FSlotBase& Child, float LayoutScaleMultiplier) const override;
	// End of SWidget interface
public:
	/**
	 * Is the given node being observed by a widget in this panel?
	 * 
	 * @param Node   The node to look for.
	 *
	 * @return True if the node is being observed by some widget in this panel; false otherwise.
	 */
	bool Contains(UObject* Node) const;

	/** @retun the zoom amount; e.g. a value of 0.25f results in quarter-sized nodes */
	float GetZoomAmount() const;
	/** @return Zoom level as a pretty string */
	FText GetZoomText() const;
	FSlateColor GetZoomTextColorAndOpacity() const;

	/** @return the view offset in graph space */
	FVector2D GetViewOffset() const;

	/** Given a coordinate in panel space (i.e. panel widget space), return the same coordinate in graph space while taking zoom and panning into account */
	FVector2D PanelCoordToGraphCoord(const FVector2D& PanelSpaceCoordinate) const;

	/** Restore the graph panel to the supplied view offset/zoom */
	void RestoreViewSettings(const FVector2D& InViewOffset, float InZoomAmount);

	/** Get the grid snap size */
	static float GetSnapGridSize();

	/** 
	 * Zooms out to fit either all nodes or only the selected ones.
	 * @param bOnlySelection Whether to zoom to fit around only the current selection (if false, will zoom to the extents of all nodes)
	 */
	void ZoomToFit(bool bOnlySelection);

	/** Get the bounding area for the currently selected nodes 
		@return false if nothing is selected					*/
	bool GetBoundsForSelectedNodes(/*out*/ class FSlateRect& Rect, float Padding = 0.0f);

	/** @return the position where where nodes should be pasted (i.e. from the clipboard) */
	FVector2D GetPastePosition() const;

	/** Ask panel to scroll to location */
	void RequestDeferredPan(const FVector2D& TargetPosition);

	/** If it is focusing on a particular object */
	bool HasDeferredObjectFocus() const;

	/** Returns the current LOD level of this panel, based on the zoom factor */
	EGraphRenderingLOD::Type GetCurrentLOD() const { return CurrentLOD; }

	/** Returns if the panel has been panned or zoomed since the last update */
	bool HasMoved() const;

	/** Returns all the panel children rather than only visible */
	FChildren* GetAllChildren();

protected:
	/** Initialize members */
	void Construct();

	/**
	 * Zooms to the specified target rect
	 * @param TopLeft The top left corner of the target rect
	 * @param BottomRight The bottom right corner of the target rect
	 */
	void ZoomToTarget(const FVector2D& TopLeft, const FVector2D& BottomRight);

	/** Update the new view offset location  */
	void UpdateViewOffset(const FGeometry& MyGeometry, const FVector2D& TargetPosition);

	/** Compute much panel needs to change to pan to location  */
	static FVector2D ComputeEdgePanAmount(const FGeometry& MyGeometry, const FVector2D& MouseEvent);

	/** Given a coordinate in graph space (e.g. a node's position), return the same coordinate in widget space while taking zoom and panning into account */
	FVector2D GraphCoordToPanelCoord(const FVector2D& GraphSpaceCoordinate) const;

	/** Given a rectangle in panel space, return a rectangle in graph space. */
	FSlateRect PanelRectToGraphRect(const FSlateRect& PanelSpaceRect) const;

	/** 
	 * Lets the CanvasPanel know that the user is interacting with a node.
	 * 
	 * @param InNodeToDrag   The node that the user wants to drag
	 * @param GrabOffset     Where within the node the user grabbed relative to split between inputs and outputs.
	 */
	virtual void OnBeginNodeInteraction(const TSharedRef<SNode>& InNodeToDrag, const FVector2D& GrabOffset);

	/** 
	 * Lets the CanvasPanel know that the user has ended interacting with a node.
	 * 
	 * @param InNodeToDrag   The node that the user was to dragging
	 */
	virtual void OnEndNodeInteraction(const TSharedRef<SNode>& InNodeToDrag);

	/** Figure out which nodes intersect the marquee rectangle */
	void FindNodesAffectedByMarquee(FGraphPanelSelectionSet& OutAffectedNodes) const;

	/**
	 * Apply the marquee operation to the current selection
	 *
	 * @param InMarquee            The marquee operation to apply.
	 * @param CurrentSelection     The selection before the marquee operation.
	 * @param OutNewSelection      The selection resulting from Marquee being applied to CurrentSelection.
	 */
	static void ApplyMarqueeSelection(const FMarqueeOperation& InMarquee, const FGraphPanelSelectionSet& CurrentSelection, FGraphPanelSelectionSet& OutNewSelection);

	/**
	 * On the next tick, centers and selects the widget associated with the object if it exists
	 *
	 * @param ObjectToSelect	The object to select, and potentially center on
	 * @param bCenter			Whether or not to center the graph node
	 */
	void SelectAndCenterObject(const UObject* ObjectToSelect, bool bCenter);

	/**
	 * On the next tick, centers the widget associated with the object if it exists
	 *
	 * @param ObjectToCenter	The object to center
	 */
	void CenterObject(const UObject* ObjectToCenter);

	/** Add a slot to the CanvasPanel dynamically */
	virtual void AddGraphNode(const TSharedRef<SNode>& NodeToAdd);

	/** Remove all nodes from the panel */
	virtual void RemoveAllNodes();
	
	/** Populate visibile children array */
	virtual void PopulateVisibleChildren(const FGeometry& AllottedGeometry);

	/** Arrange child nodes - allows derived classes to supply non-node children in OnArrangeChildren */
	virtual void ArrangeChildNodes(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const;

	// Paint the background as lines
	void PaintBackgroundAsLines(const FSlateBrush* BackgroundImage, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32& DrawLayerId) const;

	// Paint the well shadow (around the perimeter)
	void PaintSurroundSunkenShadow(const FSlateBrush* ShadowImage, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 DrawLayerId) const;

	// Paint the marquee selection rectangle
	void PaintMarquee(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 DrawLayerId) const;

	// Paint the software mouse if necessary
	void PaintSoftwareCursor(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 DrawLayerId) const;

	// Paint a comment bubble
	void PaintComment(const FString& CommentText, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 DrawLayerId, const FLinearColor& CommentTinting, float& HeightAboveNode, const FWidgetStyle& InWidgetStyle ) const;

	/** Determines if a specified node is not visually relevant. */
	bool IsNodeCulled(const TSharedRef<SNode>& Node, const FGeometry& AllottedGeometry) const;

protected:
	///////////
	// INTERFACE TO IMPLEMENT
	///////////
	/** @return the widget in the summoned context menu that should be focused. */
	virtual TSharedPtr<SWidget> OnSummonContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) { return TSharedPtr<SWidget>(); }
	virtual bool OnHandleLeftMouseRelease(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) { return false; }
protected:
	/**
	 * Get the bounds of the given node
	 * @return True if successful
	 */
	bool GetBoundsForNode(const UObject* InNode, /*out*/ FVector2D& MinCorner, /*out*/ FVector2D& MaxCorner, float Padding = 0.0f) const;

	/**
	 * Get the bounds of the selected nodes 
	 * @param bSelectionSetOnly If true, limits the query to just the selected nodes.  Otherwise it does all nodes.
	 * @return True if successful
	 */
	bool GetBoundsForNodes(bool bSelectionSetOnly, /*out*/ FVector2D& MinCorner, /*out*/ FVector2D& MaxCorner, float Padding = 0.0f);

	/**
	 * Scroll the view to the desired location 
	 * @return true when the desired location is reached
	 */
	bool ScrollToLocation(const FGeometry& MyGeometry, FVector2D DesiredCenterPosition, const float InDeltaTime);

	/**
	 * Zoom to fit the desired size 
	 * @return true when zoom fade has completed & fits the desired size
	 */
	bool ZoomToLocation(const FVector2D& CurrentSizeWithoutZoom, const FVector2D& DesiredSize, bool bDoneScrolling);

	/**
	 * Change zoom level by the specified zoom level delta, about the specified origin.
	 */
	void ChangeZoomLevel(int32 ZoomLevelDelta, const FVector2D& WidgetSpaceZoomOrigin, bool bOverrideZoomLimiting);

	// Should be called whenever the zoom level has changed
	void PostChangedZoom();

	// Fires up a per-tick function to zoom the graph to fit
	void RequestZoomToFit();

	// Cancels any active zoom-to-fit action
	void CancelZoomToFit();
protected:
	// The interface for mapping ZoomLevel values to actual node scaling values
	TUniquePtr<FZoomLevelsContainer> ZoomLevels;

	/** The position within the graph at which the user is looking */
	FVector2D ViewOffset;

	/** The position within the graph at which the user was looking last tick */
	FVector2D OldViewOffset;

	/** How zoomed in/out we are. e.g. 0.25f results in quarter-sized nodes. */
	int32 ZoomLevel;

	/** Previous Zoom Level */
	int32 PreviousZoomLevel;

	/** The actual scalar zoom amount last tick */
	float OldZoomAmount;

	/** Are we panning the view at the moment? */
	bool bIsPanning;

	/** Are we zooming the view with trackpad at the moment? */
	bool bIsZoomingWithTrackpad;

	/** The graph node widgets owned by this panel */
	TSlotlessChildren<SNode> Children;
	TSlotlessChildren<SNode> VisibleChildren;

	/** The node that the user is dragging. Null when they are not dragging a node. */
	TWeakPtr<SNode> NodeUnderMousePtr;

	/** Where in the title the user grabbed to initiate the drag */
	FVector2D NodeGrabOffset;

	/** The total distance that the mouse has been dragged while down */
	float TotalMouseDelta;

	/** The Y component of mouse drag (used when zooming) */
	float TotalMouseDeltaY;

	/** Offset in the panel the user started the LMB+RMB zoom from */
	FVector2D ZoomStartOffset;

	/** Cumulative magnify delta from trackpad gesture */
	float TotalGestureMagnify;
public:
	/** Nodes selected in this instance of the editor; the selection is per-instance of the GraphEditor */
	FGraphSelectionManager SelectionManager;
protected:
	/** A pending marquee operation if it's active */
	FMarqueeOperation Marquee;

	/** Is the graph editable (can nodes be moved, etc...)? */
	TAttribute<bool> IsEditable;

	/** Given a node, find the corresponding widget */
	TMap< UObject*, TSharedRef<SNode> > NodeToWidgetLookup;

	/** If not empty and a part of this panel, this node will be selected and brought into view on the next Tick */
	TSet<const UObject*> DeferredSelectionTargetObjects;
	/** If non-null and a part of this panel, this node will be brought into view on the next Tick */
	const UObject* DeferredMovementTargetObject;

	/** Deferred zoom to selected node extents */
	bool bDeferredZoomToSelection;

	/** Deferred zoom to node extents */
	bool bDeferredZoomToNodeExtents;

	/** Zoom selection padding */
	float ZoomPadding;

	/** Allow continous zoom interpolation? */
	bool bAllowContinousZoomInterpolation;

	/** Teleport immediately, or smoothly scroll when doing a deferred zoom */
	bool bTeleportInsteadOfScrollingWhenZoomingToFit;

	/** Fade on zoom for graph */
	FCurveSequence ZoomLevelGraphFade;

	/** Curve that handles fading the 'Zoom +X' text */
	FCurveSequence ZoomLevelFade;

	/** The position where we should paste when a user executes the paste command. */
	FVector2D PastePosition;

	/** Position to pan to */
	FVector2D DeferredPanPosition;

	/** true if pending request for deferred panning */
	bool bRequestDeferredPan;

	/**	The current position of the software cursor */
	FVector2D SoftwareCursorPosition;

	/**	Whether the software cursor should be drawn */
	bool bShowSoftwareCursor;

	/** Current LOD level for nodes/pins */
	EGraphRenderingLOD::Type CurrentLOD;

	/** Invoked when the user may be attempting to spawn a node using a shortcut */
	SGraphEditor::FOnSpawnNodeByShortcut OnSpawnNodeByShortcut;

	/** The last key chord detected in this graph panel */
	FInputChord LastKeyChordDetected;

	/** The current transaction for undo/redo */
	TSharedPtr<FScopedTransaction> ScopedTransactionPtr;

	/** Cached geometry for use within the active timer */
	FGeometry CachedGeometry;

private:
	/** Active timer that handles deferred zooming until the target zoom is reached */
	EActiveTimerReturnType HandleZoomToFit(double InCurrentTime, float InDeltaTime);

private:
	/** The handle to the active timer */
	TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;

	/** Zoom target rectangle */
	FVector2D ZoomTargetTopLeft;
	FVector2D ZoomTargetBottomRight;
};
