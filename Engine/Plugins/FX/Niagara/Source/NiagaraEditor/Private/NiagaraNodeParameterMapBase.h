// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraNodeWithDynamicPins.h"
#include "SGraphPin.h"
#include "NiagaraNodeParameterMapBase.generated.h"

class UEdGraphPin;


/** A node which allows the user to build a set of arbitrary output types from an arbitrary set of input types by connecting their inner components. */
UCLASS()
class UNiagaraNodeParameterMapBase : public UNiagaraNodeWithDynamicPins
{
public:
	GENERATED_BODY()
	UNiagaraNodeParameterMapBase();

	/** Traverse the graph looking for the history of the parameter map specified by the input pin. This will return the list of variables discovered, any per-variable warnings (type mismatches, etc)
		encountered per variable, and an array of pins encountered in order of traversal outward from the input pin.
	*/
	static TArray<FNiagaraParameterMapHistory> GetParameterMaps(UNiagaraNodeOutput* InGraphEnd, bool bLimitToOutputScriptType = false, FString EmitterNameOverride = TEXT(""));
	static TArray<FNiagaraParameterMapHistory> GetParameterMaps(UNiagaraGraph* InGraph, FString EmitterNameOverride = TEXT(""));
	static TArray<FNiagaraParameterMapHistory> GetParameterMaps(class UNiagaraScriptSourceBase* InSource, FString EmitterNameOverride = TEXT(""));

	virtual bool AllowNiagaraTypeForAddPin(const FNiagaraTypeDefinition& InType) override;
	
	virtual TSharedRef<SWidget> GenerateAddPinMenu(const FString& InWorkingPinName, SNiagaraGraphPinAdd* InPin) override;

protected:
	virtual void BuildCommonMenu(FMenuBuilder& InMenuBuilder, const FString& InWorkingName, SNiagaraGraphPinAdd* InPin);
	virtual void BuildLocalMenu(FMenuBuilder& InMenuBuilder, const FString& InWorkingName, SNiagaraGraphPinAdd* InPin);
	virtual void BuildEngineMenu(FMenuBuilder& InMenuBuilder, const FString& InWorkingName, SNiagaraGraphPinAdd* InPin);
};
