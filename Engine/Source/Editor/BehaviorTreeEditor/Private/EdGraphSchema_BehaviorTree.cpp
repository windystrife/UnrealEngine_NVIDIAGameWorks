// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EdGraphSchema_BehaviorTree.h"
#include "Layout/SlateRect.h"
#include "EdGraphNode_Comment.h"
#include "Modules/ModuleManager.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTreeEditorTypes.h"
#include "EdGraph/EdGraph.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_CompositeDecorator.h"
#include "BehaviorTreeGraphNode_Decorator.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeGraphNode_Service.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "BehaviorTreeEditorModule.h"
#include "IBehaviorTreeEditor.h"
#include "BehaviorTreeDebugger.h"
#include "GraphEditorActions.h"
#include "BehaviorTreeConnectionDrawingPolicy.h"
#include "Toolkits/ToolkitManager.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"
#include "BehaviorTree/Composites/BTComposite_SimpleParallel.h"
#include "BehaviorTreeGraphNode_SimpleParallel.h"
#include "BehaviorTreeGraphNode_SubtreeTask.h"

#define LOCTEXT_NAMESPACE "BehaviorTreeEditor"

int32 UEdGraphSchema_BehaviorTree::CurrentCacheRefreshID = 0;

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
UEdGraphNode* FBehaviorTreeSchemaAction_AutoArrange::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(ParentGraph);
	if (Graph)
	{
		Graph->AutoArrange();
	}

	return NULL;
}

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
UEdGraphNode* FBehaviorTreeSchemaAction_AddComment::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UEdGraphNode_Comment* const CommentTemplate = NewObject<UEdGraphNode_Comment>();

	FVector2D SpawnLocation = Location;

	TSharedPtr<IBehaviorTreeEditor> BTEditor;
	if (UBehaviorTree* const BTAsset = Cast<UBehaviorTree>(ParentGraph->GetOuter()))
	{
		TSharedPtr<IToolkit> BTAssetEditor = FToolkitManager::Get().FindEditorForAsset(BTAsset);
		if (BTAssetEditor.IsValid())
		{
			BTEditor = StaticCastSharedPtr<IBehaviorTreeEditor>(BTAssetEditor);
		}
	}

	FSlateRect Bounds;
	if (BTEditor.IsValid() && BTEditor->GetBoundsForSelectedNodes(Bounds, 50.0f))
	{
		CommentTemplate->SetBounds(Bounds);
		SpawnLocation.X = CommentTemplate->NodePosX;
		SpawnLocation.Y = CommentTemplate->NodePosY;
	}

	UEdGraphNode* const NewNode = FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Comment>(ParentGraph, CommentTemplate, SpawnLocation, bSelectNewNode);

	return NewNode;
}

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//

UEdGraphSchema_BehaviorTree::UEdGraphSchema_BehaviorTree(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UEdGraphSchema_BehaviorTree::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	FGraphNodeCreator<UBehaviorTreeGraphNode_Root> NodeCreator(Graph);
	UBehaviorTreeGraphNode_Root* MyNode = NodeCreator.CreateNode();
	NodeCreator.Finalize();
	SetNodeMetaData(MyNode, FNodeMetadata::DefaultGraphNode);
}

void UEdGraphSchema_BehaviorTree::GetGraphNodeContextActions(FGraphContextMenuBuilder& ContextMenuBuilder, int32 SubNodeFlags) const
{
	Super::GetGraphNodeContextActions(ContextMenuBuilder, SubNodeFlags);

	if (SubNodeFlags == ESubNode::Decorator)
	{
		const FText& Category = UBehaviorTreeGraphNode_CompositeDecorator::StaticClass()->GetMetaDataText(TEXT("Category"), TEXT("UObjectCategory"), UBehaviorTreeGraphNode_CompositeDecorator::StaticClass()->GetFullGroupName(false));
		UEdGraph* Graph = (UEdGraph*)ContextMenuBuilder.CurrentGraph;
		UBehaviorTreeGraphNode_CompositeDecorator* OpNode = NewObject<UBehaviorTreeGraphNode_CompositeDecorator>(Graph);
		TSharedPtr<FAISchemaAction_NewSubNode> AddOpAction = UAIGraphSchema::AddNewSubNodeAction(ContextMenuBuilder, Category, FText::FromString(OpNode->GetNodeTypeDescription()), FText::GetEmpty());
		AddOpAction->ParentNode = Cast<UBehaviorTreeGraphNode>(ContextMenuBuilder.SelectedObjects[0]);
		AddOpAction->NodeTemplate = OpNode;
	}
}

