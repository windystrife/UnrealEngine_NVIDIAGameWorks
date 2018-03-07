// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackItemGroup.h"
#include "NiagaraCommon.h"
#include "NiagaraParameterStore.h"
#include "NiagaraStackParameterStoreGroup.generated.h"

class FNiagaraScriptViewModel;
class UNiagaraStackEditorData;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackParameterStoreGroup : public UNiagaraStackItemGroup
{
	GENERATED_BODY()

public:
	void Initialize(
		TSharedRef<FNiagaraSystemViewModel> InSystemViewModel,
		TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel,
		UNiagaraStackEditorData& InStackEditorData,
		TSharedRef<FNiagaraScriptViewModel> InScriptViewModel,
		UObject* InOwner,
		FNiagaraParameterStore* InParameterStore);
	
	virtual FText GetDisplayName() const override;
	void SetDisplayName(FText InDisplayName);

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren) override;

private:
	void ItemAdded();

	void ChildModifiedGroupItems();

private:

	FText DisplayName;

	UPROPERTY()
	UObject* Owner;
	FNiagaraParameterStore* ParameterStore;
};