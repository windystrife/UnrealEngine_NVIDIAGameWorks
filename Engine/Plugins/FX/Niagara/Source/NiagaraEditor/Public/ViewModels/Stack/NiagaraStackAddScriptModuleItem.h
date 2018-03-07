// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackAddModuleItem.h"
#include "EdGraph/EdGraphSchema.h"
#include "NiagaraStackAddScriptModuleItem.generated.h"

class UNiagaraNodeOutput;
class UNiagaraStackEditorData;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackAddScriptModuleItem : public UNiagaraStackAddModuleItem
{
	GENERATED_BODY()

public:
	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraStackEditorData& InStackEditorData, UNiagaraNodeOutput& InOutputNode);

	virtual void GetAvailableParameters(TArray<FNiagaraVariable>& OutAvailableParameterVariables) const override;

	virtual void GetNewParameterAvailableTypes(TArray<FNiagaraTypeDefinition>& OutAvailableTypes) const override;

	virtual TOptional<FString> GetNewParameterNamespace() const override;

	virtual ENiagaraScriptUsage GetOutputUsage() const override;

	UNiagaraNodeOutput* GetOrCreateOutputNode() override;

private:
	TWeakObjectPtr<UNiagaraNodeOutput> OutputNode;
};