// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackGraphUtilities.h"
#include "NiagaraParameterMapHistory.h"
#include "NiagaraSystemViewModel.h"
#include "NiagaraSystemScriptViewModel.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeFunctionCall.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraStackEntry.h"
#include "NiagaraStackFunctionInputCollection.h"
#include "NiagaraStackFunctionInput.h"
#include "NiagaraSystem.h"
#include "NiagaraEditorModule.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"

DECLARE_CYCLE_STAT(TEXT("StackGraphUtilities - RelayoutGraph"), STAT_NiagaraEditor_StackGraphUtilities_RelayoutGraph, STATGROUP_NiagaraEditor);

#define LOCTEXT_NAMESPACE "NiagaraStackGraphUtilities"

void FNiagaraStackGraphUtilities::RelayoutGraph(UEdGraph& Graph)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_StackGraphUtilities_RelayoutGraph);
	TArray<TArray<TArray<UEdGraphNode*>>> OutputNodeTraversalStacks;
	TArray<UNiagaraNodeOutput*> OutputNodes;
	Graph.GetNodesOfClass(OutputNodes);
	TSet<UEdGraphNode*> AllTraversedNodes;
	for (UNiagaraNodeOutput* OutputNode : OutputNodes)
	{
		TSet<UEdGraphNode*> TraversedNodes;
		TArray<TArray<UEdGraphNode*>> TraversalStack;
		TArray<UEdGraphNode*> CurrentNodesToTraverse;
		CurrentNodesToTraverse.Add(OutputNode);
		while (CurrentNodesToTraverse.Num() > 0)
		{
			TArray<UEdGraphNode*> TraversedNodesThisLevel;
			TArray<UEdGraphNode*> NextNodesToTraverse;
			for (UEdGraphNode* CurrentNodeToTraverse : CurrentNodesToTraverse)
			{
				if (TraversedNodes.Contains(CurrentNodeToTraverse))
				{
					continue;
				}
				
				for (UEdGraphPin* Pin : CurrentNodeToTraverse->GetAllPins())
				{
					if (Pin->Direction == EGPD_Input)
					{
						for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
						{
							if (LinkedPin->GetOwningNode() != nullptr)
							{
								NextNodesToTraverse.Add(LinkedPin->GetOwningNode());
							}
						}
					}
				}
				TraversedNodes.Add(CurrentNodeToTraverse);
				TraversedNodesThisLevel.Add(CurrentNodeToTraverse);
			}
			TraversalStack.Add(TraversedNodesThisLevel);
			CurrentNodesToTraverse.Empty();
			CurrentNodesToTraverse.Append(NextNodesToTraverse);
		}
		OutputNodeTraversalStacks.Add(TraversalStack);
		AllTraversedNodes = AllTraversedNodes.Union(TraversedNodes);
	}

	// Find all nodes which were not traversed and put them them in a separate traversal stack.
	TArray<UEdGraphNode*> UntraversedNodes;
	for (UEdGraphNode* Node : Graph.Nodes)
	{
		if (AllTraversedNodes.Contains(Node) == false)
		{
			UntraversedNodes.Add(Node);
		}
	}
	TArray<TArray<UEdGraphNode*>> UntraversedNodeStack;
	for (UEdGraphNode* UntraversedNode : UntraversedNodes)
	{
		TArray<UEdGraphNode*> UntraversedStackItem;
		UntraversedStackItem.Add(UntraversedNode);
		UntraversedNodeStack.Add(UntraversedStackItem);
	}
	OutputNodeTraversalStacks.Add(UntraversedNodeStack);

	// Layout the traversed node stacks.
	float YOffset = 0;
	float XDistance = 400;
	float YDistance = 50;
	float YPinDistance = 50;
	for (const TArray<TArray<UEdGraphNode*>>& TraversalStack : OutputNodeTraversalStacks)
	{
		float CurrentXOffset = 0;
		float MaxYOffset = YOffset;
		for (const TArray<UEdGraphNode*> TraversalLevel : TraversalStack)
		{
			float CurrentYOffset = YOffset;
			for (UEdGraphNode* Node : TraversalLevel)
			{
				Node->Modify();
				Node->NodePosX = CurrentXOffset;
				Node->NodePosY = CurrentYOffset;
				int NumInputPins = 0;
				int NumOutputPins = 0;
				for (UEdGraphPin* Pin : Node->GetAllPins())
				{
					if (Pin->Direction == EGPD_Input)
					{
						NumInputPins++;
					}
					else
					{
						NumOutputPins++;
					}
				}
				int MaxPins = FMath::Max(NumInputPins, NumOutputPins);
				CurrentYOffset += YDistance + (MaxPins * YPinDistance);
			}
			MaxYOffset = FMath::Max(MaxYOffset, CurrentYOffset);
			CurrentXOffset -= XDistance;
		}
		YOffset = MaxYOffset + YDistance;
	}

	Graph.NotifyGraphChanged();
}

