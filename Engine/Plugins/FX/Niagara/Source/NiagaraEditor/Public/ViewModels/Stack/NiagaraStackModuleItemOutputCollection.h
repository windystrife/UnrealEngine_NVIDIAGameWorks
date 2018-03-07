// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.h"
#include "NiagaraStackModuleItemOutputCollection.generated.h"

class UNiagaraNodeFunctionCall;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackModuleItemOutputCollection : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	UNiagaraStackModuleItemOutputCollection();

	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraNodeFunctionCall& InFunctionCallNode);

	//~ UNiagaraStackEntry interface
	virtual FText GetDisplayName() const override;
	virtual FName GetTextStyleName() const override;
	virtual bool GetCanExpand() const override;
	virtual bool IsExpandedByDefault() const override;

	void SetDisplayName(FText InDisplayName);

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren) override;

private:
	UNiagaraNodeFunctionCall* FunctionCallNode;
	FText DisplayName;
};