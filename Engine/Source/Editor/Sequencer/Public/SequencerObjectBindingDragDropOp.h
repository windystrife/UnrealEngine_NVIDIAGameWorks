// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"
#include "GraphEditorDragDropAction.h"

struct FMovieSceneObjectBindingID;

class FSequencerObjectBindingDragDropOp : public FGraphEditorDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE( FSequencerDisplayNodeDragDropOpBase, FGraphEditorDragDropAction )

	virtual TArray<FMovieSceneObjectBindingID> GetDraggedBindings() const = 0;
};