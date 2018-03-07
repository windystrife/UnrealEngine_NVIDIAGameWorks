// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraTypes.h"
#include "NiagaraCommon.h"
#include "NiagaraParameterHandle.h"

class UEdGraph;
class UEdGraphPin;
class UNiagaraGraph;
class UNiagaraNode;
class UNiagaraNodeInput;
class UNiagaraNodeOutput;
class UNiagaraNodeFunctionCall;
class UNiagaraStackEditorData;
class FNiagaraSystemViewModel;
class FNiagaraEmitterViewModel;

namespace FNiagaraStackGraphUtilities
{
	void RelayoutGraph(UEdGraph& Graph);

	void GetWrittenVariablesForGraph(UEdGraph& Graph, TArray<FNiagaraVariable>& OutWrittenVariables);

	void ConnectPinToInputNode(UEdGraphPin& Pin, UNiagaraNodeInput& InputNode);

	UEdGraphPin* GetParameterMapInputPin(UNiagaraNode& Node);

	UEdGraphPin* GetParameterMapOutputPin(UNiagaraNode& Node);

	void GetOrderedModuleNodes(UNiagaraNodeOutput& OutputNode, TArray<UNiagaraNodeFunctionCall*>& ModuleNodes);

	UNiagaraNodeFunctionCall* GetPreviousModuleNode(UNiagaraNodeFunctionCall& CurrentNode);

	UNiagaraNodeFunctionCall* GetNextModuleNode(UNiagaraNodeFunctionCall& CurrentNode);

	UNiagaraNodeOutput* GetEmitterOutputNodeForStackNode(UNiagaraNode& StackNode);

	UNiagaraNodeInput* GetEmitterInputNodeForStackNode(UNiagaraNode& StackNode);

	struct FStackNodeGroup
	{
		TArray<UNiagaraNode*> StartNodes;
		UNiagaraNode* EndNode;
	};

	void GetStackNodeGroups(UNiagaraNode& StackNode, TArray<FStackNodeGroup>& OutStackNodeGroups);

	void DisconnectStackNodeGroup(const FStackNodeGroup& DisconnectGroup, const FStackNodeGroup& PreviousGroup, const FStackNodeGroup& NextGroup);

	void ConnectStackNodeGroup(const FStackNodeGroup& ConnectGroup, const FStackNodeGroup& NewPreviousGroup, const FStackNodeGroup& NewNextGroup);

	void InitializeDataInterfaceInputs(TSharedRef<FNiagaraSystemViewModel> SystemViewModel, TSharedRef<FNiagaraEmitterViewModel> EmitterViewModel, UNiagaraStackEditorData& StackEditorData, UNiagaraNodeFunctionCall& ModuleNode, UNiagaraNodeFunctionCall& InputFunctionCallNode);

	FString GenerateStackFunctionInputEditorDataKey(UNiagaraNodeFunctionCall& FunctionCallNode, FNiagaraParameterHandle InputParameterHandle);

	FString GenerateStackModuleEditorDataKey(UNiagaraNodeFunctionCall& ModuleNode);

	enum class ENiagaraGetStackFunctionInputPinsOptions
	{
		AllInputs,
		ModuleInputsOnly
	};

	void GetStackFunctionInputPins(UNiagaraNodeFunctionCall& FunctionCallNode, TArray<const UEdGraphPin*>& OutInputPins, ENiagaraGetStackFunctionInputPinsOptions Options = ENiagaraGetStackFunctionInputPinsOptions::AllInputs);

	bool ValidateGraphForOutput(UNiagaraGraph& NiagaraGraph, ENiagaraScriptUsage ScriptUsage, int32 ScriptOccurrence, FText& ErrorMessage);

	UNiagaraNodeOutput* ResetGraphForOutput(UNiagaraGraph& NiagaraGraph, ENiagaraScriptUsage ScriptUsage, int32 ScriptOccurrence);
}