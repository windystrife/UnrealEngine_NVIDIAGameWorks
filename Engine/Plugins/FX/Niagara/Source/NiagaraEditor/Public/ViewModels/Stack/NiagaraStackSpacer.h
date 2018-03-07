// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.h"
#include "NiagaraStackSpacer.generated.h"

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackSpacer : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, FName SpacerKey = NAME_None);

	//~ UNiagaraStackEntry interface
	virtual FText GetDisplayName() const override;
	virtual FName GetItemBackgroundName() const override;

	FName GetSpacerKey() const;

private:
	FName SpacerKey;
};