// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sequencer.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/MessageDialog.h"
#include "Containers/ArrayBuilder.h"
#include "Misc/FeedbackContext.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "UObject/MetaData.h"
#include "UObject/PropertyPortFlags.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Editor.h"
#include "MovieScenePossessable.h"
#include "MovieScene.h"
#include "Widgets/Layout/SBorder.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Exporters/Exporter.h"
#include "Editor/UnrealEdEngine.h"
#include "Camera/CameraActor.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "LevelEditorViewport.h"
#include "EditorModeManager.h"
#include "UnrealEdMisc.h"
#include "EditorDirectories.h"
#include "FileHelpers.h"
#include "UnrealEdGlobals.h"
#include "SequencerCommands.h"
#include "DisplayNodes/SequencerFolderNode.h"
#include "DisplayNodes/SequencerObjectBindingNode.h"
#include "ISequencerSection.h"
#include "DisplayNodes/SequencerTrackNode.h"
#include "MovieSceneClipboard.h"
#include "SequencerCommonHelpers.h"
#include "SSequencer.h"
#include "ISequencerKeyCollection.h"
#include "GroupedKeyArea.h"
#include "SequencerSettings.h"
#include "SequencerLog.h"
#include "SequencerEdMode.h"
#include "MovieSceneSequence.h"
#include "MovieSceneFolder.h"
#include "PropertyEditorModule.h"
#include "EditorWidgetsModule.h"
#include "ILevelViewport.h"
#include "EditorSupportDelegates.h"
#include "SSequencerTreeView.h"
#include "ScopedTransaction.h"
#include "Tracks/MovieScene3DAttachTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "ISequencerTrackEditor.h"
#include "MovieSceneToolHelpers.h"
#include "Sections/MovieScene3DAttachSection.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "ISettingsModule.h"
#include "Framework/Commands/GenericCommands.h"
#include "Tracks/MovieSceneSpawnTrack.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "ISequencerHotspot.h"
#include "SequencerHotspots.h"
#include "MovieSceneCaptureDialogModule.h"
#include "AutomatedLevelSequenceCapture.h"
#include "MovieSceneCommonHelpers.h"
#include "SceneOutlinerModule.h"
#include "SceneOutlinerPublicTypes.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "PackageTools.h"
#include "VirtualTrackArea.h"
#include "SequencerUtilities.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "ISequenceRecorder.h"
#include "CineCameraActor.h"
#include "CameraRig_Rail.h"
#include "CameraRig_Crane.h"
#include "Components/SplineComponent.h"
#include "DesktopPlatformModule.h"
#include "Factories.h"
#include "FbxExporter.h"
#include "UnrealExporter.h"
#include "ISequencerEditorObjectBinding.h"
#include "LevelSequence.h"
#include "IVREditorModule.h"
#include "HAL/PlatformApplicationMisc.h"
#include "SequencerKeyActor.h"

#define LOCTEXT_NAMESPACE "Sequencer"

DEFINE_LOG_CATEGORY(LogSequencer);

struct FSequencerTemplateStore : FMovieSceneSequenceTemplateStore
{
	FSequencerTemplateStore()
	{
		TemplateParameters.bForEditorPreview = true;
		bTemplatesAreVolatile = true;
	}

	void Reset()
	{
		Templates.Reset();
	}

	void PurgeStaleTracks()
	{
		for (auto& Pair : Templates)
		{
			Pair.Value->PurgeStaleTracks();
		}
	}

	virtual FMovieSceneEvaluationTemplate& GetCompiledTemplate(UMovieSceneSequence& Sequence, FObjectKey InSequenceKey)
	{
		if (TUniquePtr<FCachedMovieSceneEvaluationTemplate>* ExistingTemplate = Templates.Find(InSequenceKey))
		{
			FCachedMovieSceneEvaluationTemplate* Template = ExistingTemplate->Get();
			Template->Regenerate(TemplateParameters);
			return *Template;
		}
		else
		{
			FCachedMovieSceneEvaluationTemplate* NewTemplate = new FCachedMovieSceneEvaluationTemplate;
			NewTemplate->Initialize(Sequence, this);
			NewTemplate->Regenerate(TemplateParameters);

			Templates.Add(InSequenceKey, TUniquePtr<FCachedMovieSceneEvaluationTemplate>(NewTemplate));
			return *NewTemplate;
		}
	}

	// Store templates as unique ptrs to ensure that external pointers don't become invalid when the array is reallocated
	TMap<FObjectKey, TUniquePtr<FCachedMovieSceneEvaluationTemplate>> Templates;
	FMovieSceneTrackCompilationParams TemplateParameters;
};


void FSequencer::InitSequencer(const FSequencerInitParams& InitParams, const TSharedRef<ISequencerObjectChangeListener>& InObjectChangeListener, const TArray<FOnCreateTrackEditor>& TrackEditorDelegates, const TArray<FOnCreateEditorObjectBinding>& EditorObjectBindingDelegates)
{
	bIsEditingWithinLevelEditor = InitParams.bEditWithinLevelEditor;

	SilentModeCount = 0;
	bReadOnly = InitParams.ViewParams.bReadOnly;

	PreAnimatedState.EnableGlobalCapture();

	if (InitParams.SpawnRegister.IsValid())
	{
		SpawnRegister = InitParams.SpawnRegister;
	}
	else
	{
		// Spawnables not supported
		SpawnRegister = MakeShareable(new FNullMovieSceneSpawnRegister);
	}

	EventContextsAttribute = InitParams.EventContexts;
	if (EventContextsAttribute.IsSet())
	{
		CachedEventContexts.Reset();
		for (UObject* Object : EventContextsAttribute.Get())
		{
			CachedEventContexts.Add(Object);
		}
	}

	PlaybackContextAttribute = InitParams.PlaybackContext;
	CachedPlaybackContext = PlaybackContextAttribute.Get(nullptr);

	Settings = USequencerSettingsContainer::GetOrCreate<USequencerSettings>(*InitParams.ViewParams.UniqueName);

	Settings->GetOnLockPlaybackToAudioClockChanged().AddSP(this, &FSequencer::ResetTimingManager);
	ResetTimingManager(Settings->ShouldLockPlaybackToAudioClock());

	Settings->GetOnEvaluateSubSequencesInIsolationChanged().AddSP(this, &FSequencer::RestorePreAnimatedState);
	
	{
		FDelegateHandle OnBlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddLambda([&]{ State.InvalidateExpiredObjects(); });
		AcquiredResources.Add([=]{ GEditor->OnBlueprintCompiled().Remove(OnBlueprintCompiledHandle); });
	}

	ToolkitHost = InitParams.ToolkitHost;

	ScrubPosition = InitParams.ViewParams.InitialScrubPosition;
	PlayRate = 1.f;
	ShuttleMultiplier = 0;
	ObjectChangeListener = InObjectChangeListener;

	check( ObjectChangeListener.IsValid() );
	
	RootSequence = InitParams.RootSequence;

	TemplateStore = MakeShared<FSequencerTemplateStore>();

	ActiveTemplateIDs.Add(MovieSceneSequenceID::Root);
	RootTemplateInstance.Initialize(*InitParams.RootSequence, *this, TemplateStore.ToSharedRef());

	// Make internal widgets
	SequencerWidget = SNew( SSequencer, SharedThis( this ) )
		.ViewRange( this, &FSequencer::GetViewRange )
		.ClampRange( this, &FSequencer::GetClampRange )
		.PlaybackRange( this, &FSequencer::GetPlaybackRange )
		.PlaybackStatus( this, &FSequencer::GetPlaybackStatus )
		.SelectionRange( this, &FSequencer::GetSelectionRange )
		.SubSequenceRange( this, &FSequencer::GetSubSequenceRange )
		.OnPlaybackRangeChanged( this, &FSequencer::SetPlaybackRange )
		.OnPlaybackRangeBeginDrag( this, &FSequencer::OnPlaybackRangeBeginDrag )
		.OnPlaybackRangeEndDrag( this, &FSequencer::OnPlaybackRangeEndDrag )
		.OnSelectionRangeChanged( this, &FSequencer::SetSelectionRange )
		.OnSelectionRangeBeginDrag( this, &FSequencer::OnSelectionRangeBeginDrag )
		.OnSelectionRangeEndDrag( this, &FSequencer::OnSelectionRangeEndDrag )
		.IsPlaybackRangeLocked( this, &FSequencer::IsPlaybackRangeLocked )
		.OnTogglePlaybackRangeLocked( this, &FSequencer::TogglePlaybackRangeLocked )
		.TimeSnapInterval( this, &FSequencer::GetFixedFrameInterval )
		.ScrubPosition( this, &FSequencer::GetLocalTime )
		.OnBeginScrubbing( this, &FSequencer::OnBeginScrubbing )
		.OnEndScrubbing( this, &FSequencer::OnEndScrubbing )
		.OnScrubPositionChanged( this, &FSequencer::OnScrubPositionChanged )
		.OnViewRangeChanged( this, &FSequencer::SetViewRange )
		.OnClampRangeChanged( this, &FSequencer::OnClampRangeChanged )
		.OnGetNearestKey( this, &FSequencer::OnGetNearestKey )
		.OnGetAddMenuContent(InitParams.ViewParams.OnGetAddMenuContent)
		.OnReceivedFocus(InitParams.ViewParams.OnReceivedFocus)
		.AddMenuExtender(InitParams.ViewParams.AddMenuExtender)
		.ToolbarExtender(InitParams.ViewParams.ToolbarExtender);

	// When undo occurs, get a notification so we can make sure our view is up to date
	GEditor->RegisterForUndo(this);

	// Create tools and bind them to this sequencer
	for( int32 DelegateIndex = 0; DelegateIndex < TrackEditorDelegates.Num(); ++DelegateIndex )
	{
		check( TrackEditorDelegates[DelegateIndex].IsBound() );
		// Tools may exist in other modules, call a delegate that will create one for us 
		TSharedRef<ISequencerTrackEditor> TrackEditor = TrackEditorDelegates[DelegateIndex].Execute( SharedThis( this ) );
		TrackEditors.Add( TrackEditor );
	}

	for (int32 DelegateIndex = 0; DelegateIndex < EditorObjectBindingDelegates.Num(); ++DelegateIndex)
	{
		check(EditorObjectBindingDelegates[DelegateIndex].IsBound());
		// Object bindings may exist in other modules, call a delegate that will create one for us 
		TSharedRef<ISequencerEditorObjectBinding> ObjectBinding = EditorObjectBindingDelegates[DelegateIndex].Execute(SharedThis(this));
		ObjectBindings.Add(ObjectBinding);
	}

	ZoomAnimation = FCurveSequence();
	ZoomCurve = ZoomAnimation.AddCurve(0.f, 0.2f, ECurveEaseFunction::QuadIn);
	OverlayAnimation = FCurveSequence();
	OverlayCurve = OverlayAnimation.AddCurve(0.f, 0.2f, ECurveEaseFunction::QuadIn);

	// Update initial movie scene data
	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::ActiveMovieSceneChanged );
	UpdateTimeBoundsToFocusedMovieScene();

	// NOTE: Could fill in asset editor commands here!

	BindCommands();

	for (auto TrackEditor : TrackEditors)
	{
		TrackEditor->OnInitialize();
	}

	OnActivateSequenceEvent.Broadcast(ActiveTemplateIDs[0]);
}


FSequencer::FSequencer()
	: SequencerCommandBindings( new FUICommandList )
	, SequencerSharedBindings( new FUICommandList )
	, TargetViewRange(0.f, 5.f)
	, LastViewRange(0.f, 5.f)
	, ViewRangeBeforeZoom(TRange<float>::Empty())
	, PlaybackState( EMovieScenePlayerStatus::Stopped )
	, ScrubPosition( 0.0f )
	, bPerspectiveViewportPossessionEnabled( true )
	, bPerspectiveViewportCameraCutEnabled( false )
	, bIsEditingWithinLevelEditor( false )
	, bShowCurveEditor( false )
	, bNeedTreeRefresh( false )
	, bNeedInstanceRefresh( false )
	, StoredPlaybackState( EMovieScenePlayerStatus::Stopped )
	, NodeTree( MakeShareable( new FSequencerNodeTree( *this ) ) )
	, bUpdatingSequencerSelection( false )
	, bUpdatingExternalSelection( false )
	, OldMaxTickRate(GEngine->GetMaxFPS())
	, bNeedsEvaluate(false)
{
	Selection.GetOnOutlinerNodeSelectionChanged().AddRaw(this, &FSequencer::OnSelectedOutlinerNodesChanged);
	Selection.GetOnNodesWithSelectedKeysOrSectionsChanged().AddRaw(this, &FSequencer::OnSelectedOutlinerNodesChanged);
	Selection.GetOnOutlinerNodeSelectionChangedObjectGuids().AddRaw(this, &FSequencer::OnSelectedOutlinerNodesChanged);
}


FSequencer::~FSequencer()
{
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}

	for (auto TrackEditor : TrackEditors)
	{
		TrackEditor->OnRelease();
	}

	AcquiredResources.Release();
	SequencerWidget.Reset();
	TrackEditors.Empty();
}


void FSequencer::Close()
{
	RestorePreAnimatedState();

	for (auto TrackEditor : TrackEditors)
	{
		TrackEditor->OnRelease();
	}

	SequencerWidget.Reset();
	TrackEditors.Empty();
}


void FSequencer::Tick(float InDeltaTime)
{
	static bool bEnableRefCountCheck = true;
	if (bEnableRefCountCheck && !FSlateApplication::Get().AnyMenusVisible())
	{
		const int32 SequencerRefCount = AsShared().GetSharedReferenceCount() - 1;
		ensureAlwaysMsgf(SequencerRefCount == 1, TEXT("Multiple persistent shared references detected for Sequencer. There should only be one persistent authoritative reference. Found %d additional references which will result in FSequencer not being released correctly."), SequencerRefCount - 1);
	}

	Selection.Tick();
	
	if (PlaybackContextAttribute.IsBound())
	{
		TWeakObjectPtr<UObject> NewPlaybackContext = PlaybackContextAttribute.Get();

		if (CachedPlaybackContext != NewPlaybackContext)
		{
			PrePossessionViewTargets.Reset();
			State.ClearObjectCaches(*this);
			CachedPlaybackContext = NewPlaybackContext;
		}
	}

	if (RootTemplateInstance.IsDirty())
	{
		bNeedsEvaluate = true;
	}
	
	if ( bNeedInstanceRefresh )
	{
		UpdateRuntimeInstances();
		bNeedInstanceRefresh = false;
	}

	if (bNeedTreeRefresh)
	{
		SelectionPreview.Empty();

		SequencerWidget->UpdateLayoutTree();
		bNeedTreeRefresh = false;

		SetPlaybackStatus(StoredPlaybackState);
	}

	UObject* PlaybackContext = GetPlaybackContext();
	UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;
	float Dilation = World ? World->GetWorldSettings()->MatineeTimeDilation : 1.f;
	FTimeAndDelta TimeAndDelta = TimingManager->AdjustTime(GetGlobalTime(), InDeltaTime, PlayRate, Dilation);

	static const float AutoScrollFactor = 0.1f;

	// Animate the autoscroll offset if it's set
	if (AutoscrollOffset.IsSet())
	{
		float Offset = AutoscrollOffset.GetValue() * AutoScrollFactor;
		SetViewRange(TRange<float>(TargetViewRange.GetLowerBoundValue() + Offset, TargetViewRange.GetUpperBoundValue() + Offset), EViewRangeInterpolation::Immediate);
	}

	// Animate the autoscrub offset if it's set
	if (AutoscrubOffset.IsSet())
	{
		float Offset = AutoscrubOffset.GetValue() * AutoScrollFactor;
		SetLocalTimeDirectly(GetLocalTime() + Offset);
	}

	// override max frame rate
	if (PlaybackState == EMovieScenePlayerStatus::Playing)
	{
		bool bIsFixedFrameIntervalPlayback = false;
		if (GetFocusedMovieSceneSequence() && GetFocusedMovieSceneSequence()->GetMovieScene())
		{
			bIsFixedFrameIntervalPlayback = GetFocusedMovieSceneSequence()->GetMovieScene()->GetForceFixedFrameIntervalPlayback();
		}

		const float TimeSnapInterval = GetFixedFrameInterval();

		if (SequencerSnapValues::IsTimeSnapIntervalFrameRate(TimeSnapInterval) && bIsFixedFrameIntervalPlayback)
		{
			GEngine->SetMaxFPS(1.f / TimeSnapInterval);
		}
		else
		{
			GEngine->SetMaxFPS(OldMaxTickRate);
		}
	}

	if (GetSelectionRange().IsEmpty() && GetLoopMode() == SLM_LoopSelectionRange)
	{
		Settings->SetLoopMode(SLM_Loop);
	}

	if (PlaybackState == EMovieScenePlayerStatus::Playing ||
		PlaybackState == EMovieScenePlayerStatus::Recording)
	{
		// Put the time into local space
		SetLocalTimeLooped(TimeAndDelta.Time * RootToLocalTransform);
	}
	else
	{
		PlayPosition.Reset(GetGlobalTime());
	}

	UpdateSubSequenceData();

	// Tick all the tools we own as well
	for (int32 EditorIndex = 0; EditorIndex < TrackEditors.Num(); ++EditorIndex)
	{
		TrackEditors[EditorIndex]->Tick(TimeAndDelta.Delta * PlayRate);
	}

	if (!IsInSilentMode())
	{
		if (bNeedsEvaluate)
		{
			FMovieSceneEvaluationRange Range = PlayPosition.JumpTo(ScrubPosition, GetFocusedMovieSceneSequence()->GetMovieScene()->GetOptionalFixedFrameInterval());
			EvaluateInternal(Range);
		}
	}

	ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
	if(SequenceRecorder.IsRecording())
	{
		UMovieSceneSubSection* Section = UMovieSceneSubSection::GetRecordingSection();
		if(Section != nullptr)
		{
			Section->SetEndTime(Section->GetStartTime() + SequenceRecorder.GetCurrentRecordingLength());
		}
	}

	// Reset any player controllers that we were possessing, if we're not possessing them any more
	if (!IsPerspectiveViewportCameraCutEnabled() && PrePossessionViewTargets.Num())
	{
		for (const FCachedViewTarget& CachedView : PrePossessionViewTargets)
		{
			APlayerController* PlayerController = CachedView.PlayerController.Get();
			AActor* ViewTarget = CachedView.ViewTarget.Get();

			if (PlayerController && ViewTarget)
			{
				PlayerController->SetViewTarget(ViewTarget);
			}
		}
		PrePossessionViewTargets.Reset();
	}
}


TSharedRef<SWidget> FSequencer::GetSequencerWidget() const
{
	return SequencerWidget.ToSharedRef();
}


UMovieSceneSequence* FSequencer::GetRootMovieSceneSequence() const
{
	return RootSequence.Get();
}


UMovieSceneSequence* FSequencer::GetFocusedMovieSceneSequence() const
{
	// the last item is the focused movie scene
	if (ActiveTemplateIDs.Num())
	{
		return RootTemplateInstance.GetSequence(ActiveTemplateIDs.Last());
	}

	return nullptr;
}


void FSequencer::ResetToNewRootSequence(UMovieSceneSequence& NewSequence)
{
	RootSequence = &NewSequence;
	RestorePreAnimatedState();

	RootTemplateInstance.Finish(*this);

	TemplateStore->Reset();

	ActiveTemplateIDs.Reset();
	ActiveTemplateIDs.Add(MovieSceneSequenceID::Root);

	RootTemplateInstance.Initialize(NewSequence, *this);

	RootToLocalTransform = FMovieSceneSequenceTransform();

	ResetPerMovieSceneData();
	SequencerWidget->ResetBreadcrumbs();

	OnActivateSequenceEvent.Broadcast(ActiveTemplateIDs.Top());
}


void FSequencer::FocusSequenceInstance(UMovieSceneSubSection& InSubSection)
{
	// Root out the SequenceID for the sub section
	FMovieSceneSequenceID SequenceID = MovieSceneSequenceID::Root;

	const FMovieSceneSequenceHierarchyNode* Node = RootTemplateInstance.GetHierarchy().FindNode(ActiveTemplateIDs.Last());

	FName SearchForName(*InSubSection.GetPathNameInMovieScene());
	for (FMovieSceneSequenceIDRef ChildID : Node->Children)
	{
		const FMovieSceneSubSequenceData* SubSequence = RootTemplateInstance.GetHierarchy().FindSubData(ChildID);
		if (SearchForName == SubSequence->SectionPath)
		{
			SequenceID = ChildID;
			break;
		}
	}

	if (!ensure(SequenceID != MovieSceneSequenceID::Root))
	{
		return;
	}

	ActiveTemplateIDs.Push(SequenceID);

	if (Settings->ShouldEvaluateSubSequencesInIsolation())
	{
		RestorePreAnimatedState();
	}

	UpdateSubSequenceData();

	// Reset data that is only used for the previous movie scene
	ResetPerMovieSceneData();
	SequencerWidget->UpdateBreadcrumbs();

	OnActivateSequenceEvent.Broadcast(ActiveTemplateIDs.Top());

	bNeedsEvaluate = true;
}


FGuid FSequencer::CreateBinding(UObject& InObject, const FString& InName)
{
	UMovieSceneSequence* OwnerSequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();
		
	const FGuid PossessableGuid = OwnerMovieScene->AddPossessable(InName, InObject.GetClass());

	// Attempt to use the parent as a context if necessary
	UObject* ParentObject = OwnerSequence->GetParentObject(&InObject);
	UObject* BindingContext = GetPlaybackContext();

	if (ParentObject)
	{
		// Ensure we have possessed the outer object, if necessary
		FGuid ParentGuid = GetHandleToObject(ParentObject);
		
		if (OwnerSequence->AreParentContextsSignificant())
		{
			BindingContext = ParentObject;
		}

		// Set up parent/child guids for possessables within spawnables
		if (ParentGuid.IsValid())
		{
			FMovieScenePossessable* ChildPossessable = OwnerMovieScene->FindPossessable(PossessableGuid);
			if (ensure(ChildPossessable))
			{
				ChildPossessable->SetParent(ParentGuid);
			}

			FMovieSceneSpawnable* ParentSpawnable = OwnerMovieScene->FindSpawnable(ParentGuid);
			if (ParentSpawnable)
			{
				ParentSpawnable->AddChildPossessable(PossessableGuid);
			}
		}
	}

	OwnerSequence->BindPossessableObject(PossessableGuid, InObject, BindingContext);

	return PossessableGuid;
}


UObject* FSequencer::GetPlaybackContext() const
{
	return CachedPlaybackContext.Get();
}

TArray<UObject*> FSequencer::GetEventContexts() const
{
	TArray<UObject*> Temp;
	CopyFromWeakArray(Temp, CachedEventContexts);
	return Temp;
}

void FSequencer::GetKeysFromSelection(TUniquePtr<ISequencerKeyCollection>& KeyCollection, float DuplicateThreshold)
{
	if (!KeyCollection.IsValid())
	{
		KeyCollection.Reset(new FGroupedKeyCollection);
	}

	TArray<FSequencerDisplayNode*> SelectedNodes;
	for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
	{
		SelectedNodes.Add(&Node.Get());
	}

	KeyCollection->InitializeRecursive(SelectedNodes, DuplicateThreshold);
}


void FSequencer::PopToSequenceInstance(FMovieSceneSequenceIDRef SequenceID)
{
	if( ActiveTemplateIDs.Num() > 1 )
	{
		// Pop until we find the movie scene to focus
		while( SequenceID != ActiveTemplateIDs.Last() )
		{
			ActiveTemplateIDs.Pop();
		}

		check( ActiveTemplateIDs.Num() > 0 );
		UpdateSubSequenceData();

		// Pop out of any potentially locked cameras from the shot and toggle on camera cuts
		for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); ++i)
		{		
			FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[i];
			if (LevelVC && LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview() && LevelVC->GetViewMode() != VMI_Unknown)
			{
				LevelVC->SetActorLock(nullptr);
				LevelVC->bLockedCameraView = false;
				LevelVC->UpdateViewForLockedActor();
				LevelVC->Invalidate();
			}
		}
		SetPerspectiveViewportCameraCutEnabled(true);

		ResetPerMovieSceneData();
		SequencerWidget->UpdateBreadcrumbs();

		OnActivateSequenceEvent.Broadcast(ActiveTemplateIDs.Top());

		bNeedsEvaluate = true;
	}
}

void FSequencer::UpdateSubSequenceData()
{
	SubSequenceRange = TRange<float>(0.f);
	RootToLocalTransform = FMovieSceneSequenceTransform();

	// Find the parent sub section and set up the sub sequence range, if necessary
	if (ActiveTemplateIDs.Num() <= 1)
	{
		return;
	}

	const FMovieSceneSubSequenceData* SubSequenceData = RootTemplateInstance.GetHierarchy().FindSubData(ActiveTemplateIDs.Top());

	if (SubSequenceData)
	{
		SubSequenceRange = SubSequenceData->ValidPlayRange;
		RootToLocalTransform = SubSequenceData->RootToSequenceTransform;
	}
}

void FSequencer::RerunConstructionScripts()
{
	TSet<TWeakObjectPtr<AActor> > BoundActors;

	FMovieSceneRootEvaluationTemplateInstance& RootTemplate = GetEvaluationTemplate();
		
	UMovieSceneSequence* Sequence = RootTemplate.GetSequence(MovieSceneSequenceID::Root);

	GetConstructionScriptActors(Sequence->GetMovieScene(), MovieSceneSequenceID::Root, BoundActors);

	for (auto& SubInstance : RootTemplate.GetSubInstances())
	{
		if (RootTemplateInstance.GetThisFrameMetaData().ActiveSequences.Contains(SubInstance.Key))
		{
			UMovieSceneSequence* SubSequence = SubInstance.Value.Sequence.Get();
			if (SubSequence)
			{
				GetConstructionScriptActors(SubSequence->GetMovieScene(), SubInstance.Key, BoundActors);
			}
		}
	}

	for (TWeakObjectPtr<AActor> BoundActor : BoundActors)
	{
		if (BoundActor.IsValid())
		{
			BoundActor.Get()->RerunConstructionScripts();
		}
	}
}

void FSequencer::GetConstructionScriptActors(UMovieScene* MovieScene, FMovieSceneSequenceIDRef SequenceID, TSet<TWeakObjectPtr<AActor> >& BoundActors)
{
	for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index)
	{
		FGuid ThisGuid = MovieScene->GetPossessable(Index).GetGuid();

		for (TWeakObjectPtr<> WeakObject : FindBoundObjects(ThisGuid, SequenceID))
		{
			if (WeakObject.IsValid())
			{
				AActor* Actor = Cast<AActor>(WeakObject.Get());
	
				if (Actor)
				{
					UBlueprint* Blueprint = Cast<UBlueprint>(Actor->GetClass()->ClassGeneratedBy);
					if (Blueprint && Blueprint->bRunConstructionScriptInSequencer)
					{
						BoundActors.Add(Actor);
					}
				}
			}
		}
	}

	for (int32 Index = 0; Index < MovieScene->GetSpawnableCount(); ++Index)
	{
		FGuid ThisGuid = MovieScene->GetSpawnable(Index).GetGuid();

		for (TWeakObjectPtr<> WeakObject : FindBoundObjects(ThisGuid, SequenceID))
		{
			if (WeakObject.IsValid())
			{
				AActor* Actor = Cast<AActor>(WeakObject.Get());

				if (Actor)
				{
					UBlueprint* Blueprint = Cast<UBlueprint>(Actor->GetClass()->ClassGeneratedBy);
					if (Blueprint && Blueprint->bRunConstructionScriptInSequencer)
					{
						BoundActors.Add(Actor);
					}
				}
			}
		}
	}
}

void FSequencer::DeleteSections(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections)
{
	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	bool bAnythingRemoved = false;

	FScopedTransaction DeleteSectionTransaction( NSLOCTEXT("Sequencer", "DeleteSection_Transaction", "Delete Section") );

	for (const auto Section : Sections)
	{
		if (!Section.IsValid() || Section->IsLocked())
		{
			continue;
		}

		// if this check fails then the section is outered to a type that doesnt know about the section
		UMovieSceneTrack* Track = CastChecked<UMovieSceneTrack>(Section->GetOuter());
		{
			Track->SetFlags(RF_Transactional);
			Track->Modify();
			Track->RemoveSection(*Section);
		}

		bAnythingRemoved = true;
	}

	if (bAnythingRemoved)
	{
		// Full refresh required just in case the last section was removed from any track.
		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemRemoved );
	}

	Selection.EmptySelectedSections();
	SequencerHelpers::ValidateNodesWithSelectedKeysOrSections(*this);
}


void FSequencer::DeleteSelectedKeys()
{
	FScopedTransaction DeleteKeysTransaction( NSLOCTEXT("Sequencer", "DeleteSelectedKeys_Transaction", "Delete Selected Keys") );
	bool bAnythingRemoved = false;
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();

	for (const FSequencerSelectedKey& Key : SelectedKeysArray)
	{
		if (Key.IsValid())
		{
			if (Key.Section->TryModify())
			{
				Key.KeyArea->DeleteKey(Key.KeyHandle.GetValue());
				bAnythingRemoved = true;
			}
		}
	}

	if (bAnythingRemoved)
	{
		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}

	Selection.EmptySelectedKeys();
	SequencerHelpers::ValidateNodesWithSelectedKeysOrSections(*this);
}


