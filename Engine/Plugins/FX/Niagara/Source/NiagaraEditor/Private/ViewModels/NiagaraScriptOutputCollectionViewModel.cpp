// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraScriptOutputCollectionViewModel.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraTypes.h"
#include "NiagaraScriptParameterViewModel.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraEmitter.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "NiagaraScriptOutputCollection"

FNiagaraScriptOutputCollectionViewModel::FNiagaraScriptOutputCollectionViewModel(UNiagaraScript* InScript, ENiagaraParameterEditMode InParameterEditMode)
	: FNiagaraParameterCollectionViewModel(InParameterEditMode)
	, DisplayName(LOCTEXT("DisplayName", "Outputs"))
{
	if (InScript != nullptr)
	{
		Scripts.Add(InScript);
	}

	if (Scripts.Num() == 1 && Scripts[0].IsValid() && Scripts[0]->GetSource())
	{
		Graph = Cast<UNiagaraScriptSource>(Scripts[0]->GetSource())->NodeGraph;
		bCanHaveNumericParameters = Scripts[0]->IsStandaloneScript();
		
		OutputNode = nullptr;
		
		TArray<UNiagaraNodeOutput*> OutputNodes;
		Graph->GetNodesOfClass(OutputNodes);
		if (OutputNodes.Num() == 1)
		{
			OutputNode = OutputNodes[0];
		}

	}
	else
	{
		Graph = nullptr;
		OutputNode = nullptr;
		bCanHaveNumericParameters = true;
	}
	RefreshParameterViewModels();

	if (Graph.IsValid())
	{
		OnGraphChangedHandle = Graph->AddOnGraphChangedHandler(
			FOnGraphChanged::FDelegate::CreateRaw(this, &FNiagaraScriptOutputCollectionViewModel::OnGraphChanged));
	}
}

FNiagaraScriptOutputCollectionViewModel::FNiagaraScriptOutputCollectionViewModel(UNiagaraEmitter* InEmitter, ENiagaraParameterEditMode InParameterEditMode)
	: FNiagaraParameterCollectionViewModel(InParameterEditMode)
	, DisplayName(LOCTEXT("DisplayName", "Outputs"))
{
	TArray<UNiagaraScript*> InScripts;
	InEmitter->GetScripts(InScripts);
	// Because of weak pointers, we need to copy ourselves..
	for (UNiagaraScript* Script : InScripts)
	{
		Scripts.Add(Script);
		ensure(Script->GetSource() == InEmitter->GraphSource);
	}
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(InEmitter->GraphSource);

	if (Source)
	{
		Graph = Cast<UNiagaraScriptSource>(Source)->NodeGraph;
		bCanHaveNumericParameters = false;
		
		OutputNode = Graph->FindOutputNode(ENiagaraScriptUsage::ParticleSpawnScript);
	}
	else
	{
		Graph = nullptr;
		OutputNode = nullptr;
		bCanHaveNumericParameters = false;
	}
	RefreshParameterViewModels();

	if (Graph.IsValid())
	{
		OnGraphChangedHandle = Graph->AddOnGraphChangedHandler(
			FOnGraphChanged::FDelegate::CreateRaw(this, &FNiagaraScriptOutputCollectionViewModel::OnGraphChanged));
	}
}

FNiagaraScriptOutputCollectionViewModel::~FNiagaraScriptOutputCollectionViewModel()
{
	for (int32 i = 0; i < ParameterViewModels.Num(); i++)
	{
		FNiagaraScriptParameterViewModel* ParameterViewModel = (FNiagaraScriptParameterViewModel*)(&ParameterViewModels[i].Get());
		if (ParameterViewModel != nullptr)
		{
			ParameterViewModel->Reset();
			ParameterViewModel->OnNameChanged().RemoveAll(this);
			ParameterViewModel->OnTypeChanged().RemoveAll(this);
		}
	}

	if (Graph.IsValid())
	{
		Graph->RemoveOnGraphChangedHandler(OnGraphChangedHandle);
	}
}

