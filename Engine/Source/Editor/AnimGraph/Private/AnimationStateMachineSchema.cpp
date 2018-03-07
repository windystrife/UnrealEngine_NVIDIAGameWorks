// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimationStateMachineSchema.cpp
=============================================================================*/

#include "AnimationStateMachineSchema.h"
#include "Layout/SlateRect.h"
#include "Animation/AnimationAsset.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphNode_Comment.h"

#include "AnimationStateMachineGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimStateEntryNode.h"
#include "AnimStateNodeBase.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimStateConduitNode.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "ScopedTransaction.h"
#include "GraphEditorActions.h"

#include "EdGraphUtilities.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Framework/Commands/GenericCommands.h"

#define LOCTEXT_NAMESPACE "AnimationStateMachineSchema"

/////////////////////////////////////////////////////

TSharedPtr<FEdGraphSchemaAction_NewStateNode> AddNewStateNodeAction(FGraphContextMenuBuilder& ContextMenuBuilder, const FText& Category, const FText& MenuDesc, const FText& Tooltip, const int32 Grouping = 0)
{
	TSharedPtr<FEdGraphSchemaAction_NewStateNode> NewStateNode( new FEdGraphSchemaAction_NewStateNode(Category, MenuDesc, Tooltip, Grouping) );
	ContextMenuBuilder.AddAction( NewStateNode );
	return NewStateNode;
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_NewStateNode

UEdGraphNode* FEdGraphSchemaAction_NewStateNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	UEdGraphNode* ResultNode = NULL;

	// If there is a template, we actually use it
	if (NodeTemplate != NULL)
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "K2_AddNode", "Add Node") );
		ParentGraph->Modify();
		if (FromPin)
		{
			FromPin->Modify();
		}

		// set outer to be the graph so it doesn't go away
		NodeTemplate->Rename(NULL, ParentGraph);
		ParentGraph->AddNode(NodeTemplate, true, bSelectNewNode);

		NodeTemplate->CreateNewGuid();
		NodeTemplate->PostPlacedNewNode();
		NodeTemplate->AllocateDefaultPins();
		NodeTemplate->AutowireNewNode(FromPin);

		NodeTemplate->NodePosX = Location.X;
		NodeTemplate->NodePosY = Location.Y;
		//@TODO: ANIM: SNAP_GRID isn't centralized or exposed - NodeTemplate->SnapToGrid(SNAP_GRID);

		ResultNode = NodeTemplate;

		ResultNode->SetFlags(RF_Transactional);

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ParentGraph);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	return ResultNode;
}

void FEdGraphSchemaAction_NewStateNode::AddReferencedObjects( FReferenceCollector& Collector )
{
	FEdGraphSchemaAction::AddReferencedObjects( Collector );

	// These don't get saved to disk, but we want to make sure the objects don't get GC'd while the action array is around
	Collector.AddReferencedObject( NodeTemplate );
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_NewStateComment

UEdGraphNode* FEdGraphSchemaAction_NewStateComment::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	// Add menu item for creating comment boxes
	UEdGraphNode_Comment* CommentTemplate = NewObject<UEdGraphNode_Comment>();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph);

	FVector2D SpawnLocation = Location;

	FSlateRect Bounds;
	if ((Blueprint != NULL) && FKismetEditorUtilities::GetBoundsForSelectedNodes(Blueprint, Bounds, 50.0f))
	{
		CommentTemplate->SetBounds(Bounds);
		SpawnLocation.X = CommentTemplate->NodePosX;
		SpawnLocation.Y = CommentTemplate->NodePosY;
	}

	return FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Comment>(ParentGraph, CommentTemplate, SpawnLocation);
}

/////////////////////////////////////////////////////
// UAnimationStateMachineSchema

UAnimationStateMachineSchema::UAnimationStateMachineSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PC_Exec = TEXT("exec");
}

void UAnimationStateMachineSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	// Create the entry/exit tunnels
	FGraphNodeCreator<UAnimStateEntryNode> NodeCreator(Graph);
	UAnimStateEntryNode* EntryNode = NodeCreator.CreateNode();
	NodeCreator.Finalize();
	SetNodeMetaData(EntryNode, FNodeMetadata::DefaultGraphNode);

	if (UAnimationStateMachineGraph* StateMachineGraph = CastChecked<UAnimationStateMachineGraph>(&Graph))
	{
		StateMachineGraph->EntryNode = EntryNode;
	}
}

