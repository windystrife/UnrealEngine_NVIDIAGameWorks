// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.h"
#include "NiagaraStackItemExpander.generated.h"

class UNiagaraNodeFunctionCall;
class UNiagaraStackFunctionInputCollection;
class UNiagaraStackModuleItemOutputCollection;
class UNiagaraStackEditorData;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackItemExpander : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE(FOnExpandedChanged);

public:
	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraStackEditorData& InStackEditorData, FString InEmitterEditorDataKey, bool bInIsExpandedDefault);

	void SetOnExpnadedChanged(FOnExpandedChanged OnExpandedChanged);

	bool GetIsExpanded() const;

	void ToggleExpanded();

private:
	UNiagaraStackEditorData* StackEditorData;

	FString EmitterEditorDataKey;

	bool bIsExpandedDefault;

	FOnExpandedChanged ExpandedChangedDelegate;
};