// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "UObject/GCObject.h"
#include "Editor/Transactor.h"
#include "MatineeViewSaveData.h"
#include "MatineeTrackData.h"
#include "Framework/SlateDelegates.h"
#include "Framework/Application/IMenu.h"
#include "Toolkits/IToolkitHost.h"
#include "Editor/Matinee/Private/MatineeActions.h"
#include "MatineeGroupData.h"
#include "IMatinee.h"
#include "Matinee/InterpData.h"

#include "IDistCurveEditor.h"
#include "Editor/Matinee/Private/MatineeHitProxy.h"

class AActor;
class ACameraActor;
class AMatineeActor;
class FCameraControllerConfig;
class FCanvas;
class FEditorViewportClient;
class FLevelEditorViewportClient;
class FPrimitiveDrawInterface;
class FSceneView;
class FViewport;
class IDetailsView;
class SBorder;
class SDockableTab;
class SMatineeRecorder;
class SMatineeViewport;
class UInterpFilter;
class UInterpTrackInst;
class UInterpTrackMove;
class UMatineeOptions;
class UMatineeTransBuffer;
class UTexture2D;
struct FPropertyChangedEvent;

DECLARE_LOG_CATEGORY_EXTERN(LogSlateMatinee, Log, All);

class UInterpTrackMove;

class FMatinee : public IMatinee, public FGCObject, public FCurveEdNotifyInterface
{
public:
	/** Destructor */
	virtual ~FMatinee();

	/** Edits the specified ParticleSystem object */
	void InitMatinee(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* InObjectToEdit);

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	/** Register tabs that this toolkit can spawn with the TabManager */
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	/** Unregister tabs that this toolkit can spawn */	
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	TSharedRef<SDockTab> SpawnTab( const FSpawnTabArgs& TabSpawnArgs, FName TabIdentifier );

	void OnClose();

	/** Extends the toolbar for Matinee specific options */
	void ExtendDefaultToolbarMenu();

	/** @return the documentation location for this editor */
	virtual FString GetDocumentationLink() const override
	{
		return FString(TEXT("Engine/Matinee"));
	}

	////////////////////////////////

	/** handled the FEditoCallbacks::ActorMoved delegate */
	void OnActorMoved(AActor* InObject);

	/** handle the editor OnObjectsReplaced delegate */
	void OnObjectsReplaced(const TMap<UObject*,UObject*>& ReplacementMap);

	// FNotify interface
	void NotifyPreChange( UProperty* PropertyAboutToChange );
	void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged );

	// FCurveEdNotifyInterface
	virtual void PreEditCurve(TArray<UObject*> CurvesAboutToChange) override;
	virtual void PostEditCurve() override;
	virtual void MovedKey() override;
	virtual void DesireUndo() override;
	virtual void DesireRedo() override;

	/**
	 * FCurveEdNotifyInterface: Called by the Curve Editor when a Curve Label is clicked on
	 *
	 * @param	CurveObject	The curve object whose label was clicked on
	 */
	void OnCurveLabelClicked( UObject* CurveObject ) override;

	/** FGCObject interface */
	void AddReferencedObjects( FReferenceCollector& Collector ) override;

	/** 
	 * Starts playing the current sequence. 
	 * @param bPlayLoop		Whether or not we should play the looping section.
	 * @param bPlayForward	true if we should play forwards, or false for reverse
	 */
	void StartPlaying(bool bPlayLoop, bool bPlayForward) override;

	void ResumePlaying() override;

	/** Stops playing the current sequence. */
	void StopPlaying() override;

	/** Handle undo redo events */
	void OnPostUndoRedo(FUndoSessionContext SessionContext, bool Succeeded);

	// Menu handlers
	void OnMenuAddKey();
	void OnMenuPlay(const bool bShouldLoop, const bool bPlayForward);
	void OnMenuCreateMovie();
	void OnMenuStop();
	void OnMenuPause();
	void OnChangePlaySpeed( TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo );
	void OnMenuInsertSpace();
	void OnInsertSpaceTextEntry(const FText& InText, ETextCommit::Type CommitInfo);
	void StretchSection( bool bUseSelectedOnly );
	void OnStretchSectionTextEntered(const FText& InText, ETextCommit::Type Commitinfo, float SectionStart, float SectionEnd, float CurrentSectionLength );
	void OnMenuStretchSection();
	void OnMenuStretchSelectedKeyframes();
	void OnMenuDeleteSection();
	void OnMenuSelectInSection();
	void OnMenuDuplicateSelectedKeys();
	void OnSavePathTime();
	void OnJumpToPathTime();
	void OnViewHide3DTracks();
	bool IsViewHide3DTracksToggled();
	void OnViewZoomToScrubPos();
	bool IsViewZoomToScrubPosToggled();
	
	/**
	 * Shows or hides all movement track trajectories in the Matinee sequence
	 */
	void OnViewShowOrHideAll3DTrajectories( bool bShow );

	/** Toggles 'capture mode' for particle replay tracks */
	void OnParticleReplayTrackContext_ToggleCapture( bool bInEnableCapture );

	/** Called when the "Toggle Gore Preview" button is pressed */
	void OnToggleGorePreview();

	/** Called when the "Toggle Gore Preview" UI should be updated */
	bool IsGorePreviewToggled();

	/** Called when the "Create Camera Actor at Current Camera Location" button is pressed */
	void OnCreateCameraActorAtCurrentCameraLocation();

	/** Called when the "Launch Custom Preview Viewport" is pressed */
	void OnLaunchRecordingViewport();
	TSharedRef<SDockTab> SpawnRecordingViewport( const FSpawnTabArgs& Args );

	void OnToggleViewportFrameStats();
	bool IsViewportFrameStatsToggled();
	void OnEnableEditingGrid();
	bool IsEnableEditingGridToggled();
	void OnSetEditingGrid( uint32 InGridSize );
	bool IsEditingGridChecked( uint32 InGridSize );
	void OnToggleEditingCrosshair();
	bool IsEditingCrosshairToggled();
	
	void OnToggleSnap();
	bool IsSnapToggled();

	/** Called when the 'snap time to frames' command is triggered from the GUI */
	void OnToggleSnapTimeToFrames();
	bool IsSnapTimeToFramesToggled();
	bool IsSnapTimeToFramesEnabled();

	void OnMarkInSection();
	void OnMarkOutSection();
	
	/** Called when the 'fixed time step playback' command is triggered from the GUI */
	void OnFixedTimeStepPlaybackCommand();

	/** Updates UI state for 'fixed time step playback' option */
	bool IsFixedTimeStepPlaybackToggled();
	bool IsFixedTimeStepPlaybackEnabled();


	/** Called when the 'prefer frame numbers' command is triggered from the GUI */
	void OnPreferFrameNumbersCommand();

	/** Updates UI state for 'prefer frame numbers' option */
	bool IsPreferFrameNumbersToggled();
	bool IsPreferFrameNumbersEnabled();


	/** Updates UI state for 'show time cursor pos for all keys' option */
	void OnShowTimeCursorPosForAllKeysCommand();

	/** Called when the 'show time cursor pos for all keys' command is triggered from the GUI */
	bool IsShowTimeCursorPosForAllKeysToggled();

	void OnChangeSnapSize( TSharedPtr< FString > SelectedString, ESelectInfo::Type SelectInfo );

	/**
	 * Called when the initial curve interpolation mode for newly created keys is changed
	 */
	void OnChangeInitialInterpMode( TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo );

	void OnViewFitSequence();
	void OnViewFitToSelected();
	void OnViewFitLoop();
	void OnViewFitLoopSequence();
	void OnViewEndOfTrack();

	void OnToggleDirectorTimeline();
	bool IsDirectorTimelineToggled();

	void OnToggleCurveEditor();
	bool IsCurveEditorToggled();

	/**
	 * Called when the user selects the 'Expand All Groups' option from a menu.  Expands every group such that the
	 * entire hierarchy of groups and tracks are displayed.
	 */
	void OnExpandAllGroups();

	/**
	 * Called when the user selects the 'Collapse All Groups' option from a menu.  Collapses every group in the group
	 * list such that no tracks are displayed.
	 */
	void OnCollapseAllGroups();

	void OnContextNewTrack(UClass* NewInterpTrackClass);
	bool CanCreateNewTrack(UClass* NewInterpTrackClass) const;
	void OnContextNewGroup( FMatineeCommands::EGroupAction::Type InActionId ); 
	bool CanCreateNewGroup( FMatineeCommands::EGroupAction::Type InActionId ) const;
	void OnContextTrackRename();
	void OnContextTrackRenameTextCommitted(const FText& InText, ETextCommit::Type, UInterpTrack* Track, UInterpGroup* Group);
	void OnContextTrackDelete();

	void OnContextSelectActor( int32 InIndex );
	void OnContextGotoActors( int32 InIndex );
	void OnContextReplaceActor( int32 InIndex );
	void OnContextRemoveActors( int32 InIndex );
	void OnContextAddAllActors();
	void OnContextSelectAllActors();
	void OnContextReplaceAllActors();
	void OnContextRemoveAllActors();

	void AddActorToGroup(UInterpGroup* GroupToAdd, AActor* ActorToAdd);
	/** If ActorToRemove == NULL, it will remove all **/
	void RemoveActorFromGroup(UInterpGroup* GroupToRemove, AActor* ActorToRemove);
	/**
	 * Creates a new group track.
	 */
	void NewGroupPopup( FMatineeCommands::EGroupAction::Type InActionId, AActor* GroupActor, TArray<AActor *> OtherActorsToAddToGroup );
	void NewGroupPopupTextCommitted(
		const FText& InText, ETextCommit::Type, 
		FMatineeCommands::EGroupAction::Type InActionId, 
		AActor* GroupActor, 
		TArray<AActor *> OtherActorsToAddToGroup, 
		UInterpGroup* GroupToDuplicate);

	/**
	 * Toggles visibility of the trajectory for the selected movement track
	 */
	void OnContextTrackShow3DTrajectory();

	/**
	 * Exports the animations in the selected track to FBX
	 */
	void OnContextTrackExportAnimFBX();

	void OnContextGroupExportAnimFBX();
	
	void OnContextGroupRename();
	void OnContextGroupRenameCommitted(const FText& InText, ETextCommit::Type, UInterpGroup* GroupToRename);
	void OnContextGroupDelete();
	bool CanGroupDelete() const;
	void OnContextGroupCreateTab();
	bool CanGroupCreateTab() const;
	void OnContextGroupCreateTabTextCommitted(const FText& InText, ETextCommit::Type CommitInfo);
	void OnContextGroupSendToTab( int32 InIndex );
	void OnContextGroupRemoveFromTab();
	void OnContextDeleteGroupTab();

	/** Called when the user selects to move a group to another group folder */
	void OnContextGroupChangeGroupFolder( FMatineeCommands::EGroupAction::Type InActionId, int32 InIndex );

	void OnContextKeyInterpMode( FMatineeCommands::EKeyAction::Type InActionId );
	void OnContextRenameEventKey();
	void OnContextRenameEventKeyTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FName EventNameToChange);
	void OnContextSetKeyTime();
	void OnContextSetKeyTimeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FInterpEdSelKey* SelKey, UInterpTrack* Track);
	void OnContextSetValue();
	void OnContextSetValueTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FInterpEdSelKey* SelKey, class UInterpTrackFloatBase* FloatTrack);

	/** Pops up a menu and lets you set the color for the selected key. Not all track types are supported. */
	void OnContextSetColor();
	
	/**
	 * Flips the value of the selected key for a boolean property.
	 *
	 * @note	Assumes that the user was only given the option of flipping the 
	 *			value in the context menu (i.e. true -> false or false -> true).
	 */
	void OnContextSetBool();

	/** Pops up menu and lets the user set a group to use to lookup transform info for a movement keyframe. */
	void OnSetMoveKeyLookupGroup();
	void OnSetMoveKeyLookupGroupTextChosen(const FString& ChosenText, int32 KeyIndex, UInterpTrackMove* MoveTrack, class UInterpTrackMoveAxis* MoveTrackAxis);

	/** Clears the lookup group for a currently selected movement key. */
	void OnClearMoveKeyLookupGroup();

	void OnSetAnimKeyLooping( bool bInLooping );
	void OnSetAnimOffset( bool bInEndOffset );
	void OnSetAnimOffsetTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FInterpEdSelKey* SelKey, class UInterpTrackAnimControl* AnimTrack, bool bEndOffset);
	void OnSetAnimPlayRate();
	void OnSetAnimPlayRateTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FInterpEdSelKey* SelKey, class UInterpTrackAnimControl* AnimTrack);

	/** Handler for the toggle animation reverse menu item. */
	void OnToggleReverseAnim();

	/** Handler for UI update requests for the toggle anim reverse menu item. */
	bool IsReverseAnimToggled();

	/** Handler for the save as camera animation menu item. */
	void OnContextSaveAsCameraAnimation();

	/**
	 * Handler to move the clicked-marker to the current timeline position.
	 */
	void OnContextMoveMarkerToCurrentPosition();

	/**
	 * Handler to move the clicked-marker to the beginning of the sequence.
	 */
	void OnContextMoveMarkerToBeginning();

	/**
	 * Handler to move the clicked-marker to the end of the sequence.
	 */
	void OnContextMoveMarkerToEnd();

	/**
	 * Handler to move the clicked-marker to the end of the selected track.
	 */
	void OnContextMoveMarkerToEndOfSelectedTrack();

	/**
	 * Handler to move the grabbed marker to the end of the longest track.
	 */
	void OnContextMoveMarkerToEndOfLongestTrack();

	void OnSetSoundVolume();
	void OnSetSoundVolumeTextEntered( const FText& InText, ETextCommit::Type CommitInfo, TArray<int32> SoundTrackKeyIndices);
	void OnSetSoundPitch();
	void OnSetSoundPitchTextEntered(const FText& InText, ETextCommit::Type CommitInfo, TArray<int32> SoundTrackKeyIndices);
	void OnContextDirKeyTransitionTime();
	void OnContextDirKeyTransitionTimeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FInterpEdSelKey* SelKey, class UInterpTrackDirector* DirTrack);
	void OnContextDirKeyRenameCameraShot();
	void OnContextDirKeyRenameCameraShotTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FInterpEdSelKey* SelKey, class UInterpTrackDirector* DirTrack);
	void OnFlipToggleKey();

	/** Called when a new key condition is selected in a track keyframe context menu */
	void OnKeyContext_SetCondition( FMatineeCommands::EKeyAction::Type InCondition );
	bool KeyContext_IsSetConditionToggled( FMatineeCommands::EKeyAction::Type InCondition );

	/** Syncs the generic browser to the currently selected sound track key */
	void OnKeyContext_SyncGenericBrowserToSoundCue();

	/** Called when the user wants to set the master volume on Audio Master track keys */
	void OnKeyContext_SetMasterVolume();
	void OnKeyContext_SetMasterVolumeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, TArray<int32> SoundTrackKeyIndices);

	/** Called when the user wants to set the master pitch on Audio Master track keys */
	void OnKeyContext_SetMasterPitch();
	void OnKeyContext_SetMasterPitchTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, TArray<int32> SoundTrackKeyIndices);

	/** Called when the user wants to set the clip ID number for Particle Replay track keys */
	void OnParticleReplayKeyContext_SetClipIDNumber();
	void OnParticleReplayKeyContext_SetClipIDNumberTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, struct FParticleReplayTrackKey* ParticleReplayKey);

	/** Called when the user wants to set the duration of Particle Replay track keys */
	void OnParticleReplayKeyContext_SetDuration();
	void OnParticleReplayKeyContext_SetDurationTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, struct FParticleReplayTrackKey* ParticleReplayKey);

	/** Called to delete the currently selected keys */
	void OnDeleteSelectedKeys();

	void OnMenuUndo();
	void OnMenuRedo();
	void OnMenuCut();
	bool CanCut();
	void OnMenuCopy();
	void OnMenuPaste();

	void OnMenuImport();
	void OnMenuExport();
	void OnMenuReduceKeys();

	/** Called during color selection to refresh realtime viewports */
	void OnUpdateFromColorSelection(FLinearColor NewColor);


	/**
	 * Called when the user toggles the ability to export a key every frame. 
	 */
	void OnToggleBakeTransforms();

	/**
	 * Updates the checked-menu item for baking transforms
	 */
	bool IsBakeTransformsToggled();
	
	/**
	 * Called when the user toggles the ability to export with current hierarchy
	 */
	void OnToggleKeepHierarchy();

	/**
	 * Updates the checked-menu item for baking transforms
	 */
	bool IsKeepHierarchyToggled();
	
	/** Called when the 'Export Sound Cue Info' command is issued */
	void OnExportSoundCueInfoCommand();
	
	/** Called when 'Export Animation Info' command is issued */
	void OnExportAnimationInfoCommand();

	/** Toggles interting of the panning the interp editor left and right */
	void OnToggleInvertPan();
	bool IsInvertPanToggled();

	/** Called when a user selects the split translation and rotation option on a movement track */
	void OnSplitTranslationAndRotation();

	/** Called when a user selects the normalize velocity option on a movement track */
	void NormalizeVelocity();

	void ScaleMoveTrackTranslation();
	void ScaleTranslationByAmount( const FText& InText, ETextCommit::Type CommitInfo, UInterpTrackMove* MoveTrack  );

	// Selection
	void SetSelectedFilter( class UInterpFilter* InFilter );

	/**
	 * Selects the interp track at the given index in the given group's array of interp tracks.
	 * If the track is already selected, this function will do nothing. 
	 *
	 * @param	OwningGroup		The group that stores the interp track to be selected. Cannot be NULL.
	 * @param	TrackToSelect		The interp track to select.
	 * @param	bDeselectPreviousTracks	If true, then all previously-selected tracks will be deselected. Defaults to true.
	 */
	void SelectTrack( UInterpGroup* Group, UInterpTrack* TrackToSelect, bool bDeselectPreviousTracks = true ) override;

	/**
	 * Selects the given group.
	 *
	 * @param	GroupToSelect			The desired group to select. Cannot be NULL.
	 * @param	bDeselectPreviousGroups	If true, then all previously-selected groups will be deselected. Defaults to true.
	 */
	void SelectGroup( UInterpGroup* GroupToSelect, bool bDeselectPreviousGroups = true, bool bSelectGroupActors = true );

	/**
	 * Deselects the interp track stored in the given group at the given array index.
	 *
	 * @param	OwningGroup		The group holding the interp track to deselect.
	 * @param	TrackToDeselect		The track to deselect
	 * @param	bUpdateVisuals		If true, then all affected visual components related to track selection will be updated. Defaults to true.
	 */
	void DeselectTrack( UInterpGroup* Group, UInterpTrack* TrackToDeselect, bool bUpdateVisuals = true );

	/**
	 * Deselects every selected track. 
	 *
	 * @param	bUpdateVisuals	If true, then all affected visual components related to track selection will be updated. Defaults to true.
	 */
	void DeselectAllTracks( bool bUpdateVisuals = true );

	/**
	 * Deselects the given group.
	 *
	 * @param	GroupToDeselect	The desired group to deselect.
	 * @param	bUpdateVisuals	If true, then all affected visual components related to group selection will be updated. Defaults to true.
	 */
	void DeselectGroup( UInterpGroup* GroupToDeselect, bool bUpdateVisuals = true );

	/**
	 * Deselects all selected groups.
	 *
	 * @param	bUpdateVisuals	If true, then all affected visual components related to group selection will be updated. Defaults to true.
	 */
	void DeselectAllGroups( bool bUpdateVisuals = true );

	/**
	 * Deselects all selected groups or selected tracks. 
	 *
	 * @param	bUpdateVisuals	If true, then all affected visual components related to group and track selection will be updated. Defaults to true.
	 */
	void DeselectAll( bool bUpdateVisuals = true );

	/**
	 * @return	true if there is at least one selected group. false, otherwise.
	 */
	bool HasAGroupSelected() const;

	/**
	 * @param	GroupClass	The class type of interp group.
	 * @return	true if there is at least one selected group. false, otherwise.
	 */
	bool HasAGroupSelected( const UClass* GroupClass ) const;

	/**
	 * @return	true if at least one folder is selected; false, otherwise.
	 */
	bool HasAFolderSelected() const;

	/**
	 * @param	Group	Interp group to check if 
	 * @return	true if at least one interp group is selected; false, otherwise.
	 */
	FORCEINLINE bool IsGroupSelected( const UInterpGroup* Group ) const
	{
		check( Group );
		return Group->IsSelected();
	}

	/**
	 * @return	true if every single selected group is a folder. 
	 */
	bool AreAllSelectedGroupsFolders() const;

	/**
	 * @return	true if every single selected group is parented.
	 */
	bool AreAllSelectedGroupsParented() const;

	/**
	 * @return	true if there is at least one track in the Matinee; false, otherwise. 
	 */
	bool HasATrack() const;
	
	/**
	 * @return	true if there is at least one selected track. false, otherwise.
	 */
	bool HasATrackSelected() const;

	/**
	 * @param	TrackClass	The class type of interp track.  
	 * @return	true if there is at least one selected track of the given class type. false, otherwise.
	 */
	bool HasATrackSelected( const UClass* TrackClass ) const;
	
	/**
	 * @param	OwningGroup	Interp group to check for selected tracks.
	 * @return	true if at least one interp track selected owned by the given group; false, otherwise.
	 */
	bool HasATrackSelected( const UInterpGroup* OwningGroup ) const;

	/**
	 * @param	TrackClass	The class to check against each selected track.
	 * @return	true if every single selected track is of the given UClass; false, otherwise.
	 */
	bool AreAllSelectedTracksOfClass( const UClass* TrackClass ) const;

	/**
	 * @param	OwningGroup	The group to check against each selected track.
	 * @return	true if every single selected track is of owned by the given group; false, otherwise.
	 */
	bool AreAllSelectedTracksFromGroup( const UInterpGroup* OwningGroup ) const;

	/**
	 * @param	OwningGroup		The group associated to the track. Cannot be NULL.
	 * @param	TrackIndex		The index representing the interp track in an array of interp tracks stored in the given group. Cannot be INDEX_NONE.
	 * @return	true if an interp track at the given index in the given group's interp track array is selected; false, otherwise.
	 */
	/**
	 * @return	The number of the selected groups.
	 */
	int32 GetSelectedGroupCount() const override;

	/**
	 * @return	The number of selected tracks. 
	 */
	int32 GetSelectedTrackCount() const override;

	/**
	 * @return	A modifiable iterator that can iterate through all group entries, whether selected or not. 
	 */
	FGroupIterator GetGroupIterator() { return FGroupIterator(IData->InterpGroups); }

	/**
	 * @return	A non-modifiable iterator that can iterate through all group entries, whether selected or not. 
	 */
	FGroupConstIterator GetGroupIterator() const { return FGroupConstIterator(IData->InterpGroups); }
	
	/**
	 * @return	A modifiable iterator that can iteator over all selected interp groups.
	 */
	FSelectedGroupIterator GetSelectedGroupIterator() override { return FSelectedGroupIterator(IData->InterpGroups); }

	/**
	 * @return	A non-modifiable iterator that can iteator over all selected interp groups.
	 */
	FSelectedGroupConstIterator GetSelectedGroupIterator() const override { return FSelectedGroupConstIterator(IData->InterpGroups); }

	/**
	 * @return	A modifiable iterator that can iterate over all selected interp tracks.
	 */
	FSelectedTrackIterator GetSelectedTrackIterator() override { return FSelectedTrackIterator(IData->InterpGroups); }

	/**
	 * @return	A non-modifiable iterator that can iterate over all selected interp tracks.
	 */
	FSelectedTrackConstIterator GetSelectedTrackIterator() const override { return FSelectedTrackConstIterator(IData->InterpGroups); }

	/**
	 * @return	A modifiable iterator that can iterator over the selected interp tracks of the given template class.
	 */
	template<class TrackType> TTrackClassTypeIterator<TrackType> GetSelectedTrackIterator()
	{
		return TTrackClassTypeIterator<TrackType>(IData->InterpGroups);
	}

	/**
	 * @return	A non-modifiable iterator that can iterator over the selected interp tracks of the given template class.
	 */
	template<class TrackType> TTrackClassTypeConstIterator<TrackType> GetSelectedTrackIterator() const
	{
		return TTrackClassTypeConstIterator<TrackType>(IData->InterpGroups);
	}

	/**
	 * Locates the director group in our list of groups (if there is one)
	 *
	 * @param OutDirGroupIndex	The index of the director group in the list (if it was found)
	 *
	 * @return Returns true if a director group was found
	 */
	bool FindDirectorGroup( int32& OutDirGroupIndex ) const;

	/**
	 * Remaps the specified group index such that the director's group appears as the first element
	 *
	 * @param DirGroupIndex	The index of the 'director group' in the group list
	 * @param ElementIndex	The original index into the group list
	 *
	 * @return Returns the reordered element index for the specified element index
	 */
	int32 RemapGroupIndexForDirGroup( const int32 DirGroupIndex, const int32 ElementIndex ) const;

	/**
	 * Scrolls the view to the specified group if it is visible, otherwise it scrolls to the top of the screen.
	 *
	 * @param InGroup	Group to scroll the view to.
	 */
	void ScrollToGroup(class UInterpGroup* InGroup);

	/**
	 * Calculates the viewport vertical location for the given track.
	 *
	 * @note	This helper function is useful for determining if a track label is currently viewable.
	 *
	 * @param	InGroup				The group that owns the track.
	 * @param	InTrackIndex		The index of the track in the group's interp track array. 
	 * @param	LabelTopPosition	The viewport vertical location for the track's label top. 
	 * @param	LabelBottomPosition	The viewport vertical location for the track's label bottom. This is not the height of the label.
	 */
	void GetTrackLabelPositions( UInterpGroup* InGroup, int32 InTrackIndex, int32& LabelTopPosition, int32& LabelBottomPosition ) const;

	/**
	 * Calculates the viewport vertical location for the given group.
	 *
	 * @param	InGroup				The group that owns the track.
	 * @param	LabelTopPosition	The viewport vertical location for the group's label top. 
	 * @param	LabelBottomPosition	The viewport vertical location for the group's label bottom. This is not the height of the label.
	 */
	void GetGroupLabelPosition( UInterpGroup* InGroup, int32& LabelTopPosition, int32& LabelBottomPosition ) const;

	/**
	 * Expands or collapses all visible groups in the track editor
	 *
	 * @param bExpand true to expand all groups, or false to collapse them all
	 */
	void ExpandOrCollapseAllVisibleGroups( const bool bExpand );

	/**
	 * Updates the track window list scroll bar's vertical range to match the height of the window's content
	 */
	void UpdateTrackWindowScrollBars();

	/**
	 * Dirty the contents of the track window viewports
	 */
	void InvalidateTrackWindowViewports() override;

	/**
	 * Updates the contents of the property window based on which groups or tracks are selected if any. 
	 */
	void UpdatePropertyWindow();

	/**
	 * Creates a string with timing/frame information for the specified time value in seconds
	 *
	 * @param InTime The time value to create a timecode for
	 * @param bIncludeMinutes true if the returned string should includes minute information
	 *
	 * @return The timecode string
	 */
	FString MakeTimecodeString( float InTime, bool bIncludeMinutes = true ) const;

	/**
	 * Locates the specified group's parent group folder, if it has one
	 *
	 * @param ChildGroup The group who's parent we should search for
	 *
	 * @return Returns the parent group pointer or NULL if one wasn't found
	 */
	UInterpGroup* FindParentGroupFolder( UInterpGroup* ChildGroup ) const;

	/**
	 * Counts the number of children that the specified group folder has
	 *
	 * @param GroupFolder The group who's children we should count
	 *
	 * @return Returns the number of child groups
	 */
	int32 CountGroupFolderChildren( UInterpGroup* const GroupFolder ) const;

	/**
	 * @param	InGroup	The group to check if its a parent or has a parent. 
	 * @return	A structure containing information about the given group's parent relationship.
	 */
	FInterpGroupParentInfo GetParentInfo( UInterpGroup* InGroup ) const;

	/**
	 * Determines if the child candidate can be parented (or re-parented) by the parent candiddate.
	 *
	 * @param	ChildCandidate	The group that desires to become the child to the parent candidate.
	 * @param	ParentCandidate	The group that, if a folder, desires to parent the child candidate.
	 *
	 * @return	true if the parent candidate can parent the child candidate. 
	 */
	bool CanReparent( const FInterpGroupParentInfo& ChildCandidate, const FInterpGroupParentInfo& ParentCandidate ) const;

	/**
	 * Fixes up any problems in the folder/group hierarchy caused by bad parenting in previous builds
	 */
	void RepairHierarchyProblems();


	bool KeyIsInSelection(class UInterpGroup* InGroup, UInterpTrack* InTrack, int32 InKeyIndex) override;
	void AddKeyToSelection(class UInterpGroup* InGroup, UInterpTrack* InTrack, int32 InKeyIndex, bool bAutoWind) override;
	void RemoveKeyFromSelection(class UInterpGroup* InGroup, UInterpTrack* InTrack, int32 InKeyIndex) override;
	void ClearKeySelection() override;
	void CalcSelectedKeyRange(float& OutStartTime, float& OutEndTime);
	void SelectKeysInLoopSection();

	/**
	 * Clears all selected key of a given track.
	 *
	 * @param	OwningGroup			The group that owns the track containing the keys. 
	 * @param	Track				The track holding the keys to clear. 
	 * @param	bInvalidateDisplay	Sets the Matinee track viewport to refresh (Defaults to true).
	 */
	void ClearKeySelectionForTrack( UInterpGroup* OwningGroup, UInterpTrack* Track, bool bInvalidateDisplay = true );

	//Deletes keys if they are selected, otherwise will deleted selected tracks or groups
	void DeleteSelection (void);

	// Utils
	void DeleteSelectedKeys(bool bDoTransaction=false);
	void DuplicateSelectedKeys();
	void BeginMoveSelectedKeys();
	void EndMoveSelectedKeys();
	void MoveSelectedKeys(float DeltaTime);

	/**
	 * Adds a keyframe to the selected track.
	 *
	 * There must be one and only one track selected for a keyframe to be added.
	 */
	void AddKey() override;

	/** 
	 * Call utility to split an animation in the selected AnimControl track. 
	 *
	 * Only one interp track can be selected and it must be a anim control track for the function.
	 */
	void SplitAnimKey();
	void ReduceKeys();
	void ReduceKeysForTrack( UInterpTrack* Track, UInterpTrackInst* TrackInst, float IntervalStart, float IntervalEnd, float Tolerance );

	void ViewFitSequence();
	void ViewFitToSelected();
	void ViewFitLoop();
	void ViewFitLoopSequence();
	void ViewEndOfTrack();
	void ViewFit(float StartTime, float EndTime);

	void ChangeKeyInterpMode(EInterpCurveMode NewInterpMode=CIM_Unknown);

	/**
	 * Copies the currently selected group/group.
	 *
	 * @param bCut	Whether or not we should cut instead of simply copying the group/track.
	 */
	void CopySelectedGroupOrTrack(bool bCut);

	/**
	 * Pastes the previously copied group/track.
	 */
	void PasteSelectedGroupOrTrack();

	/**
	 * @return Whether or not we can paste a group/track.
	 */
	bool CanPasteGroupOrTrack();

	/**
	 * Duplicates the specified group
	 *
	 * @param GroupToDuplicate		Group we are going to duplicate.
	 */
	virtual void DuplicateGroup(UInterpGroup* GroupToDuplicate);

	/** Duplicates selected tracks in their respective groups and clears them to begin real time recording, and selects them */
	void DuplicateSelectedTracksForRecording (const bool bInDeleteSelectedTracks);

	/**Used during recording to capture a key frame at the current position of the timeline*/
	void RecordKeys(void);

	/**Store off parent positions so we can apply the parents delta of movement to the child*/
	void SaveRecordingParentOffsets(void);

	/**Apply the movement of the parent to child during recording*/
	void ApplyRecordingParentOffsets(void);

	/**
	 * Returns the custom recording viewport if it has been created yet
	 * @return - NULL, if no record viewport exists yet, or the current recording viewport
	 */
	FLevelEditorViewportClient* GetRecordingViewport(void);

	/**
	 * Adds a new track to the specified group.
	 *
	 * @param Group The group to add a track to
	 * @param TrackClass The class of track object we are going to add.
	 * @param TrackToCopy A optional track to copy instead of instantiating a new one.
	 * @param bAllowPrompts true if we should allow a dialog to be summoned to ask for initial information
	 * @param OutNewTrackIndex [Out] The index of the newly created track in its parent group
	 * @param bSelectTrack true if we should select the track after adding it
	 *
	 * @return Returns newly created track (or NULL if failed)
	 */
	UInterpTrack* AddTrackToGroup( UInterpGroup* Group, UClass* TrackClass, UInterpTrack* TrackToCopy, bool bAllowPrompts, int32& OutNewTrackIndex, bool bSelectTrack = true );

	/**
	 * Adds a new track to the selected group.
	 *
	 * @param TrackClass		The class of the track we are adding.
	 * @param TrackToCopy		A optional track to copy instead of instantiating a new one.  If NULL, a new track will be instantiated.
	 * @param bSelectTrack		If true, select the track after adding it
	 *
	 * @return	The newly-created track if created; NULL, otherwise. 
	 */
	UInterpTrack* AddTrackToSelectedGroup(UClass* TrackClass, UInterpTrack* TrackToCopy=NULL, bool bSelectTrack = true);

	/**
	 * Adds a new track to a group and appropriately updates/refreshes the editor
	 *
	 * @param Group				The group to add this track to
	 * @param NewTrackName		The default name of the new track to add
	 * @param TrackClass		The class of the track we are adding.
	 * @param TrackToCopy		A optional track to copy instead of instantiating a new one.  If NULL, a new track will be instantiated.
	 * @param bSelectTrack		If true, select the track after adding it
	 * return					New interp track that was created
	 */
	UInterpTrack*  AddTrackToGroupAndRefresh(UInterpGroup* Group, const FString& NewTrackName, UClass* TrackClass, UInterpTrack* TrackToCopy=NULL, bool bSelectTrack = true );

	/**
	 * Disables all tracks of a class type in this group
	 * @param Group - group in which to disable tracks of TrackClass type
	 * @param TrackClass - Type of track to disable
	 */
	void DisableTracksOfClass(UInterpGroup* Group, UClass* TrackClass);

	/** 
	 * Crops the anim key in the currently selected track. 
	 *
	 * @param	bCropBeginning		Whether to crop the section before the position marker or after.
	 */
	void CropAnimKey(bool bCropBeginning);

	/**
	 * Sets the global property name to use for newly created property tracks
	 *
	 * @param NewName The property name
	 */
	static void SetTrackAddPropName( const FName NewName );

	void LockCamToGroup(class UInterpGroup* InGroup, const bool bResetViewports=true);
	class AActor* GetViewedActor();
	virtual void UpdateCameraToGroup(const bool bInUpdateStandardViewports, bool bUpdateViewportTransform = true );
	void UpdateCamColours();

	/**
	 * Updates a viewport from a given actor
	 * @param InActor - The actor to track the viewport to
	 * @param InViewportClient - The viewport to update
	 * @param InFadeAmount - Fade amount for camera
	 * @param InColorScale - Color scale for render
	 * @param bInEnableColorScaling - whether to use color scaling or not
	 */
	void UpdateLevelViewport(AActor* InActor, FLevelEditorViewportClient* InViewportClient, const float InFadeAmount, const FVector& InColorScale, const bool bInEnableColorScaling, bool bUpdateViewportTransform );

	/** Saves data associated with viewport's which may get overridden by Matinee */
	void SaveLevelViewports();

	/** Restores viewports' settings that were overridden by UpdateLevelViewport, where necessary. */
	void RestoreLevelViewports();

	void SyncCurveEdView();

	/**
	 * Adds this Track to the curve editor
	 * @param GroupName - Name of this group
	 * @param GroupColor - Color to use for the curve
	 * @param InTrack - The track
	 */
	void AddTrackToCurveEd( FString GroupName, FColor GroupColor, UInterpTrack* InTrack, bool bShouldShowTrack );

	void SetInterpPosition(float NewPosition, bool Scrubbing = false);

	/** Refresh the Matinee position marker and viewport state */
	void RefreshInterpPosition();

	/** Make sure particle replay tracks have up-to-date editor-only transient state */
	void UpdateParticleReplayTracks();

	void SelectActiveGroupParent();

	/** Increments the cursor or selected keys by 1 interval amount, as defined by the toolbar combo. */
	void IncrementSelection();

	/** Decrements the cursor or selected keys by 1 interval amount, as defined by the toolbar combo. */
	void DecrementSelection();

	void SelectNextKey();
	void SelectPreviousKey();

	/**
	 * Zooms the curve editor and track editor in or out by the specified amount
	 *
	 * @param ZoomAmount			Amount to zoom in or out
	 * @param bZoomToTimeCursorPos	True if we should zoom to the time cursor position, otherwise mouse cursor position
	 */
	void ZoomView( float ZoomAmount, bool bZoomToTimeCursorPos );

	/** Toggles fixed time step playback mode */
	void SetFixedTimeStepPlayback( bool bInValue );

	/** Updates 'fixed time step' mode based on current playback state and user preferences */
	void UpdateFixedTimeStepPlayback();

	/** Toggles 'prefer frame number' setting */
	void SetPreferFrameNumbers( bool bInValue );

	/** Toggles 'show time cursor pos for all keys' setting */
	void SetShowTimeCursorPosForAllKeys( bool bInValue );

	void SetSnapEnabled(bool bInSnapEnabled);
	
	/** Toggles snapping the current timeline position to 'frames' in Matinee. */
	void SetSnapTimeToFrames( bool bInValue );

	/** Snaps the specified time value to the closest frame */
	float SnapTimeToNearestFrame( float InTime ) const;

	float SnapTime(float InTime, bool bIgnoreSelectedKeys);

	void BeginMoveMarker();
	void EndMoveMarker();
	void SetInterpEnd(float NewInterpLength);
	void MoveLoopMarker(float NewMarkerPos, bool bIsStart);

	void BeginDrag3DHandle(UInterpGroup* Group, int32 TrackIndex) override;
	void Move3DHandle(UInterpGroup* Group, int32 TrackIndex, int32 KeyIndex, bool bArriving, const FVector& Delta) override;
	void EndDrag3DHandle() override;
	void MoveInitialPosition(const FVector& Delta, const FRotator& DeltaRot) override;

	void ActorModified( bool bUpdateViewportTransform = true ) override;
	void ActorSelectionChange( const bool bClearSelectionIfInvalid = true ) override;
	void CamMoved(const FVector& NewCamLocation, const FRotator& NewCamRotation) override;
	bool ProcessKeyPress(FKey Key, bool bCtrlDown, bool bAltDown) override;

	void InterpEdUndo();
	void InterpEdRedo();

	void MoveActiveBy(int32 MoveBy);
	void MoveActiveUp();
	void MoveActiveDown();

	void DrawTracks3D(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	void DrawModeHUD(FEditorViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas) override;

	void TickInterp(float DeltaSeconds);

	/** Called from TickInterp, handles the ticking of the camera recording (game caster, xbox controller)*/
	void UpdateCameraRecording (void);

	void UpdateViewportSettings();

	/** Constrains the maximum frame rate to the fixed time step rate when playing back in that mode */
	void ConstrainFixedTimeStepFrameRate();

	void UpdateInitialTransformForMoveTrack(UInterpGroup* OwningGroup, UInterpTrackMove* InMoveTrack);

	static void UpdateAttachedLocations(AActor* BaseActor);

	static void InitInterpTrackClasses();

	static FString GetInterpEdFPSSnapSizeLocName( int32 StringIndex );

	/** 
	 * Sets the realtime audio override on the perspective viewport in the editor.
	 *
	 * @param bAudioIsRealtime	true if audio should be realtime
	 */
	void SetAudioRealtimeOverride( bool bAudioIsRealtime ) const;

	/**
	 * Called to enable/disable aspect ratio bar display
	 */
	void OnToggleAspectRatioBars();

	/**
	 * Called to enable/disable safe frame display
	 */
	void OnToggleSafeFrames();

	/** @return True if aspect ratio bars are being displayed in a matinee controlled viewport */
	bool AreAspectRatioBarsEnabled() const;

	/** @return True if safe frames are displayed in a matinee controlled viewport */
	bool IsSafeFrameDisplayEnabled() const;

	/** Sets the curve tab's visibility */
	void SetCurveTabVisibility(bool Visible);

	/** Menubar */
	TSharedPtr<SWidget> MenuBar;

	/** The property window (dockable) */
	TSharedPtr<class IDetailsView> PropertyWindow;

	/** The curve editor window (dockable) */
	TSharedPtr<IDistributionCurveEditor> CurveEd;

	/** A weak pointer to the curve editor's tab so that we can tell if it's open or closed */
	TWeakPtr<SDockTab> CurveEdTab;

	/** Director track editor window (dockable) */
	TSharedPtr<SMatineeViewport> DirectorTrackWindow;

	/** Main track editor window (dockable) */
	TSharedPtr<SMatineeViewport> TrackWindow;

	TSharedPtr<SBorder> GroupFilterContainer;

	UTexture2D*	BarGradText;
	FColor PosMarkerColor;
	FColor RegionFillColor;
	FColor RegionBorderColor;

	/** The Matinee actor being edited */
	class AMatineeActor* MatineeActor;
	/** Interp data associated with the Matinee Actor */
	class UInterpData* IData;

	// If we are connecting the camera to a particular group, this is it. If not, its NULL;
	class UInterpGroup* CamViewGroup;

	// Editor-specific Object, containing preferences and selection set to be serialised/undone.
	UMatineeOptions* Opt;

	// If we are looping 
	bool bLoopingSection;

	/** The real-time that we started playback last */
	double PlaybackStartRealTime;

	/** Number of continuous fixed time step frames we've played so far without any change in play back state,
	    such as time step, reverse mode, etc. */
	uint32 NumContinuousFixedTimeStepFrames;

	// Currently moving a curve handle in the 3D viewport.
	bool bDragging3DHandle;

	// Multiplier for preview playback of sequence
	float PlaybackSpeed;

	// Whether to draw the 3D version of any tracks.
	bool bHide3DTrackView;

	/** Indicates if zoom should auto-center on the current scrub position. */
	bool bZoomToScrubPos;

	/** Snap settings. */
	bool bSnapEnabled;
	bool bSnapToKeys;
	bool bSnapToFrames;
	float SnapAmount;
	int32 SnapSelectionIndex;

	/** True if the interp timeline position should be be snapped to the Matinee frame rate */
	bool bSnapTimeToFrames;

	/** True if fixed time step playback is enabled */
	bool bFixedTimeStepPlayback;
	
	/** True if the user prefers frame numbers to be drawn on track key labels (instead of time values) */
	bool bPreferFrameNumbers;

	/** True if we should draw the position of the time cursor relative to the start of each key right
	    next to time cursor in the track view */
	bool bShowTimeCursorPosForAllKeys;

	/** Initial curve interpolation mode for newly created keys.  This is loaded and saved to/from the user's
	  * editor preference file. */
	EInterpCurveMode InitialInterpMode;

	UTransactor* NormalTransactor;
	UMatineeTransBuffer* InterpEdTrans;

	/** Set to true in OnClose, at which point the editor is no longer ticked. */
	bool	bClosed;

	/** If true, the editor is modifying a CameraAnim, and functionality is tweaked appropriately */
	bool	bEditingCameraAnim;

	bool bInvertPan;
	// Static list of all InterpTrack subclasses.
	static TArray<UClass*>	InterpTrackClasses;
	static bool			bInterpTrackClassesInitialized;

	// Used to convert between seconds and size on the timeline
	int32		TrackViewSizeX;
	float	PixelsPerSec;
	float	NavPixelsPerSecond;

	float	ViewStartTime;
	float	ViewEndTime;

	EMatineeMarkerType::Type GrabbedMarkerType;

	bool	bDrawSnappingLine;
	float	SnappingLinePosition;
	float	UnsnappedMarkerPos;

	/** Width of track editor labels on left hand side */
	int32 LabelWidth;

	/** Creates a popup context menu based on the item under the mouse cursor.
	* @param	Viewport	FViewport for the FInterpEdViewportClient.
	* @param	HitResult	HHitProxy returned by FViewport::GetHitProxy( ).
	* @param	bIsDirectorTrackWindow	If we are creating this for the director view
	*/
	TSharedPtr<SWidget> CreateContextMenu( FViewport *Viewport, const HHitProxy *HitResult, bool bIsDirectorTrackWindow );

	/** Returns true if Matinee is fully initialized */
	bool IsInitialized() const
	{
		return bIsInitialized;
	}

	/** Returns true if viewport frame stats are currently enabled */
	bool IsViewportFrameStatsEnabled() const
	{
		return bViewportFrameStatsEnabled;
	}

	/** Returns the size that the editing grid should be based on user settings */
	int32 GetEditingGridSize() const
	{
		return EditingGridSize;
	}

	/** Returns true if the crosshair should be visible in matinee preview viewports */
	bool IsEditingCrosshairEnabled() const
	{
		return bEditingCrosshairEnabled;
	}

	/** Returns true if the editing grid should be enabled */
	bool IsEditingGridEnabled() //const
	{
		return bEditingGridEnabled;
	}

	/** Toggles whether or not to display the menu*/
	void ToggleRecordMenuDisplay (void) override
	{
		bDisplayRecordingMenu = !bDisplayRecordingMenu;
	}

	/**Preps Matinee to record/stop-recording realtime camera movement*/
	void ToggleRecordInterpValues(void) override;

	/**If true, real time camera recording mode has been enabled*/
	bool IsRecordingInterpValues (void) const override;

	/**Helper function to properly shut down matinee recording*/
	void StopRecordingInterpValues(void);

	/**
	 * Increments or decrements the currently selected recording menu item
	 * @param bInNext - true if going forward in the menu system, false if going backward
	 */
	void ChangeRecordingMenu(const bool bInNext) override;

	/**
	 * Increases or decreases the recording menu value
	 * @param bInIncrease - true if increasing the value, false if decreasing the value
	 */
	void ChangeRecordingMenuValue(FEditorViewportClient* InClient, const bool bInIncrease) override;

	/**
	 * Resets the recording menu value to the default
	 */
	void ResetRecordingMenuValue(FEditorViewportClient* InClient) override;

	/**
	 * Determines whether only the first click event is allowed or all repeat events are allowed
	 * @return - true, if the value should change multiple times.  false, if the user should have to release and reclick
	 */
	bool IsRecordMenuChangeAllowedRepeat (void) const override;
	
	/** Sets the record mode for matinee */
	void SetRecordMode(const uint32 InNewMode) override;

	/** Returns The time that sampling should start at */
	const double GetRecordingStartTime (void) const;

	/** Returns The time that sampling should end at */
	const double GetRecordingEndTime (void) const;

	/**Return the number of samples we're keeping around for roll smoothing*/
	int32 GetNumRecordRollSmoothingSamples (void) const override { return RecordRollSmoothingSamples; }
	/**Return the number of samples we're keeping around for roll smoothing*/
	int32 GetNumRecordPitchSmoothingSamples (void) const override { return RecordPitchSmoothingSamples; }
	/**Returns the current movement scheme we're using for the camera*/
	int32 GetCameraMovementScheme(void) const override { return RecordCameraMovementScheme; }

	/** Save record settings for next run */
	void SaveRecordingSettings(const FCameraControllerConfig& InCameraConfig);
	/** Load record settings for next run */
	void LoadRecordingSettings(FCameraControllerConfig& InCameraConfig) override;

	/**
	 * Access function to appropriate camera actor
	 * @param InCameraIndex - The index of the camera actor to return
	 * 
	 */
	ACameraActor* GetCameraActor(const int32 InCameraIndex);
	/**
	 * Access function to return the number of used camera actors
	 */
	int32 GetNumCameraActors(void) const;


	/**
	 * Simple accessor for the user's preference on whether clicking on a keyframe bar should trigger
	 * a selection or not
	 *
	 * @return	true if a click on a keyframe bar should cause a selection; false if it should not
	 */
	bool IsKeyframeBarSelectionAllowed() const 
	{ 
		return bAllowKeyframeBarSelection; 
	}

	/**
	 * Simple accessor for the user's preference on whether clicking on keyframe text should trigger
	 * a selection or not
	 *
	 * @return	true if a click on keyframe text should cause a selection; false if it should not
	 */
	bool IsKeyframeTextSelectionAllowed() const
	{
		return bAllowKeyframeTextSelection;
	}

protected:

	/**
	* Gets the visibility of the director track view
	*/
	EVisibility GetDirectorTrackWindowVisibility() const;

	TSharedRef<SWidget> BuildGroupFilterToolbar();

	TSharedRef<SWidget> AddFilterButton(UInterpFilter* Filter);

	void SetFilterActive(ECheckBoxState CheckStatus, UInterpFilter* Filter);

	ECheckBoxState GetFilterActive(UInterpFilter* Filter) const;

	/** Holds the slate object for the Matinee Recorder. */
	TWeakPtr<SMatineeRecorder> MatineeRecorderWindow;

	/** The tab for the Matinee Recorder. */
	TWeakPtr<SDockTab> MatineeRecorderTab;

	/** true if Matinee is fully initialized */
	bool bIsInitialized;

	/** true if viewport frame stats are currently enabled */
	bool bViewportFrameStatsEnabled;

	/** true if the viewport editing crosshair is enabled */
	bool bEditingCrosshairEnabled;

	/** true if the editing grid is enabled */
	bool bEditingGridEnabled;

	/** When true, a key will be exported every frame instead of just the keys that user created. */
	bool bBakeTransforms;

	/** If true, clicking on a keyframe bar (such as one representing the duration of an audio cue, etc.) will cause a selection */
	bool bAllowKeyframeBarSelection;

	/** If true, clicking on text associated with a keyframe with cause a selection */
	bool bAllowKeyframeTextSelection;

	/** If true, camera pitch will be locked to -90 to 90 degrees (defualt behavior) */
	bool bLockCameraPitch;

	/** The size of the editing grid (in number of vertical and horizontal sections) when the editing grid is displayed. 0 if no editing grid. */
	int32 EditingGridSize;

	/**
	 * Called when the user toggles the preference for allowing clicks on keyframe "bars" to cause a selection
	 */
	void OnToggleKeyframeBarSelection();
	

	/**
	 * Update the UI for the keyframe bar selection option
	 */
	bool IsKeyframeBarSelectionToggled();

	/**
	 * Called when the user toggles the preference for allowing clicks on keyframe text to cause a selection
	 */
	void OnToggleKeyframeTextSelection();

	/**
	 * Update the UI for the keyframe text selection option
	 */
	bool IsKeyframeTextSelectionToggled();

	/**
	 * Called when the user toggles the preference for allowing to lock/unlock the camera pitch contraints
	 */
	void OnToggleLockCameraPitch();

	/**
	 * Update the UI for the lock camera pitch option
	 */
	bool IsLockCameraPitchToggled();

	/**
	 * Updates the "lock camera pitch" value in all perspective viewports
	 */
	void LockCameraPitchInViewports(bool bLock);

	/**
	 * Gets "lock camera pitch" setting from the config setting
	 */
	void GetLockCameraPitchFromConfig();

	/**
	 * Utility function for gathering all the selected tracks into a TArray.
	 *
	 * @param	OutSelectedTracks	[out] An array of all interp tracks that are currently selected.
	 */
	void GetSelectedTracks( TArray<UInterpTrack*>& OutSelectedTracks );
	
	/**
	 * Utility function for gathering all the selected groups into a TArray.
	 *
	 * @param	OutSelectedGroups	[out] An array of all interp groups that are currently selected.
	 */
	void GetSelectedGroups( TArray<UInterpGroup*>& OutSelectedGroups );


	/** 
	 * Deletes the currently active track. 
	 */
	void DeleteSelectedTracks();

	/** 
	 * Deletes all selected groups.
	 */
	void DeleteSelectedGroups();

	/**
	 * Moves the marker the user grabbed to the given time on the timeline. 
	 *
	 * @param	MarkerType	The marker to move.
	 * @param	InterpTime	The position on the timeline to move the marker. 
	 */
	void MoveGrabbedMarker( float InterpTime );

	/**
	 * Calculates The timeline position of the longest track, which includes 
	 *			the duration of any assets such as: sounds or animations.
	 *
	 * @note	Use the template parameter to define which tracks to consider (all, selected only, etc).
	 * 
	 * @return	The timeline position of the longest track.
	 */
	template <class TrackFilterType>
	float GetLongestTrackTime() const;

	/**
	 * Update the preview camera actor, should be called when the track or group selection state changes
	 *
	 * @param	Associated	The group or track corresponding to the referenced actor update 
	 */
	void UpdatePreviewCamera( UInterpGroup* AssociatedGroup ) const;
	void UpdatePreviewCamera( UInterpTrack* AssociatedTrack ) const;

	/**Recording Menu Selection State*/
	int32 RecordMenuSelection;

	//whether or not to display the menu during a recording session
	bool bDisplayRecordingMenu;

	/**State of camera recording (countdown, recording, reprep)*/
	uint32 RecordingState;

	/**Mode of recording. See MatineeConstants*/
	int32 RecordMode;

	/**Number of samples for roll*/
	int32 RecordRollSmoothingSamples;
	/**Number of samples for pitch*/
	int32 RecordPitchSmoothingSamples;
	/**Camera Movement Scheme (free fly, planar/sky cam)*/
	int32 RecordCameraMovementScheme;

	/**The time the current camera recording state got changed (when did the countdown start)*/
	double RecordingStateStartTime;

	/**Tracks that are actively listening to controller input and sampling live key frames*/
	TArray <UInterpTrack*> RecordingTracks;

	/**Scratch pad for saving parent offsets for relative movement*/
	TMap<AActor*, FVector> RecordingParentOffsets;

	/**List of saved viewport clients' tansforms before entering Matinee editor*/
	TArray< FMatineeViewSaveData > SavedViewportData;

	/** Guard to prevent infinite looping on camera movement and update. */
	bool bUpdatingCameraGuard;

public:
	/** Menu Helpers */
	
	/** 
	 *	Modal Dialog 
	 *	@param Title			Title of the window
	 *  @param DialogText		Text to appear in the the dilog
	 *  @param DefaultText		Default Text in the entry box
	 *	@return					The entered string, may be empty if the user cancelled.
	 */
	FText GenericTextEntryModal(const FText& Title, const FText& DialogText, const FText& DefaultText);
	/** 
	 *	Modeless Version of the String Entry Box 
	 *  @param DialogText		Text to appear in the the dilog
	 *  @param DefaultText		Default Text in the entry box
	 *  @param OnTextCommited	Delegate: void (const FString&, ETextCommit::Type, ... );
	 */
	void GenericTextEntryModeless(const FText& DialogText, const FText& DefaultText, FOnTextCommitted OnTextComitted);
	/** Closes the popup created by GenericTextEntryModal or GenericTextEntryModeless*/
	void CloseEntryPopupMenu();
private:
	/** Popup menu function for retrieving a new name from the user. */	
	void GetNewNamePopup( const FText& InDialogTitle, const FText& InDialogCaption, const FText& InDefaultText, const FText& InOriginalName, FOnTextCommitted OnTextCommited );
	void OnNewNamePopupTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FOnTextCommitted OnTextComitted);

	/** Popup menu function for exporting animation */
	void ExportCameraAnimationNameCommitted(const FText& InAnimationPackageName, ETextCommit::Type CommitInfo);

	/** Query whether or not we're in small icon mode */
	EVisibility GetLargeIconVisibility() const;

	/** Widget Builders */
	void ExtendToolbar();
	void BuildCurveEditor();
	void BuildPropertyWindow();
	void BuildTrackWindow();

	/** Show a slate notification to say why an actor couldn't be added to a matinee group. */
	bool PrepareToAddActorAndWarnUser(AActor* ActorToAdd);
	
	/* Context Menu Builders */
	TSharedPtr<SWidget> CreateBkgMenu(bool bIsDirectorTrackWindow);
	TSharedPtr<SWidget> CreateGroupMenu();
	TSharedPtr<SWidget> CreateTrackMenu();
	TSharedPtr<SWidget> CreateKeyMenu();
	TSharedPtr<SWidget> CreateTabMenu();
	TSharedPtr<SWidget> CreateCollapseExpandMenu();
	TSharedPtr<SWidget> CreateMarkerMenu(EMatineeMarkerType::Type MarkerType);

	/** Accessors */
	AMatineeActor* GetMatineeActor() override { return MatineeActor; }
	UInterpData* GetInterpData() override { return IData; }
	bool Hide3DTrackView() override { return bHide3DTrackView; }

	/** List of open tool panels; used to ensure only one exists at any one time */
	TMap< FName, TWeakPtr<SDockableTab> > SpawnedToolPanels;

	/** Action Mapping */
	void BindCommands();

	/** Generic Popup Entry */
	TWeakPtr<class IMenu> EntryPopupMenu;

	/** ToolBar delegates */
	const FSlateBrush* GetToolbarInterpSpeedIcon();
	bool IsToolbarInterpSpeedChecked(float InPlaybackSpeed);
	
	const FSlateBrush* GetToolbarSnapIcon();
	bool IsToolbarSnapSettingChecked(uint32 InIndex);
	FString GetToolbarSnapText();

	TSharedPtr< STextComboBox > InitialInterpModeComboBox;
	TArray< TSharedPtr<FString> > InitialInterpModeStrings;

	TSharedPtr< STextComboBox> SnapCombo;
	TArray< TSharedPtr<FString> > SnapComboStrings;

	TSharedPtr< STextComboBox> SpeedCombo;
	TArray< TSharedPtr<FString> > SpeedSettingStrings;

	/** Finalizes the CreateKey process */
	void CommitAddedKeys();
	//Info passed between Matinee and the InterpTrack helper classes during key creation.
	struct AddKeyInfo
	{
		UInterpTrackInst* TrInst;
		class UInterpTrackHelper* TrackHelper;
		float fKeyTime;
	};
	TMap<UInterpTrack*, AddKeyInfo> AddKeyInfoMap;
	TMap<UInterpTrack*, int32> TrackToNewKeyIndexMap;

	/** Handle to the registered OnActorMoved delegate */
	FDelegateHandle OnActorMovedDelegateHandle;

	/** For keeping track of the previously used camera, so we can detect cuts when playing back in editor mode */
	ACameraActor* PreviousCamera;

	/** check to see if this matinee is currently editing a camera anim (restricts functionality) */
	bool IsCameraAnim() const;

	/** Update the actor selection in the editor to match those needed by the currently selected groups/tracks */
	void UpdateActorSelection() const;

public:
	/** Called by Matinee track helpers to finish the CreateKey once all the needed data is gathered 
	 *	@param	TrackToAddKey	Interp track we are creating the key for
	 *	@param	bCommitKeys		Setting to true will finalize the Key creation process
	 */
	void FinishAddKey(UInterpTrack *TrackToAddKey, bool bCommitKeys) override;

public:
	/** Constants */
	static const FColor ActiveCamColor;
	static const FColor SelectedCurveColor;
	static const int32	DuplicateKeyOffset;
	static const int32	KeySnapPixels;

	static const float InterpEditor_ZoomIncrement;

	static const FColor PositionMarkerLineColor;
	static const FColor LoopRegionFillColor;
	static const FColor Track3DSelectedColor;
	
	//static const uint32 NumInterpEdSnapSizes;
	static const float InterpEdSnapSizes[5];
	static const float InterpEdFPSSnapSizes[9];
};
