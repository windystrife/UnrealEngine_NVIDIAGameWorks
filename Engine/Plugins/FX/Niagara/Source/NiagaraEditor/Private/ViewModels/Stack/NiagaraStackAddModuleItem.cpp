// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackAddModuleItem.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScript.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeAssignment.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "NiagaraStackEditorData.h"
#include "NiagaraStackGraphUtilities.h"

#include "ScopedTransaction.h"
#include "EdGraph/EdGraph.h"

#define LOCTEXT_NAMESPACE "NiagaraStackViewModel"

void UNiagaraStackAddModuleItem::Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraStackEditorData& InStackEditorData)
{
	Super::Initialize(InSystemViewModel, InEmitterViewModel);
	StackEditorData = &InStackEditorData;
}

FText UNiagaraStackAddModuleItem::GetDisplayName() const
{
	return FText();
}

void UNiagaraStackAddModuleItem::GetAvailableParameters(TArray<FNiagaraVariable>& OutAvailableParameterVariables) const
{
}

void UNiagaraStackAddModuleItem::GetNewParameterAvailableTypes(TArray<FNiagaraTypeDefinition>& OutAvailableTypes) const
{
}

TOptional<FString> UNiagaraStackAddModuleItem::GetNewParameterNamespace() const
{
	return TOptional<FString>();
}

void UNiagaraStackAddModuleItem::SetOnItemAdded(FOnItemAdded InOnItemAdded)
{
	ItemAddedDelegate = InOnItemAdded;
}

void ConnectModuleNode(UNiagaraNode& ModuleNode, UNiagaraNode& OutputNode)
{
	TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> StackNodeGroups;
	FNiagaraStackGraphUtilities::GetStackNodeGroups(OutputNode, StackNodeGroups);

	FNiagaraStackGraphUtilities::FStackNodeGroup ModuleGroup;
	ModuleGroup.StartNodes.Add(&ModuleNode);
	ModuleGroup.EndNode = &ModuleNode;

	FNiagaraStackGraphUtilities::FStackNodeGroup& OutputGroup = StackNodeGroups[StackNodeGroups.Num() - 1];
	FNiagaraStackGraphUtilities::FStackNodeGroup& OutputGroupPrevious = StackNodeGroups[StackNodeGroups.Num() - 2];
	FNiagaraStackGraphUtilities::ConnectStackNodeGroup(ModuleGroup, OutputGroupPrevious, OutputGroup);
}

void UNiagaraStackAddModuleItem::AddScriptModule(FAssetData ModuleScriptAsset)
{
	FScopedTransaction ScopedTransaction(GetInsertTransactionText());

	UNiagaraNodeOutput* OutputNode = GetOrCreateOutputNode();
	if (OutputNode != nullptr)
	{
		UEdGraph* Graph = OutputNode->GetGraph();
		Graph->Modify();

		FGraphNodeCreator<UNiagaraNodeFunctionCall> ModuleNodeCreator(*Graph);
		UNiagaraNodeFunctionCall* NewModuleNode = ModuleNodeCreator.CreateNode();
		NewModuleNode->FunctionScriptAssetObjectPath = ModuleScriptAsset.ObjectPath;
		ModuleNodeCreator.Finalize();

		ConnectModuleNode(*NewModuleNode, *OutputNode);
		FNiagaraStackGraphUtilities::InitializeDataInterfaceInputs(GetSystemViewModel(), GetEmitterViewModel(), *StackEditorData, *NewModuleNode, *NewModuleNode);
		FNiagaraStackGraphUtilities::RelayoutGraph(*OutputNode->GetGraph());

		ItemAddedDelegate.ExecuteIfBound();
	}
}

void UNiagaraStackAddModuleItem::AddParameterModule(FNiagaraVariable ParameterVariable, bool bRenamePending)
{
	FScopedTransaction ScopedTransaction(GetInsertTransactionText());

	UNiagaraNodeOutput* OutputNode = GetOrCreateOutputNode();
	if (OutputNode != nullptr)
	{
		UEdGraph* Graph = OutputNode->GetGraph();
		Graph->Modify();

		FGraphNodeCreator<UNiagaraNodeAssignment> AssignmentNodeCreator(*Graph);
		UNiagaraNodeAssignment* NewAssignmentNode = AssignmentNodeCreator.CreateNode();
		NewAssignmentNode->AssignmentTarget = ParameterVariable;
		AssignmentNodeCreator.Finalize();

		ConnectModuleNode(*NewAssignmentNode, *OutputNode);
		FNiagaraStackGraphUtilities::InitializeDataInterfaceInputs(GetSystemViewModel(), GetEmitterViewModel(), *StackEditorData, *NewAssignmentNode, *NewAssignmentNode);
		FNiagaraStackGraphUtilities::RelayoutGraph(*OutputNode->GetGraph());

		TArray<const UEdGraphPin*> InputPins;
		FNiagaraStackGraphUtilities::GetStackFunctionInputPins(*NewAssignmentNode, InputPins);
		if (InputPins.Num() == 1)
		{
			FString FunctionInputEditorDataKey = FNiagaraStackGraphUtilities::GenerateStackFunctionInputEditorDataKey(*NewAssignmentNode, InputPins[0]->PinName);
			StackEditorData->SetModuleInputIsPinned(FunctionInputEditorDataKey, true);
			StackEditorData->SetStackEntryIsExpanded(FNiagaraStackGraphUtilities::GenerateStackModuleEditorDataKey(*NewAssignmentNode), false);
			if (bRenamePending)
			{
				StackEditorData->SetModuleInputIsRenamePending(FunctionInputEditorDataKey, true);
			}
		}

		ItemAddedDelegate.ExecuteIfBound();
	}
}

FText UNiagaraStackAddModuleItem::GetInsertTransactionText() const
{
	return LOCTEXT("InsertNewModule", "Insert new module");
}

#undef LOCTEXT_NAMESPACE