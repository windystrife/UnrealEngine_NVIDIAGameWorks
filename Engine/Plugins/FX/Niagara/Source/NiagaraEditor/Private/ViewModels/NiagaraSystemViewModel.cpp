// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraSystemViewModel.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitter.h"
#include "NiagaraScriptSource.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitterHandleViewModel.h"
#include "NiagaraEmitterViewModel.h"
#include "NiagaraSystemScriptViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraSequence.h"
#include "MovieSceneNiagaraEmitterTrack.h"
#include "MovieSceneNiagaraEmitterSection.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEditorSettings.h"
#include "NiagaraSystemEditorData.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeInput.h"
#include "NiagaraEditorModule.h"

#include "Editor.h"

#include "ScopedTransaction.h"
#include "MovieScene.h"
#include "ISequencerModule.h"
#include "EditorSupportDelegates.h"
#include "ModuleManager.h"
#include "TNiagaraViewModelManager.h"
#include "Math/NumericLimits.h"
#include "NiagaraComponent.h"
#include "MovieSceneFolder.h"

DECLARE_CYCLE_STAT(TEXT("SystemViewModel - CompileSystem"), STAT_NiagaraEditor_SystemViewModel_CompileSystem, STATGROUP_NiagaraEditor);

#define LOCTEXT_NAMESPACE "NiagaraSystemViewModel"

template<> TMap<UNiagaraSystem*, TArray<FNiagaraSystemViewModel*>> TNiagaraViewModelManager<UNiagaraSystem, FNiagaraSystemViewModel>::ObjectsToViewModels{};

FNiagaraSystemViewModel::FNiagaraSystemViewModel(UNiagaraSystem& InSystem, FNiagaraSystemViewModelOptions InOptions)
	: System(InSystem)
	, PreviewComponent(nullptr)
	, SystemInstance(nullptr)
	, SystemScriptViewModel(MakeShareable(new FNiagaraSystemScriptViewModel(System)))
	, NiagaraSequence(nullptr)
	, bSettingSequencerTimeDirectly(false)
	, bCanRemoveEmittersFromTimeline(InOptions.bCanRemoveEmittersFromTimeline)
	, bCanRenameEmittersFromTimeline(InOptions.bCanRenameEmittersFromTimeline)
	, bCanAddEmittersFromTimeline(InOptions.bCanAddEmittersFromTimeline)
	, bUseSystemExecStateForTimelineReset(InOptions.bUseSystemExecStateForTimelineReset)
	, OnGetSequencerAddMenuContent(InOptions.OnGetSequencerAddMenuContent)
	, bUpdatingFromSequencerDataChange(false)
	, bUpdatingSystemSelectionFromSequencer(false)
	, bUpdatingSequencerSelectionFromSystem(false)
{
	SetupPreviewComponentAndInstance();
	SetupSequencer();
	RefreshAll();
	GEditor->RegisterForUndo(this);
	RegisteredHandle = RegisterViewModelWithMap(&InSystem, this);
}

void FNiagaraSystemViewModel::Cleanup()
{
	UE_LOG(LogNiagaraEditor, Warning, TEXT("Cleanup System view model %p"), this);

	if (PreviewComponent)
	{
		PreviewComponent->OnSystemInstanceChanged().RemoveAll(this);
	}

	CurveOwner.EmptyCurves();

	GEditor->UnregisterForUndo(this);

	// Make sure that we clear out all of our event handlers
	UnregisterViewModelWithMap(RegisteredHandle);

	for (TSharedRef<FNiagaraEmitterHandleViewModel>& HandleRef : EmitterHandleViewModels)
	{
		HandleRef->OnPropertyChanged().RemoveAll(this);
		HandleRef->GetEmitterViewModel()->OnPropertyChanged().RemoveAll(this);
		HandleRef->GetEmitterViewModel()->OnScriptCompiled().RemoveAll(this);
	}
	EmitterHandleViewModels.Empty();

	if (Sequencer.IsValid())
	{
		Sequencer->OnMovieSceneDataChanged().RemoveAll(this);
		Sequencer->OnGlobalTimeChanged().RemoveAll(this);
		Sequencer->GetSelectionChangedTracks().RemoveAll(this);
		Sequencer->GetSelectionChangedSections().RemoveAll(this);
		Sequencer.Reset();
	}

	PreviewComponent = nullptr;
}

FNiagaraSystemViewModel::~FNiagaraSystemViewModel()
{
	Cleanup();

	UE_LOG(LogNiagaraEditor, Warning, TEXT("Deleting System view model %p"), this);
}

const TArray<TSharedRef<FNiagaraEmitterHandleViewModel>>& FNiagaraSystemViewModel::GetEmitterHandleViewModels()
{
	return EmitterHandleViewModels;
}

TSharedRef<FNiagaraSystemScriptViewModel> FNiagaraSystemViewModel::GetSystemScriptViewModel()
{
	return SystemScriptViewModel;
}

void FNiagaraSystemViewModel::CompileSystem()
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_SystemViewModel_CompileSystem);
	KillSystemInstances();
	SystemScriptViewModel->CompileSystem();
	OnSystemCompiledDelegate.Broadcast();
}

ENiagaraScriptCompileStatus FNiagaraSystemViewModel::GetLatestCompileStatus()
{
	return SystemScriptViewModel->GetLatestCompileStatus();
}

const TArray<FGuid>& FNiagaraSystemViewModel::GetSelectedEmitterHandleIds()
{
	return SelectedEmitterHandleIds;
}

void FNiagaraSystemViewModel::SetSelectedEmitterHandlesById(TArray<FGuid> InSelectedEmitterHandleIds)
{
	bool bSelectionChanged = false;
	if (SelectedEmitterHandleIds.Num() == InSelectedEmitterHandleIds.Num())
	{
		for (FGuid InSelectedEmitterHandleId : InSelectedEmitterHandleIds)
		{
			if (SelectedEmitterHandleIds.Contains(InSelectedEmitterHandleId) == false)
			{
				bSelectionChanged = true;
				break;
			}
		}
	}
	else
	{
		bSelectionChanged = true;
	}

	SelectedEmitterHandleIds.Empty();
	SelectedEmitterHandleIds.Append(InSelectedEmitterHandleIds);
	if (bSelectionChanged)
	{
		if (bUpdatingSystemSelectionFromSequencer == false)
		{
			UpdateSequencerFromEmitterHandleSelection();
		}
		OnSelectedEmitterHandlesChangedDelegate.Broadcast();
	}
}

