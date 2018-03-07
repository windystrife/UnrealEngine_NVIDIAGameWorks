// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackViewModel.h"
#include "NiagaraStackRoot.h"
#include "NiagaraSystemViewModel.h"
#include "NiagaraEmitterViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraEmitterEditorData.h"
#include "NiagaraEmitter.h"

#include "Editor.h"

#define LOCTEXT_NAMESPACE "NiagaraStackViewModel"

TSharedPtr<FNiagaraEmitterViewModel> UNiagaraStackViewModel::GetEmitterViewModel()
{
	return EmitterViewModel;
}

void UNiagaraStackViewModel::Initialize(TSharedPtr<FNiagaraSystemViewModel> InSystemViewModel, TSharedPtr<FNiagaraEmitterViewModel> InEmitterViewModel)
{
	if (EmitterViewModel.IsValid())
	{
		GEditor->UnregisterForUndo(this);
		EmitterViewModel->OnScriptCompiled().RemoveAll(this);
		RootEntry->OnStructureChanged().RemoveAll(this);
		RootEntry->OnDataObjectModified().RemoveAll(this);
		RootEntries.Empty();
		RootEntry = nullptr;
	}

	if (SystemViewModel.IsValid())
	{
		SystemViewModel->OnSystemCompiled().RemoveAll(this);
	}

	SystemViewModel = InSystemViewModel;
	EmitterViewModel = InEmitterViewModel;

	if (SystemViewModel.IsValid() && EmitterViewModel.IsValid() && EmitterViewModel->GetSharedScriptViewModel()->GetGraphViewModel()->GetGraph() != nullptr)
	{
		GEditor->RegisterForUndo(this);

		EmitterViewModel->OnScriptCompiled().AddUObject(this, &UNiagaraStackViewModel::OnEmitterCompiled);
		SystemViewModel->OnSystemCompiled().AddUObject(this, &UNiagaraStackViewModel::OnSystemCompiled);
		
		RootEntry = NewObject<UNiagaraStackRoot>(this);
		RootEntry->Initialize(SystemViewModel.ToSharedRef(), EmitterViewModel.ToSharedRef());
		RootEntry->RefreshChildren();
		RootEntry->OnStructureChanged().AddUObject(this, &UNiagaraStackViewModel::EntryStructureChanged);
		RootEntry->OnDataObjectModified().AddUObject(this, &UNiagaraStackViewModel::EntryDataObjectModified);
		RootEntries.Add(RootEntry);
	}

	StructureChangedDelegate.Broadcast();
}

void UNiagaraStackViewModel::BeginDestroy()
{
	Super::BeginDestroy();
	if (EmitterViewModel.IsValid())
	{
		GEditor->UnregisterForUndo(this);
		EmitterViewModel->OnScriptCompiled().RemoveAll(this);
	}
}

void UNiagaraStackViewModel::OnSystemCompiled()
{
	RootEntry->RefreshChildren();
}

void UNiagaraStackViewModel::OnEmitterCompiled()
{
	RootEntry->RefreshChildren();
}

TArray<UNiagaraStackEntry*>& UNiagaraStackViewModel::GetRootEntries()
{
	return RootEntries;
}

UNiagaraStackViewModel::FOnStructureChanged& UNiagaraStackViewModel::OnStructureChanged()
{
	return StructureChangedDelegate;
}

void UNiagaraStackViewModel::PostUndo(bool bSuccess)
{
	RootEntry->RefreshChildren();
}

void UNiagaraStackViewModel::EntryStructureChanged()
{
	StructureChangedDelegate.Broadcast();
}

void UNiagaraStackViewModel::EntryDataObjectModified(UObject* ChangedObject)
{
	SystemViewModel->NotifyDataObjectChanged(ChangedObject);
}

#undef LOCTEXT_NAMESPACE
