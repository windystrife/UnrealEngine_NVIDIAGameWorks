// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_TunnelBoundary.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Tunnel.h"
#include "K2Node_Composite.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_MacroInstance.h"
#include "KismetCompiledFunctionContext.h"
#include "KismetCompilerMisc.h"

#define LOCTEXT_NAMESPACE "FKCHandler_TunnelBoundary"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_TunnelBoundary

class FKCHandler_TunnelBoundary : public FNodeHandlingFunctor
{
public:
	FKCHandler_TunnelBoundary(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		if (UK2Node_TunnelBoundary* BoundaryNode = Cast<UK2Node_TunnelBoundary>(Node))
		{
			switch(BoundaryNode->TunnelBoundaryType)
			{
				case TBT_EntrySite:
				case TBT_ExitSite:
				{
					GenerateSimpleThenGoto(Context, *Node);
					break;
				}
				case TBT_EndOfThread:
				{
					FBlueprintCompiledStatement& ExitTunnelStatement = Context.AppendStatementForNode(Node);
					ExitTunnelStatement.Type = KCST_InstrumentedTunnelEndOfThread;
					FBlueprintCompiledStatement& EOTStatement = Context.AppendStatementForNode(Node);
					EOTStatement.Type = KCST_EndOfThread;
					break;
				}
			}
		}
	}

};

//////////////////////////////////////////////////////////////////////////
// UK2Node_TunnelBoundary

UK2Node_TunnelBoundary::UK2Node_TunnelBoundary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TunnelBoundaryType(TBT_Unknown)
{
	return;
}

FText UK2Node_TunnelBoundary::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("BaseName"), FText::FromName(BaseName));
	switch(TunnelBoundaryType)
	{
		case TBT_EntrySite:
		{
			Args.Add(TEXT("EntryType"), FText::FromString(TEXT("Tunnel Entry")));
			break;
		}
		case TBT_ExitSite:
		{
			Args.Add(TEXT("EntryType"), FText::FromString(TEXT("Tunnel Exit")));
			break;
		}
		case TBT_EndOfThread:
		{
			Args.Add(TEXT("EntryType"), FText::FromString(TEXT("Tunnel End Of Thread")));
			break;
		}
	}
	return FText::Format(LOCTEXT("UK2Node_TunnelBoundary_FullTitle", "{BaseName} {EntryType}"), Args);
}

FNodeHandlingFunctor* UK2Node_TunnelBoundary::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_TunnelBoundary(CompilerContext);
}

void UK2Node_TunnelBoundary::CreateBoundaryNodesForGraph(UEdGraph* TunnelGraph, FCompilerResultsLog& MessageLog)
{
	if (UK2Node_Tunnel* OuterTunnel = TunnelGraph->GetTypedOuter<UK2Node_Tunnel>())
	{
		UK2Node_Tunnel* SourceTunnel = Cast<UK2Node_Tunnel>(MessageLog.FindSourceObject(OuterTunnel));
		CreateBoundaryNodesForTunnelInstance(SourceTunnel, TunnelGraph, MessageLog);
	}
}

