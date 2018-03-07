// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.h"
#include "NiagaraStackItemGroup.generated.h"

class UNiagaraStackEditorData;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackItemGroup : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraStackEditorData& InStackEditorData);

	//~ UNiagaraStackEntry interface
	virtual bool GetCanExpand() const override;
	virtual bool GetIsExpanded() const override;
	virtual void SetIsExpanded(bool bInExpanded) override;
	virtual FName GetItemBackgroundName() const override;
	virtual FName GetTextStyleName() const override;
	virtual FText GetTooltipText() const override;

	virtual void SetTooltipText(const FText& InText);

	virtual bool CanDelete() const { return false; }
	virtual bool Delete() { return false; }

	virtual bool CanAdd() const { return false; }
	virtual bool Add() { return false; }
	
protected:
	UNiagaraStackEditorData& GetStackEditorData() const;

private:
	UNiagaraStackEditorData* StackEditorData;

	FText TooltipText;
};