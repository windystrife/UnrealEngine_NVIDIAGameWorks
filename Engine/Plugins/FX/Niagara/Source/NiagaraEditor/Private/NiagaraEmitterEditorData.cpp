// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterEditorData.h"
#include "NiagaraStackEditorData.h"

UNiagaraEmitterEditorData::UNiagaraEmitterEditorData(const FObjectInitializer& ObjectInitializer)
{
	StackEditorData = ObjectInitializer.CreateDefaultSubobject<UNiagaraStackEditorData>(this, TEXT("StackEditorData"));
}

void UNiagaraEmitterEditorData::PostLoad()
{
	Super::PostLoad();
	if (StackEditorData == nullptr)
	{
		StackEditorData = NewObject<UNiagaraStackEditorData>(this, TEXT("StackEditorData"), RF_Transactional);
	}
}

UNiagaraStackEditorData& UNiagaraEmitterEditorData::GetStackEditorData() const
{
	return *StackEditorData;
}