void UK2Node_TunnelBoundary::CreateBoundaryNodesForTunnelInstance(UK2Node_Tunnel* TunnelInstance, UEdGraph* TunnelGraph, FCompilerResultsLog& MessageLog)
{
	if (!TunnelGraph)
	{
		return;
	}

	TArray<UEdGraphPin*> TunnelEntryPins;
	TArray<UEdGraphPin*> TunnelExitPins;
	UEdGraphNode* TunnelExitNode = nullptr;
	TArray<UK2Node_Tunnel*> Tunnels;
	TunnelGraph->GetNodesOfClass<UK2Node_Tunnel>(Tunnels);
	for (UK2Node_Tunnel* TunnelNode : Tunnels)
	{
		if (IsPureTunnel(TunnelNode))
		{
			for (UEdGraphPin* Pin : TunnelNode->Pins)
			{
				if (Pin->LinkedTo.Num() && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				{
					if (Pin->Direction == EGPD_Output)
					{
						TunnelEntryPins.Add(Pin);
					}
					else
					{
						TunnelExitNode = TunnelNode;
						TunnelExitPins.Add(Pin);
					}
				}
			}
		}
	}
	// Find the blueprint tunnel instance.
	UEdGraphNode* SourceTunnelInstance = Cast<UEdGraphNode>(MessageLog.FindSourceObject(TunnelInstance));
	SourceTunnelInstance = FindTrueSourceTunnelInstance(TunnelInstance, SourceTunnelInstance);
	UEdGraphNode* RegistrantTunnelInstance = TunnelInstance->IsA<UK2Node_Composite>() ? SourceTunnelInstance : TunnelInstance;
	// Create the Boundary nodes for each unique entry/exit site.
	TArray<UEdGraphPin*> ExecutionEndpointPins;
	TArray<UEdGraphPin*> VisitedPins;
	for (UEdGraphPin* EntryPin : TunnelEntryPins)
	{
		UK2Node_TunnelBoundary* TunnelBoundaryNode = TunnelGraph->CreateIntermediateNode<UK2Node_TunnelBoundary>();
		MessageLog.NotifyIntermediateObjectCreation(TunnelBoundaryNode, TunnelInstance);
		TunnelBoundaryNode->CreateNewGuid();
		MessageLog.RegisterIntermediateTunnelNode(TunnelBoundaryNode, RegistrantTunnelInstance);
		TunnelBoundaryNode->WireUpTunnelEntry(TunnelInstance, EntryPin, MessageLog);
		for (UEdGraphPin* LinkedPin : EntryPin->LinkedTo)
		{
			FindTunnelExitSiteInstances(LinkedPin, ExecutionEndpointPins, VisitedPins, TunnelExitNode);
		}
	}
	if (ExecutionEndpointPins.Num())
	{
		// Create boundary node.
		UK2Node_TunnelBoundary* TunnelBoundaryNode = TunnelGraph->CreateIntermediateNode<UK2Node_TunnelBoundary>();
		MessageLog.NotifyIntermediateObjectCreation(TunnelBoundaryNode, TunnelInstance);
		TunnelBoundaryNode->CreateNewGuid();
		TunnelBoundaryNode->TunnelBoundaryType = TBT_EndOfThread;
		MessageLog.RegisterIntermediateTunnelNode(TunnelBoundaryNode, RegistrantTunnelInstance);
		// Create exit point pin
		UEdGraphPin* BoundaryTerminalPin = TunnelBoundaryNode->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, FString(), nullptr, TEXT("TunnelEndOfThread"));
		// Wire up the endpoint unwired links to the boundary node.
		for (UEdGraphPin* TerminationPin : ExecutionEndpointPins)
		{
			TerminationPin->MakeLinkTo(BoundaryTerminalPin);
		}
	}
	for (UEdGraphPin* ExitPin : TunnelExitPins)
	{
		UK2Node_TunnelBoundary* TunnelBoundaryNode = TunnelGraph->CreateIntermediateNode<UK2Node_TunnelBoundary>();
		MessageLog.NotifyIntermediateObjectCreation(TunnelBoundaryNode, TunnelInstance);
		TunnelBoundaryNode->CreateNewGuid();
		MessageLog.RegisterIntermediateTunnelNode(TunnelBoundaryNode, RegistrantTunnelInstance);
		TunnelBoundaryNode->WireUpTunnelExit(TunnelInstance, ExitPin, MessageLog);
	}
	// Build node guid map to locate true source node.
	TMap<FGuid, const UEdGraphNode*> TrueSourceNodeMap;
	BuildSourceNodeMap(SourceTunnelInstance, TrueSourceNodeMap);
}