void FNiagaraSystemViewModel::SetSelectedEmitterHandleById(FGuid InSelectedEmitterHandleId)
{
	TArray<FGuid> SingleSelectedEmitterHandleId;
	SingleSelectedEmitterHandleId.Add(InSelectedEmitterHandleId);
	SetSelectedEmitterHandlesById(SingleSelectedEmitterHandleId);
}

void FNiagaraSystemViewModel::GetSelectedEmitterHandles(TArray<TSharedRef<FNiagaraEmitterHandleViewModel>>& OutSelectedEmitterHanldles)
{
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		if (SelectedEmitterHandleIds.Contains(EmitterHandleViewModel->GetId()))
		{
			OutSelectedEmitterHanldles.Add(EmitterHandleViewModel);
		}
	}
}

const UNiagaraSystemEditorData& FNiagaraSystemViewModel::GetEditorData() const
{
	const UNiagaraSystemEditorData* EditorData = Cast<UNiagaraSystemEditorData>(System.GetEditorData());
	if (EditorData == nullptr)
	{
		EditorData = GetDefault<UNiagaraSystemEditorData>();
	}
	return *EditorData;
}

UNiagaraSystemEditorData& FNiagaraSystemViewModel::GetOrCreateEditorData()
{
	UNiagaraSystemEditorData* EditorData = Cast<UNiagaraSystemEditorData>(System.GetEditorData());
	if (EditorData == nullptr)
	{
		EditorData = NewObject<UNiagaraSystemEditorData>(&System, NAME_None, RF_Transactional);
		System.Modify();
		System.SetEditorData(EditorData);
	}
	return *EditorData;
}

UNiagaraComponent* FNiagaraSystemViewModel::GetPreviewComponent()
{
	return PreviewComponent;
}

TSharedPtr<ISequencer> FNiagaraSystemViewModel::GetSequencer()
{
	return Sequencer;
}

FNiagaraCurveOwner& FNiagaraSystemViewModel::GetCurveOwner()
{
	return CurveOwner;
}

bool FNiagaraSystemViewModel::GetSystemIsTransient() const
{
	return System.HasAnyFlags(RF_Transient);
}

bool FNiagaraSystemViewModel::GetCanAddEmittersFromTimeline() const
{
	return bCanAddEmittersFromTimeline;
}

void FNiagaraSystemViewModel::AddEmitterFromAssetData(const FAssetData& AssetData)
{
	UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(AssetData.GetAsset());
	if (Emitter != nullptr)
	{
		AddEmitter(*Emitter);
	}
}

void FNiagaraSystemViewModel::AddEmitter(UNiagaraEmitter& Emitter)
{
	// Kill all system instances before modifying the emitter handle list to prevent accessing deleted data.
	KillSystemInstances();

	const FScopedTransaction Transaction(LOCTEXT("AddEmitter", "Add emitter"));

	TSet<FName> EmitterHandleNames;
	for (const FNiagaraEmitterHandle& EmitterHandle : System.GetEmitterHandles())
	{
		EmitterHandleNames.Add(EmitterHandle.GetName());
	}

	System.Modify();
	FNiagaraEmitterHandle EmitterHandle = System.AddEmitterHandle(Emitter, FNiagaraEditorUtilities::GetUniqueName(Emitter.GetFName(), EmitterHandleNames));
	SystemScriptViewModel->RebuildEmitterNodes();

	if (System.GetNumEmitters() == 1)
	{
		// When adding a new emitter to an empty system start playing.
		Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Playing);
	}
		
	RefreshAll();

	SetSelectedEmitterHandleById(EmitterHandle.GetId());
}

void FNiagaraSystemViewModel::DuplicateEmitter(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleToDuplicate)
{
	if (EmitterHandleToDuplicate->GetEmitterHandle() == nullptr)
	{
		return;
	}

	// Kill all system instances before modifying the emitter handle list to prevent accessing deleted data.
	KillSystemInstances();

	const FScopedTransaction DuplicateTransaction(LOCTEXT("DuplicateEmitter", "Duplicate emitter"));

	TSet<FName> EmitterHandleNames;
	for (const FNiagaraEmitterHandle& EmitterHandle : System.GetEmitterHandles())
	{
		EmitterHandleNames.Add(EmitterHandle.GetName());
	}

	System.Modify();
	FNiagaraEmitterHandle EmitterHandle = System.DuplicateEmitterHandle(*EmitterHandleToDuplicate->GetEmitterHandle(), FNiagaraEditorUtilities::GetUniqueName(EmitterHandleToDuplicate->GetEmitterHandle()->GetName(), EmitterHandleNames));
	SystemScriptViewModel->RebuildEmitterNodes();

	RefreshAll();
}

void FNiagaraSystemViewModel::DeleteEmitter(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleToDelete)
{
	TSet<FGuid> IdsToDelete;
	IdsToDelete.Add(EmitterHandleToDelete->GetId());
	DeleteEmitters(IdsToDelete);
}

void FNiagaraSystemViewModel::DeleteEmitters(TSet<FGuid> EmitterHandleIdsToDelete)
{
	if (EmitterHandleIdsToDelete.Num() > 0)
	{
		// Kill all system instances before modifying the emitter handle list to prevent accessing deleted data.
		KillSystemInstances();

		const FScopedTransaction DeleteTransaction(EmitterHandleIdsToDelete.Num() == 1 
			? LOCTEXT("DeleteEmitter", "Delete emitter")
			: LOCTEXT("DeleteEmitters", "Delete emitters"));

		System.Modify();
		System.RemoveEmitterHandlesById(EmitterHandleIdsToDelete);
		SystemScriptViewModel->RebuildEmitterNodes();

		RefreshAll();
	}
}

FNiagaraSystemViewModel::FOnEmitterHandleViewModelsChanged& FNiagaraSystemViewModel::OnEmitterHandleViewModelsChanged()
{
	return OnEmitterHandleViewModelsChangedDelegate;
}

FNiagaraSystemViewModel::FOnCurveOwnerChanged& FNiagaraSystemViewModel::OnCurveOwnerChanged()
{
	return OnCurveOwnerChangedDelegate;
}

FNiagaraSystemViewModel::FOnSelectedEmitterHandlesChanged& FNiagaraSystemViewModel::OnSelectedEmitterHandlesChanged()
{
	return OnSelectedEmitterHandlesChangedDelegate;
}

FNiagaraSystemViewModel::FOnPostSequencerTimeChange& FNiagaraSystemViewModel::OnPostSequencerTimeChanged()
{
	return OnPostSequencerTimeChangeDelegate;
}

FNiagaraSystemViewModel::FOnSystemCompiled& FNiagaraSystemViewModel::OnSystemCompiled()
{
	return OnSystemCompiledDelegate;
}

