// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackObject.h"


UNiagaraStackObject::UNiagaraStackObject()
	: Object(nullptr)
{
}

void UNiagaraStackObject::Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UObject* InObject)
{
	checkf(Object == nullptr, TEXT("Can only initialize once."));
	Super::Initialize(InSystemViewModel, InEmitterViewModel);
	Object = InObject;
}

UObject* UNiagaraStackObject::GetObject()
{
	return Object;
}

int32 UNiagaraStackObject::GetItemIndentLevel() const
{
	return ItemIndentLevel;
}

void UNiagaraStackObject::SetItemIndentLevel(int32 InItemIndentLevel)
{
	ItemIndentLevel = InItemIndentLevel;
}

void UNiagaraStackObject::NotifyObjectChanged()
{
	OnDataObjectModified().Broadcast(Object);
}