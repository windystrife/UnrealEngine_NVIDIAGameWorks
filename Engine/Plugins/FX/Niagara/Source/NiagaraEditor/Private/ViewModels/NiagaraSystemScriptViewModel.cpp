// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraSystemScriptViewModel.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraTypes.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraScriptInputCollectionViewModel.h"
#include "NiagaraNodeEmitter.h"
#include "NiagaraNodeInput.h"
#include "NiagaraDataInterface.h"
#include "NiagaraScriptSourceBase.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraNodeOUtput.h"
#include "GraphEditAction.h"
#include "Stack/NiagaraStackEntry.h"
#include "NiagaraStackGraphUtilities.h"

FNiagaraSystemScriptViewModel::FNiagaraSystemScriptViewModel(UNiagaraSystem& InSystem)
	: FNiagaraScriptViewModel(InSystem.GetSystemSpawnScript(), NSLOCTEXT("SystemScriptViewModel", "GraphName", "System"), ENiagaraParameterEditMode::EditAll)
	, System(InSystem)
{
	if (GetGraphViewModel()->GetGraph())
	{
		OnGraphChangedHandle = GetGraphViewModel()->GetGraph()->AddOnGraphChangedHandler(
			FOnGraphChanged::FDelegate::CreateRaw(this, &FNiagaraSystemScriptViewModel::OnGraphChanged));
		
		GetGraphViewModel()->SetErrorTextToolTip("");
	}
}

FNiagaraSystemScriptViewModel::~FNiagaraSystemScriptViewModel()
{
	if (GetGraphViewModel()->GetGraph())
	{
		GetGraphViewModel()->GetGraph()->RemoveOnGraphChangedHandler(OnGraphChangedHandle);
	}
}


float EmitterNodeVerticalOffset = 150.0f;

FVector2D CalculateNewEmitterNodePlacementPosition(UNiagaraGraph* Graph, UNiagaraNodeEmitter* NewEmitterNode)
{
	FVector2D PlacementLocation(0.0f, 0.0f);
	TArray<UNiagaraNodeEmitter*> EmitterNodes;
	Graph->GetNodesOfClass(EmitterNodes);
	if (EmitterNodes.Num() > 1)
	{
		// If there are Emitter nodes, try to put it under the lowest one.
		UNiagaraNodeEmitter* LowestNode = nullptr;
		for (UNiagaraNodeEmitter* EmitterNode : EmitterNodes)
		{
			if (EmitterNode != NewEmitterNode && (LowestNode == nullptr || EmitterNode->NodePosY > LowestNode->NodePosY))
			{
				LowestNode = EmitterNode;
			}
		}
		check(LowestNode);
		PlacementLocation = FVector2D(
			LowestNode->NodePosX,
			LowestNode->NodePosY + EmitterNodeVerticalOffset);
	}
	return PlacementLocation;
}

