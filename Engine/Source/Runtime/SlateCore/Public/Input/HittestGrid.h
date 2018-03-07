// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
#include "Layout/ArrangedWidget.h"
#include "Layout/Clipping.h"
#include "Input/Events.h"
#include "Widgets/SWidget.h"

class FArrangedChildren;

class ICustomHitTestPath
{
public:
	virtual ~ICustomHitTestPath(){}

	virtual TArray<FWidgetAndPointer> GetBubblePathAndVirtualCursors( const FGeometry& InGeometry, FVector2D DesktopSpaceCoordinate, bool bIgnoreEnabledStatus ) const = 0;

	virtual void ArrangeChildren( FArrangedChildren& ArrangedChildren ) const = 0;

	virtual TSharedPtr<struct FVirtualPointerPosition> TranslateMouseCoordinateFor3DChild( const TSharedRef<SWidget>& ChildWidget, const FGeometry& ViewportGeometry, const FVector2D& ScreenSpaceMouseCoordinate, const FVector2D& LastScreenSpaceMouseCoordinate ) const = 0;
};

class SLATECORE_API FHittestGrid
{
public:

	FHittestGrid();
	~FHittestGrid();

	/**
	 * Given a Slate Units coordinate in virtual desktop space, perform a hittest
	 * and return the path along which the corresponding event would be bubbled.
	 */
	TArray<FWidgetAndPointer> GetBubblePath( FVector2D DesktopSpaceCoordinate, float CursorRadius, bool bIgnoreEnabledStatus );

	/**
	 * Clear the hittesting area and prepare to execute a new frame.
	 * Depending on monitor arrangement, the area to be hittested could begin
	 * in the negative coordinates.
	 *
	 * @param HittestArea       Size in Slate Units of the area we are considering for hittesting.
	 */
	void ClearGridForNewFrame(const FSlateRect& HittestArea);

	void PushClip(const FSlateClippingZone& ClippingZone); 

	void PopClip();

	/** Add Widget into the hittest data structure so that we can later make queries about it. */
	int32 InsertWidget(const int32 ParentHittestIndex, const EVisibility& Visibility, const FArrangedWidget& Widget, const FVector2D InWindowOffset, int32 LayerId);

	void InsertCustomHitTestPath( TSharedRef<ICustomHitTestPath> CustomHitTestPath, int32 WidgetIndex );

	/**
	 * Finds the next focusable widget by searching through the hit test grid
	 *
	 * @param StartingWidget  This is the widget we are starting at, and navigating from.
	 * @param Direction       The direction we should search in.
	 * @param NavigationReply The Navigation Reply to specify a boundary rule for the search.
	 * @param RuleWidget      The Widget that is applying the boundary rule, used to get the bounds of the Rule.
	 */
	TSharedPtr<SWidget> FindNextFocusableWidget(const FArrangedWidget& StartingWidget, const EUINavigation Direction, const FNavigationReply& NavigationReply, const FArrangedWidget& RuleWidget);

private:

	/**
	 * All the available space is partitioned into Cells.
	 * Each Cell contains a list of widgets that overlap the cell.
	 * The list is ordered from back to front.
	 */
	struct FCell
	{
		TArray<int32> CachedWidgetIndexes;
	};

	/**
	 * @todo umg: can we eliminate this?
	 *
	 * FNodes are a duplicate of the widget tree, with each widget
	 * that is encountered on the way to outputting a hit-testable element
	 * being recorded such that the event bubbling path can be reconstructed.
	 */
	struct FCachedWidget;

	/** Shared arguments to helper functions. */
	struct FGridTestingParams;

	/** Helper functions */
	bool IsValidCellCoord(const FIntPoint& CellCoord) const;
	bool IsValidCellCoord(const int32 XCoord, const int32 YCoord) const;

	/** Bubble path, distance to leafmost widget from point of picking, and LayerId of leafmost widget. */
	struct FWidgetPathAndDist
	{
		/** Ctor */
		FWidgetPathAndDist( float InDistanceSq = -1.0f )
		: DistToTopWidgetSq(InDistanceSq)
		, LayerId(0)
		{}

		FWidgetPathAndDist(const TArray<FWidgetAndPointer>& InPath, float InDistanceSq, int32 InLayerId)
		: BubblePath(InPath)
		, DistToTopWidgetSq(InDistanceSq)
		, LayerId(InLayerId)
		{}

		void Clear()
		{
			BubblePath.Reset();
			DistToTopWidgetSq = -1.0f;
		}

		bool IsValidPath() const
		{
			return DistToTopWidgetSq >= 0.0f && BubblePath.Num() > 0;
		}

		TArray<FWidgetAndPointer> BubblePath;
		float DistToTopWidgetSq;
		int32 LayerId;
	};

	FWidgetPathAndDist GetWidgetPathAndDist(const FGridTestingParams& Params, const bool bIgnoreEnabledStatus) const;

	/** Return Value for GetHitIndexFromCellIndex */
	struct FIndexAndDistance
	{
		FIndexAndDistance( int32 InIndex = INDEX_NONE, float InDistanceSq = 0 )
		: WidgetIndex(InIndex)
		, DistanceSqToWidget(InDistanceSq)
		{}
		int32 WidgetIndex;
		float DistanceSqToWidget;
	};
	FIndexAndDistance GetHitIndexFromCellIndex(const FGridTestingParams& Params) const;

	TArray<FWidgetAndPointer> GetBubblePathFromHitIndex(const int32 HitIndex, const bool bIgnoreEnabledStatus) const;
	
	friend class SWidgetReflector;

	/** @returns true if Child is a descendant of Parent. */
	bool IsDescendantOf(const TSharedRef<SWidget> Parent, const FCachedWidget& Child);

	/** Utility function for searching for the next focusable widget. */
	template<typename TCompareFunc, typename TSourceSideFunc, typename TDestSideFunc>
	TSharedPtr<SWidget> FindFocusableWidget(const FSlateRect WidgetRect, const FSlateRect SweptRect, int32 AxisIndex, int32 Increment, const EUINavigation Direction, const FNavigationReply& NavigationReply, TCompareFunc CompareFunc, TSourceSideFunc SourceSideFunc, TDestSideFunc DestSideFunc);

	/** Constrains a float position into the grid coordinate. */
	FIntPoint GetCellCoordinate(FVector2D Position);

	/** Access a cell at coordinates X, Y. Coordinates are row and column indexes. */
	FORCEINLINE_DEBUGGABLE FCell& CellAt(const int32 X, const int32 Y)
	{
		check(( Y*NumCells.X + X ) < Cells.Num());
		return Cells[Y*NumCells.X + X];
	}

	FORCEINLINE_DEBUGGABLE const FCell& CellAt( const int32 X, const int32 Y ) const
	{
		check(( Y*NumCells.X + X ) < Cells.Num());
		return Cells[Y*NumCells.X + X];
	}

	/** All the widgets and their arranged geometries encountered this frame. */
	TSharedRef< TArray<FCachedWidget> > WidgetsCachedThisFrame;

	/** The cells that make up the space partition. */
	TArray<FCell> Cells;

	/** Where the 0,0 of the upper-left-most cell corresponds to in desktop space. */
	FVector2D GridOrigin;

	/** The size of the grid in cells. */
	FIntPoint NumCells;

	/** The clipping manager that manages any clipping for hit testable widgets. */
	FSlateClippingManager ClippingManager;

	void LogGrid() const;

	static void LogChildren(int32 Index, int32 IndentLevel, const TArray<FCachedWidget>& WidgetsCachedThisFrame);
};
