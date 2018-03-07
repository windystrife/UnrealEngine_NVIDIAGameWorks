// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeGraphNode_CompositeDecorator.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BehaviorTreeDecoratorGraph.h"
#include "BehaviorTreeDecoratorGraphNode_Decorator.h"
#include "EdGraphSchema_BehaviorTreeDecorator.h"
#include "BehaviorTree/BehaviorTree.h"

#define LOCTEXT_NAMESPACE "BehaviorTreeEditor"

UBehaviorTreeGraphNode_CompositeDecorator::UBehaviorTreeGraphNode_CompositeDecorator(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bShowOperations = true;
	bCanAbortFlow = false;
	bHasBrokenInstances = false;

	FirstExecutionIndex = INDEX_NONE;
	LastExecutionIndex = INDEX_NONE;
}

void UBehaviorTreeGraphNode_CompositeDecorator::ResetExecutionRange()
{
	FirstExecutionIndex = INDEX_NONE;
	LastExecutionIndex = INDEX_NONE;
}

void UBehaviorTreeGraphNode_CompositeDecorator::AllocateDefaultPins()
{
	// No Pins for decorators
}

FString UBehaviorTreeGraphNode_CompositeDecorator::GetNodeTypeDescription() const
{
	return LOCTEXT("Composite","Composite").ToString();
}

FText UBehaviorTreeGraphNode_CompositeDecorator::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromString(CompositeName.Len() ? CompositeName : GetNodeTypeDescription());
}

FText UBehaviorTreeGraphNode_CompositeDecorator::GetDescription() const
{
	return FText::FromString(CachedDescription);
}

void UBehaviorTreeGraphNode_CompositeDecorator::PostPlacedNewNode()
{
	if (BoundGraph == nullptr)
	{
		CreateBoundGraph();
	}

	Super::PostPlacedNewNode();
}

void UBehaviorTreeGraphNode_CompositeDecorator::PostLoad()
{
	Super::PostLoad();

	if (BoundGraph == nullptr)
	{
		CreateBoundGraph();
	}
}

void UBehaviorTreeGraphNode_CompositeDecorator::PrepareForCopying()
{
	Super::PrepareForCopying();
	
	if (BoundGraph)
	{
		for (int32 i = 0; i < BoundGraph->Nodes.Num(); i++)
		{
			UEdGraphNode* Node = BoundGraph->Nodes[i];
			Node->PrepareForCopying();
		}
	}
}

void UBehaviorTreeGraphNode_CompositeDecorator::PostCopyNode()
{
	Super::PostCopyNode();

	if (BoundGraph)
	{
		for (int32 i = 0; i < BoundGraph->Nodes.Num(); i++)
		{
			UBehaviorTreeDecoratorGraphNode_Decorator* Node = Cast<UBehaviorTreeDecoratorGraphNode_Decorator>(BoundGraph->Nodes[i]);
			if (Node)
			{
				Node->PostCopyNode();
			}
		}
	}
}

bool UBehaviorTreeGraphNode_CompositeDecorator::RefreshNodeClass()
{
	bool bUpdated = false;

	if (BoundGraph)
	{
		for (int32 i = 0; i < BoundGraph->Nodes.Num(); i++)
		{
			UBehaviorTreeDecoratorGraphNode_Decorator* Node = Cast<UBehaviorTreeDecoratorGraphNode_Decorator>(BoundGraph->Nodes[i]);
			if (Node)
			{
				const bool bNodeUpdated = Node->RefreshNodeClass();
				bUpdated = bUpdated || bNodeUpdated;
			}
		}
	}
	
	return bUpdated;
}

void UBehaviorTreeGraphNode_CompositeDecorator::UpdateNodeClassData()
{
	if (BoundGraph)
	{
		for (int32 i = 0; i < BoundGraph->Nodes.Num(); i++)
		{
			UBehaviorTreeDecoratorGraphNode_Decorator* Node = Cast<UBehaviorTreeDecoratorGraphNode_Decorator>(BoundGraph->Nodes[i]);
			if (Node)
			{
				Node->UpdateNodeClassData();
			}
		}
	}
}