void FNiagaraSystemScriptViewModel::RebuildEmitterNodes()
{
	UNiagaraGraph* SystemGraph = GetGraphViewModel()->GetGraph();
	if (SystemGraph == nullptr)
	{
		return;
	}
	
	TArray<UNiagaraNodeEmitter*> CurrentEmitterNodes;
	SystemGraph->GetNodesOfClass<UNiagaraNodeEmitter>(CurrentEmitterNodes);

	const UEdGraphSchema_Niagara* Schema = Cast<UEdGraphSchema_Niagara>(SystemGraph->GetSchema());

	// Remove the old emitter nodes since they will be rebuilt below.
	for (UNiagaraNodeEmitter* CurrentEmitterNode : CurrentEmitterNodes)
	{
		CurrentEmitterNode->Modify();
		UEdGraphPin* InPin = CurrentEmitterNode->GetInputPin(0);
		UEdGraphPin* OutPin = CurrentEmitterNode->GetOutputPin(0);
		if (InPin != nullptr && OutPin != nullptr)
		{
			if (0 != InPin->LinkedTo.Num() && 0 != OutPin->LinkedTo.Num())
			{
				InPin->LinkedTo[0]->MakeLinkTo(OutPin->LinkedTo[0]);
			}
		}
		CurrentEmitterNode->DestroyNode();
	}

	// Add output nodes if they don't exist.
	TArray<UNiagaraNodeInput*> TempInputNodes;
	TArray<UNiagaraNodeInput*> InputNodes;
	TArray<UNiagaraNodeOutput*> OutputNodes;
	OutputNodes.SetNum(2);
	OutputNodes[0] = SystemGraph->FindOutputNode(ENiagaraScriptUsage::SystemSpawnScript);
	OutputNodes[1] = SystemGraph->FindOutputNode(ENiagaraScriptUsage::SystemUpdateScript);

	// Add input nodes if they don't exist
	UNiagaraGraph::FFindInputNodeOptions Options;
	Options.bFilterDuplicates = false;
	Options.bIncludeParameters = true;
	SystemGraph->FindInputNodes(TempInputNodes);
	for (int32 i = 0; i < TempInputNodes.Num(); i++)
	{
		if (Schema->PinToTypeDefinition(TempInputNodes[i]->GetOutputPin(0)) == FNiagaraTypeDefinition::GetParameterMapDef())
		{
			InputNodes.Add(TempInputNodes[i]);
		}
	}

	// Create a default id variable for the input nodes.
	FNiagaraVariable SharedInputVar(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("InputMap"));	
	InputNodes.SetNum(2);

	// Now create the nodes if they are needed, synchronize if already created.
	for (int32 i = 0; i < 2; i++)
	{
		if (OutputNodes[i] == nullptr)
		{
			FGraphNodeCreator<UNiagaraNodeOutput> OutputNodeCreator(*SystemGraph);
			OutputNodes[i] = OutputNodeCreator.CreateNode();
			OutputNodes[i]->SetUsage((ENiagaraScriptUsage)(i + (int32)ENiagaraScriptUsage::SystemSpawnScript));
			
			OutputNodes[i]->Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("Out")));
			OutputNodes[i]->NodePosX = 0;
			OutputNodes[i]->NodePosY = 0 + i*25;

			OutputNodeCreator.Finalize();
		}
		if (InputNodes[i] == nullptr)
		{
			FGraphNodeCreator<UNiagaraNodeInput> InputNodeCreator(*SystemGraph);
			InputNodes[i] =  InputNodeCreator.CreateNode();
			InputNodes[i]->Input = SharedInputVar;
			InputNodes[i]->Usage = ENiagaraInputNodeUsage::Parameter;
			InputNodes[i]->NodePosX = -50;
			InputNodes[i]->NodePosY = 0 + i * 25;

			InputNodeCreator.Finalize();

			InputNodes[i]->GetOutputPin(0)->MakeLinkTo(OutputNodes[i]->GetInputPin(0));
		}
	}

	// Add new nodes.
	UNiagaraNode* TargetNodes[2];
	TargetNodes[0] = OutputNodes[0];
	TargetNodes[1] = OutputNodes[1];

	for (const FNiagaraEmitterHandle& EmitterHandle : System.GetEmitterHandles())
	{
		for (int32 i = 0; i < 2; i++)
		{
			FGraphNodeCreator<UNiagaraNodeEmitter> EmitterNodeCreator(*SystemGraph);
			UNiagaraNodeEmitter* EmitterNode = EmitterNodeCreator.CreateNode();
			EmitterNode->SetOwnerSystem(&System);
			EmitterNode->SetEmitterHandleId(EmitterHandle.GetId());
			EmitterNode->SetUsage((ENiagaraScriptUsage)(i + (int32)ENiagaraScriptUsage::EmitterSpawnScript));

			FVector2D NewLocation = CalculateNewEmitterNodePlacementPosition(SystemGraph, EmitterNode);
			EmitterNode->NodePosX = NewLocation.X;
			EmitterNode->NodePosY = NewLocation.Y;
			EmitterNode->AllocateDefaultPins();
			EmitterNodeCreator.Finalize();

			TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> StackNodeGroups;
			FNiagaraStackGraphUtilities::GetStackNodeGroups(*OutputNodes[i], StackNodeGroups);

			FNiagaraStackGraphUtilities::FStackNodeGroup EmitterGroup;
			EmitterGroup.StartNodes.Add(EmitterNode);
			EmitterGroup.EndNode = EmitterNode;

			FNiagaraStackGraphUtilities::FStackNodeGroup& OutputGroup = StackNodeGroups[StackNodeGroups.Num() - 1];
			FNiagaraStackGraphUtilities::FStackNodeGroup& OutputGroupPrevious = StackNodeGroups[StackNodeGroups.Num() - 2];
			FNiagaraStackGraphUtilities::ConnectStackNodeGroup(EmitterGroup, OutputGroupPrevious, OutputGroup);
		}
	}
	
	FNiagaraStackGraphUtilities::RelayoutGraph(*SystemGraph);
}

FNiagaraSystemScriptViewModel::FOnSystemCompiled& FNiagaraSystemScriptViewModel::OnSystemCompiled()
{
	return OnSystemCompiledDelegate;
}

void FNiagaraSystemScriptViewModel::CompileSystem()
{

	TArray<ENiagaraScriptCompileStatus> CompileSystemStatuses;
	TArray<FString> CompileSystemErrors;
	TArray<FString> CompileSystemPaths;
	TArray<UNiagaraScript*> CompileSystemScripts;
	System.CompileScripts(CompileSystemStatuses, CompileSystemErrors, CompileSystemPaths, CompileSystemScripts);

	ENiagaraScriptCompileStatus AggregateStatus = ENiagaraScriptCompileStatus::NCS_UpToDate;
	FString AggregateErrors;

	for (int32 i = 0; i < CompileSystemStatuses.Num(); i++)
	{
		AggregateStatus = FNiagaraEditorUtilities::UnionCompileStatus(AggregateStatus, CompileSystemStatuses[i]);
		AggregateErrors += CompileSystemPaths[i] + TEXT(" ") + FNiagaraEditorUtilities::StatusToText(CompileSystemStatuses[i]).ToString() + TEXT("\n");
		AggregateErrors += CompileSystemErrors[i] + TEXT("\n");
	}

	UpdateCompileStatus(AggregateStatus, AggregateErrors, CompileSystemStatuses, CompileSystemErrors, CompileSystemPaths, CompileSystemScripts);

	if (OnSystemCompiledDelegate.IsBound())
	{
		OnSystemCompiledDelegate.Broadcast();
	}
}

void FNiagaraSystemScriptViewModel::OnGraphChanged(const struct FEdGraphEditAction& InAction)
{
	if (InAction.Action == GRAPHACTION_SelectNode)
	{
		return;
	}
}