void FNiagaraSystemViewModel::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (PreviewComponent != nullptr)
	{
		Collector.AddReferencedObject(PreviewComponent);
	}
	if (NiagaraSequence != nullptr)
	{
		Collector.AddReferencedObject(NiagaraSequence);
	}
}

void FNiagaraSystemViewModel::PostUndo(bool bSuccess)
{
	RefreshAll();
}

void FNiagaraSystemViewModel::Tick(float DeltaTime)
{
	bool bRecompile = false;
	if (SystemScriptViewModel->GetLatestCompileStatus() == ENiagaraScriptCompileStatus::NCS_Dirty)
	{
		//SystemScriptViewModel->CompileSystem();
		//UE_LOG(LogNiagaraEditor, Log, TEXT("Compiling %s due to dirty scripts."), *System.GetName());
		bRecompile |= true;
	}

	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		if (EmitterHandleViewModel->GetEmitterViewModel()->GetLatestCompileStatus() == ENiagaraScriptCompileStatus::NCS_Dirty)
		{
			bRecompile |= true;
			//EmitterHandleViewModel->GetEmitterViewModel()->CompileScripts();
			//UE_LOG(LogNiagaraEditor, Log, TEXT("Compiling %s - %s due to dirty scripts."), *System.GetName(), *EmitterHandleViewModel->GetName().ToString());
		}
	}

	if (bRecompile)
	{
		CompileSystem();
	}
}

bool FNiagaraSystemViewModel::IsTickable() const
{
	return GetDefault<UNiagaraEditorSettings>()->bAutoCompile;
}

TStatId FNiagaraSystemViewModel::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FNiagaraSystemViewModel, STATGROUP_Tickables);
}

void FNiagaraSystemViewModel::SetupPreviewComponentAndInstance()
{
	PreviewComponent = NewObject<UNiagaraComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	PreviewComponent->CastShadow = 1;
	PreviewComponent->bCastDynamicShadow = 1;
	PreviewComponent->SetAsset(&System);
	PreviewComponent->SetForceSolo(true);
	PreviewComponent->SetAgeUpdateMode(UNiagaraComponent::EAgeUpdateMode::DesiredAge);
	PreviewComponent->Activate(true);

	UNiagaraSystemEditorData& EditorData = GetOrCreateEditorData();
	FTransform OwnerTransform = EditorData.GetOwnerTransform();
	PreviewComponent->SetRelativeTransform(OwnerTransform);

	PreviewComponent->OnSystemInstanceChanged().AddRaw(this, &FNiagaraSystemViewModel::PreviewComponentSystemInstanceChanged);
	PreviewComponentSystemInstanceChanged();
}

void FNiagaraSystemViewModel::RefreshAll()
{
	ReInitializeSystemInstances();
	RefreshEmitterHandleViewModels();
	RefreshSequencerTracks();
	ResetCurveData();
}

void FNiagaraSystemViewModel::NotifyDataObjectChanged(UObject* ChangedObject)
{
	UNiagaraDataInterface* ChangedDataInterface = Cast<UNiagaraDataInterface>(ChangedObject);
	if (ChangedDataInterface)
	{
		UpdateCompiledDataInterfaces(ChangedDataInterface);
	}

	UNiagaraDataInterfaceCurveBase* ChangedDataInterfaceCurve = Cast<UNiagaraDataInterfaceCurveBase>(ChangedDataInterface);
	if (ChangedDataInterfaceCurve || ChangedObject == nullptr)
	{
		ResetCurveData();
	}
	ReInitializeSystemInstances();
}

void FNiagaraSystemViewModel::RefreshEmitterHandleViewModels()
{
	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> OldViewModels = EmitterHandleViewModels;
	EmitterHandleViewModels.Empty();

	// Map existing view models to the real instances that now exist. Reuse if we can. Create a new one if we cannot.
	TArray<FGuid> ValidEmitterHandleIds;
	int32 i;
	for (i = 0; i < System.GetNumEmitters(); ++i)
	{
		FNiagaraEmitterHandle* EmitterHandle = &System.GetEmitterHandle(i);
		TSharedPtr<FNiagaraEmitterInstance> Simulation = SystemInstance ? SystemInstance->GetSimulationForHandle(*EmitterHandle) : nullptr;
		ValidEmitterHandleIds.Add(EmitterHandle->GetId());

		bool bAdd = OldViewModels.Num() <= i;
		if (bAdd)
		{
			TSharedRef<FNiagaraEmitterHandleViewModel> ViewModel = MakeShareable(new FNiagaraEmitterHandleViewModel(EmitterHandle, Simulation, System));
			// Since we're adding fresh, we need to register all the event handlers.
			ViewModel->OnPropertyChanged().AddRaw(this, &FNiagaraSystemViewModel::EmitterHandlePropertyChanged, ViewModel);
			ViewModel->GetEmitterViewModel()->OnPropertyChanged().AddRaw(this, &FNiagaraSystemViewModel::EmitterPropertyChanged, ViewModel);
			ViewModel->GetEmitterViewModel()->OnScriptCompiled().AddRaw(this, &FNiagaraSystemViewModel::ScriptCompiled);
			EmitterHandleViewModels.Add(ViewModel);
		}
		else
		{
			TSharedRef<FNiagaraEmitterHandleViewModel> ViewModel = OldViewModels[i];
			ViewModel->Set(EmitterHandle, Simulation, System);
			EmitterHandleViewModels.Add(ViewModel);
		}

	}

	check(EmitterHandleViewModels.Num() == System.GetNumEmitters());

	// Clear out any old view models that may still be left around.
	for (; i < OldViewModels.Num(); i++)
	{
		TSharedRef<FNiagaraEmitterHandleViewModel> ViewModel = OldViewModels[i];
		ViewModel->OnPropertyChanged().RemoveAll(this);
		ViewModel->GetEmitterViewModel()->OnPropertyChanged().RemoveAll(this);
		ViewModel->GetEmitterViewModel()->OnScriptCompiled().RemoveAll(this);
		ViewModel->Set(nullptr, nullptr, System);
	}

	// Remove any invalid ids from the handle selection.
	auto EmitterHandleIdIsInvalid = [&](FGuid& EmitterHandleId) { return ValidEmitterHandleIds.Contains(EmitterHandleId) == false; };
	int32 NumRemoved = SelectedEmitterHandleIds.RemoveAll(EmitterHandleIdIsInvalid);

	OnEmitterHandleViewModelsChangedDelegate.Broadcast();
	if (NumRemoved > 0)
	{
		OnSelectedEmitterHandlesChangedDelegate.Broadcast();
	}
}

TRange<float> SequencerDefaultPlaybackRange(0, 1000.0f);
TRange<float> SequencerDefaultViewRange(0, 10.0f);

