// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackSpacer.h"

void UNiagaraStackSpacer::Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, FName InSpacerKey)
{
	Super::Initialize(InSystemViewModel, InEmitterViewModel);
	SpacerKey = InSpacerKey;
}

FText UNiagaraStackSpacer::GetDisplayName() const
{
	return FText();
}

FName UNiagaraStackSpacer::GetItemBackgroundName() const
{
	return "NiagaraEditor.Stack.Group.BackgroundColor";
}

FName UNiagaraStackSpacer::GetSpacerKey() const
{
	return SpacerKey;
}