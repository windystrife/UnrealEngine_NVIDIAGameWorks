// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Sequencer.h"
#include "ISequencerEditTool.h"

class FSlateWindowElementList;

/**
 * Abstract base class for edit tools.
 */
class FSequencerEditTool
	: public ISequencerEditTool
{
public:

	FSequencerEditTool(FSequencer& InSequencer)
		: Sequencer(InSequencer)
	{ }

	/** Virtual destructor. */
	~FSequencerEditTool() { }

public:

	// ISequencerEditTool interface

	virtual FReply OnMouseButtonDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Unhandled();
	}

	virtual FReply OnMouseMove(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Unhandled();
	}

	virtual FReply OnMouseWheel(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Unhandled();
	}

	virtual void OnMouseCaptureLost() override
	{
		// do nothing
	}

	virtual void OnMouseEnter(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		// do nothing
	}

	virtual void OnMouseLeave(SWidget& OwnerWidget, const FPointerEvent& MouseEvent) override
	{
		// do nothing
	}

	virtual int32 OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const override
	{
		return LayerId;
	}

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override
	{
		return FCursorReply::Unhandled();
	}

	virtual ISequencer& GetSequencer() const override
	{
		return Sequencer;
	}

protected:

	/** This edit tool's sequencer */
	FSequencer& Sequencer;
};
