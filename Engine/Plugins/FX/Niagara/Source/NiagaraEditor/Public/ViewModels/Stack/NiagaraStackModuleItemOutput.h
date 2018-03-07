// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.h"
#include "NiagaraParameterHandle.h"
#include "NiagaraStackModuleItemOutput.generated.h"

class UNiagaraNodeFunctionCall;

/** Represents a single module Output in the module stack view model. */
UCLASS()
class NIAGARAEDITOR_API UNiagaraStackModuleItemOutput : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	UNiagaraStackModuleItemOutput();

	/** 
	 * Sets the Output data for this entry.
	 * @param InFunctionCallNode The function call node representing the module in the stack graph which owns this Output.
	 * @param InOutputParameterHandle The Namespace.Name handle for the Output to the owning module.
	 */
	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraNodeFunctionCall& InFunctionCallNode, FString InOutputParameterHandle);

	//~ UNiagaraStackEntry interface
	virtual FText GetDisplayName() const override;
	virtual FName GetTextStyleName() const override;
	virtual bool GetCanExpand() const override;
	virtual int32 GetItemIndentLevel() const override;

	/** Gets the parameter handle which defined this Output in the module. */
	const FNiagaraParameterHandle& GetOutputParameterHandle() const;

	/** Gets the assigned parameter handle as displayable text. */
	FText GetOutputParameterHandleText() const;

private:
	/** The function call node which represents the emitter which owns this Output */
	TWeakObjectPtr<UNiagaraNodeFunctionCall> FunctionCallNode;

	/** The parameter handle which defined this Output in the module graph. */
	FNiagaraParameterHandle OutputParameterHandle;

	/** The name of this Output for display in the UI. */
	FText DisplayName;
};