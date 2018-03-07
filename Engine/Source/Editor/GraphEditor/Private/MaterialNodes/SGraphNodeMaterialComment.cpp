// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MaterialNodes/SGraphNodeMaterialComment.h"
#include "Materials/MaterialExpressionComment.h"
#include "MaterialGraph/MaterialGraphNode_Comment.h"

void SGraphNodeMaterialComment::Construct(const FArguments& InArgs, class UMaterialGraphNode_Comment* InNode)
{
	SGraphNodeComment::Construct(SGraphNodeComment::FArguments(), InNode);

	this->CommentNode = InNode;
}

void SGraphNodeMaterialComment::MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter)
{
	if( !NodeFilter.Find( SharedThis( this ) ))
	{
		SGraphNodeComment::MoveTo(NewPosition, NodeFilter);

		CommentNode->MaterialExpressionComment->MaterialExpressionEditorX = CommentNode->NodePosX;
		CommentNode->MaterialExpressionComment->MaterialExpressionEditorY = CommentNode->NodePosY;
		CommentNode->MaterialExpressionComment->MarkPackageDirty();
		CommentNode->MaterialDirtyDelegate.ExecuteIfBound();
	}
}
