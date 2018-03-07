// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.h"
#include "EdGraph/EdGraphSchema.h"
#include "NiagaraStackAddModuleItem.h"
#include "NiagaraStackAddEventScriptItem.generated.h"

class FNiagaraEmitterViewModel;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackAddEventScriptItem : public UNiagaraStackAddModuleItem
{
	GENERATED_BODY()

public:
	UNiagaraStackAddEventScriptItem();

	//~ UNiagaraStackEntry interface
	virtual FText GetDisplayName() const override;

	virtual ENiagaraScriptUsage GetOutputUsage() const override;

protected:
	//~ UNiagaraStackAddModuleItem interface
	virtual UNiagaraNodeOutput* GetOrCreateOutputNode() override;
	virtual FText GetInsertTransactionText() const override;

private:
	void AddEventScript();

};