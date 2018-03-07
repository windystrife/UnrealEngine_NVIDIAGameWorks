// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "GraphEditorDragDropAction.h"

class SGraphNode;
class SGraphPanel;
class UEdGraph;

class GRAPHEDITOR_API FDragNode : public FGraphEditorDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE(FDragNode, FGraphEditorDragDropAction)

	static TSharedRef<FDragNode> New(const TSharedRef<SGraphPanel>& InGraphPanel, const TSharedRef<SGraphNode>& InDraggedNode);

	static TSharedRef<FDragNode> New(const TSharedRef<SGraphPanel>& InGraphPanel, const TArray< TSharedRef<SGraphNode> >& InDraggedNodes);
	
	// FGraphEditorDragDropAction interface
	virtual void HoverTargetChanged() override;
	virtual FReply DroppedOnNode(FVector2D ScreenPosition, FVector2D GraphPosition) override;
	virtual FReply DroppedOnPanel( const TSharedRef< SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) override;
	//virtual void OnDragBegin( const TSharedRef<class SGraphPin>& InPin) override;
	virtual void OnDragged (const class FDragDropEvent& DragDropEvent ) override;
	// End of FGraphEditorDragDropAction interface
	
	const TArray< TSharedRef<SGraphNode> > & GetNodes() const;

	virtual bool IsValidOperation() const;

protected:
	typedef FGraphEditorDragDropAction Super;

	UEdGraphNode* GetGraphNodeForSGraphNode(TSharedRef<SGraphNode>& SNode);

protected:

	/** graph panel */
	TSharedPtr<SGraphPanel> GraphPanel;

	/** our dragged nodes*/
	TArray< TSharedRef<SGraphNode> > DraggedNodes;

	/** Offset information for the decorator widget */
	FVector2D	DecoratorAdjust;

	/** if we can drop our node here*/
	bool bValidOperation;
};
