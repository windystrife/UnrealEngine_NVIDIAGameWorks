// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "NiagaraParameterMapHistory.h"

#include "NiagaraEditorCommon.h"
#include "AssetData.h"
#include "NiagaraHlslTranslator.h"
#include "NiagaraSystem.h"

#include "ActorFactoryNiagara.h"
#include "NiagaraActor.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraNode.h"
#include "NiagaraCompiler.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeEmitter.h"
#include "NiagaraNodeParameterMapGet.h"
#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

#define LOCTEXT_NAMESPACE "NiagaraEditor"

#define PARAM_MAP_ENGINE_STR TEXT("Engine.")
#define PARAM_MAP_USER_STR TEXT("User.")
#define PARAM_MAP_SYSTEM_STR TEXT("System.")
#define PARAM_MAP_EMITTER_STR TEXT("Emitter.")
#define PARAM_MAP_MODULE_STR TEXT("Module.")
#define PARAM_MAP_ATTRIBUTE_STR TEXT("Particles.")
#define PARAM_MAP_INITIAL_STR TEXT("Initial.")
#define PARAM_MAP_INITIAL_BASE_STR TEXT("Initial")


FNiagaraParameterMapHistory::FNiagaraParameterMapHistory() 
{
	
}

int32 FNiagaraParameterMapHistory::RegisterParameterMapPin(const UEdGraphPin* Pin)
{
	int32 RetIdx =  MapPinHistory.Add(Pin);
	return RetIdx;
}

int32 FNiagaraParameterMapHistory::FindVariableByName(const FName& VariableName)
{
	int32 FoundIdx = Variables.IndexOfByPredicate([&](const FNiagaraVariable& InObj) -> bool
	{
		return (InObj.GetName() == VariableName);
	});

	return FoundIdx;
}

int32 FNiagaraParameterMapHistory::FindVariable(const FName& VariableName, const FNiagaraTypeDefinition& Type)
{
	int32 FoundIdx = Variables.IndexOfByPredicate([&](const FNiagaraVariable& InObj) -> bool
	{
		return (InObj.GetName() == VariableName && InObj.GetType() == Type);
	});

	return FoundIdx;
}

int32 FNiagaraParameterMapHistory::AddVariable(const FNiagaraVariable& InVar, const UEdGraphPin* InPin)
{
	FNiagaraVariable Var = InVar;
	
	int32 FoundIdx = FindVariable(Var.GetName(), Var.GetType());
	if (FoundIdx == -1)
	{
		FoundIdx = Variables.Add(Var);
		PerVariableWarnings.AddDefaulted(1);
		PerVariableWriteHistory.AddDefaulted(1);
		PerVariableReadHistory.AddDefaulted(1);

		if (InPin != nullptr)
		{
			PerVariableWriteHistory[FoundIdx].Add(InPin);
		}

		check(Variables.Num() == PerVariableWarnings.Num());
		check(Variables.Num() == PerVariableWriteHistory.Num());
	}
	else
	{
		if (Variables[FoundIdx].GetType() != Var.GetType())
		{
			PerVariableWarnings[FoundIdx].Append(FString::Printf(TEXT("Type mismatch %s instead of %s in map!"), *Var.GetType().GetName(), *Variables[FoundIdx].GetType().GetName()));
		}
		if (InPin != nullptr)
		{
			PerVariableWriteHistory[FoundIdx].Add(InPin);
		}
	}

	return FoundIdx;
}

int32 FNiagaraParameterMapHistory::AddExternalVariable(const FNiagaraVariable& Var)
{
	return AddVariable(Var, nullptr);
}

const UEdGraphPin* FNiagaraParameterMapHistory::GetFinalPin() const
{
	if (MapPinHistory.Num() > 0)
	{
		return MapPinHistory[MapPinHistory.Num() - 1];
	}
	return nullptr;
}

const UEdGraphPin* FNiagaraParameterMapHistory::GetOriginalPin() const
{
	if (MapPinHistory.Num() > 0)
	{
		return MapPinHistory[0];
	}
	return nullptr;
}

