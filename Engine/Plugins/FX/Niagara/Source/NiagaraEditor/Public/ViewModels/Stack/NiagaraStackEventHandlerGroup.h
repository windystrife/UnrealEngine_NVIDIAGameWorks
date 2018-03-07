// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackItemGroup.h"
#include "NiagaraStackEventHandlerGroup.generated.h"

class FNiagaraEmitterViewModel;
class UNiagaraStackAddEventScriptItem;

UCLASS()
/** Container for one or more NiagaraStackEventScriptItemGroups, allowing multiple event handlers per script.*/
class NIAGARAEDITOR_API UNiagaraStackEventHandlerGroup : public UNiagaraStackItemGroup
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE(FOnItemAdded);

public:

	virtual FText GetDisplayName() const override;
	void SetDisplayName(FText InDisplayName);
	
	virtual bool GetShouldShowInStack() const override;

	void SetOnItemAdded(FOnItemAdded InOnItemAdded);
	
protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren) override;

	void ChildModifiedGroupItems();

private:
	FText DisplayName;

	bool bForceRebuild;
	
	UPROPERTY()
	UNiagaraStackAddEventScriptItem* AddScriptItem;
	
	FOnItemAdded ItemAddedDelegate;
};