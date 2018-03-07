// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraGraph.h"
#include "Modules/ModuleManager.h"
#include "NiagaraCommon.h"
#include "NiagaraEditorModule.h"
#include "NiagaraScript.h"
#include "NiagaraComponent.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "ComponentReregisterContext.h"
#include "NiagaraConstants.h"
#include "NiagaraSystem.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeWriteDataSet.h"
#include "NiagaraNodeReadDataSet.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraDataInterface.h"
#include "GraphEditAction.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraNodeParameterMapBase.h"
#include "NiagaraNodeParameterMapGet.h"
#include "INiagaraEditorTypeUtilities.h"
#include "NiagaraConstants.h"


UNiagaraGraph::UNiagaraGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Schema = UEdGraphSchema_Niagara::StaticClass();
	ChangeId = FGuid::NewGuid();
}

void UNiagaraGraph::PostLoad()
{
	Super::PostLoad();

	// In the past, we didn't bother setting the CallSortPriority and just used lexicographic ordering.
	// In the event that we have multiple non-matching nodes with a zero call sort priority, this will
	// give every node a unique order value.
	TArray<UNiagaraNodeInput*> InputNodes;
	GetNodesOfClass(InputNodes);
	bool bAllZeroes = true;
	TArray<FName> UniqueNames;
	for (UNiagaraNodeInput* InputNode : InputNodes)
	{
		if (InputNode->CallSortPriority != 0)
		{
			bAllZeroes = false;
		}

		if (InputNode->Usage == ENiagaraInputNodeUsage::Parameter)
		{
			UniqueNames.AddUnique(InputNode->Input.GetName());
		}

		if (InputNode->Usage == ENiagaraInputNodeUsage::SystemConstant)
		{
			InputNode->Input = FNiagaraConstants::UpdateEngineConstant(InputNode->Input);
		}
	}



	if (bAllZeroes && UniqueNames.Num() > 1)
	{
		// Just do the lexicographic sort and assign the call order to their ordered index value.
		UniqueNames.Sort();
		for (UNiagaraNodeInput* InputNode : InputNodes)
		{
			if (InputNode->Usage == ENiagaraInputNodeUsage::Parameter)
			{
				int32 FoundIndex = UniqueNames.Find(InputNode->Input.GetName());
				check(FoundIndex != -1);
				InputNode->CallSortPriority = FoundIndex;
			}
		}
	}

	// If this is from a prior version, enforce a valid Change Id!
	if (ChangeId.IsValid() == false)
	{
		MarkGraphRequiresSynchronization();
	}

	// Assume that all externally referenced assets have changed, so update to match. They will return true if they have changed.
	TArray<UNiagaraNode*> NiagaraNodes;
	GetNodesOfClass<UNiagaraNode>(NiagaraNodes);
	bool bAnyExternalChanges = false;
	for (UNiagaraNode* NiagaraNode : NiagaraNodes)
	{
		UObject* ReferencedAsset = NiagaraNode->GetReferencedAsset();
		if (ReferencedAsset != nullptr)
		{
			ReferencedAsset->ConditionalPostLoad();
			NiagaraNode->ConditionalPostLoad();
			bAnyExternalChanges |= (NiagaraNode->RefreshFromExternalChanges());
		}
		else
		{
			NiagaraNode->ConditionalPostLoad();
		}
	}

	if (bAnyExternalChanges)
	{
		MarkGraphRequiresSynchronization();
		NotifyGraphNeedsRecompile();
	}

	if (GIsEditor)
	{
		SetFlags(RF_Transactional);
	}

}

void UNiagaraGraph::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	NotifyGraphChanged();
}

class UNiagaraScriptSource* UNiagaraGraph::GetSource() const
{
	return CastChecked<UNiagaraScriptSource>(GetOuter());
}


UEdGraphPin* UNiagaraGraph::FindParameterMapDefaultValuePin(const FString& VariableName)
{
	TArray<UEdGraphPin*> MatchingDefaultPins;
	TArray<UNiagaraNodeParameterMapGet*> GetNodes;
	GetNodesOfClass<UNiagaraNodeParameterMapGet>(GetNodes);

	for (UNiagaraNodeParameterMapGet* GetNode : GetNodes)
	{
		TArray<UEdGraphPin*> OutputPins;
		GetNode->GetOutputPins(OutputPins);
		for (UEdGraphPin* OutputPin : OutputPins)
		{
			if (VariableName == OutputPin->PinName)
			{
				UEdGraphPin* Pin = GetNode->GetDefaultPin(OutputPin);
				if (Pin)
				{
					MatchingDefaultPins.Add(Pin);
				}
			}
		}
	}

	if (MatchingDefaultPins.Num() > 0)
	{
		return MatchingDefaultPins[0];
	}

	return nullptr;
}