FNiagaraVariable FNiagaraParameterMapHistory::ResolveAliases(const FNiagaraVariable& InVar, const TMap<FString, FString>& InAliases, const TCHAR* InJoinSeparator)
{
	FNiagaraVariable OutVar = InVar;

	FString OutVarStrName = InVar.GetName().ToString();
	TArray<FString> SplitName;
	OutVarStrName.ParseIntoArray(SplitName, TEXT("."));

	for (int32 i = 0; i < SplitName.Num() - 1; i++)
	{
		TMap<FString, FString>::TConstIterator It = InAliases.CreateConstIterator();
		while (It)
		{
			if (SplitName[i].Equals(It.Key()))
			{
				SplitName[i] = It.Value();
			}
			++It;
		}
	}

	OutVarStrName = FString::Join<FString>(SplitName, InJoinSeparator);

	OutVar.SetName(*OutVarStrName);
	return OutVar;
}

FString FNiagaraParameterMapHistory::MakeSafeNamespaceString(const FString& InStr)
{
	FString  Sanitized = FHlslNiagaraTranslator::GetSanitizedSymbolName(InStr);
	return Sanitized;
}


FNiagaraVariable FNiagaraParameterMapHistory::ResolveAsBasicAttribute(const FNiagaraVariable& InVar)
{
	if (IsAttribute(InVar))
	{
		FString ParamName = InVar.GetName().ToString();
		ParamName.RemoveAt(0, FString(PARAM_MAP_ATTRIBUTE_STR).Len());

		ParamName = MakeSafeNamespaceString(ParamName);

		FNiagaraVariable RetVar = InVar;
		RetVar.SetName(*ParamName);
		return RetVar;
	}
	else
	{
		return InVar;
	}
}

FNiagaraVariable FNiagaraParameterMapHistory::BasicAttributeToNamespacedAttribute(const FNiagaraVariable& InVar)
{
	FString ParamName = InVar.GetName().ToString();
	ParamName.InsertAt(0, FString(PARAM_MAP_ATTRIBUTE_STR));

	ParamName = MakeSafeNamespaceString(ParamName);

	FNiagaraVariable RetVar = InVar;
	RetVar.SetName(*ParamName);
	return RetVar;
}

FNiagaraVariable FNiagaraParameterMapHistory::VariableToNamespacedVariable(const FNiagaraVariable& InVar, FString Namespace)
{
	FString ParamName = Namespace;
	if (Namespace.EndsWith(TEXT(".")))
	{
		ParamName += InVar.GetName().ToString();
	}
	else
	{
		ParamName += TEXT(".") + InVar.GetName().ToString();
	}
	

	FNiagaraVariable RetVar = InVar;
	RetVar.SetName(*ParamName);
	return RetVar;
}

bool FNiagaraParameterMapHistory::IsInNamespace(const FNiagaraVariable& InVar, const FString& Namespace)
{
	return InVar.GetName().ToString().StartsWith(Namespace);
}

bool FNiagaraParameterMapHistory::IsAliasedModuleParameter(const FNiagaraVariable& InVar)
{
	return IsInNamespace(InVar, PARAM_MAP_MODULE_STR);
}

bool FNiagaraParameterMapHistory::IsAliasedEmitterParameter(const FNiagaraVariable& InVar)
{
	return IsInNamespace(InVar, PARAM_MAP_EMITTER_STR);
}

bool FNiagaraParameterMapHistory::IsSystemParameter(const FNiagaraVariable& InVar)
{
	return IsInNamespace(InVar, PARAM_MAP_SYSTEM_STR);
}

bool FNiagaraParameterMapHistory::IsEngineParameter(const FNiagaraVariable& InVar)
{
	return IsInNamespace(InVar, PARAM_MAP_ENGINE_STR);
}

bool FNiagaraParameterMapHistory::IsUserParameter(const FNiagaraVariable& InVar)
{
	return IsInNamespace(InVar, PARAM_MAP_USER_STR);
}

bool FNiagaraParameterMapHistory::IsAttribute(const FNiagaraVariable& InVar)
{
	return IsInNamespace(InVar, PARAM_MAP_ATTRIBUTE_STR);
}