void FNiagaraStackGraphUtilities::GetWrittenVariablesForGraph(UEdGraph& Graph, TArray<FNiagaraVariable>& OutWrittenVariables)
{
	TArray<UNiagaraNodeOutput*> OutputNodes;
	Graph.GetNodesOfClass<UNiagaraNodeOutput>(OutputNodes);
	for (UNiagaraNodeOutput* OutputNode : OutputNodes)
	{
		TArray<UEdGraphPin*> InputPins;
		OutputNode->GetInputPins(InputPins);
		if (InputPins.Num() == 1)
		{
			FNiagaraParameterMapHistoryBuilder Builder;
			Builder.BuildParameterMaps(OutputNode, true);
			check(Builder.Histories.Num() == 1);
			for (int32 i = 0; i < Builder.Histories[0].Variables.Num(); i++)
			{
				if (Builder.Histories[0].PerVariableWriteHistory[i].Num() > 0)
				{
					OutWrittenVariables.Add(Builder.Histories[0].Variables[i]);
				}
			}
		}
	}
}

void FNiagaraStackGraphUtilities::ConnectPinToInputNode(UEdGraphPin& Pin, UNiagaraNodeInput& InputNode)
{
	TArray<UEdGraphPin*> InputPins;
	InputNode.GetOutputPins(InputPins);
	if (InputPins.Num() == 1)
	{
		Pin.MakeLinkTo(InputPins[0]);
	}
}

UEdGraphPin* GetParameterMapPin(const TArray<UEdGraphPin*>& Pins)
{
	auto IsParameterMapPin = [](const UEdGraphPin* Pin)
	{
		const UEdGraphSchema_Niagara* NiagaraSchema = CastChecked<UEdGraphSchema_Niagara>(Pin->GetSchema());
		FNiagaraTypeDefinition PinDefinition = NiagaraSchema->PinToTypeDefinition(Pin);
		return PinDefinition == FNiagaraTypeDefinition::GetParameterMapDef();
	};

	UEdGraphPin*const* ParameterMapPinPtr = Pins.FindByPredicate(IsParameterMapPin);

	return ParameterMapPinPtr != nullptr ? *ParameterMapPinPtr : nullptr;
}

UEdGraphPin* FNiagaraStackGraphUtilities::GetParameterMapInputPin(UNiagaraNode& Node)
{
	TArray<UEdGraphPin*> InputPins;
	Node.GetInputPins(InputPins);
	return GetParameterMapPin(InputPins);
}

UEdGraphPin* FNiagaraStackGraphUtilities::GetParameterMapOutputPin(UNiagaraNode& Node)
{
	TArray<UEdGraphPin*> OutputPins;
	Node.GetOutputPins(OutputPins);
	return GetParameterMapPin(OutputPins);
}

