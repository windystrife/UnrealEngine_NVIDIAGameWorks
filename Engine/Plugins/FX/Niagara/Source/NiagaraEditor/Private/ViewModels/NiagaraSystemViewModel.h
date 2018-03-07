// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieScenePlayer.h"

#include "EditorUndoClient.h"
#include "GCObject.h"
#include "NiagaraCurveOwner.h"
#include "TNiagaraViewModelManager.h"
#include "ISequencer.h"

#include "TickableEditorObject.h"
#include "ISequencerModule.h"

class UNiagaraSystem;
class UNiagaraComponent;
class UNiagaraSequence;
class UMovieSceneNiagaraEmitterTrack;
struct FNiagaraVariable;
class FNiagaraEmitterHandleViewModel;
class FNiagaraSystemScriptViewModel;
class FNiagaraSystemInstance;
class ISequencer;
struct FAssetData;
class UNiagaraSystemEditorData;
struct FRichCurve;

/** Defines options for the niagara System view model */
struct FNiagaraSystemViewModelOptions
{
	/** Whether or not the user can remove emitters from the timeline. */
	bool bCanRemoveEmittersFromTimeline;

	/** Whether or not the user can rename emitters from the timeline. */
	bool bCanRenameEmittersFromTimeline;

	/** Whether or not the user can add emitters from the timeline. */
	bool bCanAddEmittersFromTimeline;

	/** A delegate which is used to generate the content for the add menu in sequencer. */
	FOnGetAddMenuContent OnGetSequencerAddMenuContent;

	/** Whether or not we use the system's execution state to drive when we reset the timeline*/
	bool bUseSystemExecStateForTimelineReset;
};

/** A view model for viewing and editing a UNiagaraSystem. */
class FNiagaraSystemViewModel 
	: public TSharedFromThis<FNiagaraSystemViewModel>
	, public FGCObject
	, public FEditorUndoClient
	, public FTickableEditorObject
	, public TNiagaraViewModelManager<UNiagaraSystem, FNiagaraSystemViewModel>
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnEmitterHandleViewModelsChanged);

	DECLARE_MULTICAST_DELEGATE(FOnCurveOwnerChanged);

	DECLARE_MULTICAST_DELEGATE(FOnSelectedEmitterHandlesChanged);

	DECLARE_MULTICAST_DELEGATE(FOnPostSequencerTimeChange)

	DECLARE_MULTICAST_DELEGATE(FOnSystemCompiled);