void UK2Node_TunnelBoundary::CreateBoundariesForExpansionNodes(UEdGraphNode* SourceNode, TArray<UEdGraphNode*>& ExpansionNodes, TMap<UEdGraphPin*, UEdGraphPin*>& LinkedPinMap, FCompilerResultsLog& MessageLog)
{
	// Find boundary points and map them to the source pins
	UEdGraph* TargetGraph = nullptr;
	TMap<UEdGraphPin*, TArray<UEdGraphPin*>> EntryPins;
	TMap<UEdGraphPin*, TArray<UEdGraphPin*>> ExitPins;
	for (UEdGraphNode* ExpansionNode : ExpansionNodes)
	{
		if (!TargetGraph)
		{
			TargetGraph = ExpansionNode->GetGraph();
		}
		for (UEdGraphPin* Pin : ExpansionNode->Pins)
		{
			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				//if (ExpansionNode->IsA<UK2Node_Event>() && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				//{
				//	// Add boundary for expansion events.
				//	LinkedPinMap.Add(Pin, LinkedPin);
				//}
				if (UEdGraphPin** SourcePin = LinkedPinMap.Find(LinkedPin))
				{
					if (Pin->Direction == EGPD_Input)
					{
						TArray<UEdGraphPin*>& PinLinks = EntryPins.FindOrAdd(*SourcePin);
						PinLinks.Add(Pin);
					}
					else
					{
						TArray<UEdGraphPin*>& PinLinks = ExitPins.FindOrAdd(*SourcePin);
						PinLinks.Add(Pin);
					}
				}
			}
		}
	}
	// Create the Boundary nodes for each unique entry/exit site.
	if (TargetGraph)
	{
		if (EntryPins.Num() > 0)
		{
			for (const auto& PinEntry : EntryPins)
			{
				// Create entry node and pins
				UK2Node_TunnelBoundary* EntryBoundaryNode = TargetGraph->CreateIntermediateNode<UK2Node_TunnelBoundary>();
				MessageLog.NotifyIntermediateObjectCreation(EntryBoundaryNode, SourceNode);
				EntryBoundaryNode->CreateNewGuid();
				EntryBoundaryNode->WireUpEntry(SourceNode, PinEntry.Key, PinEntry.Value, MessageLog);
			}
		}
		if (ExitPins.Num() > 0)
		{
			for (const auto& PinExit : ExitPins)
			{
				// Create exit node and pins
				UK2Node_TunnelBoundary* ExitBoundaryNode = TargetGraph->CreateIntermediateNode<UK2Node_TunnelBoundary>();
				MessageLog.NotifyIntermediateObjectCreation(ExitBoundaryNode, SourceNode);
				ExitBoundaryNode->CreateNewGuid();
				ExitBoundaryNode->WireUpExit(SourceNode, PinExit.Key, PinExit.Value, MessageLog);
			}
		}
	}
}

bool UK2Node_TunnelBoundary::IsPureTunnel(UK2Node_Tunnel* Tunnel)
{
	return !Tunnel->IsA<UK2Node_MacroInstance>() && !Tunnel->IsA<UK2Node_Composite>();
}

void UK2Node_TunnelBoundary::WireUpTunnelEntry(UK2Node_Tunnel* TunnelInstance, UEdGraphPin* TunnelPin, FCompilerResultsLog& MessageLog)
{
	if (TunnelInstance && TunnelPin)
	{
		// Mark as entry node
		TunnelBoundaryType = TBT_EntrySite;
		// Grab the graph name
		CreateBaseNodeName(TunnelInstance);
		// Find the true source pin
		UEdGraphPin* SourcePin = TunnelInstance->FindPin(TunnelPin->PinName);
		// Wire in the node
		UEdGraphPin* OutputPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, FString(), nullptr, TEXT("TunnelEntryExec"));
		for (UEdGraphPin* LinkedPin : TunnelPin->LinkedTo)
		{
			LinkedPin->MakeLinkTo(OutputPin);
		}
		UEdGraphPin* InputPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, FString(), nullptr, SourcePin->PinName);
		MessageLog.NotifyIntermediatePinCreation(InputPin, SourcePin);
		TunnelPin->BreakAllPinLinks();
		TunnelPin->MakeLinkTo(InputPin);
	}
}

void UK2Node_TunnelBoundary::WireUpTunnelExit(UK2Node_Tunnel* TunnelInstance, UEdGraphPin* TunnelPin, FCompilerResultsLog& MessageLog)
{
	if (TunnelInstance && TunnelPin)
	{
		// Mark as exit node
		TunnelBoundaryType = TBT_ExitSite;
		// Grab the graph name
		CreateBaseNodeName(TunnelInstance);
		// Find the true source pin
		UEdGraphPin* SourcePin = TunnelInstance->FindPin(TunnelPin->PinName);
		// Wire in the node
		UEdGraphPin* InputPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, FString(), nullptr, SourcePin->PinName);
		for (UEdGraphPin* LinkedPin : TunnelPin->LinkedTo)
		{
			LinkedPin->MakeLinkTo(InputPin);
		}
		UEdGraphPin* OutputPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, FString(), nullptr, TEXT("TunnelExitExec"));
		MessageLog.NotifyIntermediatePinCreation(OutputPin, SourcePin);
		TunnelPin->BreakAllPinLinks();
		TunnelPin->MakeLinkTo(OutputPin);
	}
}