void PopulateChildMovieSceneFoldersFromNiagaraFolders(const UNiagaraSystemEditorFolder* NiagaraFolder, UMovieSceneFolder* MovieSceneFolder, const TMap<FGuid, UMovieSceneNiagaraEmitterTrack*>& EmitterHandleIdToTrackMap)
{
	for (const UNiagaraSystemEditorFolder* ChildNiagaraFolder : NiagaraFolder->GetChildFolders())
	{
		UMovieSceneFolder* MatchingMovieSceneFolder = nullptr;
		for (UMovieSceneFolder* ChildMovieSceneFolder : MovieSceneFolder->GetChildFolders())
		{
			if (ChildMovieSceneFolder->GetFolderName() == ChildNiagaraFolder->GetFolderName())
			{
				MatchingMovieSceneFolder = ChildMovieSceneFolder;
			}
		}

		if (MatchingMovieSceneFolder == nullptr)
		{
			MatchingMovieSceneFolder = NewObject<UMovieSceneFolder>(MovieSceneFolder, ChildNiagaraFolder->GetFolderName(), RF_Transactional);
			MatchingMovieSceneFolder->SetFolderName(ChildNiagaraFolder->GetFolderName());
			MovieSceneFolder->AddChildFolder(MatchingMovieSceneFolder);
		}

		PopulateChildMovieSceneFoldersFromNiagaraFolders(ChildNiagaraFolder, MatchingMovieSceneFolder, EmitterHandleIdToTrackMap);
	}

	for (const FGuid& ChildEmitterHandleId : NiagaraFolder->GetChildEmitterHandleIds())
	{
		UMovieSceneNiagaraEmitterTrack* const* TrackPtr = EmitterHandleIdToTrackMap.Find(ChildEmitterHandleId);
		if (TrackPtr != nullptr && MovieSceneFolder->GetChildMasterTracks().Contains(*TrackPtr) == false)
		{
			MovieSceneFolder->AddChildMasterTrack(*TrackPtr);
		}
	}
}

void FNiagaraSystemViewModel::RefreshSequencerTracks()
{
	TArray<UMovieSceneTrack*> MasterTracks = NiagaraSequence->GetMovieScene()->GetMasterTracks();
	for (UMovieSceneTrack* MasterTrack : MasterTracks)
	{
		if (MasterTrack != nullptr)
		{
			NiagaraSequence->GetMovieScene()->RemoveMasterTrack(*MasterTrack);
		}
	}

	float MinEmitterTime = 0;
	float MaxEmitterTime = 0;
	TMap<FGuid, UMovieSceneNiagaraEmitterTrack*> EmitterHandleIdToTrackMap;
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		UMovieSceneNiagaraEmitterTrack* EmitterTrack = Cast<UMovieSceneNiagaraEmitterTrack>(NiagaraSequence->GetMovieScene()->AddMasterTrack(UMovieSceneNiagaraEmitterTrack::StaticClass()));
		EmitterTrack->SetEmitterHandle(EmitterHandleViewModel);
		RefreshSequencerTrack(EmitterTrack);
		MinEmitterTime = FMath::Min(MinEmitterTime, EmitterHandleViewModel->GetEmitterViewModel()->GetStartTime());
		MaxEmitterTime = FMath::Max(MaxEmitterTime, EmitterHandleViewModel->GetEmitterViewModel()->GetEndTime());
		EmitterHandleIdToTrackMap.Add(EmitterHandleViewModel->GetId(), EmitterTrack);
	}

	TArray<UMovieSceneFolder*>& MovieSceneRootFolders = NiagaraSequence->GetMovieScene()->GetRootFolders();
	MovieSceneRootFolders.Empty();

	const UNiagaraSystemEditorData& SystemEditorData = GetEditorData();
	UNiagaraSystemEditorFolder& RootFolder = SystemEditorData.GetRootFolder();
	for (const UNiagaraSystemEditorFolder* RootChildFolder : RootFolder.GetChildFolders())
	{
		UMovieSceneFolder* MovieSceneRootFolder = NewObject<UMovieSceneFolder>(NiagaraSequence->GetMovieScene(), RootChildFolder->GetFolderName(), RF_Transactional);
		MovieSceneRootFolder->SetFolderName(RootChildFolder->GetFolderName());
		MovieSceneRootFolders.Add(MovieSceneRootFolder);
		PopulateChildMovieSceneFoldersFromNiagaraFolders(RootChildFolder, MovieSceneRootFolder, EmitterHandleIdToTrackMap);
	}

	// Expand the view range to show all emitters.
	TRange<float> CurrentViewRange = NiagaraSequence->GetMovieScene()->GetEditorData().ViewRange;
	float NewMinViewRange = FMath::Min(MinEmitterTime, CurrentViewRange.GetLowerBoundValue());
	float NewMaxViewRange = FMath::Max(MaxEmitterTime, CurrentViewRange.GetUpperBoundValue());

	NiagaraSequence->GetMovieScene()->GetEditorData().ViewRange = TRange<float>(NewMinViewRange, NewMaxViewRange);

	Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	Sequencer->SetGlobalTime(0);
}

UMovieSceneNiagaraEmitterTrack* FNiagaraSystemViewModel::GetTrackForHandleViewModel(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel)
{
	for (UMovieSceneTrack* Track : NiagaraSequence->GetMovieScene()->GetMasterTracks())
	{
		UMovieSceneNiagaraEmitterTrack* EmitterTrack = CastChecked<UMovieSceneNiagaraEmitterTrack>(Track);
		if (EmitterTrack->GetEmitterHandle() == EmitterHandleViewModel)
		{
			return EmitterTrack;
		}
	}
	return nullptr;
}

