// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialGraphNode_Comment.cpp
=============================================================================*/

#include "MaterialGraph/MaterialGraphNode_Comment.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "Materials/MaterialExpressionComment.h"
#include "Framework/Commands/GenericCommands.h"

/////////////////////////////////////////////////////
// UMaterialGraphNode_Comment

UMaterialGraphNode_Comment::UMaterialGraphNode_Comment(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMaterialGraphNode_Comment::PostCopyNode()
{
	// Make sure the MaterialExpression goes back to being owned by the Material after copying.
	ResetMaterialExpressionOwner();
}

void UMaterialGraphNode_Comment::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		const FName PropertyName = PropertyChangedEvent.Property->GetFName();
		if (PropertyName == FName(TEXT("NodeComment")))
		{
			if (MaterialExpressionComment)
			{
				MaterialExpressionComment->Modify();
				MaterialExpressionComment->Text = NodeComment;
			}
		}
	}
}

void UMaterialGraphNode_Comment::PostEditImport()
{
	// Make sure this MaterialExpression is owned by the Material it's being pasted into.
	ResetMaterialExpressionOwner();
}

void UMaterialGraphNode_Comment::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!bDuplicateForPIE)
	{
		CreateNewGuid();
	}
}

void UMaterialGraphNode_Comment::PrepareForCopying()
{
	if (MaterialExpressionComment)
	{
		// Temporarily take ownership of the MaterialExpression, so that it is not deleted when cutting
		MaterialExpressionComment->Rename(NULL, this, REN_DontCreateRedirectors);
	}
}

void UMaterialGraphNode_Comment::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	if (Context.Node && !Context.Pin)
	{
		// frequently used common options
		Context.MenuBuilder->BeginSection("MaterialEditorCommentMenu");
		{
			Context.MenuBuilder->AddMenuEntry( FGenericCommands::Get().Delete );
			Context.MenuBuilder->AddMenuEntry( FGenericCommands::Get().Cut );
			Context.MenuBuilder->AddMenuEntry( FGenericCommands::Get().Copy );
			Context.MenuBuilder->AddMenuEntry( FGenericCommands::Get().Duplicate );
		}
		Context.MenuBuilder->EndSection();
	}
}

bool UMaterialGraphNode_Comment::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const
{
	return Schema->IsA(UMaterialGraphSchema::StaticClass());
}

void UMaterialGraphNode_Comment::PostPlacedNewNode()
{
	// always used for material comments
	MoveMode = ECommentBoxMode::GroupMovement;

	if (MaterialExpressionComment)
	{
		NodeComment = MaterialExpressionComment->Text;
		NodePosX = MaterialExpressionComment->MaterialExpressionEditorX;
		NodePosY = MaterialExpressionComment->MaterialExpressionEditorY;
		NodeWidth = MaterialExpressionComment->SizeX;
		NodeHeight = MaterialExpressionComment->SizeY;
		CommentColor = MaterialExpressionComment->CommentColor;
	}
}

void UMaterialGraphNode_Comment::OnRenameNode(const FString& NewName)
{
	// send property changed events
	UProperty* NodeCommentProperty = FindField<UProperty>(GetClass(), "NodeComment");
	if(NodeCommentProperty != NULL)
	{
		PreEditChange(NodeCommentProperty);

		NodeComment = NewName;

		FPropertyChangedEvent NodeCommentPropertyChangedEvent(NodeCommentProperty);
		PostEditChangeProperty(NodeCommentPropertyChangedEvent);
	}
}

void UMaterialGraphNode_Comment::ResizeNode(const FVector2D& NewSize)
{
	Super::ResizeNode(NewSize);

	// Set position as well since may have been resized from top corner
	MaterialExpressionComment->SizeX = NodeWidth;
	MaterialExpressionComment->SizeY = NodeHeight;
	MaterialExpressionComment->MaterialExpressionEditorX = NodePosX;
	MaterialExpressionComment->MaterialExpressionEditorY = NodePosY;
	MaterialExpressionComment->MarkPackageDirty();
	MaterialDirtyDelegate.ExecuteIfBound();
}

void UMaterialGraphNode_Comment::ResetMaterialExpressionOwner()
{
	if (MaterialExpressionComment)
	{
		// Ensures MaterialExpression is owned by the Material or Function
		UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(GetGraph());
		UObject* ExpressionOuter = MaterialGraph->Material;
		if (MaterialGraph->MaterialFunction)
		{
			ExpressionOuter = MaterialGraph->MaterialFunction;
		}
		MaterialExpressionComment->Rename(NULL, ExpressionOuter, REN_DontCreateRedirectors);

		// Set up the back pointer for newly created material nodes
		MaterialExpressionComment->GraphNode = this;
	}
}
