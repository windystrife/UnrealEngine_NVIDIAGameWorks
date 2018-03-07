// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "ISequencerInputHandler.h"

class SWidget;

/**
 * Class responsible for handling input to multiple objects
 * that reside at the same level in the widget hierarchy.
 *
 * The sequencer track area is one such example of a single widget
 * that delegates its input handling to multiple sources (edit tool, or time slider controller).
 * To alleviate the complexity of handling input for such sources,
 * where each may fight for mouse capture, this class keeps track
 * of which handler captured the mouse and routs input accordingly.
 *
 * When no mouse capture is active, handlers are called sequentially
 * in the order they were added, until the event is handled.
 */
class FSequencerInputHandlerStack
{
public:

	FSequencerInputHandlerStack() : CapturedIndex(INDEX_NONE) {}

	/** Add a handler to the stack */
	void AddHandler(ISequencerInputHandler* Handler) { Handlers.Add(Handler); }

	/** Reset an existing entry in the stack to a new handler */
	void SetHandlerAt(int32 Index, ISequencerInputHandler* Handler)
	{
		if (Handlers.IsValidIndex(Index))
		{
			if (Handlers[Index] != Handler)
			{
				CapturedIndex = INDEX_NONE;
			}
			
			Handlers[Index] = Handler;
		}
	}

	/** Get the index of the currently captured handler, or INDEX_NONE */
	int32 GetCapturedIndex() const { return CapturedIndex; }

	/** Handle a mouse down */
	FReply HandleMouseButtonDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		return ProcessEvent(&ISequencerInputHandler::OnMouseButtonDown, OwnerWidget, MyGeometry, MouseEvent);
	}

	/** Handle a mouse up */
	FReply HandleMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		return ProcessEvent(&ISequencerInputHandler::OnMouseButtonUp, OwnerWidget, MyGeometry, MouseEvent);
	}

	/** Handle a mouse move */
	FReply HandleMouseMove(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		return ProcessEvent(&ISequencerInputHandler::OnMouseMove, OwnerWidget, MyGeometry, MouseEvent);
	}

	/** Handle a mouse wheel */
	FReply HandleMouseWheel(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		return ProcessEvent(&ISequencerInputHandler::OnMouseWheel, OwnerWidget, MyGeometry, MouseEvent);
	}


private:

	typedef FReply(ISequencerInputHandler::*InputHandlerFunction)(SWidget&, const FGeometry&, const FPointerEvent&);

	FReply ProcessEvent(InputHandlerFunction Function, SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		FReply Reply = FReply::Unhandled();

		// Give the captured index priority over everything
		if (CapturedIndex != INDEX_NONE && Handlers[CapturedIndex])
		{
			Reply = (Handlers[CapturedIndex]->*Function)(OwnerWidget, MyGeometry, MouseEvent);
			if (Reply.IsEventHandled())
			{
				return ProcessReply(Reply, CapturedIndex);
			}
		}

		for (int32 Index = 0; Index < Handlers.Num(); ++Index)
		{
			if (!Handlers[Index] || CapturedIndex == Index)
			{
				continue;
			}

			Reply = (Handlers[Index]->*Function)(OwnerWidget, MyGeometry, MouseEvent);
			if (Reply.IsEventHandled())
			{
				return ProcessReply(Reply, Index);
			}
		}

		return Reply;
	}

	const FReply& ProcessReply(const FReply& Reply, int32 ThisIndex)
	{
		if (Reply.GetMouseCaptor().IsValid())
		{
			CapturedIndex = ThisIndex;
		}
		else if (Reply.ShouldReleaseMouse())
		{
			CapturedIndex = INDEX_NONE;
		}
		return Reply;
	}

	/** Index of the handler that currently has the mouse captured */
	int32 CapturedIndex;

	/** Array o9f input handlers */
	TArray<ISequencerInputHandler*> Handlers;
};
