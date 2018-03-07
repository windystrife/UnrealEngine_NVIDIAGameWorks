// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "StructOnScope.h"
#include "Attribute.h"

class UNiagaraNodeInput;
struct FNiagaraVariable;
struct FNiagaraTypeDefinition;
class UNiagaraGraph;
class UNiagaraSystem;
class UNiagaraEmitter;
class UNiagaraScript;
class FStructOnScope;
class UEdGraph;
class UEdGraphNode;
class SWidget;

namespace FNiagaraEditorUtilities
{
	/** Determines if the contents of two sets matches */
	// TODO: Move this to TSet.
	template<typename ElementType>
	bool SetsMatch(const TSet<ElementType>& SetA, const TSet<ElementType>& SetB)
	{
		if (SetA.Num() != SetB.Num())
		{
			return false;
		}

		for (ElementType SetItemA : SetA)
		{
			if (SetB.Contains(SetItemA) == false)
			{
				return false;
			}
		}

		return true;
	}

	/** Determines if the contents of an array matches a set */
	// TODO: Move this to TSet.
	template<typename ElementType>
	bool ArrayMatchesSet(const TArray<ElementType>& Array, const TSet<ElementType>& Set)
	{
		if (Array.Num() != Set.Num())
		{
			return false;
		}

		for (ElementType ArrayItem : Array)
		{
			if (Set.Contains(ArrayItem) == false)
			{
				return false;
			}
		}

		return true;
	}

	/** Builds a unique name from a candidate name and a set of existing names.  The candidate name will be made unique 
		if necessary by adding a 3 digit index to the end. */
	FName GetUniqueName(FName CandidateName, const TSet<FName>& ExistingNames);

	/** Gets a set of the system constant names. */
	TSet<FName> GetSystemConstantNames();

	/** Resets the variables value to default, either based on the struct, or if available through registered type utilities. */
	void ResetVariableToDefaultValue(FNiagaraVariable& Variable);

	/** Fills DefaultData with the types default, either based on the struct, or if available through registered type utilities. */
	void NIAGARAEDITOR_API GetTypeDefaultValue(const FNiagaraTypeDefinition& Type, TArray<uint8>& DefaultData);

	/** Sets up a niagara input node for parameter usage. */
	void InitializeParameterInputNode(UNiagaraNodeInput& InputNode, const FNiagaraTypeDefinition& Type, const UNiagaraGraph* Graph, FName InputName = FName(TEXT("NewInput")));

	/** Options for the GetParameterVariablesFromSystem function. */
	struct FGetParameterVariablesFromSystemOptions
	{
		FGetParameterVariablesFromSystemOptions()
			: bIncludeStructParameters(true)
			, bIncludeDataInterfaceParameters(true)
		{
		}

		bool bIncludeStructParameters;
		bool bIncludeDataInterfaceParameters;
	};

	/** Gets the niagara variables for the input parameters on a niagara System. */
	void GetParameterVariablesFromSystem(UNiagaraSystem& System, TArray<FNiagaraVariable>& ParameterVariables, FGetParameterVariablesFromSystemOptions Options = FGetParameterVariablesFromSystemOptions());

	/** Generates a merged graph for an Emitter properties object that previously had unique graphs per script type.*/
	bool ConvertToMergedGraph(UNiagaraEmitter* InEmitter);

	/** Helper to clean up copy & pasted graphs.*/
	void FixUpPastedInputNodes(UEdGraph* Graph, TSet<UEdGraphNode*> PastedNodes);

	/** Helper to convert compile status to text.*/
	FText StatusToText(ENiagaraScriptCompileStatus Status);

	/** Helper method to union two distinct compiler statuses.*/
	ENiagaraScriptCompileStatus UnionCompileStatus(const ENiagaraScriptCompileStatus& StatusA, const ENiagaraScriptCompileStatus& StatusB);

	/** Returns whether the data in a niagara variable and a struct on scope match */
	bool DataMatches(const FNiagaraVariable& Variable, const FStructOnScope& StructOnScope);

	/** Returns whether the data in two structs on scope matches. */
	bool DataMatches(const FStructOnScope& StructOnScopeA, const FStructOnScope& StructOnScopeB);

	TSharedPtr<SWidget> CreateInlineErrorText(TAttribute<FText> ErrorMessage, TAttribute<FText> ErrorTooltip);

	void UpdateExistingEmitters(const TArray<UNiagaraEmitter*>& AffectedEmitters);
}