void FNiagaraStackGraphUtilities::GetOrderedModuleNodes(UNiagaraNodeOutput& OutputNode, TArray<UNiagaraNodeFunctionCall*>& ModuleNodes)
{
	UNiagaraNode* PreviousNode = &OutputNode;
	while (PreviousNode != nullptr)
	{
		UEdGraphPin* PreviousNodeInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*PreviousNode);
		if (PreviousNodeInputPin != nullptr && PreviousNodeInputPin->LinkedTo.Num() == 1)
		{
			UNiagaraNode* CurrentNode = Cast<UNiagaraNode>(PreviousNodeInputPin->LinkedTo[0]->GetOwningNode());
			UNiagaraNodeFunctionCall* ModuleNode = Cast<UNiagaraNodeFunctionCall>(CurrentNode);
			if (ModuleNode != nullptr)
			{
				ModuleNodes.Insert(ModuleNode, 0);
			}
			PreviousNode = CurrentNode;
		}
		else
		{
			PreviousNode = nullptr;
		}
	}
}

UNiagaraNodeFunctionCall* FNiagaraStackGraphUtilities::GetPreviousModuleNode(UNiagaraNodeFunctionCall& CurrentNode)
{
	UNiagaraNodeOutput* OutputNode = GetEmitterOutputNodeForStackNode(CurrentNode);
	if (OutputNode != nullptr)
	{
		TArray<UNiagaraNodeFunctionCall*> ModuleNodes;
		GetOrderedModuleNodes(*OutputNode, ModuleNodes);

		int32 ModuleIndex;
		ModuleNodes.Find(&CurrentNode, ModuleIndex);
		return ModuleIndex > 0 ? ModuleNodes[ModuleIndex - 1] : nullptr;
	}
	return nullptr;
}

UNiagaraNodeFunctionCall* FNiagaraStackGraphUtilities::GetNextModuleNode(UNiagaraNodeFunctionCall& CurrentNode)
{
	UNiagaraNodeOutput* OutputNode = GetEmitterOutputNodeForStackNode(CurrentNode);
	if (OutputNode != nullptr)
	{
		TArray<UNiagaraNodeFunctionCall*> ModuleNodes;
		GetOrderedModuleNodes(*OutputNode, ModuleNodes);

		int32 ModuleIndex;
		ModuleNodes.Find(&CurrentNode, ModuleIndex);
		return ModuleIndex < ModuleNodes.Num() - 2 ? ModuleNodes[ModuleIndex + 1] : nullptr;
	}
	return nullptr;
}

UNiagaraNodeOutput* FNiagaraStackGraphUtilities::GetEmitterOutputNodeForStackNode(UNiagaraNode& StackNode)
{
	TArray<UNiagaraNode*> NodesToCheck;
	NodesToCheck.Add(&StackNode);
	while (NodesToCheck.Num() > 0)
	{
		UNiagaraNode* NodeToCheck = NodesToCheck[0];
		NodesToCheck.RemoveAt(0);
		if (NodeToCheck->GetClass() == UNiagaraNodeOutput::StaticClass())
		{
			return CastChecked<UNiagaraNodeOutput>(NodeToCheck);
		}
		
		TArray<UEdGraphPin*> OutputPins;
		NodeToCheck->GetOutputPins(OutputPins);
		for (UEdGraphPin* OutputPin : OutputPins)
		{
			for (UEdGraphPin* LinkedPin : OutputPin->LinkedTo)
			{
				UNiagaraNode* LinkedNiagaraNode = Cast<UNiagaraNode>(LinkedPin->GetOwningNode());
				if (LinkedNiagaraNode != nullptr)
				{
					NodesToCheck.Add(LinkedNiagaraNode);
				}
			}
		}
	}
	return nullptr;
}

