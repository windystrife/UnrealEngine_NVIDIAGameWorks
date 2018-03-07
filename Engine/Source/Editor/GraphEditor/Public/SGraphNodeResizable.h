// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "SGraphNode.h"

class FScopedTransaction;

class SGraphNodeResizable : public SGraphNode
{
public:

	/**
	 * The resizable window zone the user is interacting with
	 */
	enum EResizableWindowZone
	{
		CRWZ_NotInWindow		= 0,
		CRWZ_InWindow			= 1,
		CRWZ_RightBorder		= 2,
		CRWZ_BottomBorder		= 3,
		CRWZ_BottomRightBorder	= 4,
		CRWZ_LeftBorder			= 5,
		CRWZ_TopBorder			= 6,
		CRWZ_TopLeftBorder		= 7,
		CRWZ_TopRightBorder		= 8,
		CRWZ_BottomLeftBorder	= 9,
		CRWZ_TitleBar			= 10,
	};

	//~ Begin SWidget Interface
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;
	//~ End SWidget Interface
	
protected:

	/** Find the current window zone the mouse is in */
	virtual SGraphNodeResizable::EResizableWindowZone FindMouseZone(const FVector2D& LocalMouseCoordinates) const;

	/** @return true if the current window zone is considered a selection area */
	bool InSelectionArea() const { return InSelectionArea(MouseZone); }

	/** @return true if the passed zone is a selection area */
	bool InSelectionArea(EResizableWindowZone InZone) const;

	/** Function to store anchor point before resizing the node. The node will be anchored to this point when resizing happens*/
	void InitNodeAnchorPoint();

	/** Function to fetch the corrected node position based on anchor point*/
	FVector2D GetCorrectedNodePosition() const;

	/** Get the current titlebar size */
	virtual float GetTitleBarHeight() const;

	/** Return smallest desired node size */
	virtual FVector2D GetNodeMinimumSize() const;

	/** Return largest desired node size */
	virtual FVector2D GetNodeMaximumSize() const;

	//** Return slate rect border for hit testing */
	virtual FSlateRect GetHitTestingBorder() const;

protected:

	/** The non snapped size of the node for fluid resizing */
	FVector2D DragSize;

	/** The desired size of the node set during a drag */
	FVector2D UserSize;

	/** The original size of the node while resizing */
	FVector2D StoredUserSize;

	/** The resize transaction */
	TSharedPtr<FScopedTransaction> ResizeTransactionPtr;

	/** Anchor point used to correct node position on resizing the node*/
	FVector2D NodeAnchorPoint;

	/** The current window zone the mouse is in */
	EResizableWindowZone MouseZone;

	/** If true the user is actively dragging the node */
	bool bUserIsDragging;

};
