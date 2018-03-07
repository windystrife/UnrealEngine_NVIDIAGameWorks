// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "DragNode.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "EdGraph/EdGraph.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"

TSharedRef<FDragNode> FDragNode::New(const TSharedRef<SGraphPanel>& InGraphPanel, const TSharedRef<SGraphNode>& InDraggedNode)
{
	TSharedRef<FDragNode> Operation = MakeShareable(new FDragNode);
	Operation->GraphPanel = InGraphPanel;
	Operation->DraggedNodes.Add(InDraggedNode);
	// adjust the decorator away from the current mouse location a small amount based on cursor size
	Operation->DecoratorAdjust = FSlateApplication::Get().GetCursorSize();
	Operation->Construct();
	return Operation;
}

TSharedRef<FDragNode> FDragNode::New(const TSharedRef<SGraphPanel>& InGraphPanel, const TArray< TSharedRef<SGraphNode> >& InDraggedNodes)
{
	TSharedRef<FDragNode> Operation = MakeShareable(new FDragNode);
	Operation->GraphPanel = InGraphPanel;
	Operation->DraggedNodes = InDraggedNodes;
	Operation->DecoratorAdjust = FSlateApplication::Get().GetCursorSize();
	Operation->Construct();
	return Operation;
}

const TArray< TSharedRef<SGraphNode> > & FDragNode::GetNodes() const
{
	return DraggedNodes;
}

bool FDragNode::IsValidOperation() const
{
	return bValidOperation;
}

UEdGraphNode* FDragNode::GetGraphNodeForSGraphNode(TSharedRef<SGraphNode>& SNode)
{
	return SNode->GetNodeObj();
}

void FDragNode::OnDragged(const class FDragDropEvent& DragDropEvent)
{
	FVector2D TargetPosition = DragDropEvent.GetScreenSpacePosition();

	// Reposition the info window to the dragged position
	CursorDecoratorWindow->MoveWindowTo(DragDropEvent.GetScreenSpacePosition() + DecoratorAdjust);
	// Request the active panel to scroll if required
	GraphPanel->RequestDeferredPan(TargetPosition);
}

void FDragNode::HoverTargetChanged()
{
	TArray<FPinConnectionResponse> UniqueMessages;
	UEdGraphNode* TargetNodeObj = GetHoveredNode();
	
	if (TargetNodeObj != NULL)
	{
		// Check the schema for connection responses
		for (TArray< TSharedRef<SGraphNode> >::TIterator NodeIterator(DraggedNodes); NodeIterator; ++NodeIterator)
		{
			UEdGraphNode* DraggedNodeObj = GetGraphNodeForSGraphNode(*NodeIterator);

			// The Graph object in which the nodes reside.
			UEdGraph* GraphObj = DraggedNodeObj->GetGraph();

			// Determine what the schema thinks about the action
			const FPinConnectionResponse Response = GraphObj->GetSchema()->CanMergeNodes( DraggedNodeObj, TargetNodeObj );

			UniqueMessages.AddUnique(Response);
		}
	}

	// Let the user know the status of dropping now
	if (UniqueMessages.Num() == 0)
	{
		bValidOperation = false;
		// Display the place a new node icon, we're not over a valid pin
		SetSimpleFeedbackMessage(
			FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")),
			FLinearColor::White,
			NSLOCTEXT("GraphEditor.Feedback", "DragNode", "This node cannot be placed here."));
	}
	else
	{
		bValidOperation = true;
		// Take the unique responses and create visual feedback for it
		TSharedRef<SVerticalBox> FeedbackBox = SNew(SVerticalBox);
		for (auto ResponseIt = UniqueMessages.CreateConstIterator(); ResponseIt; ++ResponseIt)
		{
			// Determine the icon
			const FSlateBrush* StatusSymbol = NULL;

			switch (ResponseIt->Response)
			{
			case CONNECT_RESPONSE_MAKE:
				StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
				break;

			case CONNECT_RESPONSE_DISALLOW:
			default:
				StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				bValidOperation = false;
				break;
			}

			// Add a new message row
			FeedbackBox->AddSlot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(3.0f)
				[
					SNew(SImage) .Image( StatusSymbol )
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock) .Text( ResponseIt->Message )
				]
			];
		}

		for (int32 i=0; i < DraggedNodes.Num(); i++)
		{
			FeedbackBox->AddSlot()
			.AutoHeight()
			[
				DraggedNodes[i]
			];
		}


		SetFeedbackMessage(FeedbackBox);
	}
}

FReply FDragNode::DroppedOnNode(FVector2D ScreenPosition, FVector2D GraphPosition)
{
	return FReply::Handled();
}

FReply FDragNode::DroppedOnPanel( const TSharedRef< SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph)
{
	return FReply::Handled();
}
