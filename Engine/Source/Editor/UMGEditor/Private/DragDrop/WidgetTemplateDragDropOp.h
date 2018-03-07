// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"
#include "DragAndDrop/DecoratedDragDropOp.h"

class FWidgetTemplate;

/**
 * This drag drop operation allows widget templates from the palate to be dragged and dropped into the designer
 * or the widget hierarchy in order to spawn new widgets.
 */
class FWidgetTemplateDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FWidgetTemplateDragDropOp, FDecoratedDragDropOp)

	/** The template to create an instance */
	TSharedPtr<FWidgetTemplate> Template;

	/** Constructs the drag drop operation */
	static TSharedRef<FWidgetTemplateDragDropOp> New(const TSharedPtr<FWidgetTemplate>& InTemplate);
};