void FSequencer::SetInterpTangentMode(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode)
{
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();
	if (SelectedKeysArray.Num() == 0)
	{
		return;
	}

	FScopedTransaction SetInterpTangentModeTransaction(NSLOCTEXT("Sequencer", "SetInterpTangentMode_Transaction", "Set Interpolation and Tangent Mode"));
	bool bAnythingChanged = false;

	for (const FSequencerSelectedKey& Key : SelectedKeysArray)
	{
		if (Key.IsValid())
		{
			if (Key.Section->TryModify())
			{
				Key.KeyArea->SetKeyInterpMode(Key.KeyHandle.GetValue(), InterpMode);
				Key.KeyArea->SetKeyTangentMode(Key.KeyHandle.GetValue(), TangentMode);
				bAnythingChanged = true;
			}
		}
	}

	if (bAnythingChanged)
	{
		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
}


bool FSequencer::IsInterpTangentModeSelected(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode) const
{
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();

	bool bAllSelected = false;
	for (const FSequencerSelectedKey& Key : SelectedKeysArray)
	{
		if (Key.IsValid())
		{
			bAllSelected = true;
			if (Key.KeyArea->GetKeyInterpMode(Key.KeyHandle.GetValue()) != InterpMode || 
				Key.KeyArea->GetKeyTangentMode(Key.KeyHandle.GetValue()) != TangentMode)
			{
				bAllSelected = false;
				break;
			}
		}
	}
	return bAllSelected;
}


float FSequencer::GetFixedFrameInterval() const
{
	if (GetFocusedMovieSceneSequence() && GetFocusedMovieSceneSequence()->GetMovieScene())
	{
		return GetFocusedMovieSceneSequence()->GetMovieScene()->GetFixedFrameInterval();
	}

	return 1.f;
}

void FSequencer::SnapToFrame()
{
	FScopedTransaction SnapToFrameTransaction(NSLOCTEXT("Sequencer", "SnapToFrame_Transaction", "Snap Selected Keys to Frame"));
	bool bAnythingChanged = false;
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();

	for (const FSequencerSelectedKey& Key : SelectedKeysArray)
	{
		if (Key.IsValid())
		{
			if (Key.Section->TryModify())
			{
				float NewKeyTime = Key.KeyArea->GetKeyTime(Key.KeyHandle.GetValue());

				// Convert to frame
				float FrameRate = 1.0f / GetFixedFrameInterval();
				int32 NewFrame = SequencerHelpers::TimeToFrame(NewKeyTime, FrameRate);

				// Convert back to time
				NewKeyTime = SequencerHelpers::FrameToTime(NewFrame, FrameRate);

				Key.KeyArea->SetKeyTime(Key.KeyHandle.GetValue(), NewKeyTime);
				bAnythingChanged = true;
			}
		}
	}

	if (bAnythingChanged)
	{
		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
}


bool FSequencer::CanSnapToFrame() const
{
	const bool bKeysSelected = Selection.GetSelectedKeys().Num() > 0;

	return bKeysSelected && CanShowFrameNumbers();
}

void FSequencer::TransformSelectedKeysAndSections(float InDeltaTime, float InScale)
{
	FScopedTransaction TransformKeysAndSectionsTransaction(NSLOCTEXT("Sequencer", "TransformKeysandSections_Transaction", "Transform Keys and Sections"));
	bool bAnythingChanged = false;
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();

	TArray<UMovieSceneSection*> SectionsWithKeys;
	
	FSequencerDisplayNode::DisableKeyGoupingRegeneration();
	
	for (const FSequencerSelectedKey& Key : SelectedKeysArray)
	{
		if (Key.IsValid())
		{
			UMovieSceneSection* Section = Key.Section;

			bool SectionModified = SectionsWithKeys.Contains(Section);

			if (!SectionModified)
			{
				if (Key.Section->TryModify())
				{
					SectionModified = true;
					SectionsWithKeys.Add(Section);
				}
			}
			
			if (SectionModified)
			{
				if (InScale != 0.f)
				{
					Key.KeyArea->DilateKey(Key.KeyHandle.GetValue(), InScale, GetLocalTime());
					bAnythingChanged = true;
				}
				if (InDeltaTime != 0.f)
				{
					Key.KeyArea->MoveKey(Key.KeyHandle.GetValue(), InDeltaTime);
					bAnythingChanged = true;
				}

				float NewKeyTime = Key.KeyArea->GetKeyTime(Key.KeyHandle.GetValue()) + InDeltaTime;
				if (NewKeyTime > Section->GetEndTime())
				{
					Section->SetEndTime(NewKeyTime);
				}
				else if (NewKeyTime < Section->GetStartTime())
				{
					Section->SetStartTime(NewKeyTime);
				}
			}
		}
	}

	FSequencerDisplayNode::EnableKeyGoupingRegeneration();

	for (TWeakObjectPtr<UMovieSceneSection> SelectedSection : Selection.GetSelectedSections())
	{
		if (SelectedSection.IsValid())
		{
			TSet<FKeyHandle> EmptyKeyHandles;
			SelectedSection.Get()->SetFlags( RF_Transactional );
			if (InScale != 1.f)
			{
				SelectedSection.Get()->DilateSection(InScale, GetLocalTime(), EmptyKeyHandles);
				bAnythingChanged = true;
			}
			if (InDeltaTime != 0.f)
			{
				SelectedSection.Get()->MoveSection(InDeltaTime, EmptyKeyHandles);
				bAnythingChanged = true;
			}
		}
	}

	if (bAnythingChanged)
	{
		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
}

void FSequencer::TranslateSelectedKeysAndSections(bool bTranslateLeft)
{
	TransformSelectedKeysAndSections(bTranslateLeft ? -GetFixedFrameInterval() : GetFixedFrameInterval(), 1.f);
}

void FSequencer::OnActorsDropped( const TArray<TWeakObjectPtr<AActor> >& Actors )
{
	AddActors(Actors);
}


void FSequencer::NotifyMovieSceneDataChangedInternal()
{
	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::Unknown );
}


void FSequencer::NotifyMovieSceneDataChanged( EMovieSceneDataChangeType DataChangeType )
{
	if ( DataChangeType == EMovieSceneDataChangeType::ActiveMovieSceneChanged ||
		DataChangeType == EMovieSceneDataChangeType::Unknown )
	{
		LabelManager.SetMovieScene( GetFocusedMovieSceneSequence()->GetMovieScene() );
	}

	StoredPlaybackState = GetPlaybackStatus();

	if ( DataChangeType == EMovieSceneDataChangeType::MovieSceneStructureItemRemoved ||
		DataChangeType == EMovieSceneDataChangeType::MovieSceneStructureItemsChanged ||
		DataChangeType == EMovieSceneDataChangeType::Unknown )
	{
		// When structure items are removed, or we don't know what may have changed, refresh the tree and instances immediately so that the data
		// is in a consistent state when the UI is updated during the next tick.
		SetPlaybackStatus( EMovieScenePlayerStatus::Stopped );
		UpdateRuntimeInstances();
		SelectionPreview.Empty();
		SequencerWidget->UpdateLayoutTree();
		bNeedInstanceRefresh = false;
		bNeedTreeRefresh = false;
		SetPlaybackStatus( StoredPlaybackState );
	}
	else if (DataChangeType == EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately)
	{
		UpdateRuntimeInstances();

		// Evaluate now
		EvaluateInternal(FMovieSceneEvaluationRange(ScrubPosition));
	}
	else if (DataChangeType == EMovieSceneDataChangeType::RefreshAllImmediately)
	{
		UpdateRuntimeInstances();

		SequencerWidget->UpdateLayoutTree();
		bNeedInstanceRefresh = false;
		bNeedTreeRefresh = false;

		// Evaluate now
		EvaluateInternal(FMovieSceneEvaluationRange(ScrubPosition));
	}
	else
	{
		if ( DataChangeType != EMovieSceneDataChangeType::TrackValueChanged )
		{
			// All changes types except for track value changes require refreshing the outliner tree.
			SetPlaybackStatus( EMovieScenePlayerStatus::Stopped );
			bNeedTreeRefresh = true;
		}
		bNeedInstanceRefresh = true;
	}

	if (DataChangeType == EMovieSceneDataChangeType::TrackValueChanged || 
		DataChangeType == EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately || 
		DataChangeType == EMovieSceneDataChangeType::Unknown ||
		DataChangeType == EMovieSceneDataChangeType::MovieSceneStructureItemRemoved)
	{
		FSequencerEdMode* SequencerEdMode = (FSequencerEdMode*)(GLevelEditorModeTools().GetActiveMode(FSequencerEdMode::EM_SequencerMode));
		if (SequencerEdMode != nullptr)
		{
			SequencerEdMode->CleanUpMeshTrails();
		}
	}

	bNeedsEvaluate = true;
	State.ClearObjectCaches(*this);

	UpdatePlaybackRange();
	OnMovieSceneDataChangedDelegate.Broadcast(DataChangeType);
}


FAnimatedRange FSequencer::GetViewRange() const
{
	FAnimatedRange AnimatedRange(FMath::Lerp(LastViewRange.GetLowerBoundValue(), TargetViewRange.GetLowerBoundValue(), ZoomCurve.GetLerp()),
		FMath::Lerp(LastViewRange.GetUpperBoundValue(), TargetViewRange.GetUpperBoundValue(), ZoomCurve.GetLerp()));

	if (ZoomAnimation.IsPlaying())
	{
		AnimatedRange.AnimationTarget = TargetViewRange;
	}

	return AnimatedRange;
}


FAnimatedRange FSequencer::GetClampRange() const
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	return FocusedMovieScene->GetEditorData().WorkingRange;
}


void FSequencer::SetClampRange(TRange<float> InNewClampRange)
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	FocusedMovieScene->GetEditorData().WorkingRange = InNewClampRange;
}


TOptional<TRange<float>> FSequencer::GetSubSequenceRange() const
{
	if (Settings->ShouldEvaluateSubSequencesInIsolation() || ActiveTemplateIDs.Num() == 1)
	{
		return TOptional<TRange<float>>();
	}
	return SubSequenceRange;
}


TRange<float> FSequencer::GetSelectionRange() const
{
	return GetFocusedMovieSceneSequence()->GetMovieScene()->GetSelectionRange();
}


void FSequencer::SetSelectionRange(TRange<float> Range)
{
	const FScopedTransaction Transaction(LOCTEXT("SetSelectionRange_Transaction", "Set Selection Range"));
	UMovieScene* FocussedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	FocussedMovieScene->SetSelectionRange(Range);
}


void FSequencer::SetSelectionRangeEnd()
{
	const float LocalTime = GetLocalTime();

	if (GetSelectionRange().GetLowerBoundValue() >= LocalTime)
	{
		SetSelectionRange(TRange<float>(LocalTime));
	}
	else
	{
		SetSelectionRange(TRange<float>(GetSelectionRange().GetLowerBoundValue(), LocalTime));
	}
}


void FSequencer::SetSelectionRangeStart()
{
	const float LocalTime = GetLocalTime();

	if (GetSelectionRange().GetUpperBoundValue() <= LocalTime)
	{
		SetSelectionRange(TRange<float>(LocalTime));
	}
	else
	{
		SetSelectionRange(TRange<float>(LocalTime, GetSelectionRange().GetUpperBoundValue()));
	}
}


void FSequencer::SelectInSelectionRange(const TSharedRef<FSequencerDisplayNode>& DisplayNode, const TRange<float>& SelectionRange, bool bSelectKeys, bool bSelectSections)
{
	if (DisplayNode->GetType() == ESequencerNode::Track)
	{
		if (bSelectKeys)
		{
			TSet<TSharedPtr<IKeyArea>> OutKeyAreas;
			SequencerHelpers::GetAllKeyAreas(DisplayNode, OutKeyAreas);

			for (auto KeyArea : OutKeyAreas)
			{
				if (!KeyArea.IsValid())
				{
					continue;
				}

				UMovieSceneSection* Section = KeyArea->GetOwningSection();

				if (Section == nullptr)
				{
					continue;
				}

				if (bSelectKeys)
				{
					TSet<FKeyHandle> OutKeyHandles;
					Section->GetKeyHandles(OutKeyHandles, SelectionRange);

					for (auto KeyHandle : OutKeyHandles)
					{
						Selection.AddToSelection(FSequencerSelectedKey(*Section, KeyArea, KeyHandle));
					}
				}
			}
		}

		if (bSelectSections)
		{
			// Use an exclusive selection range to prevent the selection of a section that ends right at the selection range start
			TRange<float> ExclusiveSectionRange = TRange<float>(TRangeBound<float>::Exclusive(SelectionRange.GetLowerBoundValue()), TRangeBound<float>::Exclusive(SelectionRange.GetUpperBoundValue()));
			TSet<TWeakObjectPtr<UMovieSceneSection>> OutSections;
			SequencerHelpers::GetAllSections(DisplayNode, OutSections);

			for (auto Section : OutSections)
			{
				if (Section.IsValid() && Section->GetRange().Overlaps(ExclusiveSectionRange))
				{
					Selection.AddToSelection(Section.Get());
				}
			}
		}
	}

	for (const auto& ChildNode : DisplayNode->GetChildNodes())
	{
		SelectInSelectionRange(ChildNode, SelectionRange, bSelectKeys, bSelectSections);
	}
}

void FSequencer::ResetSelectionRange()
{
	SetSelectionRange(TRange<float>::Empty());
}

void FSequencer::SelectInSelectionRange(bool bSelectKeys, bool bSelectSections)
{
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	TRange<float> SelectionRange = MovieScene->GetSelectionRange();

	Selection.Empty();

	for (const TSharedRef<FSequencerDisplayNode>& DisplayNode : NodeTree->GetRootNodes())
	{
		SelectInSelectionRange(DisplayNode, SelectionRange, bSelectKeys, bSelectSections);
	}
}


TRange<float> FSequencer::GetPlaybackRange() const
{
	return GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange();
}


void FSequencer::SetPlaybackRange(TRange<float> Range)
{
	if (ensure(Range.HasLowerBound() && Range.HasUpperBound() && !Range.IsDegenerate()))
	{
		UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
		
		if (!FocusedMovieScene->IsPlaybackRangeLocked())
		{
			const FScopedTransaction Transaction(LOCTEXT("SetPlaybackRange_Transaction", "Set Playback Range"));

			FocusedMovieScene->SetPlaybackRange(Range.GetLowerBoundValue(), Range.GetUpperBoundValue());

			bNeedsEvaluate = true;
		}
	}
}

UMovieSceneSection* FSequencer::FindNextOrPreviousShot(UMovieSceneSequence* Sequence, float CurrentTime, const bool bNextShot) const
{
	UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

	UMovieSceneTrack* CinematicShotTrack = OwnerMovieScene->FindMasterTrack(UMovieSceneCinematicShotTrack::StaticClass());
	if (!CinematicShotTrack)
	{
		return nullptr;
	}

	float MinTime = TNumericLimits<float>::Max();

	TMap<float, int32> StartTimeMap;
	for (int32 SectionIndex = 0; SectionIndex < CinematicShotTrack->GetAllSections().Num(); ++SectionIndex)
	{
		UMovieSceneSection* ShotSection = CinematicShotTrack->GetAllSections()[SectionIndex];

		if (ShotSection)
		{
			StartTimeMap.Add(ShotSection->GetStartTime(), SectionIndex);
		}
	}

	StartTimeMap.KeySort(TLess<float>());

	int32 MinShotIndex = -1;
	for (auto StartTimeIt = StartTimeMap.CreateIterator(); StartTimeIt; ++StartTimeIt)
	{
		float StartTime = StartTimeIt->Key;
		if (bNextShot)
		{
			if (StartTime > CurrentTime)
			{
				float DiffTime = FMath::Abs(StartTime - CurrentTime);
				if (DiffTime < MinTime)
				{
					MinTime = DiffTime;
					MinShotIndex = StartTimeIt->Value;
				}
			}
		}
		else
		{
			if (CurrentTime >= StartTime)
			{
				float DiffTime = FMath::Abs(StartTime - CurrentTime);
				if (DiffTime < MinTime)
				{
					MinTime = DiffTime;
					MinShotIndex = StartTimeIt->Value;
				}
			}
		}
	}

	int32 TargetShotIndex = -1;

	if (bNextShot)
	{
		TargetShotIndex = MinShotIndex;
	}
	else
	{
		int32 PreviousShotIndex = -1;
		for (auto StartTimeIt = StartTimeMap.CreateIterator(); StartTimeIt; ++StartTimeIt)
		{
			if (StartTimeIt->Value == MinShotIndex)
			{
				if (PreviousShotIndex != -1)
				{
					TargetShotIndex = PreviousShotIndex;
				}
				break;
			}
			PreviousShotIndex = StartTimeIt->Value;
		}
	}

	if (TargetShotIndex == -1)
	{
		return nullptr;
	}	

	return CinematicShotTrack->GetAllSections()[TargetShotIndex];
}

void FSequencer::SetSelectionRangeToShot(const bool bNextShot)
{
	UMovieSceneSection* TargetShotSection = FindNextOrPreviousShot(GetFocusedMovieSceneSequence(), GetGlobalTime(), bNextShot);
		
	if (TargetShotSection)
	{
		SetSelectionRange(TRange<float>(TargetShotSection->GetStartTime(), TargetShotSection->GetEndTime()));
	}
}

void FSequencer::SetPlaybackRangeToAllShots()
{
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

	UMovieSceneTrack* CinematicShotTrack = OwnerMovieScene->FindMasterTrack(UMovieSceneCinematicShotTrack::StaticClass());
	if (!CinematicShotTrack || CinematicShotTrack->GetAllSections().Num() == 0)
	{
		return;
	}

	float LowerBound = CinematicShotTrack->GetAllSections()[0]->GetStartTime();
	float UpperBound = CinematicShotTrack->GetAllSections()[0]->GetEndTime();

	for (int32 SectionIndex = 0; SectionIndex < CinematicShotTrack->GetAllSections().Num(); ++SectionIndex)
	{
		UMovieSceneSection* ShotSection = CinematicShotTrack->GetAllSections()[SectionIndex];
		if (ShotSection)
		{
			if (ShotSection->GetStartTime() < LowerBound)
			{
				LowerBound = ShotSection->GetStartTime();
			}

			if (ShotSection->GetEndTime() > UpperBound)
			{
				UpperBound = ShotSection->GetEndTime();
			}
		}
	}

	SetPlaybackRange(TRange<float>(LowerBound, UpperBound));
}

bool FSequencer::IsPlaybackRangeLocked() const
{
	UMovieSceneSequence* FocusedMovieSceneSequence = GetFocusedMovieSceneSequence();
	if (FocusedMovieSceneSequence != nullptr)
	{
		return FocusedMovieSceneSequence->GetMovieScene()->IsPlaybackRangeLocked();
	}

	return false;
}

void FSequencer::TogglePlaybackRangeLocked()
{
	UMovieSceneSequence* FocusedMovieSceneSequence = GetFocusedMovieSceneSequence();
	if ( FocusedMovieSceneSequence != nullptr )
	{
		FScopedTransaction TogglePlaybackRangeLockTransaction( NSLOCTEXT( "Sequencer", "TogglePlaybackRangeLocked", "Toggle playback range lock" ) );
		UMovieScene* MovieScene = FocusedMovieSceneSequence->GetMovieScene();
		MovieScene->Modify();
		MovieScene->SetPlaybackRangeLocked( !MovieScene->IsPlaybackRangeLocked() );
	}
}

void FSequencer::ResetViewRange()
{
	float InRange = GetPlaybackRange().GetLowerBoundValue();
	float OutRange = GetPlaybackRange().GetUpperBoundValue();
	const float OutputViewSize = OutRange - InRange;
	const float OutputChange = OutputViewSize * 0.1f;

	if (OutputChange > 0)
	{
		InRange -= OutputChange;
		OutRange += OutputChange;

		SetClampRange(TRange<float>(InRange, OutRange));	
		SetViewRange(TRange<float>(InRange, OutRange), EViewRangeInterpolation::Animated);
	}
}


void FSequencer::ZoomViewRange(float InZoomDelta)
{
	float LocalViewRangeMax = TargetViewRange.GetUpperBoundValue();
	float LocalViewRangeMin = TargetViewRange.GetLowerBoundValue();
	const float OutputViewSize = LocalViewRangeMax - LocalViewRangeMin;
	const float OutputChange = OutputViewSize * InZoomDelta;

	float CurrentPositionFraction = (ScrubPosition - LocalViewRangeMin) / OutputViewSize;

	float NewViewOutputMin = LocalViewRangeMin - (OutputChange * CurrentPositionFraction);
	float NewViewOutputMax = LocalViewRangeMax + (OutputChange * (1.f - CurrentPositionFraction));

	if (NewViewOutputMin < NewViewOutputMax)
	{
		SetViewRange(TRange<float>(NewViewOutputMin, NewViewOutputMax), EViewRangeInterpolation::Animated);
	}
}


void FSequencer::ZoomInViewRange()
{
	ZoomViewRange(-0.1f);
}


void FSequencer::ZoomOutViewRange()
{
	ZoomViewRange(0.1f);
}

bool GetMovieSceneSectionPlayRange(UMovieSceneSection* InSection, TRange<float>& OutBounds)
{
	if (InSection->IsInfinite())
	{
		TRange<float> KeyBounds = TRange<float>::Empty();
		 
		TSet<FKeyHandle> KeyHandles;
		InSection->GetKeyHandles(KeyHandles, TRange<float>());
		for (auto KeyHandle : KeyHandles)
		{
			TOptional<float> KeyTime = InSection->GetKeyTime(KeyHandle);
			if (KeyTime.IsSet())
			{
				if (KeyBounds.IsEmpty())
				{
					KeyBounds = TRange<float>(KeyTime.GetValue());
				}
				else
				{
					KeyBounds = TRange<float>( FMath::Min(KeyBounds.GetLowerBoundValue(), KeyTime.GetValue()),
												FMath::Max(KeyBounds.GetUpperBoundValue(), KeyTime.GetValue()) );
				}
			}
		}

		OutBounds = KeyBounds;
		return KeyHandles.Num() > 0;
	}
	else
	{
		OutBounds = InSection->GetRange();
		return true;
	}
}

void FSequencer::UpdatePlaybackRange()
{
	if (Settings->ShouldKeepPlayRangeInSectionBounds())
	{
		UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
		TArray<UMovieSceneSection*> AllSections = FocusedMovieScene->GetAllSections();

		if (AllSections.Num() > 0)
		{
			TRange<float> NewBounds = TRange<float>::Empty();

			TRange<float> OutBounds;
			if (GetMovieSceneSectionPlayRange(AllSections[0], OutBounds))
			{
				NewBounds = OutBounds;
			}
	
			for (auto MovieSceneSection : AllSections)
			{
				if (GetMovieSceneSectionPlayRange(MovieSceneSection, OutBounds))
				{
					if (NewBounds.IsEmpty())
					{
						NewBounds = OutBounds;
					}
					else
					{
						NewBounds = TRange<float>( FMath::Min(NewBounds.GetLowerBoundValue(), OutBounds.GetLowerBoundValue()),
												FMath::Max(NewBounds.GetUpperBoundValue(), OutBounds.GetUpperBoundValue()) );
					}
				}
			}

			// When the playback range is determined by the section bounds, don't mark the change in the playback range otherwise the scene will be marked dirty
			if (!NewBounds.IsDegenerate() && !FocusedMovieScene->IsPlaybackRangeLocked())
			{
				const bool bAlwaysMarkDirty = false;
				FocusedMovieScene->SetPlaybackRange(NewBounds.GetLowerBoundValue(), NewBounds.GetUpperBoundValue(), bAlwaysMarkDirty);
			}
		}
	}
	else
	{
		UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
		TRange<float> NewBounds = TRange<float>::Empty();

		for (auto MasterTrack : FocusedMovieScene->GetMasterTracks())
		{
			if (MasterTrack->AddsSectionBoundsToPlayRange())
			{
				NewBounds = TRange<float>( FMath::Min(NewBounds.GetLowerBoundValue(), MasterTrack->GetSectionBoundaries().GetLowerBoundValue()),
											FMath::Max(NewBounds.GetUpperBoundValue(), MasterTrack->GetSectionBoundaries().GetUpperBoundValue()) );
			}
		}
			
		if (!NewBounds.IsEmpty() && !FocusedMovieScene->IsPlaybackRangeLocked())
		{
			FocusedMovieScene->SetPlaybackRange(NewBounds.GetLowerBoundValue(), NewBounds.GetUpperBoundValue());
		}
	}
}


EAutoChangeMode FSequencer::GetAutoChangeMode() const 
{
	return Settings->GetAutoChangeMode();
}


void FSequencer::SetAutoChangeMode(EAutoChangeMode AutoChangeMode)
{
	Settings->SetAutoChangeMode(AutoChangeMode);
}


EAllowEditsMode FSequencer::GetAllowEditsMode() const 
{
	return Settings->GetAllowEditsMode();
}


void FSequencer::SetAllowEditsMode(EAllowEditsMode AllowEditsMode)
{
	Settings->SetAllowEditsMode(AllowEditsMode);
}


bool FSequencer::GetKeyAllEnabled() const 
{
	return Settings->GetKeyAllEnabled();
}


void FSequencer::SetKeyAllEnabled(bool bKeyAllEnabled) 
{
	Settings->SetKeyAllEnabled(bKeyAllEnabled);
}


bool FSequencer::GetKeyInterpPropertiesOnly() const 
{
	return Settings->GetKeyInterpPropertiesOnly();
}


void FSequencer::SetKeyInterpPropertiesOnly(bool bKeyInterpPropertiesOnly) 
{
	Settings->SetKeyInterpPropertiesOnly(bKeyInterpPropertiesOnly);
}


EMovieSceneKeyInterpolation FSequencer::GetKeyInterpolation() const
{
	return Settings->GetKeyInterpolation();
}


void FSequencer::SetKeyInterpolation(EMovieSceneKeyInterpolation InKeyInterpolation)
{
	Settings->SetKeyInterpolation(InKeyInterpolation);
}


bool FSequencer::GetInfiniteKeyAreas() const
{
	return Settings->GetInfiniteKeyAreas();
}


void FSequencer::SetInfiniteKeyAreas(bool bInfiniteKeyAreas)
{
	Settings->SetInfiniteKeyAreas(bInfiniteKeyAreas);
}


bool FSequencer::GetAutoSetTrackDefaults() const
{
	return Settings->GetAutoSetTrackDefaults();
}


bool FSequencer::IsRecordingLive() const 
{
	return PlaybackState == EMovieScenePlayerStatus::Recording && GIsPlayInEditorWorld;
}


float FSequencer::GetLocalTime() const
{
	return ScrubPosition * RootToLocalTransform;
}


float FSequencer::GetGlobalTime() const
{
	return ScrubPosition;
}

void FSequencer::SetLocalTime( float NewTime, ESnapTimeMode SnapTimeMode )
{
	// Ensure the time is in the current view
	ScrollIntoView(NewTime);

	// Perform snapping
	if ((SnapTimeMode & ESnapTimeMode::STM_Interval) && Settings->GetIsSnapEnabled())
	{
		NewTime = SequencerHelpers::SnapTimeToInterval(NewTime, GetFixedFrameInterval());
	}

	if ((SnapTimeMode & ESnapTimeMode::STM_Keys) && (Settings->GetSnapPlayTimeToKeys() || FSlateApplication::Get().GetModifierKeys().IsShiftDown()))
	{
		NewTime = OnGetNearestKey(NewTime);
	}

	SetLocalTimeDirectly(NewTime);
}


void FSequencer::SetLocalTimeDirectly(float NewTime)
{
	// Transform the time to the root time-space
	SetGlobalTime(NewTime * RootToLocalTransform.Inverse());
}


void FSequencer::SetGlobalTime( float NewTime )
{
	// Update the position
	ScrubPosition = NewTime;

	// Don't update the sequence if the time hasn't changed as this will cause duplicate events and the like to fire.
	// If we need to reevaluate the sequence at the same time for whetever reason, we should call ForceEvaluate()
	TOptional<float> LastPosition = PlayPosition.GetPreviousPosition();
	if (!LastPosition.IsSet() || LastPosition.GetValue() != ScrubPosition)
	{
		FMovieSceneEvaluationRange Range = PlayPosition.JumpTo(ScrubPosition, GetRootMovieSceneSequence()->GetMovieScene()->GetOptionalFixedFrameInterval());
		EvaluateInternal(Range);
	}
}

void FSequencer::ForceEvaluate()
{
	FMovieSceneEvaluationRange Range = PlayPosition.JumpTo(ScrubPosition, GetFocusedMovieSceneSequence()->GetMovieScene()->GetOptionalFixedFrameInterval());
	EvaluateInternal(Range);
}

void FSequencer::EvaluateInternal(FMovieSceneEvaluationRange InRange, bool bHasJumped)
{
	bNeedsEvaluate = false;

	if (PlaybackContextAttribute.IsBound())
	{
		CachedPlaybackContext = PlaybackContextAttribute.Get();
	}
	
	if (EventContextsAttribute.IsBound())
	{
		CachedEventContexts.Reset();
		for (UObject* Object : EventContextsAttribute.Get())
		{
			CachedEventContexts.Add(Object);
		}
	}

	FMovieSceneContext Context = FMovieSceneContext(InRange, PlaybackState).SetIsSilent(SilentModeCount != 0);
	Context.SetHasJumped(bHasJumped);

	FMovieSceneSequenceID RootOverride = MovieSceneSequenceID::Root;
	if (Settings->ShouldEvaluateSubSequencesInIsolation())
	{
		RootOverride = ActiveTemplateIDs.Top();
	}
	
	RootTemplateInstance.Evaluate(Context, *this, RootOverride);

	TemplateStore->PurgeStaleTracks();

	if (Settings->ShouldRerunConstructionScripts())
	{
		RerunConstructionScripts();
	}

	// If realtime is off, this needs to be called to update the pivot location when scrubbing.
	GUnrealEd->UpdatePivotLocationForSelection();

	if (!IsInSilentMode())
	{
		OnGlobalTimeChangedDelegate.Broadcast();
	}
}

void FSequencer::ScrollIntoView(float InLocalTime)
{
	if (IsAutoScrollEnabled())
	{
		float RangeOffset = CalculateAutoscrollEncroachment(InLocalTime).Get(0.f);
		
		// When not scrubbing, we auto scroll the view range immediately
		if (RangeOffset != 0.f)
		{
			TRange<float> WorkingRange = GetClampRange();

			// Adjust the offset so that the target range will be within the working range.
			if (TargetViewRange.GetLowerBoundValue() + RangeOffset < WorkingRange.GetLowerBoundValue())
			{
				RangeOffset = WorkingRange.GetLowerBoundValue() - TargetViewRange.GetLowerBoundValue();
			}
			else if (TargetViewRange.GetUpperBoundValue() + RangeOffset > WorkingRange.GetUpperBoundValue())
			{
				RangeOffset = WorkingRange.GetUpperBoundValue() - TargetViewRange.GetUpperBoundValue();
			}

			SetViewRange(TRange<float>(TargetViewRange.GetLowerBoundValue() + RangeOffset, TargetViewRange.GetUpperBoundValue() + RangeOffset), EViewRangeInterpolation::Immediate);
		}
	}
}

void FSequencer::UpdateAutoScroll(float NewTime)
{
	float ThresholdPercentage = 0.025f;
	AutoscrollOffset = CalculateAutoscrollEncroachment(NewTime, ThresholdPercentage);

	if (!AutoscrollOffset.IsSet())
	{
		AutoscrubOffset.Reset();
		return;
	}

	TRange<float> ViewRange = GetViewRange();
	const float Threshold = (ViewRange.GetUpperBoundValue() - ViewRange.GetLowerBoundValue()) * ThresholdPercentage;

	const float LocalPosition = GetLocalTime();

	// If we have no autoscrub offset yet, we move the scrub position to the boundary of the autoscroll threasdhold, then autoscrub from there
	if (!AutoscrubOffset.IsSet())
	{
		if (AutoscrollOffset.GetValue() < 0 && LocalPosition > ViewRange.GetLowerBoundValue() + Threshold)
		{
			SetLocalTimeDirectly( ViewRange.GetLowerBoundValue() + Threshold );
		}
		else if (AutoscrollOffset.GetValue() > 0 && LocalPosition < ViewRange.GetUpperBoundValue() - Threshold)
		{
			SetLocalTimeDirectly( ViewRange.GetUpperBoundValue() - Threshold );
		}
	}

	// Don't autoscrub if we're at the extremes of the movie scene range
	const TRange<float>& WorkingRange = GetFocusedMovieSceneSequence()->GetMovieScene()->GetEditorData().WorkingRange;
	if (NewTime < WorkingRange.GetLowerBoundValue() + Threshold ||
		NewTime > WorkingRange.GetUpperBoundValue() - Threshold
		)
	{
		AutoscrubOffset.Reset();
		return;
	}

	// Scrub at the same rate we scroll
	AutoscrubOffset = AutoscrollOffset;
}