void FNiagaraSystemViewModel::RefreshSequencerTrack(UMovieSceneNiagaraEmitterTrack* EmitterTrack)
{
	if(EmitterTrack != nullptr)
	{
		TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = EmitterTrack->GetEmitterHandle();
		UNiagaraEmitter* Emitter = EmitterHandleViewModel->GetEmitterViewModel()->GetEmitter();

		TArray<UMovieSceneSection*> Sections = EmitterTrack->GetAllSections();
		UMovieSceneNiagaraEmitterSection* EmitterSection = Cast<UMovieSceneNiagaraEmitterSection>(Sections.Num() == 1 ? Sections[0] : nullptr);

		if (EmitterSection != nullptr && Emitter != nullptr)
		{
			EmitterTrack->SetDisplayName(EmitterHandleViewModel->GetNameText());

			bool bIsInfinite = EmitterHandleViewModel->GetEmitterViewModel()->GetStartTime() == 0
				&& EmitterHandleViewModel->GetEmitterViewModel()->GetEndTime() == 0;
			float StartTime;
			float EndTime;

// 			if (bIsInfinite && Emitter->Bursts.Num() > 0)
// 			{
// 				StartTime = TNumericLimits<float>::Max();
// 				EndTime = TNumericLimits<float>::Lowest();
// 				for (const FNiagaraEmitterBurst& Burst : Emitter->Bursts)
// 				{
// 					StartTime = FMath::Min(StartTime, Burst.Time);
// 					EndTime = FMath::Max(EndTime, Burst.Time);
// 				}
// 			}
// 			else
			{
				StartTime = EmitterHandleViewModel->GetEmitterViewModel()->GetStartTime();
				EndTime = EmitterHandleViewModel->GetEmitterViewModel()->GetEndTime();
			}

			EmitterSection->SetEmitterHandle(EmitterHandleViewModel.ToSharedRef());
			EmitterSection->SetIsActive(EmitterHandleViewModel->GetIsEnabled());
			EmitterSection->SetStartTime(StartTime);
			EmitterSection->SetEndTime(EndTime);
			EmitterSection->SetIsInfinite(bIsInfinite);

// 			TSharedPtr<FBurstCurve> BurstCurve = EmitterSection->GetBurstCurve();
// 			if (BurstCurve.IsValid())
// 			{
// 				BurstCurve->Reset();
// 				for (const FNiagaraEmitterBurst& Burst : Emitter->Bursts)
// 				{
// 					FMovieSceneBurstKey BurstKey;
// 					BurstKey.TimeRange = Burst.TimeRange;
// 					BurstKey.SpawnMinimum = Burst.SpawnMinimum;
// 					BurstKey.SpawnMaximum = Burst.SpawnMaximum;
// 					BurstCurve->AddKeyValue(Burst.Time, BurstKey);
// 				}
// 			}
		}
	}
}

void FNiagaraSystemViewModel::SetupSequencer()
{
	NiagaraSequence = NewObject<UNiagaraSequence>(GetTransientPackage());
	UMovieScene* MovieScene = NewObject<UMovieScene>(NiagaraSequence, FName("Niagara System MovieScene"), RF_Transactional);
	MovieScene->SetPlaybackRange(SequencerDefaultPlaybackRange.GetLowerBoundValue(), SequencerDefaultPlaybackRange.GetUpperBoundValue());
	MovieScene->GetEditorData().ViewRange = SequencerDefaultViewRange;
	MovieScene->GetEditorData().WorkingRange = SequencerDefaultPlaybackRange;

	NiagaraSequence->Initialize(this, MovieScene);

	FSequencerViewParams ViewParams(TEXT("NiagaraSequencerSettings"));
	{
		ViewParams.InitialScrubPosition = 0;
		ViewParams.UniqueName = "NiagaraSequenceEditor";
		ViewParams.OnGetAddMenuContent = OnGetSequencerAddMenuContent;
	}

	FSequencerInitParams SequencerInitParams;
	{
		SequencerInitParams.ViewParams = ViewParams;
		SequencerInitParams.RootSequence = NiagaraSequence;
		SequencerInitParams.bEditWithinLevelEditor = false;
		SequencerInitParams.ToolkitHost = nullptr;
	}

	ISequencerModule &SequencerModule = FModuleManager::LoadModuleChecked< ISequencerModule >("Sequencer");
	Sequencer = SequencerModule.CreateSequencer(SequencerInitParams);

	Sequencer->OnMovieSceneDataChanged().AddRaw(this, &FNiagaraSystemViewModel::SequencerDataChanged);
	Sequencer->OnGlobalTimeChanged().AddRaw(this, &FNiagaraSystemViewModel::SequencerTimeChanged);
	Sequencer->GetSelectionChangedTracks().AddRaw(this, &FNiagaraSystemViewModel::SequencerTrackSelectionChanged);
	Sequencer->GetSelectionChangedSections().AddRaw(this, &FNiagaraSystemViewModel::SequencerSectionSelectionChanged);
	Sequencer->SetPlaybackStatus(System.GetNumEmitters() > 0 
		? EMovieScenePlayerStatus::Playing
		: EMovieScenePlayerStatus::Stopped);
}

void FNiagaraSystemViewModel::ResetSystem()
{
 	if (Sequencer->GetPlaybackStatus() == EMovieScenePlayerStatus::Playing)
	{
		Sequencer->SetGlobalTime(0.0f);
	}

	for (TObjectIterator<UNiagaraComponent> ComponentIt; ComponentIt; ++ComponentIt)
	{
		UNiagaraComponent* Component = *ComponentIt;
		if (Component->GetAsset() == &System)
		{
			Component->SynchronizeWithSourceSystem();
			Component->ResetSystem();
		}
	}
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

void FNiagaraSystemViewModel::KillSystemInstances()
{
	for (TObjectIterator<UNiagaraComponent> ComponentIt; ComponentIt; ++ComponentIt)
	{
		UNiagaraComponent* Component = *ComponentIt;
		if (Component->GetAsset() == &System)
		{
			Component->DestroyInstance();
		}
	}
}

void FNiagaraSystemViewModel::ReInitializeSystemInstances()
{
	if (Sequencer.IsValid() && Sequencer->GetPlaybackStatus() == EMovieScenePlayerStatus::Playing)
	{
		Sequencer->SetGlobalTime(0.0f);
	}

	for (TObjectIterator<UNiagaraComponent> ComponentIt; ComponentIt; ++ComponentIt)
	{
		UNiagaraComponent* Component = *ComponentIt;
		if (Component->GetAsset() == &System)
		{
			Component->SynchronizeWithSourceSystem();
			Component->ReinitializeSystem();
		}
	}
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

struct FNiagaraSystemCurveData
{
	FRichCurve* Curve;
	FName Name;
	FLinearColor Color;
	UObject* Owner;
};

void GetCurveData(FString CurveSource, UNiagaraGraph* SourceGraph, TArray<FNiagaraSystemCurveData>& OutCurveData)
{
	TArray<UNiagaraNodeInput*> InputNodes;
	SourceGraph->GetNodesOfClass<UNiagaraNodeInput>(InputNodes);
	TSet<FName> HandledInputs;
	for (UNiagaraNodeInput* InputNode : InputNodes)
	{
		if (HandledInputs.Contains(InputNode->Input.GetName()) == false)
		{
			if (InputNode->Usage == ENiagaraInputNodeUsage::Parameter)
			{
				UNiagaraDataInterfaceCurveBase* CurveDataInterface = Cast<UNiagaraDataInterfaceCurveBase>(InputNode->DataInterface);
				if (CurveDataInterface != nullptr)
				{
					TArray<UNiagaraDataInterfaceCurveBase::FCurveData> CurveData;
					CurveDataInterface->GetCurveData(CurveData);
					for (UNiagaraDataInterfaceCurveBase::FCurveData CurveDataItem : CurveData)
					{
						FNiagaraSystemCurveData SystemCurveData;
						SystemCurveData.Curve = CurveDataItem.Curve;
						SystemCurveData.Color = CurveDataItem.Color;
						SystemCurveData.Owner = CurveDataInterface;
						FString ParameterName = CurveDataItem.Name == NAME_None
							? InputNode->Input.GetName().ToString()
							: InputNode->Input.GetName().ToString() + ".";
						FString DataName = CurveDataItem.Name == NAME_None
							? FString()
							: CurveDataItem.Name.ToString();
						SystemCurveData.Name = *(CurveSource + ParameterName + DataName);
						OutCurveData.Add(SystemCurveData);
					}
				}
			}
			HandledInputs.Add(InputNode->Input.GetName());
		}
	}
}

void FNiagaraSystemViewModel::ResetCurveData()
{
	CurveOwner.EmptyCurves();

	TArray<FNiagaraSystemCurveData> CurveData;

	GetCurveData(
		TEXT("System"),
		SystemScriptViewModel->GetGraphViewModel()->GetGraph(),
		CurveData);

	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		GetCurveData(
			EmitterHandleViewModel->GetName().ToString(),
			EmitterHandleViewModel->GetEmitterViewModel()->GetSharedScriptViewModel()->GetGraphViewModel()->GetGraph(),
			CurveData);
	}

	for (FNiagaraSystemCurveData& CurveDataItem : CurveData)
	{
		CurveOwner.AddCurve(
			*CurveDataItem.Curve,
			CurveDataItem.Name,
			CurveDataItem.Color,
			*CurveDataItem.Owner,
			FNiagaraCurveOwner::FNotifyCurveChanged::CreateRaw(this, &FNiagaraSystemViewModel::CurveChanged));
	}

	OnCurveOwnerChangedDelegate.Broadcast();
}

void FNiagaraSystemViewModel::UpdateCompiledDataInterfaces(UNiagaraDataInterface* ChangedDataInterface)
{
	SystemScriptViewModel->UpdateCompiledDataInterfaces(ChangedDataInterface);
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		EmitterHandleViewModel->GetEmitterViewModel()->GetSharedScriptViewModel()->UpdateCompiledDataInterfaces(ChangedDataInterface);
	}
}

