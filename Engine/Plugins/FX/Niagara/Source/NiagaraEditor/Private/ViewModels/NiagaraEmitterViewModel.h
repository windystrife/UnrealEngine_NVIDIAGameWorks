// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraScriptViewModel.h"

class UNiagaraEmitter;
class FNiagaraScriptViewModel;
class FNiagaraScriptGraphViewModel;
class UNiagaraEmitterEditorData;
struct FNiagaraEmitterInstance;
struct FNiagaraVariable;

/** The view model for the UNiagaraEmitter objects */
class FNiagaraEmitterViewModel : public TSharedFromThis<FNiagaraEmitterViewModel>,  public TNiagaraViewModelManager<UNiagaraEmitter, FNiagaraEmitterViewModel>
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnEmitterChanged);
	DECLARE_MULTICAST_DELEGATE(FOnPropertyChanged);
	DECLARE_MULTICAST_DELEGATE(FOnScriptCompiled);

public:
	/** Creates a new emitter editor view model with the supplied emitter handle and simulation. */
	FNiagaraEmitterViewModel(UNiagaraEmitter* InEmitter, TWeakPtr<FNiagaraEmitterInstance> InSimulation);
	virtual ~FNiagaraEmitterViewModel();

	/** Reuse this view model with new parameters.*/
	bool Set(UNiagaraEmitter* InEmitter, TWeakPtr<FNiagaraEmitterInstance> InSimulation);

	/** Sets this view model to a different emitter. */
	void SetEmitter(UNiagaraEmitter* InEmitter);

	/** Sets the current simulation for the emitter. */
	void SetSimulation(TWeakPtr<FNiagaraEmitterInstance> InSimulation);

	/** Gets the start time for the emitter. */
	float GetStartTime() const;

	/** Sets the start time for the emitter. */
	void SetStartTime(float InStartTime);

	/** Gets the end time for the emitter. */
	float GetEndTime() const;

	/** Sets the end time for the emitter. */
	void SetEndTime(float InEndTime);

	/** Gets the number of loops for the emitter.  0 for infinite. */
	int32 GetNumLoops() const;

	/** Gets the emitter represented by this view model. */
	UNiagaraEmitter* GetEmitter();

	/** Gets text representing stats for the emitter. */
	//~ TODO: Instead of a single string here, we should probably have separate controls with tooltips etc.
	FText GetStatsText() const;
	
	/** Geta a view model for the update/spawn Script. */
	TSharedRef<FNiagaraScriptViewModel> GetSharedScriptViewModel();

	/** Compiles the spawn and update scripts. */
	void CompileScripts();

	/* Get the latest status of this view-model's script compilation.*/
	ENiagaraScriptCompileStatus GetLatestCompileStatus();

	/** Gets a multicast delegate which is called when the emitter for this view model changes to a different emitter. */
	FOnEmitterChanged& OnEmitterChanged();

	/** Gets a delegate which is called when a property on the emitter changes. */
	FOnPropertyChanged& OnPropertyChanged();

	/** Gets a delegate which is called when the shared script is compiled. */
	FOnScriptCompiled& OnScriptCompiled();
		
	bool GetDirty() const;
	void SetDirty(bool bDirty);

	/** Gets editor specific data which can be stored per emitter.  If this data hasn't been created the default version will be returned. */
	const UNiagaraEmitterEditorData& GetEditorData() const;

	/** Gets editor specific data which is stored per emitter.  If this data hasn't been created then it will be created. */
	UNiagaraEmitterEditorData& GetOrCreateEditorData();

private:

	/** The text format stats display .*/
	static const FText StatsFormat;

	/** The emitter object being displayed by the control .*/
	TWeakObjectPtr<UNiagaraEmitter> Emitter;

	/** The runtime simulation for the emitter being displayed by the control */
	TWeakPtr<FNiagaraEmitterInstance> Simulation;
	
	/** The view model for the update/spawn/event script. */
	TSharedRef<FNiagaraScriptViewModel> SharedScriptViewModel;

	/** A flag to prevent reentrancy when updating selection sets. */
	bool bUpdatingSelectionInternally;

	/** A multicast delegate which is called whenever the emitter for this view model is changed to a different emitter. */
	FOnEmitterChanged OnEmitterChangedDelegate;

	/** A multicast delegate which is called whenever a property on the emitter changes. */
	FOnPropertyChanged OnPropertyChangedDelegate;

	FOnScriptCompiled OnScriptCompiledDelegate;

	ENiagaraScriptCompileStatus LastEventScriptStatus;

	bool bEmitterDirty;

	TNiagaraViewModelManager<UNiagaraEmitter, FNiagaraEmitterViewModel>::Handle RegisteredHandle;
};