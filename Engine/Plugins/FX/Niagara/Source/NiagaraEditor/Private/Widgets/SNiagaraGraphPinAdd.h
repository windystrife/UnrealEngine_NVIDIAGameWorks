// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphPin.h"
#include "NiagaraTypes.h"

class UNiagaraNodeWithDynamicPins;
struct FNiagaraTypeDefinition;

/** A graph pin for adding additional pins a dynamic niagara node. */
class SNiagaraGraphPinAdd : public SGraphPin
{
	SLATE_BEGIN_ARGS(SNiagaraGraphPinAdd) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);
	
	void OnAddType(FNiagaraVariable InAdd);

private:
	TSharedRef<SWidget>	ConstructAddButton();

	TSharedRef<SWidget> OnGetAddButtonMenuContent();
	

private:
	UNiagaraNodeWithDynamicPins* OwningNode;
};