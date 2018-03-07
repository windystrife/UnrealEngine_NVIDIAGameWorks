// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraScriptViewModel.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraEmitter.h"
#include "NiagaraScriptInputCollectionViewModel.h"
#include "NiagaraScriptOutputCollectionViewModel.h"
#include "NiagaraNodeInput.h"
#include "INiagaraCompiler.h"
#include "CompilerResultsLog.h"
#include "GraphEditAction.h"
#include "Package.h"
#include "Editor.h"
#include "TNiagaraViewModelManager.h"
#include "NiagaraDataInterface.h"

template<> TMap<UNiagaraScript*, TArray<FNiagaraScriptViewModel*>> TNiagaraViewModelManager<UNiagaraScript, FNiagaraScriptViewModel>::ObjectsToViewModels{};

FNiagaraScriptViewModel::FNiagaraScriptViewModel(UNiagaraScript* InScript, FText DisplayName, ENiagaraParameterEditMode InParameterEditMode)
	: InputCollectionViewModel(MakeShareable(new FNiagaraScriptInputCollectionViewModel(InScript, DisplayName, InParameterEditMode)))
	, OutputCollectionViewModel(MakeShareable(new FNiagaraScriptOutputCollectionViewModel(InScript, InParameterEditMode)))
	, GraphViewModel(MakeShareable(new FNiagaraScriptGraphViewModel(Cast<UNiagaraScriptSource>(InScript->GetSource()), DisplayName)))
	, bUpdatingSelectionInternally(false)
	, LastCompileStatus(ENiagaraScriptCompileStatus::NCS_Unknown)
	, bNeedsSave(true)
{
	if (InScript != nullptr)
	{
		Scripts.Add(InScript);
		Source = Cast<UNiagaraScriptSource>(InScript->GetSource());
	}

	InputCollectionViewModel->GetSelection().OnSelectedObjectsChanged().AddRaw(this, &FNiagaraScriptViewModel::InputViewModelSelectionChanged);
	GraphViewModel->GetSelection()->OnSelectedObjectsChanged().AddRaw(this, &FNiagaraScriptViewModel::GraphViewModelSelectedNodesChanged);
	GEditor->RegisterForUndo(this);
	
	if (Source != nullptr)
	{
		UNiagaraGraph* Graph = Source->NodeGraph;
		if (Graph != nullptr)
		{
			OnGraphChangedHandle = Graph->AddOnGraphChangedHandler(
				FOnGraphChanged::FDelegate::CreateRaw(this, &FNiagaraScriptViewModel::OnGraphChanged));
		}
	}

	// Guess at initial compile status
	if (InScript != nullptr && InScript->ByteCode.Num() == 0) // This is either a brand new script or failed in the past. Since we create a default working script, assume invalid.
	{
		bNeedsSave = true;
		LastCompileStatus = ENiagaraScriptCompileStatus::NCS_Error;
		GraphViewModel->SetErrorTextToolTip("Please recompile for full error stack.");
	}
	else // Possibly warnings previously, but still compiled. It *could* have been dirtied somehow, but we assume that it is up-to-date.
	{
		if (InScript != nullptr && InScript->AreScriptAndSourceSynchronized())
		{
			bNeedsSave = false;
			LastCompileStatus = InScript->GetLastCompileStatus();
		}
		else
		{
			bNeedsSave = true;
			LastCompileStatus = ENiagaraScriptCompileStatus::NCS_Error;
			GraphViewModel->SetErrorTextToolTip("Please recompile for full error stack.");
		}
	}

	RegisteredHandles.Add(RegisterViewModelWithMap(InScript, this));
}

