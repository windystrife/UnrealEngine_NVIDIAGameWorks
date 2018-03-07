// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterViewModel.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraObjectSelection.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScriptSource.h"
#include "NiagaraEmitterEditorData.h"
#include "NiagaraParameterEditMode.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "EmitterEditorViewModel"

template<> TMap<UNiagaraEmitter*, TArray<FNiagaraEmitterViewModel*>> TNiagaraViewModelManager<UNiagaraEmitter, FNiagaraEmitterViewModel>::ObjectsToViewModels{};

const FText FNiagaraEmitterViewModel::StatsFormat = NSLOCTEXT("NiagaraEmitterViewModel", "StatsFormat", "{0} Particles | {1} ms | {2} MB");
const float Megabyte = 1024.0f * 1024.0f;

FNiagaraEmitterViewModel::FNiagaraEmitterViewModel(UNiagaraEmitter* InEmitter, TWeakPtr<FNiagaraEmitterInstance> InSimulation)
	: Emitter(InEmitter)
	, Simulation(InSimulation)
	, SharedScriptViewModel(MakeShareable(new FNiagaraScriptViewModel(InEmitter, LOCTEXT("SharedDisplayName", "Graph"), ENiagaraParameterEditMode::EditAll)))
	, bUpdatingSelectionInternally(false)
	, LastEventScriptStatus(ENiagaraScriptCompileStatus::NCS_Unknown)
	, bEmitterDirty(false)
{
	if (Emitter.IsValid() && Emitter->EventHandlerScriptProps.Num() != 0 && Emitter->EventHandlerScriptProps[0].Script && Emitter->EventHandlerScriptProps[0].Script->ByteCode.Num() != 0)
	{
		LastEventScriptStatus = ENiagaraScriptCompileStatus::NCS_UpToDate;
	}

	RegisteredHandle = RegisterViewModelWithMap(Emitter.Get(), this);
}


bool FNiagaraEmitterViewModel::Set(UNiagaraEmitter* InEmitter, TWeakPtr<FNiagaraEmitterInstance> InSimulation)
{
	SetEmitter(InEmitter);
	SetSimulation(InSimulation);
	return true;
}

FNiagaraEmitterViewModel::~FNiagaraEmitterViewModel()
{
	SharedScriptViewModel->GetGraphViewModel()->GetSelection()->OnSelectedObjectsChanged().RemoveAll(this);
	UnregisterViewModelWithMap(RegisteredHandle);
}


void FNiagaraEmitterViewModel::SetEmitter(UNiagaraEmitter* InEmitter)
{
	UnregisterViewModelWithMap(RegisteredHandle);

	Emitter = InEmitter;

	RegisteredHandle = RegisterViewModelWithMap(InEmitter, this);

	SharedScriptViewModel->SetScripts(InEmitter);

	OnEmitterChanged().Broadcast();
}


void FNiagaraEmitterViewModel::SetSimulation(TWeakPtr<FNiagaraEmitterInstance> InSimulation)
{
	Simulation = InSimulation;
}


float FNiagaraEmitterViewModel::GetStartTime() const
{
	// TODO, reference values off the parameter store...
	return 0.0f;
}


void FNiagaraEmitterViewModel::SetStartTime(float InStartTime)
{
	// TODO, reference values off the parameter store...
}


float FNiagaraEmitterViewModel::GetEndTime() const
{
	// TODO, reference values off the parameter store...
	return 0.0f;
}


void FNiagaraEmitterViewModel::SetEndTime(float InEndTime)
{
	// TODO, reference values off the parameter store...
}


int32 FNiagaraEmitterViewModel::GetNumLoops() const
{
	// TODO, reference values off the parameter store...
	return 0;
}

UNiagaraEmitter* FNiagaraEmitterViewModel::GetEmitter()
{
	return Emitter.Get();
}


