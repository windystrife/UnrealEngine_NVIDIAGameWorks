// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackItemGroup.h"
#include "NiagaraStackRenderItemGroup.generated.h"

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackRenderItemGroup : public UNiagaraStackItemGroup
{
	GENERATED_BODY()

public:
	virtual FText GetDisplayName() const override;
	void SetDisplayName(FText InDisplayName);

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren) override;

private:
	void ChildModifiedGroupItems();

private:
	FText DisplayName;
};