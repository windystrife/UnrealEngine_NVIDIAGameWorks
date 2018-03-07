// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "Templates/Tuple.h"

class UNiagaraNodeOutput;
class UEdGraphPin;
class UEdGraphNode;

/** Traverses a Niagara node graph to identify the variables that have been written and read from a parameter map. 
* 	This class is meant to aid in UI and compilation of the graph. There are several main script types and each one interacts
*	slightly differently with the history depending on context.
*/
struct FNiagaraParameterMapHistory
{
public:
	FNiagaraParameterMapHistory();

	/** The variables that have been identified during the traversal. */
	TArray<FNiagaraVariable> Variables;

	/** Are there any warnings that were encountered during the traversal of the graph for a given variable? */
	TArray<FString> PerVariableWarnings;

	/** For each variable that was found, identify the pins that wrote to them in order from first to last write.*/
	TArray<TArray<const UEdGraphPin*> > PerVariableWriteHistory;

	/** For each variable that was found, identify the pins that read them from the map in order from first to last read. First of the pair has the read pin, second of the pair has the last set that wrote to the pin.*/
	TArray<TArray<TTuple<const UEdGraphPin*, const UEdGraphPin*> > > PerVariableReadHistory; 

	/** List of pins that manipulated the parameter map from input to output. */
	TArray<const UEdGraphPin*> MapPinHistory;

	/** List of emitter namespaces encountered as this parameter map was built.*/
	TArray<FString> EmitterNamespacesEncountered;

	/**
	* Called in a depth-first traversal to identify a given Niagara Parameter Map pin that was touched during traversal.
	*/
	int32 RegisterParameterMapPin(const UEdGraphPin* Pin);
		
	/**
	* Find a variable by name with no concern for type.
	*/
	int32 FindVariableByName(const FName& VariableName);

	/**
	* Find a variable by both name and type. 
	*/
	int32 FindVariable(const FName& VariableName, const FNiagaraTypeDefinition& Type);


	/**
	* Add a variable outside the normal get/set node paradigm.
	*/
	int32 AddExternalVariable(const FNiagaraVariable& InVar);

	/**
	* Get the pin that added the parameter map to the graph.
	*/
	const UEdGraphPin* GetOriginalPin() const;

	/**
	* Get the output pin that we traced to build this history object.
	*/
	const UEdGraphPin* GetFinalPin() const;


	/**
	* Use the input alias map to resolve any aliases in this input variable name.
	*/
	static FNiagaraVariable ResolveAliases(const FNiagaraVariable& InVar, const TMap<FString, FString>& InAliases, const TCHAR* InJoinSeparator);
		
	/**
	* Remove the Particles namespace if it exists.
	*/
	static FNiagaraVariable ResolveAsBasicAttribute(const FNiagaraVariable& InVar);


	/**
	* Reverses ResolveAsBasicAttribute.
	*/
	static FNiagaraVariable BasicAttributeToNamespacedAttribute(const FNiagaraVariable& InVar);

	
	/** Prepends the namespace string to the variable name.*/
	static FNiagaraVariable VariableToNamespacedVariable(const FNiagaraVariable& InVar, FString Namespace);
		
	/**
	* Does this parameter start with the "Module" namespace? Note that the emitter namespace is an alias
	* for all non-funcion/module script types and will be specialized to the function call node's name using the module.
	*/
	static bool IsAliasedModuleParameter(const FNiagaraVariable& InVar);

	/**
	* Does this parameter start with the "Emitter" namespace? Note that the emitter namespace is an alias
	* for Emitter and System script types and will be specialized to the name of that specific emitter.
	*/
	static bool IsAliasedEmitterParameter(const FNiagaraVariable& InVar);
	/** Is this parameter in the special "System" namespace?*/
	static bool IsSystemParameter(const FNiagaraVariable& InVar);
	/** Is this parameter in the special "Engine" namespace?*/
	static bool IsEngineParameter(const FNiagaraVariable& InVar);
	static bool IsUserParameter(const FNiagaraVariable& InVar);
	
	/** Take an input string and make it hlsl safe.*/
	static FString MakeSafeNamespaceString(const FString& InStr);

	/** Does the variable start with this namespace?*/
	static bool IsInNamespace(const FNiagaraVariable& InVar, const FString& Namespace);

