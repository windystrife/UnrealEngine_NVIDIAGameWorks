// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCompoundWidget.h"
#include "DeclarativeSyntaxSupport.h"
#include "UObject/GCObject.h"

class FNiagaraSystemViewModel;
class UNiagaraStackViewModel;
class SNiagaraStack;
class SBox;

class SNiagaraSelectedEmitterHandle : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SNiagaraSelectedEmitterHandle)
	{}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedRef<FNiagaraSystemViewModel> InSystemViewModel);

	~SNiagaraSelectedEmitterHandle();

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	void SelectedEmitterHandlesChanged();

	void RefreshEmitterWidgets();

	EVisibility GetUnsupportedSelectionTextVisibility() const;

	FText GetUnsupportedSelectionText() const;

private:
	TSharedPtr<FNiagaraSystemViewModel> SystemViewModel;

	UNiagaraStackViewModel* StackViewModel;

	TSharedPtr<SBox> EmitterHeaderContainer;
	TSharedPtr<SNiagaraStack> NiagaraStack;
};