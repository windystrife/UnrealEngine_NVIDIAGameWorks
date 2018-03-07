// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimationGraphSchema.cpp
=============================================================================*/

#include "AnimationGraphSchema.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimBlueprint.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "K2Node.h"
#include "EdGraphSchema_K2_Actions.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Animation/AnimSequence.h"
#include "AnimStateNode.h"
#include "Animation/BlendSpace.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimNodeBase.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_RotationOffsetBlendSpace.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "Animation/PoseAsset.h"
#include "AnimGraphNode_PoseBlendNode.h"
#include "AnimGraphNode_PoseByName.h"
#include "AnimGraphCommands.h"
#include "K2Node_Knot.h"
#include "ScopedTransaction.h"
#include "Animation/AnimMontage.h"

#define LOCTEXT_NAMESPACE "AnimationGraphSchema"

/////////////////////////////////////////////////////
// UAnimationGraphSchema

UAnimationGraphSchema::UAnimationGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PN_SequenceName = TEXT("Sequence");

	NAME_NeverAsPin = TEXT("NeverAsPin");
	NAME_PinHiddenByDefault = TEXT("PinHiddenByDefault");
	NAME_PinShownByDefault = TEXT("PinShownByDefault");
	NAME_AlwaysAsPin = TEXT("AlwaysAsPin");
	NAME_OnEvaluate = TEXT("OnEvaluate");
	NAME_CustomizeProperty = TEXT("CustomizeProperty");
	DefaultEvaluationHandlerName = TEXT("EvaluateGraphExposedInputs");
}

FLinearColor UAnimationGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	const bool bAdditive = PinType.PinSubCategory == TEXT("Additive");
	if (UAnimationGraphSchema::IsLocalSpacePosePin(PinType))
	{
		if (bAdditive) 
		{
			return FLinearColor(0.12, 0.60, 0.10);
		}
		else
		{
			return FLinearColor::White;
		}
	}
	else if (UAnimationGraphSchema::IsComponentSpacePosePin(PinType))
	{
		//@TODO: Pick better colors
		if (bAdditive) 
		{
			return FLinearColor(0.12, 0.60, 0.60);
		}
		else
		{
			return FLinearColor(0.20f, 0.50f, 1.00f);
		}
	}

	return Super::GetPinTypeColor(PinType);
}

EGraphType UAnimationGraphSchema::GetGraphType(const UEdGraph* TestEdGraph) const
{
	return GT_Animation;
}

void UAnimationGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	// Create the result node
	FGraphNodeCreator<UAnimGraphNode_Root> NodeCreator(Graph);
	UAnimGraphNode_Root* ResultSinkNode = NodeCreator.CreateNode();
	NodeCreator.Finalize();
	SetNodeMetaData(ResultSinkNode, FNodeMetadata::DefaultGraphNode);
}

void UAnimationGraphSchema::HandleGraphBeingDeleted(UEdGraph& GraphBeingRemoved) const
{
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(&GraphBeingRemoved))
	{
		// Look for state nodes that reference this graph
		TArray<UAnimStateNodeBase*> StateNodes;
		FBlueprintEditorUtils::GetAllNodesOfClassEx<UAnimStateNode>(Blueprint, StateNodes);

		TSet<UAnimStateNodeBase*> NodesToDelete;
		for (int32 i = 0; i < StateNodes.Num(); ++i)
		{
			UAnimStateNodeBase* StateNode = StateNodes[i];
			if (StateNode->GetBoundGraph() == &GraphBeingRemoved)
			{
				NodesToDelete.Add(StateNode);
			}
		}

		// Delete the node that owns us
		ensure(NodesToDelete.Num() <= 1);
		for (TSet<UAnimStateNodeBase*>::TIterator It(NodesToDelete); It; ++It)
		{
			UAnimStateNodeBase* NodeToDelete = *It;

			FBlueprintEditorUtils::RemoveNode(Blueprint, NodeToDelete, true);

			// Prevent re-entrancy here
			NodeToDelete->ClearBoundGraph();
		}
	}
}

