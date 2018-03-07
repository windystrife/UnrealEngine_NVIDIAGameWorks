// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackAddEventScriptItem.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScript.h"
#include "NiagaraEmitter.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraStackEventScriptItemGroup.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "ViewModels/NiagaraScriptGraphViewModel.h"
#include "NiagaraStackGraphUtilities.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "NiagaraStackViewModel"

UNiagaraStackAddEventScriptItem::UNiagaraStackAddEventScriptItem()
{
}

FText UNiagaraStackAddEventScriptItem::GetDisplayName() const
{
	return FText();
}

ENiagaraScriptUsage UNiagaraStackAddEventScriptItem::GetOutputUsage() const
{
	return ENiagaraScriptUsage::ParticleEventScript;
}

UNiagaraNodeOutput* UNiagaraStackAddEventScriptItem::GetOrCreateOutputNode()
{
	UNiagaraEmitter* Emitter = GetEmitterViewModel()->GetEmitter();
	UNiagaraScriptSource* Source = GetEmitterViewModel()->GetSharedScriptViewModel()->GetGraphViewModel()->GetScriptSource();
	UNiagaraGraph* Graph = GetEmitterViewModel()->GetSharedScriptViewModel()->GetGraphViewModel()->GetGraph();

	// The stack should not have been created if any of these are null, so bail out if it happens somehow rather than try to handle all of these cases.
	checkf(Emitter != nullptr && Source != nullptr && Graph != nullptr, TEXT("Stack created for invalid emitter or graph."));

	// Check that there isn't already an event output at the new index before making any changes so that we can early out and don't leave the change
	// half completed.
	if(ensureMsgf(Graph->FindOutputNode(ENiagaraScriptUsage::ParticleEventScript, Emitter->EventHandlerScriptProps.Num()) == nullptr,
		TEXT("Invalid Stack Graph - While creating a new event handler an event script output node already existed with the new index.")))
	{
		Emitter->Modify();
		FNiagaraEventScriptProperties EventScriptProperties;
		EventScriptProperties.Script = NewObject<UNiagaraScript>(Emitter, MakeUniqueObjectName(Emitter, UNiagaraScript::StaticClass(), "EventScript"), EObjectFlags::RF_Transactional);
		EventScriptProperties.Script->SetUsage(ENiagaraScriptUsage::ParticleEventScript);
		EventScriptProperties.Script->SetSource(Source);

		int32 Index = Emitter->EventHandlerScriptProps.Add(EventScriptProperties);
		Emitter->EventHandlerScriptProps[Index].Script->SetUsageIndex(Index); // synchronize usage index with entry in array

		UNiagaraNodeOutput* OutputNode = FNiagaraStackGraphUtilities::ResetGraphForOutput(*Graph, ENiagaraScriptUsage::ParticleEventScript, Index);

		// Set the emitter here so that the internal state of the view model is updated.
		// TODO: Move the logic for managing event handlers into the emitter view model or script view model.
		GetEmitterViewModel()->GetSharedScriptViewModel()->SetScripts(Emitter);

		return OutputNode;
	}
	return nullptr;
}

FText UNiagaraStackAddEventScriptItem::GetInsertTransactionText() const
{
	return LOCTEXT("InsertNewEventScript", "Insert new EventScript");
}

#undef LOCTEXT_NAMESPACE