FNiagaraScriptViewModel::FNiagaraScriptViewModel(UNiagaraEmitter* InEmitter, FText DisplayName, ENiagaraParameterEditMode InParameterEditMode)
	: InputCollectionViewModel(MakeShareable(new FNiagaraScriptInputCollectionViewModel(InEmitter, DisplayName, InParameterEditMode)))
	, OutputCollectionViewModel(MakeShareable(new FNiagaraScriptOutputCollectionViewModel(InEmitter, InParameterEditMode)))
	, GraphViewModel(MakeShareable(new FNiagaraScriptGraphViewModel(Cast<UNiagaraScriptSource>(InEmitter->GraphSource), DisplayName)))
	, bUpdatingSelectionInternally(false)
	, LastCompileStatus(ENiagaraScriptCompileStatus::NCS_Unknown)
	, bNeedsSave(true)
{
	TArray<UNiagaraScript*> InScripts;
	InEmitter->GetScripts(InScripts);
	// Because of weak pointers, we need to copy ourselves..
	for (UNiagaraScript* Script : InScripts)
	{
		Scripts.Add(Script);
		check(Script->GetSource() == InEmitter->GraphSource);
	}
	Source = Cast<UNiagaraScriptSource>(InEmitter->GraphSource);

	InputCollectionViewModel->GetSelection().OnSelectedObjectsChanged().AddRaw(this, &FNiagaraScriptViewModel::InputViewModelSelectionChanged);
	GraphViewModel->GetSelection()->OnSelectedObjectsChanged().AddRaw(this, &FNiagaraScriptViewModel::GraphViewModelSelectedNodesChanged);
	GEditor->RegisterForUndo(this);

	UNiagaraGraph* Graph = Source->NodeGraph;
	if (Graph != nullptr)
	{
		OnGraphChangedHandle = Graph->AddOnGraphChangedHandler(
			FOnGraphChanged::FDelegate::CreateRaw(this, &FNiagaraScriptViewModel::OnGraphChanged));
	}

	// Guess at initial compile status
	LastCompileStatus = ENiagaraScriptCompileStatus::NCS_UpToDate;
	bNeedsSave = false;

	CompileStatuses.Empty();
	CompileErrors.Empty();
	CompilePaths.Empty();

	for (int32 i = 0; i < Scripts.Num(); i++)
	{
		FString Message;
		ENiagaraScriptCompileStatus ScriptStatus = Scripts[i]->GetLastCompileStatus();
		if (Scripts[i].IsValid() && Scripts[i]->ByteCode.Num() == 0) // This is either a brand new script or failed in the past. Since we create a default working script, assume invalid.
		{
			bNeedsSave = true;
			LastCompileStatus = FNiagaraEditorUtilities::UnionCompileStatus(LastCompileStatus, ENiagaraScriptCompileStatus::NCS_Error);
			ScriptStatus = ENiagaraScriptCompileStatus::NCS_Error;
			Message = TEXT("Please recompile for full error stack.");
			GraphViewModel->SetErrorTextToolTip(Message);
		}
		else // Possibly warnings previously, but still compiled. It *could* have been dirtied somehow, but we assume that it is up-to-date.
		{
			if (Scripts[i].IsValid() && Scripts[i]->AreScriptAndSourceSynchronized())
			{
				LastCompileStatus = FNiagaraEditorUtilities::UnionCompileStatus(LastCompileStatus, Scripts[i]->GetLastCompileStatus());
			}
			else
			{
				bNeedsSave = true;
				LastCompileStatus = FNiagaraEditorUtilities::UnionCompileStatus(LastCompileStatus, ENiagaraScriptCompileStatus::NCS_Error);
				ScriptStatus = ENiagaraScriptCompileStatus::NCS_Error;
				Message = TEXT("Please recompile for full error stack.");
				GraphViewModel->SetErrorTextToolTip(Message);
			}
		}

		CompilePaths.Add(Scripts[i]->GetPathName());
		CompileErrors.Add(Message);
		CompileStatuses.Add(ScriptStatus);

		RegisteredHandles.Add(RegisterViewModelWithMap(Scripts[i].Get(), this));
	}

	CompileTypes.SetNum(CompileStatuses.Num());
	for (int32 i = 0; i < CompileStatuses.Num(); i++)
	{
		CompileTypes[i].Key = Scripts[i]->GetUsage();
		if (i > 0 && CompileTypes[i - 1].Key == Scripts[i]->GetUsage())
			CompileTypes[i].Value = CompileTypes[i - 1].Value + 1;
		else
			CompileTypes[i].Value = 0;
	}
}