bool UAnimationGraphSchema::IsPosePin(const FEdGraphPinType& PinType)
{
	return IsLocalSpacePosePin(PinType) || IsComponentSpacePosePin(PinType);
}

bool UAnimationGraphSchema::IsLocalSpacePosePin(const FEdGraphPinType& PinType)
{
	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();

	UScriptStruct* PoseLinkStruct = FPoseLink::StaticStruct();
	return (PinType.PinCategory == Schema->PC_Struct) && (PinType.PinSubCategoryObject == PoseLinkStruct);
}

bool UAnimationGraphSchema::IsComponentSpacePosePin(const FEdGraphPinType& PinType)
{
	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();

	UScriptStruct* ComponentSpacePoseLinkStruct = FComponentSpacePoseLink::StaticStruct();
	return (PinType.PinCategory == Schema->PC_Struct) && (PinType.PinSubCategoryObject == ComponentSpacePoseLinkStruct);
}

bool UAnimationGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	UEdGraphPin* OutputPin = nullptr;
	UEdGraphPin* InputPin = nullptr;

	if(A->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		OutputPin = A;
		InputPin = B;
	}
	else
	{
		OutputPin = B;
		InputPin = A;
	}
	check(OutputPin && InputPin);

	UEdGraphNode* OutputNode = OutputPin->GetOwningNode();

	if(UK2Node_Knot* RerouteNode = Cast<UK2Node_Knot>(OutputNode))
	{
		// Double check this is our "exec"-like line
		bool bOutputIsPose = IsPosePin(OutputPin->PinType);
		bool bInputIsPose = IsPosePin(InputPin->PinType);
		bool bHavePosePin = bOutputIsPose || bInputIsPose;
		bool bHaveWildPin = InputPin->PinType.PinCategory == PC_Wildcard || OutputPin->PinType.PinCategory == PC_Wildcard;

		if((bOutputIsPose && bInputIsPose) || (bHavePosePin && bHaveWildPin))
		{
			// Ok this is a valid exec-like line, we need to kill any connections already on the output pin
			OutputPin->BreakAllPinLinks();
		}
	}

	return Super::TryCreateConnection(A, B);
}

