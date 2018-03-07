// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraParameterEditor.h"

void SNiagaraParameterEditor::SetOnBeginValueChange(FOnValueChange InOnBeginValueChange)
{
	OnBeginValueChange = InOnBeginValueChange;
}

void SNiagaraParameterEditor::SetOnEndValueChange(FOnValueChange InOnEndValueChange)
{
	OnEndValueChange = InOnEndValueChange;
}

void SNiagaraParameterEditor::SetOnValueChanged(FOnValueChange InOnValueChanged)
{
	OnValueChanged = InOnValueChanged;
}

bool SNiagaraParameterEditor::GetIsEditingExclusively()
{
	return bIsEditingExclusively;
}

void SNiagaraParameterEditor::SetIsEditingExclusively(bool bInIsEditingExclusively)
{
	bIsEditingExclusively = bInIsEditingExclusively;
}

void SNiagaraParameterEditor::ExecuteOnBeginValueChange()
{
	OnBeginValueChange.ExecuteIfBound();
}

void SNiagaraParameterEditor::ExecuteOnEndValueChange()
{
	OnEndValueChange.ExecuteIfBound();
}

void SNiagaraParameterEditor::ExecuteOnValueChanged()
{
	OnValueChanged.ExecuteIfBound();
}