void UNiagaraGraph::FindOutputNodes(TArray<UNiagaraNodeOutput*>& OutputNodes) const
{
	for (UEdGraphNode* Node : Nodes)
	{
		if (UNiagaraNodeOutput* OutNode = Cast<UNiagaraNodeOutput>(Node))
		{
			OutputNodes.Add(OutNode);
		}
	}
}


void UNiagaraGraph::FindOutputNodes(ENiagaraScriptUsage TargetUsageType, TArray<UNiagaraNodeOutput*>& OutputNodes) const
{
	TArray<UNiagaraNodeOutput*> NodesFound;
	for (UEdGraphNode* Node : Nodes)
	{
		UNiagaraNodeOutput* OutNode = Cast<UNiagaraNodeOutput>(Node);
		if (OutNode && OutNode->GetUsage() == TargetUsageType)
		{
			NodesFound.Add(OutNode);
		}
	}

	NodesFound.Sort([](const UNiagaraNodeOutput& LHS, const UNiagaraNodeOutput& RHS) { return LHS.GetUsageIndex() > RHS.GetUsageIndex(); });
	OutputNodes = NodesFound;
}

UNiagaraNodeOutput* UNiagaraGraph::FindOutputNode(ENiagaraScriptUsage TargetUsageType, int32 TargetOccurence) const
{
	for (UEdGraphNode* Node : Nodes)
	{
		if (UNiagaraNodeOutput* OutNode = Cast<UNiagaraNodeOutput>(Node))
		{
			if (OutNode->GetUsage() == TargetUsageType)
			{
				if (OutNode->ScriptTypeIndex == TargetOccurence)
				{
					return OutNode;
				}
			}
		}
	}
	return nullptr;
}

void BuildTraversalHelper(TArray<class UNiagaraNode*>& OutNodesTraversed, UNiagaraNode* CurrentNode)
{
	if (CurrentNode == nullptr)
	{
		return;
	}

	TArray<UEdGraphPin*> Pins = CurrentNode->GetAllPins();
	for (int32 i = 0; i < Pins.Num(); i++)
	{
		if (Pins[i]->Direction == EEdGraphPinDirection::EGPD_Input && Pins[i]->LinkedTo.Num() == 1)
		{
			BuildTraversalHelper(OutNodesTraversed, Cast<UNiagaraNode>(Pins[i]->LinkedTo[0]->GetOwningNode()));
		}
	}

	OutNodesTraversed.Add(CurrentNode);
}

void UNiagaraGraph::BuildTraversal(TArray<class UNiagaraNode*>& OutNodesTraversed, ENiagaraScriptUsage TargetUsage, int32 TargetOccurence) const
{
	UNiagaraNodeOutput* Output = FindOutputNode(TargetUsage, TargetOccurence);
	if (Output)
	{
		BuildTraversalHelper(OutNodesTraversed, Output);
	}
}

void UNiagaraGraph::BuildTraversal(TArray<class UNiagaraNode*>& OutNodesTraversed, UNiagaraNode* FinalNode) const
{
	if (FinalNode)
	{
		BuildTraversalHelper(OutNodesTraversed, FinalNode);
	}
}