TOptional<float> FSequencer::CalculateAutoscrollEncroachment(float NewTime, float ThresholdPercentage) const
{
	enum class EDirection { Positive, Negative };
	const EDirection Movement = NewTime - GetLocalTime() >= 0 ? EDirection::Positive : EDirection::Negative;

	const TRange<float> CurrentRange = GetViewRange();
	const float RangeMin = CurrentRange.GetLowerBoundValue(), RangeMax = CurrentRange.GetUpperBoundValue();
	const float AutoScrollThreshold = (RangeMax - RangeMin) * ThresholdPercentage;

	if (Movement == EDirection::Negative && NewTime < RangeMin + AutoScrollThreshold)
	{
		// Scrolling backwards in time, and have hit the threshold
		return NewTime - (RangeMin + AutoScrollThreshold);
	}
	
	if (Movement == EDirection::Positive && NewTime > RangeMax - AutoScrollThreshold)
	{
		// Scrolling forwards in time, and have hit the threshold
		return NewTime - (RangeMax - AutoScrollThreshold);
	}

	return TOptional<float>();
}


void FSequencer::SetPerspectiveViewportPossessionEnabled(bool bEnabled)
{
	bPerspectiveViewportPossessionEnabled = bEnabled;
}


void FSequencer::SetPerspectiveViewportCameraCutEnabled(bool bEnabled)
{
	bPerspectiveViewportCameraCutEnabled = bEnabled;
}

void FSequencer::RenderMovie(UMovieSceneSection* InSection) const
{
	RenderMovieInternal(InSection->GetStartTime(), InSection->GetEndTime(), true);
}

void FSequencer::RenderMovieInternal(float InStartTime, float InEndTime, bool bSetFrameOverrides) const
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

	// Create a new movie scene capture object for an automated level sequence, and open the tab
	UAutomatedLevelSequenceCapture* MovieSceneCapture = NewObject<UAutomatedLevelSequenceCapture>(GetTransientPackage(), UAutomatedLevelSequenceCapture::StaticClass(), NAME_None, RF_Transient);
	MovieSceneCapture->LoadFromConfig();

	MovieSceneCapture->SetLevelSequenceAsset(GetCurrentAsset()->GetPathName());

	if (CanShowFrameNumbers())
	{
		MovieSceneCapture->Settings.FrameRate = FMath::RoundToInt(1.f / GetFixedFrameInterval());
		MovieSceneCapture->Settings.ZeroPadFrameNumbers = Settings->GetZeroPadFrames();
		MovieSceneCapture->Settings.bUseRelativeFrameNumbers = false;

		const int32 StartFrame = FMath::RoundToInt( InStartTime * MovieSceneCapture->Settings.FrameRate );
		const int32 EndFrame = FMath::Max( StartFrame, FMath::RoundToInt( InEndTime * MovieSceneCapture->Settings.FrameRate ) );

		if (bSetFrameOverrides)
		{
			MovieSceneCapture->SetFrameOverrides(StartFrame, EndFrame);
		}
		else
		{
			if (!MovieSceneCapture->bUseCustomStartFrame)
			{
				MovieSceneCapture->StartFrame = StartFrame;
			}

			if (!MovieSceneCapture->bUseCustomEndFrame)
			{
				MovieSceneCapture->EndFrame = EndFrame;
			}
		}
	}

	IMovieSceneCaptureDialogModule::Get().OpenDialog(LevelEditorModule.GetLevelEditorTabManager().ToSharedRef(), MovieSceneCapture);
}

ISequencer::FOnActorAddedToSequencer& FSequencer::OnActorAddedToSequencer()
{
	return OnActorAddedToSequencerEvent;
}

ISequencer::FOnPreSave& FSequencer::OnPreSave()
{
	return OnPreSaveEvent;
}

ISequencer::FOnPostSave& FSequencer::OnPostSave()
{
	return OnPostSaveEvent;
}

ISequencer::FOnActivateSequence& FSequencer::OnActivateSequence()
{
	return OnActivateSequenceEvent;
}

ISequencer::FOnCameraCut& FSequencer::OnCameraCut()
{
	return OnCameraCutEvent;
}

TSharedRef<INumericTypeInterface<float>> FSequencer::GetNumericTypeInterface()
{
	return SequencerWidget->GetNumericTypeInterface();
}

TSharedRef<INumericTypeInterface<float>> FSequencer::GetZeroPadNumericTypeInterface()
{
	return SequencerWidget->GetZeroPadNumericTypeInterface();
}

TSharedRef<SWidget> FSequencer::MakeTimeRange(const TSharedRef<SWidget>& InnerContent, bool bShowWorkingRange, bool bShowViewRange, bool bShowPlaybackRange)
{
	return SequencerWidget->MakeTimeRange(InnerContent, bShowWorkingRange, bShowViewRange, bShowPlaybackRange);
}

/** Attempt to find an object binding ID that relates to an unspawned spawnable object */
FGuid FindUnspawnedObjectGuid(UObject& InObject, UMovieSceneSequence& Sequence)
{
	UMovieScene* MovieScene = Sequence.GetMovieScene();

	// If the object is an archetype, the it relates to an unspawned spawnable.
	UObject* ParentObject = Sequence.GetParentObject(&InObject);
	if (ParentObject && FMovieSceneSpawnable::IsSpawnableTemplate(*ParentObject))
	{
		FMovieSceneSpawnable* ParentSpawnable = MovieScene->FindSpawnable([&](FMovieSceneSpawnable& InSpawnable){
			return InSpawnable.GetObjectTemplate() == ParentObject;
		});

		if (ParentSpawnable)
		{
			UObject* ParentContext = ParentSpawnable->GetObjectTemplate();

			// The only way to find the object now is to resolve all the child bindings, and see if they are the same
			for (const FGuid& ChildGuid : ParentSpawnable->GetChildPossessables())
			{
				const bool bHasObject = Sequence.LocateBoundObjects(ChildGuid, ParentContext).Contains(&InObject);
				if (bHasObject)
				{
					return ChildGuid;
				}
			}
		}
	}
	else if (FMovieSceneSpawnable::IsSpawnableTemplate(InObject))
	{
		FMovieSceneSpawnable* SpawnableByArchetype = MovieScene->FindSpawnable([&](FMovieSceneSpawnable& InSpawnable){
			return InSpawnable.GetObjectTemplate() == &InObject;
		});

		if (SpawnableByArchetype)
		{
			return SpawnableByArchetype->GetGuid();
		}
	}

	return FGuid();
}

FGuid FSequencer::GetHandleToObject( UObject* Object, bool bCreateHandleIfMissing )
{
	if (Object == nullptr)
	{
		return FGuid();
	}

	UMovieSceneSequence* FocusedMovieSceneSequence = GetFocusedMovieSceneSequence();
	UMovieScene* FocusedMovieScene = FocusedMovieSceneSequence->GetMovieScene();
	
	// Attempt to resolve the object through the movie scene instance first, 
	FGuid ObjectGuid = FindObjectId(*Object, ActiveTemplateIDs.Top());

	if (ObjectGuid.IsValid())
	{
		// Check here for spawnable otherwise spawnables get recreated as possessables, which doesn't make sense
		FMovieSceneSpawnable* Spawnable = FocusedMovieScene->FindSpawnable(ObjectGuid);
		if (Spawnable)
		{
			return ObjectGuid;
		}

		// Make sure that the possessable is still valid, if it's not remove the binding so new one 
		// can be created.  This can happen due to undo.
		FMovieScenePossessable* Possessable = FocusedMovieScene->FindPossessable(ObjectGuid);
		if(Possessable == nullptr)
		{
			FocusedMovieSceneSequence->UnbindPossessableObjects(ObjectGuid);
			ObjectGuid.Invalidate();
		}
	}
	else
	{
		ObjectGuid = FindUnspawnedObjectGuid(*Object, *FocusedMovieSceneSequence);
	}

	if (ObjectGuid.IsValid() || IsReadOnly())
	{
		return ObjectGuid;
	}

	UObject* PlaybackContext = PlaybackContextAttribute.Get(nullptr);

	// If the object guid was not found attempt to add it
	// Note: Only possessed actors can be added like this
	if (FocusedMovieSceneSequence->CanPossessObject(*Object, PlaybackContext) && bCreateHandleIfMissing)
	{
		AActor* PossessedActor = Cast<AActor>(Object);

		ObjectGuid = CreateBinding(*Object, PossessedActor != nullptr ? PossessedActor->GetActorLabel() : Object->GetName());

		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	}
	
	return ObjectGuid;
}


ISequencerObjectChangeListener& FSequencer::GetObjectChangeListener()
{ 
	return *ObjectChangeListener;
}

void FSequencer::PossessPIEViewports(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut)
{
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		UWorld* World = WorldContext.World();
		if (!World || WorldContext.WorldType != EWorldType::PIE)
		{
			continue;
		}
		APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController();
		if (PC == nullptr)
		{
			continue;
		}

		TWeakObjectPtr<APlayerController> WeakPC = PC;
		auto FindViewTarget = [=](const FCachedViewTarget& In){ return In.PlayerController == WeakPC; };

		// skip same view target
		AActor* ViewTarget = PC->GetViewTarget();

		// save the last view target so that it can be restored when the camera object is null
		if (!PrePossessionViewTargets.ContainsByPredicate(FindViewTarget))
		{
			PrePossessionViewTargets.Add(FCachedViewTarget{ PC, ViewTarget });
		}

		UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromRuntimeObject(CameraObject);

		if (CameraObject == ViewTarget)
		{
			if ( bJumpCut )
			{
				if (PC->PlayerCameraManager)
				{
					PC->PlayerCameraManager->bGameCameraCutThisFrame = true;
				}

				if (CameraComponent)
				{
					CameraComponent->NotifyCameraCut();
				}
			}
			continue;
		}

		// skip unlocking if the current view target differs
		AActor* UnlockIfCameraActor = Cast<AActor>(UnlockIfCameraObject);

		// if unlockIfCameraActor is valid, release lock if currently locked to object
		if (CameraObject == nullptr && UnlockIfCameraActor != nullptr && UnlockIfCameraActor != ViewTarget)
		{
			return;
		}

		// override the player controller's view target
		AActor* CameraActor = Cast<AActor>(CameraObject);

		// if the camera object is null, use the last view target so that it is restored to the state before the sequence takes control
		if (CameraActor == nullptr)
		{
			if (const FCachedViewTarget* CachedTarget = PrePossessionViewTargets.FindByPredicate(FindViewTarget))
			{
				CameraActor = CachedTarget->ViewTarget.Get();
			}
		}

		FViewTargetTransitionParams TransitionParams;
		PC->SetViewTarget(CameraActor, TransitionParams);

		if (CameraComponent)
		{
			CameraComponent->NotifyCameraCut();
		}

		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->bClientSimulatingViewTarget = (CameraActor != nullptr);
			PC->PlayerCameraManager->bGameCameraCutThisFrame = true;
		}
	}
}

TSharedPtr<class ITimeSlider> FSequencer::GetTopTimeSliderWidget() const
{
	return SequencerWidget->GetTopTimeSliderWidget();
}

void FSequencer::UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut)
{
	OnCameraCutEvent.Broadcast(CameraObject, bJumpCut);

	if (!IsPerspectiveViewportCameraCutEnabled())
	{
		return;
	}

	if (Settings->ShouldAllowPossessionOfPIEViewports())
	{
		PossessPIEViewports(CameraObject, UnlockIfCameraObject, bJumpCut);
	}

	AActor* UnlockIfCameraActor = Cast<AActor>(UnlockIfCameraObject);

	for (FLevelEditorViewportClient* LevelVC : GEditor->LevelViewportClients)
	{
		if ((LevelVC == nullptr) || !LevelVC->IsPerspective() || !LevelVC->AllowsCinematicPreview())
		{
			continue;
		}

		if ((CameraObject != nullptr) || LevelVC->IsLockedToActor(UnlockIfCameraActor))
		{
			UpdatePreviewLevelViewportClientFromCameraCut(*LevelVC, CameraObject, bJumpCut);
		}
	}
}

void FSequencer::NotifyBindingsChanged()
{
	ISequencer::NotifyBindingsChanged();

	OnMovieSceneBindingsChangedDelegate.Broadcast();
}


void FSequencer::SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap)
{
	if (!IsPerspectiveViewportPossessionEnabled())
	{
		return;
	}

	for (FLevelEditorViewportClient* LevelVC : GEditor->LevelViewportClients)
	{
		if (LevelVC && LevelVC->IsPerspective())
		{
			if (LevelVC->AllowsCinematicPreview())
			{
				if (ViewportParamsMap.Contains(LevelVC))
				{
					const EMovieSceneViewportParams* ViewportParams = ViewportParamsMap.Find(LevelVC);
					if (ViewportParams->SetWhichViewportParam & EMovieSceneViewportParams::SVP_FadeAmount)
					{
						LevelVC->FadeAmount = ViewportParams->FadeAmount;
						LevelVC->bEnableFading = true;
					}
					if (ViewportParams->SetWhichViewportParam & EMovieSceneViewportParams::SVP_FadeColor)
					{
						LevelVC->FadeColor = ViewportParams->FadeColor.ToFColor(/*bSRGB=*/ true);
						LevelVC->bEnableFading = true;
					}
					if (ViewportParams->SetWhichViewportParam & EMovieSceneViewportParams::SVP_ColorScaling)
					{
						LevelVC->bEnableColorScaling = ViewportParams->bEnableColorScaling;
						LevelVC->ColorScale = ViewportParams->ColorScale;
					}
				}
			}
			else
			{
				LevelVC->bEnableFading = false;
				LevelVC->bEnableColorScaling = false;
			}
		}
	}
}


void FSequencer::GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const
{
	for (FLevelEditorViewportClient* LevelVC : GEditor->LevelViewportClients)
	{
		if (LevelVC && LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview())
		{
			EMovieSceneViewportParams ViewportParams;
			ViewportParams.FadeAmount = LevelVC->FadeAmount;
			ViewportParams.FadeColor = FLinearColor(LevelVC->FadeColor);
			ViewportParams.ColorScale = LevelVC->ColorScale;

			ViewportParamsMap.Add(LevelVC, ViewportParams);
		}
	}
}


EMovieScenePlayerStatus::Type FSequencer::GetPlaybackStatus() const
{
	return PlaybackState;
}


void FSequencer::SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus)
{
	PlaybackState = InPlaybackStatus;

	// Inform the renderer when Sequencer is in a 'paused' state for the sake of inter-frame effects
	const bool bIsPaused = (InPlaybackStatus == EMovieScenePlayerStatus::Stopped || InPlaybackStatus == EMovieScenePlayerStatus::Scrubbing || InPlaybackStatus == EMovieScenePlayerStatus::Stepping);

	for (FLevelEditorViewportClient* LevelVC : GEditor->LevelViewportClients)
	{
		if (LevelVC && LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview())
		{
			LevelVC->ViewState.GetReference()->SetSequencerState(bIsPaused);
		}
	}

	// backup or restore tick rate
	if (InPlaybackStatus == EMovieScenePlayerStatus::Playing)
	{
		OldMaxTickRate = GEngine->GetMaxFPS();
	}
	else
	{
		GEngine->SetMaxFPS(OldMaxTickRate);

		PlayRate = 1.f;
		ShuttleMultiplier = 0;
	}

	TimingManager->Update(PlaybackState, GetGlobalTime());
}


void FSequencer::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( Settings );

	if (UMovieSceneSequence* RootSequencePtr = RootSequence.Get())
	{
		Collector.AddReferencedObject( RootSequencePtr );
	}

	if (RootTemplateInstance.IsValid())
	{
		// Sequencer references all sub movie scene sequences contained within the root
		for (auto& Pair : RootTemplateInstance.GetSubInstances())
		{
			if (UMovieSceneSequence* Sequence = Pair.Value.Sequence.Get())
			{
				Collector.AddReferencedObject(Sequence);
			}
		}
	}
}


void FSequencer::ResetPerMovieSceneData()
{
	//@todo Sequencer - We may want to preserve selections when moving between movie scenes
	Selection.Empty();

	SequencerWidget->UpdateLayoutTree();

	UpdateTimeBoundsToFocusedMovieScene();
	UpdateRuntimeInstances();

	LabelManager.SetMovieScene( GetFocusedMovieSceneSequence()->GetMovieScene() );

	// @todo run through all tracks for new movie scene changes
	//  needed for audio track decompression
}


void FSequencer::UpdateRuntimeInstances()
{
	// If realtime is off, this needs to be called to update the pivot location when scrubbing.
	GUnrealEd->UpdatePivotLocationForSelection();
	
	// Redraw
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

void FSequencer::RecordSelectedActors()
{
	ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
	if (SequenceRecorder.IsRecording())
	{
		FNotificationInfo Info(LOCTEXT("UnableToRecord_AlreadyRecording", "Cannot start a new recording while one is already in progress."));
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	if (Settings->ShouldRewindOnRecord())
	{
		JumpToStart();
	}
	
	TArray<ACameraActor*> SelectedCameras;
	TArray<AActor*> EntireSelection;

	GEditor->GetSelectedActors()->GetSelectedObjects(SelectedCameras);
	GEditor->GetSelectedActors()->GetSelectedObjects(EntireSelection);

	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	// Figure out what we're recording into - a sub track, or a camera cut track, or a shot track
	UMovieSceneTrack* DestinationTrack = nullptr;
	if (SelectedCameras.Num())
	{
		DestinationTrack = MovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>();
		if (!DestinationTrack)
		{
			DestinationTrack = MovieScene->AddMasterTrack<UMovieSceneCinematicShotTrack>();
		}
	}
	else if (EntireSelection.Num())
	{
		DestinationTrack = MovieScene->FindMasterTrack<UMovieSceneSubTrack>();
		if (!DestinationTrack)
		{
			DestinationTrack = MovieScene->AddMasterTrack<UMovieSceneSubTrack>();
		}
	}
	else
	{
		FNotificationInfo Info(LOCTEXT("UnableToRecordNoSelection", "Unable to start recording because no actors are selected"));
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	if (!DestinationTrack)
	{
		FNotificationInfo Info(LOCTEXT("UnableToRecord", "Unable to start recording because a valid sub track could not be found or created"));
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	int32 MaxRow = -1;
	for (UMovieSceneSection* Section : DestinationTrack->GetAllSections())
	{
		MaxRow = FMath::Max(Section->GetRowIndex(), MaxRow);
	}
	// @todo: Get row at current time
	UMovieSceneSubSection* NewSection = CastChecked<UMovieSceneSubSection>(DestinationTrack->CreateNewSection());
	NewSection->SetRowIndex(MaxRow + 1);
	DestinationTrack->AddSection(*NewSection);
	NewSection->SetAsRecording(true);

	NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);

	if (UMovieSceneSubSection::IsSetAsRecording())
	{
		TArray<AActor*> ActorsToRecord;
		for (AActor* Actor : EntireSelection)
		{
			AActor* CounterpartActor = EditorUtilities::GetSimWorldCounterpartActor(Actor);
			ActorsToRecord.Add(CounterpartActor ? CounterpartActor : Actor);
		}

		const FString& PathToRecordTo = UMovieSceneSubSection::GetRecordingSection()->GetTargetPathToRecordTo();
		const FString& SequenceName = UMovieSceneSubSection::GetRecordingSection()->GetTargetSequenceName();
		SequenceRecorder.StartRecording(
			ActorsToRecord,
			FOnRecordingStarted::CreateSP(this, &FSequencer::HandleRecordingStarted),
			FOnRecordingFinished::CreateSP(this, &FSequencer::HandleRecordingFinished),
			PathToRecordTo,
			SequenceName);
	}
}

TSharedRef<SWidget> FSequencer::MakeTransportControls(bool bExtended)
{
	FEditorWidgetsModule& EditorWidgetsModule = FModuleManager::Get().LoadModuleChecked<FEditorWidgetsModule>( "EditorWidgets" );

	FTransportControlArgs TransportControlArgs;
	{
		TransportControlArgs.OnBackwardEnd.BindSP( this, &FSequencer::OnJumpToStart );
		TransportControlArgs.OnBackwardStep.BindSP( this, &FSequencer::OnStepBackward );
		TransportControlArgs.OnForwardPlay.BindSP( this, &FSequencer::OnPlay, true, 1.f );
		TransportControlArgs.OnBackwardPlay.BindSP( this, &FSequencer::OnPlay, true, -1.f );
		TransportControlArgs.OnForwardStep.BindSP( this, &FSequencer::OnStepForward );
		TransportControlArgs.OnForwardEnd.BindSP( this, &FSequencer::OnJumpToEnd );
		TransportControlArgs.OnGetPlaybackMode.BindSP( this, &FSequencer::GetPlaybackMode );
		TransportControlArgs.OnGetRecording.BindSP(this, &FSequencer::IsRecording);

		if(bExtended)
		{
			TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportSetPlaybackStart)));
		}
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::BackwardEnd));
		if(bExtended)
		{
			TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportJumpToPreviousKey)));
		}
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::BackwardStep));
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::BackwardPlay));
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::ForwardPlay));
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportRecord)));
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::ForwardStep));
		if(bExtended)
		{
			TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportJumpToNextKey)));
		}
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::ForwardEnd));
		if(bExtended)
		{
			TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportSetPlaybackEnd)));
		}
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportLoopMode)));
		TransportControlArgs.bAreButtonsFocusable = false;
	}

	return EditorWidgetsModule.CreateTransportControl( TransportControlArgs );
}

TSharedRef<SWidget> FSequencer::OnCreateTransportSetPlaybackStart()
{
	return SNew(SButton)
		.OnClicked(this, &FSequencer::SetPlaybackStart)
		.ToolTipText(LOCTEXT("SetPlayStart_Tooltip", "Set playback start to the current position"))
		.ButtonStyle(FEditorStyle::Get(), "Sequencer.Transport.SetPlayStart")
		.ContentPadding(2.0f);
}

TSharedRef<SWidget> FSequencer::OnCreateTransportJumpToPreviousKey()
{
	return SNew(SButton)
		.OnClicked(this, &FSequencer::JumpToPreviousKey)
		.ToolTipText(LOCTEXT("JumpToPreviousKey_Tooltip", "Jump to the previous key in the selected track(s)"))
		.ButtonStyle(FEditorStyle::Get(), "Sequencer.Transport.JumpToPreviousKey")
		.ContentPadding(2.0f);
}

TSharedRef<SWidget> FSequencer::OnCreateTransportJumpToNextKey()
{
	return SNew(SButton)
		.OnClicked(this, &FSequencer::JumpToNextKey)
		.ToolTipText(LOCTEXT("JumpToNextKey_Tooltip", "Jump to the next key in the selected track(s)"))
		.ButtonStyle(FEditorStyle::Get(), "Sequencer.Transport.JumpToNextKey")
		.ContentPadding(2.0f);
}

TSharedRef<SWidget> FSequencer::OnCreateTransportSetPlaybackEnd()
{
	return SNew(SButton)
		.OnClicked(this, &FSequencer::SetPlaybackEnd)
		.ToolTipText(LOCTEXT("SetPlayEnd_Tooltip", "Set playback end to the current position"))
		.ButtonStyle(FEditorStyle::Get(), "Sequencer.Transport.SetPlayEnd")
		.ContentPadding(2.0f);
}

TSharedRef<SWidget> FSequencer::OnCreateTransportLoopMode()
{
	TSharedRef<SButton> LoopButton = SNew(SButton)
		.OnClicked(this, &FSequencer::OnCycleLoopMode)
		.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
		.ToolTipText_Lambda([&]()
		{ 
			if (GetLoopMode() == ESequencerLoopMode::SLM_NoLoop)
			{
				return LOCTEXT("LoopModeNoLoop_Tooltip", "No looping");
			}
			else if (GetLoopMode() == ESequencerLoopMode::SLM_Loop)
			{
				return LOCTEXT("LoopModeLoop_Tooltip", "Loop playback range");
			}
			else
			{
				return LOCTEXT("LoopModeLoopSelectionRange_Tooltip", "Loop selection range");
			}
		})
		.ContentPadding(2.0f);

	TWeakPtr<SButton> WeakButton = LoopButton;

	LoopButton->SetContent(SNew(SImage)
		.Image_Lambda([&, WeakButton]()
		{
			if (GetLoopMode() == ESequencerLoopMode::SLM_NoLoop)
			{
				return WeakButton.IsValid() && WeakButton.Pin()->IsPressed() ? 
					&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Loop.Disabled").Pressed : 
					&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Loop.Disabled").Normal;
			}
			else if (GetLoopMode() == ESequencerLoopMode::SLM_Loop)
			{
				return WeakButton.IsValid() && WeakButton.Pin()->IsPressed() ? 
					&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Loop.Enabled").Pressed : 
					&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Loop.Enabled").Normal;
			}
			else
			{
				return WeakButton.IsValid() && WeakButton.Pin()->IsPressed() ? 
					&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Loop.SelectionRange").Pressed : 
					&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Loop.SelectionRange").Normal;
			}
		})
	);

	return LoopButton;
}

TSharedRef<SWidget> FSequencer::OnCreateTransportRecord()
{
	ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");

	TSharedRef<SButton> RecordButton = SNew(SButton)
		.OnClicked(this, &FSequencer::OnRecord)
		.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
		.ToolTipText_Lambda([&](){ return SequenceRecorder.IsRecording() ? LOCTEXT("StopRecord_Tooltip", "Stop recording current sub-track.") : LOCTEXT("Record_Tooltip", "Record the primed sequence sub-track."); })
		.Visibility(this, &FSequencer::GetRecordButtonVisibility)
		.ContentPadding(2.0f);

	TWeakPtr<SButton> WeakButton = RecordButton;

	RecordButton->SetContent(SNew(SImage)
		.Image_Lambda([&SequenceRecorder, WeakButton]()
		{
			if (SequenceRecorder.IsRecording())
			{
				return WeakButton.IsValid() && WeakButton.Pin()->IsPressed() ? 
					&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Recording").Pressed : 
					&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Recording").Normal;
			}

			return WeakButton.IsValid() && WeakButton.Pin()->IsPressed() ? 
				&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Record").Pressed : 
				&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Record").Normal;
		})
	);

	return RecordButton;
}


UObject* FSequencer::FindSpawnedObjectOrTemplate(const FGuid& BindingId)
{
	TArrayView<TWeakObjectPtr<>> Objects = FindObjectsInCurrentSequence(BindingId);
	if (Objects.Num())
	{
		return Objects[0].Get();
	}

	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	if (!Sequence)
	{
		return nullptr;
	}

	UMovieScene* FocusedMovieScene = Sequence->GetMovieScene();

	FMovieScenePossessable* Possessable = FocusedMovieScene->FindPossessable(BindingId);
	// If we're a possessable with a parent spawnable and we don't have the object, we look the object up within the default object of the spawnable
	if (Possessable && Possessable->GetParent().IsValid())
	{
		// If we're a spawnable and we don't have the object, use the default object to build up the track menu
		FMovieSceneSpawnable* ParentSpawnable = FocusedMovieScene->FindSpawnable(Possessable->GetParent());
		if (ParentSpawnable)
		{
			UObject* ParentObject = ParentSpawnable->GetObjectTemplate();
			if (ParentObject)
			{
				for (UObject* Obj : Sequence->LocateBoundObjects(BindingId, ParentObject))
				{
					return Obj;
				}
			}
		}
	}
	// If we're a spawnable and we don't have the object, use the default object to build up the track menu
	else if (FMovieSceneSpawnable* Spawnable = FocusedMovieScene->FindSpawnable(BindingId))
	{
		return Spawnable->GetObjectTemplate();
	}

	return nullptr;
}

FReply FSequencer::OnPlay(bool bTogglePlay, float InPlayRate)
{
	if( (PlaybackState == EMovieScenePlayerStatus::Playing ||
		 PlaybackState == EMovieScenePlayerStatus::Recording) && bTogglePlay && (FMath::Sign(InPlayRate) == FMath::Sign(PlayRate) ) )
	{
		Pause();
	}
	else
	{
		PlayRate = InPlayRate;

		SetPlaybackStatus(EMovieScenePlayerStatus::Playing);

		// Make sure Slate ticks during playback
		SequencerWidget->RegisterActiveTimerForPlayback();
	}

	return FReply::Handled();
}


EVisibility FSequencer::GetRecordButtonVisibility() const
{
	return UMovieSceneSubSection::IsSetAsRecording() ? EVisibility::Visible : EVisibility::Collapsed;
}


FReply FSequencer::OnRecord()
{
	ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");

	if(UMovieSceneSubSection::IsSetAsRecording() && !SequenceRecorder.IsRecording())
	{
		AActor* ActorToRecord = UMovieSceneSubSection::GetActorToRecord();
		if (ActorToRecord != nullptr)
		{
			AActor* OutActor = EditorUtilities::GetSimWorldCounterpartActor(ActorToRecord);
			if (OutActor != nullptr)
			{
				ActorToRecord = OutActor;
			}
		}

		const FString& PathToRecordTo = UMovieSceneSubSection::GetRecordingSection()->GetTargetPathToRecordTo();
		const FString& SequenceName = UMovieSceneSubSection::GetRecordingSection()->GetTargetSequenceName();
		SequenceRecorder.StartRecording(ActorToRecord, FOnRecordingStarted::CreateSP(this, &FSequencer::HandleRecordingStarted), FOnRecordingFinished::CreateSP(this, &FSequencer::HandleRecordingFinished), PathToRecordTo, SequenceName);
	}
	else if(SequenceRecorder.IsRecording())
	{
		SequenceRecorder.StopRecording();
	}

	return FReply::Handled();
}

void FSequencer::HandleRecordingStarted(UMovieSceneSequence* Sequence)
{
	OnPlay(false);
		
	// Make sure Slate ticks during playback
	SequencerWidget->RegisterActiveTimerForPlayback();

	// sync recording section to start
	UMovieSceneSubSection* Section = UMovieSceneSubSection::GetRecordingSection();
	if(Section != nullptr)
	{
		float LocalTime = GetLocalTime();

		Section->SetStartTime(LocalTime);
		Section->SetEndTime(LocalTime + GetFixedFrameInterval());
	}
}

void FSequencer::HandleRecordingFinished(UMovieSceneSequence* Sequence)
{
	// toggle us to no playing if we are still playing back
	// as the post processing takes such a long time we don't really care if the sequence doesnt carry on
	if(PlaybackState == EMovieScenePlayerStatus::Playing)
	{
		OnPlay(true);
	}

	// now patchup the section that was recorded to
	UMovieSceneSubSection* Section = UMovieSceneSubSection::GetRecordingSection();
	if(Section != nullptr)
	{
		Section->SetAsRecording(false);
		Section->SetSequence(Sequence);
		Section->SetEndTime(Section->GetStartTime() + Sequence->GetMovieScene()->GetPlaybackRange().Size<float>());
	
		if (Section->IsA<UMovieSceneCinematicShotSection>())
		{
			const FMovieSceneSpawnable* SpawnedCamera = Sequence->GetMovieScene()->FindSpawnable(
				[](FMovieSceneSpawnable& InSpawnable){
					return InSpawnable.GetObjectTemplate() && InSpawnable.GetObjectTemplate()->IsA<ACameraActor>();
				}
			);

			if (SpawnedCamera && !Sequence->GetMovieScene()->GetCameraCutTrack())
			{
				UMovieSceneTrack* CameraCutTrack = Sequence->GetMovieScene()->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass());
				UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(CameraCutTrack->CreateNewSection());
				CameraCutSection->SetCameraGuid(SpawnedCamera->GetGuid());
				CameraCutSection->SetRange(Sequence->GetMovieScene()->GetPlaybackRange());
				CameraCutTrack->AddSection(*CameraCutSection);
			}
		}
	}

	bNeedTreeRefresh = true;
	bNeedInstanceRefresh = true;
}

