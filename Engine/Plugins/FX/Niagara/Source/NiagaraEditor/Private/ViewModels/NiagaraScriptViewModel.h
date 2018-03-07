// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraParameterEditMode.h"
#include "INiagaraCompiler.h"
#include "TNiagaraViewModelManager.h"
#include "EditorUndoClient.h"
#include "NotifyHook.h"

class UNiagaraScript;
class UNiagaraScriptSource;
class INiagaraParameterCollectionViewModel;
class FNiagaraScriptGraphViewModel;
class FNiagaraScriptInputCollectionViewModel;
class FNiagaraScriptOutputCollectionViewModel;
class UNiagaraEmitter;


/** A view model for niagara scripts which manages other script related view models. */
class FNiagaraScriptViewModel : public TSharedFromThis<FNiagaraScriptViewModel>, public FEditorUndoClient, public TNiagaraViewModelManager<UNiagaraScript, FNiagaraScriptViewModel>
{
public:
	FNiagaraScriptViewModel(UNiagaraScript* InScript, FText DisplayName, ENiagaraParameterEditMode InParameterEditMode);
	FNiagaraScriptViewModel(UNiagaraEmitter* InEmitter, FText DisplayName, ENiagaraParameterEditMode InParameterEditMode);

	~FNiagaraScriptViewModel();

	/** Sets the view model to a different script. */
	void SetScript(UNiagaraScript* InScript);

	void SetScripts(UNiagaraEmitter* InEmitter);

	/** Gets the view model for the input parameter collection. */
	TSharedRef<FNiagaraScriptInputCollectionViewModel> GetInputCollectionViewModel();
	
	/** Gets the view model for the output parameter collection. */
	TSharedRef<FNiagaraScriptOutputCollectionViewModel> GetOutputCollectionViewModel();

	/** Gets the view model for the graph. */
	TSharedRef<FNiagaraScriptGraphViewModel> GetGraphViewModel();

	/** Updates the script with the latest compile status. */
	void UpdateCompileStatus(ENiagaraScriptCompileStatus InAggregateCompileStatus, const FString& InAggregateCompileErrors,
		const TArray<ENiagaraScriptCompileStatus>& InCompileStatuses, const TArray<FString>& InCompileErrors, const TArray<FString>& InCompilePaths,
		const TArray<UNiagaraScript*>& InCompileSources);

	/** Compiles a script that isn't part of an emitter or System. */
	void CompileStandaloneScript();

	/** Get the latest status of this view-model's script compilation.*/
	ENiagaraScriptCompileStatus GetLatestCompileStatus();

	/** Refreshes the nodes in the script graph, updating the pins to match external changes. */
	void RefreshNodes();

	//~ FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }

	bool GetScriptDirty() const { return bNeedsSave; }

	void SetScriptDirty(bool InNeedsSave) { bNeedsSave = InNeedsSave; }

	const UNiagaraScript* GetScript(ENiagaraScriptUsage InUsage) const;

	ENiagaraScriptCompileStatus GetScriptCompileStatus(ENiagaraScriptUsage InUsage, int32 InOccurrence) const;
	FText GetScriptErrors(ENiagaraScriptUsage InUsage, int32 InOccurrence) const;

	/** Updates the compiled versions of data interfaces from changes to their source. */
	void UpdateCompiledDataInterfaces(UNiagaraDataInterface* ChangedDataInterface);

private:
	/** Handles the selection changing in the graph view model. */
	void GraphViewModelSelectedNodesChanged();

	/** Handles the selection changing in the input collection view model. */
	void InputViewModelSelectionChanged();

    /** Handle the graph being changed by the UI notifications to see if we need to mark as needing recompile.*/
	void OnGraphChanged(const struct FEdGraphEditAction& InAction);

	void SetScripts(UNiagaraScriptSource* InScriptSource, TArray<UNiagaraScript*>& InScripts);

protected:
	/** The script which provides the data for this view model. */
	TArray<TWeakObjectPtr<UNiagaraScript>> Scripts;

	TWeakObjectPtr<UNiagaraScriptSource> Source;

	/** The view model for the input parameter collection. */
	TSharedRef<FNiagaraScriptInputCollectionViewModel> InputCollectionViewModel;

	/** The view model for the output parameter collection .*/
	TSharedRef<FNiagaraScriptOutputCollectionViewModel> OutputCollectionViewModel;

	/** The view model for the graph. */
	TSharedRef<FNiagaraScriptGraphViewModel> GraphViewModel;

	/** A flag for preventing reentrancy when synchronizing selection. */
	bool bUpdatingSelectionInternally;

	/** The stored latest compile status.*/
	ENiagaraScriptCompileStatus LastCompileStatus;
	
	/** The handle to the graph changed delegate needed for removing. */
	FDelegateHandle OnGraphChangedHandle;

	/** An edit has been made since the last save.*/
	bool bNeedsSave;

	TArray<TNiagaraViewModelManager<UNiagaraScript, FNiagaraScriptViewModel>::Handle> RegisteredHandles;

	bool IsGraphDirty() const;

	TArray<ENiagaraScriptCompileStatus> CompileStatuses;
	TArray<FString> CompileErrors;
	TArray<FString> CompilePaths;
	TArray<TPair<ENiagaraScriptUsage, int32> > CompileTypes;
};