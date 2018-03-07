// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ViewportDragOperation.h"
#include "ViewportInteractionTypes.h"

UViewportDragOperationComponent::UViewportDragOperationComponent() : 
	Super(),
	DragOperation(nullptr)
{

}

UViewportDragOperation* UViewportDragOperationComponent::GetDragOperation()
{
	return DragOperation;
}

void UViewportDragOperationComponent::SetDragOperationClass( const TSubclassOf<UViewportDragOperation> InDragOperation )
{
	DragOperationSubclass = InDragOperation;
}

void UViewportDragOperationComponent::StartDragOperation()
{
	if ( DragOperationSubclass )
	{
		// Reset the drag operation to make sure we start a new one
		ClearDragOperation();
		// Create the drag object with the latest class
		DragOperation = NewObject<UViewportDragOperation>( ( UObject* ) GetTransientPackage(), DragOperationSubclass );
	}
}

void UViewportDragOperationComponent::ClearDragOperation()
{
	if (DragOperation)
	{
		DragOperation->MarkPendingKill();
	}

	DragOperation = nullptr;
}

bool UViewportDragOperationComponent::IsDragging() const
{
	return DragOperation != nullptr;
}