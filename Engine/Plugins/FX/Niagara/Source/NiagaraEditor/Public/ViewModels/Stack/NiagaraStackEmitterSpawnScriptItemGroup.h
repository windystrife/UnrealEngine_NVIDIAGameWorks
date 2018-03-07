// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackScriptItemGroup.h"
#include "NiagaraStackItem.h"
#include "NiagaraStackEmitterSpawnScriptItemGroup.generated.h"

class FNiagaraEmitterViewModel;
class FNiagaraScriptViewModel;
class UNiagaraStackObject;
class UNiagaraStackSpacer;
class UNiagaraStackItemExpander;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackEmitterPropertiesItem : public UNiagaraStackItem
{
	GENERATED_BODY()

public:
	virtual FText GetDisplayName() const override;

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren) override;

private:
	void EmitterExpandedChanged();

private:
	UPROPERTY()
	UNiagaraStackObject* EmitterObject;

	UPROPERTY()
	UNiagaraStackItemExpander* EmitterExpander;
};

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackEmitterSpawnScriptItemGroup : public UNiagaraStackScriptItemGroup
{
	GENERATED_BODY()

public:
	UNiagaraStackEmitterSpawnScriptItemGroup();

protected:
	void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren) override;

private:

	UPROPERTY()
	UNiagaraStackEmitterPropertiesItem* PropertiesItem;

	UPROPERTY()
	UNiagaraStackSpacer* PropertiesSpacer;
};