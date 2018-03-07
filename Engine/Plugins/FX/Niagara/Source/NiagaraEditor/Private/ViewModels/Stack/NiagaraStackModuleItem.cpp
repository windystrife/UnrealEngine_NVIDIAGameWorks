// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackModuleItem.h"
#include "NiagaraEmitterViewModel.h"
#include "NiagaraStackFunctionInputCollection.h"
#include "NiagaraStackFunctionInput.h"
#include "NiagaraStackModuleItemOutputCollection.h"
#include "NiagaraStackItemExpander.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraEmitterEditorData.h"
#include "NiagaraStackEditorData.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraGraph.h"
#include "NiagaraStackGraphUtilities.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "NiagaraStackViewModel"

UNiagaraStackModuleItem::UNiagaraStackModuleItem()
	: FunctionCallNode(nullptr)
	, PinnedInputCollection(nullptr)
	, UnpinnedInputCollection(nullptr)
	, ModuleExpander(nullptr)
{
}

const UNiagaraNodeFunctionCall& UNiagaraStackModuleItem::GetModuleNode() const
{
	return *FunctionCallNode;
}

void UNiagaraStackModuleItem::Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraStackEditorData& InStackEditorData, UNiagaraNodeFunctionCall& InFunctionCallNode)
{
	checkf(FunctionCallNode == nullptr, TEXT("Can not set the node more than once."));
	Super::Initialize(InSystemViewModel, InEmitterViewModel, InStackEditorData);
	FunctionCallNode = &InFunctionCallNode;
}

FText UNiagaraStackModuleItem::GetDisplayName() const
{
	if (FunctionCallNode != nullptr)
	{
		return FunctionCallNode->GetNodeTitle(ENodeTitleType::ListView);
	}
	else
	{
		return FText::FromName(NAME_None);
	}
}


FText UNiagaraStackModuleItem::GetTooltipText() const
{
	if (FunctionCallNode != nullptr)
	{
		return FunctionCallNode->GetTooltipText();
	}
	else
	{
		return FText();
	}
}

void UNiagaraStackModuleItem::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren)
{
	if (PinnedInputCollection == nullptr)
	{
		UNiagaraStackFunctionInputCollection::FDisplayOptions DisplayOptions;
		DisplayOptions.DisplayName = LOCTEXT("PinnedInputsLabel", "Pinned Inputs");
		DisplayOptions.ChildItemIndentLevel = 0;
		DisplayOptions.bShouldShowInStack = false;
		DisplayOptions.ChildFilter = UNiagaraStackFunctionInputCollection::FOnFilterChildren::CreateLambda(
			[](UNiagaraStackFunctionInput* FunctionInput) { return FunctionInput->GetIsPinned(); });

		TArray<FString> InputParameterHandlePath;
		PinnedInputCollection = NewObject<UNiagaraStackFunctionInputCollection>(this);
		PinnedInputCollection->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetStackEditorData(), *FunctionCallNode, *FunctionCallNode, DisplayOptions);
		PinnedInputCollection->OnInputPinnedChanged().AddUObject(this, &UNiagaraStackModuleItem::InputPinnedChanged);
	}

	if (UnpinnedInputCollection == nullptr)
	{
		UNiagaraStackFunctionInputCollection::FDisplayOptions DisplayOptions;
		DisplayOptions.DisplayName = LOCTEXT("InputsLabel", "Inputs");
		DisplayOptions.ChildItemIndentLevel = 1;
		DisplayOptions.bShouldShowInStack = true;
		DisplayOptions.ChildFilter = UNiagaraStackFunctionInputCollection::FOnFilterChildren::CreateLambda(
			[](UNiagaraStackFunctionInput* FunctionInput) { return FunctionInput->GetIsPinned() == false; });

		TArray<FString> InputParameterHandlePath;
		UnpinnedInputCollection = NewObject<UNiagaraStackFunctionInputCollection>(this);
		UnpinnedInputCollection->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetStackEditorData(), *FunctionCallNode, *FunctionCallNode, DisplayOptions);
		UnpinnedInputCollection->OnInputPinnedChanged().AddUObject(this, &UNiagaraStackModuleItem::InputPinnedChanged);
	}

	if (OutputCollection == nullptr)
	{
		OutputCollection = NewObject<UNiagaraStackModuleItemOutputCollection>(this);
		OutputCollection->Initialize(GetSystemViewModel(), GetEmitterViewModel(), *FunctionCallNode);
		OutputCollection->SetDisplayName(LOCTEXT("OutputsLabel", "Outputs"));
	}

	FString ModuleEditorDataKey = FNiagaraStackGraphUtilities::GenerateStackModuleEditorDataKey(*FunctionCallNode);
	if (ModuleExpander == nullptr)
	{
		ModuleExpander = NewObject<UNiagaraStackItemExpander>(this);
		ModuleExpander->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetStackEditorData(), ModuleEditorDataKey, true);
		ModuleExpander->SetOnExpnadedChanged(UNiagaraStackItemExpander::FOnExpandedChanged::CreateUObject(this, &UNiagaraStackModuleItem::ModuleExpandedChanged));
	}

	NewChildren.Add(PinnedInputCollection);
	if (GetStackEditorData().GetStackEntryIsExpanded(ModuleEditorDataKey, true))
	{
		NewChildren.Add(UnpinnedInputCollection);
		NewChildren.Add(OutputCollection);
	}
	NewChildren.Add(ModuleExpander);
}

