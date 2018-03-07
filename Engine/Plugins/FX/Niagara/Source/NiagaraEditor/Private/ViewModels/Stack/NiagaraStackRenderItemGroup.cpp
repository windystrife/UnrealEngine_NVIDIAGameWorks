// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackRenderItemGroup.h"
#include "NiagaraStackRendererItem.h"
#include "NiagaraStackSpacer.h"
#include "NiagaraStackAddRendererItem.h"
#include "NiagaraEmitterViewModel.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterEditorData.h"

FText UNiagaraStackRenderItemGroup::GetDisplayName() const
{
	return DisplayName;
}

void UNiagaraStackRenderItemGroup::SetDisplayName(FText InDisplayName)
{
	DisplayName = InDisplayName;
}

void UNiagaraStackRenderItemGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren)
{
	int32 RendererIndex = 0;
	for (UNiagaraRendererProperties* RendererProperties : GetEmitterViewModel()->GetEmitter()->RendererProperties)
	{
		UNiagaraStackRendererItem* RendererItem = FindCurrentChildOfTypeByPredicate<UNiagaraStackRendererItem>(CurrentChildren,
			[=](UNiagaraStackRendererItem* CurrentRendererItem) { return CurrentRendererItem->GetRendererProperties() == RendererProperties; });

		if (RendererItem == nullptr)
		{
			RendererItem = NewObject<UNiagaraStackRendererItem>(this);
			RendererItem->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetStackEditorData(), RendererProperties);
			RendererItem->SetOnModifiedGroupItems(UNiagaraStackItem::FOnModifiedGroupItems::CreateUObject(this, &UNiagaraStackRenderItemGroup::ChildModifiedGroupItems));
		}

		FName RendererSpacerKey = *FString::Printf(TEXT("Renderer%i"), RendererIndex);
		UNiagaraStackSpacer* RendererSpacer = FindCurrentChildOfTypeByPredicate<UNiagaraStackSpacer>(CurrentChildren,
			[=](UNiagaraStackSpacer* CurrentRendererSpacer) { return CurrentRendererSpacer->GetSpacerKey() == RendererSpacerKey; });

		if (RendererSpacer == nullptr)
		{
			RendererSpacer = NewObject<UNiagaraStackSpacer>(this);
			RendererSpacer->Initialize(GetSystemViewModel(), GetEmitterViewModel(), RendererSpacerKey);
		}

		NewChildren.Add(RendererItem);
		NewChildren.Add(RendererSpacer);
	}

	UNiagaraStackAddRendererItem* AddRendererItem = NewObject<UNiagaraStackAddRendererItem>(this);
	AddRendererItem->Initialize(GetSystemViewModel(), GetEmitterViewModel());
	AddRendererItem->SetOnItemAdded(UNiagaraStackAddRendererItem::FOnItemAdded::CreateUObject(this, &UNiagaraStackRenderItemGroup::ChildModifiedGroupItems));
	NewChildren.Add(AddRendererItem);
}

void UNiagaraStackRenderItemGroup::ChildModifiedGroupItems()
{
	RefreshChildren();
}

