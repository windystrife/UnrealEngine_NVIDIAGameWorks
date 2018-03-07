// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PhysicsAssetGraphSchema.h"
#include "GlobalEditorCommonCommands.h"
#include "PhysicsAssetGraph.h"
#include "PhysicsAssetGraphNode_Bone.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsAssetConnectionDrawingPolicy.h"
#include "PhysicsAssetGraphNode_Constraint.h"
#include "PhysicsAssetGraph.h"
#include "PhysicsAssetEditor.h"

UPhysicsAssetGraphSchema::UPhysicsAssetGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPhysicsAssetGraphSchema::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
	if (MenuBuilder)
	{
		const UPhysicsAssetGraph* PhysicsAssetGraph = CastChecked<const UPhysicsAssetGraph>(CurrentGraph);
		TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetGraph->GetPhysicsAssetEditor()->GetSharedData();

		if (const UPhysicsAssetGraphNode_Constraint* ConstraintNode = Cast<const UPhysicsAssetGraphNode_Constraint>(InGraphNode))
		{
			PhysicsAssetGraph->GetPhysicsAssetEditor()->BuildMenuWidgetConstraint(*MenuBuilder);
		}
		else if (const UPhysicsAssetGraphNode_Bone* BoneNode = Cast<const UPhysicsAssetGraphNode_Bone>(InGraphNode))
		{
			PhysicsAssetGraph->GetPhysicsAssetEditor()->BuildMenuWidgetBody(*MenuBuilder);
		}

		PhysicsAssetGraph->GetPhysicsAssetEditor()->BuildMenuWidgetSelection(*MenuBuilder);
	}
}

FLinearColor UPhysicsAssetGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FLinearColor::White;
}

void UPhysicsAssetGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	// Don't allow breaking any links
}

void UPhysicsAssetGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin)
{
	// Don't allow breaking any links
}

FPinConnectionResponse UPhysicsAssetGraphSchema::MovePinLinks(UEdGraphPin& MoveFromPin, UEdGraphPin& MoveToPin, bool bIsItermeadiateMove) const
{
	// Don't allow moving any links
	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT(""));
}

FPinConnectionResponse UPhysicsAssetGraphSchema::CopyPinLinks(UEdGraphPin& CopyFromPin, UEdGraphPin& CopyToPin, bool bIsItermeadiateCopy) const
{
	// Don't allow copying any links
	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT(""));
}

FConnectionDrawingPolicy* UPhysicsAssetGraphSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const
{
	return new FPhysicsAssetConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements);
}

UPhysicsAssetGraphNode_Bone* UPhysicsAssetGraphSchema::CreateGraphNodesForBone(UPhysicsAssetGraph* InGraph, USkeletalBodySetup* InBodySetup, int32 InBodyIndex, UPhysicsAsset* InPhysicsAsset) const
{
	const bool bSelectNewNode = false;
	FGraphNodeCreator<UPhysicsAssetGraphNode_Bone> GraphNodeCreator(*InGraph);
	UPhysicsAssetGraphNode_Bone* BoneNode = GraphNodeCreator.CreateNode(bSelectNewNode);
	GraphNodeCreator.Finalize();

	BoneNode->SetupBoneNode(InBodySetup, InBodyIndex, InPhysicsAsset);

	return BoneNode;
}

UPhysicsAssetGraphNode_Constraint* UPhysicsAssetGraphSchema::CreateGraphNodesForConstraint(UPhysicsAssetGraph* InGraph, UPhysicsConstraintTemplate* InConstraint, int32 InConstraintIndex, UPhysicsAsset* InPhysicsAsset) const
{
	const bool bSelectNewNode = false;
	FGraphNodeCreator<UPhysicsAssetGraphNode_Constraint> GraphNodeCreator(*InGraph);
	UPhysicsAssetGraphNode_Constraint* ConstraintNode = GraphNodeCreator.CreateNode(bSelectNewNode);
	GraphNodeCreator.Finalize();

	ConstraintNode->SetupConstraintNode(InConstraint, InConstraintIndex, InPhysicsAsset);

	return ConstraintNode;
}