FReply FSequencer::OnStepForward()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
	float NewPosition = GetLocalTime() + GetFixedFrameInterval();
	SetLocalTime(NewPosition, ESnapTimeMode::STM_Interval);
	return FReply::Handled();
}


FReply FSequencer::OnStepBackward()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
	float NewPosition = GetLocalTime() - GetFixedFrameInterval();
	const bool bAllowSnappingToFrames = true;
	SetLocalTime(NewPosition, ESnapTimeMode::STM_Interval);
	return FReply::Handled();
}


FReply FSequencer::OnJumpToStart()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
	SetLocalTime(GetPlaybackRange().GetLowerBoundValue());
	return FReply::Handled();
}


FReply FSequencer::OnJumpToEnd()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
	SetLocalTime(GetPlaybackRange().GetUpperBoundValue());
	return FReply::Handled();
}


FReply FSequencer::OnCycleLoopMode()
{
	ESequencerLoopMode LoopMode = Settings->GetLoopMode();
	if (LoopMode == ESequencerLoopMode::SLM_NoLoop)
	{
		Settings->SetLoopMode(ESequencerLoopMode::SLM_Loop);
	}
	else if (LoopMode == ESequencerLoopMode::SLM_Loop && !GetSelectionRange().IsEmpty())
	{
		Settings->SetLoopMode(ESequencerLoopMode::SLM_LoopSelectionRange);
	}
	else if (LoopMode == ESequencerLoopMode::SLM_LoopSelectionRange || GetSelectionRange().IsEmpty())
	{
		Settings->SetLoopMode(ESequencerLoopMode::SLM_NoLoop);
	}
	return FReply::Handled();
}


FReply FSequencer::SetPlaybackEnd()
{
	const UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();
	if (FocusedSequence)
	{
		TRange<float> CurrentRange = FocusedSequence->GetMovieScene()->GetPlaybackRange();
		float NewPos = FMath::Max(GetLocalTime(), CurrentRange.GetLowerBoundValue());
		SetPlaybackRange(TRange<float>(CurrentRange.GetLowerBoundValue(), NewPos));
	}

	return FReply::Handled();
}

FReply FSequencer::SetPlaybackStart()
{
	const UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();
	if (FocusedSequence)
	{
		TRange<float> CurrentRange = FocusedSequence->GetMovieScene()->GetPlaybackRange();
		float NewPos = FMath::Min(GetLocalTime(), CurrentRange.GetUpperBoundValue());
		SetPlaybackRange(TRange<float>(NewPos, CurrentRange.GetUpperBoundValue()));	
	}

	return FReply::Handled();
}

FReply FSequencer::JumpToPreviousKey()
{
	GetKeysFromSelection(SelectedKeyCollection, SMALL_NUMBER);
	if (SelectedKeyCollection.IsValid())
	{
		TRange<float> FindRange(TRange<float>::BoundsType(), GetLocalTime());

		TOptional<float> NewTime = SelectedKeyCollection->FindFirstKeyInRange(FindRange, EFindKeyDirection::Backwards);
		if (NewTime.IsSet())
		{
			SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
			SetLocalTimeDirectly(NewTime.GetValue());
		}
	}

	return FReply::Handled();
}

FReply FSequencer::JumpToNextKey()
{
	GetKeysFromSelection(SelectedKeyCollection, SMALL_NUMBER);
	if (SelectedKeyCollection.IsValid())
	{
		TRange<float> FindRange(GetLocalTime(), TRange<float>::BoundsType());

		TOptional<float> NewTime = SelectedKeyCollection->FindFirstKeyInRange(FindRange, EFindKeyDirection::Forwards);
		if (NewTime.IsSet())
		{
			SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
			SetLocalTimeDirectly(NewTime.GetValue());
		}
	}

	return FReply::Handled();
}

ESequencerLoopMode FSequencer::GetLoopMode() const
{
	return Settings->GetLoopMode();
}


void FSequencer::SetLocalTimeLooped(float NewLocalTime)
{
	TOptional<EMovieScenePlayerStatus::Type> NewPlaybackStatus;

	float NewGlobalTime = NewLocalTime * RootToLocalTransform.Inverse();

	TRange<float> TimeBounds = GetTimeBounds();

	bool bHasJumped = false;
	bool bRestarted = false;
	if (GetLoopMode() == ESequencerLoopMode::SLM_Loop || GetLoopMode() == ESequencerLoopMode::SLM_LoopSelectionRange)
	{
		const UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();
		if (FocusedSequence)
		{
			if (NewLocalTime <= TimeBounds.GetLowerBoundValue() || NewLocalTime >= TimeBounds.GetUpperBoundValue())
			{
				NewGlobalTime = (PlayRate > 0 ? TimeBounds.GetLowerBoundValue() : TimeBounds.GetUpperBoundValue()) * RootToLocalTransform.Inverse();
				TimingManager->OnStartPlaying(NewGlobalTime);

				// Always evaluate from the start/end when looping
				PlayPosition.Reset(NewGlobalTime);

				bHasJumped = true;
			}
		}
	}
	else
	{
		TRange<float> WorkingRange = GetClampRange();

		bool bReachedEnd = false;
		if (PlayRate > 0)
		{
			bReachedEnd = GetLocalTime() < TimeBounds.GetUpperBoundValue() && NewLocalTime >= TimeBounds.GetUpperBoundValue();
		}
		else
		{
			bReachedEnd = GetLocalTime() > TimeBounds.GetLowerBoundValue() && NewLocalTime <= TimeBounds.GetLowerBoundValue();
		}

		// Stop if we hit the playback range end
		if (bReachedEnd)
		{
			NewGlobalTime = (PlayRate > 0 ? TimeBounds.GetUpperBoundValue() : TimeBounds.GetLowerBoundValue()) * RootToLocalTransform.Inverse();
			TimingManager->OnStartPlaying(NewGlobalTime);
			NewPlaybackStatus = EMovieScenePlayerStatus::Stopped;
		}
		// Constrain to the play range if necessary
		else if (Settings->ShouldKeepCursorInPlayRange())
		{
			// Clamp to bound or jump back if necessary
			if (NewLocalTime <= TimeBounds.GetLowerBoundValue() || 
				NewLocalTime >= TimeBounds.GetUpperBoundValue())
			{
				NewGlobalTime = (PlayRate > 0 ? TimeBounds.GetLowerBoundValue() : TimeBounds.GetUpperBoundValue()) * RootToLocalTransform.Inverse();
				TimingManager->OnStartPlaying(NewGlobalTime);

				// Always evaluate from the start/end when looping
				PlayPosition.Reset(NewGlobalTime);
			}
		}
		// Ensure the time is within the working range
		else if (!WorkingRange.Contains(NewLocalTime))
		{
			NewGlobalTime = FMath::Clamp(NewLocalTime, WorkingRange.GetLowerBoundValue(), WorkingRange.GetUpperBoundValue()) * RootToLocalTransform.Inverse();
			TimingManager->OnStartPlaying(NewGlobalTime);

			// Always evaluate from the start/end when looping
			PlayPosition.Reset(NewGlobalTime);
			NewPlaybackStatus = EMovieScenePlayerStatus::Stopped;
		}
	}

	// Ensure the time is in the current view
	ScrollIntoView(NewGlobalTime * RootToLocalTransform);

	// Update the position before fixing it to the time interval
	ScrubPosition = NewGlobalTime;

	// Evaluate the sequence
	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	FMovieSceneEvaluationRange EvalRange = PlayPosition.PlayTo(ScrubPosition, MovieScene->GetOptionalFixedFrameInterval());

	EvaluateInternal(EvalRange, bHasJumped);

	// Set the playback status if we need to
	if (NewPlaybackStatus.IsSet())
	{
		SetPlaybackStatus(NewPlaybackStatus.GetValue());
		// Evaluate the sequence with the new status
		EvaluateInternal(EvalRange);
	}
}


bool FSequencer::CanShowFrameNumbers() const
{
	return SequencerSnapValues::IsTimeSnapIntervalFrameRate(GetFixedFrameInterval());
}


EPlaybackMode::Type FSequencer::GetPlaybackMode() const
{
	if (PlaybackState == EMovieScenePlayerStatus::Playing)
	{
		if (PlayRate > 0)
		{
			return EPlaybackMode::PlayingForward;
		}
		else
		{
			return EPlaybackMode::PlayingReverse;
		}
	}
		
	return EPlaybackMode::Stopped;
}

bool FSequencer::IsRecording() const
{
	return PlaybackState == EMovieScenePlayerStatus::Recording;
}

void FSequencer::UpdateTimeBoundsToFocusedMovieScene()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	// Set the view range to:
	// 1. The moviescene view range
	// 2. The moviescene playback range
	// 3. Some sensible default
	TRange<float> NewRange = FocusedMovieScene->GetEditorData().ViewRange;

	if (NewRange.IsEmpty() || NewRange.IsDegenerate())
	{
		NewRange = FocusedMovieScene->GetPlaybackRange();
	}
	if (NewRange.IsEmpty() || NewRange.IsDegenerate())
	{
		NewRange = TRange<float>(0.f, 5.f);
	}

	// Set the view range to the new range
	SetViewRange(NewRange, EViewRangeInterpolation::Immediate);

	// Make sure the current time is within the bounds
	if (!TargetViewRange.Contains(GetLocalTime()))
	{
		SetLocalTimeDirectly(LastViewRange.GetLowerBoundValue());
		OnGlobalTimeChangedDelegate.Broadcast();
	}
}


TRange<float> FSequencer::GetTimeBounds() const
{
	const UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();

	// When recording, we never want to constrain the time bound range.  You might not even have any sections or keys yet
	// but we need to be able to move the time cursor during playback so you can capture data in real-time
	if( PlaybackState == EMovieScenePlayerStatus::Recording || !FocusedSequence)
	{
		return TRange<float>( -100000.0f, 100000.0f );
	}
	
	if (GetLoopMode() == ESequencerLoopMode::SLM_LoopSelectionRange)
	{
		if (!GetSelectionRange().IsEmpty())
		{
			return GetSelectionRange();
		}
	}

	if (Settings->ShouldEvaluateSubSequencesInIsolation() || ActiveTemplateIDs.Num() == 1)
	{
		return FocusedSequence->GetMovieScene()->GetPlaybackRange();
	}

	return SubSequenceRange;
}


void FSequencer::SetViewRange(TRange<float> NewViewRange, EViewRangeInterpolation Interpolation)
{
	if (!ensure(NewViewRange.HasUpperBound() && NewViewRange.HasLowerBound() && !NewViewRange.IsDegenerate()))
	{
		return;
	}

	const float AnimationLengthSeconds = Interpolation == EViewRangeInterpolation::Immediate ? 0.f : 0.1f;
	if (AnimationLengthSeconds != 0.f)
	{
		if (ZoomAnimation.GetCurve(0).DurationSeconds != AnimationLengthSeconds)
		{
			ZoomAnimation = FCurveSequence();
			ZoomCurve = ZoomAnimation.AddCurve(0.f, AnimationLengthSeconds, ECurveEaseFunction::QuadIn);
		}

		if (!ZoomAnimation.IsPlaying())
		{
			LastViewRange = TargetViewRange;
			ZoomAnimation.Play( SequencerWidget.ToSharedRef() );
		}
		TargetViewRange = NewViewRange;
	}
	else
	{
		TargetViewRange = LastViewRange = NewViewRange;
		ZoomAnimation.JumpToEnd();
	}


	UMovieSceneSequence* FocusedMovieSequence = GetFocusedMovieSceneSequence();
	if (FocusedMovieSequence != nullptr)
	{
		UMovieScene* FocusedMovieScene = FocusedMovieSequence->GetMovieScene();
		if (FocusedMovieScene != nullptr)
		{
			FocusedMovieScene->GetEditorData().ViewRange = TargetViewRange;

			// Always ensure the working range is big enough to fit the view range
			TRange<float>& WorkingRange = FocusedMovieScene->GetEditorData().WorkingRange;

			WorkingRange = TRange<float>(
				FMath::Min(TargetViewRange.GetLowerBoundValue(), WorkingRange.GetLowerBoundValue()),
				FMath::Max(TargetViewRange.GetUpperBoundValue(), WorkingRange.GetUpperBoundValue())
				);
		}
	}
}


void FSequencer::OnClampRangeChanged( TRange<float> NewClampRange )
{
	if (!NewClampRange.IsEmpty())
	{
		GetFocusedMovieSceneSequence()->GetMovieScene()->GetEditorData().WorkingRange = NewClampRange;
	}
}

float FSequencer::OnGetNearestKey(float InTime)
{
	float NearestKeyTime = InTime;
	GetKeysFromSelection(SelectedKeyCollection, SMALL_NUMBER);

	if (SelectedKeyCollection.IsValid())
	{
		TRange<float> FindRangeBackwards(TRange<float>::BoundsType(), NearestKeyTime);
		TOptional<float> NewTimeBackwards = SelectedKeyCollection->FindFirstKeyInRange(FindRangeBackwards, EFindKeyDirection::Backwards);

		TRange<float> FindRangeForwards(NearestKeyTime, TRange<float>::BoundsType());
		TOptional<float> NewTimeForwards = SelectedKeyCollection->FindFirstKeyInRange(FindRangeForwards, EFindKeyDirection::Forwards);
		if (NewTimeForwards.IsSet())
		{
			if (NewTimeBackwards.IsSet())
			{
				if (FMath::Abs(NewTimeForwards.GetValue() - NearestKeyTime) < FMath::Abs(NewTimeBackwards.GetValue() - NearestKeyTime))
				{
					NearestKeyTime = NewTimeForwards.GetValue();
				}
				else
				{
					NearestKeyTime = NewTimeBackwards.GetValue();
				}
			}
			else
			{
				NearestKeyTime = NewTimeForwards.GetValue();
			}
		}
		else if (NewTimeBackwards.IsSet())
		{
			NearestKeyTime = NewTimeBackwards.GetValue();
		}
	}
	return NearestKeyTime;
}

void FSequencer::OnScrubPositionChanged( float NewScrubPosition, bool bScrubbing )
{
	bool bClampToViewRange = true;

	if (PlaybackState == EMovieScenePlayerStatus::Scrubbing)
	{
		if (!bScrubbing)
		{
			OnEndScrubbing();
		}
		else if (IsAutoScrollEnabled())
		{
			// Clamp to the view range when not auto-scrolling
			bClampToViewRange = false;
	
			UpdateAutoScroll(NewScrubPosition);
			
			// When scrubbing, we animate auto-scrolled scrub position in Tick()
			if (AutoscrubOffset.IsSet())
			{
				return;
			}
		}
	}

	if (bClampToViewRange)
	{
		float LowerBound = TargetViewRange.GetLowerBoundValue();
		float UpperBound = TargetViewRange.GetUpperBoundValue();

		if (Settings->GetIsSnapEnabled() && Settings->GetSnapPlayTimeToInterval())
		{
			LowerBound = SequencerHelpers::SnapTimeToInterval(LowerBound, GetFixedFrameInterval());
			UpperBound = SequencerHelpers::SnapTimeToInterval(UpperBound, GetFixedFrameInterval());
		}

		NewScrubPosition = FMath::Clamp(NewScrubPosition, LowerBound, UpperBound);		
	}

	SetLocalTimeDirectly( NewScrubPosition );
}


void FSequencer::OnBeginScrubbing()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Scrubbing);
	SequencerWidget->RegisterActiveTimerForPlayback();

	OnBeginScrubbingDelegate.Broadcast();
}


void FSequencer::OnEndScrubbing()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
	AutoscrubOffset.Reset();
	StopAutoscroll();

	OnEndScrubbingDelegate.Broadcast();

	ForceEvaluate();
}


void FSequencer::OnPlaybackRangeBeginDrag()
{
	GEditor->BeginTransaction(LOCTEXT("SetPlaybackRange_Transaction", "Set Playback Range"));
}


void FSequencer::OnPlaybackRangeEndDrag()
{
	GEditor->EndTransaction();
}


void FSequencer::OnSelectionRangeBeginDrag()
{
	GEditor->BeginTransaction(LOCTEXT("SetSelectionRange_Transaction", "Set Selection Range"));
}


void FSequencer::OnSelectionRangeEndDrag()
{
	GEditor->EndTransaction();
}


void FSequencer::StartAutoscroll(float UnitsPerS)
{
	AutoscrollOffset = UnitsPerS;
}


void FSequencer::StopAutoscroll()
{
	AutoscrollOffset.Reset();
}


void FSequencer::OnToggleAutoScroll()
{
	Settings->SetAutoScrollEnabled(!Settings->GetAutoScrollEnabled());
}


bool FSequencer::IsAutoScrollEnabled() const
{
	return Settings->GetAutoScrollEnabled();
}


void FSequencer::FindInContentBrowser()
{
	if (GetFocusedMovieSceneSequence())
	{
		TArray<UObject*> ObjectsToFocus;
		ObjectsToFocus.Add(GetCurrentAsset());

		GEditor->SyncBrowserToObjects(ObjectsToFocus);
	}
}


UObject* FSequencer::GetCurrentAsset() const
{
	// For now we find the asset by looking at the root movie scene's outer.
	// @todo: this may need refining if/when we support editing movie scene instances
	return GetFocusedMovieSceneSequence()->GetMovieScene()->GetOuter();
}

bool FSequencer::IsReadOnly() const
{
	return bReadOnly;
}

void FSequencer::VerticalScroll(float ScrollAmountUnits)
{
	SequencerWidget->GetTreeView()->ScrollByDelta(ScrollAmountUnits);
}

FGuid FSequencer::AddSpawnable(UObject& Object)
{
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	if (!Sequence->AllowsSpawnableObjects())
	{
		return FGuid();
	}

	// Grab the MovieScene that is currently focused.  We'll add our Blueprint as an inner of the
	// MovieScene asset.
	UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

	TValueOrError<FNewSpawnable, FText> Result = SpawnRegister->CreateNewSpawnableType(Object, *OwnerMovieScene);
	if (!Result.IsValid())
	{
		FNotificationInfo Info(Result.GetError());
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return FGuid();
	}

	FNewSpawnable& NewSpawnable = Result.GetValue();

	auto DuplName = [&](FMovieSceneSpawnable& InSpawnable)
	{
		return InSpawnable.GetName() == NewSpawnable.Name;
	};

	int32 Index = 2;
	FString UniqueString;
	while (OwnerMovieScene->FindSpawnable(DuplName))
	{
		NewSpawnable.Name.RemoveFromEnd(UniqueString);
		UniqueString = FString::Printf(TEXT(" (%d)"), Index++);
		NewSpawnable.Name += UniqueString;
	}

	FGuid NewGuid = OwnerMovieScene->AddSpawnable(NewSpawnable.Name, *NewSpawnable.ObjectTemplate);

	ForceEvaluate();

	UpdateRuntimeInstances();
	
	return NewGuid;
}

FGuid FSequencer::MakeNewSpawnable( UObject& Object )
{
	// @todo sequencer: Undo doesn't seem to be working at all
	const FScopedTransaction Transaction( LOCTEXT("UndoAddingObject", "Add Object to MovieScene") );

	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = Sequence->GetMovieScene();

	FGuid NewGuid = AddSpawnable(Object);
	if (!NewGuid.IsValid())
	{
		return FGuid();
	}

	FMovieSceneSpawnable* Spawnable = GetFocusedMovieSceneSequence()->GetMovieScene()->FindSpawnable(NewGuid);
	if (!Spawnable)
	{
		return FGuid();
	}

	// Override spawn ownership during this process to ensure it never gets destroyed
	ESpawnOwnership SavedOwnership = Spawnable->GetSpawnOwnership();
	Spawnable->SetSpawnOwnership(ESpawnOwnership::External);

	// Spawn the object so we can position it correctly, it's going to get spawned anyway since things default to spawned.
	UObject* SpawnedObject = SpawnRegister->SpawnObject(NewGuid, *MovieScene, ActiveTemplateIDs.Top(), *this);

	FTransformData TransformData;
	SpawnRegister->SetupDefaultsForSpawnable(SpawnedObject, Spawnable->GetGuid(), TransformData, AsShared(), Settings);

	Spawnable->SetSpawnOwnership(SavedOwnership);

	return NewGuid;
}

void FSequencer::AddSubSequence(UMovieSceneSequence* Sequence)
{
	// @todo Sequencer - sub-moviescenes This should be moved to the sub-moviescene editor

	// Grab the MovieScene that is currently focused.  This is the movie scene that will contain the sub-moviescene
	UMovieScene* OwnerMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	// @todo sequencer: Undo doesn't seem to be working at all
	const FScopedTransaction Transaction( LOCTEXT("UndoAddingObject", "Add Object to MovieScene") );
	OwnerMovieScene->Modify();

	UMovieSceneSubTrack* SubTrack = OwnerMovieScene->AddMasterTrack<UMovieSceneSubTrack>();
	float Duration = Sequence->GetMovieScene()->GetPlaybackRange().Size<float>();
	SubTrack->AddSequence(Sequence, ScrubPosition, Duration);
}


bool FSequencer::OnHandleAssetDropped(UObject* DroppedAsset, const FGuid& TargetObjectGuid)
{
	bool bWasConsumed = false;
	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		bool bWasHandled = TrackEditors[i]->HandleAssetAdded(DroppedAsset, TargetObjectGuid);
		if (bWasHandled)
		{
			// @todo Sequencer - This will crash if multiple editors try to handle a single asset
			// Should we allow this? How should it consume then?
			// gmp 10/7/2015: the user should be presented with a dialog asking what kind of track they want to create
			check(!bWasConsumed);
			bWasConsumed = true;
		}
	}
	return bWasConsumed;
}


// Takes a display node and traverses it's parents to find the nearest track node if any.  Also collects the names of the nodes which make
// up the path from the track node to the display node being checked.  The name path includes the name of the node being checked, but not
// the name of the track node.
void GetParentTrackNodeAndNamePath(TSharedRef<const FSequencerDisplayNode> DisplayNode, TSharedPtr<FSequencerTrackNode>& OutParentTrack, TArray<FName>& OutNamePath )
{
	TArray<FName> PathToTrack;
	PathToTrack.Add( DisplayNode->GetNodeName() );
	TSharedPtr<FSequencerDisplayNode> CurrentParent = DisplayNode->GetParent();

	while ( CurrentParent.IsValid() && CurrentParent->GetType() != ESequencerNode::Track )
	{
		PathToTrack.Add( CurrentParent->GetNodeName() );
		CurrentParent = CurrentParent->GetParent();
	}

	if ( CurrentParent.IsValid() )
	{
		OutParentTrack = StaticCastSharedPtr<FSequencerTrackNode>( CurrentParent );
		for ( int32 i = PathToTrack.Num() - 1; i >= 0; i-- )
		{
			OutNamePath.Add( PathToTrack[i] );
		}
	}
}


bool FSequencer::OnRequestNodeDeleted( TSharedRef<const FSequencerDisplayNode> NodeToBeDeleted )
{
	bool bAnythingRemoved = false;
	
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

	if ( NodeToBeDeleted->GetType() == ESequencerNode::Folder )
	{
		// Delete Children
		for ( const TSharedRef<FSequencerDisplayNode>& ChildNode : NodeToBeDeleted->GetChildNodes() )
		{
			OnRequestNodeDeleted( ChildNode );
		}

		// Delete from parent, or root.
		TSharedRef<const FSequencerFolderNode> FolderToBeDeleted = StaticCastSharedRef<const FSequencerFolderNode>(NodeToBeDeleted);
		if ( NodeToBeDeleted->GetParent().IsValid() )
		{
			TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>( NodeToBeDeleted->GetParent() );
			ParentFolder->GetFolder().Modify();
			ParentFolder->GetFolder().RemoveChildFolder( &FolderToBeDeleted->GetFolder() );
		}
		else
		{
			UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
			FocusedMovieScene->Modify();
			FocusedMovieScene->GetRootFolders().Remove( &FolderToBeDeleted->GetFolder() );
		}

		bAnythingRemoved = true;
	}
	else if (NodeToBeDeleted->GetType() == ESequencerNode::Object)
	{
		// Delete any child object bindings
		for (const TSharedRef<FSequencerDisplayNode>& ChildNode : NodeToBeDeleted->GetChildNodes())
		{
			if (ChildNode->GetType() == ESequencerNode::Object)
			{
				OnRequestNodeDeleted(ChildNode);
			}
		}

		const FGuid& BindingToRemove = StaticCastSharedRef<const FSequencerObjectBindingNode>( NodeToBeDeleted )->GetObjectBinding();

		// Remove from a parent folder if necessary.
		if ( NodeToBeDeleted->GetParent().IsValid() && NodeToBeDeleted->GetParent()->GetType() == ESequencerNode::Folder )
		{
			TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>( NodeToBeDeleted->GetParent() );
			ParentFolder->GetFolder().Modify();
			ParentFolder->GetFolder().RemoveChildObjectBinding( BindingToRemove );
		}
		
		// Try to remove as a spawnable first
		if (OwnerMovieScene->RemoveSpawnable(BindingToRemove))
		{
			SpawnRegister->DestroySpawnedObject(BindingToRemove, ActiveTemplateIDs.Top(), *this);
		}
		// The guid should be associated with a possessable if it wasnt a spawnable
		else if (OwnerMovieScene->RemovePossessable(BindingToRemove))
		{
			Sequence->Modify();
			Sequence->UnbindPossessableObjects( BindingToRemove );
		}

		bAnythingRemoved = true;
	}
	else if( NodeToBeDeleted->GetType() == ESequencerNode::Track  )
	{
		TSharedRef<const FSequencerTrackNode> SectionAreaNode = StaticCastSharedRef<const FSequencerTrackNode>( NodeToBeDeleted );
		UMovieSceneTrack* Track = SectionAreaNode->GetTrack();

		// Remove from a parent folder if necessary.
		if ( NodeToBeDeleted->GetParent().IsValid() && NodeToBeDeleted->GetParent()->GetType() == ESequencerNode::Folder )
		{
			TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>( NodeToBeDeleted->GetParent() );
			ParentFolder->GetFolder().Modify();
			ParentFolder->GetFolder().RemoveChildMasterTrack( Track );
		}

		if (Track != nullptr)
		{
			// Remove sub tracks belonging to this row only
			if (SectionAreaNode->GetSubTrackMode() == FSequencerTrackNode::ESubTrackMode::SubTrack)
			{
				SectionAreaNode->GetTrack()->Modify();
				TSet<TWeakObjectPtr<UMovieSceneSection> > SectionsToDelete;
				for (TSharedRef<ISequencerSection> SectionToDelete : SectionAreaNode->GetSections())
				{
					UMovieSceneSection* Section = SectionToDelete->GetSectionObject();
					if (Section)
					{
						SectionsToDelete.Add(Section);
					}
				}
				DeleteSections(SectionsToDelete);
				SectionAreaNode->GetTrack()->FixRowIndices();
			}
			else
			{
				OwnerMovieScene->Modify();
				if (OwnerMovieScene->IsAMasterTrack(*Track))
				{
					OwnerMovieScene->RemoveMasterTrack(*Track);
				}
				else if (OwnerMovieScene->GetCameraCutTrack() == Track)
				{
					OwnerMovieScene->RemoveCameraCutTrack();
				}
				else
				{
					OwnerMovieScene->RemoveTrack(*Track);
				}
			}
		
			bAnythingRemoved = true;
		}
	}
	else if ( NodeToBeDeleted->GetType() == ESequencerNode::Category )
	{
		TSharedPtr<FSequencerTrackNode> ParentTrackNode;
		TArray<FName> PathFromTrack;
		GetParentTrackNodeAndNamePath(NodeToBeDeleted, ParentTrackNode, PathFromTrack);
		if ( ParentTrackNode.IsValid() )
		{
			for ( TSharedRef<ISequencerSection> Section : ParentTrackNode->GetSections() )
			{
				bAnythingRemoved |= Section->RequestDeleteCategory( PathFromTrack );
			}
		}
	}
	else if ( NodeToBeDeleted->GetType() == ESequencerNode::KeyArea )
	{
		TSharedPtr<FSequencerTrackNode> ParentTrackNode;
		TArray<FName> PathFromTrack;
		GetParentTrackNodeAndNamePath( NodeToBeDeleted, ParentTrackNode, PathFromTrack );
		if ( ParentTrackNode.IsValid() )
		{
			for ( TSharedRef<ISequencerSection> Section : ParentTrackNode->GetSections() )
			{
				bAnythingRemoved |= Section->RequestDeleteKeyArea( PathFromTrack );
			}
		}
	}

	return bAnythingRemoved;
}

void FSequencer::PostUndo(bool bSuccess)
{
	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::Unknown );
	SynchronizeSequencerSelectionWithExternalSelection();
	OnActivateSequenceEvent.Broadcast(ActiveTemplateIDs.Top());
}

