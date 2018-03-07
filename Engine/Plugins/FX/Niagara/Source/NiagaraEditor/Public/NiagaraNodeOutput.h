// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraCommon.h"
#include "NiagaraEditorCommon.h"
#include "NiagaraNode.h"
#include "NiagaraScript.h"
#include "NiagaraNodeOutput.generated.h"

UCLASS(MinimalAPI)
class UNiagaraNodeOutput : public UNiagaraNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Output)
	TArray<FNiagaraVariable> Outputs;

	UPROPERTY()
	ENiagaraScriptUsage ScriptType;

	UPROPERTY()
	int32 ScriptTypeIndex;

public:

	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	//~ End UObject Interface

	//~ Begin EdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual bool CanUserDeleteNode() const override;
	virtual bool CanDuplicateNode() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
	//~ End EdGraphNode Interface

	/** Notifies the node that it's output variables have been modified externally. */
	void NotifyOutputVariablesChanged();

	virtual void Compile(class FHlslNiagaraTranslator *Translator, TArray<int32>& OutputExpressions)override;
	const TArray<FNiagaraVariable>& GetOutputs() const {return Outputs;}
	
	/** Gets the usage of this graph root. */
	ENiagaraScriptUsage GetUsage() const { return ScriptType; }
	void SetUsage(ENiagaraScriptUsage InUsage) { ScriptType = InUsage; }

	/** Gets the usage index of this graph root. */
	int32 GetUsageIndex() const { return ScriptTypeIndex; }
	void SetUsageIndex(int32 InIndex) { ScriptTypeIndex = InIndex; }
protected:
	virtual int32 CompileInputPin(class FHlslNiagaraTranslator *Translator, UEdGraphPin* Pin) override;

	/** Removes a pin from this node with a transaction. */
	virtual void RemoveOutputPin(UEdGraphPin* Pin);

private:

	/** Gets the display text for a pin. */
	FText GetPinNameText(UEdGraphPin* Pin) const;

	/** Called when a pin's name text is committed. */
	void PinNameTextCommitted(const FText& Text, ETextCommit::Type CommitType, UEdGraphPin* Pin);
};

