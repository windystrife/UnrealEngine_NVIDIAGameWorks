// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackItem.h"

void UNiagaraStackItem::Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraStackEditorData& InStackEditorData)
{
	Super::Initialize(InSystemViewModel, InEmitterViewModel);
	StackEditorData = &InStackEditorData;
}

void UNiagaraStackItem::SetOnModifiedGroupItems(FOnModifiedGroupItems OnModifiedGroupItems)
{
	ModifiedGroupItemsDelegate = OnModifiedGroupItems;
}

UNiagaraStackEditorData& UNiagaraStackItem::GetStackEditorData() const
{
	return *StackEditorData;
}
