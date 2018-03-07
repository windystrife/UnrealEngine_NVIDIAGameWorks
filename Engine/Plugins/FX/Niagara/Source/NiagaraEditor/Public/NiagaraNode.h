// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNode.h"
#include "NiagaraEditorCommon.h"
#include "NiagaraParameterMapHistory.h"
#include "NiagaraNode.generated.h"

class UEdGraphPin;
class INiagaraCompiler;

UCLASS()
class NIAGARAEDITOR_API UNiagaraNode : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()
protected:

	bool ReallocatePins();

	bool CompileInputPins(class FHlslNiagaraTranslator *Translator, TArray<int32>& OutCompiledInputs);

public:

	//~ Begin UObject interface
	virtual void PostLoad() override;
	//~ End UObject interface

	//~ Begin EdGraphNode Interface
	virtual void AutowireNewNode(UEdGraphPin* FromPin)override;	
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void PinTypeChanged(UEdGraphPin* Pin) override;
	//~ End EdGraphNode Interface

	/** Get the Niagara graph that owns this node */
	const class UNiagaraGraph* GetNiagaraGraph()const;
	class UNiagaraGraph* GetNiagaraGraph();

	/** Get the source object */
	class UNiagaraScriptSource* GetSource()const;

	/** Gets the asset referenced by this node, or nullptr if there isn't one. */
	virtual UObject* GetReferencedAsset() const { return nullptr; }

	/** Refreshes the node due to external changes, e.g. the underlying function changed for a function call node. Return true if the graph changed. */
	virtual bool RefreshFromExternalChanges() { return false; }

	virtual void Compile(class FHlslNiagaraTranslator *Translator, TArray<int32>& Outputs);

	UEdGraphPin* GetInputPin(int32 InputIndex) const;
	void GetInputPins(TArray<class UEdGraphPin*>& OutInputPins) const;
	UEdGraphPin* GetOutputPin(int32 OutputIndex) const;
	void GetOutputPins(TArray<class UEdGraphPin*>& OutOutputPins) const;

	/** Apply any node-specific logic to determine if it is safe to add this node to the graph. This is meant to be called only in the Editor before placing the node.*/
	virtual bool CanAddToGraph(UNiagaraGraph* TargetGraph, FString& OutErrorMsg) const;

	/** Gets which mode to use when deducing the type of numeric output pins from the types of the input pins. */
	virtual ENiagaraNumericOutputTypeSelectionMode GetNumericOutputTypeSelectionMode() const;

	/** Convert the type of an existing numeric pin to a more known type.*/
	virtual bool ConvertNumericPinToType(UEdGraphPin* InGraphPin, FNiagaraTypeDefinition TypeDef);

	/** Determine if there are any external dependencies wrt to scripts and ensure that those dependencies are sucked into the existing package.*/
	virtual void SubsumeExternalDependencies(TMap<const UObject*, UObject*>& ExistingConversions) {};

	/** Determine whether or not a pin should be renamable. */
	virtual bool IsPinNameEditable(const UEdGraphPin* GraphPinObj) const { return false; }

	/** Determine whether or not a specific pin should immediately be opened for rename.*/
	virtual bool IsPinNameEditableUponCreation(const UEdGraphPin* GraphPinObj) const { return false; }

	/** Verify that the potential rename has produced acceptable results for a pin.*/
	virtual bool VerifyEditablePinName(const FText& InName, FText& OutErrorMessage, const UEdGraphPin* InGraphPinObj) const { return false; }

	/** Verify that the potential rename has produced acceptable results for a pin.*/
	virtual bool CommitEditablePinName(const FText& InName,  UEdGraphPin* InGraphPinObj) { return false; }
	
	virtual void BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive = true);

	bool ShouldVisit(int32 InVisitID) 
	{ 
		bool bShouldVisit = InVisitID != VisitID;
		VisitID = InVisitID;
		return bShouldVisit;
	}
	
protected:
	virtual int32 CompileInputPin(class FHlslNiagaraTranslator *Translator, UEdGraphPin* Pin);

	//ID for the most recent visitor. Allows faster tracking of what nodes have been visited.
	int32 VisitID;
};