bool FNiagaraScriptViewModel::IsGraphDirty() const
{
	for (int32 i = 0; i < Scripts.Num(); i++)
	{
		if (!Scripts[i].IsValid())
		{
			continue;
		}

		if (!Scripts[i]->AreScriptAndSourceSynchronized())
		{
			return true;
		}
	}
	return false;
}


FNiagaraScriptViewModel::~FNiagaraScriptViewModel()
{
	InputCollectionViewModel->GetSelection().OnSelectedObjectsChanged().RemoveAll(this);
	GraphViewModel->GetSelection()->OnSelectedObjectsChanged().RemoveAll(this);

	if (Source.IsValid() && Source != nullptr)
	{
		UNiagaraGraph* Graph = Source->NodeGraph;
		if (Graph != nullptr)
		{
			Graph->RemoveOnGraphChangedHandler(OnGraphChangedHandle);
		}
	}

	GEditor->UnregisterForUndo(this);

	for (TNiagaraViewModelManager<UNiagaraScript, FNiagaraScriptViewModel>::Handle RegisteredHandle : RegisteredHandles)
	{
		UnregisterViewModelWithMap(RegisteredHandle);
	}
}


void FNiagaraScriptViewModel::SetScripts(UNiagaraScriptSource* InScriptSource, TArray<UNiagaraScript*>& InScripts)
{
	// Remove the graph changed handler on the child.
	if (Source.IsValid())
	{
		UNiagaraGraph* Graph = Source->NodeGraph;
		if (Graph != nullptr)
		{
			Graph->RemoveOnGraphChangedHandler(OnGraphChangedHandle);
		}
	}
	else
	{
		Source = nullptr;
	}

	for (TNiagaraViewModelManager<UNiagaraScript, FNiagaraScriptViewModel>::Handle RegisteredHandle : RegisteredHandles)
	{
		UnregisterViewModelWithMap(RegisteredHandle);
	}
	RegisteredHandles.Empty();

	Scripts.Empty();
	for (UNiagaraScript* Script : InScripts)
	{
		Scripts.Add(Script);
		check(Script->GetSource() == InScriptSource);
	}
	Source = InScriptSource;

	InputCollectionViewModel->SetScripts(InScripts);
	OutputCollectionViewModel->SetScripts(InScripts);
	GraphViewModel->SetScriptSource(Source.Get());

	UNiagaraGraph* Graph = nullptr;
	if (Source.IsValid() && Source != nullptr)
	{
		// The underlying graph may have changed after the previous call.
		Graph = Source->NodeGraph;
		if (Graph != nullptr)
		{
			OnGraphChangedHandle = Graph->AddOnGraphChangedHandler(
				FOnGraphChanged::FDelegate::CreateRaw(this, &FNiagaraScriptViewModel::OnGraphChanged));
		}
	}
	
	// Guess at initial compile status
	LastCompileStatus = ENiagaraScriptCompileStatus::NCS_UpToDate;
	bNeedsSave = false;

	CompileStatuses.Empty();
	CompileErrors.Empty();
	CompilePaths.Empty();

	for (int32 i = 0; i < Scripts.Num(); i++)
	{
		FString Message;
		ENiagaraScriptCompileStatus ScriptStatus = Scripts[i]->GetLastCompileStatus();
		if (Scripts[i].IsValid() && Scripts[i]->ByteCode.Num() == 0) // This is either a brand new script or failed in the past. Since we create a default working script, assume invalid.
		{
			bNeedsSave = true;
			LastCompileStatus = FNiagaraEditorUtilities::UnionCompileStatus(LastCompileStatus, ENiagaraScriptCompileStatus::NCS_Error);
			ScriptStatus = ENiagaraScriptCompileStatus::NCS_Error;
			Message = TEXT("Please recompile for full error stack.");
			GraphViewModel->SetErrorTextToolTip(Message);
		}
		else // Possibly warnings previously, but still compiled. It *could* have been dirtied somehow, but we assume that it is up-to-date.
		{
			if (Scripts[i].IsValid() && Scripts[i]->AreScriptAndSourceSynchronized())
			{
				LastCompileStatus = FNiagaraEditorUtilities::UnionCompileStatus(LastCompileStatus, Scripts[i]->GetLastCompileStatus());
			}
			else
			{
				bNeedsSave = true;
				LastCompileStatus = FNiagaraEditorUtilities::UnionCompileStatus(LastCompileStatus, ENiagaraScriptCompileStatus::NCS_Error);
				ScriptStatus = ENiagaraScriptCompileStatus::NCS_Error;
				Message = TEXT("Please recompile for full error stack.");
				GraphViewModel->SetErrorTextToolTip(Message);
			}
		}

		CompilePaths.Add(Scripts[i]->GetPathName());
		CompileErrors.Add(Message);
		CompileStatuses.Add(ScriptStatus);

		RegisteredHandles.Add(RegisterViewModelWithMap(Scripts[i].Get(), this));
	}

	CompileTypes.SetNum(CompileStatuses.Num());
	for (int32 i = 0; i < CompileStatuses.Num(); i++)
	{
		CompileTypes[i].Key = Scripts[i]->GetUsage();
		if (i > 0 && CompileTypes[i - 1].Key == Scripts[i]->GetUsage())
			CompileTypes[i].Value = CompileTypes[i - 1].Value + 1;
		else
			CompileTypes[i].Value = 0;
	}
}