bool FNiagaraParameterMapHistory::IsSystemNamespaceReadOnly(UNiagaraScript* InScript)
{
	if (InScript->IsSystemSpawnScript() || InScript->IsSystemUpdateScript())
	{
		return false;
	}
	else if (InScript->IsStandaloneScript())
	{
		TArray<ENiagaraScriptUsage> IntendedUsages = InScript->GetSupportedUsageContexts();
		if (IntendedUsages.Contains(ENiagaraScriptUsage::SystemSpawnScript) || IntendedUsages.Contains(ENiagaraScriptUsage::SystemUpdateScript))
		{
			return false;
		}
	}

	return true;
}

bool FNiagaraParameterMapHistory::IsEmitterNamespaceReadOnly(UNiagaraScript* InScript)
{
	if (InScript->IsEmitterSpawnScript() || InScript->IsEmitterUpdateScript() || InScript->IsSystemSpawnScript() || InScript->IsSystemUpdateScript())
	{
		return false;
	}
	else if (InScript->IsStandaloneScript())
	{
		TArray<ENiagaraScriptUsage> IntendedUsages = InScript->GetSupportedUsageContexts();
		if (IntendedUsages.Contains(ENiagaraScriptUsage::EmitterSpawnScript) || IntendedUsages.Contains(ENiagaraScriptUsage::EmitterUpdateScript) || IntendedUsages.Contains(ENiagaraScriptUsage::SystemSpawnScript) || IntendedUsages.Contains(ENiagaraScriptUsage::SystemUpdateScript))
		{
			return false;
		}
	}

	return true;
}

const UEdGraphPin* FNiagaraParameterMapHistory::GetDefaultValuePin(int32 VarIdx) const
{
	if (PerVariableWriteHistory[VarIdx].Num() > 0)
	{
		const UEdGraphPin* Pin = PerVariableWriteHistory[VarIdx][0];
		if (Pin != nullptr && Pin->Direction == EEdGraphPinDirection::EGPD_Input && Cast<UNiagaraNodeParameterMapGet>(Pin->GetOwningNode()) != nullptr)
		{
			return Pin;
		}
	}
	return nullptr;
}

