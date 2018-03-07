// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackEmitterSpawnScriptItemGroup.h"
#include "NiagaraStackObject.h"
#include "NiagaraStackSpacer.h"
#include "NiagaraStackItemExpander.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "NiagaraEmitter.h"
#include "NiagaraStackEditorData.h"

#define LOCTEXT_NAMESPACE "UNiagaraStackScriptItemGroup"

FText UNiagaraStackEmitterPropertiesItem::GetDisplayName() const
{
	return LOCTEXT("EmitterPropertiesDisplayName", "Emitter Properties");
}

void UNiagaraStackEmitterPropertiesItem::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren)
{
	if (EmitterObject == nullptr)
	{
		EmitterObject = NewObject<UNiagaraStackObject>(this);
		EmitterObject->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetEmitterViewModel()->GetEmitter());
	}

	if (EmitterExpander == nullptr)
	{
		EmitterExpander = NewObject<UNiagaraStackItemExpander>(this);
		EmitterExpander->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetStackEditorData(), TEXT("Emitter"), false);
		EmitterExpander->SetOnExpnadedChanged(UNiagaraStackItemExpander::FOnExpandedChanged::CreateUObject(this, &UNiagaraStackEmitterPropertiesItem::EmitterExpandedChanged));
	}

	if (GetStackEditorData().GetStackEntryIsExpanded(TEXT("Emitter"), false))
	{
		NewChildren.Add(EmitterObject);
	}

	NewChildren.Add(EmitterExpander);
}

void UNiagaraStackEmitterPropertiesItem::EmitterExpandedChanged()
{
	RefreshChildren();
}

UNiagaraStackEmitterSpawnScriptItemGroup::UNiagaraStackEmitterSpawnScriptItemGroup()
	: PropertiesItem(nullptr)
{
}

void UNiagaraStackEmitterSpawnScriptItemGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren)
{
	if (PropertiesItem == nullptr)
	{
		PropertiesItem = NewObject<UNiagaraStackEmitterPropertiesItem>(this);
		PropertiesItem->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetStackEditorData());
	}

	if (PropertiesSpacer == nullptr)
	{
		PropertiesSpacer = NewObject<UNiagaraStackSpacer>(this);
		PropertiesSpacer->Initialize(GetSystemViewModel(), GetEmitterViewModel(), "EmitterProperties");
	}

	NewChildren.Add(PropertiesItem);
	NewChildren.Add(PropertiesSpacer);

	Super::RefreshChildrenInternal(CurrentChildren, NewChildren);
}

#undef LOCTEXT_NAMESPACE