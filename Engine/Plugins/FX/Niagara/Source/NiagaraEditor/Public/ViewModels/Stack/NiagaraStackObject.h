// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.h"
#include "NiagaraStackObject.generated.h"

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackObject : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	UNiagaraStackObject();

	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UObject* InObject);

	UObject* GetObject();

	//~ UNiagaraStackEntry interface
	virtual int32 GetItemIndentLevel() const override;

	/** Sets the item indent level for this stack entry. */
	void SetItemIndentLevel(int32 InItemIndentLevel);

	/** Notifies the stack entry that it's object has been modified. */
	void NotifyObjectChanged();

private:
	UObject* Object;
	int32 ItemIndentLevel;
};