bool FNiagaraParameterMapHistory::IsInitialValue(const FNiagaraVariable& InVar)
{
	TArray<FString> SplitName;
	InVar.GetName().ToString().ParseIntoArray(SplitName, TEXT("."));

	for (int32 i = 1; i < SplitName.Num() - 1; i++)
	{
		if (SplitName[i].Equals(PARAM_MAP_INITIAL_BASE_STR, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

FNiagaraVariable FNiagaraParameterMapHistory::GetSourceForInitialValue(const FNiagaraVariable& InVar)
{
	TArray<FString> SplitName;
	InVar.GetName().ToString().ParseIntoArray(SplitName, TEXT("."));

	TArray<FString> JoinString;
	bool bFound = false;
	for (int32 i = 0; i < SplitName.Num(); i++)
	{
		if (!bFound && SplitName[i].Equals(PARAM_MAP_INITIAL_BASE_STR, ESearchCase::IgnoreCase))
		{
			bFound = true;
			continue;
		}
		else
		{
			JoinString.Add(SplitName[i]);
		}
	}

	FString OutVarStrName = FString::Join<FString>(JoinString, TEXT("."));
	FNiagaraVariable Var = InVar;
	Var.SetName(*OutVarStrName);

	return Var;
}

bool FNiagaraParameterMapHistory::IsPrimaryDataSetOutput(const FNiagaraVariable& InVar, UNiagaraScript* InScript)
{
	return IsPrimaryDataSetOutput(InVar, InScript->GetUsage());
}

bool FNiagaraParameterMapHistory::IsPrimaryDataSetOutput(const FNiagaraVariable& InVar, ENiagaraScriptUsage Usage)
{
	if (InVar.GetType().GetClass() != nullptr)
	{
		return false;
	}

	if (Usage == ENiagaraScriptUsage::EmitterSpawnScript || Usage == ENiagaraScriptUsage::EmitterUpdateScript || 
		Usage == ENiagaraScriptUsage::SystemSpawnScript || Usage == ENiagaraScriptUsage::SystemUpdateScript)
	{
		// In the case of system/emitter scripts we must include the variables in the overall system namespace as well as any of 
		// the child emitters that were encountered.
		for (FString EmitterEncounteredNamespace : EmitterNamespacesEncountered)
		{
			if (IsInNamespace(InVar, EmitterEncounteredNamespace))
			{
				return true;
			}
		}
		return IsInNamespace(InVar, PARAM_MAP_SYSTEM_STR) || IsInNamespace(InVar, PARAM_MAP_EMITTER_STR);
	}
	else if (Usage == ENiagaraScriptUsage::Module || Usage == ENiagaraScriptUsage::Function)
	{
		return IsInNamespace(InVar, PARAM_MAP_MODULE_STR);
	}
	return IsInNamespace(InVar, PARAM_MAP_ATTRIBUTE_STR);
}


FNiagaraVariable FNiagaraParameterMapHistory::MoveToExternalConstantNamespaceVariable(const FNiagaraVariable& InVar, UNiagaraScript* InScript)
{
	if (InScript->IsParticleEventScript() || InScript->IsParticleSpawnScript() || InScript->IsParticleUpdateScript())
	{
		return VariableToNamespacedVariable(InVar, PARAM_MAP_EMITTER_STR);
	}
	else if (InScript->IsStandaloneScript())
	{
		return VariableToNamespacedVariable(InVar, PARAM_MAP_MODULE_STR);
	}
	else if (InScript->IsEmitterSpawnScript() || InScript->IsEmitterUpdateScript() || InScript->IsSystemSpawnScript() || InScript->IsSystemUpdateScript())
	{
		return VariableToNamespacedVariable(InVar, PARAM_MAP_USER_STR);
	}
	return InVar;
}

bool FNiagaraParameterMapHistory::IsExportableExternalConstant(const FNiagaraVariable& InVar, UNiagaraScript* InScript)
{
	if (InScript->IsEquivalentUsage(ENiagaraScriptUsage::SystemSpawnScript))
	{
		return IsExternalConstantNamespace(InVar, InScript);
	}
	else
	{
		return false;
	}
}

bool FNiagaraParameterMapHistory::IsExternalConstantNamespace(const FNiagaraVariable& InVar, UNiagaraScript* InScript)
{
	// Engine parameters are never writable.
	if (IsInNamespace(InVar, PARAM_MAP_ENGINE_STR))
	{
		return true;
	}

	if (IsInNamespace(InVar, PARAM_MAP_USER_STR))
	{
		return true;
	}

	// Modules and functions need to act as if they are within the script types that they 
	// say that they support rather than using their exact script type.
	if (InScript->IsStandaloneScript())
	{
		TArray<ENiagaraScriptUsage> SupportedContexts = InScript->GetSupportedUsageContexts();
		if (((!SupportedContexts.Contains(ENiagaraScriptUsage::EmitterSpawnScript) && !SupportedContexts.Contains(ENiagaraScriptUsage::EmitterUpdateScript)) && IsInNamespace(InVar, PARAM_MAP_EMITTER_STR))
			|| ((!SupportedContexts.Contains(ENiagaraScriptUsage::SystemSpawnScript) && !SupportedContexts.Contains(ENiagaraScriptUsage::SystemUpdateScript)) && IsInNamespace(InVar, PARAM_MAP_SYSTEM_STR)))
		{
			return true;
		}
	}
	
	// Particle scripts cannot write to the emitter or system namespace.
	if (InScript->IsParticleEventScript() || InScript->IsParticleSpawnScript() || InScript->IsParticleUpdateScript())
	{
		if (IsInNamespace(InVar, PARAM_MAP_EMITTER_STR) || IsInNamespace(InVar, PARAM_MAP_SYSTEM_STR))
		{
			return true;
		}
	}
	
	return false;
}

const UNiagaraNodeOutput* FNiagaraParameterMapHistory::GetFinalOutputNode() const
{
	const UEdGraphPin* Pin = GetFinalPin();
	if (Pin != nullptr)
	{
		const UNiagaraNodeOutput* Output = Cast<const UNiagaraNodeOutput>(Pin->GetOwningNode());
		if (Output != nullptr)
		{
			return Output;
		}
	}

	return nullptr;
}

FNiagaraParameterMapHistoryBuilder::FNiagaraParameterMapHistoryBuilder()
{
	ContextuallyVisitedNodes.AddDefaulted(1);
	PinToParameterMapIndices.AddDefaulted(1);
	bFilterByScriptWhitelist = false;
	FilterScriptType = ENiagaraScriptUsage::Function;
}

void FNiagaraParameterMapHistoryBuilder::BuildParameterMaps(UNiagaraNodeOutput* OutputNode, bool bRecursive)
{
	RelevantScriptUsageContext.Emplace(OutputNode->GetUsage());
	OutputNode->BuildParameterMapHistory(*this, bRecursive);
	RelevantScriptUsageContext.Pop();
}

void FNiagaraParameterMapHistoryBuilder::EnableScriptWhitelist(bool bInEnable, ENiagaraScriptUsage InScriptType)
{
	bFilterByScriptWhitelist = bInEnable;
	FilterScriptType = InScriptType;
}

ENiagaraScriptUsage FNiagaraParameterMapHistoryBuilder::GetCurrentUsageContext()const
{
	return RelevantScriptUsageContext.Last();
}

int32 FNiagaraParameterMapHistoryBuilder::CreateParameterMap()
{
	int32 RetValue = Histories.AddDefaulted(1);
	return RetValue;
}

int32 FNiagaraParameterMapHistoryBuilder::RegisterParameterMapPin(int32 WhichParameterMap, const UEdGraphPin* Pin)
{
	if (WhichParameterMap != INDEX_NONE)
	{
		if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
		{
			PinToParameterMapIndices.Last().Add(Pin, WhichParameterMap);
		}

		return Histories[WhichParameterMap].RegisterParameterMapPin(Pin);
	}
	else
	{
		return INDEX_NONE;
	}
}

int32 FNiagaraParameterMapHistoryBuilder::TraceParameterMapOutputPin(const UEdGraphPin* OutputPin)
{
	if (OutputPin->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		const int32* IdxPtr = PinToParameterMapIndices.Last().Find(OutputPin);
		if (IdxPtr != nullptr)
		{
			return *IdxPtr;
		}
	}
	return INDEX_NONE;
}

bool FNiagaraParameterMapHistoryBuilder::GetPinPreviouslyVisited(const UEdGraphPin* InPin) const
{
	if (InPin != nullptr)
		return GetNodePreviouslyVisited(CastChecked<UNiagaraNode>(InPin->GetOwningNode()));
	else
		return true;
}


bool FNiagaraParameterMapHistoryBuilder::GetNodePreviouslyVisited(const class UNiagaraNode* Node) const
{
	return ContextuallyVisitedNodes.Last().Contains(Node);
}

int32 FNiagaraParameterMapHistoryBuilder::FindMatchingParameterMapFromContextInputs(const FNiagaraVariable& InVar) const
{
	if (CallingContext.Num() == 0)
	{
		return INDEX_NONE;
	}
	UNiagaraNode* Node = CallingContext.Last();
	TArray<UEdGraphPin*> Inputs;
	Node->GetInputPins(Inputs);
	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(Node->GetSchema());

	for (int32 i = 0; i < Inputs.Num(); i++)
	{
		FNiagaraVariable CallInputVar = Schema->PinToNiagaraVariable(Inputs[i]);
		if (CallInputVar.IsEquivalent(InVar) && CallInputVar.GetType() == FNiagaraTypeDefinition::GetParameterMapDef())
		{
			if (Inputs[i]->LinkedTo.Num() != 0 && PinToParameterMapIndices.Num() >= 2)
			{
				const int32* ParamMapIdxPtr = PinToParameterMapIndices[PinToParameterMapIndices.Num() - 2].Find(Inputs[i]->LinkedTo[0]);
				if (ParamMapIdxPtr != nullptr)
				{
					return *ParamMapIdxPtr;
				}
				else
				{
					FString ScriptUsageDisplayName;
					UNiagaraNodeOutput* ContextOutputNode = FNiagaraStackGraphUtilities::GetEmitterOutputNodeForStackNode(*Node);
					if (ContextOutputNode != nullptr)
					{
						UEnum* NiagaraScriptUsageEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("ENiagaraScriptUsage"), true);
						ScriptUsageDisplayName = NiagaraScriptUsageEnum->GetDisplayNameTextByValue((uint64)ContextOutputNode->GetUsage()).ToString();
					}
					else
					{
						ScriptUsageDisplayName = "Unknown";
					}
					FString FunctionDisplayName;
					if (FunctionNameContextStack.Num() > 0)
					{
						FunctionDisplayName = FunctionNameContextStack.Last().ToString();
					}
					else
					{
						FunctionDisplayName = Node->GetName();
					}

					
					/* 
					UE_LOG(LogNiagaraEditor, Error, TEXT("Unable to find matching parameter map for variable.  Name:%s  Function:%s  Usage:%s  Package:%s"),
						*InVar.GetName().ToString(), *FunctionDisplayName, *ScriptUsageDisplayName, *Node->GetOutermost()->GetName());
					*/
					
				}
			}
		}
	}
	return INDEX_NONE;
}

void FNiagaraParameterMapHistoryBuilder::BeginTranslation(UNiagaraScript* Script)
{
	//For now this will just tell particle scripts what emitter they're being compiled as part of but maybe we want to do more here.
	//This is mainly so that parameter names match up between System/Emitter scripts and the parameters they drive within particle scripts.
	//I dislike this coupling of the translator to emitters but for now it'll have to do.
	//Will refactor in the future.
	UNiagaraEmitter* Emitter = Script->GetTypedOuter<UNiagaraEmitter>();
	BeginTranslation(Emitter);
}

void FNiagaraParameterMapHistoryBuilder::EndTranslation(UNiagaraScript* Script)
{
	EmitterNameContextStack.Reset();
}

void FNiagaraParameterMapHistoryBuilder::BeginTranslation(UNiagaraEmitter* Emitter)
{
	//For now this will just tell particle scripts what emitter they're being compiled as part of but maybe we want to do more here.
	//This is mainly so that parameter names match up between System/Emitter scripts and the parameters they drive within particle scripts.
	//I dislike this coupling of the translator to emitters but for now it'll have to do.
	//Will refactor in the future.
	if (Emitter)
	{
		FString EmitterUniqueName = Emitter->GetUniqueEmitterName();
		EmitterNameContextStack.Add(*EmitterUniqueName);
	}
	BuildCurrentAliases();
}

void FNiagaraParameterMapHistoryBuilder::EndTranslation(UNiagaraEmitter* Emitter)
{
	EmitterNameContextStack.Reset();
}

void FNiagaraParameterMapHistoryBuilder::EnterFunction(const FString& InNodeName, UNiagaraScript* InScript, UNiagaraNode* Node)
{
	if (InScript != nullptr )
	{
		RegisterNodeVisitation(Node);
		CallingContext.Push(Node);
		PinToParameterMapIndices.Emplace();
		FunctionNameContextStack.Emplace(*InNodeName);
		BuildCurrentAliases();
		ContextuallyVisitedNodes.Emplace();
	}
}

void FNiagaraParameterMapHistoryBuilder::ExitFunction(const FString& InNodeName, UNiagaraScript* InScript, UNiagaraNode* Node)
{
	if (InScript != nullptr)
	{
		CallingContext.Pop();
		PinToParameterMapIndices.Pop();
		FunctionNameContextStack.Pop();
		BuildCurrentAliases();
		ContextuallyVisitedNodes.Pop();
	}
}

void FNiagaraParameterMapHistoryBuilder::EnterEmitter(const FString& InEmitterName, UNiagaraNode* Node)
{
	RegisterNodeVisitation(Node);
	CallingContext.Push(Node);
	EmitterNameContextStack.Emplace(*InEmitterName);
	BuildCurrentAliases();

	// Emitters must record their namespaces to their histories as well as
	// make sure to record their current usage type is so that we can filter variables
	// for relevance downstream.
	UNiagaraNodeEmitter* EmitterNode = Cast<UNiagaraNodeEmitter>(Node);
	if (EmitterNode != nullptr)
	{
		RelevantScriptUsageContext.Emplace(EmitterNode->GetUsage());
	}
	else
	{
		RelevantScriptUsageContext.Emplace(ENiagaraScriptUsage::EmitterSpawnScript);
	}

	for (FNiagaraParameterMapHistory& History : Histories)
	{
		History.EmitterNamespacesEncountered.AddUnique(InEmitterName);
	}
	ContextuallyVisitedNodes.Emplace();
}

void FNiagaraParameterMapHistoryBuilder::ExitEmitter(const FString& InEmitterName, UNiagaraNode* Node)
{
	CallingContext.Pop();
	EmitterNameContextStack.Pop();
	BuildCurrentAliases();
	ContextuallyVisitedNodes.Pop();
}

/**
* Use the current alias map to resolve any aliases in this input variable name.
*/
FNiagaraVariable FNiagaraParameterMapHistoryBuilder::ResolveAliases(const FNiagaraVariable& InVar)
{
	return FNiagaraParameterMapHistory::ResolveAliases(InVar, AliasMap, TEXT("."));
}


void FNiagaraParameterMapHistoryBuilder::RegisterNodeVisitation(const UEdGraphNode* Node)
{
	ContextuallyVisitedNodes.Last().AddUnique(CastChecked<UNiagaraNode>(Node));
}


const FString* FNiagaraParameterMapHistoryBuilder::GetModuleAlias() const
{
	return AliasMap.Find(TEXT("Module"));
}

const FString* FNiagaraParameterMapHistoryBuilder::GetEmitterAlias() const
{
	return AliasMap.Find(TEXT("Emitter"));
}

void FNiagaraParameterMapHistoryBuilder::VisitInputPin(const UEdGraphPin* Pin, class UNiagaraNode* InNode)
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	if (Pin != nullptr && Pin->Direction == EEdGraphPinDirection::EGPD_Input)
	{
		for (int32 j = 0; j < Pin->LinkedTo.Num(); j++)
		{
			UNiagaraNode* Node = CastChecked<UNiagaraNode>(Pin->LinkedTo[j]->GetOwningNode());

			if (!GetNodePreviouslyVisited(Node))
			{
				Node->BuildParameterMapHistory(*this, true);
				RegisterNodeVisitation(Node);
			}

			if (Schema->PinToTypeDefinition(Pin) == FNiagaraTypeDefinition::GetParameterMapDef())
			{
				int32 ParamMapIdx = TraceParameterMapOutputPin(Pin->LinkedTo[j]);
				RegisterParameterMapPin(ParamMapIdx, Pin);
			}
		} 
	}
}

void FNiagaraParameterMapHistoryBuilder::VisitInputPins(class UNiagaraNode* InNode)
{
	TArray<UEdGraphPin*> InputPins;
	InNode->GetInputPins(InputPins);

	for (int32 i = 0; i < InputPins.Num(); i++)
	{
		VisitInputPin(InputPins[i], InNode);
	}
}

bool FNiagaraParameterMapHistoryBuilder::IsNamespacedVariableRelevantToScriptType(const FNiagaraVariable& InVar, ENiagaraScriptUsage InFilterScriptType)
{
	return true;
}

bool FNiagaraParameterMapHistoryBuilder::ShouldTrackVariable(const FNiagaraVariable& InVar)
{
	if (!bFilterByScriptWhitelist)
	{
		return true;
	}
	if (IsNamespacedVariableRelevantToScriptType(InVar, FilterScriptType))
	{
		return true;
	}
	return false;
}

int32 FNiagaraParameterMapHistoryBuilder::HandleVariableWrite(int32 ParamMapIdx, const UEdGraphPin* InPin)
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	FNiagaraVariable Var = Schema->PinToNiagaraVariable(InPin);

	if (!ShouldTrackVariable(Var))
	{
		return INDEX_NONE;
	}
	Var = ResolveAliases(Var);

	return Histories[ParamMapIdx].AddVariable(Var, InPin);
}


int32 FNiagaraParameterMapHistoryBuilder::HandleVariableRead(int32 ParamMapIdx, const UEdGraphPin* InPin, bool RegisterReadsAsVariables, const UEdGraphPin* InDefaultPin, bool& OutUsedDefault)
{
	OutUsedDefault = false;
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	FNiagaraVariable Var = Schema->PinToNiagaraVariable(InPin);

	if (!ShouldTrackVariable(Var))
	{
		return INDEX_NONE;
	}
	Var = ResolveAliases(Var);

	int32 FoundIdx = Histories[ParamMapIdx].FindVariable(Var.GetName(), Var.GetType());
	if (FoundIdx == -1)
	{
		if (RegisterReadsAsVariables)
		{
			OutUsedDefault = false;
			if (InDefaultPin)
			{
				OutUsedDefault = true;
				VisitInputPin(InDefaultPin, Cast<UNiagaraNode>(InDefaultPin->GetOwningNode()));
				FoundIdx = Histories[ParamMapIdx].FindVariable(Var.GetName(), Var.GetType());
			}

			if (FoundIdx == -1)
			{
				FoundIdx = Histories[ParamMapIdx].Variables.Add(Var);
			}
			Histories[ParamMapIdx].PerVariableWarnings.AddDefaulted(1);
			Histories[ParamMapIdx].PerVariableWriteHistory.AddDefaulted(1);
			Histories[ParamMapIdx].PerVariableReadHistory.AddDefaulted(1);
			Histories[ParamMapIdx].PerVariableReadHistory[FoundIdx].Add(TTuple<const UEdGraphPin*, const UEdGraphPin*>(InPin, nullptr));

			if (InDefaultPin && OutUsedDefault)
			{
				Histories[ParamMapIdx].PerVariableWriteHistory[FoundIdx].Add(InDefaultPin);				
			}
		}
		check(Histories[ParamMapIdx].Variables.Num() == Histories[ParamMapIdx].PerVariableWarnings.Num());
		check(Histories[ParamMapIdx].Variables.Num() == Histories[ParamMapIdx].PerVariableWriteHistory.Num());
		check(Histories[ParamMapIdx].Variables.Num() == Histories[ParamMapIdx].PerVariableReadHistory.Num());
	}
	else
	{
		if (Histories[ParamMapIdx].Variables[FoundIdx].GetType() != Var.GetType())
		{
			Histories[ParamMapIdx].PerVariableWarnings[FoundIdx].Append(FString::Printf(TEXT("Type mismatch %s instead of %s in map!"), *Var.GetType().GetName(), *Histories[ParamMapIdx].Variables[FoundIdx].GetType().GetName()));
		}


		if (Histories[ParamMapIdx].PerVariableWriteHistory[FoundIdx].Num() > 0)
		{
			Histories[ParamMapIdx].PerVariableReadHistory[FoundIdx].Add(TTuple<const UEdGraphPin*, const UEdGraphPin*>(InPin, Histories[ParamMapIdx].PerVariableWriteHistory[FoundIdx][Histories[ParamMapIdx].PerVariableWriteHistory[FoundIdx].Num() - 1]));
		}
		else
		{
			Histories[ParamMapIdx].PerVariableReadHistory[FoundIdx].Add(TTuple<const UEdGraphPin*, const UEdGraphPin*>(InPin, nullptr));
		}
	}

	return FoundIdx;
}

void FNiagaraParameterMapHistoryBuilder::BuildCurrentAliases()
{
	AliasMap = TMap<FString, FString>();

	FString Callstack;
	for (int32 i = 0; i < FunctionNameContextStack.Num(); i++)
	{
		if (i == 0)
		{
			Callstack += FunctionNameContextStack[i].ToString();
		}
		else
		{
			Callstack += TEXT(".") + FunctionNameContextStack[i].ToString();
		}
	}

	if (!Callstack.IsEmpty())
	{
		AliasMap.Add(TEXT("Module"), Callstack);
	}

	Callstack.Empty();
	for (int32 i = 0; i < EmitterNameContextStack.Num(); i++)
	{
		if (i == 0)
		{
			Callstack += EmitterNameContextStack[i].ToString();
		}
		else
		{
			Callstack += TEXT(".") + EmitterNameContextStack[i].ToString();
		}
	}

	if (!Callstack.IsEmpty())
	{
		AliasMap.Add(TEXT("Emitter"), Callstack);
	}
}

#undef LOCTEXT_NAMESPACE
