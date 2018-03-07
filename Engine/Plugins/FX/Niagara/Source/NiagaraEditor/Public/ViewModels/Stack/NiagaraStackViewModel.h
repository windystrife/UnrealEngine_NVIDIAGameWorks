// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "NiagaraStackViewModel.generated.h"

class FNiagaraSystemViewModel;
class FNiagaraEmitterViewModel;
class UNiagaraStackEntry;
class UNiagaraStackRoot;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackViewModel : public UObject, public FEditorUndoClient
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnStructureChanged);

public:
	TSharedPtr<FNiagaraEmitterViewModel> GetEmitterViewModel();
	void Initialize(TSharedPtr<FNiagaraSystemViewModel> InSystemViewModel, TSharedPtr<FNiagaraEmitterViewModel> InEmitterViewModel);

	virtual void BeginDestroy() override;

	TArray<UNiagaraStackEntry*>& GetRootEntries();

	FOnStructureChanged& OnStructureChanged();

	//~ FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }

private:
	void EntryStructureChanged();
	void EntryDataObjectModified(UObject* ChangedObject);
	void OnSystemCompiled();
	void OnEmitterCompiled();

private:
	TSharedPtr<FNiagaraEmitterViewModel> EmitterViewModel;
	TSharedPtr<FNiagaraSystemViewModel> SystemViewModel;

	TArray<UNiagaraStackEntry*> RootEntries;

	UPROPERTY()
	UNiagaraStackRoot* RootEntry;

	FOnStructureChanged StructureChangedDelegate;
};