void UEdGraphSchema_BehaviorTree::GetSubNodeClasses(int32 SubNodeFlags, TArray<FGraphNodeClassData>& ClassData, UClass*& GraphNodeClass) const
{
	FBehaviorTreeEditorModule& EditorModule = FModuleManager::GetModuleChecked<FBehaviorTreeEditorModule>(TEXT("BehaviorTreeEditor"));
	FGraphNodeClassHelper* ClassCache = EditorModule.GetClassCache().Get();

	if (SubNodeFlags == ESubNode::Decorator)
	{
		ClassCache->GatherClasses(UBTDecorator::StaticClass(), ClassData);
		GraphNodeClass = UBehaviorTreeGraphNode_Decorator::StaticClass();
	}
	else
	{
		ClassCache->GatherClasses(UBTService::StaticClass(), ClassData);
		GraphNodeClass = UBehaviorTreeGraphNode_Service::StaticClass();
	}
}

void UEdGraphSchema_BehaviorTree::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	const FString PinCategory = ContextMenuBuilder.FromPin ?
		ContextMenuBuilder.FromPin->PinType.PinCategory : 
		UBehaviorTreeEditorTypes::PinCategory_MultipleNodes;

	const bool bNoParent = (ContextMenuBuilder.FromPin == NULL);
	const bool bOnlyTasks = (PinCategory == UBehaviorTreeEditorTypes::PinCategory_SingleTask);
	const bool bOnlyComposites = (PinCategory == UBehaviorTreeEditorTypes::PinCategory_SingleComposite);
	const bool bAllowComposites = bNoParent || !bOnlyTasks || bOnlyComposites;
	const bool bAllowTasks = bNoParent || !bOnlyComposites || bOnlyTasks;

	FBehaviorTreeEditorModule& EditorModule = FModuleManager::GetModuleChecked<FBehaviorTreeEditorModule>(TEXT("BehaviorTreeEditor"));
	FGraphNodeClassHelper* ClassCache = EditorModule.GetClassCache().Get();

	if (bAllowComposites)
	{
		FCategorizedGraphActionListBuilder CompositesBuilder(TEXT("Composites"));

		TArray<FGraphNodeClassData> NodeClasses;
		ClassCache->GatherClasses(UBTCompositeNode::StaticClass(), NodeClasses);

		const FString ParallelClassName = UBTComposite_SimpleParallel::StaticClass()->GetName();
		for (const auto& NodeClass : NodeClasses)
		{
			const FText NodeTypeName = FText::FromString(FName::NameToDisplayString(NodeClass.ToString(), false));

			TSharedPtr<FAISchemaAction_NewNode> AddOpAction = UAIGraphSchema::AddNewNodeAction(CompositesBuilder, NodeClass.GetCategory(), NodeTypeName, FText::GetEmpty());

			UClass* GraphNodeClass = UBehaviorTreeGraphNode_Composite::StaticClass();
			if (NodeClass.GetClassName() == ParallelClassName)
			{
				GraphNodeClass = UBehaviorTreeGraphNode_SimpleParallel::StaticClass();
			}

			UBehaviorTreeGraphNode* OpNode = NewObject<UBehaviorTreeGraphNode>(ContextMenuBuilder.OwnerOfTemporaries, GraphNodeClass);
			OpNode->ClassData = NodeClass;
			AddOpAction->NodeTemplate = OpNode;
		}

		ContextMenuBuilder.Append(CompositesBuilder);
	}

	if (bAllowTasks)
	{
		FCategorizedGraphActionListBuilder TasksBuilder(TEXT("Tasks"));

		TArray<FGraphNodeClassData> NodeClasses;
		ClassCache->GatherClasses(UBTTaskNode::StaticClass(), NodeClasses);

		for (const auto& NodeClass : NodeClasses)
		{
			const FText NodeTypeName = FText::FromString(FName::NameToDisplayString(NodeClass.ToString(), false));

			TSharedPtr<FAISchemaAction_NewNode> AddOpAction = UAIGraphSchema::AddNewNodeAction(TasksBuilder, NodeClass.GetCategory(), NodeTypeName, FText::GetEmpty());
			
			UClass* GraphNodeClass = UBehaviorTreeGraphNode_Task::StaticClass();
			if (NodeClass.GetClassName() == UBTTask_RunBehavior::StaticClass()->GetName())
			{
				GraphNodeClass = UBehaviorTreeGraphNode_SubtreeTask::StaticClass();
			}

			UBehaviorTreeGraphNode* OpNode = NewObject<UBehaviorTreeGraphNode>(ContextMenuBuilder.OwnerOfTemporaries, GraphNodeClass);
			OpNode->ClassData = NodeClass;
			AddOpAction->NodeTemplate = OpNode;
		}

		ContextMenuBuilder.Append(TasksBuilder);
	}
	
	if (bNoParent)
	{
		TSharedPtr<FBehaviorTreeSchemaAction_AutoArrange> Action = TSharedPtr<FBehaviorTreeSchemaAction_AutoArrange>(
			new FBehaviorTreeSchemaAction_AutoArrange(FText::GetEmpty(), LOCTEXT("AutoArrange", "Auto Arrange"), FText::GetEmpty(), 0)
			);

		ContextMenuBuilder.AddAction(Action);
	}
}

