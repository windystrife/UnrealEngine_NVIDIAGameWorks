// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackAddRendererItem.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScript.h"
#include "NiagaraEmitter.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraStackRendererItem.h"
#include "NiagaraEmitterViewModel.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "ScopedTransaction.h"
#include "EdGraph/EdGraph.h"

#define LOCTEXT_NAMESPACE "NiagaraStackViewModel"

FText UNiagaraStackAddRendererItem::GetDisplayName() const
{
	return FText();
}

void UNiagaraStackAddRendererItem::SetOnItemAdded(FOnItemAdded InOnItemAdded)
{
	ItemAddedDelegate = InOnItemAdded;
}

void UNiagaraStackAddRendererItem::AddRenderer(UClass* InRendererClass)
{
	FScopedTransaction ScopedTransaction(LOCTEXT("InsertNewRenderer", "Insert new renderer"));

	UNiagaraEmitter* Emitter = GetEmitterViewModel()->GetEmitter();
	Emitter->Modify();
	UNiagaraRendererProperties* RendererProperties = NewObject<UNiagaraRendererProperties>(Emitter, InRendererClass, NAME_None, RF_Transactional);
	Emitter->RendererProperties.Add(RendererProperties);

	bool bVarsAdded = false;
	TArray<FNiagaraVariable> MissingAttributes = UNiagaraStackRendererItem::GetMissingVariables(RendererProperties, Emitter);
	for (int32 i = 0; i < MissingAttributes.Num(); i++)
	{
		if (UNiagaraStackRendererItem::AddMissingVariable(Emitter, MissingAttributes[i]))
		{
			bVarsAdded = true;
		}
	}

	if (bVarsAdded)
	{
		FNotificationInfo Info(LOCTEXT("AddedVariables","One or more variables have been added to the Spawn script to support the added renderer."));
		Info.ExpireDuration = 5.0f;
		Info.bFireAndForget = true;
		Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Info"));
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	ItemAddedDelegate.ExecuteIfBound();
}



#undef LOCTEXT_NAMESPACE