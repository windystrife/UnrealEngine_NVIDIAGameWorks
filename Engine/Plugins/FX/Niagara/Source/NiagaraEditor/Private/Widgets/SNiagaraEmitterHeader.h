// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SharedPointer.h"
#include "SCompoundWidget.h"
#include "DeclarativeSyntaxSupport.h"

class FNiagaraEmitterHandleViewModel;

/** A widget for viewing and editing header information for an emitter. */
class SNiagaraEmitterHeader : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraEmitterHeader)
	{}

		/** A slot which can be used to add additional content for the header. */
		SLATE_NAMED_SLOT(FArguments, AdditionalHeaderContent)

	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedRef<FNiagaraEmitterHandleViewModel> InViewModel);

	//~ SWidget interface
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

private:
	/** The view model which exposes the data used by the widget. */
	TSharedPtr<FNiagaraEmitterHandleViewModel> ViewModel;
};