void UEdGraphSchema_BehaviorTree::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
	if (InGraphNode && !InGraphPin)
	{
		const UBehaviorTreeGraphNode* BTGraphNode = Cast<const UBehaviorTreeGraphNode>(InGraphNode);
		if (BTGraphNode && BTGraphNode->CanPlaceBreakpoints())
		{
			MenuBuilder->BeginSection("EdGraphSchemaBreakpoints", LOCTEXT("BreakpointsHeader", "Breakpoints"));
			{
				MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().ToggleBreakpoint);
				MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().AddBreakpoint);
				MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().RemoveBreakpoint);
				MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().EnableBreakpoint);
				MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().DisableBreakpoint);
			}
			MenuBuilder->EndSection();
		}
	}

	Super::GetContextMenuActions(CurrentGraph, InGraphNode, InGraphPin, MenuBuilder, bIsDebugging);
}

const FPinConnectionResponse UEdGraphSchema_BehaviorTree::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	// Make sure the pins are not on the same node
	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorSameNode","Both are on the same node"));
	}

	const bool bPinAIsSingleComposite = (PinA->PinType.PinCategory == UBehaviorTreeEditorTypes::PinCategory_SingleComposite);
	const bool bPinAIsSingleTask = (PinA->PinType.PinCategory == UBehaviorTreeEditorTypes::PinCategory_SingleTask);
	const bool bPinAIsSingleNode = (PinA->PinType.PinCategory == UBehaviorTreeEditorTypes::PinCategory_SingleNode);

	const bool bPinBIsSingleComposite = (PinB->PinType.PinCategory == UBehaviorTreeEditorTypes::PinCategory_SingleComposite);
	const bool bPinBIsSingleTask = (PinB->PinType.PinCategory == UBehaviorTreeEditorTypes::PinCategory_SingleTask);
	const bool bPinBIsSingleNode = (PinB->PinType.PinCategory == UBehaviorTreeEditorTypes::PinCategory_SingleNode);

	const bool bPinAIsTask = PinA->GetOwningNode()->IsA(UBehaviorTreeGraphNode_Task::StaticClass());
	const bool bPinAIsComposite = PinA->GetOwningNode()->IsA(UBehaviorTreeGraphNode_Composite::StaticClass());
	
	const bool bPinBIsTask = PinB->GetOwningNode()->IsA(UBehaviorTreeGraphNode_Task::StaticClass());
	const bool bPinBIsComposite = PinB->GetOwningNode()->IsA(UBehaviorTreeGraphNode_Composite::StaticClass());

	if ((bPinAIsSingleComposite && !bPinBIsComposite) || (bPinBIsSingleComposite && !bPinAIsComposite))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorOnlyComposite","Only composite nodes are allowed"));
	}

	if ((bPinAIsSingleTask && !bPinBIsTask) || (bPinBIsSingleTask && !bPinAIsTask))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorOnlyTask","Only task nodes are allowed"));
	}

	// Compare the directions
	if ((PinA->Direction == EGPD_Input) && (PinB->Direction == EGPD_Input))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorInput", "Can't connect input node to input node"));
	}
	else if ((PinB->Direction == EGPD_Output) && (PinA->Direction == EGPD_Output))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorOutput", "Can't connect output node to output node"));
	}

	class FNodeVisitorCycleChecker
	{
	public:
		/** Check whether a loop in the graph would be caused by linking the passed-in nodes */
		bool CheckForLoop(UEdGraphNode* StartNode, UEdGraphNode* EndNode)
		{
			VisitedNodes.Add(EndNode);
			return TraverseInputNodesToRoot(StartNode);
		}

	private:
		/** 
		 * Helper function for CheckForLoop()
		 * @param	Node	The node to start traversal at
		 * @return true if we reached a root node (i.e. a node with no input pins), false if we encounter a node we have already seen
		 */
		bool TraverseInputNodesToRoot(UEdGraphNode* Node)
		{
			VisitedNodes.Add(Node);

			// Follow every input pin until we cant any more ('root') or we reach a node we have seen (cycle)
			for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
			{
				UEdGraphPin* MyPin = Node->Pins[PinIndex];

				if (MyPin->Direction == EGPD_Input)
				{
					for (int32 LinkedPinIndex = 0; LinkedPinIndex < MyPin->LinkedTo.Num(); ++LinkedPinIndex)
					{
						UEdGraphPin* OtherPin = MyPin->LinkedTo[LinkedPinIndex];
						if( OtherPin )
						{
							UEdGraphNode* OtherNode = OtherPin->GetOwningNode();
							if (VisitedNodes.Contains(OtherNode))
							{
								return false;
							}
							else
							{
								return TraverseInputNodesToRoot(OtherNode);
							}
						}
					}
				}
			}

			return true;
		}

		TSet<UEdGraphNode*> VisitedNodes;
	};

	// check for cycles
	FNodeVisitorCycleChecker CycleChecker;
	if(!CycleChecker.CheckForLoop(PinA->GetOwningNode(), PinB->GetOwningNode()))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorcycle", "Can't create a graph cycle"));
	}

	const bool bPinASingleLink = bPinAIsSingleComposite || bPinAIsSingleTask || bPinAIsSingleNode;
	const bool bPinBSingleLink = bPinBIsSingleComposite || bPinBIsSingleTask || bPinBIsSingleNode;

	if (PinB->Direction == EGPD_Input && PinB->LinkedTo.Num() > 0)
	{
		if(bPinASingleLink)
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_AB, LOCTEXT("PinConnectReplace", "Replace connection"));
		}
		else
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, LOCTEXT("PinConnectReplace", "Replace connection"));
		}
	}
	else if (PinA->Direction == EGPD_Input && PinA->LinkedTo.Num() > 0)
	{
		if (bPinBSingleLink)
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_AB, LOCTEXT("PinConnectReplace", "Replace connection"));
		}
		else
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, LOCTEXT("PinConnectReplace", "Replace connection"));
		}
	}

	if (bPinASingleLink && PinA->LinkedTo.Num() > 0)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, LOCTEXT("PinConnectReplace", "Replace connection"));
	}
	else if(bPinBSingleLink && PinB->LinkedTo.Num() > 0)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, LOCTEXT("PinConnectReplace", "Replace connection"));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("PinConnect", "Connect nodes"));
}

