// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackItemExpander.h"
#include "NiagaraStackEditorData.h"
#include "NiagaraEmitterViewModel.h"

void UNiagaraStackItemExpander::Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraStackEditorData& InStackEditorData, FString InEmitterEditorDataKey, bool bInIsExpandedDefault)
{
	Super::Initialize(InSystemViewModel, InEmitterViewModel);
	StackEditorData = &InStackEditorData;
	EmitterEditorDataKey = InEmitterEditorDataKey;
	bIsExpandedDefault = bInIsExpandedDefault;
}

void UNiagaraStackItemExpander::SetOnExpnadedChanged(FOnExpandedChanged OnExpandedChanged)
{
	ExpandedChangedDelegate = OnExpandedChanged;
}

bool UNiagaraStackItemExpander::GetIsExpanded() const
{
	return StackEditorData->GetStackEntryIsExpanded(EmitterEditorDataKey, bIsExpandedDefault);
}

void UNiagaraStackItemExpander::ToggleExpanded()
{
	StackEditorData->SetStackEntryIsExpanded(EmitterEditorDataKey, !GetIsExpanded());
	ExpandedChangedDelegate.ExecuteIfBound();
}