void FSequencer::OnNewActorsDropped(const TArray<UObject*>& DroppedObjects, const TArray<AActor*>& DroppedActors)
{
	bool bAddSpawnable = FSlateApplication::Get().GetModifierKeys().IsShiftDown();
	bool bAddPossessable = FSlateApplication::Get().GetModifierKeys().IsControlDown();

	if (bAddSpawnable || bAddPossessable)
	{
		TArray<AActor*> SpawnedActors;

		const FScopedTransaction Transaction(LOCTEXT("UndoAddActors", "Add Actors to Sequencer"));
		
		UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
		UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

		Sequence->Modify();

		for ( AActor* Actor : DroppedActors )
		{
			AActor* NewActor = Actor;
			bool bCreateAndAttachCamera = false;
			if (NewActor->GetClass() == ACameraRig_Rail::StaticClass() ||
				NewActor->GetClass() == ACameraRig_Crane::StaticClass())
			{
				bCreateAndAttachCamera = true;
			}

			FGuid PossessableGuid = CreateBinding(*NewActor, NewActor->GetActorLabel());
			FGuid NewGuid = PossessableGuid;

			OnActorAddedToSequencerEvent.Broadcast(NewActor, PossessableGuid);

			if (bAddSpawnable)
			{
				FMovieSceneSpawnable* Spawnable = ConvertToSpawnableInternal(PossessableGuid);

				ForceEvaluate();

				for (TWeakObjectPtr<> WeakObject : FindBoundObjects(Spawnable->GetGuid(), ActiveTemplateIDs.Top()))
				{
					AActor* SpawnedActor = Cast<AActor>(WeakObject.Get());
					if (SpawnedActor)
					{
						SpawnedActors.Add(SpawnedActor);
						NewActor = SpawnedActor;
					}
				}

				NewGuid = Spawnable->GetGuid();
			}

			if (bCreateAndAttachCamera)
			{
				ACameraRig_Rail* RailActor = nullptr;
				if (Actor->GetClass() == ACameraRig_Rail::StaticClass())
				{
					RailActor = Cast<ACameraRig_Rail>(NewActor);
				}

				// Create a cine camera actor
				UWorld* PlaybackContext = Cast<UWorld>(GetPlaybackContext());
				ACineCameraActor* NewCamera = PlaybackContext->SpawnActor<ACineCameraActor>();
				FGuid NewCameraGuid = CreateBinding(*NewCamera, NewCamera->GetActorLabel());

				if (RailActor)
				{
					NewCamera->SetActorRotation(FRotator(0.f, -90.f, 0.f));
				}

				OnActorAddedToSequencerEvent.Broadcast(NewCamera, NewCameraGuid);

				if (bAddSpawnable)
				{
					FMovieSceneSpawnable* Spawnable = ConvertToSpawnableInternal(NewCameraGuid);

					ForceEvaluate();

					for (TWeakObjectPtr<> WeakObject : FindBoundObjects(Spawnable->GetGuid(), ActiveTemplateIDs.Top()))
					{
						NewCamera = Cast<ACineCameraActor>(WeakObject.Get());
						if (NewCamera)
						{
							break;
						}
					}

					NewCameraGuid = Spawnable->GetGuid();

					// Create an attach track
					UMovieScene3DAttachTrack* AttachTrack = Cast<UMovieScene3DAttachTrack>(OwnerMovieScene->AddTrack(UMovieScene3DAttachTrack::StaticClass(), NewCameraGuid));
					AttachTrack->AddConstraint(GetPlaybackRange().GetLowerBoundValue(), GetPlaybackRange().GetUpperBoundValue(), NAME_None, NAME_None, NewGuid);
				}
				else
				{
					// Parent it
					NewCamera->AttachToActor(NewActor, FAttachmentTransformRules::KeepRelativeTransform);
				}

				if (RailActor)
				{
					// Extend the rail a bit
					if (RailActor->GetRailSplineComponent()->GetNumberOfSplinePoints() == 2)
					{
						FVector SplinePoint1 = RailActor->GetRailSplineComponent()->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local);
						FVector SplinePoint2 = RailActor->GetRailSplineComponent()->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::Local);
						FVector SplineDirection = SplinePoint2 - SplinePoint1;
						SplineDirection.Normalize();

						float DefaultRailDistance = 650.f;
						SplinePoint2 = SplinePoint1 + SplineDirection* DefaultRailDistance;
						RailActor->GetRailSplineComponent()->SetLocationAtSplinePoint(1, SplinePoint2, ESplineCoordinateSpace::Local);
						RailActor->GetRailSplineComponent()->bSplineHasBeenEdited = true;
					}

					// Create a track for the CurrentPositionOnRail
					FPropertyPath PropertyPath;
					PropertyPath.AddProperty(FPropertyInfo(RailActor->GetClass()->FindPropertyByName(TEXT("CurrentPositionOnRail"))));

					FKeyPropertyParams KeyPropertyParams(TArrayBuilder<UObject*>().Add(RailActor), PropertyPath, ESequencerKeyMode::ManualKeyForced);

					float OriginalTime = GetLocalTime();

					SetLocalTimeDirectly(GetPlaybackRange().GetLowerBoundValue());
					RailActor->CurrentPositionOnRail = 0.f;
					KeyProperty(KeyPropertyParams);

					SetLocalTimeDirectly(GetPlaybackRange().GetUpperBoundValue());
					RailActor->CurrentPositionOnRail = 1.f;
					KeyProperty(KeyPropertyParams);

					SetLocalTimeDirectly(OriginalTime);
				}

				// New camera added, don't lock the view to the camera because we want to see where the camera rig was placed
				const bool bLockToCamera = false;
				NewCameraAdded(NewCamera, NewCameraGuid, bLockToCamera);
			}
		}

		if (SpawnedActors.Num())
		{
			const bool bNotifySelectionChanged = true;
			const bool bDeselectBSP = true;
			const bool bWarnAboutTooManyActors = false;
			const bool bSelectEvenIfHidden = false;
	
			GEditor->GetSelectedActors()->Modify();
			GEditor->GetSelectedActors()->BeginBatchSelectOperation();
			GEditor->SelectNone( bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors );
			for (auto SpawnedActor : SpawnedActors)
			{
				GEditor->SelectActor( SpawnedActor, true, bNotifySelectionChanged, bSelectEvenIfHidden );
			}
			GEditor->GetSelectedActors()->EndBatchSelectOperation();
			GEditor->NoteSelectionChange();
		}

		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );

		SynchronizeSequencerSelectionWithExternalSelection();
	}
}


void FSequencer::UpdatePreviewLevelViewportClientFromCameraCut(FLevelEditorViewportClient& InViewportClient, UObject* InCameraObject, bool bJumpCut) const
{
	AActor* CameraActor = Cast<AActor>(InCameraObject);

	bool bCameraHasBeenCut = bJumpCut;

	if (CameraActor)
	{
		bCameraHasBeenCut = bCameraHasBeenCut || !InViewportClient.IsLockedToActor(CameraActor);
		InViewportClient.SetViewLocation(CameraActor->GetActorLocation());
		InViewportClient.SetViewRotation(CameraActor->GetActorRotation());
	}
	else
	{
		InViewportClient.ViewFOV = InViewportClient.FOVAngle;
	}


	if (bCameraHasBeenCut)
	{
		InViewportClient.SetIsCameraCut();
	}


	// Set the actor lock.
	InViewportClient.SetMatineeActorLock(CameraActor);
	InViewportClient.bLockedCameraView = CameraActor != nullptr;
	InViewportClient.RemoveCameraRoll();

	UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromRuntimeObject(InCameraObject);
	if (CameraComponent)
	{
		if (bCameraHasBeenCut)
		{
			// tell the camera we cut
			CameraComponent->NotifyCameraCut();
		}

		// enforce aspect ratio.
		if (CameraComponent->AspectRatio == 0)
		{
			InViewportClient.AspectRatio = 1.7f;
		}
		else
		{
			InViewportClient.AspectRatio = CameraComponent->AspectRatio;
		}

		//don't stop the camera from zooming when not playing back
		InViewportClient.ViewFOV = CameraComponent->FieldOfView;

		// If there are selected actors, invalidate the viewports hit proxies, otherwise they won't be selectable afterwards
		if (InViewportClient.Viewport && GEditor->GetSelectedActorCount() > 0)
		{
			InViewportClient.Viewport->InvalidateHitProxy();
		}
	}

	// Update ControllingActorViewInfo, so it is in sync with the updated viewport
	InViewportClient.UpdateViewForLockedActor();
}


void GetDescendantMovieScenes(UMovieSceneSequence* InSequence, TArray<UMovieScene*> & InMovieScenes)
{
	UMovieScene* InMovieScene = InSequence->GetMovieScene();
	if (InMovieScene == nullptr || InMovieScenes.Contains(InMovieScene))
	{
		return;
	}

	InMovieScenes.Add(InMovieScene);

	for (auto Section : InMovieScene->GetAllSections())
	{
		UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
		if (SubSection != nullptr)
		{
			UMovieSceneSequence* SubSequence = SubSection->GetSequence();
			if (SubSequence != nullptr)
			{
				GetDescendantMovieScenes(SubSequence, InMovieScenes);
			}
		}
	}
}

void FSequencer::SetShowCurveEditor(bool bInShowCurveEditor)
{
	bShowCurveEditor = bInShowCurveEditor; 
	SequencerWidget->OnCurveEditorVisibilityChanged();
}

void FSequencer::SaveCurrentMovieScene()
{
	// Capture thumbnail
	// Convert UObject* array to FAssetData array
	TArray<FAssetData> AssetDataList;
	AssetDataList.Add(FAssetData(GetCurrentAsset()));

	FViewport* Viewport = GEditor->GetActiveViewport();

	// If there's no active viewport, find any other viewport that allows cinematic preview.
	if (Viewport == nullptr)
	{
		for (FLevelEditorViewportClient* LevelVC : GEditor->LevelViewportClients)
		{
			if ((LevelVC == nullptr) || !LevelVC->IsPerspective() || !LevelVC->AllowsCinematicPreview())
			{
				continue;
			}

			Viewport = LevelVC->Viewport;
		}
	}

	if ( ensure(GCurrentLevelEditingViewportClient) && Viewport != nullptr )
	{
		bool bIsInGameView = GCurrentLevelEditingViewportClient->IsInGameView();
		GCurrentLevelEditingViewportClient->SetGameView(true);

		//have to re-render the requested viewport
		FLevelEditorViewportClient* OldViewportClient = GCurrentLevelEditingViewportClient;
		//remove selection box around client during render
		GCurrentLevelEditingViewportClient = NULL;

		Viewport->Draw();

		IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
		ContentBrowser.CaptureThumbnailFromViewport(Viewport, AssetDataList);

		//redraw viewport to have the yellow highlight again
		GCurrentLevelEditingViewportClient = OldViewportClient;
		GCurrentLevelEditingViewportClient->SetGameView(bIsInGameView);
		Viewport->Draw();
	}

	OnPreSaveEvent.Broadcast(*this);

	TArray<UPackage*> PackagesToSave;
	TArray<UMovieScene*> MovieScenesToSave;
	GetDescendantMovieScenes(GetRootMovieSceneSequence(), MovieScenesToSave);
	for (auto MovieSceneToSave : MovieScenesToSave)
	{
		UPackage* MovieScenePackageToSave = MovieSceneToSave->GetOuter()->GetOutermost();
		if (MovieScenePackageToSave->IsDirty())
		{
			PackagesToSave.Add(MovieScenePackageToSave);
		}
	}

	// If there's more than 1 movie scene to save, prompt the user whether to save all dirty movie scenes.
	const bool bCheckDirty = PackagesToSave.Num() > 1;
	const bool bPromptToSave = PackagesToSave.Num() > 1;

	FEditorFileUtils::PromptForCheckoutAndSave( PackagesToSave, bCheckDirty, bPromptToSave );

	UpdateRuntimeInstances();
	FMovieSceneEvaluationRange Range = PlayPosition.JumpTo(ScrubPosition, GetFocusedMovieSceneSequence()->GetMovieScene()->GetOptionalFixedFrameInterval());
	EvaluateInternal(Range);

	OnPostSaveEvent.Broadcast(*this);
}


void FSequencer::SaveCurrentMovieSceneAs()
{
	TSharedPtr<IToolkitHost> MyToolkitHost = GetToolkitHost();

	if (!MyToolkitHost.IsValid())
	{
		return;
	}

	TArray<UObject*> AssetsToSave;
	AssetsToSave.Add(GetCurrentAsset());

	TArray<UObject*> SavedAssets;
	FEditorFileUtils::SaveAssetsAs(AssetsToSave, SavedAssets);

	if (SavedAssets.Num() == 0)
	{
		return;
	}

	if ((SavedAssets[0] != AssetsToSave[0]) && (SavedAssets[0] != nullptr))
	{
		FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();
		AssetEditorManager.CloseAllEditorsForAsset(AssetsToSave[0]);
		AssetEditorManager.OpenEditorForAssets(SavedAssets, EToolkitMode::Standalone, MyToolkitHost.ToSharedRef());
	}
}


TArray<FGuid> FSequencer::AddActors(const TArray<TWeakObjectPtr<AActor> >& InActors)
{
	// @todo sequencer: Undo doesn't seem to be working at all
	const FScopedTransaction Transaction(LOCTEXT("UndoPossessingObject", "Possess Object in Sequencer"));
	GetFocusedMovieSceneSequence()->Modify();

	TArray<FGuid> PossessableGuids;
	bool bPossessableAdded = false;
	for (TWeakObjectPtr<AActor> WeakActor : InActors)
	{
		if (AActor* Actor = WeakActor.Get())
		{
			FGuid ExistingGuid = FindObjectId(*Actor, ActiveTemplateIDs.Top());
			if (!ExistingGuid.IsValid())
			{
				FGuid PossessableGuid = CreateBinding(*Actor, Actor->GetActorLabel());
				PossessableGuids.Add(PossessableGuid);

				UpdateRuntimeInstances();

				OnActorAddedToSequencerEvent.Broadcast(Actor, PossessableGuid);
			}
			bPossessableAdded = true;
		}
	}

	if (bPossessableAdded)
	{
		SequencerWidget->UpdateLayoutTree();

		SynchronizeSequencerSelectionWithExternalSelection();
	}

	return PossessableGuids;
}


void FSequencer::OnSelectedOutlinerNodesChanged()
{
	SynchronizeExternalSelectionWithSequencerSelection();

	FSequencerEdMode* SequencerEdMode = (FSequencerEdMode*)(GLevelEditorModeTools().GetActiveMode(FSequencerEdMode::EM_SequencerMode));
	if (SequencerEdMode != nullptr)
	{
		AActor* NewlySelectedActor = GEditor->GetSelectedActors()->GetTop<AActor>();
		// If we selected an Actor or a node for an Actor that is a potential autokey candidate, clean up any existing mesh trails
		if (NewlySelectedActor && !NewlySelectedActor->IsEditorOnly())
		{
			SequencerEdMode->CleanUpMeshTrails();
		}
	}

	OnSelectionChangedObjectGuidsDelegate.Broadcast(Selection.GetBoundObjectsGuids());
	OnSelectionChangedTracksDelegate.Broadcast(Selection.GetSelectedTracks());
	TArray<UMovieSceneSection*> SelectedSections;
	for (TWeakObjectPtr<UMovieSceneSection> SelectedSectionPtr : Selection.GetSelectedSections())
	{
		if (SelectedSectionPtr.IsValid())
		{
			SelectedSections.Add(SelectedSectionPtr.Get());
		}
	}
	OnSelectionChangedSectionsDelegate.Broadcast(SelectedSections);
}


void FSequencer::SynchronizeExternalSelectionWithSequencerSelection()
{
	if ( bUpdatingSequencerSelection || !IsLevelEditorSequencer() )
	{
		return;
	}

	TGuardValue<bool> Guard(bUpdatingExternalSelection, true);

	TSet<AActor*> SelectedSequencerActors;
	TSet<USceneComponent*> SelectedSequencerComponents;

	TSet<TSharedRef<FSequencerDisplayNode> > DisplayNodes = Selection.GetNodesWithSelectedKeysOrSections();
	DisplayNodes.Append(Selection.GetSelectedOutlinerNodes());

	for ( TSharedRef<FSequencerDisplayNode> DisplayNode : DisplayNodes)
	{
		// Get the closest object binding node.
		TSharedPtr<FSequencerDisplayNode> CurrentNode = DisplayNode;
		TSharedPtr<FSequencerObjectBindingNode> ObjectBindingNode;
		while ( CurrentNode.IsValid() )
		{
			if ( CurrentNode->GetType() == ESequencerNode::Object )
			{
				ObjectBindingNode = StaticCastSharedPtr<FSequencerObjectBindingNode>(CurrentNode);
				break;
			}
			CurrentNode = CurrentNode->GetParent();
		}

		// If the closest node is an object node, try to get the actor/component nodes from it.
		if ( ObjectBindingNode.IsValid() )
		{
			for (auto RuntimeObject : FindBoundObjects(ObjectBindingNode->GetObjectBinding(), ActiveTemplateIDs.Top()) )
			{
				AActor* Actor = Cast<AActor>(RuntimeObject.Get());
				if ( Actor != nullptr )
				{
					SelectedSequencerActors.Add( Actor );
				}

				USceneComponent* SceneComponent = Cast<USceneComponent>(RuntimeObject.Get());
				if ( SceneComponent != nullptr )
				{
					SelectedSequencerComponents.Add( SceneComponent );

					Actor = SceneComponent->GetOwner();
					if ( Actor != nullptr )
					{
						SelectedSequencerActors.Add( Actor );
					}
				}
			}
		}
	}

	const bool bNotifySelectionChanged = false;
	const bool bDeselectBSP = true;
	const bool bWarnAboutTooManyActors = false;
	const bool bSelectEvenIfHidden = true;

	if (SelectedSequencerComponents.Num() + SelectedSequencerActors.Num() == 0)
	{
		if (GEditor->GetSelectedActorCount())
		{
			const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "UpdatingActorComponentSelectionNone", "Select None" ) );
			GEditor->SelectNone( bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors );
			GEditor->NoteSelectionChange();
		}
		return;
	}

	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "UpdatingActorComponentSelection", "Select Actors/Components" ) );


	GEditor->GetSelectedActors()->Modify();
	GEditor->GetSelectedActors()->BeginBatchSelectOperation();

	GEditor->SelectNone( bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors );

	for ( AActor* SelectedSequencerActor : SelectedSequencerActors )
	{
		GEditor->SelectActor( SelectedSequencerActor, true, bNotifySelectionChanged, bSelectEvenIfHidden );
	}

	GEditor->GetSelectedActors()->EndBatchSelectOperation();

	if (SelectedSequencerComponents.Num())
	{
		GEditor->GetSelectedComponents()->Modify();
		GEditor->GetSelectedComponents()->BeginBatchSelectOperation();

		for ( USceneComponent* SelectedSequencerComponent : SelectedSequencerComponents )
		{
			GEditor->SelectComponent( SelectedSequencerComponent, true, bNotifySelectionChanged, bSelectEvenIfHidden );
		}

		GEditor->GetSelectedComponents()->EndBatchSelectOperation();
	}
		
	GEditor->NoteSelectionChange();
}


void GetRootObjectBindingNodes(const TArray<TSharedRef<FSequencerDisplayNode>>& DisplayNodes, TArray<TSharedRef<FSequencerObjectBindingNode>>& RootObjectBindings )
{
	for ( TSharedRef<FSequencerDisplayNode> DisplayNode : DisplayNodes )
	{
		switch ( DisplayNode->GetType() )
		{
		case ESequencerNode::Folder:
			GetRootObjectBindingNodes( DisplayNode->GetChildNodes(), RootObjectBindings );
			break;
		case ESequencerNode::Object:
			RootObjectBindings.Add( StaticCastSharedRef<FSequencerObjectBindingNode>( DisplayNode ) );
			break;
		}
	}
}


void FSequencer::SynchronizeSequencerSelectionWithExternalSelection()
{
	if ( bUpdatingExternalSelection || !IsLevelEditorSequencer() )
	{
		return;
	}

	TGuardValue<bool> Guard(bUpdatingSequencerSelection, true);

	// If all nodes are already selected, do nothing. This ensures that when an undo event happens, 
	// nodes are not cleared and reselected, which can cause issues with the curve editor auto-fitting 
	// based on selection.
	bool bAllAlreadySelected = true;

	USelection* ActorSelection = GEditor->GetSelectedActors();
	
	// Get the selected sequencer keys for viewport interaction
	TArray<ASequencerKeyActor*> SelectedSequencerKeyActors;
	ActorSelection->GetSelectedObjects<ASequencerKeyActor>(SelectedSequencerKeyActors);

	TSet<TSharedRef<FSequencerDisplayNode>> NodesToSelect;
	for (auto ObjectBinding : NodeTree->GetObjectBindingMap() )
	{
		if (!ObjectBinding.Value.IsValid())
		{
			continue;
		}

		TSharedRef<FSequencerObjectBindingNode> ObjectBindingNode = ObjectBinding.Value.ToSharedRef();
		for ( TWeakObjectPtr<UObject> RuntimeObjectPtr : FindBoundObjects(ObjectBindingNode->GetObjectBinding(), ActiveTemplateIDs.Top()) )
		{
			UObject* RuntimeObject = RuntimeObjectPtr.Get();
			if ( RuntimeObject != nullptr)
			{
				for (ASequencerKeyActor* KeyActor : SelectedSequencerKeyActors)
				{
					if (KeyActor->IsEditorOnly())
					{
						AActor* TrailActor = KeyActor->GetAssociatedActor();
						if (TrailActor != nullptr && RuntimeObject == TrailActor)
						{
							NodesToSelect.Add(ObjectBindingNode);
							bAllAlreadySelected = false;
							break;
						}
					}
				}

				bool bActorSelected = ActorSelection->IsSelected( RuntimeObject );
				bool bComponentSelected = GEditor->GetSelectedComponents()->IsSelected( RuntimeObject);

				if (bActorSelected || bComponentSelected)
				{
					NodesToSelect.Add( ObjectBindingNode );

					if (bAllAlreadySelected)
					{
						bool bAlreadySelected = Selection.IsSelected(ObjectBindingNode);

						if (!bAlreadySelected)
						{
							TSet<TSharedRef<FSequencerDisplayNode> > DescendantNodes;
							SequencerHelpers::GetDescendantNodes(ObjectBindingNode, DescendantNodes);

							for (auto DescendantNode : DescendantNodes)
							{
								if (Selection.IsSelected(DescendantNode) || Selection.NodeHasSelectedKeysOrSections(DescendantNode))
								{
									bAlreadySelected = true;
									break;
								}
							}
						}

						if (!bAlreadySelected)
						{
							bAllAlreadySelected = false;
						}
					}
				}
				else if (Selection.IsSelected(ObjectBindingNode))
				{
					bAllAlreadySelected = false;
				}
			}
		}
	}

	if (!bAllAlreadySelected || NodesToSelect.Num() == 0)
	{
		Selection.SuspendBroadcast();
		Selection.EmptySelectedOutlinerNodes();
		for ( TSharedRef<FSequencerDisplayNode> NodeToSelect : NodesToSelect)
		{
			Selection.AddToSelection( NodeToSelect );
		}
		Selection.ResumeBroadcast();
		Selection.GetOnOutlinerNodeSelectionChanged().Broadcast();
	}
}


void FSequencer::ZoomToSelectedSections()
{
	TArray< TRange<float> > Bounds;
	for (TWeakObjectPtr<UMovieSceneSection> SelectedSection : Selection.GetSelectedSections())
	{
		Bounds.Add(SelectedSection->GetRange());
	}
	TRange<float> BoundsHull = TRange<float>::Hull(Bounds);

	if (BoundsHull.IsEmpty())
	{
		BoundsHull = GetTimeBounds();
	}

	if (!BoundsHull.IsEmpty() && !BoundsHull.IsDegenerate())
	{
		// Zoom back to last view range if already expanded
		if (!ViewRangeBeforeZoom.IsEmpty() &&
			FMath::IsNearlyEqual(BoundsHull.GetLowerBoundValue(), GetViewRange().GetLowerBoundValue(), KINDA_SMALL_NUMBER) &&
			FMath::IsNearlyEqual(BoundsHull.GetUpperBoundValue(), GetViewRange().GetUpperBoundValue(), KINDA_SMALL_NUMBER))
		{
			SetViewRange(ViewRangeBeforeZoom, EViewRangeInterpolation::Animated);
		}
		else
		{
			ViewRangeBeforeZoom = GetViewRange();

			SetViewRange(BoundsHull, EViewRangeInterpolation::Animated);
		}
	}
}


bool FSequencer::CanKeyProperty(FCanKeyPropertyParams CanKeyPropertyParams) const
{
	return ObjectChangeListener->CanKeyProperty(CanKeyPropertyParams);
} 


void FSequencer::KeyProperty(FKeyPropertyParams KeyPropertyParams) 
{
	ObjectChangeListener->KeyProperty(KeyPropertyParams);
}


FSequencerSelection& FSequencer::GetSelection()
{
	return Selection;
}


FSequencerSelectionPreview& FSequencer::GetSelectionPreview()
{
	return SelectionPreview;
}

void FSequencer::GetSelectedTracks(TArray<UMovieSceneTrack*>& OutSelectedTracks)
{
	OutSelectedTracks.Append(Selection.GetSelectedTracks());
}

void FSequencer::GetSelectedSections(TArray<UMovieSceneSection*>& OutSelectedSections)
{
	for (TWeakObjectPtr<UMovieSceneSection> SelectedSection : Selection.GetSelectedSections())
	{
		if (SelectedSection.IsValid())
		{
			OutSelectedSections.Add(SelectedSection.Get());
		}
	}
}

void FSequencer::SelectObject(FGuid ObjectBinding)
{
	const TSharedPtr<FSequencerObjectBindingNode>* Node = NodeTree->GetObjectBindingMap().Find(ObjectBinding);
	if (Node != nullptr && Node->IsValid())
	{
		GetSelection().Empty();
		GetSelection().AddToSelection(Node->ToSharedRef());
	}
}

void FSequencer::SelectTrack(UMovieSceneTrack* Track)
{
	for (TSharedRef<FSequencerDisplayNode> Node : NodeTree->GetAllNodes())
	{
		if (Node->GetType() == ESequencerNode::Track)
		{
			TSharedRef<FSequencerTrackNode> TrackNode = StaticCastSharedRef<FSequencerTrackNode>(Node);
			UMovieSceneTrack* TrackForNode = TrackNode->GetTrack();
			if (TrackForNode == Track)
			{
				Selection.AddToSelection(Node);
				break;
			}
		}
	}
}

void FSequencer::SelectSection(UMovieSceneSection* Section)
{
	Selection.AddToSelection(Section);
}

void FSequencer::SelectByPropertyPaths(const TArray<FString>& InPropertyPaths)
{
	TArray<TSharedRef<FSequencerDisplayNode>> NodesToSelect;
	for (const TSharedRef<FSequencerDisplayNode>& Node : NodeTree->GetAllNodes())
	{
		if (Node->GetType() == ESequencerNode::Track)
		{
			if (UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(StaticCastSharedRef<FSequencerTrackNode>(Node)->GetTrack()))
			{
				for (const FString& PropertyPath : InPropertyPaths)
				{
					if (PropertyTrack->GetPropertyPath() == PropertyPath)
					{
						NodesToSelect.Add(Node);
						break;
					}
				}
			}
		}
	}

	Selection.SuspendBroadcast();
	Selection.Empty();
	Selection.ResumeBroadcast();

	if (NodesToSelect.Num())
	{
		Selection.AddToSelection(NodesToSelect);
	}
}

void FSequencer::EmptySelection()
{
	Selection.Empty();
}

float FSequencer::GetOverlayFadeCurve() const
{
	return OverlayCurve.GetLerp();
}


void FSequencer::DeleteSelectedItems()
{
	if (Selection.GetSelectedKeys().Num())
	{
		FScopedTransaction DeleteKeysTransaction( NSLOCTEXT("Sequencer", "DeleteKeys_Transaction", "Delete Keys") );
		
		DeleteSelectedKeys();
	}
	else if (Selection.GetSelectedSections().Num())
	{
		FScopedTransaction DeleteSectionsTransaction( NSLOCTEXT("Sequencer", "DeleteSections_Transaction", "Delete Sections") );
	
		DeleteSections(Selection.GetSelectedSections());
	}
	else if (Selection.GetSelectedOutlinerNodes().Num())
	{
		DeleteSelectedNodes();
	}
}


void FSequencer::AssignActor(FMenuBuilder& MenuBuilder, FGuid InObjectBinding)
{
	TSet<const AActor*> BoundObjects;
	{
		for (TWeakObjectPtr<> Ptr : FindObjectsInCurrentSequence(InObjectBinding))
		{
			if (const AActor* Actor = Cast<AActor>(Ptr.Get()))
			{
				BoundObjects.Add(Actor);
			}
		}
	}

	auto IsActorValidForAssignment = [BoundObjects](const AActor* InActor){
		return !BoundObjects.Contains(InActor);
	};

	using namespace SceneOutliner;

	// Set up a menu entry to assign an actor to the object binding node
	FInitializationOptions InitOptions;
	{
		InitOptions.Mode = ESceneOutlinerMode::ActorPicker;

		// We hide the header row to keep the UI compact.
		InitOptions.bShowHeaderRow = false;
		InitOptions.bShowSearchBox = true;
		InitOptions.bShowCreateNewFolder = false;
		InitOptions.bFocusSearchBoxWhenOpened = true;
		// Only want the actor label column
		InitOptions.ColumnMap.Add(FBuiltInColumnTypes::Label(), FColumnInfo(EColumnVisibility::Visible, 0));

		// Only display actors that are not possessed already
		InitOptions.Filters->AddFilterPredicate( FActorFilterPredicate::CreateLambda( IsActorValidForAssignment ) );
	}

	// actor selector to allow the user to choose an actor
	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
	TSharedRef< SWidget > MiniSceneOutliner =
		SNew( SBox )
		.MaxDesiredHeight(400.0f)
		.WidthOverride(300.0f)
		[
			SceneOutlinerModule.CreateSceneOutliner(
				InitOptions,
				FOnActorPicked::CreateLambda([=](AActor* Actor){
					// Create a new binding for this actor
					FSlateApplication::Get().DismissAllMenus();
					DoAssignActor(&Actor, 1, InObjectBinding);
				})
			)
		];

	MenuBuilder.AddWidget(MiniSceneOutliner, FText::GetEmpty(), true);
	MenuBuilder.EndSection();
}


FGuid FSequencer::DoAssignActor(AActor*const* InActors, int32 NumActors, FGuid InObjectBinding)
{
	if (NumActors <= 0)
	{
		return FGuid();
	}

	//@todo: this code doesn't work with multiple actors, or when the existing binding is bound to multiple actors

	AActor* Actor = InActors[0];

	if (Actor == nullptr)
	{
		return FGuid();
	}

	FScopedTransaction AssignActor( NSLOCTEXT("Sequencer", "AssignActor", "Assign Actor") );

	UMovieSceneSequence* OwnerSequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();

	Actor->Modify();
	OwnerSequence->Modify();
	OwnerMovieScene->Modify();

	TArrayView<TWeakObjectPtr<>> RuntimeObjects = FindObjectsInCurrentSequence(InObjectBinding);

	UObject* RuntimeObject = RuntimeObjects.Num() ? RuntimeObjects[0].Get() : nullptr;

	// Replace the object itself
	FMovieScenePossessable NewPossessableActor;
	FGuid NewGuid;
	{
		// Get the object guid to assign, remove the binding if it already exists
		FGuid ParentGuid = FindObjectId(*Actor, ActiveTemplateIDs.Top());
		FString NewActorLabel = Actor->GetActorLabel();
		if (ParentGuid.IsValid())
		{
			OwnerMovieScene->RemovePossessable(ParentGuid);
			OwnerSequence->UnbindPossessableObjects(ParentGuid);
		}

		// Add this object
		NewPossessableActor = FMovieScenePossessable( NewActorLabel, Actor->GetClass());
		NewGuid = NewPossessableActor.GetGuid();
		OwnerSequence->BindPossessableObject(NewPossessableActor.GetGuid(), *Actor, GetPlaybackContext());

		// Defer replacing this object until the components have been updated
	}

	auto UpdateComponent = [&]( FGuid OldComponentGuid, UActorComponent* NewComponent )
	{
		// Get the object guid to assign, remove the binding if it already exists
		FGuid NewComponentGuid = FindObjectId( *NewComponent, ActiveTemplateIDs.Top() );
		if ( NewComponentGuid.IsValid() )
		{
			OwnerMovieScene->RemovePossessable( NewComponentGuid );
			OwnerSequence->UnbindPossessableObjects( NewComponentGuid );
		}

		// Add this object
		FMovieScenePossessable NewPossessable( NewComponent->GetName(), NewComponent->GetClass() );
		OwnerSequence->BindPossessableObject( NewPossessable.GetGuid(), *NewComponent, Actor );

		// Replace
		OwnerMovieScene->ReplacePossessable( OldComponentGuid, NewPossessable );
		State.Invalidate(OldComponentGuid, ActiveTemplateIDs.Top());

		FMovieScenePossessable* ThisPossessable = OwnerMovieScene->FindPossessable( NewPossessable.GetGuid() );
		if ( ensure( ThisPossessable ) )
		{
			ThisPossessable->SetParent( NewGuid );
		}
	};

	// Handle components
	AActor* ActorToReplace = Cast<AActor>(RuntimeObject);
	if (ActorToReplace != nullptr && ActorToReplace->IsActorBeingDestroyed() == false)
	{
		for (UActorComponent* ComponentToReplace : ActorToReplace->GetComponents())
		{
			if (ComponentToReplace != nullptr)
			{
				FGuid ComponentGuid = FindObjectId(*ComponentToReplace, ActiveTemplateIDs.Top());
				if (ComponentGuid.IsValid())
				{
					for (UActorComponent* NewComponent : Actor->GetComponents())
					{
						if (NewComponent->GetFullName(Actor) == ComponentToReplace->GetFullName(ActorToReplace))
						{
							UpdateComponent( ComponentGuid, NewComponent );
						}
					}
				}
			}
		}
	}
	else // If the actor didn't exist, try to find components who's parent guids were the previous actors guid.
	{
		TMap<FString, UActorComponent*> ComponentNameToComponent;
		for ( UActorComponent* Component : Actor->GetComponents() )
		{
			ComponentNameToComponent.Add( Component->GetName(), Component );
		}
		for ( int32 i = 0; i < OwnerMovieScene->GetPossessableCount(); i++ )
		{
			FMovieScenePossessable& OldPossessable = OwnerMovieScene->GetPossessable(i);
			if ( OldPossessable.GetParent() == InObjectBinding )
			{
				UActorComponent** ComponentPtr = ComponentNameToComponent.Find( OldPossessable.GetName() );
				if ( ComponentPtr != nullptr )
				{
					UpdateComponent( OldPossessable.GetGuid(), *ComponentPtr );
				}
			}
		}
	}

	// Replace the actor itself after components have been updated
	OwnerMovieScene->ReplacePossessable(InObjectBinding, NewPossessableActor);

	State.Invalidate(InObjectBinding, ActiveTemplateIDs.Top());

	// Try to fix up folders
	TArray<UMovieSceneFolder*> FoldersToCheck;
	FoldersToCheck.Append(GetFocusedMovieSceneSequence()->GetMovieScene()->GetRootFolders());
	bool bFolderFound = false;
	while ( FoldersToCheck.Num() > 0 && bFolderFound == false )
	{
		UMovieSceneFolder* Folder = FoldersToCheck[0];
		FoldersToCheck.RemoveAt(0);
		if ( Folder->GetChildObjectBindings().Contains( InObjectBinding ) )
		{
			Folder->Modify();
			Folder->RemoveChildObjectBinding( InObjectBinding );
			Folder->AddChildObjectBinding( NewGuid );
			bFolderFound = true;
		}

		for ( UMovieSceneFolder* ChildFolder : Folder->GetChildFolders() )
		{
			FoldersToCheck.Add( ChildFolder );
		}
	}

	RestorePreAnimatedState();

	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );

	return NewGuid;
}