const FPinConnectionResponse UEdGraphSchema_BehaviorTree::CanMergeNodes(const UEdGraphNode* NodeA, const UEdGraphNode* NodeB) const
{
	// Make sure the nodes are not the same 
	if (NodeA == NodeB)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Both are the same node"));
	}

	const bool bNodeAIsDecorator = NodeA->IsA(UBehaviorTreeGraphNode_Decorator::StaticClass()) || NodeA->IsA(UBehaviorTreeGraphNode_CompositeDecorator::StaticClass());
	const bool bNodeAIsService = NodeA->IsA(UBehaviorTreeGraphNode_Service::StaticClass());
	const bool bNodeBIsComposite = NodeB->IsA(UBehaviorTreeGraphNode_Composite::StaticClass());
	const bool bNodeBIsTask = NodeB->IsA(UBehaviorTreeGraphNode_Task::StaticClass());
	const bool bNodeBIsDecorator = NodeB->IsA(UBehaviorTreeGraphNode_Decorator::StaticClass()) || NodeB->IsA(UBehaviorTreeGraphNode_CompositeDecorator::StaticClass());
	const bool bNodeBIsService = NodeB->IsA(UBehaviorTreeGraphNode_Service::StaticClass());

	if (FBehaviorTreeDebugger::IsPIENotSimulating())
	{
		if (bNodeAIsDecorator)
		{
			const UBehaviorTreeGraphNode* BTNodeA = Cast<const UBehaviorTreeGraphNode>(NodeA);
			const UBehaviorTreeGraphNode* BTNodeB = Cast<const UBehaviorTreeGraphNode>(NodeB);
			
			if (BTNodeA && BTNodeA->bInjectedNode)
			{
				return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("MergeInjectedNodeNoMove", "Can't move injected nodes!"));
			}

			if (BTNodeB && BTNodeB->bInjectedNode)
			{
				const UBehaviorTreeGraphNode* ParentNodeB = Cast<const UBehaviorTreeGraphNode>(BTNodeB->ParentNode);

				int32 FirstInjectedIdx = INDEX_NONE;
				for (int32 Idx = 0; Idx < ParentNodeB->Decorators.Num(); Idx++)
				{
					if (ParentNodeB->Decorators[Idx]->bInjectedNode)
					{
						FirstInjectedIdx = Idx;
						break;
					}
				}

				int32 NodeIdx = ParentNodeB->Decorators.IndexOfByKey(BTNodeB);
				if (NodeIdx != FirstInjectedIdx)
				{
					return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("MergeInjectedNodeAtEnd", "Decorators must be placed above injected nodes!"));
				}
			}

			if (BTNodeB && BTNodeB->Decorators.Num())
			{
				for (int32 Idx = 0; Idx < BTNodeB->Decorators.Num(); Idx++)
				{
					if (BTNodeB->Decorators[Idx]->bInjectedNode)
					{
						return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("MergeInjectedNodeAtEnd", "Decorators must be placed above injected nodes!"));
					}
				}
			}
		}

		if ((bNodeAIsDecorator && (bNodeBIsComposite || bNodeBIsTask || bNodeBIsDecorator))
			|| (bNodeAIsService && (bNodeBIsComposite || bNodeBIsTask || bNodeBIsService)))
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
		}
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT(""));
}