const FPinConnectionResponse UAnimationStateMachineSchema::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	// Make sure the pins are not on the same node
	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Both are on the same node"));
	}

	// Connect entry node to a state is OK
	const bool bPinAIsEntry = PinA->GetOwningNode()->IsA(UAnimStateEntryNode::StaticClass());
	const bool bPinBIsEntry = PinB->GetOwningNode()->IsA(UAnimStateEntryNode::StaticClass());
	const bool bPinAIsStateNode = PinA->GetOwningNode()->IsA(UAnimStateNodeBase::StaticClass());
	const bool bPinBIsStateNode = PinB->GetOwningNode()->IsA(UAnimStateNodeBase::StaticClass());

	if (bPinAIsEntry || bPinBIsEntry)
	{
		if (bPinAIsEntry && bPinBIsStateNode)
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, TEXT(""));
		}

		if (bPinBIsEntry && bPinAIsStateNode)
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, TEXT(""));
		}

		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Entry must connect to a state node"));
	}


	const bool bPinAIsTransition = PinA->GetOwningNode()->IsA(UAnimStateTransitionNode::StaticClass());
	const bool bPinBIsTransition = PinB->GetOwningNode()->IsA(UAnimStateTransitionNode::StaticClass());

	if (bPinAIsTransition && bPinBIsTransition)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Cannot wire a transition to a transition"));
	}
	
	// Compare the directions
	bool bDirectionsOK = false;

	if ((PinA->Direction == EGPD_Input) && (PinB->Direction == EGPD_Output))
	{
		bDirectionsOK = true;
	}
	else if ((PinB->Direction == EGPD_Input) && (PinA->Direction == EGPD_Output))
	{
		bDirectionsOK = true;
	}

	/*
	if (!bDirectionsOK)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Directions are not compatible"));
	}
	*/

	// Transitions are exclusive (both input and output), but states are not
	if (bPinAIsTransition)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, TEXT(""));
	}
	else if (bPinBIsTransition)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, TEXT(""));
	}
	else if (!bPinAIsTransition && !bPinBIsTransition)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE, TEXT("Create a transition"));
	}
	else
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
	}
}

bool UAnimationStateMachineSchema::TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	if (PinB->Direction == PinA->Direction)
	{
		if (UAnimStateNodeBase* Node = Cast<UAnimStateNodeBase>(PinB->GetOwningNode()))
		{
			if (PinA->Direction == EGPD_Input)
			{
				PinB = Node->GetOutputPin();
			}
			else
			{
				PinB = Node->GetInputPin();
			}
		}
	}

	const bool bModified = UEdGraphSchema::TryCreateConnection(PinA, PinB);

	if (bModified)
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(PinA->GetOwningNode());
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}

	return bModified;
}

bool UAnimationStateMachineSchema::CreateAutomaticConversionNodeAndConnections(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	UAnimStateNodeBase* NodeA = Cast<UAnimStateNodeBase>(PinA->GetOwningNode());
	UAnimStateNodeBase* NodeB = Cast<UAnimStateNodeBase>(PinB->GetOwningNode());

	if ((NodeA != NULL) && (NodeB != NULL) 
		&& (NodeA->GetInputPin() != NULL) && (NodeA->GetOutputPin() != NULL)
		&& (NodeB->GetInputPin() != NULL) && (NodeB->GetOutputPin() != NULL))
	{
		UAnimStateTransitionNode* TransitionNode = FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate<UAnimStateTransitionNode>(NodeA->GetGraph(), NewObject<UAnimStateTransitionNode>(), FVector2D(0.0f, 0.0f), false);

		if (PinA->Direction == EGPD_Output)
		{
			TransitionNode->CreateConnections(NodeA, NodeB);
		}
		else
		{
			TransitionNode->CreateConnections(NodeB, NodeA);
		}

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(TransitionNode->GetBoundGraph());
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

		return true;
	}

	return false;
}

void UAnimationStateMachineSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	// Add state node
	{
		TSharedPtr<FEdGraphSchemaAction_NewStateNode> Action = AddNewStateNodeAction(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddState", "Add State..."), LOCTEXT("AddStateTooltip", "A new state"));
		Action->NodeTemplate = NewObject<UAnimStateNode>(ContextMenuBuilder.OwnerOfTemporaries);
	}

	// Add conduit node
	{
		TSharedPtr<FEdGraphSchemaAction_NewStateNode> Action = AddNewStateNodeAction(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddConduit", "Add Conduit..."), LOCTEXT("AddConduitTooltip", "A new conduit state"));
		Action->NodeTemplate = NewObject<UAnimStateConduitNode>(ContextMenuBuilder.OwnerOfTemporaries);
	}

	// Entry point (only if doesn't already exist)
	{
		bool bHasEntry = false;
		for (auto NodeIt = ContextMenuBuilder.CurrentGraph->Nodes.CreateConstIterator(); NodeIt; ++NodeIt)
		{
			UEdGraphNode* Node = *NodeIt;
			if (const UAnimStateEntryNode* StateNode = Cast<UAnimStateEntryNode>(Node))
			{
				bHasEntry = true;
				break;
			}
		}

		if (!bHasEntry)
		{
			TSharedPtr<FEdGraphSchemaAction_NewStateNode> Action = AddNewStateNodeAction(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddEntryPoint", "Add Entry Point..."), LOCTEXT("AddEntryPointTooltip", "Define State Machine's Entry Point"));
			Action->NodeTemplate = NewObject<UAnimStateEntryNode>(ContextMenuBuilder.OwnerOfTemporaries);
		}
	}

	// Add Comment
	if (!ContextMenuBuilder.FromPin)
	{
		UBlueprint* OwnerBlueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ContextMenuBuilder.CurrentGraph);
		const bool bIsManyNodesSelected = (FKismetEditorUtilities::GetNumberOfSelectedNodes(OwnerBlueprint) > 0);
		const FText MenuDescription = bIsManyNodesSelected ? LOCTEXT("CreateCommentSelection", "Create Comment from Selection") : LOCTEXT("AddComment", "Add Comment...");
		const FText ToolTip = LOCTEXT("CreateCommentSelectionTooltip", "Create a resizeable comment box around selected nodes.");

		TSharedPtr<FEdGraphSchemaAction_NewStateComment> NewComment( new FEdGraphSchemaAction_NewStateComment(FText::GetEmpty(), MenuDescription, ToolTip, 0) );
		ContextMenuBuilder.AddAction( NewComment );
	}
}

EGraphType UAnimationStateMachineSchema::GetGraphType(const UEdGraph* TestEdGraph) const
{
	return GT_StateMachine;
}

void UAnimationStateMachineSchema::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
	check(CurrentGraph);
	UBlueprint* OwnerBlueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(CurrentGraph);

	if (InGraphNode != NULL)
	{
		MenuBuilder->BeginSection("AnimationStateMachineNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));
		{
			if (!bIsDebugging)
			{
				// Node contextual actions
				MenuBuilder->AddMenuEntry( FGenericCommands::Get().Delete );
				MenuBuilder->AddMenuEntry( FGenericCommands::Get().Cut );
				MenuBuilder->AddMenuEntry( FGenericCommands::Get().Copy );
				MenuBuilder->AddMenuEntry( FGenericCommands::Get().Duplicate );
				MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().ReconstructNodes );
				MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().BreakNodeLinks );
				if(InGraphNode->bCanRenameNode)
				{
					MenuBuilder->AddMenuEntry( FGenericCommands::Get().Rename );
				}
			}
		}
		MenuBuilder->EndSection();
	}

	Super::GetContextMenuActions(CurrentGraph, InGraphNode, InGraphPin, MenuBuilder, bIsDebugging);
}

FLinearColor UAnimationStateMachineSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FLinearColor::White;
}

void UAnimationStateMachineSchema::GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const
{
	DisplayInfo.PlainName = FText::FromString( Graph.GetName() );
	DisplayInfo.DisplayName = DisplayInfo.PlainName;
}

