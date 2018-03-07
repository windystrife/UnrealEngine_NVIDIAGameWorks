// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/CursorReply.h"
#include "ISequencerInputHandler.h"

class FSlateWindowElementList;
class FVirtualTrackArea;
class ISequencer;
class SWidget;
struct ISequencerHotspot;

/**
 * Interface for drag and drop operations that are handled by edit tools in Sequencer.
 */
class ISequencerEditToolDragOperation
{
public:

	/**
	 * Called to initiate a drag
	 *
	 * @param MouseEvent		The associated mouse event for dragging
	 * @param LocalMousePos		The current location of the mouse, relative to the top-left corner of the physical track area
	 * @param VirtualTrackArea	A virtual track area that can be used for pixel->time conversions and hittesting
	 */
	virtual void OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) = 0;

	/**
	 * Notification called when the mouse moves while dragging
	 *
	 * @param MouseEvent		The associated mouse event for dragging
	 * @param LocalMousePos		The current location of the mouse, relative to the top-left corner of the physical track area
	 * @param VirtualTrackArea	A virtual track area that can be used for pixel->time conversions and hittesting
	 */
	virtual void OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) = 0;

	/** Called when a drag has ended
	 *
	 * @param MouseEvent		The associated mouse event for dragging
	 * @param LocalMousePos		The current location of the mouse, relative to the top-left corner of the physical track area
	 * @param VirtualTrackArea	A virtual track area that can be used for pixel->time conversions and hittesting
	 */
	virtual void OnEndDrag( const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) = 0;

	/** Request the cursor for this drag operation */
	virtual FCursorReply GetCursor() const = 0;

	/** Override to implement drag-specific paint logic */
	virtual int32 OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const = 0;

public:

	/** Virtual destructor. */
	virtual ~ISequencerEditToolDragOperation() { }
};


/**
 * Interface for edit tools in Sequencer.
 */
class ISequencerEditTool : public ISequencerInputHandler
{
public:

	// @todo sequencer: documentation needed
	virtual void OnMouseCaptureLost() = 0;
	virtual int32 OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const = 0;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const = 0;
	virtual ISequencer& GetSequencer() const = 0;
	virtual void OnMouseEnter(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;
	virtual void OnMouseLeave(SWidget& OwnerWidget, const FPointerEvent& MouseEvent) = 0;
	virtual FName GetIdentifier() const = 0;
	virtual bool CanDeactivate() const = 0;
	virtual const ISequencerHotspot* GetDragHotspot() const = 0;
	
public:

	/** Virtual destructor. */
	virtual ~ISequencerEditTool() { }
};
