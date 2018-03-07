// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackFunctionInputCollection.h"
#include "NiagaraStackFunctionInput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraEmitterEditorData.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraStackGraphUtilities.h"

#include "EdGraph/EdGraphPin.h"

UNiagaraStackFunctionInputCollection::UNiagaraStackFunctionInputCollection()
	: ModuleNode(nullptr)
	, InputFunctionCallNode(nullptr)
{
}

UNiagaraNodeFunctionCall* UNiagaraStackFunctionInputCollection::GetModuleNode() const
{
	return ModuleNode;
}

UNiagaraNodeFunctionCall* UNiagaraStackFunctionInputCollection::GetInputFunctionCallNode() const
{
	return InputFunctionCallNode;
}

void UNiagaraStackFunctionInputCollection::Initialize(
	TSharedRef<FNiagaraSystemViewModel> InSystemViewModel,
	TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel,
	UNiagaraStackEditorData& InStackEditorData,
	UNiagaraNodeFunctionCall& InModuleNode,
	UNiagaraNodeFunctionCall& InInputFunctionCallNode,
	FDisplayOptions InDisplayOptions)
{
	checkf(ModuleNode == nullptr && InputFunctionCallNode == nullptr, TEXT("Can not set the node more than once."));
	Super::Initialize(InSystemViewModel, InEmitterViewModel);
	StackEditorData = &InStackEditorData;
	ModuleNode = &InModuleNode;
	InputFunctionCallNode = &InInputFunctionCallNode;
	DisplayOptions = InDisplayOptions;
}

FText UNiagaraStackFunctionInputCollection::GetDisplayName() const
{
	return DisplayOptions.DisplayName;
}

FName UNiagaraStackFunctionInputCollection::GetTextStyleName() const
{
	return "NiagaraEditor.Stack.ParameterCollectionText";
}

bool UNiagaraStackFunctionInputCollection::GetCanExpand() const
{
	return true;
}

bool UNiagaraStackFunctionInputCollection::GetShouldShowInStack() const
{
	return DisplayOptions.bShouldShowInStack;
}

UNiagaraStackFunctionInputCollection::FOnInputPinnedChanged& UNiagaraStackFunctionInputCollection::OnInputPinnedChanged()
{
	return InputPinnedChangedDelegate;
}

void UNiagaraStackFunctionInputCollection::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren)
{
	TArray<const UEdGraphPin*> InputPins;
	FNiagaraStackGraphUtilities::GetStackFunctionInputPins(*InputFunctionCallNode, InputPins, FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly);
	const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();
	for(const UEdGraphPin* InputPin : InputPins)
	{
		UNiagaraStackFunctionInput* Input = nullptr;
		for (UNiagaraStackEntry* CurrentChild : CurrentChildren)
		{
			UNiagaraStackFunctionInput* ChildInput = CastChecked<UNiagaraStackFunctionInput>(CurrentChild);
			if (ChildInput->GetInputParameterHandle().GetParameterHandleString() == InputPin->PinName)
			{
				Input = ChildInput;
				break;
			}
		}
		if (Input == nullptr)
		{
			Input = NewObject<UNiagaraStackFunctionInput>(this);
			Input->Initialize(GetSystemViewModel(), GetEmitterViewModel(), *StackEditorData, *ModuleNode, *InputFunctionCallNode, InputPin->PinName, NiagaraSchema->PinToTypeDefinition(InputPin));
			Input->SetItemIndentLevel(DisplayOptions.ChildItemIndentLevel);
			Input->OnPinnedChanged().AddUObject(this, &UNiagaraStackFunctionInputCollection::ChildPinnedChanged);
		}

		if (DisplayOptions.ChildFilter.IsBound() == false || DisplayOptions.ChildFilter.Execute(Input))
		{
			NewChildren.Add(Input);
		}
	}
}

void UNiagaraStackFunctionInputCollection::ChildPinnedChanged()
{
	InputPinnedChangedDelegate.Broadcast();
}