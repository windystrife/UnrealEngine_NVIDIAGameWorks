// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/CursorReply.h"
#include "Input/DragAndDrop.h"
#include "UObject/GCObject.h"

class SObjectWidget;
class SWidget;
class UDragDropOperation;
class UGameViewportClient;

/**
 * This is the drag/drop class used for UMG, all UMG drag drop operations utilize this operation.
 * It supports moving a UObject payload and using a UWidget decorator.
 */
class FUMGDragDropOp : public FGameDragDropOperation, public FGCObject
{
public:
	DRAG_DROP_OPERATOR_TYPE(FUMGDragDropOp, FGameDragDropOperation)
	
	static TSharedRef<FUMGDragDropOp> New(UDragDropOperation* Operation, const FVector2D &CursorPosition, const FVector2D &ScreenPositionOfNode, float DPIScale, TSharedPtr<SObjectWidget> SourceUserWidget);

	FUMGDragDropOp();

	// Begin FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End FGCObject

	virtual void OnDrop( bool bDropWasHandled, const FPointerEvent& MouseEvent ) override;
	virtual void OnDragged( const class FDragDropEvent& DragDropEvent ) override;
	virtual FCursorReply OnCursorQuery() override;
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

	UDragDropOperation* GetOperation() const { return DragOperation; }

protected:
	virtual void Construct() override;

private:

	// Raw pointer to the drag operation, kept alive by AddReferencedObjects.
	UDragDropOperation* DragOperation;

	/** Source User Widget */
	TSharedPtr<SObjectWidget> SourceUserWidget;

	/** The viewport this drag/drop operation is associated with. */
	UGameViewportClient* GameViewport;

	/** The widget used during the drag/drop action to show something being dragged. */
	TSharedPtr<SWidget> DecoratorWidget;

	/** The offset to use when dragging the object so that it says the same distance away from the mouse. */
	FVector2D MouseDownOffset;

	/** The starting screen location where the drag operation started. */
	FVector2D StartingScreenPos;

	/** Allows smooth interpolation of the dragged visual over a few frames. */
	double StartTime;
};