void FNiagaraScriptViewModel::SetScripts(UNiagaraEmitter* InEmitter)
{
	if (InEmitter == nullptr)
	{
		TArray<UNiagaraScript*> EmptyScripts;
		SetScripts(nullptr, EmptyScripts);
	}
	else
	{
		TArray<UNiagaraScript*> InScripts;
		InEmitter->GetScripts(InScripts);
		SetScripts(Cast<UNiagaraScriptSource>(InEmitter->GraphSource), InScripts);
	}
}

void FNiagaraScriptViewModel::SetScript(UNiagaraScript* InScript)
{
	TArray<UNiagaraScript*> InScripts;
	UNiagaraScriptSource* InSource = nullptr;
	if (InScript)
	{
		InScripts.Add(InScript);
		InSource = Cast<UNiagaraScriptSource>(InScript->GetSource());
	}
	SetScripts(InSource, InScripts);
}

void FNiagaraScriptViewModel::OnGraphChanged(const struct FEdGraphEditAction& InAction)
{
	if ((InAction.Action & GRAPHACTION_AddNode) != 0 || (InAction.Action & GRAPHACTION_RemoveNode) != 0 ||
		(InAction.Action & GRAPHACTION_GenericNeedsRecompile) != 0)
	{
		for (int32 i = 0; i < Scripts.Num(); i++)
		{
			if (Scripts[i].IsValid())
			{
				Scripts[i]->MarkScriptAndSourceDesynchronized();
			}
		}
		bNeedsSave = true;
	}
}

TSharedRef<FNiagaraScriptInputCollectionViewModel> FNiagaraScriptViewModel::GetInputCollectionViewModel()
{
	return InputCollectionViewModel;
}


TSharedRef<FNiagaraScriptOutputCollectionViewModel> FNiagaraScriptViewModel::GetOutputCollectionViewModel()
{
	return OutputCollectionViewModel;
}

TSharedRef<FNiagaraScriptGraphViewModel> FNiagaraScriptViewModel::GetGraphViewModel()
{
	return GraphViewModel;
}