public:
	/** Creates a new view model with the supplied System and System instance. */
	FNiagaraSystemViewModel(UNiagaraSystem& InSystem, FNiagaraSystemViewModelOptions InOptions);

	~FNiagaraSystemViewModel();

	/** Gets an array of the view models for the emitter handles owned by this System. */
	const TArray<TSharedRef<FNiagaraEmitterHandleViewModel>>& GetEmitterHandleViewModels();

	/** Gets the view model for the System script. */
	TSharedRef<FNiagaraSystemScriptViewModel> GetSystemScriptViewModel();

	/** Gets a niagara component for previewing the simulated System. */
	UNiagaraComponent* GetPreviewComponent();

	/** Gets the sequencer for this System for displaying the timeline. */
	TSharedPtr<ISequencer> GetSequencer();

	/** Gets the curve owner for the System represented by this view model, for use with the curve editor widget. */
	FNiagaraCurveOwner& GetCurveOwner();

	/** Get access to the underlying system*/
	UNiagaraSystem& GetSystem() { return System; }

	/** Gets whether or not this system is transient.  This will be true for the system view model in the emitter editor. */
	bool GetSystemIsTransient() const;

	/** Gets whether or not emitters can be added from the timeline. */
	bool GetCanAddEmittersFromTimeline() const;

	/** Adds a new emitter to the System from an emitter asset data. */
	void AddEmitterFromAssetData(const FAssetData& AssetData);

	/** Adds a new emitter to the System. */
	void AddEmitter(UNiagaraEmitter& Emitter);

	/** Duplicates the selected emitter in this System. */
	void DuplicateEmitter(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleToDuplicate);

	/** Deletes the selected emitter from the System. */
	void DeleteEmitter(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleToDelete);

	/** Deletes the emitters with the supplied ids from the system */
	void DeleteEmitters(TSet<FGuid> EmitterHandleIdsToDelete);

	/** Gets a multicast delegate which is called any time the array of emitter handle view models changes. */
	FOnEmitterHandleViewModelsChanged& OnEmitterHandleViewModelsChanged();

	/** Gets a delegate which is called any time the data in the curve owner is changed internally by this view model. */
	FOnCurveOwnerChanged& OnCurveOwnerChanged();

	/** Gets a multicast delegate which is called whenever the selected emitter handles changes. */
	FOnSelectedEmitterHandlesChanged& OnSelectedEmitterHandlesChanged();
	
	/** Gets a multicast delegate which is called whenever we've received and handled a sequencer time update.*/
	FOnPostSequencerTimeChange& OnPostSequencerTimeChanged();

	/** Gets a multicast delegate which is called whenever the system has been compiled. */
	FOnSystemCompiled& OnSystemCompiled();

	//~ FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	//~ FEditorUndoClient interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }

	// ~ FTickableEditorObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	void ResynchronizeAllHandles();

	/** Resets the System instance to initial conditions. */
	void ResetSystem();

	/** Compiles the spawn and update scripts. */
	void CompileSystem();

	/* Get the latest status of this view-model's script compilation.*/
	ENiagaraScriptCompileStatus GetLatestCompileStatus();

	/** Gets the ids for the currently selected emitter handles. */
	const TArray<FGuid>& GetSelectedEmitterHandleIds();

	/** Sets the currently selected emitter handles by id. */
	void SetSelectedEmitterHandlesById(TArray<FGuid> InSelectedEmitterHandleIds);

	/** Sets the currently selected emitter handle by id. */
	void SetSelectedEmitterHandleById(FGuid InSelectedEmitterHandleId);

	/** Gets the currently selected emitter handles. */
	void GetSelectedEmitterHandles(TArray<TSharedRef<FNiagaraEmitterHandleViewModel>>& OutSelectedEmitterHanldles);

	/** Gets editor specific data which can be stored per system.  If this data hasn't been created the default version will be returned. */
	const UNiagaraSystemEditorData& GetEditorData() const;

	/** Gets editor specific data which is stored per system.  If this data hasn't been created then it will be created. */
	UNiagaraSystemEditorData& GetOrCreateEditorData();
	
	/** Act as if the system has been fully destroyed although references might persist.*/
	void Cleanup();

	/** Reinitializes all System instances, and rebuilds emitter handle view models and tracks. */
	void RefreshAll();

	/** Called to notify the system view model that one of the data objects in the system was modified. */
	void NotifyDataObjectChanged(UObject* ChangedObject);
private:

	/** Sets up the preview component and System instance. */
	void SetupPreviewComponentAndInstance();

	/** Rebuilds the emitter handle view models. */
	void RefreshEmitterHandleViewModels();

	/** Rebuilds the sequencer tracks. */
	void RefreshSequencerTracks();

	/** Gets the sequencer emitter track for the supplied emitter handle view model. */
	UMovieSceneNiagaraEmitterTrack* GetTrackForHandleViewModel(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel);

	/** Refreshes sequencer track data from it's emitter handle. */
	void RefreshSequencerTrack(UMovieSceneNiagaraEmitterTrack* EmitterTrack);

	/** Sets up the sequencer for this emitter. */
	void SetupSequencer();

	/** Gets the content for the add menu in sequencer. */
	void GetSequencerAddMenuContent(FMenuBuilder& MenuBuilder, TSharedRef<ISequencer> Sequencer);

	/** Kills all system instances using the system being displayed by this view model. */
	void KillSystemInstances();

	/** Reinitializes the System instance to initial conditions - rebuilds all data sets and resets data interfaces. */
	void ReInitializeSystemInstances();

	/** Resets and rebuilds the data in the curve owner. */
	void ResetCurveData();

	/** Updates the compiled versions of data interfaces when their sources change. */
	void UpdateCompiledDataInterfaces(UNiagaraDataInterface* ChangedDataInterface);

	/** Called whenever a property on the emitter handle changes. */
	void EmitterHandlePropertyChanged(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel);

	/** Called whenever a property on the emitter changes. */
	void EmitterPropertyChanged(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel);

	/** Called when a script is compiled */
	void ScriptCompiled();

	/** Handles when a curve in the curve owner is changed by the curve editor. */
	void CurveChanged(FRichCurve* ChangedCurve, UObject* CurveOwnerObject);

	/** Called whenever the data in the sequence is changed. */
	void SequencerDataChanged(EMovieSceneDataChangeType DataChangeType);

	/** Called whenever the global time in the sequencer is changed. */
	void SequencerTimeChanged();

	/** Called whenever the track selection in sequencer changes. */
	void SequencerTrackSelectionChanged(TArray<UMovieSceneTrack*> SelectedTracks);

	/** Called whenever the section selection in sequencer changes. */
	void SequencerSectionSelectionChanged(TArray<UMovieSceneSection*> SelectedSections);

	/** Updates the current emitter handle selection base on the sequencer selection. */
	void UpdateEmitterHandleSelectionFromSequencer();

	/** Updates the sequencer selection based on the current emitter handle selection. */
	void UpdateSequencerFromEmitterHandleSelection();

	/** Called when the system instance on the preview component changes. */
	void PreviewComponentSystemInstanceChanged();

	/** Called whenever the System instance is initialized. */
	void SystemInstanceInitialized();