UNiagaraNodeInput* FNiagaraStackGraphUtilities::GetEmitterInputNodeForStackNode(UNiagaraNode& StackNode)
{
	// Since the stack graph can have arbitrary branches when traversing inputs, the only safe way to get the initial input
	// is to start at the output node and then trace only parameter map inputs.
	UNiagaraNodeOutput* OutputNode = GetEmitterOutputNodeForStackNode(StackNode);

	UNiagaraNode* PreviousNode = OutputNode;
	while (PreviousNode != nullptr)
	{
		UEdGraphPin* PreviousNodeInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*PreviousNode);
		if (PreviousNodeInputPin != nullptr && PreviousNodeInputPin->LinkedTo.Num() == 1)
		{
			UNiagaraNode* CurrentNode = Cast<UNiagaraNode>(PreviousNodeInputPin->LinkedTo[0]->GetOwningNode());
			UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(CurrentNode);
			if (InputNode != nullptr)
			{
				return InputNode;
			}
			PreviousNode = CurrentNode;
		}
		else
		{
			PreviousNode = nullptr;
		}
	}
	return nullptr;
}

void FNiagaraStackGraphUtilities::GetStackNodeGroups(UNiagaraNode& StackNode, TArray<FNiagaraStackGraphUtilities::FStackNodeGroup>& OutStackNodeGroups)
{
	UNiagaraNodeOutput* OutputNode = GetEmitterOutputNodeForStackNode(StackNode);
	if (OutputNode != nullptr)
	{
		UNiagaraNodeInput* InputNode = GetEmitterInputNodeForStackNode(*OutputNode);
		if (InputNode != nullptr)
		{
			FStackNodeGroup InputGroup;
			InputGroup.StartNodes.Add(InputNode);
			InputGroup.EndNode = InputNode;
			OutStackNodeGroups.Add(InputGroup);

			TArray<UNiagaraNodeFunctionCall*> ModuleNodes;
			GetOrderedModuleNodes(*OutputNode, ModuleNodes);
			for (UNiagaraNodeFunctionCall* ModuleNode : ModuleNodes)
			{
				FStackNodeGroup ModuleGroup;
				UEdGraphPin* PreviousOutputPin = GetParameterMapOutputPin(*OutStackNodeGroups.Last().EndNode);
				for (UEdGraphPin* PreviousOutputPinLinkedPin : PreviousOutputPin->LinkedTo)
				{
					ModuleGroup.StartNodes.Add(CastChecked<UNiagaraNode>(PreviousOutputPinLinkedPin->GetOwningNode()));
				}
				ModuleGroup.EndNode = ModuleNode;
				OutStackNodeGroups.Add(ModuleGroup);
			}

			FStackNodeGroup OutputGroup;
			UEdGraphPin* PreviousOutputPin = GetParameterMapOutputPin(*OutStackNodeGroups.Last().EndNode);
			for (UEdGraphPin* PreviousOutputPinLinkedPin : PreviousOutputPin->LinkedTo)
			{
				OutputGroup.StartNodes.Add(CastChecked<UNiagaraNode>(PreviousOutputPinLinkedPin->GetOwningNode()));
			}
			OutputGroup.EndNode = OutputNode;
			OutStackNodeGroups.Add(OutputGroup);
		}
	}
}

void FNiagaraStackGraphUtilities::DisconnectStackNodeGroup(const FStackNodeGroup& DisconnectGroup, const FStackNodeGroup& PreviousGroup, const FStackNodeGroup& NextGroup)
{
	UEdGraphPin* PreviousOutputPin = FNiagaraStackGraphUtilities::GetParameterMapOutputPin(*PreviousGroup.EndNode);
	PreviousOutputPin->BreakAllPinLinks();

	UEdGraphPin* DisconnectOutputPin = FNiagaraStackGraphUtilities::GetParameterMapOutputPin(*DisconnectGroup.EndNode);
	DisconnectOutputPin->BreakAllPinLinks();

	for (UNiagaraNode* NextStartNode : NextGroup.StartNodes)
	{
		UEdGraphPin* NextStartInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*NextStartNode);
		PreviousOutputPin->MakeLinkTo(NextStartInputPin);
	}
}

