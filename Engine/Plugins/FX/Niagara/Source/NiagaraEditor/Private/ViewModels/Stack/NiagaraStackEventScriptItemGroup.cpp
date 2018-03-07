// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackEventScriptItemGroup.h"
#include "NiagaraStackModuleItem.h"
#include "NiagaraStackAddModuleItem.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraEmitterViewModel.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraStackErrorItem.h"
#include "NiagaraStackStruct.h"
#include "NiagaraScriptSource.h"

#include "Internationalization.h"
#include "ScopedTransaction.h"
#include "NiagaraSystemViewModel.h"
#include "Customizations/NiagaraEventScriptPropertiesCustomization.h"

#define LOCTEXT_NAMESPACE "UNiagaraStackEventScriptItemGroup"

FText UNiagaraStackEventScriptItemGroup::GetDisplayName() const 
{
	FText EventName = FText::FromName(GetEmitterViewModel()->GetEmitter()->EventHandlerScriptProps[GetScriptOccurrence()].SourceEventName);
	return (FText::Format(LOCTEXT("FormatEventScriptDisplayName", "Event Handler {0} - Source: {1}"), GetScriptOccurrence(), EventName));
}
void UNiagaraStackEventScriptItemGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren)
{
	UNiagaraEmitter* Emitter = GetEmitterViewModel()->GetEmitter();
	if (GetScriptOccurrence() < Emitter->EventHandlerScriptProps.Num())
	{
		uint8* EventStructMemory = (uint8*)&(Emitter->EventHandlerScriptProps[GetScriptOccurrence()]);

		UNiagaraStackEntry* FoundItem = nullptr;
		for (UNiagaraStackEntry* CurrentChild : CurrentChildren)
		{
			UNiagaraStackStruct* CurrentStructNode = Cast<UNiagaraStackStruct>(CurrentChild);
			if (CurrentStructNode != nullptr &&
				CurrentStructNode->GetOwningObject() == Emitter &&
				CurrentStructNode->GetStructOnScope()->GetStructMemory() == EventStructMemory)
			{
				FoundItem = CurrentStructNode;
				break;
			}
		}

		if (FoundItem == nullptr)
		{
			UNiagaraStackStruct* StructNode = NewObject<UNiagaraStackStruct>(this);
			StructNode->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetEmitterViewModel()->GetEmitter(), FNiagaraEventScriptProperties::StaticStruct(), EventStructMemory);
			UNiagaraSystem* System = &GetSystemViewModel()->GetSystem();
			StructNode->SetDetailsCustomization(FOnGetDetailCustomizationInstance::CreateStatic(&FNiagaraEventScriptPropertiesCustomization::MakeInstance, TWeakObjectPtr<UNiagaraSystem>(System), TWeakObjectPtr<UNiagaraEmitter>(Emitter)));
			FoundItem = StructNode;
		}

		NewChildren.Add(FoundItem);
	}

	Super::RefreshChildrenInternal(CurrentChildren, NewChildren);
}

bool UNiagaraStackEventScriptItemGroup::CanDelete() const
{
	return true;
}

bool UNiagaraStackEventScriptItemGroup::Delete()
{
	UNiagaraEmitter* Emitter = GetEmitterViewModel()->GetEmitter();
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Emitter->GraphSource);

	if (!Source || !Source->NodeGraph)
	{
		return false;
	}

	FScopedTransaction Transaction(FText::Format(LOCTEXT("DeleteEventHandler", "Deleted {0}"), GetDisplayName()));
	Emitter->Modify();
	Source->NodeGraph->Modify();
	TArray<UNiagaraNode*> EventIndexNodes;
	Source->NodeGraph->BuildTraversal(EventIndexNodes, GetScriptUsage(), GetScriptOccurrence());
	for (UNiagaraNode* Node : EventIndexNodes)
	{
		Node->Modify();
	}

	TArray<UNiagaraNodeOutput*> EventOutputNodes;
	Source->NodeGraph->FindOutputNodes(GetScriptUsage(), EventOutputNodes);
	for (UNiagaraNodeOutput* OutputNode : EventOutputNodes)
	{
		OutputNode->Modify();
	}

	int32 OldScriptOccurrence = GetScriptOccurrence();
	
	// First, remove the event handler script properties object.
	Emitter->EventHandlerScriptProps.RemoveAt(OldScriptOccurrence);

	// Now move all the usage indices down the list.
	for (int32 i = OldScriptOccurrence; i < Emitter->EventHandlerScriptProps.Num(); i++)
	{
		Emitter->EventHandlerScriptProps[i].Script->Modify();
		Emitter->EventHandlerScriptProps[i].Script->UsageIndex -= 1;
	}
	
	// Now remove all graph nodes associated with the event script index.
	for (UNiagaraNode* Node : EventIndexNodes)
	{
		Node->DestroyNode();
	}

	// Now move all the other output nodes down in their index to match the event script properties array.
	for (UNiagaraNodeOutput* OutputNode : EventOutputNodes)
	{
		if (EventIndexNodes.Contains(OutputNode))
		{
			continue;
		}
		else if (OutputNode->GetUsageIndex() > OldScriptOccurrence)
		{
			OutputNode->SetUsageIndex(OutputNode->GetUsageIndex() - 1);
		}
	}

	// Set the emitter here to that the internal state of the view model is updated.
	// TODO: Move the logic for managing event handlers into the emitter view model or script view model.
	ScriptViewModel->SetScripts(Emitter);
	
	OnModifiedEventHandlersDelegate.ExecuteIfBound();

	return true;
}

void UNiagaraStackEventScriptItemGroup::SetOnModifiedEventHandlers(FOnModifiedEventHandlers OnModifiedEventHandlers)
{
	OnModifiedEventHandlersDelegate = OnModifiedEventHandlers;
}


#undef LOCTEXT_NAMESPACE