void UPhysicsAssetGraphSchema::LayoutNodes(UPhysicsAssetGraph* InGraph, UPhysicsAsset* InPhysicsAsset) const
{
	const int32 NodeMarginX = 20;
	const int32 NodeMarginY = 5;

	const TArray<UPhysicsAssetGraphNode_Bone*>& RootNodes = InGraph->GetRootNodes();
	if (RootNodes.Num() > 0)
	{
		int32 CurrentColumnX = 0;
		
		// Lay out root nodes
		float MaxWidth = 0.0f;
		int32 CurrentYOffset = 0;
		int32 TotalRootY = 0;
		for (int32 RootNodeIndex = 0; RootNodeIndex < RootNodes.Num(); ++RootNodeIndex)
		{
			UPhysicsAssetGraphNode_Bone* RootNode = RootNodes[RootNodeIndex];
			TotalRootY += (int32)RootNode->GetDimensions().Y + NodeMarginY;
		}

		for (int32 RootNodeIndex = 0; RootNodeIndex < RootNodes.Num(); ++RootNodeIndex)
		{
			UPhysicsAssetGraphNode_Bone* RootNode = RootNodes[RootNodeIndex];
			RootNode->NodePosX = CurrentColumnX;
			RootNode->NodePosY = CurrentYOffset - (TotalRootY / 2);

			CurrentYOffset += (int32)RootNode->GetDimensions().Y + NodeMarginY;
			MaxWidth = FMath::Max(MaxWidth, RootNode->GetDimensions().X);
		}

		CurrentColumnX += (MaxWidth + NodeMarginX);

		// Lay out constraints
		MaxWidth = 0.0f;
		CurrentYOffset = 0;
		int32 TotalConstraintY = 0;
		TArray<UPhysicsAssetGraphNode_Constraint*> ConstraintNodes;
		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (UPhysicsAssetGraphNode_Constraint* ConstraintNode = Cast<UPhysicsAssetGraphNode_Constraint>(Node))
			{
				ConstraintNodes.Add(ConstraintNode);
				TotalConstraintY += (int32)ConstraintNode->GetDimensions().Y + NodeMarginY;
			}
		}
		
		float ConstraintCount = ConstraintNodes.Num();
		for (int32 ConstraintIndex = 0; ConstraintIndex < ConstraintNodes.Num(); ++ConstraintIndex)
		{
			UPhysicsAssetGraphNode_Constraint* ConstraintNode  = ConstraintNodes[ConstraintIndex];
			ConstraintNode->NodePosX = CurrentColumnX;
			ConstraintNode->NodePosY = CurrentYOffset - (TotalConstraintY / 2);

			CurrentYOffset += (int32)ConstraintNode->GetDimensions().Y + NodeMarginY;
			MaxWidth = FMath::Max(MaxWidth, ConstraintNode->GetDimensions().X);
		}

		CurrentColumnX += (MaxWidth + NodeMarginX);

		// now layout linked nodes
		MaxWidth = 0.0f;
		CurrentYOffset = 0;
		int32 TotalLinkedNodeY = 0;
		TArray<UPhysicsAssetGraphNode_Bone*> LinkedNodes;
		for (int32 ConstraintIndex = 0; ConstraintIndex < ConstraintNodes.Num(); ++ConstraintIndex)
		{
			UPhysicsAssetGraphNode_Constraint* ConstraintNode = ConstraintNodes[ConstraintIndex];
			if (ConstraintNode->GetOutputPin().LinkedTo.Num() > 0)
			{
				for(UEdGraphPin* LinkedPin : ConstraintNode->GetOutputPin().LinkedTo)
				{
					UPhysicsAssetGraphNode_Bone* BoneNode = CastChecked<UPhysicsAssetGraphNode_Bone>(LinkedPin->GetOwningNode());
					LinkedNodes.AddUnique(BoneNode);
					TotalLinkedNodeY += (int32)BoneNode->GetDimensions().Y + NodeMarginY;
				}
			}
		}

		for (int32 LinkedNodeIndex = 0; LinkedNodeIndex < LinkedNodes.Num(); ++LinkedNodeIndex)
		{
			UPhysicsAssetGraphNode_Bone* LinkedNode = LinkedNodes[LinkedNodeIndex];
			LinkedNode->NodePosX = CurrentColumnX;
			LinkedNode->NodePosY = CurrentYOffset - (TotalLinkedNodeY / 2);

			CurrentYOffset += (int32)LinkedNode->GetDimensions().Y + NodeMarginY;
			MaxWidth = FMath::Max(MaxWidth, LinkedNode->GetDimensions().X);
		}
	}
}