void FNiagaraStackGraphUtilities::ConnectStackNodeGroup(const FStackNodeGroup& ConnectGroup, const FStackNodeGroup& NewPreviousGroup, const FStackNodeGroup& NewNextGroup)
{
	UEdGraphPin* NewPreviousOutputPin = FNiagaraStackGraphUtilities::GetParameterMapOutputPin(*NewPreviousGroup.EndNode);
	NewPreviousOutputPin->BreakAllPinLinks();

	for (UNiagaraNode* ConnectStartNode : ConnectGroup.StartNodes)
	{
		UEdGraphPin* ConnectInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*ConnectStartNode);
		NewPreviousOutputPin->MakeLinkTo(ConnectInputPin);
	}

	UEdGraphPin* ConnectOutputPin = FNiagaraStackGraphUtilities::GetParameterMapOutputPin(*ConnectGroup.EndNode);

	for (UNiagaraNode* NewNextStartNode : NewNextGroup.StartNodes)
	{
		UEdGraphPin* NewNextStartInputPin = FNiagaraStackGraphUtilities::GetParameterMapInputPin(*NewNextStartNode);
		ConnectOutputPin->MakeLinkTo(NewNextStartInputPin);
	}
}

void FNiagaraStackGraphUtilities::InitializeDataInterfaceInputs(TSharedRef<FNiagaraSystemViewModel> SystemViewModel, TSharedRef<FNiagaraEmitterViewModel> EmitterViewModel, UNiagaraStackEditorData& StackEditorData, UNiagaraNodeFunctionCall& ModuleNode, UNiagaraNodeFunctionCall& InputFunctionCallNode)
{
	UNiagaraStackFunctionInputCollection* FunctionInputCollection = NewObject<UNiagaraStackFunctionInputCollection>(GetTransientPackage()); 
	FunctionInputCollection->Initialize(SystemViewModel, EmitterViewModel, StackEditorData, ModuleNode, InputFunctionCallNode, UNiagaraStackFunctionInputCollection::FDisplayOptions());
	FunctionInputCollection->RefreshChildren();

	TArray<UNiagaraStackEntry*> Children;
	FunctionInputCollection->GetChildren(Children);
	for (UNiagaraStackEntry* Child : Children)
	{
		UNiagaraStackFunctionInput* FunctionInput = Cast<UNiagaraStackFunctionInput>(Child);
		if (FunctionInput != nullptr && FunctionInput->GetInputType().GetClass() != nullptr)
		{
			FunctionInput->Reset();
		}
	}

	SystemViewModel->NotifyDataObjectChanged(nullptr);
}

FString FNiagaraStackGraphUtilities::GenerateStackFunctionInputEditorDataKey(UNiagaraNodeFunctionCall& FunctionCallNode, FNiagaraParameterHandle InputParameterHandle)
{
	return FunctionCallNode.GetFunctionName() + InputParameterHandle.GetParameterHandleString();
}

FString FNiagaraStackGraphUtilities::GenerateStackModuleEditorDataKey(UNiagaraNodeFunctionCall& ModuleNode)
{
	return ModuleNode.GetFunctionName();
}

void FNiagaraStackGraphUtilities::GetStackFunctionInputPins(UNiagaraNodeFunctionCall& FunctionCallNode, TArray<const UEdGraphPin*>& OutInputPins, ENiagaraGetStackFunctionInputPinsOptions Options)
{
	FNiagaraParameterMapHistoryBuilder Builder;
	FunctionCallNode.BuildParameterMapHistory(Builder, false);
	
	if (Builder.Histories.Num() == 1)
	{
		for (int32 i = 0; i < Builder.Histories[0].Variables.Num(); i++)
		{
			FNiagaraVariable& Variable = Builder.Histories[0].Variables[i];
			TArray<TTuple<const UEdGraphPin*, const UEdGraphPin*>>& ReadHistory = Builder.Histories[0].PerVariableReadHistory[i];

			// A read is only really exposed if it's the first read and it has no corresponding write.
			if (ReadHistory.Num() > 0 && ReadHistory[0].Get<1>() == nullptr)
			{
				const UEdGraphPin* InputPin = ReadHistory[0].Get<0>();
				if (Options == ENiagaraGetStackFunctionInputPinsOptions::AllInputs || FNiagaraParameterHandle(InputPin->PinName).IsModuleHandle())
				{
					OutInputPins.Add(InputPin);
				}
			}
		}
	}
}

