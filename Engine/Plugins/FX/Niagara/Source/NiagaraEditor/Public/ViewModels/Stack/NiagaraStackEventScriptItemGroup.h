// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "NiagaraEmitter.h"
#include "NiagaraStackScriptItemGroup.h"
#include "NiagaraStackAddEventScriptItem.h"
#include "NiagaraStackEventScriptItemGroup.generated.h"

class FNiagaraScriptViewModel;

UCLASS()
/** Meant to contain a single binding of a Emitter::EventScriptProperties to the stack.*/
class NIAGARAEDITOR_API UNiagaraStackEventScriptItemGroup : public UNiagaraStackScriptItemGroup
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE(FOnModifiedEventHandlers);

protected:
	FOnModifiedEventHandlers OnModifiedEventHandlersDelegate;

	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren) override;

	virtual bool CanDelete() const override;
	virtual bool Delete() override;
	
public:
	virtual FText GetDisplayName() const override;
	void SetOnModifiedEventHandlers(FOnModifiedEventHandlers OnModifiedEventHandlers);

};