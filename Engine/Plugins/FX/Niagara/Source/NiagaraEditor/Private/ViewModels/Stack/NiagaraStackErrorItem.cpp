// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackErrorItem.h"

UNiagaraStackErrorItem::UNiagaraStackErrorItem() : ErrorSource(nullptr), ErrorIdx(INDEX_NONE)
{
}

void UNiagaraStackErrorItem::Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraStackEntry* InErrorSource, int32 InErrorIndex)
{
	checkf(ErrorSource == nullptr, TEXT("Can only initialize once."));
	Super::Initialize(InSystemViewModel, InEmitterViewModel);
	ErrorSource = InErrorSource;
	ErrorIdx = InErrorIndex;
}

int32 UNiagaraStackErrorItem::GetItemIndentLevel() const
{
	return ItemIndentLevel;
}

void UNiagaraStackErrorItem::SetItemIndentLevel(int32 InItemIndentLevel)
{
	ItemIndentLevel = InItemIndentLevel;
}

FText UNiagaraStackErrorItem::ErrorText() const
{
	FText SummaryText = ErrorSource->GetErrorSummaryText(ErrorIdx);
	if (SummaryText.IsEmpty())
	{
		return ErrorSource->GetErrorText(ErrorIdx);
	}
	else
	{
		return SummaryText;
	}
}

FText UNiagaraStackErrorItem::ErrorTextTooltip() const
{
	return ErrorSource->GetErrorText(ErrorIdx);
}

FReply UNiagaraStackErrorItem::OnTryFixError()
{
	if (ErrorSource->TryFixError(ErrorIdx))
	{
		return FReply::Handled();
	}
	return FReply::Handled();
}

EVisibility UNiagaraStackErrorItem::CanFixVisibility() const
{
	if (ErrorSource->GetErrorFixable(ErrorIdx))
	{
		return EVisibility::Visible;
	}
	else
	{
		return EVisibility::Hidden;
	}
}


FName UNiagaraStackErrorItem::GetItemBackgroundName() const 
{
	return "NiagaraEditor.Stack.Item.ErrorBackgroundColor";
}