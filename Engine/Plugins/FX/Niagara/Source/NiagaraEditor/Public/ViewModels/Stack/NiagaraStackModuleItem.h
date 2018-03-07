// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackItem.h"
#include "NiagaraStackModuleItem.generated.h"

class UNiagaraNodeFunctionCall;
class UNiagaraStackFunctionInputCollection;
class UNiagaraStackModuleItemOutputCollection;
class UNiagaraStackItemExpander;
class UNiagaraStackEditorData;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackModuleItem : public UNiagaraStackItem
{
	GENERATED_BODY()

public:
	UNiagaraStackModuleItem();

	const UNiagaraNodeFunctionCall& GetModuleNode() const;
	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraStackEditorData& InStackEditorData, UNiagaraNodeFunctionCall& InFunctionCallNode);

	virtual FText GetDisplayName() const override;
	virtual FText GetTooltipText() const override;

	void MoveUp();
	void MoveDown();
	void Delete();

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren) override;

private:
	void InputPinnedChanged();
	void ModuleExpandedChanged();

private:
	UNiagaraNodeFunctionCall* FunctionCallNode;

	UPROPERTY()
	UNiagaraStackFunctionInputCollection* PinnedInputCollection;

	UPROPERTY()
	UNiagaraStackFunctionInputCollection* UnpinnedInputCollection;

	UPROPERTY()
	UNiagaraStackModuleItemOutputCollection* OutputCollection;

	UPROPERTY()
	UNiagaraStackItemExpander* ModuleExpander;
};