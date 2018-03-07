// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/NotifyHook.h"
#include "Input/Reply.h"
#include "UObject/GCObject.h"
#include "Toolkits/IToolkitHost.h"

#include "AssetEditorToolkit.h"

#include "ISequencer.h"
#include "ISequencerTrackEditor.h"

class FNiagaraSystemInstance;
class FNiagaraSystemViewModel;
class SNiagaraSystemEditorViewport;
class SNiagaraSystemEditorWidget;
class SNiagaraSystemViewport;
class SNiagaraSystemEditor;
class UNiagaraSystem;
class UNiagaraEmitter;
class UNiagaraSequence;
struct FAssetData;
class FMenuBuilder;
class ISequencer;

/** Viewer/editor for a NiagaraSystem
*/
class FNiagaraSystemToolkit : public FAssetEditorToolkit, public FGCObject
{
	enum class ESystemToolkitMode
	{
		System,
		Emitter,
	};

public:
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/** Edits the specified Niagara System */
	void InitializeWithSystem(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraSystem& InSystem);

	/** Edits the specified Niagara Emitter */
	void InitializeWithEmitter(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraEmitter& InEmitter);

	/** Destructor */
	virtual ~FNiagaraSystemToolkit();

	//~ Begin IToolkit Interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	//~ End IToolkit Interface

	//~ FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	FSlateIcon GetCompileStatusImage() const;
	FText GetCompileStatusTooltip() const;

	/** Compiles the system script. */
	void CompileSystem();

protected:
	//~ FAssetEditorToolkit interface
	virtual void GetSaveableObjects(TArray<UObject*>& OutObjects) const override;
	virtual void SaveAsset_Execute() override;
	virtual void SaveAssetAs_Execute() override;
	virtual bool OnRequestClose() override;

private:
	void InitializeInternal(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost);

	void UpdateOriginalEmitter();
	static void UpdateExistingEmitters(TArray<UNiagaraEmitter*> AffectedEmitters);

	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_CurveEd(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Sequencer(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SystemScript(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SystemDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SelectedEmitterStack(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SelectedEmitterGraph(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_DebugSpreadsheet(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_PreviewSettings(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_GeneratedCode(const FSpawnTabArgs& Args);

	/** Builds the toolbar widget */
	void ExtendToolbar();	
	void SetupCommands();

	void ResetSimulation();

	void GetSequencerAddMenuContent(FMenuBuilder& MenuBuilder, TSharedRef<ISequencer> Sequencer);
	TSharedRef<SWidget> CreateAddEmitterMenuContent();
	static TSharedRef<SWidget> GenerateCompileMenuContent();

	void EmitterAssetSelected(const FAssetData& AssetData);
	void ToggleUnlockToChanges();
	bool IsToggleUnlockToChangesChecked();

	FText GetEmitterLockToChangesLabel() const;
	FText GetEmitterLockToChangesLabelTooltip() const;
	FSlateIcon GetEmitterLockToChangesIcon() const;

	static void ToggleCompileEnabled();
	static bool IsAutoCompileEnabled();

private:
	/** The System being edited in system mode, or the placeholder system being edited in emitter mode. */
	UNiagaraSystem* System;

	/** The emitter being edited in emitter mode, or null when editing in system mode. */
	UNiagaraEmitter* Emitter;

	ESystemToolkitMode SystemToolkitMode;

	TSharedPtr<SNiagaraSystemViewport> Viewport;

	/* The view model for the System being edited */
	TSharedPtr<FNiagaraSystemViewModel> SystemViewModel;

	/** The command list for this editor */
	TSharedPtr<FUICommandList> EditorCommands;

	static const FName ViewportTabID;
	static const FName CurveEditorTabID;
	static const FName SequencerTabID;
	static const FName SystemScriptTabID;
	static const FName SystemDetailsTabID;
	static const FName SelectedEmitterStackTabID;
	static const FName SelectedEmitterGraphTabID;
	static const FName DebugSpreadsheetTabID;
	static const FName PreviewSettingsTabId;
	static const FName GeneratedCodeTabID;
};