void FNiagaraSystemViewModel::EmitterHandlePropertyChanged(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel)
{
	// When the emitter handle changes, refresh the System scripts emitter nodes just in cast the
	// property that changed was the handles emitter.
	if (bUpdatingFromSequencerDataChange == false)
	{
		RefreshSequencerTrack(GetTrackForHandleViewModel(EmitterHandleViewModel));
	}
	ReInitializeSystemInstances();
}

void FNiagaraSystemViewModel::EmitterPropertyChanged(TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel)
{
	if (bUpdatingFromSequencerDataChange == false)
	{
		RefreshSequencerTrack(GetTrackForHandleViewModel(EmitterHandleViewModel));
	}
	ReInitializeSystemInstances();
}

void FNiagaraSystemViewModel::ScriptCompiled()
{
	ReInitializeSystemInstances();
}

void FNiagaraSystemViewModel::CurveChanged(FRichCurve* ChangedCurve, UObject* InCurveOwner)
{
	UNiagaraDataInterface* ChangedDataInterface = Cast<UNiagaraDataInterface>(InCurveOwner);
	if (ChangedDataInterface != nullptr)
	{
		UpdateCompiledDataInterfaces(ChangedDataInterface);
	}
	ResetSystem();
}

void PopulateNiagaraFoldersFromMovieSceneFolders(const TArray<UMovieSceneFolder*>& MovieSceneFolders, const TArray<UMovieSceneTrack*>& MovieSceneTracks, UNiagaraSystemEditorFolder* ParentFolder)
{
	TArray<FName> ValidFolderNames;
	for (UMovieSceneFolder* MovieSceneFolder : MovieSceneFolders)
	{
		ValidFolderNames.Add(MovieSceneFolder->GetFolderName());
		UNiagaraSystemEditorFolder* MatchingNiagaraFolder = nullptr;
		for (UNiagaraSystemEditorFolder* ChildNiagaraFolder : ParentFolder->GetChildFolders())
		{
			CA_ASSUME(ChildNiagaraFolder != nullptr);
			if (ChildNiagaraFolder->GetFolderName() == MovieSceneFolder->GetFolderName())
			{
				MatchingNiagaraFolder = ChildNiagaraFolder;
				break;
			}
		}

		if (MatchingNiagaraFolder == nullptr)
		{
			MatchingNiagaraFolder = NewObject<UNiagaraSystemEditorFolder>(ParentFolder, MovieSceneFolder->GetFolderName(), RF_Transactional);
			MatchingNiagaraFolder->SetFolderName(MovieSceneFolder->GetFolderName());
			ParentFolder->AddChildFolder(MatchingNiagaraFolder);
		}

		PopulateNiagaraFoldersFromMovieSceneFolders(MovieSceneFolder->GetChildFolders(), MovieSceneFolder->GetChildMasterTracks(), MatchingNiagaraFolder);
	}

	TArray<UNiagaraSystemEditorFolder*> ChildNiagaraFolders = ParentFolder->GetChildFolders();
	for (UNiagaraSystemEditorFolder* ChildNiagaraFolder : ChildNiagaraFolders)
	{
		if (ValidFolderNames.Contains(ChildNiagaraFolder->GetFolderName()) == false)
		{
			ParentFolder->RemoveChildFolder(ChildNiagaraFolder);
		}
	}

	TArray<FGuid> ValidEmitterHandleIds;
	for (UMovieSceneTrack* MovieSceneTrack : MovieSceneTracks)
	{
		UMovieSceneNiagaraEmitterTrack* NiagaraEmitterTrack = Cast<UMovieSceneNiagaraEmitterTrack>(MovieSceneTrack);
		if (NiagaraEmitterTrack != nullptr)
		{
			FGuid EmitterHandleId = NiagaraEmitterTrack->GetEmitterHandle()->GetId();
			ValidEmitterHandleIds.Add(EmitterHandleId);
			if (ParentFolder->GetChildEmitterHandleIds().Contains(EmitterHandleId) == false)
			{
				ParentFolder->AddChildEmitterHandleId(EmitterHandleId);
			}
		}
	}

	TArray<FGuid> ChildEmitterHandleIds = ParentFolder->GetChildEmitterHandleIds();
	for (FGuid& ChildEmitterHandleId : ChildEmitterHandleIds)
	{
		if (ValidEmitterHandleIds.Contains(ChildEmitterHandleId) == false)
		{
			ParentFolder->RemoveChildEmitterHandleId(ChildEmitterHandleId);
		}
	}
}

