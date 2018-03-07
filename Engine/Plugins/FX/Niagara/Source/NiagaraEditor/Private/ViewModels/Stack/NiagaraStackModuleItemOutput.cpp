// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackModuleItemOutput.h"
#include "NiagaraNodeFunctionCall.h"

#define LOCTEXT_NAMESPACE "NiagaraStackViewModel"

UNiagaraStackModuleItemOutput::UNiagaraStackModuleItemOutput()
	: FunctionCallNode(nullptr)
{
}

int32 UNiagaraStackModuleItemOutput::GetItemIndentLevel() const
{
	return 1;
}

void UNiagaraStackModuleItemOutput::Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraNodeFunctionCall& InFunctionCallNode, FString InOutputParameterHandle)
{
	checkf(FunctionCallNode.Get() == nullptr, TEXT("Can only set the Output once."));
	Super::Initialize(InSystemViewModel, InEmitterViewModel);
	FunctionCallNode = &InFunctionCallNode;

	OutputParameterHandle = FNiagaraParameterHandle(InOutputParameterHandle);
	DisplayName = FText::FromString(OutputParameterHandle.GetName());
}

FText UNiagaraStackModuleItemOutput::GetDisplayName() const
{
	return DisplayName;
}

FName UNiagaraStackModuleItemOutput::GetTextStyleName() const
{
	return "NiagaraEditor.Stack.ParameterText";
}

bool UNiagaraStackModuleItemOutput::GetCanExpand() const
{
	return true;
}

const FNiagaraParameterHandle& UNiagaraStackModuleItemOutput::GetOutputParameterHandle() const
{
	return OutputParameterHandle;
}

FText UNiagaraStackModuleItemOutput::GetOutputParameterHandleText() const
{
	return FText::FromString(OutputParameterHandle.GetParameterHandleString());
}

#undef LOCTEXT_NAMESPACE