void UNiagaraStackModuleItem::InputPinnedChanged()
{
	PinnedInputCollection->RefreshChildren();
	UnpinnedInputCollection->RefreshChildren();
}

void UNiagaraStackModuleItem::ModuleExpandedChanged()
{
	RefreshChildren();
}

void UNiagaraStackModuleItem::MoveUp()
{
	TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> StackNodeGroups;
	FNiagaraStackGraphUtilities::GetStackNodeGroups(*FunctionCallNode, StackNodeGroups);
	int32 ModuleStackIndex = StackNodeGroups.IndexOfByPredicate([&](const FNiagaraStackGraphUtilities::FStackNodeGroup& StackNodeGroup) { return StackNodeGroup.EndNode == FunctionCallNode; });

	if (ModuleStackIndex > 1)
	{
		FScopedTransaction ScopedTransaction(LOCTEXT("MoveModuleUpTheStackTransaction", "Move module up the stack"));

		FNiagaraStackGraphUtilities::DisconnectStackNodeGroup(StackNodeGroups[ModuleStackIndex], StackNodeGroups[ModuleStackIndex - 1], StackNodeGroups[ModuleStackIndex + 1]);
		FNiagaraStackGraphUtilities::ConnectStackNodeGroup(StackNodeGroups[ModuleStackIndex], StackNodeGroups[ModuleStackIndex - 2], StackNodeGroups[ModuleStackIndex - 1]);

		FunctionCallNode->GetNiagaraGraph()->NotifyGraphNeedsRecompile();
		FNiagaraStackGraphUtilities::RelayoutGraph(*FunctionCallNode->GetGraph());
		ModifiedGroupItemsDelegate.ExecuteIfBound();
	}
}

void UNiagaraStackModuleItem::MoveDown()
{
	TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> StackNodeGroups;
	FNiagaraStackGraphUtilities::GetStackNodeGroups(*FunctionCallNode, StackNodeGroups);
	int32 ModuleStackIndex = StackNodeGroups.IndexOfByPredicate([&](const FNiagaraStackGraphUtilities::FStackNodeGroup& StackNodeGroup) { return StackNodeGroup.EndNode == FunctionCallNode; });

	if (ModuleStackIndex < StackNodeGroups.Num() - 2)
	{
		FScopedTransaction ScopedTransaction(LOCTEXT("MoveModuleDownTheStackTransaction", "Move module down the stack"));

		FNiagaraStackGraphUtilities::DisconnectStackNodeGroup(StackNodeGroups[ModuleStackIndex], StackNodeGroups[ModuleStackIndex - 1], StackNodeGroups[ModuleStackIndex + 1]);
		FNiagaraStackGraphUtilities::ConnectStackNodeGroup(StackNodeGroups[ModuleStackIndex], StackNodeGroups[ModuleStackIndex + 1], StackNodeGroups[ModuleStackIndex + 2]);

		FunctionCallNode->GetNiagaraGraph()->NotifyGraphNeedsRecompile();
		FNiagaraStackGraphUtilities::RelayoutGraph(*FunctionCallNode->GetGraph());
		ModifiedGroupItemsDelegate.ExecuteIfBound();
	}
}

void UNiagaraStackModuleItem::Delete()
{
	TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> StackNodeGroups;
	FNiagaraStackGraphUtilities::GetStackNodeGroups(*FunctionCallNode, StackNodeGroups);
	int32 ModuleStackIndex = StackNodeGroups.IndexOfByPredicate([&](const FNiagaraStackGraphUtilities::FStackNodeGroup& StackNodeGroup) { return StackNodeGroup.EndNode == FunctionCallNode; });

	FScopedTransaction ScopedTransaction(LOCTEXT("RemoveAModuleFromTheStack", "Remove a module from the stack"));

	FNiagaraStackGraphUtilities::DisconnectStackNodeGroup(StackNodeGroups[ModuleStackIndex], StackNodeGroups[ModuleStackIndex - 1], StackNodeGroups[ModuleStackIndex + 1]);

	FNiagaraStackGraphUtilities::FStackNodeGroup ModuleGroup = StackNodeGroups[ModuleStackIndex];

	TArray<UNiagaraNode*> NodesToRemove;
	TArray<UNiagaraNode*> NodesToCheck;
	NodesToCheck.Add(ModuleGroup.EndNode);
	while (NodesToCheck.Num() > 0)
	{
		UNiagaraNode* NodeToRemove = NodesToCheck[0];
		NodesToCheck.RemoveAt(0);
		NodesToRemove.Add(NodeToRemove);

		TArray<UEdGraphPin*> InputPins;
		NodeToRemove->GetInputPins(InputPins);
		for (UEdGraphPin* InputPin : InputPins)
		{
			if (InputPin->LinkedTo.Num() == 1)
			{
				UNiagaraNode* LinkedNode = Cast<UNiagaraNode>(InputPin->LinkedTo[0]->GetOwningNode());
				if (LinkedNode != nullptr)
				{
					NodesToCheck.Add(LinkedNode);
				}
			}
		}
	}

	UNiagaraGraph* Graph = FunctionCallNode->GetNiagaraGraph();
	for (UNiagaraNode* NodeToRemove : NodesToRemove)
	{
		Graph->RemoveNode(NodeToRemove);
	}

	Graph->NotifyGraphNeedsRecompile();
	FNiagaraStackGraphUtilities::RelayoutGraph(*FunctionCallNode->GetGraph());
	ModifiedGroupItemsDelegate.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE
