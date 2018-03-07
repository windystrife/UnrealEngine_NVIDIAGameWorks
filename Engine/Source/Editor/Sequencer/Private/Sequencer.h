// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Misc/Guid.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "SequencerNodeTree.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "UObject/GCObject.h"
#include "MovieSceneSequenceID.h"
#include "IMovieScenePlayer.h"
#include "ITimeSlider.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Animation/CurveHandle.h"
#include "Animation/CurveSequence.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "TickableEditorObject.h"
#include "EditorUndoClient.h"
#include "KeyPropertyParams.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "ISequencerObjectChangeListener.h"
#include "SequencerSelection.h"
#include "SequencerSelectionPreview.h"
#include "Editor/EditorWidgets/Public/ITransportControl.h"
#include "SequencerLabelManager.h"
#include "Evaluation/MovieSceneSequenceTransform.h"
#include "Evaluation/MovieScenePlayback.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "LevelEditor.h"
#include "SequencerTimingManager.h"
#include "AcquiredResources.h"
#include "SequencerSettings.h"

class AActor;
class ACineCameraActor;
class APlayerController;
class FLevelEditorViewportClient;
class FMenuBuilder;
class FMovieSceneClipboard;
class FSequencerObjectBindingNode;
class FSequencerTrackNode;
class FViewportClient;
class IDetailKeyframeHandler;
class ILevelViewport;
class IMenu;
class ISequencerEditTool;
class ISequencerKeyCollection;
class ISequencerTrackEditor;
class ISequencerEditorObjectBinding;
class SSequencer;
class ULevel;
class UMovieSceneSequence;
class UMovieSceneSubSection;
class USequencerSettings;
struct FMovieScenePossessable;
struct FSequencerTemplateStore;
struct FTransformData;
struct ISequencerHotspot;
enum class EMapChangeType : uint8;

/**
 * Sequencer is the editing tool for MovieScene assets.
 */
class FSequencer
	: public ISequencer
	, public FGCObject
	, public FEditorUndoClient
	, public FTickableEditorObject
{
public:

	/** Constructor */
	FSequencer();

	/** Virtual destructor */
	virtual ~FSequencer();

public:

	/**
	 * Initializes sequencer
	 *
	 * @param InitParams Initialization parameters.
	 * @param InObjectChangeListener The object change listener to use.
	 * @param TrackEditorDelegates Delegates to call to create auto-key handlers for this sequencer.
	 * @param EditorObjectBindingDelegates Delegates to call to create object bindings for this sequencer.
	 */
	void InitSequencer(const FSequencerInitParams& InitParams, const TSharedRef<ISequencerObjectChangeListener>& InObjectChangeListener, const TArray<FOnCreateTrackEditor>& TrackEditorDelegates, const TArray<FOnCreateEditorObjectBinding>& EditorObjectBindingDelegates);

	/** @return The current view range */
	virtual FAnimatedRange GetViewRange() const override;
	virtual void SetViewRange(TRange<float> NewViewRange, EViewRangeInterpolation Interpolation = EViewRangeInterpolation::Animated) override;

	/** @return The current clamp range */
	FAnimatedRange GetClampRange() const;
	void SetClampRange(TRange<float> InNewClampRange);

public:

	/**
	 * Get the selection range.
	 *
	 * @return The selection range.
	 * @see SetSelectionRange, SetSelectionRangeEnd, SetSelectionRangeStart
	 */
	TRange<float> GetSelectionRange() const;

	/**
	 * Set the selection selection range.
	 *
	 * @param Range The new range to set.
	 * @see GetSelectionRange, SetSelectionRangeEnd, SetSelectionRangeStart
	 */
	void SetSelectionRange(TRange<float> Range);
	
	/**
	 * Set the selection range's end position to the current global time.
	 *
	 * @see GetSelectionRange, SetSelectionRange, SetSelectionRangeStart
	 */
	void SetSelectionRangeEnd();
	
	/**
	 * Set the selection range's start position to the current global time.
	 *
	 * @see GetSelectionRange, SetSelectionRange, SetSelectionRangeEnd
	 */
	void SetSelectionRangeStart();

	/** Clear and reset the selection range. */
	void ResetSelectionRange();

	/** Select all keys that fall into the current selection range. */
	void SelectInSelectionRange(bool bSelectKeys, bool bSelectSections);

	/**
	 * Get the currently viewed sub sequence range
	 *
	 * @return The sub sequence range, or an empty optional if we're viewing the root.
	 */
	TOptional<TRange<float>> GetSubSequenceRange() const;

public:

	/**
	 * Get the playback range.
	 *
	 * @return Playback range.
	 * @see SetPlaybackRange, SetPlaybackRangeEnd, SetPlaybackRangeStart
	 */
	TRange<float> GetPlaybackRange() const;

	/**
	 * Set the playback range.
	 *
	 * @param Range The new range to set.
	 * @see GetPlaybackRange, SetPlaybackRangeEnd, SetPlaybackRangeStart
	 */
	void SetPlaybackRange(TRange<float> Range);
	
	/**
	 * Set the playback range's end position to the current global time.
	 *
	 * @see GetPlaybackRange, SetPlaybackRange, SetPlaybackRangeStart
	 */
	void SetPlaybackRangeEnd()
	{
		SetPlaybackRange(TRange<float>(GetPlaybackRange().GetLowerBoundValue(), GetLocalTime()));
	}

	/**
	 * Set the playback range's start position to the current global time.
	 *
	 * @see GetPlaybackRange, SetPlaybackRange, SetPlaybackRangeStart
	 */
	void SetPlaybackRangeStart()
	{
		SetPlaybackRange(TRange<float>(GetLocalTime(), GetPlaybackRange().GetUpperBoundValue()));
	}

	/**
	 * Set the selection range to the next or previous shot's range.
	 *
	 */	
	void SetSelectionRangeToShot(const bool bNextShot);

	/**
	 * Set the playback range to all the shot's playback ranges.
	 *
	 */	
	void SetPlaybackRangeToAllShots();

public:

	bool IsPlaybackRangeLocked() const;
	void TogglePlaybackRangeLocked();
	void ResetViewRange();
	void ZoomViewRange(float InZoomDelta);
	void ZoomInViewRange();
	void ZoomOutViewRange();

public:

	/** Access the user-supplied settings object */
	USequencerSettings* GetSettings() const
	{
		return Settings;
	}

	/** Gets the tree of nodes which is used to populate the animation outliner. */
	TSharedRef<FSequencerNodeTree> GetNodeTree()
	{
		return NodeTree;
	}

	bool IsPerspectiveViewportPossessionEnabled() const override
	{
		return bPerspectiveViewportPossessionEnabled;
	}

	bool IsPerspectiveViewportCameraCutEnabled() const override
	{
		return bPerspectiveViewportCameraCutEnabled;
	}

	/**
	 * Pops the current focused movie scene from the stack.  The parent of this movie scene will be come the focused one
	 */
	void PopToSequenceInstance( FMovieSceneSequenceIDRef SequenceID );

	/** Deletes the passed in sections. */
	void DeleteSections(const TSet<TWeakObjectPtr<UMovieSceneSection> > & Sections);

	/** Deletes the currently selected in keys. */
	void DeleteSelectedKeys();

	/** Set interpolation modes. */
	void SetInterpTangentMode(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode);

	/** Is interpolation mode selected. */
	bool IsInterpTangentModeSelected(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode) const;

	/** Get the fixed frame interval of the current movie scene. */
	float GetFixedFrameInterval() const;

	/** Snap the currently selected keys to frame. */
	void SnapToFrame();

	/** Are there keys to snap? */
	bool CanSnapToFrame() const;

	/** Transform the selected keys and sections */
	void TransformSelectedKeysAndSections(float InDeltaTime, float InScale);

	/** Translate the selected keys and section by the time snap interval */
	void TranslateSelectedKeysAndSections(bool bTranslateLeft);

	/**
	 * @return Movie scene tools used by the sequencer
	 */
	const TArray<TSharedPtr<ISequencerTrackEditor>>& GetTrackEditors() const
	{
		return TrackEditors;
	}

public:

	/**
	 * Converts the specified possessable GUID to a spawnable
	 *
	 * @param	PossessableGuid		The guid of the possessable to convert
	 */
	void ConvertToSpawnable(TSharedRef<FSequencerObjectBindingNode> NodeToBeConverted);

	/**
	 * Converts the specified spawnable GUID to a possessable
	 *
	 * @param	SpawnableGuid		The guid of the spawnable to convert
	 */
	void ConvertToPossessable(TSharedRef<FSequencerObjectBindingNode> NodeToBeConverted);

	/**
	 * Converts all the currently selected nodes to be spawnables, if possible
	 */
	void ConvertSelectedNodesToSpawnables();

	/**
	 * Converts all the currently selected nodes to be possessables, if possible
	 */
	void ConvertSelectedNodesToPossessables();

protected:

	/**
	 * Attempts to add a new spawnable to the MovieScene for the specified asset, class, or actor
	 *
	 * @param	Object	The asset, class, or actor to add a spawnable for
	 *
	 * @return	The spawnable ID, or invalid ID on failure
	 */
	FGuid AddSpawnable( UObject& Object );

	/**
	 * Save default spawnable state for the currently selected objects
	 */
	void SaveSelectedNodesSpawnableState();

public:

	/** Called when new actors are dropped in the viewport. */
	void OnNewActorsDropped(const TArray<UObject*>& DroppedObjects, const TArray<AActor*>& DroppedActors);

	/**
	 * Call when an asset is dropped into the sequencer. Will proprogate this
	 * to all tracks, and potentially consume this asset
	 * so it won't be added as a spawnable
	 *
	 * @param DroppedAsset		The asset that is dropped in
	 * @param TargetObjectGuid	Object to be targeted on dropping
	 * @return					If true, this asset should be consumed
	 */
	virtual bool OnHandleAssetDropped( UObject* DroppedAsset, const FGuid& TargetObjectGuid );
	
	/**
	 * Called to delete all moviescene data from a node
	 *
	 * @param NodeToBeDeleted	Node with data that should be deleted
	 * @return true if anything was deleted, otherwise false.
	 */
	virtual bool OnRequestNodeDeleted( TSharedRef<const FSequencerDisplayNode> NodeToBeDeleted );

	/** Zooms to the edges of all currently selected sections. */
	void ZoomToSelectedSections();

	/** Gets the overlay fading animation curve lerp. */
	float GetOverlayFadeCurve() const;

	/** Gets the command bindings for the sequencer */
	virtual TSharedPtr<FUICommandList> GetCommandBindings(ESequencerCommandBindings Type = ESequencerCommandBindings::Sequencer) const override
	{
		if (Type == ESequencerCommandBindings::Sequencer)
		{
			return SequencerCommandBindings;
		}

		return SequencerSharedBindings;
	}

	/**
	 * Builds up the sequencer's "Add Track" menu.
	 *
	 * @param MenuBuilder The menu builder to add things to.
	 */
	void BuildAddTrackMenu(FMenuBuilder& MenuBuilder);

	/**
	 * Builds up the object bindings in sequencer's "Add Track" menu.
	 *
	 * @param MenuBuilder The menu builder to add things to.
	 */
	void BuildAddObjectBindingsMenu(FMenuBuilder& MenuBuilder);

	/**
	 * Builds up the track menu for object binding nodes in the outliner
	 * 
	 * @param MenuBuilder	The track menu builder to add things to
	 * @param ObjectBinding	The object binding of the selected node
	 * @param ObjectClass	The class of the selected object
	 */
	void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass);

	/**
	 * Builds up the edit buttons for object binding nodes in the outliner
	 * 
	 * @param EditBox	    The edit box to add things to
	 * @param ObjectBinding	The object binding of the selected node
	 * @param ObjectClass	The class of the selected object
	 */
	void BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectBinding, const UClass* ObjectClass);

	/** Called when an actor is dropped into Sequencer */
	void OnActorsDropped( const TArray<TWeakObjectPtr<AActor> >& Actors );

	void RecordSelectedActors();

	/** Functions to push on to the transport controls we use */
	FReply OnRecord();
	FReply OnStepForward();
	FReply OnStepBackward();
	FReply OnJumpToStart();
	FReply OnJumpToEnd();
	FReply OnCycleLoopMode();
	FReply SetPlaybackEnd();
	FReply SetPlaybackStart();
	FReply JumpToPreviousKey();
	FReply JumpToNextKey();

	/** Get the visibility of the record button */
	EVisibility GetRecordButtonVisibility() const;

	/** Delegate handler for recording starting */
	void HandleRecordingStarted(UMovieSceneSequence* Sequence);

	/** Delegate handler for recording finishing */
	void HandleRecordingFinished(UMovieSceneSequence* Sequence);

	/** Set the new global time, accounting for looping options */
	void SetLocalTimeLooped(float InTime);

	float AutoScroll(float InTime, ESnapTimeMode SnapTimeMode);

	ESequencerLoopMode GetLoopMode() const;

	EPlaybackMode::Type GetPlaybackMode() const;

	bool IsRecording() const;

	/** Called to determine whether a frame number is set so that frame numbers can be shown */
	bool CanShowFrameNumbers() const;

	/** @return The toolkit that this sequencer is hosted in (if any) */
	TSharedPtr<IToolkitHost> GetToolkitHost() const { return ToolkitHost.Pin(); }

	/** @return Whether or not this sequencer is used in the level editor */
	bool IsLevelEditorSequencer() const { return bIsEditingWithinLevelEditor; }

	/** @return Whether to show the curve editor or not */
	void SetShowCurveEditor(bool bInShowCurveEditor);
	bool GetShowCurveEditor() const { return bShowCurveEditor; }

	/** Called to save the current movie scene */
	void SaveCurrentMovieScene();

	/** Called to save the current movie scene under a new name */
	void SaveCurrentMovieSceneAs();

	/** Called when a user executes the assign actor to track menu item */
	void AssignActor(FMenuBuilder& MenuBuilder, FGuid ObjectBinding);
	FGuid DoAssignActor(AActor*const* InActors, int32 NumActors, FGuid ObjectBinding);

	/** Called when a user executes the delete node menu item */
	void DeleteNode(TSharedRef<FSequencerDisplayNode> NodeToBeDeleted);
	void DeleteSelectedNodes();

	/** Called when a user executes the copy track menu item */
	void CopySelectedTracks(TArray<TSharedPtr<FSequencerTrackNode>>& TrackNodes);
	void ExportTracksToText(TArray<UMovieSceneTrack*> TrackToExport, /*out*/ FString& ExportedText);

	/** Called when a user executes the paste track menu item */
	bool CanPaste(const FString& TextToImport) const;
	void PasteCopiedTracks();
	void ImportTracksFromText(const FString& TextToImport, /*out*/ TArray<UMovieSceneTrack*>& ImportedTrack);

	/** Called when a user executes the active node menu item */
	void ToggleNodeActive();
	bool IsNodeActive() const;

	/** Called when a user executes the locked node menu item */
	void ToggleNodeLocked();
	bool IsNodeLocked() const;

	/** Called when a user executes the set key time for selected keys */
	bool CanSetKeyTime() const;
	void SetKeyTime(const bool bUseFrames);
	void OnSetKeyTimeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, const bool bUseFrames);

	FSequencerLabelManager& GetLabelManager()
	{
		return LabelManager;
	}

	/** Select keys belonging to a section at the key time */
	void SelectTrackKeys(TWeakObjectPtr<UMovieSceneSection> Section, float KeyTime, bool bAddToSelection, bool bToggleSelection);

	/** Updates the external selection to match the current sequencer selection. */
	void SynchronizeExternalSelectionWithSequencerSelection();

	/** Updates the sequencer selection to match the current external selection. */
	void SynchronizeSequencerSelectionWithExternalSelection();

public:

	/** Copy the selection, whether it's keys or tracks */
	void CopySelection();

	/** Cut the selection, whether it's keys or tracks */
	void CutSelection();

	/** Copy the selected keys to the clipboard */
	void CopySelectedKeys();

	/** Copy the selected keys to the clipboard, then delete them as part of an undoable transaction */
	void CutSelectedKeys();

	/** Get the in-memory clipboard stack */
	const TArray<TSharedPtr<FMovieSceneClipboard>>& GetClipboardStack() const;

	/** Promote a clipboard to the top of the clipboard stack, and update its timestamp */
	void OnClipboardUsed(TSharedPtr<FMovieSceneClipboard> Clipboard);

	/** Discard all changes to the current movie scene. */
	void DiscardChanges();

	/** Create camera and set it as the current camera cut. */
	void CreateCamera();

	/** Called when a new camera is added */
	void NewCameraAdded(ACineCameraActor* NewCamera, FGuid CameraGuid, bool bLockToCamera);

	/** Attempts to automatically fix up broken actor references in the current scene. */
	void FixActorReferences();

	/** Rebinds all possessable references in the current sequence to update them to the latest referencing mechanism. */
	void RebindPossessableReferences();

	/** Moves all time data for the current scene onto a valid frame. */
	void FixFrameTiming();

	/** Imports the animation from an fbx file. */
	void ImportFBX();

	/** Exports the animation to an fbx file. */
	void ExportFBX();

public:
	
	/** Access the currently active track area edit tool */
	const ISequencerEditTool* GetEditTool() const;

	/** Get the current active hotspot */
	TSharedPtr<ISequencerHotspot> GetHotspot() const;

	/** Set the hotspot to something else */
	void SetHotspot(TSharedPtr<ISequencerHotspot> NewHotspot);

protected:

	/** The current hotspot that can be set from anywhere to initiate drags */
	TSharedPtr<ISequencerHotspot> Hotspot;

public:

	/** Put the sequencer in a horizontally auto-scrolling state with the given rate */
	void StartAutoscroll(float UnitsPerS);

	/** Stop the sequencer from auto-scrolling */
	void StopAutoscroll();

	/** Scroll the sequencer vertically by the specified number of slate units */
	void VerticalScroll(float ScrollAmountUnits);

public:

	//~ FGCObject Interface

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

public:

	//~ FTickableEditorObject Interface

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FSequencer, STATGROUP_Tickables); };

public:

	//~ ISequencer Interface

	virtual void Close() override;
	virtual TSharedRef<SWidget> GetSequencerWidget() const override;
	virtual FMovieSceneSequenceIDRef GetRootTemplateID() const override { return ActiveTemplateIDs[0]; }
	virtual FMovieSceneSequenceIDRef GetFocusedTemplateID() const override { return ActiveTemplateIDs.Top(); }
	virtual UMovieSceneSequence* GetRootMovieSceneSequence() const override;
	virtual UMovieSceneSequence* GetFocusedMovieSceneSequence() const override;
	virtual FMovieSceneRootEvaluationTemplateInstance& GetEvaluationTemplate() override { return RootTemplateInstance; }
	virtual void ResetToNewRootSequence(UMovieSceneSequence& NewSequence) override;
	virtual void FocusSequenceInstance(UMovieSceneSubSection& InSubSection) override;
	virtual EAutoChangeMode GetAutoChangeMode() const override;
	virtual void SetAutoChangeMode(EAutoChangeMode AutoChangeMode) override;
	virtual EAllowEditsMode GetAllowEditsMode() const override;
	virtual void SetAllowEditsMode(EAllowEditsMode AllowEditsMode) override;
	virtual bool GetKeyAllEnabled() const override;
	virtual void SetKeyAllEnabled(bool bKeyAllEnabled) override;
	virtual bool GetKeyInterpPropertiesOnly() const override;
	virtual void SetKeyInterpPropertiesOnly(bool bKeyInterpPropertiesOnly) override;
	virtual EMovieSceneKeyInterpolation GetKeyInterpolation() const override;
	virtual void SetKeyInterpolation(EMovieSceneKeyInterpolation) override;
	virtual bool GetInfiniteKeyAreas() const override;
	virtual void SetInfiniteKeyAreas(bool bInfiniteKeyAreas) override;
	virtual bool GetAutoSetTrackDefaults() const override;
	virtual bool IsRecordingLive() const override;
	virtual float GetLocalTime() const override;
	virtual float GetGlobalTime() const override;
	virtual void SetLocalTime(float Time, ESnapTimeMode SnapTimeMode = ESnapTimeMode::STM_None) override;
	virtual void SetLocalTimeDirectly(float NewTime) override;
	virtual void SetGlobalTime(float Time) override;
	virtual void ForceEvaluate() override;
	virtual void SetPerspectiveViewportPossessionEnabled(bool bEnabled) override;
	virtual void SetPerspectiveViewportCameraCutEnabled(bool bEnabled) override;
	virtual void RenderMovie(UMovieSceneSection* InSection) const override;
	virtual void EnterSilentMode() override { ++SilentModeCount; }
	virtual void ExitSilentMode() override { --SilentModeCount; ensure(SilentModeCount >= 0); }
	virtual bool IsInSilentMode() const override { return SilentModeCount != 0; }
	virtual FGuid GetHandleToObject(UObject* Object, bool bCreateHandleIfMissing = true) override;
	virtual ISequencerObjectChangeListener& GetObjectChangeListener() override;
protected:
	virtual void NotifyMovieSceneDataChangedInternal() override;
public:
	virtual void NotifyMovieSceneDataChanged( EMovieSceneDataChangeType DataChangeType ) override;
	virtual void UpdateRuntimeInstances() override;
	virtual void UpdatePlaybackRange() override;
	virtual TArray<FGuid> AddActors(const TArray<TWeakObjectPtr<AActor> >& InActors) override;
	virtual void AddSubSequence(UMovieSceneSequence* Sequence) override;
	virtual bool CanKeyProperty(FCanKeyPropertyParams CanKeyPropertyParams) const override;
	virtual void KeyProperty(FKeyPropertyParams KeyPropertyParams) override;
	virtual FSequencerSelection& GetSelection() override;
	virtual FSequencerSelectionPreview& GetSelectionPreview() override;
	virtual void GetSelectedTracks(TArray<UMovieSceneTrack*>& OutSelectedTracks) override;
	virtual void GetSelectedSections(TArray<UMovieSceneSection*>& OutSelectedSections) override;
	virtual void SelectObject(FGuid ObjectBinding) override;
	virtual void SelectTrack(UMovieSceneTrack* Track) override;
	virtual void SelectSection(UMovieSceneSection* Section) override;
	virtual void SelectByPropertyPaths(const TArray<FString>& InPropertyPaths) override;
	virtual void EmptySelection() override;
	virtual FOnGlobalTimeChanged& OnGlobalTimeChanged() override { return OnGlobalTimeChangedDelegate; }
	virtual FOnBeginScrubbingEvent& OnBeginScrubbingEvent() override { return OnBeginScrubbingDelegate; }
	virtual FOnEndScrubbingEvent& OnEndScrubbingEvent() override { return OnEndScrubbingDelegate; }
	virtual FOnMovieSceneDataChanged& OnMovieSceneDataChanged() override { return OnMovieSceneDataChangedDelegate; }
	virtual FOnMovieSceneBindingsChanged& OnMovieSceneBindingsChanged() override { return OnMovieSceneBindingsChangedDelegate; }
	virtual FOnSelectionChangedObjectGuids& GetSelectionChangedObjectGuids() override { return OnSelectionChangedObjectGuidsDelegate; }
	virtual FOnSelectionChangedTracks& GetSelectionChangedTracks() override { return OnSelectionChangedTracksDelegate; }
	virtual FOnSelectionChangedSections& GetSelectionChangedSections() override { return OnSelectionChangedSectionsDelegate; }
	virtual FGuid CreateBinding(UObject& InObject, const FString& InName) override;
	virtual UObject* GetPlaybackContext() const override;
	virtual TArray<UObject*> GetEventContexts() const override;
	virtual FOnActorAddedToSequencer& OnActorAddedToSequencer() override;
	virtual FOnPreSave& OnPreSave() override;
	virtual FOnPostSave& OnPostSave() override;
	virtual FOnActivateSequence& OnActivateSequence() override;
	virtual FOnCameraCut& OnCameraCut() override;
	virtual TSharedRef<INumericTypeInterface<float>> GetNumericTypeInterface() override;
	virtual TSharedRef<INumericTypeInterface<float>> GetZeroPadNumericTypeInterface() override;
	virtual TSharedRef<SWidget> MakeTransportControls(bool bExtended) override;
	virtual FReply OnPlay(bool bTogglePlay = true, float InPlayRate = 1.f) override;
	virtual void Pause() override;
	virtual TSharedRef<SWidget> MakeTimeRange(const TSharedRef<SWidget>& InnerContent, bool bShowWorkingRange, bool bShowViewRange, bool bShowPlaybackRange) override;
	virtual UObject* FindSpawnedObjectOrTemplate(const FGuid& BindingId) override;
	virtual FGuid MakeNewSpawnable(UObject& SourceObject) override;
	virtual bool IsReadOnly() const override;
	virtual void ExternalSelectionHasChanged() override { SynchronizeSequencerSelectionWithExternalSelection(); }
	virtual USequencerSettings* GetSequencerSettings() override { return Settings; }
	virtual TSharedPtr<class ITimeSlider> GetTopTimeSliderWidget() const override;

public:

	// IMovieScenePlayer interface

	virtual void UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut) override;
	virtual void NotifyBindingsChanged() override;
	virtual void SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) override;
	virtual void GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const override;
	virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const override;
	virtual void SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus) override;
	virtual FMovieSceneSpawnRegister& GetSpawnRegister() override { return *SpawnRegister; }
	virtual bool IsPreview() const override { return SilentModeCount != 0; }

public:

	FMovieSceneRootEvaluationTemplateInstance& GetSequenceInstance() { return RootTemplateInstance; }

protected:

	/** Reevaluate the sequence at the current time */
	void EvaluateInternal(FMovieSceneEvaluationRange InRange, bool bHasJumped = false);

	/** Reset data about a movie scene when pushing or popping a movie scene. */
	void ResetPerMovieSceneData();

	/** Update the time bounds to the focused movie scene */
	void UpdateTimeBoundsToFocusedMovieScene();

	/**
	 * Gets the far time boundaries of the currently edited movie scene
	 * If the scene has shots, it only takes the shot section boundaries
	 * Otherwise, it finds the furthest boundaries of all sections
	 */
	TRange<float> GetTimeBounds() const;
	
	/**
	 * Gets the time boundaries of the currently filtering shot sections.
	 * If there are no shot filters, an empty range is returned.
	 */
	TRange<float> GetFilteringShotsTimeBounds() const;

	/**
	 * Called when the clamp range is changed by the user
	 *
	 * @param	NewClampRange The new clamp range
	 */
	void OnClampRangeChanged( TRange<float> NewClampRange );

	/*
	 * Called to get the nearest key
	 *
	 * @param InTime The time to get the nearest key to
	 * @return NearestKey
	 */
	float OnGetNearestKey(float InTime);

	/**
	 * Called when the scrub position is changed by the user
	 * This will stop any playback from happening
	 *
	 * @param NewScrubPosition	The new scrub position
	 */
	void OnScrubPositionChanged( float NewScrubPosition, bool bScrubbing );

	/** Called when the user has begun scrubbing */
	void OnBeginScrubbing();

	/** Called when the user has finished scrubbing */
	void OnEndScrubbing();

	/** Called when the user has begun dragging the playback range */
	void OnPlaybackRangeBeginDrag();

	/** Called when the user has finished dragging the playback range */
	void OnPlaybackRangeEndDrag();

	/** Called when the user has begun dragging the selection range */
	void OnSelectionRangeBeginDrag();

	/** Called when the user has finished dragging the selection range */
	void OnSelectionRangeEndDrag();

protected:

	/**
	 * Update auto-scroll mechanics as a result of a new time position
	 */
	void UpdateAutoScroll(float NewTime);

	/**
	 * Ensure that the specified local time is in the view
	 */
	void ScrollIntoView(float InLocalTime);

	/**
	 * Calculates the amount of encroachment the specified time has into the autoscroll region, if any
	 */
	TOptional<float> CalculateAutoscrollEncroachment(float NewTime, float ThresholdPercentage = 0.1f) const;

	/** Called to toggle auto-scroll on and off */
	void OnToggleAutoScroll();

	/**
	 * Whether auto-scroll is enabled.
	 *
	 * @return true if auto-scroll is enabled, false otherwise.
	 */
	bool IsAutoScrollEnabled() const;

	/** Find the viewed sequence asset in the content browser. */
	void FindInContentBrowser();

	/** Get the asset we're currently editing, if applicable. */
	UObject* GetCurrentAsset() const;

protected:

	/** Get all the keys for the current sequencer selection */
	virtual void GetKeysFromSelection(TUniquePtr<ISequencerKeyCollection>& KeyCollection, float DuplicateThresoldTime) override;

	UMovieSceneSection* FindNextOrPreviousShot(UMovieSceneSequence* Sequence, float Time, const bool bNext) const;

protected:
	
	/** Called when a user executes the delete command to delete sections or keys */
	void DeleteSelectedItems();
	
	/** Transport controls */
	void TogglePlay();
	void PlayForward();
	void JumpToStart();
	void JumpToEnd();
	void ShuttleForward();
	void ShuttleBackward();
	void StepForward();
	void StepBackward();
	void StepToNextKey();
	void StepToPreviousKey();
	void StepToNextCameraKey();
	void StepToPreviousCameraKey();
	void StepToNextShot();
	void StepToPreviousShot();

	void ExpandAllNodesAndDescendants();
	void CollapseAllNodesAndDescendants();

	/** Expand or collapse selected nodes */
	void ToggleExpandCollapseNodes();

	/** Expand or collapse selected nodes and descendants*/
	void ToggleExpandCollapseNodesAndDescendants();

	/** Manually sets a key for the selected objects at the current time */
	void SetKey();

	/** Modeless Version of the String Entry Box */
	void GenericTextEntryModeless(const FText& DialogText, const FText& DefaultText, FOnTextCommitted OnTextComitted);
	
	/** Closes the popup created by GenericTextEntryModeless*/
	void CloseEntryPopupMenu();

	/** Trim a section to the left or right */
	void TrimSection(bool bTrimLeft);

	/** Split a section */
	void SplitSection();

	/** Generates command bindings for UI commands */
	void BindCommands();

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
	// End of FEditorUndoClient

	void OnSelectedOutlinerNodesChanged();

	/** Updates a viewport client from camera cut data */
	void UpdatePreviewLevelViewportClientFromCameraCut(FLevelEditorViewportClient& InViewportClient, UObject* InCameraObject, bool bJumpCut) const;

	/** Internal conversion function that doesn't perform expensive reset/update tasks */
	FMovieSceneSpawnable* ConvertToSpawnableInternal(FGuid PossessableGuid);

	/** Internal conversion function that doesn't perform expensive reset/update tasks */
	FMovieScenePossessable* ConvertToPossessableInternal(FGuid SpawnableGuid);

	/** Internal function to render movie for a given start/end time */
	void RenderMovieInternal(float InStartTime, float InEndTime, bool bSetFrameOverrides = false) const;

	/** Handles adding a new folder to the outliner tree. */
	void OnAddFolder();

	/** Create set playback start transport control */
	TSharedRef<SWidget> OnCreateTransportSetPlaybackStart();

	/** Create jump to previous key transport control */
	TSharedRef<SWidget> OnCreateTransportJumpToPreviousKey();

	/** Create jump to next key transport control */
	TSharedRef<SWidget> OnCreateTransportJumpToNextKey();

	/** Create set playback end transport control */
	TSharedRef<SWidget> OnCreateTransportSetPlaybackEnd();

	/** Select keys and/or sections in a display node that fall into the current selection range. */
	void SelectInSelectionRange(const TSharedRef<FSequencerDisplayNode>& DisplayNode, const TRange<float>& SelectionRange, bool bSelectKeys, bool bSelectSections);
	
	/** Create loop mode transport control */
	TSharedRef<SWidget> OnCreateTransportLoopMode();

	/** Create record transport control */
	TSharedRef<SWidget> OnCreateTransportRecord();

	/** Possess PIE viewports with the specified camera settings (a mirror of level viewport possession, but for game viewport clients) */
	void PossessPIEViewports(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut);

	/** Update the locked subsequence range (displayed as playback range for subsequences), and root to local transform */
	void UpdateSubSequenceData();

	/** Rerun construction scripts on bound actors */
	void RerunConstructionScripts();

	/** Get actors that want to rerun construction scripts */
	void GetConstructionScriptActors(UMovieScene*, FMovieSceneSequenceIDRef SequenceID, TSet<TWeakObjectPtr<AActor> >& BoundActors);

	/** Check whether we're viewing the master sequence or not */
	bool IsViewingMasterSequence() const { return ActiveTemplateIDs.Num() == 1; }

