// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.h"
#include "NiagaraStackItem.generated.h"

class UNiagaraStackEditorData;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackItem : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE(FOnModifiedGroupItems);

public:
	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraStackEditorData& InStackEditorData);

	void SetOnModifiedGroupItems(FOnModifiedGroupItems OnModifiedGroupItems);

protected:
	UNiagaraStackEditorData& GetStackEditorData() const;

protected:
	FOnModifiedGroupItems ModifiedGroupItemsDelegate;

private:
	UNiagaraStackEditorData* StackEditorData;
};