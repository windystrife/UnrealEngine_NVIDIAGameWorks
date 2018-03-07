// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraEditorCommon.h"
#include "NiagaraNode.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeFunctionCall.generated.h"

class UNiagaraScript;

UCLASS(MinimalAPI)
class UNiagaraNodeFunctionCall : public UNiagaraNode
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Function")
	UNiagaraScript* FunctionScript;

	/** 
	 * A path to a script asset which can be used to assign the function script the first time that
	 * default pins are generated. This is used so that the function nodes can be populated in the graph context
	 * menu without having to load all of the actual script assets.
	 */
	UPROPERTY(Transient)
	FName FunctionScriptAssetObjectPath;

	/** Some functions can be provided a signature directly rather than a script. */
	UPROPERTY()
	FNiagaraFunctionSignature Signature;

	//Begin UObject interface
	virtual void PostLoad()override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)override;
	//End UObject interface

	//~ Begin UNiagaraNode Interface
	virtual void Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs)override;
	virtual UObject* GetReferencedAsset() const override;
	virtual bool RefreshFromExternalChanges() override;
	virtual ENiagaraNumericOutputTypeSelectionMode GetNumericOutputTypeSelectionMode() const override;
	virtual bool CanAddToGraph(UNiagaraGraph* TargetGraph, FString& OutErrorMsg) const override;
	virtual void SubsumeExternalDependencies(TMap<const UObject*, UObject*>& ExistingConversions) override;
	//End UNiagaraNode interface

	//~ Begin EdGraphNode Interface
	virtual void AutowireNewNode(UEdGraphPin* FromPin)override;
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	//~ End EdGraphNode Interface

	bool FindAutoBoundInput(UNiagaraNodeInput* InputNode, UEdGraphPin* PinToAutoBind, FNiagaraVariable& OutFoundVar, ENiagaraInputNodeUsage& OutNodeUsage);

	virtual void BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive = true) override;

	FString GetFunctionName() const { return FunctionDisplayName; }

	/** Walk through the internal script graph for an ParameterMapGet nodes and see if any of them specify a default for VariableName.*/
	UEdGraphPin* FindParameterMapDefaultValuePin(const FString& VariableName);

protected:
	/** Resets the node name based on the referenced script or signature. Guaranteed unique within a given graph instance.*/
	void ComputeNodeName();

	/** Adjusted every time that we compile this script. Lets us know that we might differ from any cached versions.*/
	UPROPERTY()
	FGuid CachedChangeId;

	UPROPERTY()
	FString FunctionDisplayName;
};