void UAnimationStateMachineSchema::DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const
{
	UAnimationAsset* Asset = FAssetData::GetFirstAsset<UAnimationAsset>(Assets);
	if(Asset != NULL && GetNodeClassForAsset(Asset->GetClass()))
	{
		// Spawn new state
		UAnimStateNode* NewStateNode = FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate<UAnimStateNode>(Graph, NewObject<UAnimStateNode>(), GraphPosition);

		// Try to name the state close to the asset
		FEdGraphUtilities::RenameGraphToNameOrCloseToName(NewStateNode->BoundGraph, Asset->GetName());

		// Change the current graph context to the inner graph, so that the rest of the drag drop happens inside it
		FVector2D NewGraphPosition = FVector2D(-300.0f, 0.0f);
		UAnimationGraphSchema::SpawnNodeFromAsset(Asset, NewGraphPosition, NewStateNode->BoundGraph, NewStateNode->GetPoseSinkPinInsideState());
	}
}

void UAnimationStateMachineSchema::DroppedAssetsOnNode(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphNode* Node) const
{
	UAnimationAsset* Asset = FAssetData::GetFirstAsset<UAnimationAsset>(Assets);
	UAnimStateNode* StateNodeUnderCursor = Cast<UAnimStateNode>(Node);
	if(Asset != NULL && StateNodeUnderCursor != NULL)
	{
		// Dropped onto a state machine state; try some user friendly resposnes
		if (UEdGraphPin* PosePin = StateNodeUnderCursor->GetPoseSinkPinInsideState())
		{
			if (PosePin->LinkedTo.Num() > 0)
			{
				//@TODO: A2REMOVAL: This doesn't do anything
				/*
				// Try dropping in onto the node attached to the sink inside the state
				check(PosePin->LinkedTo[0] != NULL);
				UA2Node* A2Node = Cast<UA2Node>(PosePin->LinkedTo[0]->GetOwningNode());
				if(A2Node != NULL)
				{
					UAnimationGraphSchema::UpdateNodeWithAsset(A2Node, Asset);
				}
				*/
			}
			else
			{
				// Try dropping onto the pose pin, since nothing is currently connected
				const FVector2D NewGraphPosition(-300.0f, 0.0f);
				UAnimationGraphSchema::SpawnNodeFromAsset(Asset, NewGraphPosition, StateNodeUnderCursor->BoundGraph, PosePin);
			}
		}
	}
}

void UAnimationStateMachineSchema::DroppedAssetsOnPin(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphPin* Pin) const
{
	// unused for state machines?
}

void UAnimationStateMachineSchema::GetAssetsNodeHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphNode* HoverNode, FString& OutTooltipText, bool& OutOkIcon) const 
{ 
	UAnimationAsset* Asset = FAssetData::GetFirstAsset<UAnimationAsset>(Assets);
	if ((Asset == NULL) || (HoverNode == NULL))
	{
		OutTooltipText = TEXT("");
		OutOkIcon = false;
		return;
	}

	const UAnimStateNode* StateNodeUnderCursor = Cast<const UAnimStateNode>(HoverNode);
	if (StateNodeUnderCursor != NULL)
	{
		OutOkIcon = true;
		OutTooltipText = FString::Printf(TEXT("Change node to play %s"), *(Asset->GetName()));
	}
	else
	{
		OutTooltipText = TEXT("");
		OutOkIcon = false;
	}
}

void UAnimationStateMachineSchema::GetAssetsPinHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphPin* HoverPin, FString& OutTooltipText, bool& OutOkIcon) const 
{ 
	// unused for state machines?

	OutTooltipText = TEXT("");
	OutOkIcon = false;
}

void UAnimationStateMachineSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "GraphEd_BreakNodeLinks", "Break Node Links"));

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(&TargetNode);
	Super::BreakNodeLinks(TargetNode);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void UAnimationStateMachineSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "GraphEd_BreakPinLinks", "Break Pin Links"));
	// cache this here, as BreakPinLinks can trigger a node reconstruction invalidating the TargetPin references
	UBlueprint* const Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(TargetPin.GetOwningNode());
	Super::BreakPinLinks(TargetPin, bSendsNodeNotification);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void UAnimationStateMachineSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin)
{
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "GraphEd_BreakSinglePinLink", "Break Pin Link"));
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(TargetPin->GetOwningNode());
	Super::BreakSinglePinLink(SourcePin, TargetPin);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

#undef LOCTEXT_NAMESPACE