void FSequencer::DeleteNode(TSharedRef<FSequencerDisplayNode> NodeToBeDeleted)
{
	// If this node is selected, delete all selected nodes
	if (GetSelection().IsSelected(NodeToBeDeleted))
	{
		DeleteSelectedNodes();
	}
	else
	{
		const FScopedTransaction Transaction( NSLOCTEXT("Sequencer", "UndoDeletingObject", "Delete Node") );
		bool bAnythingDeleted = OnRequestNodeDeleted(NodeToBeDeleted);
		if ( bAnythingDeleted )
		{
			NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemRemoved );
		}
	}
}


void FSequencer::DeleteSelectedNodes()
{
	TSet< TSharedRef<FSequencerDisplayNode> > SelectedNodesCopy = GetSelection().GetSelectedOutlinerNodes();

	if (SelectedNodesCopy.Num() == 0)
	{
		return;
	}

	const FScopedTransaction Transaction( NSLOCTEXT("Sequencer", "UndoDeletingObject", "Delete Node") );

	bool bAnythingDeleted = false;

	for( const TSharedRef<FSequencerDisplayNode>& SelectedNode : SelectedNodesCopy )
	{
		if( !SelectedNode->IsHidden() )
		{
			// Delete everything in the entire node
			TSharedRef<const FSequencerDisplayNode> NodeToBeDeleted = StaticCastSharedRef<const FSequencerDisplayNode>(SelectedNode);
			bAnythingDeleted |= OnRequestNodeDeleted( NodeToBeDeleted );
		}
	}

	if ( bAnythingDeleted )
	{
		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemRemoved );
	}
}


void FSequencer::CopySelectedTracks(TArray<TSharedPtr<FSequencerTrackNode>>& TrackNodes)
{
	TArray<UMovieSceneTrack*> TracksToCopy;
	for (TSharedPtr<FSequencerTrackNode> TrackNode : TrackNodes)
	{
		TracksToCopy.Add(TrackNode->GetTrack());
	}

	FString ExportedText;
	FSequencer::ExportTracksToText(TracksToCopy, /*out*/ ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}


void FSequencer::ExportTracksToText(TArray<UMovieSceneTrack*> TracksToExport, FString& ExportedText)
{
	// Clear the mark state for saving.
	UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));

	FStringOutputDevice Archive;
	const FExportObjectInnerContext Context;

	// Export each of the selected nodes
	UObject* LastOuter = nullptr;

	for (UMovieSceneTrack* TrackToExport : TracksToExport)
	{
		// The nodes should all be from the same scope
		UObject* ThisOuter = TrackToExport->GetOuter();
		check((LastOuter == ThisOuter) || (LastOuter == nullptr));
		LastOuter = ThisOuter;

		UExporter::ExportToOutputDevice(&Context, TrackToExport, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, ThisOuter);
	}

	ExportedText = Archive;
}

void FSequencer::PasteCopiedTracks()
{
	TSet<TSharedRef<FSequencerDisplayNode>> SelectedNodes = Selection.GetSelectedOutlinerNodes();

	TArray<TSharedPtr<FSequencerObjectBindingNode>> ObjectNodes;
	for (TSharedRef<FSequencerDisplayNode> Node : SelectedNodes)
	{
		if (Node->GetType() != ESequencerNode::Object)
		{
			continue;
		}

		TSharedPtr<FSequencerObjectBindingNode> ObjectNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);
		if (ObjectNode.IsValid())
		{
			ObjectNodes.Add(ObjectNode);
		}
	}

	FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());
	// Grab the text to paste from the clipboard
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	TArray<UMovieSceneTrack*> ImportedTracks;
	FSequencer::ImportTracksFromText(TextToImport, ImportedTracks);

	if (ImportedTracks.Num() == 0)
	{
		Transaction.Cancel();
		return;
	}
	
	if (ObjectNodes.Num())
	{
		for (TSharedPtr<FSequencerObjectBindingNode> ObjectNode : ObjectNodes)
		{
			FGuid ObjectGuid = ObjectNode->GetObjectBinding();

			TArray<UMovieSceneTrack*> NewTracks;
			FSequencer::ImportTracksFromText(TextToImport, NewTracks);

			for (UMovieSceneTrack* NewTrack : NewTracks)
			{
				if (!GetFocusedMovieSceneSequence()->GetMovieScene()->AddGivenTrack(NewTrack, ObjectGuid))
				{
					FNotificationInfo Info(LOCTEXT("TrackAlreadyBound", "Can't Paste: Binding doesn't exist"));
					Info.FadeInDuration = 0.1f;
					Info.FadeOutDuration = 0.5f;
					Info.ExpireDuration = 2.5f;
					auto NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);

					NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
					NotificationItem->ExpireAndFadeout();

					continue;
				}
				else
				{
					NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
				}
			}
		}

		return;
	}

	// Add as master track or set camera cut track
	for (UMovieSceneTrack* NewTrack : ImportedTracks)
	{
		if (NewTrack->IsA(UMovieSceneCameraCutTrack::StaticClass()))
		{
			GetFocusedMovieSceneSequence()->GetMovieScene()->SetCameraCutTrack(NewTrack);
			NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
		}
		else
		{
			if (GetFocusedMovieSceneSequence()->GetMovieScene()->AddGivenMasterTrack(NewTrack))
			{
				NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
			}
		}
	}
}


class FTrackObjectTextFactory : public FCustomizableTextObjectFactory
{
public:
	FTrackObjectTextFactory()
		: FCustomizableTextObjectFactory(GWarn)
	{
	}

	// FCustomizableTextObjectFactory implementation
	virtual bool CanCreateClass(UClass* InObjectClass, bool& bOmitSubObjs) const override
	{
		if (InObjectClass->IsChildOf(UMovieSceneTrack::StaticClass()))
		{
			return true;
		}
		return false;
	}
	

	virtual void ProcessConstructedObject(UObject* NewObject) override
	{
		check(NewObject);

		NewTracks.Add(Cast<UMovieSceneTrack>(NewObject));
	}

public:
	TArray<UMovieSceneTrack*> NewTracks;
};

bool FSequencer::CanPaste(const FString& TextToImport) const
{
	FTrackObjectTextFactory Factory;
	if (!Factory.CanCreateObjectsFromText(TextToImport))
	{
		return false;
	}

	return true;
}

void FSequencer::ImportTracksFromText(const FString& TextToImport, /*out*/ TArray<UMovieSceneTrack*>& ImportedTracks)
{
	UPackage* TempPackage = NewObject<UPackage>(nullptr, TEXT("/Engine/Sequencer/Editor/Transient"), RF_Transient);
	TempPackage->AddToRoot();

	// Turn the text buffer into objects
	FTrackObjectTextFactory Factory;
	Factory.ProcessBuffer(TempPackage, RF_Transactional, TextToImport);

	ImportedTracks = Factory.NewTracks;

	// Remove the temp package from the root now that it has served its purpose
	TempPackage->RemoveFromRoot();
}


void FSequencer::ToggleNodeActive()
{
	bool bIsActive = !IsNodeActive();
	const FScopedTransaction Transaction( NSLOCTEXT("Sequencer", "ToggleNodeActive", "Toggle Node Active") );

	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(OutlinerNode, Sections);

		for (auto Section : Sections)
		{
			Section->Modify();
			Section->SetIsActive(bIsActive);
		}
	}
}


bool FSequencer::IsNodeActive() const
{
	// Active only if all are active
	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(OutlinerNode, Sections);

		for (auto Section : Sections)
		{
			if (!Section->IsActive())
			{
				return false;
			}
		}
	}
	return true;
}


void FSequencer::ToggleNodeLocked()
{
	bool bIsLocked = !IsNodeLocked();

	const FScopedTransaction Transaction( NSLOCTEXT("Sequencer", "ToggleNodeLocked", "Toggle Node Locked") );

	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(OutlinerNode, Sections);

		for (auto Section : Sections)
		{
			Section->Modify();
			Section->SetIsLocked(bIsLocked);
		}
	}
}


bool FSequencer::IsNodeLocked() const
{
	// Locked only if all are locked
	int NumSections = 0;
	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(OutlinerNode, Sections);

		for (auto Section : Sections)
		{
			if (!Section->IsLocked())
			{
				return false;
			}
			++NumSections;
		}
	}
	return NumSections > 0;
}

void FSequencer::SaveSelectedNodesSpawnableState()
{
	const FScopedTransaction Transaction( LOCTEXT("SaveSpawnableState", "Save spawnable state") );

	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	MovieScene->Modify();

	TArray<FMovieSceneSpawnable*> Spawnables;

	for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
	{
		if (Node->GetType() == ESequencerNode::Object)
		{
			FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(StaticCastSharedRef<FSequencerObjectBindingNode>(Node)->GetObjectBinding());
			if (Spawnable)
			{
				Spawnables.Add(Spawnable);
			}
		}
	}

	FScopedSlowTask SlowTask(Spawnables.Num(), LOCTEXT("SaveSpawnableStateProgress", "Saving selected spawnables"));
	SlowTask.MakeDialog(true);

	TArray<AActor*> PossessedActors;
	for (FMovieSceneSpawnable* Spawnable : Spawnables)
	{
		SlowTask.EnterProgressFrame();
		
		SpawnRegister->SaveDefaultSpawnableState(*Spawnable, ActiveTemplateIDs.Top(), *this);

		if (GWarn->ReceivedUserCancel())
		{
			break;
		}
	}

	NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}

void FSequencer::ConvertToSpawnable(TSharedRef<FSequencerObjectBindingNode> NodeToBeConverted)
{
	const FScopedTransaction Transaction( LOCTEXT("ConvertSelectedNodeSpawnable", "Convert Node to Spawnables") );

	// Ensure we're in a non-possessed state
	RestorePreAnimatedState();
	GetFocusedMovieSceneSequence()->GetMovieScene()->Modify();
	FMovieScenePossessable* Possessable = GetFocusedMovieSceneSequence()->GetMovieScene()->FindPossessable(NodeToBeConverted->GetObjectBinding());
	if (Possessable)
	{
		ConvertToSpawnableInternal(Possessable->GetGuid());
		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );
	}
}

void FSequencer::ConvertSelectedNodesToSpawnables()
{
	// @todo sequencer: Undo doesn't seem to be working at all
	const FScopedTransaction Transaction( LOCTEXT("ConvertSelectedNodesSpawnable", "Convert Selected Nodes to Spawnables") );

	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	// Ensure we're in a non-possessed state
	RestorePreAnimatedState();
	MovieScene->Modify();

	TArray<TSharedRef<FSequencerObjectBindingNode>> ObjectBindingNodes;

	for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
	{
		if (Node->GetType() == ESequencerNode::Object)
		{
			auto ObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);

			// If we have a possessable for this node, and it has no parent, we can convert it to a spawnable
			FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBindingNode->GetObjectBinding());
			if (Possessable && !Possessable->GetParent().IsValid())
			{
				ObjectBindingNodes.Add(ObjectBindingNode);
			}
		}
	}

	FScopedSlowTask SlowTask(ObjectBindingNodes.Num(), LOCTEXT("ConvertSpawnableProgress", "Converting Selected Possessable Nodes to Spawnables"));
	SlowTask.MakeDialog(true);

	TArray<AActor*> SpawnedActors;
	for (const TSharedRef<FSequencerObjectBindingNode>& ObjectBindingNode : ObjectBindingNodes)
	{
		SlowTask.EnterProgressFrame();
	
		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBindingNode->GetObjectBinding());
		if (Possessable)
		{
			FMovieSceneSpawnable* Spawnable = ConvertToSpawnableInternal(Possessable->GetGuid());

			if (Spawnable)
			{
				ForceEvaluate();

				for (TWeakObjectPtr<> WeakObject : FindBoundObjects(Spawnable->GetGuid(), ActiveTemplateIDs.Top()))
				{
					if (AActor* SpawnedActor = Cast<AActor>(WeakObject.Get()))
					{
						SpawnedActors.Add(SpawnedActor);
					}
				}
			}
		}

		if (GWarn->ReceivedUserCancel())
		{
			break;
		}
	}

	if (SpawnedActors.Num())
	{
		const bool bNotifySelectionChanged = true;
		const bool bDeselectBSP = true;
		const bool bWarnAboutTooManyActors = false;
		const bool bSelectEvenIfHidden = false;

		GEditor->GetSelectedActors()->Modify();
		GEditor->GetSelectedActors()->BeginBatchSelectOperation();
		GEditor->SelectNone(bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors);
		for (auto SpawnedActor : SpawnedActors)
		{
			GEditor->SelectActor(SpawnedActor, true, bNotifySelectionChanged, bSelectEvenIfHidden);
		}
		GEditor->GetSelectedActors()->EndBatchSelectOperation();
		GEditor->NoteSelectionChange();
	}

	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );
}

FMovieSceneSpawnable* FSequencer::ConvertToSpawnableInternal(FGuid PossessableGuid)
{
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = Sequence->GetMovieScene();

	// Find the object in the environment
	FMovieScenePossessable* Possessable = MovieScene->FindPossessable(PossessableGuid);

	//@todo: this code doesn't work where multiple objects are bound
	TArrayView<TWeakObjectPtr<>> FoundObjects = FindBoundObjects(PossessableGuid, ActiveTemplateIDs.Top());
	if (FoundObjects.Num() != 1)
	{
		return nullptr;
	}

	UObject* FoundObject = FoundObjects[0].Get();
	if (!FoundObject)
	{
		return nullptr;
	}

	Sequence->Modify();

	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(AddSpawnable(*FoundObject));
	if (!Spawnable)
	{
		return nullptr;
	}

	// Swap the guids, so the possessable's tracks now belong to the spawnable
	{
		FGuid BenignSpawnableGuid = Spawnable->GetGuid();
		FGuid PersistentGuid = Possessable->GetGuid();

		Possessable->SetGuid(BenignSpawnableGuid);
		Spawnable->SetGuid(PersistentGuid);

		if (MovieScene->RemovePossessable(BenignSpawnableGuid))
		{
			Sequence->UnbindPossessableObjects(PersistentGuid);
		}

		for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index)
		{
			FMovieScenePossessable& MovieScenePossessable = MovieScene->GetPossessable(Index);
			bool bBelongsToNewSpawnable = MovieScenePossessable.GetParent() == PersistentGuid;
			if (bBelongsToNewSpawnable)
			{
				MovieScenePossessable.SetParent(PersistentGuid);
				Spawnable->AddChildPossessable(MovieScenePossessable.GetGuid());
			}
		}
	}

	FTransformData TransformData;
	SpawnRegister->HandleConvertPossessableToSpawnable(FoundObject, *this, TransformData);
	SpawnRegister->SetupDefaultsForSpawnable(nullptr, Spawnable->GetGuid(), TransformData, AsShared(), Settings);

	SetLocalTimeDirectly(ScrubPosition);

	return Spawnable;
}

void FSequencer::ConvertToPossessable(TSharedRef<FSequencerObjectBindingNode> NodeToBeConverted)
{
	const FScopedTransaction Transaction( LOCTEXT("ConvertSelectedNodePossessable", "Convert Node to Possessables") );

	// Ensure we're in a non-possessed state
	RestorePreAnimatedState();
	GetFocusedMovieSceneSequence()->GetMovieScene()->Modify();
	FMovieSceneSpawnable* Spawnable = GetFocusedMovieSceneSequence()->GetMovieScene()->FindSpawnable(NodeToBeConverted->GetObjectBinding());
	if (Spawnable)
	{
		ConvertToPossessableInternal(Spawnable->GetGuid());
		NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}
}

void FSequencer::ConvertSelectedNodesToPossessables()
{
	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	TArray<TSharedRef<FSequencerObjectBindingNode>> ObjectBindingNodes;

	for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
	{
		if (Node->GetType() == ESequencerNode::Object)
		{
			auto ObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);

			FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingNode->GetObjectBinding());
			if (Spawnable && SpawnRegister->CanConvertSpawnableToPossessable(*Spawnable))
			{
				ObjectBindingNodes.Add(ObjectBindingNode);
			}
		}
	}

	if (ObjectBindingNodes.Num() > 0)
	{
		const FScopedTransaction Transaction(LOCTEXT("ConvertSelectedNodesPossessable", "Convert Selected Nodes to Possessables"));
		MovieScene->Modify();

		FScopedSlowTask SlowTask(ObjectBindingNodes.Num(), LOCTEXT("ConvertPossessablesProgress", "Converting Selected Spawnable Nodes to Possessables"));
		SlowTask.MakeDialog(true);

		TArray<AActor*> PossessedActors;
		for (const TSharedRef<FSequencerObjectBindingNode>& ObjectBindingNode : ObjectBindingNodes)
		{
			SlowTask.EnterProgressFrame();

			FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingNode->GetObjectBinding());
			if (Spawnable)
			{
				FMovieScenePossessable* Possessable = ConvertToPossessableInternal(Spawnable->GetGuid());

				ForceEvaluate();

				for (TWeakObjectPtr<> WeakObject : FindBoundObjects(Possessable->GetGuid(), ActiveTemplateIDs.Top()))
				{
					if (AActor* PossessedActor = Cast<AActor>(WeakObject.Get()))
					{
						PossessedActors.Add(PossessedActor);
					}
				}
			}

			if (GWarn->ReceivedUserCancel())
			{
				break;
			}
		}

		if (PossessedActors.Num())
		{
			const bool bNotifySelectionChanged = true;
			const bool bDeselectBSP = true;
			const bool bWarnAboutTooManyActors = false;
			const bool bSelectEvenIfHidden = false;

			GEditor->GetSelectedActors()->Modify();
			GEditor->GetSelectedActors()->BeginBatchSelectOperation();
			GEditor->SelectNone(bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors);
			for (auto PossessedActor : PossessedActors)
			{
				GEditor->SelectActor(PossessedActor, true, bNotifySelectionChanged, bSelectEvenIfHidden);
			}
			GEditor->GetSelectedActors()->EndBatchSelectOperation();
			GEditor->NoteSelectionChange();

			NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
		}
	}
}

FMovieScenePossessable* FSequencer::ConvertToPossessableInternal(FGuid SpawnableGuid)
{
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = Sequence->GetMovieScene();

	// Find the object in the environment
	UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();
	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(SpawnableGuid);
	if (!Spawnable || !Spawnable->GetObjectTemplate())
	{
		return nullptr;
	}

	AActor* SpawnableActorTemplate = Cast<AActor>(Spawnable->GetObjectTemplate());
	if (!SpawnableActorTemplate)
	{
		return nullptr;
	}

	// Delete the spawn track
	UMovieSceneSpawnTrack* SpawnTrack = Cast<UMovieSceneSpawnTrack>(MovieScene->FindTrack(UMovieSceneSpawnTrack::StaticClass(), SpawnableGuid, NAME_None));
	if (SpawnTrack)
	{
		MovieScene->Modify();
		MovieScene->RemoveTrack(*SpawnTrack);
	}

	FTransform SpawnTransform = SpawnableActorTemplate->GetActorTransform();

	UWorld* PlaybackContext = Cast<UWorld>(GetPlaybackContext());
	AActor* PossessedActor = PlaybackContext->SpawnActor(Spawnable->GetObjectTemplate()->GetClass(), &SpawnTransform);

	if (!PossessedActor)
	{
		return nullptr;
	}

	UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
	PossessedActor->UnregisterAllComponents();
	UEngine::CopyPropertiesForUnrelatedObjects(SpawnableActorTemplate, PossessedActor, CopyParams);
	PossessedActor->RegisterAllComponents();

	const FGuid PossessableGuid = CreateBinding(*PossessedActor, PossessedActor->GetActorLabel());

	FMovieScenePossessable* Possessable = MovieScene->FindPossessable(PossessableGuid);
	if (!Possessable)
	{
		return nullptr;
	}

	// Swap the guids, so the spawnable's tracks now belong to the possessable
	{
		FGuid BenignSpawnableGuid = Spawnable->GetGuid();
		FGuid PersistentGuid = Possessable->GetGuid();

		Spawnable->SetGuid(PersistentGuid);
		Possessable->SetGuid(BenignSpawnableGuid);

		if (MovieScene->RemoveSpawnable(PersistentGuid))
		{
			SpawnRegister->DestroySpawnedObject(BenignSpawnableGuid, ActiveTemplateIDs.Top(), *this);
		}

		Sequence->BindPossessableObject(BenignSpawnableGuid, *PossessedActor, PlaybackContext);
	}
	
	GEditor->SelectActor(PossessedActor, false, true);

	SetLocalTimeDirectly(ScrubPosition);

	return Possessable;
}

void FSequencer::OnAddFolder()
{
	FScopedTransaction AddFolderTransaction( NSLOCTEXT("Sequencer", "AddFolder_Transaction", "Add Folder") );

	// Check if a folder, or child of a folder is currently selected.
	TArray<UMovieSceneFolder*> SelectedParentFolders;
	if ( Selection.GetSelectedOutlinerNodes().Num() > 0 )
	{
		for ( TSharedRef<FSequencerDisplayNode> SelectedNode : Selection.GetSelectedOutlinerNodes() )
		{
			TSharedPtr<FSequencerDisplayNode> CurrentNode = SelectedNode;
			while ( CurrentNode.IsValid() && CurrentNode->GetType() != ESequencerNode::Folder )
			{
				CurrentNode = CurrentNode->GetParent();
			}
			if ( CurrentNode.IsValid() )
			{
				SelectedParentFolders.Add( &StaticCastSharedPtr<FSequencerFolderNode>( CurrentNode )->GetFolder() );
			}
		}
	}

	TArray<FName> ExistingFolderNames;
	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	
	// If there is a folder selected the existing folder names are the sibling folders.
	if ( SelectedParentFolders.Num() == 1 )
	{
		for ( UMovieSceneFolder* SiblingFolder : SelectedParentFolders[0]->GetChildFolders() )
		{
			ExistingFolderNames.Add( SiblingFolder->GetFolderName() );
		}
	}
	// Otherwise use the root folders.
	else
	{
		for ( UMovieSceneFolder* MovieSceneFolder : FocusedMovieScene->GetRootFolders() )
		{
			ExistingFolderNames.Add( MovieSceneFolder->GetFolderName() );
		}
	}

	FName UniqueName = FSequencerUtilities::GetUniqueName(FName("New Folder"), ExistingFolderNames);
	UMovieSceneFolder* NewFolder = NewObject<UMovieSceneFolder>( FocusedMovieScene, NAME_None, RF_Transactional );
	NewFolder->SetFolderName( UniqueName );

	if ( SelectedParentFolders.Num() == 1 )
	{
		SelectedParentFolders[0]->Modify();
		SelectedParentFolders[0]->AddChildFolder( NewFolder );
	}
	else
	{
		FocusedMovieScene->Modify();
		FocusedMovieScene->GetRootFolders().Add( NewFolder );
	}

	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}


void FSequencer::TogglePlay()
{
	OnPlay();
}


void FSequencer::PlayForward()
{
	OnPlay(false);
}


void FSequencer::JumpToStart()
{
	OnJumpToStart();
}

void FSequencer::JumpToEnd()
{
	OnJumpToEnd();
}

void FSequencer::ShuttleForward()
{
	float NewPlayRate = PlayRate;
	if (ShuttleMultiplier == 0 || PlayRate < 0) 
	{
		ShuttleMultiplier = 2.f;
		NewPlayRate = 1.f;
	}
	else
	{
		NewPlayRate *= ShuttleMultiplier;
	}

	OnPlay(false, NewPlayRate);
}

void FSequencer::ShuttleBackward()
{
	float NewPlayRate = PlayRate;
	if (ShuttleMultiplier == 0 || PlayRate > 0)
	{
		ShuttleMultiplier = 2.f;
		NewPlayRate = -1.f;
	}
	else
	{
		NewPlayRate *= ShuttleMultiplier;
	}

	OnPlay(false, NewPlayRate);
}

void FSequencer::Pause()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);

	// When stopping a sequence, we always evaluate a non-empty range if possible. This ensures accurate paused motion blur effects.
	const float TimeSnapInterval = GetFixedFrameInterval();
	if (Settings->GetIsSnapEnabled() && TimeSnapInterval > 0)
	{
		ScrubPosition = (SequencerHelpers::SnapTimeToInterval(GetLocalTime(), GetFixedFrameInterval()) + TimeSnapInterval) * RootToLocalTransform.Inverse();

		FMovieSceneEvaluationRange Range = PlayPosition.PlayTo(ScrubPosition, GetRootMovieSceneSequence()->GetMovieScene()->GetOptionalFixedFrameInterval());
		EvaluateInternal(Range);
	}
	else
	{
		// Update on stop (cleans up things like sounds that are playing)
		TOptional<FMovieSceneEvaluationRange> LastRange = PlayPosition.GetLastRange();
		FMovieSceneEvaluationRange Range = LastRange.IsSet() ? LastRange.GetValue() : PlayPosition.JumpTo(ScrubPosition, GetFocusedMovieSceneSequence()->GetMovieScene()->GetOptionalFixedFrameInterval());
		EvaluateInternal(Range);
	}
}

void FSequencer::StepForward()
{
	OnStepForward();
}


void FSequencer::StepBackward()
{
	OnStepBackward();
}


void FSequencer::StepToNextKey()
{
	SequencerWidget->StepToNextKey();
}


void FSequencer::StepToPreviousKey()
{
	SequencerWidget->StepToPreviousKey();
}


void FSequencer::StepToNextCameraKey()
{
	SequencerWidget->StepToNextCameraKey();
}


void FSequencer::StepToPreviousCameraKey()
{
	SequencerWidget->StepToPreviousCameraKey();
}


void FSequencer::StepToNextShot()
{
	if (ActiveTemplateIDs.Num() < 2)
	{
		return;
	}

	UMovieSceneSequence* Sequence = RootTemplateInstance.GetSequence(ActiveTemplateIDs[ActiveTemplateIDs.Num()-2]);

	float StartTime = 0.f * RootToLocalTransform.Inverse();
	float CurrentTime = StartTime * RootTemplateInstance.GetInstance(ActiveTemplateIDs[ActiveTemplateIDs.Num()-2])->RootToSequenceTransform;

	UMovieSceneSubSection* NextShot = Cast<UMovieSceneSubSection>(FindNextOrPreviousShot(Sequence, CurrentTime, true));
	if (!NextShot)
	{
		return;
	}
		
	SequencerWidget->PopBreadcrumb();

	PopToSequenceInstance(ActiveTemplateIDs[ActiveTemplateIDs.Num()-2]);
			
	FocusSequenceInstance(*NextShot);
	
	SetLocalTime(0.f);
}


void FSequencer::StepToPreviousShot()
{
	if (ActiveTemplateIDs.Num() < 2)
	{
		return;
	}

	UMovieSceneSequence* Sequence = RootTemplateInstance.GetSequence(ActiveTemplateIDs[ActiveTemplateIDs.Num()-2]);

	float StartTime = 0.f * RootToLocalTransform.Inverse();
	float CurrentTime = StartTime * RootTemplateInstance.GetInstance(ActiveTemplateIDs[ActiveTemplateIDs.Num()-2])->RootToSequenceTransform;

	UMovieSceneSubSection* PreviousShot = Cast<UMovieSceneSubSection>(FindNextOrPreviousShot(Sequence, CurrentTime, false));
	if (!PreviousShot)
	{
		return;
	}

	SequencerWidget->PopBreadcrumb();
		
	PopToSequenceInstance(ActiveTemplateIDs[ActiveTemplateIDs.Num()-2]);
					
	FocusSequenceInstance(*PreviousShot);
	
	SetLocalTime(0.f);
}


void FSequencer::ExpandAllNodesAndDescendants()
{
	const bool bExpandAll = true;
	SequencerWidget->GetTreeView()->ExpandNodes(ETreeRecursion::Recursive, bExpandAll);
}


void FSequencer::CollapseAllNodesAndDescendants()
{
	const bool bExpandAll = true;
	SequencerWidget->GetTreeView()->CollapseNodes(ETreeRecursion::Recursive, bExpandAll);
}


void FSequencer::ToggleExpandCollapseNodes()
{
	SequencerWidget->GetTreeView()->ToggleExpandCollapseNodes(ETreeRecursion::NonRecursive);
}


void FSequencer::ToggleExpandCollapseNodesAndDescendants()
{
	SequencerWidget->GetTreeView()->ToggleExpandCollapseNodes(ETreeRecursion::Recursive);
}