void UNiagaraGraph::FindInputNodes(TArray<UNiagaraNodeInput*>& OutInputNodes, UNiagaraGraph::FFindInputNodeOptions Options) const
{
	TArray<UNiagaraNodeInput*> InputNodes;

	if (!Options.bFilterByScriptUsage)
	{
		for (UEdGraphNode* Node : Nodes)
		{
			UNiagaraNodeInput* NiagaraInputNode = Cast<UNiagaraNodeInput>(Node);
			if (NiagaraInputNode != nullptr &&
				((NiagaraInputNode->Usage == ENiagaraInputNodeUsage::Parameter && Options.bIncludeParameters) ||
					(NiagaraInputNode->Usage == ENiagaraInputNodeUsage::Attribute && Options.bIncludeAttributes) ||
					(NiagaraInputNode->Usage == ENiagaraInputNodeUsage::SystemConstant && Options.bIncludeSystemConstants)))
			{
				InputNodes.Add(NiagaraInputNode);
			}
		}
	}
	else
	{
		TArray<class UNiagaraNode*> Traversal;
		BuildTraversal(Traversal, Options.TargetScriptUsage, Options.TargetOccurence);
		for (UNiagaraNode* Node : Traversal)
		{
			UNiagaraNodeInput* NiagaraInputNode = Cast<UNiagaraNodeInput>(Node);
			if (NiagaraInputNode != nullptr &&
				((NiagaraInputNode->Usage == ENiagaraInputNodeUsage::Parameter && Options.bIncludeParameters) ||
					(NiagaraInputNode->Usage == ENiagaraInputNodeUsage::Attribute && Options.bIncludeAttributes) ||
					(NiagaraInputNode->Usage == ENiagaraInputNodeUsage::SystemConstant && Options.bIncludeSystemConstants)))
			{
				InputNodes.Add(NiagaraInputNode);
			}
		}
	}

	if (Options.bFilterDuplicates)
	{
		for (UNiagaraNodeInput* InputNode : InputNodes)
		{
			auto NodeMatches = [=](UNiagaraNodeInput* UniqueInputNode)
			{
				if (InputNode->Usage == ENiagaraInputNodeUsage::Parameter)
				{
					return UniqueInputNode->Input.IsEquivalent(InputNode->Input, false);
				}
				else
				{
					return UniqueInputNode->Input.IsEquivalent(InputNode->Input);
				}
			};

			if (OutInputNodes.ContainsByPredicate(NodeMatches) == false)
			{
				OutInputNodes.Add(InputNode);
			}
		}
	}
	else
	{
		OutInputNodes.Append(InputNodes);
	}

	if (Options.bSort)
	{
		UNiagaraNodeInput::SortNodes(OutInputNodes);
	}
}

void UNiagaraGraph::GetParameters(TArray<FNiagaraVariable>& Inputs, TArray<FNiagaraVariable>& Outputs)const
{
	Inputs.Empty();
	Outputs.Empty();

	TArray<UNiagaraNodeInput*> InputsNodes;
	FFindInputNodeOptions Options;
	Options.bSort = true;
	FindInputNodes(InputsNodes, Options);
	for (UNiagaraNodeInput* Input : InputsNodes)
	{
		Inputs.Add(Input->Input);
	}

	TArray<UNiagaraNodeOutput*> OutputNodes;
	FindOutputNodes(OutputNodes);
	for (UNiagaraNodeOutput* OutputNode : OutputNodes)
	{
		for (FNiagaraVariable& Var : OutputNode->Outputs)
		{
			Outputs.AddUnique(Var);
		}
	}

	//Do we need to sort outputs?
	//Should leave them as they're defined in the output node?
// 	auto SortVars = [](const FNiagaraVariable& A, const FNiagaraVariable& B)
// 	{
// 		//Case insensitive lexicographical comparisons of names for sorting.
// 		return A.GetName().ToString() < B.GetName().ToString();
// 	};
// 	Outputs.Sort(SortVars);
}

void UNiagaraGraph::FindReadDataSetNodes(TArray<class UNiagaraNodeReadDataSet*>& OutReadNodes) const
{
	for (UEdGraphNode* Node : Nodes)
	{
		if (UNiagaraNodeReadDataSet* InNode = Cast<UNiagaraNodeReadDataSet>(Node))
		{
			OutReadNodes.Add(InNode);
		}
	}
}

void UNiagaraGraph::FindWriteDataSetNodes(TArray<class UNiagaraNodeWriteDataSet*>& OutWriteNodes) const
{
	for (UEdGraphNode* Node : Nodes)
	{
		if (UNiagaraNodeWriteDataSet* InNode = Cast<UNiagaraNodeWriteDataSet>(Node))
		{
			OutWriteNodes.Add(InNode);
		}
	}
}

int32 UNiagaraGraph::GetOutputNodeVariableIndex(const FNiagaraVariable& Variable)const
{
	TArray<FNiagaraVariable> Variables;
	GetOutputNodeVariables(Variables);
	return Variables.Find(Variable);
}

