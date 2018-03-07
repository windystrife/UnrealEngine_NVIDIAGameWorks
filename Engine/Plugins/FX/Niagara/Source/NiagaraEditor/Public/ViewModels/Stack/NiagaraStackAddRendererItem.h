// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.h"
#include "EdGraph/EdGraphSchema.h"
#include "NiagaraStackAddRendererItem.generated.h"

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackAddRendererItem : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE(FOnItemAdded);

public:
	//~ UNiagaraStackEntry interface
	virtual FText GetDisplayName() const override;

	void SetOnItemAdded(FOnItemAdded InOnItemAdded);

	void AddRenderer(UClass* RendererClass);

private:
	FOnItemAdded ItemAddedDelegate;
};