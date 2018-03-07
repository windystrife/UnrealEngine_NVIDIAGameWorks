// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraScriptSource.h"
#include "Modules/ModuleManager.h"
#include "NiagaraCommon.h"
#include "NiagaraEditorModule.h"
#include "NiagaraScript.h"
#include "NiagaraComponent.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "ComponentReregisterContext.h"
#include "NiagaraGraph.h"
#include "NiagaraConstants.h"
#include "NiagaraSystem.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeWriteDataSet.h"
#include "NiagaraNodeReadDataSet.h"
#include "NiagaraScript.h"
#include "NiagaraDataInterface.h"
#include "GraphEditAction.h"
#include "NiagaraNodeParameterMapBase.h"
#include "NiagaraNodeOutput.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_Niagara.h"
#include "TokenizedMessage.h"
#include "NiagaraEditorUtilities.h"

UNiagaraScriptSource::UNiagaraScriptSource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), bIsPrecompiled(false)
{
}

void UNiagaraScriptSource::PostLoad()
{
	Super::PostLoad();
	// We need to make sure that the node-graph is already resolved b/c we may be asked IsSyncrhonized later...
	if (NodeGraph)
	{
		NodeGraph->ConditionalPostLoad();
	}
}

UNiagaraScriptSourceBase* UNiagaraScriptSource::MakeRecursiveDeepCopy(UObject* DestOuter, TMap<const UObject*, UObject*>& ExistingConversions) const
{
	check(GetOuter() != DestOuter);
	EObjectFlags Flags = RF_AllFlags & ~RF_Standalone & ~RF_Public; // Remove Standalone and Public flags.
	ResetLoaders(GetTransientPackage()); // Make sure that we're not going to get invalid version number linkers into the package we are going into. 
	GetTransientPackage()->LinkerCustomVersion.Empty();

	UNiagaraScriptSource*	ScriptSource = CastChecked<UNiagaraScriptSource>(StaticDuplicateObject(this, GetTransientPackage(), NAME_None, Flags));
	check(ScriptSource->HasAnyFlags(RF_Standalone) == false);
	check(ScriptSource->HasAnyFlags(RF_Public) == false);

	ScriptSource->Rename(nullptr, DestOuter, REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
	UE_LOG(LogNiagaraEditor, Warning, TEXT("MakeRecursiveDeepCopy %s"), *ScriptSource->GetFullName());
	ExistingConversions.Add(this, ScriptSource);

	ScriptSource->SubsumeExternalDependencies(ExistingConversions);
	return ScriptSource;
}

void UNiagaraScriptSource::SubsumeExternalDependencies(TMap<const UObject*, UObject*>& ExistingConversions)
{
	if (NodeGraph)
	{
		NodeGraph->SubsumeExternalDependencies(ExistingConversions);
	}
}

bool UNiagaraScriptSource::IsSynchronized(const FGuid& InChangeId)
{
	if (NodeGraph)
	{
		return NodeGraph->IsOtherSynchronized(InChangeId);
	}
	else
	{
		return false;
	}
}

void UNiagaraScriptSource::MarkNotSynchronized()
{
	if (NodeGraph)
	{
		NodeGraph->MarkGraphRequiresSynchronization();
	}
}

bool UNiagaraScriptSource::IsPreCompiled() const
{
	return bIsPrecompiled;
}

void UNiagaraScriptSource::PreCompile(UNiagaraEmitter* InEmitter, bool bClearErrors)
{
	if (!bIsPrecompiled)
	{
		bIsPrecompiled = true;

		if (bClearErrors)
		{
			//Clear previous graph errors.
			bool bHasClearedGraphErrors = false;
			for (UEdGraphNode* Node : NodeGraph->Nodes)
			{
				if (Node->bHasCompilerMessage)
				{
					Node->ErrorMsg.Empty();
					Node->ErrorType = EMessageSeverity::Info;
					Node->bHasCompilerMessage = false;
					Node->Modify(true);
					bHasClearedGraphErrors = true;
				}
			}
			if (bHasClearedGraphErrors)
			{
				NodeGraph->NotifyGraphChanged();
			}
		}

		// Clone the source graph so we can modify it as needed; merging in the child graphs
		NodeGraphDeepCopy = CastChecked<UNiagaraGraph>(FEdGraphUtilities::CloneGraph(NodeGraph, this, 0));
		FEdGraphUtilities::MergeChildrenGraphsIn(NodeGraphDeepCopy, NodeGraphDeepCopy, /*bRequireSchemaMatch=*/ true);
		
		PrecompiledHistories.Empty();

		TArray<UNiagaraNodeOutput*> OutputNodes;
		NodeGraphDeepCopy->FindOutputNodes(OutputNodes);
		PrecompiledHistories.Empty();
		for (UNiagaraNodeOutput* FoundOutputNode : OutputNodes)
		{
			// Map all for this output node
			FNiagaraParameterMapHistoryBuilder Builder;
			Builder.BeginTranslation(InEmitter);
			Builder.EnableScriptWhitelist(true, FoundOutputNode->GetUsage());
			Builder.BuildParameterMaps(FoundOutputNode, true);
			TArray<FNiagaraParameterMapHistory> Histories = Builder.Histories;
			ensure(Histories.Num() <= 1);
			PrecompiledHistories.Append(Histories);
			Builder.EndTranslation(InEmitter);
		}
	}
}


bool UNiagaraScriptSource::GatherPreCompiledVariables(const FString& InNamespaceFilter, TArray<FNiagaraVariable>& OutVars)
{
	if (!bIsPrecompiled || PrecompiledHistories.Num() == 0)
	{
		return false;
	}

	for (const FNiagaraParameterMapHistory& History : PrecompiledHistories)
	{
		for (const FNiagaraVariable& Var : History.Variables)
		{
			if (FNiagaraParameterMapHistory::IsInNamespace(Var, InNamespaceFilter))
			{
				FNiagaraVariable NewVar = Var;
				if (NewVar.IsDataAllocated() == false && !Var.IsDataInterface())
				{
					FNiagaraEditorUtilities::ResetVariableToDefaultValue(NewVar);
				}
				OutVars.AddUnique(NewVar);
			}
		}
	}
	return true;
}

void UNiagaraScriptSource::PostCompile() 
{
	bIsPrecompiled = false;
	PrecompiledHistories.Empty();
	NodeGraphDeepCopy = nullptr;
}


ENiagaraScriptCompileStatus UNiagaraScriptSource::Compile(UNiagaraScript* ScriptOwner, FString& OutGraphLevelErrorMessages)
{
	bool bDoPostCompile = false;
	if (!bIsPrecompiled)
	{
		PreCompile(nullptr);
		bDoPostCompile = true;
	}

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::Get().LoadModuleChecked<FNiagaraEditorModule>(TEXT("NiagaraEditor"));
	ENiagaraScriptCompileStatus Status = NiagaraEditorModule.CompileScript(ScriptOwner, OutGraphLevelErrorMessages);
	check(ScriptOwner != nullptr && IsSynchronized(ScriptOwner->GetChangeID()));
	
	if (bDoPostCompile)
	{
		PostCompile();
	}
	return Status;

// 	FNiagaraConstants& ExternalConsts = ScriptOwner->ConstantData.GetExternalConstants();
// 
// 	//Build the constant list. 
// 	//This is mainly just jumping through some hoops for the custom UI. Should be removed and have the UI just read directly from the constants stored in the UScript.
// 	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(NodeGraph->GetSchema());
// 	ExposedVectorConstants.Empty();
// 	for (int32 ConstIdx = 0; ConstIdx < ExternalConsts.GetNumVectorConstants(); ConstIdx++)
// 	{
// 		FNiagaraVariableInfo Info;
// 		FVector4 Value;
// 		ExternalConsts.GetVectorConstant(ConstIdx, Value, Info);
// 		if (Schema->IsSystemConstant(Info))
// 		{
// 			continue;//System constants are "external" but should not be exposed to the editor.
// 		}
// 			
// 		EditorExposedVectorConstant *Const = new EditorExposedVectorConstant();
// 		Const->ConstName = Info.Name;
// 		Const->Value = Value;
// 		ExposedVectorConstants.Add(MakeShareable(Const));
// 	}

}

FGuid UNiagaraScriptSource::GetChangeID() 
{ 
	return NodeGraph->GetChangeID(); 
}