const FPinConnectionResponse UAnimationGraphSchema::DetermineConnectionResponseOfCompatibleTypedPins(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const
{
	// Enforce a tree hierarchy; where poses can only have one output (parent) connection
	if (IsPosePin(OutputPin->PinType) && IsPosePin(InputPin->PinType))
	{
		if ((OutputPin->LinkedTo.Num() > 0) || (InputPin->LinkedTo.Num() > 0))
		{
			const ECanCreateConnectionResponse ReplyBreakOutputs = CONNECT_RESPONSE_BREAK_OTHERS_AB;
			return FPinConnectionResponse(ReplyBreakOutputs, TEXT("Replace existing connections"));
		}
	}

	// Fall back to standard K2 rules
	return Super::DetermineConnectionResponseOfCompatibleTypedPins(PinA, PinB, InputPin, OutputPin);
}

bool UAnimationGraphSchema::ArePinsCompatible(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UClass* CallingContext, bool bIgnoreArray) const
{
	// both are pose pin, but doesn't match type, then return false;
	if (IsPosePin(PinA->PinType) && IsPosePin(PinB->PinType) && IsLocalSpacePosePin(PinA->PinType) != IsLocalSpacePosePin(PinB->PinType))
	{
		return false;
	}

	// Disallow pose pins connecting to wildcards (apart from reroute nodes)
	if(IsPosePin(PinA->PinType) && PinB->PinType.PinCategory == PC_Wildcard)
	{
		return Cast<UK2Node_Knot>(PinB->GetOwningNode()) != nullptr;
	}
	else if(IsPosePin(PinB->PinType) && PinA->PinType.PinCategory == PC_Wildcard)
	{
		return Cast<UK2Node_Knot>(PinA->GetOwningNode()) != nullptr;
	}

	return Super::ArePinsCompatible(PinA, PinB, CallingContext, bIgnoreArray);
}

bool UAnimationGraphSchema::DoesSupportAnimNotifyActions() const
{
	// Don't offer notify items in anim graph
	return false;
}

bool UAnimationGraphSchema::SearchForAutocastFunction(const UEdGraphPin* OutputPin, const UEdGraphPin* InputPin, FName& TargetFunction, /*out*/ UClass*& FunctionOwner) const
{
	if (IsComponentSpacePosePin(OutputPin->PinType) && IsLocalSpacePosePin(InputPin->PinType))
	{
		// Insert a Component To LocalSpace conversion
		return true;
	}
	else if (IsLocalSpacePosePin(OutputPin->PinType) && IsComponentSpacePosePin(InputPin->PinType))
	{
		// Insert a Local To ComponentSpace conversion
		return true;
	}
	else
	{
		return Super::SearchForAutocastFunction(OutputPin, InputPin, TargetFunction, FunctionOwner);
	}
}

bool UAnimationGraphSchema::CreateAutomaticConversionNodeAndConnections(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	// Determine which pin is an input and which pin is an output
	UEdGraphPin* InputPin = NULL;
	UEdGraphPin* OutputPin = NULL;
	if (!CategorizePinsByDirection(PinA, PinB, /*out*/ InputPin, /*out*/ OutputPin))
	{
		return false;
	}

	// Look for animation specific conversion operations
	UK2Node* TemplateNode = NULL;
	if (IsComponentSpacePosePin(OutputPin->PinType) && IsLocalSpacePosePin(InputPin->PinType))
	{
		TemplateNode = NewObject<UAnimGraphNode_ComponentToLocalSpace>();
	}
	else if (IsLocalSpacePosePin(OutputPin->PinType) && IsComponentSpacePosePin(InputPin->PinType))
	{
		TemplateNode = NewObject<UAnimGraphNode_LocalToComponentSpace>();
	}

	// Spawn the conversion node if it's specific to animation
	if (TemplateNode != NULL)
	{
		UEdGraph* Graph = InputPin->GetOwningNode()->GetGraph();
		FVector2D AverageLocation = CalculateAveragePositionBetweenNodes(InputPin, OutputPin);

		UK2Node* ConversionNode = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node>(Graph, TemplateNode, AverageLocation);
		AutowireConversionNode(InputPin, OutputPin, ConversionNode);

		return true;
	}
	else
	{
		// Give the regular conversions a shot
		return Super::CreateAutomaticConversionNodeAndConnections(PinA, PinB);
	}
}

bool IsAimOffsetBlendSpace(UBlendSpaceBase* BlendSpace)
{
	return	BlendSpace->IsA(UAimOffsetBlendSpace::StaticClass()) ||
			BlendSpace->IsA(UAimOffsetBlendSpace1D::StaticClass());
}

void UAnimationGraphSchema::SpawnNodeFromAsset(UAnimationAsset* Asset, const FVector2D& GraphPosition, UEdGraph* Graph, UEdGraphPin* PinIfAvailable)
{
	check(Graph);
	check(Graph->GetSchema()->IsA(UAnimationGraphSchema::StaticClass()));
	check(Asset);

	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForGraph(Graph));

	const bool bSkelMatch = (AnimBlueprint != NULL) && (AnimBlueprint->TargetSkeleton == Asset->GetSkeleton());
	const bool bTypeMatch = (PinIfAvailable == NULL) || UAnimationGraphSchema::IsLocalSpacePosePin(PinIfAvailable->PinType);
	const bool bDirectionMatch = (PinIfAvailable == NULL) || (PinIfAvailable->Direction == EGPD_Input);

	if (bSkelMatch && bTypeMatch && bDirectionMatch)
	{
		FEdGraphSchemaAction_K2NewNode Action;

		UClass* NewNodeClass = GetNodeClassForAsset(Asset->GetClass());
		
		if (NewNodeClass)
		{
			check(NewNodeClass->IsChildOf(UAnimGraphNode_AssetPlayerBase::StaticClass()));

			UAnimGraphNode_AssetPlayerBase* NewNode = NewObject<UAnimGraphNode_AssetPlayerBase>(GetTransientPackage(), NewNodeClass);
			NewNode->SetAnimationAsset(Asset);
			Action.NodeTemplate = NewNode;

			Action.PerformAction(Graph, PinIfAvailable, GraphPosition);
		}
	}
}