bool FNiagaraStackGraphUtilities::ValidateGraphForOutput(UNiagaraGraph& NiagaraGraph, ENiagaraScriptUsage ScriptUsage, int32 ScriptOccurrence, FText& ErrorMessage)
{
	UNiagaraNodeOutput* OutputNode = NiagaraGraph.FindOutputNode(ScriptUsage, ScriptOccurrence);
	if (OutputNode == nullptr)
	{
		ErrorMessage = LOCTEXT("ValidateNoOutputMessage", "Output node doesn't exist for script.");
		return false;
	}

	TArray<FStackNodeGroup> NodeGroups;
	GetStackNodeGroups(*OutputNode, NodeGroups);
	
	if (NodeGroups.Num() < 2 || NodeGroups[0].EndNode->IsA<UNiagaraNodeInput>() == false || NodeGroups.Last().EndNode->IsA<UNiagaraNodeOutput>() == false)
	{
		ErrorMessage = LOCTEXT("ValidateInvalidStackMessage", "Stack graph does not include an input node connected to an output node.");
		return false;
	}

	return true;
}

UNiagaraNodeOutput* FNiagaraStackGraphUtilities::ResetGraphForOutput(UNiagaraGraph& NiagaraGraph, ENiagaraScriptUsage ScriptUsage, int32 ScriptOccurrence)
{
	NiagaraGraph.Modify();
	UNiagaraNodeOutput* OutputNode = NiagaraGraph.FindOutputNode(ScriptUsage, ScriptOccurrence);
	UEdGraphPin* OutputNodeInputPin = OutputNode != nullptr ? GetParameterMapInputPin(*OutputNode) : nullptr;
	if (OutputNode != nullptr && OutputNodeInputPin == nullptr)
	{
		NiagaraGraph.RemoveNode(OutputNode);
		OutputNode = nullptr;
	}

	if (OutputNode == nullptr)
	{
		FGraphNodeCreator<UNiagaraNodeOutput> OutputNodeCreator(NiagaraGraph);
		OutputNode = OutputNodeCreator.CreateNode();
		OutputNode->SetUsage(ScriptUsage);
		OutputNode->SetUsageIndex(ScriptOccurrence);
		OutputNode->Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("Out")));
		OutputNodeCreator.Finalize();

		OutputNodeInputPin = GetParameterMapInputPin(*OutputNode);
	}
	else
	{
		OutputNode->Modify();
	}

	FNiagaraVariable InputVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("InputMap"));
	FGraphNodeCreator<UNiagaraNodeInput> InputNodeCreator(NiagaraGraph);
	UNiagaraNodeInput* InputNode = InputNodeCreator.CreateNode();
	InputNode->Input = FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("InputMap"));
	InputNode->Usage = ENiagaraInputNodeUsage::Parameter;
	InputNodeCreator.Finalize();

	UEdGraphPin* InputNodeOutputPin = GetParameterMapOutputPin(*InputNode);
	OutputNodeInputPin->BreakAllPinLinks();
	OutputNodeInputPin->MakeLinkTo(InputNodeOutputPin);

	if (ScriptUsage == ENiagaraScriptUsage::SystemSpawnScript || ScriptUsage == ENiagaraScriptUsage::SystemUpdateScript)
	{
		// TODO: Move the emitter node wrangling to a utility function instead of getting the typed outer here and creating a view model.
		UNiagaraSystem* System = NiagaraGraph.GetTypedOuter<UNiagaraSystem>();
		if (System != nullptr)
		{
			TSharedRef<FNiagaraSystemScriptViewModel> SystemScriptViewModel = MakeShared<FNiagaraSystemScriptViewModel>(*System);
			SystemScriptViewModel->RebuildEmitterNodes();
		}
	}

	RelayoutGraph(NiagaraGraph);

	return OutputNode;
}

#undef LOCTEXT_NAMESPACE