private:
	/** The System being viewed and edited by this view model. */
	UNiagaraSystem& System;

	/** The component used for previewing the System in a viewport. */
	UNiagaraComponent* PreviewComponent;

	/** The system instance currently simulating this system if available. */
	FNiagaraSystemInstance* SystemInstance;

	/** The view models for the emitter handles owned by the System. */
	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> EmitterHandleViewModels;

	/** The view model for the System script. */
	TSharedRef<FNiagaraSystemScriptViewModel> SystemScriptViewModel;

	/** A niagara sequence for displaying this System in the sequencer timeline. */
	UNiagaraSequence *NiagaraSequence;

	/** The sequencer instance viewing and editing the niagara sequence. */
	TSharedPtr<ISequencer> Sequencer;

	/** Flag which indicates we are setting the sequencer time directly in an internal operation. */
	bool bSettingSequencerTimeDirectly;

	/** The previous play status for sequencer timeline. */
	EMovieScenePlayerStatus::Type PreviousSequencerStatus;

	/** The previous time for the sequencer timeline. */
	float PreviousSequencerTime;

	/** Whether or not the user can remove emitters from the timeline. */
	bool bCanRemoveEmittersFromTimeline;

	/** Whether or not the user can rename emitters from the timeline. */
	bool bCanRenameEmittersFromTimeline;

	/** Whether or not the user can add emitters from the timeline. */
	bool bCanAddEmittersFromTimeline;

	/** Whether or not we use the system's execution state to drive when we reset the timeline*/
	bool bUseSystemExecStateForTimelineReset;

	/** A delegate which is used to generate the content for the add menu in sequencer. */
	FOnGetAddMenuContent OnGetSequencerAddMenuContent;

	/** A multicast delegate which is called any time the array of emitter handle view models changes. */
	FOnEmitterHandleViewModelsChanged OnEmitterHandleViewModelsChangedDelegate;

	/** A multicase delegate which is called when the contents of the curve owner is changed internally by this view model. */
	FOnCurveOwnerChanged OnCurveOwnerChangedDelegate;

	/** A multicast delegate which is called whenever the selected emitter changes. */
	FOnSelectedEmitterHandlesChanged OnSelectedEmitterHandlesChangedDelegate;

	/** A multicast delegate which is called whenever we've received and handled a sequencer time update.*/
	FOnPostSequencerTimeChange OnPostSequencerTimeChangeDelegate;

	/** A multicast delegate which is called whenever the system has been compiled. */
	FOnSystemCompiled OnSystemCompiledDelegate;

	/** A flag for preventing reentrancy when syncrhonizing sequencer data. */
	bool bUpdatingFromSequencerDataChange;

	/** A flag for preventing reentrancy when synchronizing system selection with sequencer selection */
	bool bUpdatingSystemSelectionFromSequencer;

	/** A flag for preventing reentrancy when synchronizing sequencer selection with system selection */
	bool bUpdatingSequencerSelectionFromSystem;

	/** A curve owner implementation for curves in a niagara System. */
	FNiagaraCurveOwner CurveOwner;

	/** The ids for the currently selected emitter handles. */
	TArray<FGuid> SelectedEmitterHandleIds;

	TNiagaraViewModelManager<UNiagaraSystem, FNiagaraSystemViewModel>::Handle RegisteredHandle;
};