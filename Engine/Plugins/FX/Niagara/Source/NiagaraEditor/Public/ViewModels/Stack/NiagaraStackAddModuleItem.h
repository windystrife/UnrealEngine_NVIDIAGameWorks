// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.h"
#include "NiagaraTypes.h"
#include "EdGraph/EdGraphSchema.h"
#include "NiagaraCommon.h"
#include "NiagaraStackAddModuleItem.generated.h"

class UNiagaraNodeOutput;
class UNiagaraStackEditorData;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackAddModuleItem : public UNiagaraStackEntry
{
	GENERATED_BODY()
public:
	DECLARE_DELEGATE(FOnItemAdded);

public:
	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraStackEditorData& InStackEditorData);

	virtual FText GetDisplayName() const override;

	void SetOnItemAdded(FOnItemAdded InOnItemAdded);

	void AddScriptModule(FAssetData ModuleScriptAsset);

	void AddParameterModule(FNiagaraVariable ParameterVariable, bool bRenamePending);

	virtual void GetAvailableParameters(TArray<FNiagaraVariable>& OutAvailableParameterVariables) const;

	virtual void GetNewParameterAvailableTypes(TArray<FNiagaraTypeDefinition>& OutAvailableTypes) const;

	virtual TOptional<FString> GetNewParameterNamespace() const;

	virtual ENiagaraScriptUsage GetOutputUsage() const PURE_VIRTUAL(UNiagaraStackAddModuleItem::GetOutputUsage, return ENiagaraScriptUsage::EmitterSpawnScript;);

protected:
	virtual UNiagaraNodeOutput* GetOrCreateOutputNode() PURE_VIRTUAL(UNiagaraStackModuleItem::GetOrCreateOutputNode, return nullptr;);

	virtual FText GetInsertTransactionText() const;

protected:
	FOnItemAdded ItemAddedDelegate;

	UNiagaraStackEditorData* StackEditorData;
};