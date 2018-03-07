// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackStruct.h"


UNiagaraStackStruct::UNiagaraStackStruct()
	: OwningObject(nullptr)
{
}

void UNiagaraStackStruct::Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UObject* InOwningObject, const UStruct* InScriptStructClass, uint8* InStructData)
{
	checkf(OwningObject == nullptr, TEXT("Can only initialize once."));
	Super::Initialize(InSystemViewModel, InEmitterViewModel);
	OwningObject = InOwningObject;

	StructData = MakeShareable(new FStructOnScope(InScriptStructClass, InStructData));
}

UObject* UNiagaraStackStruct::GetOwningObject()
{
	return OwningObject;
}


TSharedPtr<FStructOnScope> UNiagaraStackStruct::GetStructOnScope()
{
	return StructData;
}

int32 UNiagaraStackStruct::GetItemIndentLevel() const
{
	return ItemIndentLevel;
}

void UNiagaraStackStruct::SetItemIndentLevel(int32 InItemIndentLevel)
{
	ItemIndentLevel = InItemIndentLevel;
}

bool UNiagaraStackStruct::HasDetailCustomization() const
{
	if (DetailCustomization.IsBound())
	{
		return true;
	}
	return false;
}

FOnGetDetailCustomizationInstance UNiagaraStackStruct::GetDetailsCustomization() const
{
	return DetailCustomization;
}


void UNiagaraStackStruct::SetDetailsCustomization(FOnGetDetailCustomizationInstance Customization)
{
	DetailCustomization = Customization;
}