void FSequencer::SetKey()
{
	FScopedTransaction SetKeyTransaction( NSLOCTEXT("Sequencer", "SetKey_Transaction", "Set Key") );

	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		if (OutlinerNode->GetType() == ESequencerNode::Track)
		{
			TSharedRef<FSequencerTrackNode> TrackNode = StaticCastSharedRef<FSequencerTrackNode>(OutlinerNode);
						
			TSharedRef<FSequencerDisplayNode> ObjectBindingNode = OutlinerNode;
			if (SequencerHelpers::FindObjectBindingNode(TrackNode, ObjectBindingNode))
			{
				FGuid ObjectGuid = StaticCastSharedRef<FSequencerObjectBindingNode>(ObjectBindingNode)->GetObjectBinding();
				TrackNode->AddKey(ObjectGuid);
			}
		}
	}

	TSet<TSharedPtr<IKeyArea> > KeyAreas;
	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		SequencerHelpers::GetAllKeyAreas(OutlinerNode, KeyAreas);
	}

	if (KeyAreas.Num() > 0)
	{	
		for (auto KeyArea : KeyAreas)
		{
			if (KeyArea->GetOwningSection()->TryModify())
			{
				KeyArea->AddKeyUnique(GetLocalTime(), GetKeyInterpolation());
			}
		}
	}

	UpdatePlaybackRange();
}


bool FSequencer::CanSetKeyTime() const
{
	return Selection.GetSelectedKeys().Num() > 0;
}


void FSequencer::SetKeyTime(const bool bUseFrames)
{
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();

	float KeyTime = 0.f;
	for ( const FSequencerSelectedKey& Key : SelectedKeysArray )
	{
		if (Key.IsValid())
		{
			KeyTime = Key.KeyArea->GetKeyTime(Key.KeyHandle.GetValue());
			break;
		}
	}

	float FrameRate = 1.0f / GetFixedFrameInterval();
	
	GenericTextEntryModeless(
		bUseFrames ? NSLOCTEXT("Sequencer.Popups", "SetKeyFramePopup", "New Frame") : NSLOCTEXT("Sequencer.Popups", "SetKeyTimePopup", "New Time"),
		bUseFrames ? FText::AsNumber( SequencerHelpers::TimeToFrame( KeyTime, FrameRate )) : FText::AsNumber( KeyTime ),
		FOnTextCommitted::CreateSP(this, &FSequencer::OnSetKeyTimeTextCommitted, bUseFrames)
	);
}


void FSequencer::OnSetKeyTimeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, const bool bUseFrames)
{
	bool bAnythingChanged = false;
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		float FrameRate = 1.0f / GetFixedFrameInterval();
		double dNewTime = bUseFrames ? SequencerHelpers::FrameToTime(FCString::Atod(*InText.ToString()), FrameRate) : FCString::Atod(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric(); 
		if(!bIsNumber)
			return;

		const float NewKeyTime = (float)dNewTime;

		FScopedTransaction SetKeyTimeTransaction(NSLOCTEXT("Sequencer", "SetKeyTime_Transaction", "Set Key Time"));
		TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();
	
		for ( const FSequencerSelectedKey& Key : SelectedKeysArray )
		{
			if (Key.IsValid())
			{
				if (Key.Section->TryModify())
				{
					Key.KeyArea->SetKeyTime(Key.KeyHandle.GetValue(), NewKeyTime);
					bAnythingChanged = true;

					if( NewKeyTime > Key.Section->GetEndTime() )
					{
						Key.Section->SetEndTime( NewKeyTime );
					}
					else if( NewKeyTime < Key.Section->GetStartTime() )
					{
						Key.Section->SetStartTime( NewKeyTime );
					}
				}
			}
		}
	}

	if (bAnythingChanged)
	{
		NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
	}
}


void FSequencer::SelectTrackKeys(TWeakObjectPtr<UMovieSceneSection> Section, float KeyTime, bool bAddToSelection, bool bToggleSelection)
{
	if (!bAddToSelection && !bToggleSelection)
	{
		Selection.EmptySelectedKeys();
	}

	TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
	Sections.Add(Section);
	TArray<FSectionHandle> SectionHandles = SequencerWidget->GetSectionHandles(Sections);
	for (auto SectionHandle : SectionHandles)
	{
		if (SectionHandle.TrackNode.IsValid())
		{
			TSet<TSharedPtr<IKeyArea> > KeyAreas;
			SequencerHelpers::GetAllKeyAreas(SectionHandle.TrackNode, KeyAreas);

			for (auto KeyArea : KeyAreas)
			{
				if (KeyArea.IsValid())
				{
					TArray<FKeyHandle> KeyHandles = KeyArea.Get()->GetUnsortedKeyHandles();
					for (auto KeyHandle : KeyHandles)
					{
						float KeyHandleTime = KeyArea.Get()->GetKeyTime(KeyHandle);

						if (FMath::IsNearlyEqual(KeyHandleTime, KeyTime, KINDA_SMALL_NUMBER))
						{
							FSequencerSelectedKey SelectedKey(*Section.Get(), KeyArea, KeyHandle);

							if (bToggleSelection)
							{
								if (Selection.IsSelected(SelectedKey))
								{
									Selection.RemoveFromSelection(SelectedKey);
								}
								else
								{
									Selection.AddToSelection(SelectedKey);
								}
							}
							else
							{
								Selection.AddToSelection(SelectedKey);
							}
						}
					}
				}
			}
		}
	}

	SequencerHelpers::ValidateNodesWithSelectedKeysOrSections(*this);
}


TArray<TSharedPtr<FMovieSceneClipboard>> GClipboardStack;

void FSequencer::CopySelection()
{
	if (Selection.GetSelectedKeys().Num() == 0)
	{
		TArray<TSharedPtr<FSequencerTrackNode>> TracksToCopy;
		TSet<TSharedRef<FSequencerDisplayNode>> SelectedNodes = Selection.GetNodesWithSelectedKeysOrSections();
		if (SelectedNodes.Num() == 0)
		{
			SelectedNodes = Selection.GetSelectedOutlinerNodes();
		}
		for (TSharedRef<FSequencerDisplayNode> Node : SelectedNodes)
		{
			if (Node->GetType() != ESequencerNode::Track)
			{
				continue;
			}

			TSharedPtr<FSequencerTrackNode> TrackNode = StaticCastSharedRef<FSequencerTrackNode>(Node);
			if (TrackNode.IsValid())
			{
				TracksToCopy.Add(TrackNode);
			}
		}

		// Make a empty clipboard if the stack is empty
		if (GClipboardStack.Num() == 0)
		{
			TSharedRef<FMovieSceneClipboard> NullClipboard = MakeShareable(new FMovieSceneClipboard());
			GClipboardStack.Push(NullClipboard);
		}
		CopySelectedTracks(TracksToCopy);
	}
	else
	{
		CopySelectedKeys();
	}
}


void FSequencer::CutSelection()
{
	FScopedTransaction CutSelectionTransaction(LOCTEXT("CutSelection_Transaction", "Cut Selection(s)"));
	if (Selection.GetSelectedKeys().Num() == 0)
	{
		TArray<TSharedPtr<FSequencerTrackNode>> TracksToCopy;
		TSet<TSharedRef<FSequencerDisplayNode>> SelectedNodes = Selection.GetNodesWithSelectedKeysOrSections();
		if (SelectedNodes.Num() == 0)
		{
			SelectedNodes = Selection.GetSelectedOutlinerNodes();
		}
		for (TSharedRef<FSequencerDisplayNode> Node : SelectedNodes)
		{
			if (Node->GetType() != ESequencerNode::Track)
			{
				FNotificationInfo Info(LOCTEXT("InvalidCut", "Warning: One of the selected node is not a track node"));
				Info.FadeInDuration = 0.1f;
				Info.FadeOutDuration = 0.5f;
				Info.ExpireDuration = 2.5f;
				auto NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);

				NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
				NotificationItem->ExpireAndFadeout();
				return;
			}

			TSharedPtr<FSequencerTrackNode> TrackNode = StaticCastSharedRef<FSequencerTrackNode>(Node);
			if (TrackNode.IsValid())
			{
				TracksToCopy.Add(TrackNode);
			}
		}
		// Make a empty clipboard if the stack is empty
		if (GClipboardStack.Num() == 0)
		{
			TSharedRef<FMovieSceneClipboard> NullClipboard = MakeShareable(new FMovieSceneClipboard());
			GClipboardStack.Push(NullClipboard);
		}
		CopySelectedTracks(TracksToCopy);
		DeleteSelectedItems();
	}
	else
	{
		CutSelectedKeys();
	}
}

void FSequencer::CopySelectedKeys()
{
	TOptional<float> CopyRelativeTo;
	
	// Copy relative to the current key hotspot, if applicable
	if (Hotspot.IsValid() && Hotspot->GetType() == ESequencerHotspot::Key)
	{
		FKeyHotspot* KeyHotspot = StaticCastSharedPtr<FKeyHotspot>(Hotspot).Get();
		if (KeyHotspot->Key.KeyArea.IsValid() && KeyHotspot->Key.KeyHandle.IsSet())
		{
			CopyRelativeTo = KeyHotspot->Key.KeyArea->GetKeyTime(KeyHotspot->Key.KeyHandle.GetValue());
		}
	}

	FMovieSceneClipboardBuilder Builder;

	// Map selected keys to their key areas
	TMap<const IKeyArea*, TArray<FKeyHandle>> KeyAreaMap;
	for (const FSequencerSelectedKey& Key : Selection.GetSelectedKeys())
	{
		if (Key.KeyHandle.IsSet())
		{
			KeyAreaMap.FindOrAdd(Key.KeyArea.Get()).Add(Key.KeyHandle.GetValue());
		}
	}

	// Serialize each key area to the clipboard
	for (auto& Pair : KeyAreaMap)
	{
		Pair.Key->CopyKeys(Builder, [&](FKeyHandle Handle, const IKeyArea&){
			return Pair.Value.Contains(Handle);
		});
	}

	TSharedRef<FMovieSceneClipboard> Clipboard = MakeShareable( new FMovieSceneClipboard(Builder.Commit(CopyRelativeTo)) );
	
	if (Clipboard->GetKeyTrackGroups().Num())
	{
		GClipboardStack.Push(Clipboard);

		if (GClipboardStack.Num() > 10)
		{
			GClipboardStack.RemoveAt(0, 1);
		}
	}
}


void FSequencer::CutSelectedKeys()
{
	FScopedTransaction CutSelectedKeysTransaction(LOCTEXT("CutSelectedKeys_Transaction", "Cut Selected keys"));
	CopySelectedKeys();
	DeleteSelectedKeys();
}


const TArray<TSharedPtr<FMovieSceneClipboard>>& FSequencer::GetClipboardStack() const
{
	return GClipboardStack;
}


void FSequencer::OnClipboardUsed(TSharedPtr<FMovieSceneClipboard> Clipboard)
{
	Clipboard->GetEnvironment().DateTime = FDateTime::UtcNow();

	// Last entry in the stack should be the most up-to-date
	GClipboardStack.Sort([](const TSharedPtr<FMovieSceneClipboard>& A, const TSharedPtr<FMovieSceneClipboard>& B){
		return A->GetEnvironment().DateTime < B->GetEnvironment().DateTime;
	});
}


void FSequencer::DiscardChanges()
{
	if (ActiveTemplateIDs.Num() == 0)
	{
		return;
	}

	TSharedPtr<IToolkitHost> MyToolkitHost = GetToolkitHost();

	if (!MyToolkitHost.IsValid())
	{
		return;
	}

	UMovieSceneSequence* EditedSequence = GetFocusedMovieSceneSequence();

	if (EditedSequence == nullptr)
	{
		return;
	}

	if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("RevertConfirm", "Are you sure you want to discard your current changes?")) != EAppReturnType::Yes)
	{
		return;
	}

	FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();
	UClass* SequenceClass = EditedSequence->GetClass();
	FString SequencePath = EditedSequence->GetPathName();
	UPackage* SequencePackage = EditedSequence->GetOutermost();

	// close asset editor
	AssetEditorManager.CloseAllEditorsForAsset(EditedSequence);

	// collect objects to be unloaded
	TMap<FString, UObject*> MovedObjects;

	ForEachObjectWithOuter(SequencePackage, [&](UObject* Object) {
		MovedObjects.Add(Object->GetPathName(), Object);
	}, true);

	// move objects into transient package
	UPackage* const TransientPackage = GetTransientPackage();

	for (auto MovedObject : MovedObjects)
	{
		UObject* Object = MovedObject.Value;

		const FString OldName = Object->GetName();
		const FString NewName = FString::Printf(TEXT("UNLOADING_%s"), *OldName);
		const FName UniqueName = MakeUniqueObjectName(TransientPackage, Object->GetClass(), FName(*NewName));
		UObject* NewOuter = (Object->GetOuter() == SequencePackage) ? TransientPackage : Object->GetOuter();
	
		Object->Rename(*UniqueName.ToString(), NewOuter, REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional);
		Object->SetFlags(RF_Transient);
		Object->ClearFlags(RF_Standalone | RF_Transactional);
	}

	for (auto MovedObject : MovedObjects)
	{
		GLog->Logf(TEXT("Moved %s ---------> %s"), *MovedObject.Key, *MovedObject.Value->GetPathName());
	}

	// unload package
	SequencePackage->SetDirtyFlag(false);

	TArray<UPackage*> PackagesToUnload;
	PackagesToUnload.Add(SequencePackage);

	FText PackageUnloadError;
	PackageTools::UnloadPackages(PackagesToUnload, PackageUnloadError);

	if (!PackageUnloadError.IsEmpty())
	{
		ResetLoaders(SequencePackage);
		SequencePackage->ClearFlags(RF_WasLoaded);
		SequencePackage->bHasBeenFullyLoaded = false;
		SequencePackage->GetMetaData()->RemoveMetaDataOutsidePackage();
	}

	// reload package
	TMap<UObject*, UObject*> MovedToReloadedObjectMap;

	for (const auto MovedObject : MovedObjects)
	{
		UObject* ReloadedObject = StaticLoadObject(MovedObject.Value->GetClass(), nullptr, *MovedObject.Key, nullptr);//, LOAD_NoWarn);
		MovedToReloadedObjectMap.Add(MovedObject.Value, ReloadedObject);
	}

	for (TObjectIterator<UObject> It; It; ++It)
	{
		// @todo sequencer: only process objects that actually reference the package?
		FArchiveReplaceObjectRef<UObject> Ar(*It, MovedToReloadedObjectMap, false, false, false, false);
	}

	auto ReloadedSequence = Cast<UMovieSceneSequence>(StaticLoadObject(SequenceClass, nullptr, *SequencePath, nullptr));//, LOAD_NoWarn));

	// release transient objects
	for (auto MovedObject : MovedObjects)
	{
		MovedObject.Value->RemoveFromRoot();
		MovedObject.Value->MarkPendingKill();
	}

//	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	// clear undo buffer
	if (true) // @todo sequencer: check whether objects are actually referenced in undo buffer
	{
		GEditor->Trans->Reset(LOCTEXT("UnloadedSequence", "Unloaded Sequence"));
	}

	// reopen asset editor
	TArray<UObject*> AssetsToReopen;
	AssetsToReopen.Add(ReloadedSequence);

	AssetEditorManager.OpenEditorForAssets(AssetsToReopen, EToolkitMode::Standalone, MyToolkitHost.ToSharedRef());
}


void FSequencer::CreateCamera()
{
	UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "CreateCameraHere", "Create Camera Here"));

	const bool bCreateAsSpawnable = Settings->GetCreateSpawnableCameras();

	FActorSpawnParameters SpawnParams;
	if (bCreateAsSpawnable)
	{
		// Don't bother transacting this object if we're creating a spawnable since it's temporary
		SpawnParams.ObjectFlags &= ~RF_Transactional;
	}

	// Set new camera to match viewport
	ACineCameraActor* NewCamera = World->SpawnActor<ACineCameraActor>(SpawnParams);
	if (!NewCamera)
	{
		return;
	}

	FGuid CameraGuid;

	FMovieSceneSpawnable* Spawnable = nullptr;
	ESpawnOwnership SavedOwnership = Spawnable ? Spawnable->GetSpawnOwnership() : ESpawnOwnership::InnerSequence;

	if (bCreateAsSpawnable)
	{
		CameraGuid = MakeNewSpawnable(*NewCamera);
		Spawnable = GetFocusedMovieSceneSequence()->GetMovieScene()->FindSpawnable(CameraGuid);

		if (ensure(Spawnable))
		{
			// Override spawn ownership during this process to ensure it never gets destroyed
			SavedOwnership = Spawnable->GetSpawnOwnership();
			Spawnable->SetSpawnOwnership(ESpawnOwnership::External);
		}

		// Destroy the old actor
		World->EditorDestroyActor(NewCamera, false);

		for (TWeakObjectPtr<UObject>& Object : FindBoundObjects(CameraGuid, ActiveTemplateIDs.Top()))
		{
			NewCamera = Cast<ACineCameraActor>(Object.Get());
			if (NewCamera)
			{
				break;
			}
		}
		ensure(NewCamera);
	}
	else
	{
		CameraGuid = CreateBinding(*NewCamera, NewCamera->GetActorLabel());
	}
	
	if (!CameraGuid.IsValid())
	{
		return;
	}
	
	NewCamera->SetActorLocation( GCurrentLevelEditingViewportClient->GetViewLocation(), false );
	NewCamera->SetActorRotation( GCurrentLevelEditingViewportClient->GetViewRotation() );
	//pNewCamera->CameraComponent->FieldOfView = ViewportClient->ViewFOV; //@todo set the focal length from this field of view

	OnActorAddedToSequencerEvent.Broadcast(NewCamera, CameraGuid);

	const bool bLockToCamera = true;
	NewCameraAdded(NewCamera, CameraGuid, bLockToCamera);

	if (ensure(Spawnable))
	{
		Spawnable->SetSpawnOwnership(SavedOwnership);
	}

	NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}

void FSequencer::NewCameraAdded(ACineCameraActor* NewCamera, FGuid CameraGuid, bool bLockToCamera)
{
	SetPerspectiveViewportCameraCutEnabled(false);

	// Lock the viewport to this camera
	if (bLockToCamera && NewCamera && NewCamera->GetLevel())
	{
		GCurrentLevelEditingViewportClient->SetMatineeActorLock(nullptr);
		GCurrentLevelEditingViewportClient->SetActorLock(NewCamera);
		GCurrentLevelEditingViewportClient->bLockedCameraView = true;
		GCurrentLevelEditingViewportClient->UpdateViewForLockedActor();
		GCurrentLevelEditingViewportClient->Invalidate();
	}

	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

	// If there's a cinematic shot track, no need to set this camera to a shot
	UMovieSceneTrack* CinematicShotTrack = OwnerMovieScene->FindMasterTrack(UMovieSceneCinematicShotTrack::StaticClass());
	if (CinematicShotTrack)
	{
		return;
	}

	// If there's a camera cut track, create or set the camera section to this new camera
	UMovieSceneTrack* CameraCutTrack = OwnerMovieScene->GetCameraCutTrack();

	if (!CameraCutTrack)
	{
		CameraCutTrack = OwnerMovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass());
	}

	if (CameraCutTrack)
	{
		UMovieSceneSection* Section = MovieSceneHelpers::FindSectionAtTime(CameraCutTrack->GetAllSections(), GetLocalTime());
		UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(Section);

		if (CameraCutSection)
		{
			CameraCutSection->Modify();
			CameraCutSection->SetCameraGuid(CameraGuid);
		}
		else
		{
			CameraCutTrack->Modify();

			UMovieSceneCameraCutSection* NewSection = Cast<UMovieSceneCameraCutSection>(CameraCutTrack->CreateNewSection());
			NewSection->SetStartTime(GetPlaybackRange().GetLowerBoundValue());
			NewSection->SetEndTime(GetPlaybackRange().GetUpperBoundValue());
			NewSection->SetCameraGuid(CameraGuid);
			CameraCutTrack->AddSection(*NewSection);
		}
	}
}


void FSequencer::FixActorReferences()
{
	FScopedTransaction FixActorReferencesTransaction( NSLOCTEXT( "Sequencer", "FixActorReferences", "Fix Actor References" ) );

	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	TMap<FString, AActor*> ActorNameToActorMap;
	UWorld* PlaybackContext = Cast<UWorld>(GetPlaybackContext());
	for ( TActorIterator<AActor> ActorItr( PlaybackContext ); ActorItr; ++ActorItr )
	{
		// Same as with the Object Iterator, access the subclass instance with the * or -> operators.
		AActor *Actor = *ActorItr;
		ActorNameToActorMap.Add( Actor->GetActorLabel(), Actor);
	}

	// Cache the possessables to fix up first since the bindings will change as the fix ups happen.
	TArray<FMovieScenePossessable> ActorsPossessablesToFix;
	for ( int32 i = 0; i < FocusedMovieScene->GetPossessableCount(); i++ )
	{
		FMovieScenePossessable& Possessable = FocusedMovieScene->GetPossessable( i );
		// Possessables with parents are components so ignore them.
		if ( Possessable.GetParent().IsValid() == false )
		{
			if ( FindBoundObjects(Possessable.GetGuid(), ActiveTemplateIDs.Top()).Num() == 0 )
			{
				ActorsPossessablesToFix.Add( Possessable );
			}
		}
	}

	// For the possessables to fix, look up the actors by name and reassign them if found.
	TMap<FGuid, FGuid> OldGuidToNewGuidMap;
	for ( const FMovieScenePossessable& ActorPossessableToFix : ActorsPossessablesToFix )
	{
		AActor** ActorPtr = ActorNameToActorMap.Find( ActorPossessableToFix.GetName() );
		if ( ActorPtr != nullptr )
		{
			FGuid OldGuid = ActorPossessableToFix.GetGuid();

			// The actor might have an existing guid while the possessable with the same name might not. 
			// In that case, make sure we also replace the existing guid with the new guid 
			FGuid ExistingGuid = FindObjectId( **ActorPtr, ActiveTemplateIDs.Top() );

			FGuid NewGuid = DoAssignActor( ActorPtr, 1, ActorPossessableToFix.GetGuid() );

			OldGuidToNewGuidMap.Add(OldGuid, NewGuid);

			if (ExistingGuid.IsValid())
			{
				OldGuidToNewGuidMap.Add(ExistingGuid, NewGuid);
			}
		}
	}

	// Fixup any section bindings
	for (UMovieSceneSection* Section : FocusedMovieScene->GetAllSections())
	{
		Section->OnBindingsUpdated(OldGuidToNewGuidMap);
	}
}

void FSequencer::RebindPossessableReferences()
{
	FScopedTransaction Transaction(LOCTEXT("RebindAllPossessables", "Rebind Possessable References"));

	UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();
	FocusedSequence->Modify();

	UMovieScene* FocusedMovieScene = FocusedSequence->GetMovieScene();

	TMap<FGuid, TArray<UObject*, TInlineAllocator<1>>> AllObjects;

	UObject* PlaybackContext = PlaybackContextAttribute.Get(nullptr);

	for (int32 Index = 0; Index < FocusedMovieScene->GetPossessableCount(); Index++)
	{
		const FMovieScenePossessable& Possessable = FocusedMovieScene->GetPossessable(Index);

		TArray<UObject*, TInlineAllocator<1>>& References = AllObjects.FindOrAdd(Possessable.GetGuid());
		FocusedSequence->LocateBoundObjects(Possessable.GetGuid(), PlaybackContext, References);
	}

	for (auto& Pair : AllObjects)
	{
		// Only rebind things if they exist
		if (Pair.Value.Num() > 0)
		{
			FocusedSequence->UnbindPossessableObjects(Pair.Key);
			for (UObject* Object : Pair.Value)
			{
				FocusedSequence->BindPossessableObject(Pair.Key, *Object, PlaybackContext);
			}
		}
	}
}

float SnapTime( float TimeValue, float TimeInterval )
{
	return FMath::RoundToInt( TimeValue / TimeInterval ) * TimeInterval;
}


void FixSceneRangeTiming( UMovieScene* MovieScene, float FrameInterval )
{
	TRange<float> SceneRange = MovieScene->GetPlaybackRange();

	float LowerBoundValue = SceneRange.GetLowerBoundValue();
	float SnappedLowerBoundValue = SnapTime( LowerBoundValue, FrameInterval );

	float UpperBoundValue = SceneRange.GetUpperBoundValue();
	float SnappedUpperBoundValue = SnapTime( UpperBoundValue, FrameInterval );

	if ( SnappedLowerBoundValue != LowerBoundValue || SnappedUpperBoundValue != UpperBoundValue)
	{
		MovieScene->SetPlaybackRange( SnappedLowerBoundValue, SnappedUpperBoundValue );
	}
}


void FixSectionFrameTiming( UMovieSceneSection* Section, float FrameInterval )
{
	bool bSectionModified = false;
	float SnappedStartTime = SnapTime( Section->GetStartTime(), FrameInterval );
	if ( SnappedStartTime != Section->GetStartTime() )
	{
		Section->Modify();
		bSectionModified = true;
		Section->SetStartTime( SnappedStartTime );
	}

	float SnappedEndTime = SnapTime( Section->GetEndTime(), FrameInterval );
	if ( SnappedEndTime != Section->GetEndTime() )
	{
		if ( bSectionModified == false )
		{
			Section->Modify();
			bSectionModified = true;
		}
		Section->SetEndTime( SnappedEndTime );
	}

	TSet<FKeyHandle> KeyHandles;
	Section->GetKeyHandles( KeyHandles, Section->GetRange() );
	for ( const FKeyHandle& KeyHandle : KeyHandles )
	{
		TOptional<float> KeyTime = Section->GetKeyTime( KeyHandle );
		if ( KeyTime.IsSet() )
		{
			float SnappedKeyTime = SnapTime( KeyTime.GetValue(), FrameInterval );
			if ( SnappedKeyTime != KeyTime.GetValue() )
			{
				if ( bSectionModified == false )
				{
					Section->Modify();
					bSectionModified = true;
				}
				Section->SetKeyTime( KeyHandle, SnappedKeyTime );
			}
		}
	}
}


void GetAllMovieScenesRecursively( UMovieScene* CurrentMovieScene, TArray<UMovieScene*>& AllMovieScenes )
{
	if( CurrentMovieScene != nullptr && AllMovieScenes.Contains( CurrentMovieScene ) == false)
	{
		AllMovieScenes.Add( CurrentMovieScene );
		for ( UMovieSceneTrack* MasterTrack : CurrentMovieScene->GetMasterTracks() )
		{
			UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>( MasterTrack );
			if ( SubTrack != nullptr )
			{
				for ( UMovieSceneSection* Section : SubTrack->GetAllSections() )
				{
					UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>( Section );
					if ( SubSection != nullptr )
					{
						GetAllMovieScenesRecursively( SubSection->GetSequence()->GetMovieScene(), AllMovieScenes );
					}
				}
			}
		}
	}
}


void FSequencer::FixFrameTiming()
{
	TArray<UMovieScene*> ScenesToFix;
	GetAllMovieScenesRecursively( GetRootMovieSceneSequence()->GetMovieScene(), ScenesToFix );

	FScopedTransaction FixFrameTimingTransaction( NSLOCTEXT( "Sequencer", "FixFrameTiming", "Fix frame timing" ) );
	for( UMovieScene* SceneToFix : ScenesToFix)
	{
		float FrameInterval = SceneToFix->GetFixedFrameInterval();
		if ( FrameInterval > 0 )
		{
			FixSceneRangeTiming( SceneToFix, FrameInterval );

			// Collect all tracks.
			TArray<UMovieSceneTrack*> TracksToFix;

			if ( SceneToFix->GetCameraCutTrack() != nullptr )
			{
				TracksToFix.Add( SceneToFix->GetCameraCutTrack() );
			}

			for ( UMovieSceneTrack* MasterTrack : SceneToFix->GetMasterTracks() )
			{
				TracksToFix.Add( MasterTrack );
			}

			for ( const FMovieSceneBinding& ObjectBinding : SceneToFix->GetBindings() )
			{
				TracksToFix.Append( ObjectBinding.GetTracks() );
			}

			// Fix section and keys for tracks in the current scene.
			for ( UMovieSceneTrack* TrackToFix : TracksToFix )
			{
				for ( UMovieSceneSection* Section : TrackToFix->GetAllSections() )
				{
					FixSectionFrameTiming( Section, FrameInterval );
				}
			}
		}
	}
}

void FSequencer::ImportFBX()
{
	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	// The object binding and names to match when importing from fbx
	TMap<FGuid, FString> ObjectBindingNameMap;

	for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
	{
		if (Node->GetType() == ESequencerNode::Object)
		{
			auto ObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);

			FGuid ObjectBinding = ObjectBindingNode.Get().GetObjectBinding();

			ObjectBindingNameMap.Add(ObjectBinding, ObjectBindingNode.Get().GetDisplayName().ToString());
		}
	}

	// If nothing selected, try to map onto everything
	if (ObjectBindingNameMap.Num() == 0)
	{
		TArray<TSharedRef<FSequencerObjectBindingNode>> RootObjectBindingNodes;
		GetRootObjectBindingNodes( NodeTree->GetRootNodes(), RootObjectBindingNodes );

		for (auto RootObjectBindingNode : RootObjectBindingNodes)
		{
			FGuid ObjectBinding = RootObjectBindingNode.Get().GetObjectBinding();

			ObjectBindingNameMap.Add(ObjectBinding, RootObjectBindingNode.Get().GetDisplayName().ToString());
		}
	}
	
	if (MovieSceneToolHelpers::ImportFBX(MovieScene, *this, ObjectBindingNameMap))
	{
		NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	}
}


void FSequencer::ExportFBX()
{
	TArray<FString> SaveFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bExportFileNamePicked = false;
	if ( DesktopPlatform != NULL )
	{
		bExportFileNamePicked = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT( "ExportLevelSequence", "Export Level Sequence" ).ToString(),
			*( FEditorDirectories::Get().GetLastDirectory( ELastDirectory::FBX ) ),
			TEXT( "" ),
			TEXT( "FBX document|*.fbx" ),
			EFileDialogFlags::None,
			SaveFilenames );
	}

	if ( bExportFileNamePicked )
	{
		FString ExportFilename = SaveFilenames[0];
		FEditorDirectories::Get().SetLastDirectory( ELastDirectory::FBX, FPaths::GetPath( ExportFilename ) ); // Save path as default for next time.

		UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
		//Show the fbx export dialog options
		bool ExportCancel = false;
		bool ExportAll = false;
		Exporter->FillExportOptions(false, true, ExportFilename, ExportCancel, ExportAll);
		if (!ExportCancel)
		{
			Exporter->CreateDocument();
			Exporter->SetTrasformBaking(false);
			Exporter->SetKeepHierarchy(true);

			// Select selected nodes if there are selected nodes
			TArray<FGuid> Bindings;
			for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
			{
				if (Node->GetType() == ESequencerNode::Object)
				{
					auto ObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);
					Bindings.Add(ObjectBindingNode.Get().GetObjectBinding());

					TSet<TSharedRef<FSequencerDisplayNode> > DescendantNodes;
					SequencerHelpers::GetDescendantNodes(Node, DescendantNodes);
					for (auto DescendantNode : DescendantNodes)
					{
						if (!Selection.IsSelected(DescendantNode) && DescendantNode->GetType() == ESequencerNode::Object)
						{
							auto DescendantObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(DescendantNode);
							Bindings.Add(DescendantObjectBindingNode.Get().GetObjectBinding());
						}
					}
				}
			}

			const bool bSelectedOnly = Bindings.Num() != 0;

			UnFbx::FFbxExporter::FLevelSequenceNodeNameAdapter NodeNameAdapter(GetFocusedMovieSceneSequence()->GetMovieScene(), this, GetFocusedTemplateID());

			// Export the persistent level and all of it's actors
			UWorld* World = Cast<UWorld>(GetPlaybackContext());
			Exporter->ExportLevelMesh(World->PersistentLevel, bSelectedOnly, NodeNameAdapter);

			// Export streaming levels and actors
			for (int32 CurLevelIndex = 0; CurLevelIndex < World->GetNumLevels(); ++CurLevelIndex)
			{
				ULevel* CurLevel = World->GetLevel(CurLevelIndex);
				if (CurLevel != NULL && CurLevel != (World->PersistentLevel))
				{
					Exporter->ExportLevelMesh(CurLevel, bSelectedOnly, NodeNameAdapter);
				}
			}

			// Export the movie scene data.
			Exporter->ExportLevelSequence(GetFocusedMovieSceneSequence()->GetMovieScene(), Bindings, this, GetFocusedTemplateID());

			// Save to disk
			Exporter->WriteToFile(*ExportFilename);
		}
	}
}