void UBehaviorTreeGraphNode_CompositeDecorator::UpdateBrokenInstances()
{
	bHasBrokenInstances = false;
	if (BoundGraph)
	{
		for (int32 i = 0; i < BoundGraph->Nodes.Num(); i++)
		{
			UBehaviorTreeDecoratorGraphNode_Decorator* Node = Cast<UBehaviorTreeDecoratorGraphNode_Decorator>(BoundGraph->Nodes[i]);
			if (Node && Node->NodeInstance == nullptr)
			{
				bHasBrokenInstances = true;
				break;
			}
		}
	}
}

bool UBehaviorTreeGraphNode_CompositeDecorator::HasErrors() const
{
	return bHasObserverError || bHasBrokenInstances;
}

void UBehaviorTreeGraphNode_CompositeDecorator::CreateBoundGraph()
{
	// Create a new animation graph
	check(BoundGraph == NULL);

	// don't use white space in name here, it prevents links from being copied correctly
	BoundGraph = FBlueprintEditorUtils::CreateNewGraph(this, TEXT("CompositeDecorator"), UBehaviorTreeDecoratorGraph::StaticClass(), UEdGraphSchema_BehaviorTreeDecorator::StaticClass());
	check(BoundGraph);

	// Initialize the anim graph
	const UEdGraphSchema* Schema = BoundGraph->GetSchema();
	Schema->CreateDefaultNodesForGraph(*BoundGraph);

	// Add the new graph as a child of our parent graph
	GetGraph()->SubGraphs.Add(BoundGraph);
}

bool UBehaviorTreeGraphNode_CompositeDecorator::IsSubNode() const
{
	return true;
}

void UBehaviorTreeGraphNode_CompositeDecorator::CollectDecoratorData(TArray<UBTDecorator*>& NodeInstances, TArray<FBTDecoratorLogic>& Operations) const
{
	const UBehaviorTreeDecoratorGraph* MyGraph = Cast<const UBehaviorTreeDecoratorGraph>(BoundGraph);
	if (MyGraph)
	{
		MyGraph->CollectDecoratorData(NodeInstances, Operations);
	}
}

void UBehaviorTreeGraphNode_CompositeDecorator::SetDecoratorData(class UBTCompositeNode* InParentNode, uint8 InChildIndex)
{
	ParentNodeInstance = InParentNode;
	ChildIndex = InChildIndex;
}

void UBehaviorTreeGraphNode_CompositeDecorator::InitializeDecorator(class UBTDecorator* InnerDecorator)
{
	InnerDecorator->InitializeNode(ParentNodeInstance, 0, 0, 0);
	InnerDecorator->InitializeParentLink(ChildIndex);
}

void UBehaviorTreeGraphNode_CompositeDecorator::OnBlackboardUpdate()
{
	UBehaviorTreeDecoratorGraph* MyGraph = Cast<UBehaviorTreeDecoratorGraph>(BoundGraph);
	UBehaviorTree* BTAsset = Cast<UBehaviorTree>(GetOuter()->GetOuter());
	if (MyGraph && BTAsset)
	{
		for (int32 i = 0; i < MyGraph->Nodes.Num(); i++)
		{
			UBehaviorTreeDecoratorGraphNode_Decorator* MyNode = Cast<UBehaviorTreeDecoratorGraphNode_Decorator>(MyGraph->Nodes[i]);
			UBTNode* MyNodeInstance = MyNode ? Cast<UBTNode>(MyNode->NodeInstance) : NULL;
			if (MyNodeInstance)
			{
				MyNodeInstance->InitializeFromAsset(*BTAsset);
			}
		}
	}
}

void UBehaviorTreeGraphNode_CompositeDecorator::OnInnerGraphChanged()
{
	BuildDescription();

	bCanAbortFlow = false;

	UBehaviorTreeDecoratorGraph* MyGraph = Cast<UBehaviorTreeDecoratorGraph>(BoundGraph);
	if (MyGraph)
	{
		for (int32 i = 0; i < MyGraph->Nodes.Num(); i++)
		{
			UBehaviorTreeDecoratorGraphNode_Decorator* MyNode = Cast<UBehaviorTreeDecoratorGraphNode_Decorator>(MyGraph->Nodes[i]);
			UBTDecorator* MyNodeInstance = MyNode ? Cast<UBTDecorator>(MyNode->NodeInstance) : NULL;
			if (MyNodeInstance && MyNodeInstance->GetFlowAbortMode() != EBTFlowAbortMode::None)
			{
				bCanAbortFlow = true;
				break;
			}
		}
	}
}