void FNiagaraScriptOutputCollectionViewModel::SetScripts(TArray<UNiagaraScript*> InScripts)
{
	// Remove callback on previously held graph.
	if (Graph.IsValid())
	{
		Graph->RemoveOnGraphChangedHandler(OnGraphChangedHandle);
	}

	Scripts.Empty();
	for (UNiagaraScript* Script : InScripts)
	{
		Scripts.Add(Script);
		check(Script->GetSource() == InScripts[0]->GetSource());
	}

	OutputNode = nullptr;
	if (Scripts.Num() > 0 && Scripts[0].IsValid() && Scripts[0]->GetSource())
	{
		Graph = Cast<UNiagaraScriptSource>(Scripts[0]->GetSource())->NodeGraph;
		OnGraphChangedHandle = Graph->AddOnGraphChangedHandler(
			FOnGraphChanged::FDelegate::CreateRaw(this, &FNiagaraScriptOutputCollectionViewModel::OnGraphChanged));
		bCanHaveNumericParameters = Scripts[0]->IsStandaloneScript();

		if (Scripts.Num() == 1)
		{
			TArray<UNiagaraNodeOutput*> OutputNodes;
			Graph->GetNodesOfClass(OutputNodes);
			if (OutputNodes.Num() == 1)
			{
				OutputNode = OutputNodes[0];
			}
		}
		else
		{
			OutputNode = Graph->FindOutputNode(ENiagaraScriptUsage::ParticleSpawnScript);
		}
	}
	else
	{
		Graph = nullptr;
		OutputNode = nullptr;
		bCanHaveNumericParameters = true;
	}

	RefreshParameterViewModels();
}

FText FNiagaraScriptOutputCollectionViewModel::GetDisplayName() const 
{
	return DisplayName;
}

void FNiagaraScriptOutputCollectionViewModel::AddParameter(TSharedPtr<FNiagaraTypeDefinition> ParameterType)
{
	if (Graph.IsValid() == false)
	{
		return;
	}
	
	TArray<UNiagaraNodeOutput*> OutputNodes;
	Graph->FindOutputNodes(OutputNodes);
	if (OutputNodes.Num() == 0)
	{
		return;
	}

	FScopedTransaction ScopedTransaction(LOCTEXT("AddScriptOutput", "Add script output"));
	FName OutputName = FNiagaraEditorUtilities::GetUniqueName(FName(TEXT("NewOutput")), GetParameterNames());
	for (UNiagaraNodeOutput* OtherOutputNode : OutputNodes)
	{
		OtherOutputNode->Modify();
		OtherOutputNode->Outputs.Add(FNiagaraVariable(*ParameterType.Get(), OutputName));

		OtherOutputNode->NotifyOutputVariablesChanged();
	}
	Graph->MarkGraphRequiresSynchronization();
	OnOutputParametersChangedDelegate.Broadcast();
}

bool FNiagaraScriptOutputCollectionViewModel::CanDeleteParameters() const
{
	return GetSelection().GetSelectedObjects().Num() > 0;
}

void FNiagaraScriptOutputCollectionViewModel::DeleteSelectedParameters()
{
	if (Graph.IsValid() == false)
	{
		return;
	}

	TArray<UNiagaraNodeOutput*> OutputNodes;
	Graph->FindOutputNodes(OutputNodes);
	if (OutputNodes.Num() == 0)
	{
		return;
	}
	
	if(GetSelection().GetSelectedObjects().Num() > 0)
	{
		TSet<FName> OutputNamesToDelete;
		for (TSharedRef<INiagaraParameterViewModel> OutputParameter : GetSelection().GetSelectedObjects())
		{
			OutputNamesToDelete.Add(OutputParameter->GetName());
		}
		GetSelection().ClearSelectedObjects();

		auto DeletePredicate = [&OutputNamesToDelete] (FNiagaraVariable& OutputVariable)
		{ 
			return OutputNamesToDelete.Contains(OutputVariable.GetName());
		};

		FScopedTransaction ScopedTransaction(LOCTEXT("DeletedSelectedNodes", "Delete selected nodes"));
		for (UNiagaraNodeOutput* OtherOutputNode : OutputNodes)
		{
			OtherOutputNode->Modify();
			OtherOutputNode->Outputs.RemoveAll(DeletePredicate);
			OtherOutputNode->NotifyOutputVariablesChanged();
		}
		OnOutputParametersChangedDelegate.Broadcast();
	}
}

const TArray<TSharedRef<INiagaraParameterViewModel>>& FNiagaraScriptOutputCollectionViewModel::GetParameters()
{
	return ParameterViewModels;
}

void FNiagaraScriptOutputCollectionViewModel::RefreshParameterViewModels()
{
	for (int32 i = 0; i < ParameterViewModels.Num(); i++)
	{
		FNiagaraScriptParameterViewModel* ParameterViewModel = (FNiagaraScriptParameterViewModel*)(&ParameterViewModels[i].Get());
		if (ParameterViewModel != nullptr)
		{
			ParameterViewModel->Reset();
			ParameterViewModel->OnNameChanged().RemoveAll(this);
			ParameterViewModel->OnTypeChanged().RemoveAll(this);
		}
	}
	ParameterViewModels.Empty();

	if (OutputNode.IsValid())
	{
		for (FNiagaraVariable& OutputVariable : OutputNode->Outputs)
		{
			TSharedRef<FNiagaraScriptParameterViewModel> ParameterViewModel = MakeShareable(new FNiagaraScriptParameterViewModel(OutputVariable, *OutputNode, nullptr, nullptr, ParameterEditMode));
			ParameterViewModel->OnNameChanged().AddRaw(this, &FNiagaraScriptOutputCollectionViewModel::OnParameterNameChanged, &OutputVariable);
			ParameterViewModel->OnTypeChanged().AddRaw(this, &FNiagaraScriptOutputCollectionViewModel::OnParameterTypeChanged, &OutputVariable);
			ParameterViewModels.Add(ParameterViewModel);
		}
	}

	OnCollectionChangedDelegate.Broadcast();
}