	/** Given the script type and flags, should we treat the System namespace as read-only?*/
	static bool IsSystemNamespaceReadOnly(UNiagaraScript* InScript);

	/** Given the script type and flags, should we treat the Emitter aliased namespace as read-only?*/
	static bool IsEmitterNamespaceReadOnly(UNiagaraScript* InScript);

	/** Called to determine if a given variable should be output from a script. It is not static as it requires the overall context to include emitter namespaces visited for system scripts.*/
	bool IsPrimaryDataSetOutput(const FNiagaraVariable& InVar, UNiagaraScript* InScript);
	bool IsPrimaryDataSetOutput(const FNiagaraVariable& InVar, ENiagaraScriptUsage InUsage);

	/** Are we required to export this variable as an external constant?*/
	bool IsExportableExternalConstant(const FNiagaraVariable& InVar, UNiagaraScript* InScript);

	/** Does this variable belong in a namespace that needs to come in as an external constant to this script?*/
	static bool IsExternalConstantNamespace(const FNiagaraVariable& InVar, UNiagaraScript* InScript);
	
	/** Take a non-namespaced variable and move it to an appropriate external constant namespace for this script type.*/
	static FNiagaraVariable MoveToExternalConstantNamespaceVariable(const FNiagaraVariable& InVar, UNiagaraScript* InScript);
	
	/**
	* Does this parameter start with the "Particles" namespace?
	*/
	static bool IsAttribute(const FNiagaraVariable& InVar);
	
	/** Does this parameter contain the "Initial" namespace as one of its intermediate namespaces?*/
	static bool IsInitialValue(const FNiagaraVariable& InVar);

	/** Get the output node associated with this graph.*/
	const UNiagaraNodeOutput* GetFinalOutputNode() const;

	/** Does this parameter contain the "Initial" namespace as one of its intermediate namespaces? If so, remove the "Initial" namespace and return the original value.*/
	static FNiagaraVariable GetSourceForInitialValue(const FNiagaraVariable& InVar);

	/**
	* Helper to add a variable to the known list for a parameter map.
	*/
	int32 AddVariable(const FNiagaraVariable& InVar, const UEdGraphPin* InPin);

	/** Get the default value for this variable.*/
	const UEdGraphPin* GetDefaultValuePin(int32 VarIdx) const;


};


class FNiagaraParameterMapHistoryBuilder
{
public:
	/** Collection of the build histories from the graph traversal.*/
	TArray<FNiagaraParameterMapHistory> Histories;

	/** Constructor*/
	FNiagaraParameterMapHistoryBuilder();

	/** Add a new parameter map to the array.*/
	int32 CreateParameterMap();

	/** Called in order to set up the correct initial context for an Output node and invokes the UNiagaraNodeOutput's BuildParameterMapHistory method.*/
	void BuildParameterMaps(UNiagaraNodeOutput* OutputNode, bool bRecursive = true);

	/**
	* Called first during a node's visitation during traversal to identify that a node has been visited.
	*/
	void RegisterNodeVisitation(const UEdGraphNode* Node);

	/** Important. Must be called for each routing of the parameter map. This feeds the list used by TraceParameterMapOutputPin.*/
	int32 RegisterParameterMapPin(int32 WhichParameterMap, const UEdGraphPin* Pin);

	/** Trace back a pin to whom it was connected to to find the current parameter map to use.*/
	int32 TraceParameterMapOutputPin(const UEdGraphPin* OutputPin);

	void BeginTranslation(UNiagaraScript* Script);

	void EndTranslation(UNiagaraScript* Script);

	void BeginTranslation(UNiagaraEmitter* Emitter);

	void EndTranslation(UNiagaraEmitter* Emitter);

	/**
	* Record that we have entered a new function scope.
	*/
	void EnterFunction(const FString& InNodeName, class UNiagaraScript* InScript, class UNiagaraNode* Node);

	/**
	* Record that we have exited a function scope.
	*/
	void ExitFunction(const FString& InNodeName, class UNiagaraScript* InScript, class UNiagaraNode* Node);

	/**
	* Record that we have entered an emitter scope.
	*/
	void EnterEmitter(const FString& InEmitterName, class UNiagaraNode* Node);

	/**
	* Record that we have exited an emitter scope.
	*/
	void ExitEmitter(const FString& InEmitterName, class UNiagaraNode* Node);

