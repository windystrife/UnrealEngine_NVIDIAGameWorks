// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackItemGroup.h"
#include "NiagaraStackItem.h"
#include "NiagaraStackEditorData.h"

void UNiagaraStackItemGroup::Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraStackEditorData& InStackEditorData)
{
	Super::Initialize(InSystemViewModel, InEmitterViewModel);
	StackEditorData = &InStackEditorData;
}

bool UNiagaraStackItemGroup::GetCanExpand() const
{
	return true;
}

bool UNiagaraStackItemGroup::GetIsExpanded() const
{
	return StackEditorData->GetStackEntryIsExpanded(GetDisplayName().ToString(), IsExpandedByDefault());
}

void UNiagaraStackItemGroup::SetIsExpanded(bool bInExpanded)
{
	StackEditorData->SetStackEntryIsExpanded(GetDisplayName().ToString(), bInExpanded);
}

FName UNiagaraStackItemGroup::GetItemBackgroundName() const
{
	return "NiagaraEditor.Stack.Group.BackgroundColor";
}

FName UNiagaraStackItemGroup::GetTextStyleName() const
{
	return "NiagaraEditor.Stack.GroupText";
}

FText UNiagaraStackItemGroup::GetTooltipText() const 
{
	return TooltipText;
}

void UNiagaraStackItemGroup::SetTooltipText(const FText& InText)
{
	TooltipText = InText;
}

UNiagaraStackEditorData& UNiagaraStackItemGroup::GetStackEditorData() const
{
	return *StackEditorData;
}