void FNiagaraScriptViewModel::UpdateCompileStatus(ENiagaraScriptCompileStatus InAggregateCompileStatus, const FString& InAggregateCompileErrorString,
	const TArray<ENiagaraScriptCompileStatus>& InCompileStatuses, const TArray<FString>& InCompileErrors, const TArray<FString>& InCompilePaths,
	const TArray<UNiagaraScript*>& InCompileSources)
{
	if (Source.IsValid() && Source != nullptr)
	{
		CompileStatuses = InCompileStatuses;
		CompileErrors = InCompileErrors;
		CompilePaths = InCompilePaths;

		CompileTypes.SetNum(CompileStatuses.Num());
		for (int32 i = 0; i < CompileStatuses.Num(); i++)
		{
			CompileTypes[i].Key = InCompileSources[i]->GetUsage();
			if (i > 0 && CompileTypes[i - 1].Key == InCompileSources[i]->GetUsage())
				CompileTypes[i].Value = CompileTypes[i - 1].Value + 1;
			else
				CompileTypes[i].Value = 0;
		}

		LastCompileStatus = InAggregateCompileStatus;
		bNeedsSave = true;
		
		if (LastCompileStatus == ENiagaraScriptCompileStatus::NCS_Error)
		{
			GraphViewModel->SetErrorTextToolTip(InAggregateCompileErrorString + "\n(These same errors are also in the log)");
		}
		else
		{
			GraphViewModel->SetErrorTextToolTip("");
		}

		InputCollectionViewModel->RefreshParameterViewModels();
		OutputCollectionViewModel->RefreshParameterViewModels();
	}
}

ENiagaraScriptCompileStatus FNiagaraScriptViewModel::GetScriptCompileStatus(ENiagaraScriptUsage InUsage, int32 InOccurrence) const
{
	ENiagaraScriptCompileStatus Status = ENiagaraScriptCompileStatus::NCS_Unknown;
	for (int32 i = 0; i < CompileTypes.Num(); i++)
	{
		if (UNiagaraScript::IsEquivalentUsage(CompileTypes[i].Key, InUsage) && CompileTypes[i].Value == InOccurrence)
		{
			return CompileStatuses[i];
		}
	}
	return Status;
}

FText FNiagaraScriptViewModel::GetScriptErrors(ENiagaraScriptUsage InUsage, int32 InOccurrence) const
{
	FText Text = FText::GetEmpty();
	for (int32 i = 0; i < CompileTypes.Num(); i++)
	{
		if (UNiagaraScript::IsEquivalentUsage(CompileTypes[i].Key, InUsage) && CompileTypes[i].Value == InOccurrence)
		{
			return FText::FromString(CompileErrors[i]);
		}
	}
	return Text;
}

void FNiagaraScriptViewModel::UpdateCompiledDataInterfaces(UNiagaraDataInterface* ChangedDataInterface)
{
	FName ChangedInputName = NAME_None;
	TArray<UNiagaraNodeInput*> InputNodes;
	GetGraphViewModel()->GetGraph()->GetNodesOfClass<UNiagaraNodeInput>(InputNodes);
	for (UNiagaraNodeInput* InputNode : InputNodes)
	{
		if (InputNode->DataInterface == ChangedDataInterface)
		{
			ChangedInputName = InputNode->Input.GetName();
			break;
		}
	}

	if (ChangedInputName != NAME_None)
	{
		for (TWeakObjectPtr<UNiagaraScript> Script : Scripts)
		{
			if (Script.IsValid())
			{
				for (FNiagaraScriptDataInterfaceInfo& DataInterfaceInfo : Script->DataInterfaceInfo)
				{
					if (DataInterfaceInfo.Name == ChangedInputName)
					{
						ChangedDataInterface->CopyTo(DataInterfaceInfo.DataInterface);
					}
				}
			}
		}
	}
}

const UNiagaraScript* FNiagaraScriptViewModel::GetScript(ENiagaraScriptUsage InUsage) const
{
	for (int32 i = 0; i < Scripts.Num(); i++)
	{
		if (Scripts[i]->IsEquivalentUsage(InUsage))
		{
			return Scripts[i].Get();
		}
	}
	return nullptr;
}