void FSequencer::GenericTextEntryModeless(const FText& DialogText, const FText& DefaultText, FOnTextCommitted OnTextComitted)
{
	TSharedRef<STextEntryPopup> TextEntryPopup = 
		SNew(STextEntryPopup)
		.Label(DialogText)
		.DefaultText(DefaultText)
		.OnTextCommitted(OnTextComitted)
		.ClearKeyboardFocusOnCommit(false)
		.SelectAllTextWhenFocused(true)
		.MaxWidth(1024.0f);

	EntryPopupMenu = FSlateApplication::Get().PushMenu(
		ToolkitHost.Pin()->GetParentWidget(),
		FWidgetPath(),
		TextEntryPopup,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
	);
}


void FSequencer::CloseEntryPopupMenu()
{
	if (EntryPopupMenu.IsValid())
	{
		EntryPopupMenu.Pin()->Dismiss();
	}
}


void FSequencer::TrimSection(bool bTrimLeft)
{
	FScopedTransaction TrimSectionTransaction( NSLOCTEXT("Sequencer", "TrimSection_Transaction", "Trim Section") );
	MovieSceneToolHelpers::TrimSection(Selection.GetSelectedSections(), GetLocalTime(), bTrimLeft);
	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}


void FSequencer::SplitSection()
{
	FScopedTransaction SplitSectionTransaction( NSLOCTEXT("Sequencer", "SplitSection_Transaction", "Split Section") );
	MovieSceneToolHelpers::SplitSection(Selection.GetSelectedSections(), GetLocalTime());
	NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}

const ISequencerEditTool* FSequencer::GetEditTool() const
{
	return SequencerWidget->GetEditTool();
}

TSharedPtr<ISequencerHotspot> FSequencer::GetHotspot() const
{
	return Hotspot;
}

void FSequencer::SetHotspot(TSharedPtr<ISequencerHotspot> NewHotspot)
{
	if (!Hotspot.IsValid() || !Hotspot->bIsLocked)
	{
		Hotspot = MoveTemp(NewHotspot);
	}
}

void FSequencer::BindCommands()
{
	const FSequencerCommands& Commands = FSequencerCommands::Get();

	SequencerCommandBindings->MapAction(
		Commands.StepToNextKey,
		FExecuteAction::CreateSP( this, &FSequencer::StepToNextKey ) );

	SequencerCommandBindings->MapAction(
		Commands.StepToPreviousKey,
		FExecuteAction::CreateSP( this, &FSequencer::StepToPreviousKey ) );

	SequencerCommandBindings->MapAction(
		Commands.StepToNextCameraKey,
		FExecuteAction::CreateSP( this, &FSequencer::StepToNextCameraKey ) );

	SequencerCommandBindings->MapAction(
		Commands.StepToPreviousCameraKey,
		FExecuteAction::CreateSP( this, &FSequencer::StepToPreviousCameraKey ) );

	SequencerCommandBindings->MapAction(
		Commands.StepToNextShot,
		FExecuteAction::CreateSP( this, &FSequencer::StepToNextShot ) );

	SequencerCommandBindings->MapAction(
		Commands.StepToPreviousShot,
		FExecuteAction::CreateSP( this, &FSequencer::StepToPreviousShot ) );

	SequencerCommandBindings->MapAction(
		Commands.SetStartPlaybackRange,
		FExecuteAction::CreateSP( this, &FSequencer::SetPlaybackRangeStart ),
		FCanExecuteAction::CreateSP( this, &FSequencer::IsViewingMasterSequence ) );

	SequencerCommandBindings->MapAction(
		Commands.ResetViewRange,
		FExecuteAction::CreateSP( this, &FSequencer::ResetViewRange ) );

	SequencerCommandBindings->MapAction(
		Commands.ZoomInViewRange,
		FExecuteAction::CreateSP( this, &FSequencer::ZoomInViewRange ),
		FCanExecuteAction(),
		EUIActionRepeatMode::RepeatEnabled );

	SequencerCommandBindings->MapAction(
		Commands.ZoomOutViewRange,
		FExecuteAction::CreateSP( this, &FSequencer::ZoomOutViewRange ),		
		FCanExecuteAction(),
		EUIActionRepeatMode::RepeatEnabled );

	SequencerCommandBindings->MapAction(
		Commands.SetEndPlaybackRange,
		FExecuteAction::CreateSP( this, &FSequencer::SetPlaybackRangeEnd ),
		FCanExecuteAction::CreateSP( this, &FSequencer::IsViewingMasterSequence ) );

	SequencerCommandBindings->MapAction(
		Commands.SetSelectionRangeToNextShot,
		FExecuteAction::CreateSP( this, &FSequencer::SetSelectionRangeToShot, true ),
		FCanExecuteAction::CreateSP( this, &FSequencer::IsViewingMasterSequence ) );

	SequencerCommandBindings->MapAction(
		Commands.SetSelectionRangeToPreviousShot,
		FExecuteAction::CreateSP( this, &FSequencer::SetSelectionRangeToShot, false ),
		FCanExecuteAction::CreateSP( this, &FSequencer::IsViewingMasterSequence ) );

	SequencerCommandBindings->MapAction(
		Commands.SetPlaybackRangeToAllShots,
		FExecuteAction::CreateSP( this, &FSequencer::SetPlaybackRangeToAllShots ),
		FCanExecuteAction::CreateSP( this, &FSequencer::IsViewingMasterSequence ) );

	SequencerCommandBindings->MapAction(
		Commands.ExpandAllNodesAndDescendants,
		FExecuteAction::CreateSP(this, &FSequencer::ExpandAllNodesAndDescendants));

	SequencerCommandBindings->MapAction(
		Commands.CollapseAllNodesAndDescendants,
		FExecuteAction::CreateSP(this, &FSequencer::CollapseAllNodesAndDescendants));

	SequencerCommandBindings->MapAction(
		Commands.ToggleExpandCollapseNodes,
		FExecuteAction::CreateSP(this, &FSequencer::ToggleExpandCollapseNodes));

	SequencerCommandBindings->MapAction(
		Commands.ToggleExpandCollapseNodesAndDescendants,
		FExecuteAction::CreateSP(this, &FSequencer::ToggleExpandCollapseNodesAndDescendants));

	SequencerCommandBindings->MapAction(
		Commands.SetKey,
		FExecuteAction::CreateSP( this, &FSequencer::SetKey ) );

	SequencerCommandBindings->MapAction(
		Commands.TranslateLeft,
		FExecuteAction::CreateSP( this, &FSequencer::TranslateSelectedKeysAndSections, true) );

	SequencerCommandBindings->MapAction(
		Commands.TranslateRight,
		FExecuteAction::CreateSP( this, &FSequencer::TranslateSelectedKeysAndSections, false) );

	SequencerCommandBindings->MapAction(
		Commands.TrimSectionLeft,
		FExecuteAction::CreateSP( this, &FSequencer::TrimSection, true ) );

	SequencerCommandBindings->MapAction(
		Commands.TrimSectionRight,
		FExecuteAction::CreateSP( this, &FSequencer::TrimSection, false ) );

	SequencerCommandBindings->MapAction(
		Commands.SplitSection,
		FExecuteAction::CreateSP( this, &FSequencer::SplitSection ) );

	// We can convert to spawnables if anything selected is a root-level possessable
	auto CanConvertToSpawnables = [this]{
		UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

		for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
		{
			if (Node->GetType() == ESequencerNode::Object)
			{
				FMovieScenePossessable* Possessable = MovieScene->FindPossessable(static_cast<FSequencerObjectBindingNode&>(*Node).GetObjectBinding());
				if (Possessable && !Possessable->GetParent().IsValid())
				{
					return true;
				}
			}
		}
		return false;
	};
	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().ConvertToSpawnable,
		FExecuteAction::CreateSP(this, &FSequencer::ConvertSelectedNodesToSpawnables),
		FCanExecuteAction::CreateLambda(CanConvertToSpawnables)
	);

	auto AreConvertableSpawnablesSelected = [this] {
		UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

		for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
		{
			if (Node->GetType() == ESequencerNode::Object)
			{
				FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(static_cast<FSequencerObjectBindingNode&>(*Node).GetObjectBinding());
				if (Spawnable && SpawnRegister->CanConvertSpawnableToPossessable(*Spawnable))
				{
					return true;
				}
			}
		}
		return false;
	};

	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().ConvertToPossessable,
		FExecuteAction::CreateSP(this, &FSequencer::ConvertSelectedNodesToPossessables),
		FCanExecuteAction::CreateLambda(AreConvertableSpawnablesSelected)
	);

	auto AreSpawnablesSelected = [this] {
		UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

		for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
		{
			if (Node->GetType() == ESequencerNode::Object)
			{
				FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(static_cast<FSequencerObjectBindingNode&>(*Node).GetObjectBinding());
				if (Spawnable)
				{
					return true;
				}
			}
		}
		return false;
	};

	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().SaveCurrentSpawnableState,
		FExecuteAction::CreateSP(this, &FSequencer::SaveSelectedNodesSpawnableState),
		FCanExecuteAction::CreateLambda(AreSpawnablesSelected)
	);

	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().RestoreAnimatedState,
		FExecuteAction::CreateSP(this, &FSequencer::RestorePreAnimatedState)
	);

	SequencerCommandBindings->MapAction(
		Commands.SetAutoKey,
		FExecuteAction::CreateLambda( [this]{ Settings->SetAutoChangeMode( EAutoChangeMode::AutoKey ); } ),
		FCanExecuteAction::CreateLambda( [this]{ return Settings->GetAllowEditsMode() != EAllowEditsMode::AllowLevelEditsOnly; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetAutoChangeMode() == EAutoChangeMode::AutoKey; } ) );

	SequencerCommandBindings->MapAction(
		Commands.SetAutoTrack,
		FExecuteAction::CreateLambda([this] { Settings->SetAutoChangeMode(EAutoChangeMode::AutoTrack); } ),
		FCanExecuteAction::CreateLambda([this] { return Settings->GetAllowEditsMode() != EAllowEditsMode::AllowLevelEditsOnly; }),
		FIsActionChecked::CreateLambda([this] { return Settings->GetAutoChangeMode() == EAutoChangeMode::AutoTrack; } ) );

	SequencerCommandBindings->MapAction(
		Commands.SetAutoChangeAll,
		FExecuteAction::CreateLambda([this] { Settings->SetAutoChangeMode(EAutoChangeMode::All); } ),
		FCanExecuteAction::CreateLambda([this] { return Settings->GetAllowEditsMode() != EAllowEditsMode::AllowLevelEditsOnly; }),
		FIsActionChecked::CreateLambda([this] { return Settings->GetAutoChangeMode() == EAutoChangeMode::All; } ) );
	
	SequencerCommandBindings->MapAction(
		Commands.SetAutoChangeNone,
		FExecuteAction::CreateLambda([this] { Settings->SetAutoChangeMode(EAutoChangeMode::None); } ),
		FCanExecuteAction::CreateLambda([this] { return Settings->GetAllowEditsMode() != EAllowEditsMode::AllowLevelEditsOnly; }),
		FIsActionChecked::CreateLambda([this] { return Settings->GetAutoChangeMode() == EAutoChangeMode::None; } ) );

	SequencerCommandBindings->MapAction(
		Commands.AllowAllEdits,
		FExecuteAction::CreateLambda( [this]{ Settings->SetAllowEditsMode( EAllowEditsMode::AllEdits ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetAllowEditsMode() == EAllowEditsMode::AllEdits; } ) );

	SequencerCommandBindings->MapAction(
		Commands.AllowSequencerEditsOnly,
		FExecuteAction::CreateLambda([this] { Settings->SetAllowEditsMode(EAllowEditsMode::AllowSequencerEditsOnly); }),
		FCanExecuteAction::CreateLambda([] { return true; }),
		FIsActionChecked::CreateLambda([this] { return Settings->GetAllowEditsMode() == EAllowEditsMode::AllowSequencerEditsOnly; }));

	SequencerCommandBindings->MapAction(
		Commands.AllowLevelEditsOnly,
		FExecuteAction::CreateLambda([this] { Settings->SetAllowEditsMode(EAllowEditsMode::AllowLevelEditsOnly); }),
		FCanExecuteAction::CreateLambda([] { return true; }),
		FIsActionChecked::CreateLambda([this] { return Settings->GetAllowEditsMode() == EAllowEditsMode::AllowLevelEditsOnly; }));

	SequencerCommandBindings->MapAction(
		Commands.ToggleAutoKeyEnabled,
		FExecuteAction::CreateLambda( [this]{ Settings->SetAutoChangeMode(Settings->GetAutoChangeMode() == EAutoChangeMode::None ? EAutoChangeMode::AutoKey : EAutoChangeMode::None); } ),
		FCanExecuteAction::CreateLambda( [this]{ return Settings->GetAllowEditsMode() != EAllowEditsMode::AllowLevelEditsOnly; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetAutoChangeMode() == EAutoChangeMode::AutoKey; } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleKeyAllEnabled,
		FExecuteAction::CreateLambda( [this]{ Settings->SetKeyAllEnabled( !Settings->GetKeyAllEnabled() ); } ),
		FCanExecuteAction::CreateLambda( [this]{ return Settings->GetAllowEditsMode() != EAllowEditsMode::AllowLevelEditsOnly; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetKeyAllEnabled(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleAutoScroll,
		FExecuteAction::CreateLambda( [this]{ Settings->SetAutoScrollEnabled( !Settings->GetAutoScrollEnabled() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetAutoScrollEnabled(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.FindInContentBrowser,
		FExecuteAction::CreateSP( this, &FSequencer::FindInContentBrowser ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleCombinedKeyframes,
		FExecuteAction::CreateLambda( [this]{
			Settings->SetShowCombinedKeyframes( !Settings->GetShowCombinedKeyframes() );
		} ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetShowCombinedKeyframes(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleChannelColors,
		FExecuteAction::CreateLambda( [this]{
			Settings->SetShowChannelColors( !Settings->GetShowChannelColors() );
		} ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetShowChannelColors(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleLabelBrowser,
		FExecuteAction::CreateLambda( [this]{
			Settings->SetLabelBrowserVisible( !Settings->GetLabelBrowserVisible() );
		} ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetLabelBrowserVisible(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleShowFrameNumbers,
		FExecuteAction::CreateLambda( [this]{ Settings->SetShowFrameNumbers( !Settings->GetShowFrameNumbers() ); } ),
		FCanExecuteAction::CreateSP( this, &FSequencer::CanShowFrameNumbers ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetShowFrameNumbers(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleShowRangeSlider,
		FExecuteAction::CreateLambda( [this]{ Settings->SetShowRangeSlider( !Settings->GetShowRangeSlider() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetShowRangeSlider(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleIsSnapEnabled,
		FExecuteAction::CreateLambda( [this]{ Settings->SetIsSnapEnabled( !Settings->GetIsSnapEnabled() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetIsSnapEnabled(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapKeyTimesToInterval,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapKeyTimesToInterval( !Settings->GetSnapKeyTimesToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapKeyTimesToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapKeyTimesToKeys,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapKeyTimesToKeys( !Settings->GetSnapKeyTimesToKeys() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapKeyTimesToKeys(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapSectionTimesToInterval,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapSectionTimesToInterval( !Settings->GetSnapSectionTimesToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapSectionTimesToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapSectionTimesToSections,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapSectionTimesToSections( !Settings->GetSnapSectionTimesToSections() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapSectionTimesToSections(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapPlayTimeToKeys,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapPlayTimeToKeys( !Settings->GetSnapPlayTimeToKeys() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapPlayTimeToKeys(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapPlayTimeToInterval,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapPlayTimeToInterval( !Settings->GetSnapPlayTimeToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapPlayTimeToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapPlayTimeToPressedKey,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapPlayTimeToPressedKey( !Settings->GetSnapPlayTimeToPressedKey() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapPlayTimeToPressedKey(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapPlayTimeToDraggedKey,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapPlayTimeToDraggedKey( !Settings->GetSnapPlayTimeToDraggedKey() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapPlayTimeToDraggedKey(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapCurveValueToInterval,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapCurveValueToInterval( !Settings->GetSnapCurveValueToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapCurveValueToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleShowCurveEditor,
		FExecuteAction::CreateLambda( [this]{ SetShowCurveEditor(!GetShowCurveEditor()); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return GetShowCurveEditor(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleLinkCurveEditorTimeRange,
		FExecuteAction::CreateLambda( [this]{ Settings->SetLinkCurveEditorTimeRange(!Settings->GetLinkCurveEditorTimeRange()); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetLinkCurveEditorTimeRange(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleShowPreAndPostRoll,
		FExecuteAction::CreateLambda( [this]{ Settings->SetShouldShowPrePostRoll(!Settings->ShouldShowPrePostRoll()); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->ShouldShowPrePostRoll(); } ) );

	auto CanCutOrCopy = [this]{
		// For copy tracks
		TArray<TSharedPtr<FSequencerTrackNode>> TracksToCopy;
		TSet<TSharedRef<FSequencerDisplayNode>> SelectedNodes = Selection.GetNodesWithSelectedKeysOrSections();
		// If this is empty then we are selecting display nodes
		if (SelectedNodes.Num() == 0)
		{
			SelectedNodes = Selection.GetSelectedOutlinerNodes();
			for (TSharedRef<FSequencerDisplayNode> Node : SelectedNodes)
			{
				if (Node->GetType() == ESequencerNode::Track)
				{
					// if contains one node that can be copied we allow the action
					// later on we will filter out the invalid nodes in CopySelection() or CutSelection()
					return true;
				}
				else if (Node->GetParent().IsValid() && Node->GetParent()->GetType() == ESequencerNode::Track)
				{
					// Although copying only the child nodes (ex. translation) is not allowed, we still show the copy & cut button
					// so that users are not misled and can achieve this in copy/cut the parent node (ex. transform)
					return true;
				}
			}
			return false;
		}

		UMovieSceneTrack* Track = nullptr;
		for (FSequencerSelectedKey Key : Selection.GetSelectedKeys())
		{
			if (!Track)
			{
				Track = Key.Section->GetTypedOuter<UMovieSceneTrack>();
			}
			if (!Track || Track != Key.Section->GetTypedOuter<UMovieSceneTrack>())
			{
				return false;
			}
		}
		return true;
	};

	auto CanDelete = [this]{
		return Selection.GetSelectedKeys().Num() || Selection.GetSelectedSections().Num() || Selection.GetSelectedOutlinerNodes().Num();
	};

	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Cut,
		FExecuteAction::CreateSP(this, &FSequencer::CutSelection),
		FCanExecuteAction::CreateLambda(CanCutOrCopy)
	);

	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &FSequencer::CopySelection),
		FCanExecuteAction::CreateLambda(CanCutOrCopy)
	);

	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP( this, &FSequencer::DeleteSelectedItems ),
		FCanExecuteAction::CreateLambda(CanDelete));

	SequencerCommandBindings->MapAction(
		Commands.TogglePlaybackRangeLocked,
		FExecuteAction::CreateSP( this, &FSequencer::TogglePlaybackRangeLocked ),
		FCanExecuteAction::CreateLambda( [this] { return GetFocusedMovieSceneSequence() != nullptr;	} ),
		FIsActionChecked::CreateSP( this, &FSequencer::IsPlaybackRangeLocked ));

	SequencerCommandBindings->MapAction(
		Commands.ToggleForceFixedFrameIntervalPlayback,
		FExecuteAction::CreateLambda( [this] 
		{
			UMovieSceneSequence* FocusedMovieSceneSequence = GetFocusedMovieSceneSequence();
			if ( FocusedMovieSceneSequence != nullptr )
			{
				FScopedTransaction ToggleForceFixedFrameIntervalPlaybackTransaction( NSLOCTEXT( "Sequencer", "ToggleForceFixedFrameIntervalPlaybackTransaction", "Toggle force fixed frame interval playback" ) );
				UMovieScene* MovieScene = FocusedMovieSceneSequence->GetMovieScene();
				MovieScene->Modify();
				MovieScene->SetForceFixedFrameIntervalPlayback( !MovieScene->GetForceFixedFrameIntervalPlayback() );
				if ( MovieScene->GetForceFixedFrameIntervalPlayback() && MovieScene->GetFixedFrameInterval() == 0 )
				{
					MovieScene->SetFixedFrameInterval( Settings->GetTimeSnapInterval() );
				}
			} 
		} ),
		FCanExecuteAction::CreateLambda( [this] { return GetFocusedMovieSceneSequence() != nullptr;	} ),
		FIsActionChecked::CreateLambda( [this] 
		{ 
			UMovieSceneSequence* FocusedMovieSceneSequence = GetFocusedMovieSceneSequence();
			return FocusedMovieSceneSequence != nullptr
				? FocusedMovieSceneSequence->GetMovieScene()->GetForceFixedFrameIntervalPlayback()
				: false;
		} ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleRerunConstructionScripts,
		FExecuteAction::CreateLambda( [this]{ Settings->SetRerunConstructionScripts( !Settings->ShouldRerunConstructionScripts() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->ShouldRerunConstructionScripts(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleKeepCursorInPlaybackRangeWhileScrubbing,
		FExecuteAction::CreateLambda( [this]{ Settings->SetKeepCursorInPlayRangeWhileScrubbing( !Settings->ShouldKeepCursorInPlayRangeWhileScrubbing() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->ShouldKeepCursorInPlayRangeWhileScrubbing(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleKeepCursorInPlaybackRange,
		FExecuteAction::CreateLambda( [this]{ Settings->SetKeepCursorInPlayRange( !Settings->ShouldKeepCursorInPlayRange() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->ShouldKeepCursorInPlayRange(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleKeepPlaybackRangeInSectionBounds,
		FExecuteAction::CreateLambda( [this]{ Settings->SetKeepPlayRangeInSectionBounds( !Settings->ShouldKeepPlayRangeInSectionBounds() ); NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->ShouldKeepPlayRangeInSectionBounds(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleEvaluateSubSequencesInIsolation,
		FExecuteAction::CreateLambda( [this]{
			Settings->SetEvaluateSubSequencesInIsolation( !Settings->ShouldEvaluateSubSequencesInIsolation() );
			FMovieSceneEvaluationRange Range = PlayPosition.JumpTo(ScrubPosition, GetFocusedMovieSceneSequence()->GetMovieScene()->GetOptionalFixedFrameInterval());
			EvaluateInternal(Range);
		} ),
		FCanExecuteAction::CreateLambda( [this]{ return ActiveTemplateIDs.Num() > 1; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->ShouldEvaluateSubSequencesInIsolation(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.RenderMovie,
		FExecuteAction::CreateLambda([this]{ RenderMovieInternal(GetPlaybackRange().GetLowerBoundValue(), GetPlaybackRange().GetUpperBoundValue()); }),
		FCanExecuteAction(),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateLambda([this]{ return ExactCast<ULevelSequence>(GetFocusedMovieSceneSequence()) != nullptr; })
	);

	SequencerCommandBindings->MapAction(
		Commands.CreateCamera,
		FExecuteAction::CreateSP(this, &FSequencer::CreateCamera),
		FCanExecuteAction(),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateLambda([this] { return ExactCast<ULevelSequence>(GetFocusedMovieSceneSequence()) != nullptr && IVREditorModule::Get().IsVREditorModeActive() == false; }) //@todo VREditor: Creating a camera while in VR mode disrupts the hmd. This is a temporary fix by hiding the button when in VR mode.
	);

	SequencerCommandBindings->MapAction(
		Commands.DiscardChanges,
		FExecuteAction::CreateSP(this, &FSequencer::DiscardChanges),
		FCanExecuteAction::CreateLambda([this]{
			UMovieSceneSequence* EditedSequence = GetFocusedMovieSceneSequence();
			if (!EditedSequence)
			{
				return false;
			}

			UPackage* EditedPackage = EditedSequence->GetOutermost();

			return ((EditedPackage->FileSize != 0) && EditedPackage->IsDirty());
		})
	);

	SequencerCommandBindings->MapAction(
		Commands.FixActorReferences,
		FExecuteAction::CreateSP( this, &FSequencer::FixActorReferences ),
		FCanExecuteAction::CreateLambda( []{ return true; } ) );

	SequencerCommandBindings->MapAction(
		Commands.RebindPossessableReferences,
		FExecuteAction::CreateSP( this, &FSequencer::RebindPossessableReferences ),
		FCanExecuteAction::CreateLambda( []{ return true; } ) );

	SequencerCommandBindings->MapAction(
		Commands.FixFrameTiming,
		FExecuteAction::CreateSP( this, &FSequencer::FixFrameTiming ),
		FCanExecuteAction::CreateLambda( [] { return true; } ) );

	SequencerCommandBindings->MapAction(
		Commands.ImportFBX,
		FExecuteAction::CreateSP( this, &FSequencer::ImportFBX ),
		FCanExecuteAction::CreateLambda( [] { return true; } ) );

	SequencerCommandBindings->MapAction(
		Commands.ExportFBX,
		FExecuteAction::CreateSP( this, &FSequencer::ExportFBX ),
		FCanExecuteAction::CreateLambda( [] { return true; } ) );

	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		TrackEditors[i]->BindCommands(SequencerCommandBindings);
	}

	// copy subset of sequencer commands to shared commands
	*SequencerSharedBindings = *SequencerCommandBindings;

	// Sequencer-only bindings
	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationCubicAuto,
		FExecuteAction::CreateSP(this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Cubic, ERichCurveTangentMode::RCTM_Auto));

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationCubicUser,
		FExecuteAction::CreateSP(this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Cubic, ERichCurveTangentMode::RCTM_User));

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationCubicBreak,
		FExecuteAction::CreateSP(this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Cubic, ERichCurveTangentMode::RCTM_Break));

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationLinear,
		FExecuteAction::CreateSP(this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Linear, ERichCurveTangentMode::RCTM_Auto));

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationConstant,
		FExecuteAction::CreateSP(this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Constant, ERichCurveTangentMode::RCTM_Auto));

	SequencerCommandBindings->MapAction(
		Commands.TogglePlay,
		FExecuteAction::CreateSP( this, &FSequencer::TogglePlay ));

	SequencerCommandBindings->MapAction(
		Commands.PlayForward,
		FExecuteAction::CreateSP( this, &FSequencer::PlayForward ));

	SequencerCommandBindings->MapAction(
		Commands.JumpToStart,
		FExecuteAction::CreateSP( this, &FSequencer::JumpToStart ));

	SequencerCommandBindings->MapAction(
		Commands.JumpToEnd,
		FExecuteAction::CreateSP( this, &FSequencer::JumpToEnd ));

	SequencerCommandBindings->MapAction(
		Commands.ShuttleForward,
		FExecuteAction::CreateSP( this, &FSequencer::ShuttleForward ));

	SequencerCommandBindings->MapAction(
		Commands.ShuttleBackward,
		FExecuteAction::CreateSP( this, &FSequencer::ShuttleBackward ));

	SequencerCommandBindings->MapAction(
		Commands.Pause,
		FExecuteAction::CreateSP( this, &FSequencer::Pause ));

	SequencerCommandBindings->MapAction(
		Commands.StepForward,
		FExecuteAction::CreateSP( this, &FSequencer::StepForward ),
		EUIActionRepeatMode::RepeatEnabled );

	SequencerCommandBindings->MapAction(
		Commands.StepBackward,
		FExecuteAction::CreateSP( this, &FSequencer::StepBackward ),
		EUIActionRepeatMode::RepeatEnabled );

	SequencerCommandBindings->MapAction(
		Commands.SetSelectionRangeEnd,
		FExecuteAction::CreateLambda([this]{ SetSelectionRangeEnd(); }));

	SequencerCommandBindings->MapAction(
		Commands.SetSelectionRangeStart,
		FExecuteAction::CreateLambda([this]{ SetSelectionRangeStart(); }));

	SequencerCommandBindings->MapAction(
		Commands.ResetSelectionRange,
		FExecuteAction::CreateLambda([this]{ ResetSelectionRange(); }));

	SequencerCommandBindings->MapAction(
		Commands.SelectKeysInSelectionRange,
		FExecuteAction::CreateSP(this, &FSequencer::SelectInSelectionRange, true, false));

	SequencerCommandBindings->MapAction(
		Commands.SelectSectionsInSelectionRange,
		FExecuteAction::CreateSP(this, &FSequencer::SelectInSelectionRange, false, true));

	SequencerCommandBindings->MapAction(
		Commands.SelectAllInSelectionRange,
		FExecuteAction::CreateSP(this, &FSequencer::SelectInSelectionRange, true, true));

	// bind widget specific commands
	SequencerWidget->BindCommands(SequencerCommandBindings);
}

void FSequencer::BuildAddTrackMenu(class FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT( "AddFolder", "Add Folder" ),
		LOCTEXT( "AddFolderToolTip", "Adds a new folder." ),
		FSlateIcon( FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetTreeFolderOpen" ),
		FUIAction( FExecuteAction::CreateRaw( this, &FSequencer::OnAddFolder ) ) );

	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		if (TrackEditors[i]->SupportsSequence(GetFocusedMovieSceneSequence()))
		{
			TrackEditors[i]->BuildAddTrackMenu(MenuBuilder);
		}
	}
}


void FSequencer::BuildAddObjectBindingsMenu(class FMenuBuilder& MenuBuilder)
{
	for (int32 i = 0; i < ObjectBindings.Num(); ++i)
	{
		if (ObjectBindings[i]->SupportsSequence(GetFocusedMovieSceneSequence()))
		{
			ObjectBindings[i]->BuildSequencerAddMenu(MenuBuilder);
		}
	}
}

void FSequencer::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		TrackEditors[i]->BuildObjectBindingTrackMenu(MenuBuilder, ObjectBinding, ObjectClass);
	}
}


void FSequencer::BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		TrackEditors[i]->BuildObjectBindingEditButtons(EditBox, ObjectBinding, ObjectClass);
	}
}

void FSequencer::ResetTimingManager(bool bInShouldLockToAudioClock)
{
	if (bInShouldLockToAudioClock && GEngine->GetMainAudioDevice())
	{
		TimingManager.Reset(new FSequencerAudioClockTimer);
	}
	else
	{
		TimingManager.Reset(new FSequencerDefaultTimingManager);
	}
	TimingManager->Update(PlaybackState, GetGlobalTime());
}

#undef LOCTEXT_NAMESPACE