void FNiagaraSystemViewModel::SequencerDataChanged(EMovieSceneDataChangeType DataChangeType)
{
	bUpdatingFromSequencerDataChange = true;
	TSet<FGuid> VaildTrackEmitterHandleIds;
	for (UMovieSceneTrack* Track : NiagaraSequence->GetMovieScene()->GetMasterTracks())
	{
		UMovieSceneNiagaraEmitterTrack* EmitterTrack = CastChecked<UMovieSceneNiagaraEmitterTrack>(Track);
		TArray<UMovieSceneSection*> Sections = EmitterTrack->GetAllSections();
		UMovieSceneNiagaraEmitterSection* EmitterSection = Cast<UMovieSceneNiagaraEmitterSection>(Sections.Num() == 1 ? Sections[0] : nullptr);
		if (EmitterSection != nullptr)
		{
			VaildTrackEmitterHandleIds.Add(EmitterTrack->GetEmitterHandle()->GetId());

			TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = EmitterTrack->GetEmitterHandle();
			if (bCanRenameEmittersFromTimeline)
			{
				EmitterHandleViewModel->SetName(*EmitterTrack->GetDisplayName().ToString());
			}
			else
			{
				EmitterTrack->SetDisplayName(EmitterHandleViewModel->GetNameText());
			}
			EmitterHandleViewModel->SetIsEnabled(EmitterSection->IsActive());

			TSharedPtr<FNiagaraEmitterViewModel> EmitterViewModel = EmitterTrack->GetEmitterHandle()->GetEmitterViewModel();
			if (EmitterSection->IsInfinite() == false)
			{
				EmitterViewModel->SetStartTime(EmitterSection->GetStartTime());
				EmitterViewModel->SetEndTime(EmitterSection->GetEndTime());
			}

			TSharedPtr<FBurstCurve> BurstCurve = EmitterSection->GetBurstCurve();
			if (BurstCurve.IsValid())
			{
				UNiagaraEmitter* Emitter = EmitterViewModel->GetEmitter();
// 				if (Emitter)
// 				{
// 					Emitter->Bursts.Empty();
// 				}
				TArray<FKeyHandle> KeyHandles;
				for (TKeyTimeIterator<float> KeyIterator = BurstCurve->IterateKeys(); KeyIterator; ++KeyIterator)
				{
					KeyHandles.Add(KeyIterator.GetKeyHandle());
				}

				for (const FKeyHandle& KeyHandle : KeyHandles)
				{
					auto Key = BurstCurve->GetKey(KeyHandle).GetValue();
					FNiagaraEmitterBurst Burst;
					Burst.Time = Key.Time;
					Burst.TimeRange = Key.Value.TimeRange;
					Burst.SpawnMinimum = Key.Value.SpawnMinimum;
					Burst.SpawnMaximum = Key.Value.SpawnMaximum;
// 					if (Emitter)
// 					{
// 						Emitter->Bursts.Add(Burst);
// 					}
				}
			}
		}
	}

	TSet<FGuid> AllEmitterHandleIds;
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		AllEmitterHandleIds.Add(EmitterHandleViewModel->GetId());
	}

	TSet<FGuid> RemovedEmitterHandleIds = AllEmitterHandleIds.Difference(VaildTrackEmitterHandleIds);
	if (RemovedEmitterHandleIds.Num() > 0)
	{
		if (bCanRemoveEmittersFromTimeline)
		{
			DeleteEmitters(RemovedEmitterHandleIds);
			// When deleting emitters from sequencer, select a new one if one is available.
			if (SelectedEmitterHandleIds.Num() == 0 && EmitterHandleViewModels.Num() > 0)
			{
				SetSelectedEmitterHandleById(EmitterHandleViewModels[0]->GetId());
			}
		}
		else
		{
			RefreshSequencerTracks();
		}
	}

	TArray<UMovieSceneTrack*> RootTracks;
	TArray<UMovieSceneFolder*> RootFolders = NiagaraSequence->GetMovieScene()->GetRootFolders();
	if (RootFolders.Num() != 0 || GetEditorData().GetRootFolder().GetChildFolders().Num() != 0)
	{
		PopulateNiagaraFoldersFromMovieSceneFolders(RootFolders, RootTracks, &GetOrCreateEditorData().GetRootFolder());
	}

	if (SystemInstance != nullptr)
	{
		SystemInstance->Reset(FNiagaraSystemInstance::EResetMode::DeferredReset);
	}
	bUpdatingFromSequencerDataChange = false;
}