void FNiagaraScriptViewModel::CompileStandaloneScript()
{
	if (Source.IsValid() && Source != nullptr && Scripts.Num() == 1 && Scripts[0].IsValid () && (Scripts[0]->IsStandaloneScript()))
	{
		FString ErrorMsg;
		LastCompileStatus = Scripts[0]->Compile(ErrorMsg);
		Scripts[0]->CacheResourceShadersForRendering(true);
		bNeedsSave = true;

		if (LastCompileStatus == ENiagaraScriptCompileStatus::NCS_Error)
		{
			GraphViewModel->SetErrorTextToolTip(ErrorMsg + "\n(These same errors are also in the log)");
		}
		else
		{
			GraphViewModel->SetErrorTextToolTip("");
		}

		InputCollectionViewModel->RefreshParameterViewModels();
		OutputCollectionViewModel->RefreshParameterViewModels();
	}
	else
	{
		ensure(0); // Should not call this for a script that isn't standalone.
	}
}


ENiagaraScriptCompileStatus FNiagaraScriptViewModel::GetLatestCompileStatus()
{
	if (GraphViewModel->GetGraph() && IsGraphDirty())
		return ENiagaraScriptCompileStatus::NCS_Dirty;
	return LastCompileStatus;
}

void FNiagaraScriptViewModel::RefreshNodes()
{
	if (GraphViewModel->GetGraph())
	{
		TArray<UNiagaraNode*> NiagaraNodes;
		GraphViewModel->GetGraph()->GetNodesOfClass<UNiagaraNode>(NiagaraNodes);
		for (UNiagaraNode* NiagaraNode : NiagaraNodes)
		{
			if (NiagaraNode->RefreshFromExternalChanges())
			{
				for (int32 i = 0; i < Scripts.Num(); i++)
				{
					if (Scripts[i].IsValid())
					{
						Scripts[i]->MarkScriptAndSourceDesynchronized();
					}
				}
			}
			bNeedsSave = true;
		}
	}
}

void FNiagaraScriptViewModel::PostUndo(bool bSuccess)
{
	InputCollectionViewModel->RefreshParameterViewModels();
	OutputCollectionViewModel->RefreshParameterViewModels();
}

void FNiagaraScriptViewModel::GraphViewModelSelectedNodesChanged()
{
	if (bUpdatingSelectionInternally == false)
	{
		bUpdatingSelectionInternally = true;
		{
			TSet<FName> SelectedInputIds;
			for (UObject* SelectedObject : GraphViewModel->GetSelection()->GetSelectedObjects())
			{
				UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(SelectedObject);
				if (InputNode != nullptr)
				{
					SelectedInputIds.Add(InputNode->Input.GetName());
				}
			}

			TSet<TSharedRef<INiagaraParameterViewModel>> ParametersToSelect;
			for (TSharedRef<INiagaraParameterViewModel> Parameter : InputCollectionViewModel->GetParameters())
			{
				if (SelectedInputIds.Contains(Parameter->GetName()))
				{
					ParametersToSelect.Add(Parameter);
				}
			}

			InputCollectionViewModel->GetSelection().SetSelectedObjects(ParametersToSelect);
		}
		bUpdatingSelectionInternally = false;
	}
}

void FNiagaraScriptViewModel::InputViewModelSelectionChanged()
{
	if (bUpdatingSelectionInternally == false)
	{
		bUpdatingSelectionInternally = true;
		{
			TSet<FName> SelectedInputIds;
			for (TSharedRef<INiagaraParameterViewModel> SelectedParameter : InputCollectionViewModel->GetSelection().GetSelectedObjects())
			{
				SelectedInputIds.Add(SelectedParameter->GetName());
			}

			TArray<UNiagaraNodeInput*> InputNodes;
			if (GraphViewModel->GetGraph())
			{
				GraphViewModel->GetGraph()->GetNodesOfClass(InputNodes);
			}
			TSet<UObject*> NodesToSelect;
			for (UNiagaraNodeInput* InputNode : InputNodes)
			{
				if (SelectedInputIds.Contains(InputNode->Input.GetName()))
				{
					NodesToSelect.Add(InputNode);
				}
			}

			GraphViewModel->GetSelection()->SetSelectedObjects(NodesToSelect);
		}
		bUpdatingSelectionInternally = false;
	}
}