void UK2Node_TunnelBoundary::WireUpEntry(UEdGraphNode* SourceNode, UEdGraphPin* SourcePin, const TArray<UEdGraphPin*>& EntryPins, FCompilerResultsLog& MessageLog)
{
	if (SourceNode && SourcePin && EntryPins.Num())
	{
		// Mark as entry node
		TunnelBoundaryType = TBT_EntrySite;
		// Create the pin base name
		CreateBaseNodeName(SourceNode);
		// Wire in the node
		UEdGraphPin* InputPin = CreatePin(EGPD_Input, SourcePin->PinType.PinCategory, SourcePin->PinType.PinSubCategory, SourcePin->PinType.PinSubCategoryObject.Get(), TEXT("EntryBoundary"), SourcePin->PinType.ContainerType, SourcePin->PinType.bIsReference, SourcePin->PinType.bIsConst, INDEX_NONE, SourcePin->PinType.PinValueType);
		MessageLog.NotifyIntermediatePinCreation(InputPin, SourcePin);
		UEdGraphPin* OutputPin = CreatePin(EGPD_Output, SourcePin->PinType.PinCategory, SourcePin->PinType.PinSubCategory, SourcePin->PinType.PinSubCategoryObject.Get(), SourcePin->PinName, SourcePin->PinType.ContainerType, SourcePin->PinType.bIsReference, SourcePin->PinType.bIsConst, INDEX_NONE, SourcePin->PinType.PinValueType);
		for (UEdGraphPin* EntryPin : EntryPins)
		{
			for (UEdGraphPin* LinkedPin : EntryPin->LinkedTo)
			{
				check (LinkedPin->Direction != InputPin->Direction);
				LinkedPin->MakeLinkTo(InputPin);
			}
			EntryPin->BreakAllPinLinks();
			check (EntryPin->Direction != OutputPin->Direction);
			EntryPin->MakeLinkTo(OutputPin);
		}
	}
}

void UK2Node_TunnelBoundary::WireUpExit(UEdGraphNode* SourceNode, UEdGraphPin* SourcePin, const TArray<UEdGraphPin*>& ExitPins, FCompilerResultsLog& MessageLog)
{
	if (SourceNode && SourcePin && ExitPins.Num())
	{
		// Mark as exit node
		TunnelBoundaryType = TBT_ExitSite;
		// Create the pin base name
		CreateBaseNodeName(SourceNode);
		// Wire in the node
		UEdGraphPin* OutputPin = CreatePin(EGPD_Output, SourcePin->PinType.PinCategory, SourcePin->PinType.PinSubCategory, SourcePin->PinType.PinSubCategoryObject.Get(), SourcePin->PinName, SourcePin->PinType.ContainerType, SourcePin->PinType.bIsReference, SourcePin->PinType.bIsConst, INDEX_NONE, SourcePin->PinType.PinValueType);
		MessageLog.NotifyIntermediatePinCreation(OutputPin, SourcePin);
		UEdGraphPin* InputPin = CreatePin(EGPD_Input, SourcePin->PinType.PinCategory, SourcePin->PinType.PinSubCategory, SourcePin->PinType.PinSubCategoryObject.Get(), TEXT("ExitBoundary"), SourcePin->PinType.ContainerType, SourcePin->PinType.bIsReference, SourcePin->PinType.bIsConst, INDEX_NONE, SourcePin->PinType.PinValueType);
		for (UEdGraphPin* ExitPin : ExitPins)
		{
			for (UEdGraphPin* LinkedPin : ExitPin->LinkedTo)
			{
				check (LinkedPin->Direction != OutputPin->Direction);
				LinkedPin->MakeLinkTo(OutputPin);
			}
			ExitPin->BreakAllPinLinks();
			check (ExitPin->Direction != InputPin->Direction);
			ExitPin->MakeLinkTo(InputPin);
		}
	}
}

void UK2Node_TunnelBoundary::CreateBaseNodeName(UEdGraphNode* SourceNode)
{
	UEdGraph* TunnelGraph = nullptr;
	if (const UK2Node_MacroInstance* MacroInstance = Cast<UK2Node_MacroInstance>(SourceNode))
	{
		TunnelGraph = MacroInstance->GetMacroGraph();
		BaseName = TunnelGraph ? TunnelGraph->GetFName() : NAME_None;
	}
	else if (const UK2Node_Composite* CompositeInstance = Cast<UK2Node_Composite>(SourceNode))
	{
		TunnelGraph = CompositeInstance->BoundGraph;
		BaseName = TunnelGraph ? TunnelGraph->GetFName() : NAME_None;
	}
	else
	{
		BaseName = SourceNode ? SourceNode->GetFName() : NAME_None;
	}
}