	/**
	* Use the current alias map to resolve any aliases in this input variable name.
	*/
	FNiagaraVariable ResolveAliases(const FNiagaraVariable& InVar);

	/**
	* Has RegisterNodeVisitation been called yet on the owning node of this pin?
	*/
	bool GetPinPreviouslyVisited(const UEdGraphPin* InPin) const;

	/**
	* Has RegisterNodeVisitation been called on the input node yet?
	*/
	bool GetNodePreviouslyVisited(const class UNiagaraNode* Node) const;

	/** If we haven't already visited the owning nodes, do so.*/
	void VisitInputPins(class UNiagaraNode*);
	
	/** If we haven't already visited the owning node, do so.*/
	void VisitInputPin(const UEdGraphPin* Pin, class UNiagaraNode*);

	/**
	* Record that a pin writes to the parameter map. The pin name is expected to be the namespaced parameter map version of the name. If any aliases are in place, they are removed.
	*/
	int32 HandleVariableWrite(int32 ParameterMapIndex, const UEdGraphPin* InPin);

	/**
	* Record that a pin reads from the parameter map. The pin name is expected to be the namespaced parameter map version of the name. If any aliases are in place, they are removed.
	*/
	int32 HandleVariableRead(int32 ParameterMapIndex, const UEdGraphPin* InPin, bool RegisterReadsAsVariables, const UEdGraphPin* InDefaultPin, bool& OutUsedDefault);
	
	/**
	* Get the string that the "Module" namespace maps to currently (if it exists)
	*/
	const FString* GetModuleAlias() const;

	/**
	* Get the string that the "Emitter" namespace maps to currently (if it exists)
	*/
	const FString* GetEmitterAlias() const;

	/** Get the node calling this sub-graph.*/
	UNiagaraNode* GetCallingContext() const;

	/** Helper method to identify any matching input nodes from the calling context node to the input variable.*/
	int32 FindMatchingParameterMapFromContextInputs(const FNiagaraVariable& InVar) const;

	/** In some cases, we don't want all the variables encountered in a traversal. In this case, you can filter 
	*  the map history to only include variables that are relevant to the specific script type. For instance, a System 
	*  script doesn't really care about the Particles namespace.
	*/
	void EnableScriptWhitelist(bool bInEnable, ENiagaraScriptUsage InScriptType);

	ENiagaraScriptUsage GetCurrentUsageContext()const;

protected:
	/**
	* Generate the internal alias map from the current traversal state.
	*/
	void BuildCurrentAliases();

	/** Helper function called when variables are added to enable the filtering specified in EnableScriptWhitelist.*/
	bool ShouldTrackVariable(const FNiagaraVariable& InVar);

	/** Helper method used to take in input script type and determine if the passed in namespaced variable is worth tracking.*/
	bool IsNamespacedVariableRelevantToScriptType(const FNiagaraVariable& InVar, ENiagaraScriptUsage ScriptType);

	/** Contains the hierarchy of nodes leading to the current graph being processed. Usually made up of FunctionCall and Emitter nodes.*/
	TArray<UNiagaraNode*> CallingContext;
	/** Tracker for each context level of the parameter map index associated with a given pin. Used to trace parameter maps through the graph.*/
	TArray<TMap<const UEdGraphPin*, int32> > PinToParameterMapIndices;
	/** List of previously visited nodes per context. Note that the same node may be visited multiple times across all graph traversals, but only one time per context level.*/
	TArray<TArray<const class UNiagaraNode*> > ContextuallyVisitedNodes;
	/** Contains the hierarchy of emitter node names leading to the current graph being processed.*/
	TArray<FName> EmitterNameContextStack;
	/** Contains the hierarchy of function call node names leading to the current graph being processed.*/
	TArray<FName> FunctionNameContextStack;
	/** Keeps track of the script usage at the current context level. This allows us to make some decisions about relevence.*/
	TArray<ENiagaraScriptUsage> RelevantScriptUsageContext;
	/** Resolved alias map for the current context level. Rebuilt by BuildCurrentAliases.*/
	TMap<FString, FString> AliasMap;
	
	/** Whether or not the script whitelist is active.*/
	bool bFilterByScriptWhitelist;
	/** What the script type is that we should be filtering to if the whitelist is enabled.*/
	ENiagaraScriptUsage FilterScriptType;
};