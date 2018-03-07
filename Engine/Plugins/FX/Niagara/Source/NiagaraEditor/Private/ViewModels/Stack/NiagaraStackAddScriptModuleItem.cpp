// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackAddScriptModuleItem.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScript.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeParameterMapBase.h"
#include "NiagaraConstants.h"
#include "Stack/NiagaraParameterHandle.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "NiagaraStackViewModel"

void UNiagaraStackAddScriptModuleItem::Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraStackEditorData& InStackEditorData, UNiagaraNodeOutput& InOutputNode)
{
	Super::Initialize(InSystemViewModel, InEmitterViewModel, InStackEditorData);
	OutputNode = &InOutputNode;
}

void UNiagaraStackAddScriptModuleItem::GetAvailableParameters(TArray<FNiagaraVariable>& OutAvailableParameterVariables) const
{
	TArray<FNiagaraParameterMapHistory> Histories = UNiagaraNodeParameterMapBase::GetParameterMaps(OutputNode->GetNiagaraGraph());

	if (OutputNode->GetUsage() == ENiagaraScriptUsage::ParticleSpawnScript ||
		OutputNode->GetUsage() == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated ||
		OutputNode->GetUsage() == ENiagaraScriptUsage::ParticleUpdateScript ||
		OutputNode->GetUsage() == ENiagaraScriptUsage::ParticleEventScript)
	{
		OutAvailableParameterVariables.Append(FNiagaraConstants::GetCommonParticleAttributes());
	}

	for (FNiagaraParameterMapHistory& History : Histories)
	{
		for (FNiagaraVariable& Variable : History.Variables)
		{
			if (History.IsPrimaryDataSetOutput(Variable, OutputNode->GetUsage()))
			{
				OutAvailableParameterVariables.AddUnique(Variable);
			}
		}
	}
}

void UNiagaraStackAddScriptModuleItem::GetNewParameterAvailableTypes(TArray<FNiagaraTypeDefinition>& OutAvailableTypes) const
{
	for (const FNiagaraTypeDefinition& RegisteredParameterType : FNiagaraTypeRegistry::GetRegisteredParameterTypes())
	{
		if (RegisteredParameterType != FNiagaraTypeDefinition::GetGenericNumericDef() && RegisteredParameterType != FNiagaraTypeDefinition::GetParameterMapDef())
		{
			OutAvailableTypes.Add(RegisteredParameterType);
		}
	}
}

TOptional<FString> UNiagaraStackAddScriptModuleItem::GetNewParameterNamespace() const
{
	switch (GetOutputUsage())
	{
	case ENiagaraScriptUsage::ParticleSpawnScript:
	case ENiagaraScriptUsage::ParticleUpdateScript:
		return FNiagaraParameterHandle::ParticleAttributeNamespace;
	case ENiagaraScriptUsage::EmitterSpawnScript:
	case ENiagaraScriptUsage::EmitterUpdateScript:
		return FNiagaraParameterHandle::EmitterNamespace;
	case ENiagaraScriptUsage::SystemSpawnScript:
	case ENiagaraScriptUsage::SystemUpdateScript:
		return FNiagaraParameterHandle::SystemNamespace;
	default:
		return TOptional<FString>();
	}
}

ENiagaraScriptUsage UNiagaraStackAddScriptModuleItem::GetOutputUsage() const
{
	return OutputNode->GetUsage();
}

UNiagaraNodeOutput* UNiagaraStackAddScriptModuleItem::GetOrCreateOutputNode()
{
	return OutputNode.Get();
}

#undef LOCTEXT_NAMESPACE