void UAnimationGraphSchema::UpdateNodeWithAsset(UK2Node* K2Node, UAnimationAsset* Asset)
{
	if (Asset != NULL)
	{
		if (UAnimGraphNode_AssetPlayerBase* AssetPlayerNode = Cast<UAnimGraphNode_AssetPlayerBase>(K2Node))
		{
			if (AssetPlayerNode->SupportsAssetClass(Asset->GetClass()) != EAnimAssetHandlerType::NotSupported)
			{
				const FScopedTransaction Transaction(LOCTEXT("UpdateNodeWithAsset", "Updating Node with Asset"));
				AssetPlayerNode->Modify();

				AssetPlayerNode->SetAnimationAsset(Asset);

				K2Node->GetSchema()->ForceVisualizationCacheClear();
				K2Node->ReconstructNode();
			}
		}
	}
}


void UAnimationGraphSchema::DroppedAssetsOnGraph( const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph ) const 
{
	UAnimationAsset* Asset = FAssetData::GetFirstAsset<UAnimationAsset>(Assets);
	if ((Asset != NULL) && (Graph != NULL))
	{
		SpawnNodeFromAsset(Asset, GraphPosition, Graph, NULL);
	}
}



void UAnimationGraphSchema::DroppedAssetsOnNode(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphNode* Node) const
{
	UAnimationAsset* Asset = FAssetData::GetFirstAsset<UAnimationAsset>(Assets);
	UK2Node* K2Node = Cast<UK2Node>(Node);
	if ((Asset != NULL) && (K2Node!= NULL))
	{
		UpdateNodeWithAsset(K2Node, Asset);
	}
}

void UAnimationGraphSchema::DroppedAssetsOnPin(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphPin* Pin) const
{
	UAnimationAsset* Asset = FAssetData::GetFirstAsset<UAnimationAsset>(Assets);
	if ((Asset != NULL) && (Pin != NULL))
	{
		SpawnNodeFromAsset(Asset, GraphPosition, Pin->GetOwningNode()->GetGraph(), Pin);
	}
}

void UAnimationGraphSchema::GetAssetsNodeHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphNode* HoverNode, FString& OutTooltipText, bool& OutOkIcon) const 
{ 
	UAnimationAsset* Asset = FAssetData::GetFirstAsset<UAnimationAsset>(Assets);
	if ((Asset == NULL) || (HoverNode == NULL) || !HoverNode->IsA(UAnimGraphNode_Base::StaticClass()))
	{
		OutTooltipText = TEXT("");
		OutOkIcon = false;
		return;
	}

	bool bCanPlayAsset = SupportNodeClassForAsset(Asset->GetClass(), HoverNode->GetClass());

	// this one only should happen when there is an Anim Blueprint
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForNode(HoverNode));
	const bool bSkelMatch = (AnimBlueprint != NULL) && (AnimBlueprint->TargetSkeleton == Asset->GetSkeleton());

	if (!bSkelMatch)
	{
		OutOkIcon = false;
		OutTooltipText = LOCTEXT("SkeletonsNotCompatible", "Skeletons are not compatible").ToString();
	}
	else if (bCanPlayAsset)
	{
		OutOkIcon = true;
		OutTooltipText = FText::Format(LOCTEXT("AssetNodeHoverMessage_Success", "Change node to play '{0}'"), FText::FromString(Asset->GetName())).ToString();
	}
	else
	{
		OutOkIcon = false;
		OutTooltipText = FText::Format(LOCTEXT("AssetNodeHoverMessage_Fail", "Cannot play '{0}' on this node type"),  FText::FromString(Asset->GetName())).ToString();
	}
}