void FNiagaraSystemViewModel::SequencerTimeChanged()
{
	if (!PreviewComponent)
	{
		return;
	}
	EMovieScenePlayerStatus::Type CurrentStatus = Sequencer->GetPlaybackStatus();
	float CurrentSequencerTime = Sequencer->GetGlobalTime();
	if (SystemInstance != nullptr)
	{
		// Avoid reentrancy if we're setting the time directly.
		if (bSettingSequencerTimeDirectly == false && CurrentSequencerTime != PreviousSequencerTime)
		{
			bool bUpdateDesiredAge = false;
			bool bResetSystemInstance = false;
			bool bSetEnabled = false;

			if (CurrentStatus == EMovieScenePlayerStatus::Playing)
			{
				bool bSystemIsAlive = false;
				//float MaxEmitterTime = 0.0f;

				if (bUseSystemExecStateForTimelineReset)
				{
					if (SystemInstance->GetExecutionState() == ENiagaraExecutionState::Disabled)
					{
						bSystemIsAlive = false;
						bSetEnabled = true;
					}
					else
					{
						bSystemIsAlive = true;
					}
				}
				else 
				{
					for (const TSharedRef<FNiagaraEmitterInstance> Simulation : SystemInstance->GetEmitters())
					{
						if (Simulation->GetExecutionState() != ENiagaraExecutionState::Dead)
						{
							bSystemIsAlive |= true;
						}

						/*for (const TSharedRef<FNiagaraEmitterHandleViewModel> HandleViewModel : GetEmitterHandleViewModels())
						{
							TSharedRef<FNiagaraEmitterViewModel> EmitterViewModel = HandleViewModel->GetEmitterViewModel();
							if (EmitterViewModel->GetEmitter() == Simulation->GetEmitterHandle().GetInstance())
							{
								float EmitterEndTime = EmitterViewModel->GetNumLoops() == 0
								? EmitterViewModel->GetEndTime()
								: (EmitterViewModel->GetEndTime() - EmitterViewModel->GetStartTime()) * EmitterViewModel->GetNumLoops();
								MaxEmitterTime = FMath::Max(MaxEmitterTime, EmitterEndTime);

							}
						}*/
					}
				}

				bool bIsUpdatingOrCanSpawn = bSystemIsAlive /*|| (MaxEmitterTime == 0.0f || CurrentSequencerTime <= MaxEmitterTime)*/;
				if (bIsUpdatingOrCanSpawn)
				{
					// Skip the first update after going from stopped to playing because snapping in sequencer may have made the time
					// reverse by a small amount, and sending that update to the System will reset it unnecessarily.
					bUpdateDesiredAge = PreviousSequencerStatus != EMovieScenePlayerStatus::Stopped;
				}
				else
				{
					// If there are no active particles and no more particles will be spawned reset the System so it loops.
					bResetSystemInstance = true;
				}
			}
			else
			{
				// If the time changed and we're not playing, or stopping playing, than the user is scrubbing, or jumping to a different time using some
				// other means so just update the System time.  Skip the first update after going from playing to stopped because snapping in sequencer 
				// may have made the time reverse by a small amount, and sending that update to the System will reset it unnecessarily.
				bool bStoppedPlaying = PreviousSequencerStatus == EMovieScenePlayerStatus::Playing && CurrentStatus != EMovieScenePlayerStatus::Playing;
				bUpdateDesiredAge = !bStoppedPlaying;
			}

			if (bUpdateDesiredAge)
			{
				PreviewComponent->SetDesiredAge(FMath::Max(CurrentSequencerTime, 0.0f));

			//SystemInstance->Tick(0);TODO: What was this for? Possibly reinstate but I don't like use dealing with the instance directly.
			}

			if (bResetSystemInstance)
			{
				if (bSetEnabled)
				{
					SystemInstance->Enable();
				}
				else
				{
					SystemInstance->Reset(FNiagaraSystemInstance::EResetMode::ImmediateReset);
				}
				TGuardValue<bool>(bSettingSequencerTimeDirectly, true);
				Sequencer->SetLocalTime(0);
			}
		}
	}

	PreviousSequencerStatus = CurrentStatus;
	PreviousSequencerTime = CurrentSequencerTime;

	OnPostSequencerTimeChangeDelegate.Broadcast();
}

void FNiagaraSystemViewModel::SequencerTrackSelectionChanged(TArray<UMovieSceneTrack*> SelectedTracks)
{
	if (bUpdatingSequencerSelectionFromSystem == false)
	{
		UpdateEmitterHandleSelectionFromSequencer();
	}
}

void FNiagaraSystemViewModel::SequencerSectionSelectionChanged(TArray<UMovieSceneSection*> SelectedSections)
{
	if (bUpdatingSequencerSelectionFromSystem == false)
	{
		UpdateEmitterHandleSelectionFromSequencer();
	}
}

void FNiagaraSystemViewModel::UpdateEmitterHandleSelectionFromSequencer()
{
	TArray<FGuid> NewSelectedEmitterHandleIds;

	TArray<UMovieSceneTrack*> SelectedTracks;
	Sequencer->GetSelectedTracks(SelectedTracks);
	for (UMovieSceneTrack* SelectedTrack : SelectedTracks)
	{
		UMovieSceneNiagaraEmitterTrack* SelectedEmitterTrack = Cast<UMovieSceneNiagaraEmitterTrack>(SelectedTrack);
		if (SelectedEmitterTrack != nullptr && SelectedEmitterTrack->GetEmitterHandle().IsValid())
		{
			NewSelectedEmitterHandleIds.AddUnique(SelectedEmitterTrack->GetEmitterHandle()->GetId());
		}
	}

	TArray<UMovieSceneSection*> SelectedSections;
	Sequencer->GetSelectedSections(SelectedSections);
	for (UMovieSceneSection* SelectedSection : SelectedSections)
	{
		UMovieSceneNiagaraEmitterSection* SelectedEmitterSection = Cast<UMovieSceneNiagaraEmitterSection>(SelectedSection);
		if (SelectedEmitterSection != nullptr && SelectedEmitterSection->GetEmitterHandle().IsValid())
		{
			NewSelectedEmitterHandleIds.AddUnique(SelectedEmitterSection->GetEmitterHandle()->GetId());
		}
	}

	TGuardValue<bool> UpdateGuard(bUpdatingSystemSelectionFromSequencer, true);
	SetSelectedEmitterHandlesById(NewSelectedEmitterHandleIds);
}

void FNiagaraSystemViewModel::UpdateSequencerFromEmitterHandleSelection()
{
	TGuardValue<bool> UpdateGuard(bUpdatingSequencerSelectionFromSystem, true);
	Sequencer->EmptySelection();
	for (FGuid SelectedEmitterHandleId : SelectedEmitterHandleIds)
	{
		for (UMovieSceneTrack* MasterTrack : NiagaraSequence->GetMovieScene()->GetMasterTracks())
		{
			UMovieSceneNiagaraEmitterTrack* EmitterTrack = Cast<UMovieSceneNiagaraEmitterTrack>(MasterTrack);
			if (EmitterTrack != nullptr && EmitterTrack->GetEmitterHandle()->GetId() == SelectedEmitterHandleId)
			{
				Sequencer->SelectTrack(EmitterTrack);
			}
		}
	}
}

void FNiagaraSystemViewModel::PreviewComponentSystemInstanceChanged()
{
	FNiagaraSystemInstance* OldSystemInstance = SystemInstance;
	SystemInstance = PreviewComponent->GetSystemInstance();
	if (SystemInstance != OldSystemInstance)
	{
		if (SystemInstance != nullptr)
		{
			SystemInstance->OnInitialized().AddRaw(this, &FNiagaraSystemViewModel::SystemInstanceInitialized);
		}
		else
		{
			for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
			{
				if (EmitterHandleViewModel->GetEmitterHandle())
				{
					EmitterHandleViewModel->SetSimulation(nullptr);
				}
			}
		}
	}
}

void FNiagaraSystemViewModel::SystemInstanceInitialized()
{
	for (TSharedRef<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel : EmitterHandleViewModels)
	{
		if (EmitterHandleViewModel->GetEmitterHandle())
		{
			EmitterHandleViewModel->SetSimulation(SystemInstance->GetSimulationForHandle(*EmitterHandleViewModel->GetEmitterHandle()));
		}
	}
}

void FNiagaraSystemViewModel::ResynchronizeAllHandles()
{
	System.ResynchronizeAllHandles();
	RefreshAll();
}

#undef LOCTEXT_NAMESPACE // NiagaraSystemViewModel
