// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Rendering/SlateDrawBuffer.h"
#include "Rendering/DrawElements.h"


/* FSlateDrawBuffer interface
 *****************************************************************************/

FSlateWindowElementList& FSlateDrawBuffer::AddWindowElementList(TSharedRef<SWindow> ForWindow)
{
	TSharedPtr<FSlateWindowElementList> WindowElements;

	for ( int32 WindowIndex = 0; WindowIndex < WindowElementListsPool.Num(); ++WindowIndex )
	{
		WindowElements = WindowElementListsPool[WindowIndex];

		if ( WindowElements->GetWindow() == ForWindow )
		{
			WindowElementLists.Add(WindowElements);
			WindowElementListsPool.RemoveAtSwap(WindowIndex);

			WindowElements->ResetBuffers();

			return *WindowElements;
		}
	}

	WindowElements = MakeShareable(new FSlateWindowElementList(ForWindow));
	WindowElementLists.Add(WindowElements);

	return *WindowElements;
}

bool FSlateDrawBuffer::Lock()
{
	return FPlatformAtomics::InterlockedCompareExchange(&Locked, 1, 0) == 0;
}

void FSlateDrawBuffer::Unlock()
{
	FPlatformAtomics::InterlockedExchange(&Locked, 0);
}

void FSlateDrawBuffer::ClearBuffer()
{
	// Remove any window elements that are no longer valid.
	for ( int32 WindowIndex = 0; WindowIndex < WindowElementListsPool.Num(); ++WindowIndex )
	{
		if ( WindowElementListsPool[WindowIndex]->GetWindow().IsValid() == false )
		{
			WindowElementListsPool.RemoveAtSwap(WindowIndex);
			--WindowIndex;
		}
	}

	// Move all the window elements back into the pool.
	for ( TSharedPtr<FSlateWindowElementList> ExistingList : WindowElementLists )
	{
		if( ExistingList->GetWindow().IsValid() )
		{
			WindowElementListsPool.Add(ExistingList);
		}
	}

	WindowElementLists.Reset();
}
