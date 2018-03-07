// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraCommon.h"
#include "EdGraph/EdGraph.h"
#include "NiagaraScript.h"
#include "NiagaraGraph.generated.h"

/** This is the type of action that occurred on a given Niagara graph. Note that this should follow from EEdGraphActionType, leaving some slop for growth. */
enum ENiagaraGraphActionType
{
	GRAPHACTION_GenericNeedsRecompile = 0x1 << 16,
};


UCLASS(MinimalAPI)
class UNiagaraGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)override;
	//~ End UObjet Interface
	
	/** Get the source that owns this graph */
	class UNiagaraScriptSource* GetSource() const;

	/** Determine if there are any nodes in this graph.*/
	bool IsEmpty() const { return Nodes.Num() == 0; }
			
	/** Find the first output node bound to the target usage type.*/
	class UNiagaraNodeOutput* FindOutputNode(ENiagaraScriptUsage TargetUsageType, int32 TargetOccurrence = 0) const;

	/** Find all output nodes.*/
	void FindOutputNodes(TArray<UNiagaraNodeOutput*>& OutputNodes) const;
	void FindOutputNodes(ENiagaraScriptUsage TargetUsageType, TArray<UNiagaraNodeOutput*>& OutputNodes) const;

	/** Options for the FindInputNodes function */
	struct FFindInputNodeOptions
	{
		FFindInputNodeOptions()
			: bSort(false)
			, bIncludeParameters(true)
			, bIncludeAttributes(true)
			, bIncludeSystemConstants(true)
			, bFilterDuplicates(false)
			, bFilterByScriptUsage(false)
			, TargetScriptUsage(ENiagaraScriptUsage::Function)
			, TargetOccurence(0)
		{
		}

		/** Whether or not to sort the nodes, defaults to false. */
		bool bSort;
		/** Whether or not to include parameters, defaults to true. */
		bool bIncludeParameters;
		/** Whether or not to include attributes, defaults to true. */
		bool bIncludeAttributes;
		/** Whether or not to include system parameters, defaults to true. */
		bool bIncludeSystemConstants;
		/** Whether of not to filter out duplicate nodes, defaults to false. */
		bool bFilterDuplicates;
		/** Whether or not to limit to nodes connected to an output node of the specified script type.*/
		bool bFilterByScriptUsage;
		/** The specified script usage required for an input.*/
		ENiagaraScriptUsage TargetScriptUsage;
		/** The specified occurrence within the graph of the script usage*/
		int32 TargetOccurence;
	};

	/** Finds input nodes in the graph with. */
	void FindInputNodes(TArray<class UNiagaraNodeInput*>& OutInputNodes, FFindInputNodeOptions Options = FFindInputNodeOptions()) const;

	/** Get an in-order traversal of a graph by the specified target output script usage.*/
	void BuildTraversal(TArray<class UNiagaraNode*>& OutNodesTraversed, ENiagaraScriptUsage TargetUsage, int32 TargetOccurence = 0) const;
	void BuildTraversal(TArray<class UNiagaraNode*>& OutNodesTraversed, UNiagaraNode* FinalNode) const;

	/** Generates a list of unique input and output parameters for when this script is used as a function. */
	void GetParameters(TArray<FNiagaraVariable>& Inputs, TArray<FNiagaraVariable>& Outputs) const;

	void FindReadDataSetNodes(TArray<class UNiagaraNodeReadDataSet*>& OutReadNodes) const;
	void FindWriteDataSetNodes(TArray<class UNiagaraNodeWriteDataSet*>& OutWriteNodes) const;

	/** Returns the index of this variable in the output node of the graph. INDEX_NONE if this is not a valid attribute. */
	int32 GetOutputNodeVariableIndex(const FNiagaraVariable& Attr)const;
	void GetOutputNodeVariables(TArray< FNiagaraVariable >& OutAttributes)const;
	void GetOutputNodeVariables(ENiagaraScriptUsage InTargetScriptUsage, TArray< FNiagaraVariable >& OutAttributes)const;

	bool HasNumericParameters()const;

	/** Signal to listeners that the graph has changed */
	void NotifyGraphNeedsRecompile();

	/** Get all referenced graphs in this specified graph, including this graph. */
	void GetAllReferencedGraphs(TArray<const UNiagaraGraph*>& Graphs) const;

	/** Determine if there are any external dependencies wrt to scripts and ensure that those dependencies are sucked into the existing package.*/
	void SubsumeExternalDependencies(TMap<const UObject*, UObject*>& ExistingConversions);

	/** Determine if another item has been synchronized with this graph.*/
	bool IsOtherSynchronized(const FGuid& InChangeId) const;

	/** Identify that this graph has undergone changes that will require synchronization with a compiled script.*/
	void MarkGraphRequiresSynchronization();

	FGuid GetChangeID() { return ChangeId; }

	/** Walk through the graph for an ParameterMapGet nodes and see if any of them specify a default for VariableName.*/
	UEdGraphPin* FindParameterMapDefaultValuePin(const FString& VariableName);
private:
	
	/** The current change identifier for this graph. Used to sync status with UNiagaraScripts.*/
	UPROPERTY()
	FGuid ChangeId;
};

