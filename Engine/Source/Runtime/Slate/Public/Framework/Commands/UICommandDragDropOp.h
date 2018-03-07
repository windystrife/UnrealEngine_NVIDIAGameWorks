// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"
#include "Framework/Commands/UICommandInfo.h"

class SWidget;

/**
 * A drag drop operation for UI Commands
 */
class SLATE_API FUICommandDragDropOp
	: public FDragDropOperation
{
public:

	DRAG_DROP_OPERATOR_TYPE(FUICommandDragDropOp, FDragDropOperation)

	static TSharedRef<FUICommandDragDropOp> New( TSharedRef<const FUICommandInfo> InCommandInfo, FName InOriginMultiBox, TSharedPtr<SWidget> CustomDectorator, FVector2D DecoratorOffset );

	FUICommandDragDropOp( TSharedRef<const FUICommandInfo> InUICommand, FName InOriginMultiBox, TSharedPtr<SWidget> InCustomDecorator, FVector2D DecoratorOffset )
		: UICommand( InUICommand )
		, OriginMultiBox( InOriginMultiBox )
		, CustomDecorator( InCustomDecorator )
		, Offset( DecoratorOffset )
	{ }

	/**
	 * Sets a delegate that will be called when the command is dropped
	 */
	void SetOnDropNotification( FSimpleDelegate InOnDropNotification ) { OnDropNotification = InOnDropNotification; }

	/** FDragDropOperation interface */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;
	virtual void OnDragged( const class FDragDropEvent& DragDropEvent ) override;
	virtual void OnDrop( bool bDropWasHandled, const FPointerEvent& MouseEvent ) override;

public:

	/** UI command being dragged */
	TSharedPtr< const FUICommandInfo > UICommand;

	/** Multibox the UI command was dragged from if any*/
	FName OriginMultiBox;

	/** Custom decorator to display */
	TSharedPtr<SWidget> CustomDecorator;

	/** Offset from the cursor where the decorator should be displayed */
	FVector2D Offset;

	/** Delegate called when the command is dropped */
	FSimpleDelegate OnDropNotification;
};
