// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphPin.h"
#include "NiagaraNodeParameterMapBase.h"
#include "NiagaraNodeParameterMapSet.generated.h"

class UEdGraphPin;


/** A node that allows a user to set multiple values into a parameter map. */
UCLASS()
class UNiagaraNodeParameterMapSet : public UNiagaraNodeParameterMapBase
{
public:
	GENERATED_BODY()

	UNiagaraNodeParameterMapSet();

	virtual void AllocateDefaultPins() override;

	virtual bool IsPinNameEditable(const UEdGraphPin* GraphPinObj) const override;
	virtual bool IsPinNameEditableUponCreation(const UEdGraphPin* GraphPinObj) const override;
	virtual bool VerifyEditablePinName(const FText& InName, FText& OutErrorMessage, const UEdGraphPin* InGraphPinObj) const override;
	virtual bool CommitEditablePinName(const FText& InName, UEdGraphPin* InGraphPinObj)  override;

	virtual void BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive = true) override;

	virtual void Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs);

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const;
protected:
	virtual void OnNewTypedPinAdded(UEdGraphPin* NewPin); 
	virtual void OnPinRenamed(UEdGraphPin* RenamedPin) override;
	UEdGraphPin* PinPendingRename;

};
