// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSlateWindowElementList;
class SWindow;

/**
 * Implements a draw buffer for Slate.
 */
class SLATECORE_API FSlateDrawBuffer
{
public:

	/** Default constructor. */
	explicit FSlateDrawBuffer( )
		: Locked(0)
	{ }

public:

	/** Removes all data from the buffer. */
	void ClearBuffer();

	/**
	 * Creates a new FSlateWindowElementList and returns a reference to it so it can have draw elements added to it
	 *
	 * @param ForWindow    The window for which we are creating a list of paint elements.
	 */
	FSlateWindowElementList& AddWindowElementList(TSharedRef<SWindow> ForWindow);

	/**
	 * Gets all window element lists in this buffer.
	 */
	TArray< TSharedPtr<FSlateWindowElementList> >& GetWindowElementLists()
	{
		return WindowElementLists;
	}

	/** 
	 * Locks the draw buffer.  Indicates that the viewport is in use.
	 *
	 * @return true if the viewport could be locked.  False otherwise.
	 * @see Unlock
	 */
	bool Lock( );

	/**
	 * Unlocks the buffer.  Indicates that the buffer is free.
	 *
	 * @see Lock
	 */
	void Unlock( );

protected:

	// List of window element lists.
	TArray< TSharedPtr<FSlateWindowElementList> > WindowElementLists;

	// List of window element lists that we store from the previous frame 
	// that we restore if they're requested again.
	TArray< TSharedPtr<FSlateWindowElementList> > WindowElementListsPool;

	// 1 if this buffer is locked, 0 otherwise.
	volatile int32 Locked;

public:
	FVector2D ViewOffset;
};