void UAnimationGraphSchema::GetAssetsPinHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphPin* HoverPin, FString& OutTooltipText, bool& OutOkIcon) const 
{ 
	UAnimationAsset* Asset = FAssetData::GetFirstAsset<UAnimationAsset>(Assets);
	if ((Asset == NULL) || (HoverPin == NULL))
	{
		OutTooltipText = TEXT("");
		OutOkIcon = false;
		return;
	}

	// this one only should happen when there is an Anim Blueprint
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForNode(HoverPin->GetOwningNode()));

	const bool bSkelMatch = (AnimBlueprint != NULL) && (AnimBlueprint->TargetSkeleton == Asset->GetSkeleton());
	const bool bTypeMatch = UAnimationGraphSchema::IsLocalSpacePosePin(HoverPin->PinType);
	const bool bDirectionMatch = HoverPin->Direction == EGPD_Input;

	if (bSkelMatch && bTypeMatch && bDirectionMatch)
	{
		OutOkIcon = true;
		OutTooltipText = FText::Format(LOCTEXT("AssetPinHoverMessage_Success", "Play {0} and feed to {1}"), FText::FromString(Asset->GetName()), FText::FromString(HoverPin->PinName)).ToString();
	}
	else
	{
		OutOkIcon = false;
		OutTooltipText = LOCTEXT("AssetPinHoverMessage_Fail", "Type or direction mismatch; must be wired to a pose input").ToString();
	}
}

void UAnimationGraphSchema::GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* HoverGraph, FString& OutTooltipText, bool& OutOkIcon) const
{
	UAnimationAsset* Asset = FAssetData::GetFirstAsset<UAnimationAsset>(Assets);
	if (Asset)
	{
		UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForGraph(HoverGraph));
		const bool bSkelMatch = (AnimBlueprint != NULL) && (AnimBlueprint->TargetSkeleton == Asset->GetSkeleton());
		if (!bSkelMatch)
		{
			OutOkIcon = false;
			OutTooltipText = LOCTEXT("SkeletonsNotCompatible", "Skeletons are not compatible").ToString();
		}
		else if(UAnimMontage* Montage = FAssetData::GetFirstAsset<UAnimMontage>(Assets))
		{
			OutOkIcon = false;
			OutTooltipText = LOCTEXT("NoMontagesInAnimGraphs", "Montages cannot be used in animation graphs").ToString();
		}
		else
		{
			OutOkIcon = true;
			OutTooltipText = TEXT("");
		}
	}
}

void UAnimationGraphSchema::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
	Super::GetContextMenuActions(CurrentGraph, InGraphNode, InGraphPin, MenuBuilder, bIsDebugging);

	if (const UAnimGraphNode_Base* AnimGraphNode = Cast<const UAnimGraphNode_Base>(InGraphNode))
	{
		MenuBuilder->BeginSection("AnimGraphSchemaNodeActions", LOCTEXT("AnimNodeActionsMenuHeader", "Anim Node Actions"));
		{
			// Node contextual actions
			MenuBuilder->AddMenuEntry(FAnimGraphCommands::Get().TogglePoseWatch);
		}
		MenuBuilder->EndSection();
	}
}

FText UAnimationGraphSchema::GetPinDisplayName(const UEdGraphPin* Pin) const 
{
	check(Pin != NULL);

	FText DisplayName = Super::GetPinDisplayName(Pin);

	if (UAnimGraphNode_Base* Node = Cast<UAnimGraphNode_Base>(Pin->GetOwningNode()))
	{
		FString ProcessedDisplayName = DisplayName.ToString();
		Node->PostProcessPinName(Pin, ProcessedDisplayName);
		DisplayName = FText::FromString(ProcessedDisplayName);
	}

	return DisplayName;
}

#undef LOCTEXT_NAMESPACE