FText FNiagaraEmitterViewModel::GetStatsText() const
{
	if (Simulation.IsValid())
	{
		TSharedPtr<FNiagaraEmitterInstance> SimInstance = Simulation.Pin();
		check(SimInstance.IsValid());
		return FText::Format(StatsFormat,
			FText::AsNumber(SimInstance->GetNumParticles()),
			FText::AsNumber(SimInstance->GetTotalCPUTime()),
			FText::AsNumber(SimInstance->GetTotalBytesUsed() / Megabyte));
	}
	else
	{
		return LOCTEXT("InvalidSimulation", "Simulation is invalid.");
	}
}

TSharedRef<FNiagaraScriptViewModel> FNiagaraEmitterViewModel::GetSharedScriptViewModel()
{
	return SharedScriptViewModel;
}

bool FNiagaraEmitterViewModel::GetDirty() const
{
	bool bDirty = SharedScriptViewModel->GetScriptDirty() || bEmitterDirty;
	return bDirty;
}

void FNiagaraEmitterViewModel::SetDirty(bool bDirty)
{
	SharedScriptViewModel->SetScriptDirty(bDirty);
	bEmitterDirty = bDirty;
}

const UNiagaraEmitterEditorData& FNiagaraEmitterViewModel::GetEditorData() const
{
	check(Emitter.IsValid());

	const UNiagaraEmitterEditorData* EditorData = Cast<UNiagaraEmitterEditorData>(Emitter->EditorData);
	if (EditorData == nullptr)
	{
		EditorData = GetDefault<UNiagaraEmitterEditorData>();
	}
	return *EditorData;
}

UNiagaraEmitterEditorData& FNiagaraEmitterViewModel::GetOrCreateEditorData()
{
	check(Emitter.IsValid());
	UNiagaraEmitterEditorData* EditorData = Cast<UNiagaraEmitterEditorData>(Emitter->EditorData);
	if (EditorData == nullptr)
	{
		EditorData = NewObject<UNiagaraEmitterEditorData>(Emitter.Get(), NAME_None, RF_Transactional);
		Emitter->Modify();
		Emitter->EditorData = EditorData;
	}
	return *EditorData;
}

void FNiagaraEmitterViewModel::CompileScripts()
{
	if (Emitter.IsValid())
	{
		TArray<ENiagaraScriptCompileStatus> CompileStatuses;
		TArray<FString> CompileErrors;
		TArray<FString> CompilePaths;
		TArray<UNiagaraScript*> Scripts;
		TArray<TPair<ENiagaraScriptUsage, int32> > Usages;
		Emitter->CompileScripts(CompileStatuses, CompileErrors, CompilePaths, Scripts);

		ENiagaraScriptCompileStatus AggregateStatus = ENiagaraScriptCompileStatus::NCS_UpToDate;
		FString AggregateErrors;

		for (int32 i = 0; i < CompileStatuses.Num(); i++)
		{
			AggregateStatus = FNiagaraEditorUtilities::UnionCompileStatus(AggregateStatus, CompileStatuses[i]);
			AggregateErrors += CompilePaths[i] + TEXT(" ") + FNiagaraEditorUtilities::StatusToText(CompileStatuses[i]).ToString() + TEXT("\n");
			AggregateErrors += CompileErrors[i] + TEXT("\n");
		}
		SharedScriptViewModel->UpdateCompileStatus(AggregateStatus, AggregateErrors, CompileStatuses, CompileErrors, CompilePaths, Scripts);
	}
	OnScriptCompiled().Broadcast();
}



ENiagaraScriptCompileStatus FNiagaraEmitterViewModel::GetLatestCompileStatus()
{
	ENiagaraScriptCompileStatus UnionStatus = SharedScriptViewModel->GetLatestCompileStatus();
	return UnionStatus;
}


FNiagaraEmitterViewModel::FOnEmitterChanged& FNiagaraEmitterViewModel::OnEmitterChanged()
{
	return OnEmitterChangedDelegate;
}

FNiagaraEmitterViewModel::FOnPropertyChanged& FNiagaraEmitterViewModel::OnPropertyChanged()
{
	return OnPropertyChangedDelegate;
}

FNiagaraEmitterViewModel::FOnScriptCompiled& FNiagaraEmitterViewModel::OnScriptCompiled()
{
	return OnScriptCompiledDelegate;
}

#undef LOCTEXT_NAMESPACE
