// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackModuleItemOutputCollection.h"
#include "NiagaraStackModuleItemOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraEmitterEditorData.h"
#include "NiagaraStackGraphUtilities.h"

#include "EdGraph/EdGraphPin.h"

UNiagaraStackModuleItemOutputCollection::UNiagaraStackModuleItemOutputCollection()
	: FunctionCallNode(nullptr)
{
}

void UNiagaraStackModuleItemOutputCollection::Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraNodeFunctionCall& InFunctionCallNode)
{
	checkf(FunctionCallNode == nullptr, TEXT("Can not set the node more than once."));
	Super::Initialize(InSystemViewModel, InEmitterViewModel);
	FunctionCallNode = &InFunctionCallNode;
}

FText UNiagaraStackModuleItemOutputCollection::GetDisplayName() const
{
	return DisplayName;
}

FName UNiagaraStackModuleItemOutputCollection::GetTextStyleName() const
{
	return "NiagaraEditor.Stack.ParameterCollectionText";
}

void UNiagaraStackModuleItemOutputCollection::SetDisplayName(FText InDisplayName)
{
	DisplayName = InDisplayName;
}

bool UNiagaraStackModuleItemOutputCollection::GetCanExpand() const
{
	return true;
}

bool UNiagaraStackModuleItemOutputCollection::IsExpandedByDefault() const
{
	return false;
}

void UNiagaraStackModuleItemOutputCollection::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren)
{
	UEdGraphPin* OutputParameterMapPin = FNiagaraStackGraphUtilities::GetParameterMapOutputPin(*FunctionCallNode);
	if (ensureMsgf(OutputParameterMapPin != nullptr, TEXT("Invalid Stack Graph - Function call node has no output pin.")))
	{
		FNiagaraParameterMapHistoryBuilder Builder;
		FunctionCallNode->BuildParameterMapHistory(Builder, false);
		check(Builder.Histories.Num() == 1);
		for (int32 i = 0; i < Builder.Histories[0].Variables.Num(); i++)
		{
			FNiagaraVariable& Variable = Builder.Histories[0].Variables[i];
			TArray<const UEdGraphPin*>& WriteHistory = Builder.Histories[0].PerVariableWriteHistory[i];

			for (const UEdGraphPin* WritePin : WriteHistory)
			{
				if (Cast<UNiagaraNodeParameterMapSet>(WritePin->GetOwningNode()) != nullptr)
				{
					UNiagaraStackModuleItemOutput* Output = nullptr;
					for (UNiagaraStackEntry* CurrentChild : CurrentChildren)
					{
						UNiagaraStackModuleItemOutput* ChildOutput = CastChecked<UNiagaraStackModuleItemOutput>(CurrentChild);
						if (ChildOutput->GetOutputParameterHandle().GetParameterHandleString() == Variable.GetName().ToString())
						{
							Output = ChildOutput;
							break;
						}
					}
					if (Output == nullptr)
					{
						Output = NewObject<UNiagaraStackModuleItemOutput>(this);
						Output->Initialize(GetSystemViewModel(), GetEmitterViewModel(), *FunctionCallNode, Variable.GetName().ToString());
					}

					NewChildren.Add(Output);
					break;
				}
			}
		}
	}
}