void UK2Node_TunnelBoundary::BuildSourceNodeMap(UEdGraphNode* Tunnel, TMap<FGuid, const UEdGraphNode*>& SourceNodeMap)
{
	// Crawl the graph and locate source node guids
	UEdGraph* TunnelGraph = nullptr;
	if (UK2Node_MacroInstance* SourceMacroInstance = Cast<UK2Node_MacroInstance>(Tunnel))
	{
		TunnelGraph = SourceMacroInstance->GetMacroGraph();
	}
	else if (UK2Node_Composite* SourceCompositeInstance = Cast<UK2Node_Composite>(Tunnel))
	{
		TunnelGraph = SourceCompositeInstance->BoundGraph;
	}
	if (TunnelGraph)
	{
		for (UEdGraphNode* GraphNode : TunnelGraph->Nodes)
		{
			SourceNodeMap.Add(GraphNode->NodeGuid) = GraphNode;
		}
	}
}

UEdGraphNode* UK2Node_TunnelBoundary::FindTrueSourceTunnelInstance(UEdGraphNode* Tunnel, UEdGraphNode* SourceTunnelInstance)
{
	UEdGraphNode* SourceNode = nullptr;
	if (Tunnel && SourceTunnelInstance)
	{
		if (Tunnel->NodeGuid == SourceTunnelInstance->NodeGuid)
		{
			SourceNode = SourceTunnelInstance;
		}
		else
		{
			UEdGraph* TunnelGraph = nullptr;
			if (UK2Node_MacroInstance* SourceMacroInstance = Cast<UK2Node_MacroInstance>(SourceTunnelInstance))
			{
				TunnelGraph = SourceMacroInstance->GetMacroGraph();
			}
			else if (UK2Node_Composite* SourceCompositeInstance = Cast<UK2Node_Composite>(SourceTunnelInstance))
			{
				TunnelGraph = SourceCompositeInstance->BoundGraph;
			}
			if (TunnelGraph)
			{
				for (UEdGraphNode* GraphNode : TunnelGraph->Nodes)
				{
					if (GraphNode->NodeGuid == Tunnel->NodeGuid)
					{
						SourceNode = GraphNode;
						break;
					}

					if (GraphNode->IsA<UK2Node_Composite>() || GraphNode->IsA<UK2Node_MacroInstance>())
					{
						SourceNode = FindTrueSourceTunnelInstance(Tunnel, GraphNode);
					if (SourceNode)
					{
						break;
					}
				}
			}
		}
	}
	}
	return SourceNode;
}

void UK2Node_TunnelBoundary::FindTunnelExitSiteInstances(UEdGraphPin* NodeEntryPin, TArray<UEdGraphPin*>& ExitPins, TArray<UEdGraphPin*>& VisitedPins, const UEdGraphNode* TunnelExitNode)
{
	UEdGraphNode* PinNode = NodeEntryPin->GetOwningNode();

	// Grab pin node
	if (PinNode != nullptr && PinNode != TunnelExitNode && !VisitedPins.Contains(NodeEntryPin))
	{
		VisitedPins.Push(NodeEntryPin);

		// Find all exec out pins
		TArray<UEdGraphPin*> ExecPins;
		TArray<UEdGraphPin*> ConnectedExecPins;
		for (UEdGraphPin* Pin : PinNode->Pins)
		{
			if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				ExecPins.Add(Pin);
				if (Pin->LinkedTo.Num() > 0)
				{
					ConnectedExecPins.Add(Pin);
				}
			}
		}
		if (ConnectedExecPins.Num() > 1)
		{
			if (PinNode->IsA<UK2Node_ExecutionSequence>())
			{
				// Execution sequence style nodes, follow the last connected pin
				for (UEdGraphPin* LinkedPin : ConnectedExecPins.Last()->LinkedTo)
				{
					FindTunnelExitSiteInstances(LinkedPin, ExitPins, VisitedPins);
				}
			}
			else
			{
				// Branch style nodes, more tricky
				for (UEdGraphPin* ExecPin : ConnectedExecPins)
				{
					for (UEdGraphPin* LinkedPin : ExecPin->LinkedTo)
					{
						FindTunnelExitSiteInstances(LinkedPin, ExitPins, VisitedPins);
					}
				}
			}
		}
		else if (ConnectedExecPins.Num() == 1)
		{
			for (UEdGraphPin* LinkedPin : ConnectedExecPins[0]->LinkedTo)
			{
				FindTunnelExitSiteInstances(LinkedPin, ExitPins, VisitedPins);
			}
		}
		else if (ExecPins.Num() > 0)
		{
			ExitPins.Add(ExecPins.Last());
		}

		VisitedPins.Pop(false);
	}
}

#undef LOCTEXT_NAMESPACE