bool FNiagaraScriptOutputCollectionViewModel::SupportsType(const FNiagaraTypeDefinition& Type) const
{
	return Type.GetScriptStruct() != nullptr && (bCanHaveNumericParameters || Type != FNiagaraTypeDefinition::GetGenericNumericDef());
}

FNiagaraScriptOutputCollectionViewModel::FOnOutputParametersChanged& FNiagaraScriptOutputCollectionViewModel::OnOutputParametersChanged()
{
	return OnOutputParametersChangedDelegate;
}

void FNiagaraScriptOutputCollectionViewModel::OnGraphChanged(const struct FEdGraphEditAction& InAction)
{
	RefreshParameterViewModels();
}

void FNiagaraScriptOutputCollectionViewModel::OnParameterNameChanged(FName OldName, FName NewName, FNiagaraVariable* ParameterVariable)
{
	TSet<FName> CurrentNames;

	if (OutputNode.IsValid() == false)
	{
		return;
	}

	for (FNiagaraVariable& OutputVariable : OutputNode->Outputs)
	{
		if (&OutputVariable != ParameterVariable)
		{
			CurrentNames.Add(OutputVariable.GetName());
		}
	}

	// If the new name matches a current output name, rename it to something unique.
	if (CurrentNames.Contains(ParameterVariable->GetName()))
	{
		ParameterVariable->SetName(FNiagaraEditorUtilities::GetUniqueName(ParameterVariable->GetName(), CurrentNames));
	}
	
	OutputNode->NotifyOutputVariablesChanged();

	// Now sync the others in the graph to this one...
	TArray<UNiagaraNodeOutput*> OutputNodes;
	Graph->GetNodesOfClass(OutputNodes);
	for (UNiagaraNodeOutput* GraphOutputNode : OutputNodes)
	{
		if (GraphOutputNode == OutputNode.Get())
		{
			continue;
		} 
		
		GraphOutputNode->Modify();
		GraphOutputNode->Outputs = OutputNode->Outputs;
		GraphOutputNode->NotifyOutputVariablesChanged();
	}
	OnOutputParametersChangedDelegate.Broadcast();
}

void FNiagaraScriptOutputCollectionViewModel::OnParameterTypeChanged(FNiagaraVariable* ParameterVariable)
{
	if (OutputNode.IsValid() == false)
	{
		return;
	}
	OutputNode->NotifyOutputVariablesChanged();

	// Now sync the others in the graph to this one...
	TArray<UNiagaraNodeOutput*> OutputNodes;
	Graph->GetNodesOfClass(OutputNodes);
	for (UNiagaraNodeOutput* GraphOutputNode : OutputNodes)
	{
		if (GraphOutputNode == OutputNode.Get())
		{
			continue;
		}

		GraphOutputNode->Modify();
		GraphOutputNode->Outputs = OutputNode->Outputs;
		GraphOutputNode->NotifyOutputVariablesChanged();
	}

	OnOutputParametersChangedDelegate.Broadcast();
}

void FNiagaraScriptOutputCollectionViewModel::OnParameterValueChangedInternal(FNiagaraVariable* ParameterVariable)
{
	if (OutputNode.IsValid() == false)
	{
		return;
	}
	OutputNode->NotifyOutputVariablesChanged();
	OnOutputParametersChangedDelegate.Broadcast();

	// Now sync the others in the graph to this one...
	TArray<UNiagaraNodeOutput*> OutputNodes;
	Graph->GetNodesOfClass(OutputNodes);
	for (UNiagaraNodeOutput* GraphOutputNode : OutputNodes)
	{
		if (GraphOutputNode == OutputNode.Get())
		{
			continue;
		}

		GraphOutputNode->Modify();
		GraphOutputNode->Outputs = OutputNode->Outputs;
		GraphOutputNode->NotifyOutputVariablesChanged();
	}

	OnParameterValueChanged().Broadcast(ParameterVariable->GetName());
}

#undef LOCTEXT_NAMESPACE // "NiagaraScriptInputCollection"