private:

	/** Reset the timing manager to default, or audio clock locked */
	void ResetTimingManager(bool bUseAudioClock);

	/** User-supplied settings object for this sequencer */
	USequencerSettings* Settings;

	/** Command list for sequencer commands (Sequencer widgets only). */
	TSharedRef<FUICommandList> SequencerCommandBindings;

	/** Command list for sequencer commands (shared by non-Sequencer). */
	TSharedRef<FUICommandList> SequencerSharedBindings;

	/** List of tools we own */
	TArray<TSharedPtr<ISequencerTrackEditor>> TrackEditors;

	/** List of object bindings we can use */
	TArray<TSharedPtr<ISequencerEditorObjectBinding>> ObjectBindings;

	/** Listener for object changes being made while this sequencer is open*/
	TSharedPtr<ISequencerObjectChangeListener> ObjectChangeListener;

	/** Main sequencer widget */
	TSharedPtr<SSequencer> SequencerWidget;
	
	/** Spawn register for keeping track of what is spawned */
	TSharedPtr<FMovieSceneSpawnRegister> SpawnRegister;

	/** The asset editor that created this Sequencer if any */
	TWeakPtr<IToolkitHost> ToolkitHost;

	TWeakObjectPtr<UMovieSceneSequence> RootSequence;
	FMovieSceneRootEvaluationTemplateInstance RootTemplateInstance;

	TArray<FMovieSceneSequenceID> ActiveTemplateIDs;

	FMovieSceneSequenceTransform RootToLocalTransform;

	/** The time range target to be viewed */
	TRange<float> TargetViewRange;

	/** The last time range that was viewed */
	TRange<float> LastViewRange;

	/** The view range before zooming */
	TRange<float> ViewRangeBeforeZoom;

	/** The amount of autoscroll pan offset that is currently being applied */
	TOptional<float> AutoscrollOffset;

	/** The amount of autoscrub offset that is currently being applied */
	TOptional<float> AutoscrubOffset;

	/** Zoom smoothing curves */
	FCurveSequence ZoomAnimation;
	FCurveHandle ZoomCurve;

	/** Overlay fading curves */
	FCurveSequence OverlayAnimation;
	FCurveHandle OverlayCurve;

	/** Whether we are playing, recording, etc. */
	EMovieScenePlayerStatus::Type PlaybackState;

	/** The current scrub position */
	// @todo sequencer: Should use FTimespan or "double" for Time Cursor Position! (cascades)
	float ScrubPosition;

	/** Current play position */
	FMovieScenePlaybackPosition PlayPosition;

	/** The playback rate */
	float PlayRate;

	/** The shuttle multiplier */
	float ShuttleMultiplier;

	bool bPerspectiveViewportPossessionEnabled;
	bool bPerspectiveViewportCameraCutEnabled;

	/** True if this sequencer is being edited within the level editor */
	bool bIsEditingWithinLevelEditor;

	bool bShowCurveEditor;

	/** Whether the sequence should be editable or read only */
	bool bReadOnly;

	/** Generic Popup Entry */
	TWeakPtr<IMenu> EntryPopupMenu;

	/** Stores a dirty bit for whether the sequencer tree (and other UI bits) may need to be refreshed.  We
	    do this simply to avoid refreshing the UI more than once per frame. (e.g. during live recording where
		the MovieScene data can change many times per frame.) */
	bool bNeedTreeRefresh;

	/** When true, the runtime instances need to be updated next frame. */
	bool bNeedInstanceRefresh;

	/** Stores the playback status to be restored on refresh. */
	EMovieScenePlayerStatus::Type StoredPlaybackState;

	FSequencerLabelManager LabelManager;
	FSequencerSelection Selection;
	FSequencerSelectionPreview SelectionPreview;

	/** Represents the tree of nodes to display in the animation outliner. */
	TSharedRef<FSequencerNodeTree> NodeTree;

	/** A delegate which is called any time the global time changes. */
	FOnGlobalTimeChanged OnGlobalTimeChangedDelegate;

	/** A delegate which is called whenever the user begins scrubbing. */
	FOnBeginScrubbingEvent OnBeginScrubbingDelegate;

	/** A delegate which is called whenever the user stops scrubbing. */
	FOnEndScrubbingEvent OnEndScrubbingDelegate;

	/** A delegate which is called any time the movie scene data is changed. */
	FOnMovieSceneDataChanged OnMovieSceneDataChangedDelegate;

	/** A delegate which is called any time the movie scene bindings are changed. */
	FOnMovieSceneBindingsChanged OnMovieSceneBindingsChangedDelegate;

	/** A delegate which is called any time the sequencer selection changes. */
	FOnSelectionChangedObjectGuids OnSelectionChangedObjectGuidsDelegate;

	/** A delegate which is called any time the sequencer selection changes. */
	FOnSelectionChangedTracks OnSelectionChangedTracksDelegate;

	/** A delegate which is called any time the sequencer selection changes. */
	FOnSelectionChangedSections OnSelectionChangedSectionsDelegate;

	FOnActorAddedToSequencer OnActorAddedToSequencerEvent;
	FOnCameraCut OnCameraCutEvent;
	FOnPreSave OnPreSaveEvent;
	FOnPostSave OnPostSaveEvent;
	FOnActivateSequence OnActivateSequenceEvent;

	int32 SilentModeCount;

	/** When true the sequencer selection is being updated from changes to the external selection. */
	bool bUpdatingSequencerSelection;

	/** When true the external selection is being updated from changes to the sequencer selection. */
	bool bUpdatingExternalSelection;

	/** The maximum tick rate prior to playing (used for overriding delta time during playback). */
	double OldMaxTickRate;

	/** Timing manager that can adjust playback times */
	TUniquePtr<FSequencerTimingManager> TimingManager;
	
	struct FCachedViewTarget
	{
		/** The player controller we're possessing */
		TWeakObjectPtr<APlayerController> PlayerController;
		/** The view target it was pointing at before we took over */
		TWeakObjectPtr<AActor> ViewTarget;
	};

	/** Cached array of view targets that were set before we possessed the player controller with a camera from sequencer */
	TArray<FCachedViewTarget> PrePossessionViewTargets;

	/** Attribute used to retrieve the playback context for this frame */
	TAttribute<UObject*> PlaybackContextAttribute;

	/** Cached playback context for this frame */
	TWeakObjectPtr<UObject> CachedPlaybackContext;

	/** Attribute used to retrieve event contexts */
	TAttribute<TArray<UObject*>> EventContextsAttribute;

	/** Event contexts retrieved from the above attribute once per frame */
	TArray<TWeakObjectPtr<UObject>> CachedEventContexts;

	bool bNeedsEvaluate;

	FAcquiredResources AcquiredResources;

	/** The range of the currently displayed sub sequence in relation to its parent section */
	TRange<float> SubSequenceRange;

	TSharedPtr<struct FSequencerTemplateStore> TemplateStore;

	TMap<FName, TFunction<void()>> CleanupFunctions;

	/** Transient collection of keys that is used for jumping between keys contained within the current selection */
	TUniquePtr<ISequencerKeyCollection> SelectedKeyCollection;
};