void UNiagaraGraph::GetOutputNodeVariables(TArray< FNiagaraVariable >& OutVariables)const
{
	TArray<UNiagaraNodeOutput*> OutputNodes;
	FindOutputNodes(OutputNodes);
	for (UNiagaraNodeOutput* OutputNode : OutputNodes)
	{
		for (FNiagaraVariable& Var : OutputNode->Outputs)
		{
			OutVariables.AddUnique(Var);
		}
	}
}

void UNiagaraGraph::GetOutputNodeVariables(ENiagaraScriptUsage InScriptUsage, TArray< FNiagaraVariable >& OutVariables)const
{
	TArray<UNiagaraNodeOutput*> OutputNodes;
	FindOutputNodes(InScriptUsage, OutputNodes);
	for (UNiagaraNodeOutput* OutputNode : OutputNodes)
	{
		for (FNiagaraVariable& Var : OutputNode->Outputs)
		{
			OutVariables.AddUnique(Var);
		}
	}
}

bool UNiagaraGraph::HasNumericParameters()const
{
	TArray<FNiagaraVariable> Inputs;
	TArray<FNiagaraVariable> Outputs;
	
	GetParameters(Inputs, Outputs);
	
	for (FNiagaraVariable& Var : Inputs)
	{
		if (Var.GetType() == FNiagaraTypeDefinition::GetGenericNumericDef())
		{
			return true;
		}
	}
	for (FNiagaraVariable& Var : Outputs)
	{
		if (Var.GetType() == FNiagaraTypeDefinition::GetGenericNumericDef())
		{
			return true;
		}
	}

	return false;
}

void UNiagaraGraph::NotifyGraphNeedsRecompile()
{
	FEdGraphEditAction Action;
	Action.Action = (EEdGraphActionType)GRAPHACTION_GenericNeedsRecompile;
	NotifyGraphChanged(Action);
}

void UNiagaraGraph::SubsumeExternalDependencies(TMap<const UObject*, UObject*>& ExistingConversions)
{
	TArray<UNiagaraNode*> NiagaraNodes;
	GetNodesOfClass<UNiagaraNode>(NiagaraNodes);
	for (UNiagaraNode* NiagaraNode : NiagaraNodes)
	{
		NiagaraNode->SubsumeExternalDependencies(ExistingConversions);
	}
}

void UNiagaraGraph::GetAllReferencedGraphs(TArray<const UNiagaraGraph*>& Graphs) const
{
	Graphs.AddUnique(this);
	for (UEdGraphNode* Node : Nodes)
	{
		if (UNiagaraNode* InNode = Cast<UNiagaraNode>(Node))
		{
			UObject* AssetRef = InNode->GetReferencedAsset();
			if (AssetRef != nullptr && AssetRef->IsA(UNiagaraScript::StaticClass()))
			{
				if (UNiagaraScript* FunctionScript = Cast<UNiagaraScript>(AssetRef))
				{
					if (FunctionScript->GetSource())
					{
						UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(FunctionScript->GetSource());
						if (Source != nullptr)
						{
							UNiagaraGraph* FunctionGraph = CastChecked<UNiagaraGraph>(Source->NodeGraph);
							if (FunctionGraph != nullptr)
							{
								if (!Graphs.Contains(FunctionGraph))
								{
									FunctionGraph->GetAllReferencedGraphs(Graphs);
								}
							}
						}
					}
				}
				else if (UNiagaraGraph* FunctionGraph = Cast<UNiagaraGraph>(AssetRef))
				{
					if (!Graphs.Contains(FunctionGraph))
					{
						FunctionGraph->GetAllReferencedGraphs(Graphs);
					}
				}
			}
		}
	}
}

/** Determine if another item has been synchronized with this graph.*/
bool UNiagaraGraph::IsOtherSynchronized(const FGuid& InChangeId) const
{
	if (ChangeId.IsValid() && ChangeId == InChangeId)
	{
		return true;
	}
	return false;
}

/** Identify that this graph has undergone changes that will require synchronization with a compiled script.*/
void UNiagaraGraph::MarkGraphRequiresSynchronization()
{
	Modify();
	ChangeId = FGuid::NewGuid();
	UE_LOG(LogNiagaraEditor, Log, TEXT("MarkGraphRequiresSynchronization %s"), *ChangeId.ToString());
}
