// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.h"
#include "NiagaraStackFunctionInputCollection.generated.h"

class UNiagaraNodeFunctionCall;
class UNiagaraStackFunctionInput;
class UNiagaraStackEditorData;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackFunctionInputCollection : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnInputPinnedChanged);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnFilterChildren, UNiagaraStackFunctionInput*);

public:
	struct FDisplayOptions
	{
		FText DisplayName;
		bool bShouldShowInStack;
		int32 ChildItemIndentLevel;
		FOnFilterChildren ChildFilter;
	};

public:
	UNiagaraStackFunctionInputCollection();

	UNiagaraNodeFunctionCall* GetModuleNode() const;

	UNiagaraNodeFunctionCall* GetInputFunctionCallNode() const;

	void Initialize(
		TSharedRef<FNiagaraSystemViewModel> InSystemViewModel,
		TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel,
		UNiagaraStackEditorData& InStackEditorData,
		UNiagaraNodeFunctionCall& InModuleNode,
		UNiagaraNodeFunctionCall& InInputFunctionCallNode,
		FDisplayOptions InDisplayOptions);

	//~ UNiagaraStackEntry interface
	virtual FText GetDisplayName() const override;
	virtual FName GetTextStyleName() const override;
	virtual bool GetCanExpand() const override;
	virtual bool GetShouldShowInStack() const override;

	FOnInputPinnedChanged& OnInputPinnedChanged();

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren) override;

private:
	void ChildPinnedChanged();

private:
	UNiagaraStackEditorData* StackEditorData;
	UNiagaraNodeFunctionCall* ModuleNode;
	UNiagaraNodeFunctionCall* InputFunctionCallNode;

	FDisplayOptions DisplayOptions;

	FOnInputPinnedChanged InputPinnedChangedDelegate;
};