int32 UBehaviorTreeGraphNode_CompositeDecorator::SpawnMissingNodes(const TArray<class UBTDecorator*>& NodeInstances, const TArray<struct FBTDecoratorLogic>& Operations, int32 StartIndex)
{
	int32 FirstIdxOutside = StartIndex + 1;
	
	UBehaviorTreeDecoratorGraph* MyGraph = Cast<UBehaviorTreeDecoratorGraph>(BoundGraph);
	if (MyGraph)
	{
		FirstIdxOutside = MyGraph->SpawnMissingNodes(NodeInstances, Operations, StartIndex);
	}

	return FirstIdxOutside;
}

void UBehaviorTreeGraphNode_CompositeDecorator::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property &&
		PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UBehaviorTreeGraphNode_CompositeDecorator, bShowOperations))
	{
		BuildDescription();
	}
}

struct FLogicDesc
{
	FString OperationDesc;
	int32 NumLeft;
};

void UpdateLogicOpStack(TArray<FLogicDesc>& OpStack, FString& Description, FString& Indent)
{
	if (OpStack.Num())
	{
		const int32 LastIdx = OpStack.Num() - 1;
		OpStack[LastIdx].NumLeft = OpStack[LastIdx].NumLeft - 1;

		if (OpStack[LastIdx].NumLeft <= 0)
		{
			OpStack.RemoveAt(LastIdx);
			Indent = Indent.LeftChop(2);

			UpdateLogicOpStack(OpStack, Description, Indent);
		}
		else
		{
			Description += Indent;
			Description += OpStack[LastIdx].OperationDesc;
		}
	}
}

void UBehaviorTreeGraphNode_CompositeDecorator::BuildDescription()
{
	FString BaseDesc("Composite Decorator");
	if (!bShowOperations)
	{
		CachedDescription = BaseDesc;
		return;
	}

	TArray<UBTDecorator*> NodeInstances;
	TArray<FBTDecoratorLogic> Operations;
	CollectDecoratorData(NodeInstances, Operations);

	TArray<FLogicDesc> OpStack;
	FString Description = BaseDesc + TEXT(":");
	FString Indent("\n");
	bool bPendingNotOp = false;

	for (int32 i = 0; i < Operations.Num(); i++)
	{
		const FBTDecoratorLogic& TestOp = Operations[i];
		if (TestOp.Operation == EBTDecoratorLogic::And ||
			TestOp.Operation == EBTDecoratorLogic::Or)
		{
			Indent += TEXT("- ");

			FLogicDesc NewOpDesc;
			NewOpDesc.NumLeft = TestOp.Number;
			NewOpDesc.OperationDesc = (TestOp.Operation == EBTDecoratorLogic::And) ? TEXT("AND") : TEXT("OR");
			
			OpStack.Add(NewOpDesc);
		}
		else if (TestOp.Operation == EBTDecoratorLogic::Not)
		{
			// special case: NOT before TEST
			if (Operations.IsValidIndex(i + 1) && Operations[i + 1].Operation == EBTDecoratorLogic::Test)
			{
				bPendingNotOp = true;
			}
			else
			{
				Indent += TEXT("- ");
				Description += Indent;
				Description += TEXT("NOT:");

				FLogicDesc NewOpDesc;
				NewOpDesc.NumLeft = 0;

				OpStack.Add(NewOpDesc);
			}
		}
		else if (TestOp.Operation == EBTDecoratorLogic::Test)
		{
			Description += Indent;
			if (bPendingNotOp)
			{
				Description += TEXT("NOT: ");
				bPendingNotOp = false;
			}

			Description += NodeInstances[TestOp.Number]->GetStaticDescription();
			UpdateLogicOpStack(OpStack, Description, Indent);
		}
	}

	CachedDescription = Description;
}

#undef LOCTEXT_NAMESPACE
