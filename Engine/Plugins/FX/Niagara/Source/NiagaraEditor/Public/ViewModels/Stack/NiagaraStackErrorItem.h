// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.h"
#include "Reply.h"
#include "Layout/Visibility.h"
#include "NiagaraStackErrorItem.generated.h"

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackErrorItem : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	UNiagaraStackErrorItem();

	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraStackEntry* InErrorSource, int32 InErrorIndex);

	//~ UNiagaraStackEntry interface
	virtual int32 GetItemIndentLevel() const override;

	/** Sets the item indent level for this stack entry. */
	void SetItemIndentLevel(int32 InItemIndentLevel);

	int32 GetErrorIndex() const { return ErrorIdx; }

	FText ErrorText() const;
	FText ErrorTextTooltip() const;
	FReply OnTryFixError();
	EVisibility CanFixVisibility() const;

	virtual FName GetItemBackgroundName() const override;

protected:
	UNiagaraStackEntry* ErrorSource;
	int32 ErrorIdx;
	int32 ItemIndentLevel;
};