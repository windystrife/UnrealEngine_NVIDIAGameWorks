// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/TaskGraphInterfaces.h"

////////////////////////////////////
// Render fences
////////////////////////////////////

 /**
 * Used to track pending rendering commands from the game thread.
 */
class RENDERCORE_API FRenderCommandFence
{
public:

	/**
	 * Adds a fence command to the rendering command queue.
	 * Conceptually, the pending fence count is incremented to reflect the pending fence command.
	 * Once the rendering thread has executed the fence command, it decrements the pending fence count.
	 */
	void BeginFence();

	/**
	 * Waits for pending fence commands to retire.
	 * @param bProcessGameThreadTasks, if true we are on a short callstack where it is safe to process arbitrary game thread tasks while we wait
	 */
	void Wait(bool bProcessGameThreadTasks = false) const;

	// return true if the fence is complete
	bool IsFenceComplete() const;

private:
	/** Graph event that represents completion of this fence **/
	mutable FGraphEventRef CompletionEvent;
};