FLinearColor UEdGraphSchema_BehaviorTree::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FColor::White;
}

class FConnectionDrawingPolicy* UEdGraphSchema_BehaviorTree::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const
{
	return new FBehaviorTreeConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

int32 UEdGraphSchema_BehaviorTree::GetNodeSelectionCount(const UEdGraph* Graph) const
{
	if (Graph)
	{
		TSharedPtr<IBehaviorTreeEditor> BTEditor;
		if (UBehaviorTree* BTAsset = Cast<UBehaviorTree>(Graph->GetOuter()))
		{
			TSharedPtr< IToolkit > BTAssetEditor = FToolkitManager::Get().FindEditorForAsset(BTAsset);
			if (BTAssetEditor.IsValid())
			{
				BTEditor = StaticCastSharedPtr<IBehaviorTreeEditor>(BTAssetEditor);
			}
		}
		if(BTEditor.IsValid())
		{
			return BTEditor->GetSelectedNodesCount();
		}
	}

	return 0;
}

bool UEdGraphSchema_BehaviorTree::IsCacheVisualizationOutOfDate(int32 InVisualizationCacheID) const
{
	return CurrentCacheRefreshID != InVisualizationCacheID;
}

int32 UEdGraphSchema_BehaviorTree::GetCurrentVisualizationCacheID() const
{
	return CurrentCacheRefreshID;
}

void UEdGraphSchema_BehaviorTree::ForceVisualizationCacheClear() const
{
	++CurrentCacheRefreshID;
}

TSharedPtr<FEdGraphSchemaAction> UEdGraphSchema_BehaviorTree::GetCreateCommentAction() const
{
	return TSharedPtr<FEdGraphSchemaAction>(static_cast<FEdGraphSchemaAction*>(new FBehaviorTreeSchemaAction_AddComment));
}

#undef LOCTEXT_NAMESPACE
