// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "Misc/App.h"
#include "UObject/Class.h"
#include "InputCoreTypes.h"
#include "GameFramework/Actor.h"
#include "Editor/UnrealEdEngine.h"
#include "Camera/CameraActor.h"
#include "Engine/Selection.h"
#include "LevelEditorViewport.h"
#include "UnrealEdGlobals.h"
#include "Matinee/InterpGroup.h"
#include "Matinee/InterpTrack.h"
#include "MatineeOptions.h"
#include "MatineeTransBuffer.h"
#include "MatineeViewportClient.h"
#include "MatineeViewSaveData.h"
#include "SMatineeRecorder.h"
#include "Editor.h"
#include "MatineeTrackData.h"
#include "Engine/InterpCurveEdSetup.h"
#include "Matinee.h"
#include "InterpTrackHelper.h"
#include "Matinee/MatineeActor.h"
#include "Matinee/MatineeActorCameraAnim.h"
#include "Matinee/InterpTrackInst.h"
#include "Matinee/InterpTrackMove.h"
#include "Matinee/InterpTrackFloatBase.h"
#include "Matinee/InterpTrackMoveAxis.h"
#include "Matinee/InterpTrackInstMove.h"
#include "Matinee/InterpTrackEvent.h"
#include "Matinee/InterpTrackFloatProp.h"
#include "Matinee/InterpTrackVectorBase.h"
#include "Matinee/InterpTrackLinearColorBase.h"
#include "Matinee/InterpTrackSound.h"
#include "Matinee/InterpTrackDirector.h"
#include "Matinee/InterpTrackFade.h"
#include "Matinee/InterpTrackSlomo.h"
#include "Matinee/InterpTrackColorScale.h"
#include "Matinee/InterpTrackInstDirector.h"
#include "Matinee/InterpTrackAnimControl.h"
#include "Matinee/InterpTrackParticleReplay.h"
#include "Matinee/InterpGroupInst.h"
#include "Matinee/InterpGroupDirector.h"
#include "Matinee/InterpGroupInstDirector.h"
#include "Matinee/InterpFilter.h"

#include "MatineeConstants.h"
#include "MatineeDelegates.h"

#include "EditorSupportDelegates.h"

#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Camera/CameraAnim.h"
#include "Misc/ConfigCacheIni.h"
#include "GameFramework/WorldSettings.h"

///// UTILS

void FMatinee::TickInterp(float DeltaTime)
{
	static bool bWasPlayingLastTick = false;

	if ( !bClosed )
	{
		UpdateViewportSettings();
	}
	
	// Don't tick if a windows close request was issued.
	if( !bClosed && MatineeActor->bIsPlaying )
	{
		// When in 'fixed time step' playback, we may need to constrain the frame rate (by sleeping!)
		ConstrainFixedTimeStepFrameRate();

		// Make sure particle replay tracks have up-to-date editor-only transient state
		UpdateParticleReplayTracks();

		// Modify playback rate by desired speed.
		float TimeDilation = MatineeActor->GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation();
		MatineeActor->StepInterp(DeltaTime * PlaybackSpeed * TimeDilation, true);
		
		// If we are looping the selected section, when we pass the end, place it back to the beginning 
		if(bLoopingSection)
		{
			if(MatineeActor->InterpPosition >= IData->EdSectionEnd || MatineeActor->InterpPosition < IData->EdSectionStart)
			{
				MatineeActor->UpdateInterp(IData->EdSectionStart, true, true);
				MatineeActor->Play();
			}
		}

		UpdateCameraToGroup(true);
		UpdateCamColours();
		CurveEd->SetPositionMarker(true, MatineeActor->InterpPosition, PosMarkerColor );
	}
	else
	{
		UpdateCameraToGroup(false);
	}

	if( bWasPlayingLastTick && !MatineeActor->bIsPlaying )
	{
		// If the interp was playing last tick but is now no longer playing turn off audio.
		SetAudioRealtimeOverride( false );
	}

	bWasPlayingLastTick = MatineeActor->bIsPlaying;

	// Make sure fixed time step mode is set correctly based on whether we're currently 'playing' or not
	// We need to do this here because interp sequences can stop without us ever telling them to (and
	// we won't find out about it!)
	UpdateFixedTimeStepPlayback();

	/**Capture key frames and increment the state of recording*/
	UpdateCameraRecording();
}

void FMatinee::UpdateViewportSettings()
{
	if ( GCurrentLevelEditingViewportClient )
	{
		if ( GCurrentLevelEditingViewportClient->IsPerspective() && GCurrentLevelEditingViewportClient->AllowsCinematicPreview() )
		{
			bool bSafeFrames = IsSafeFrameDisplayEnabled();
			bool bAspectRatioBars = AreAspectRatioBarsEnabled();

			if ( GCurrentLevelEditingViewportClient->IsShowingSafeFrameBoxDisplay() != bSafeFrames )
			{
				GCurrentLevelEditingViewportClient->SetShowSafeFrameBoxDisplay(bSafeFrames);
			}

			if ( GCurrentLevelEditingViewportClient->IsShowingAspectRatioBarDisplay() != bAspectRatioBars )
			{
				GCurrentLevelEditingViewportClient->SetShowAspectRatioBarDisplay(bAspectRatioBars);
			}
		}
	}
}

void  FMatinee::UpdateCameraRecording (void)
{
	//if we're recording a real-time camera playback, capture camera frame
	if (RecordingState != MatineeConstants::ERecordingState::RECORDING_COMPLETE)
	{
		double CurrentTime = FPlatformTime::Seconds();
		double TimeSinceStateStart = (CurrentTime - RecordingStateStartTime);
		
		switch (RecordingState)
		{
			case MatineeConstants::ERecordingState::RECORDING_GET_READY_PAUSE:
				//if time to begin recording
				if (TimeSinceStateStart >= MatineeConstants::CountdownDurationInSeconds)
				{
					//Set the new start time
					RecordingStateStartTime = CurrentTime;
					//change state
					RecordingState = MatineeConstants::ERecordingState::RECORDING_ACTIVE;

					//Clear all tracks that think they are recording
					RecordingTracks.Empty();

					//turn off looping!
					bLoopingSection = false;

					// Start Time moving, MUST be done done before set position, as Play rewinds time
					MatineeActor->Play();

					// Move to proper start time
					SetInterpPosition(GetRecordingStartTime());


					//if we're in camera duplication mode
					if ((RecordMode == MatineeConstants::ERecordMode::RECORD_MODE_NEW_CAMERA) || (RecordMode == MatineeConstants::ERecordMode::RECORD_MODE_NEW_CAMERA_ATTACHED))
					{
						//add new camera
						FLevelEditorViewportClient* LevelVC = GetRecordingViewport();
						if (!LevelVC)
						{
							StopRecordingInterpValues();
							return;
						}

						AActor* ActorToUseForBase = NULL;
						if ((RecordMode == MatineeConstants::ERecordMode::RECORD_MODE_NEW_CAMERA_ATTACHED) && (GEditor->GetSelectedActorCount()==1))
						{
							USelection& SelectedActors = *GEditor->GetSelectedActors();
							ActorToUseForBase = CastChecked< AActor >( SelectedActors.GetSelectedObject( 0 ) );
						}

						const FTransform Transform(LevelVC->GetViewRotation(), LevelVC->GetViewLocation());
						ACameraActor* NewCam = Cast<ACameraActor>(GEditor->AddActor( LevelVC->GetWorld()->GetCurrentLevel(), ACameraActor::StaticClass(), Transform ));
						NewCam->GetCameraComponent()->bConstrainAspectRatio = LevelVC->IsAspectRatioConstrained();
						NewCam->GetCameraComponent()->AspectRatio = LevelVC->AspectRatio;
						NewCam->GetCameraComponent()->FieldOfView = LevelVC->ViewFOV;

						//make new group for the camera
						UInterpGroup* NewGroup = NewObject<UInterpGroup>(IData, NAME_None, RF_Transactional);
						FString NewGroupName = NSLOCTEXT( "UnrealEd", "InterpEd_RecordMode_CameraGroupName", "CameraGroupCG" ).ToString();
						NewGroup->GroupName = FName(*NewGroupName);
						NewGroup->EnsureUniqueName();
						//add new camera group to matinee
						IData->Modify();
						IData->InterpGroups.Add(NewGroup);

						//add group instance for camera
						UInterpGroupInst* NewGroupInst = NewObject<UInterpGroupInst>(MatineeActor, NAME_None, RF_Transactional);
						// Initialise group instance, saving ref to actor it works on.
						NewGroupInst->InitGroupInst(NewGroup, NewCam);
						const int32 NewGroupInstIndex = MatineeActor->GroupInst.Add(NewGroupInst);

						//Link group with actor
						MatineeActor->InitGroupActorForGroup(NewGroup, NewCam);

						//unselect all, so we can select the newly added tracks
						DeselectAll();

						//Add new tracks to the camera group
						int32 MovementTrackIndex = INDEX_NONE;
						UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(AddTrackToGroup( NewGroup, UInterpTrackMove::StaticClass(), NULL, false, MovementTrackIndex, false ));
						check(MoveTrack);

						//add fov track
						SetTrackAddPropName( FName( TEXT( "FOVAngle" ) ) );
						int32 FOVTrackIndex = INDEX_NONE;
						UInterpTrack* FOVTrack = AddTrackToGroup( NewGroup, UInterpTrackFloatProp::StaticClass(), NULL, false, FOVTrackIndex, false );

						//set this group as the preview group
						const bool bResetViewports = false;
						LockCamToGroup(NewGroup, bResetViewports);

						//Select camera tracks
						SelectTrack( NewGroup, MoveTrack , false);
						SelectTrack( NewGroup, FOVTrack, false);

						RecordingTracks.Add(MoveTrack);
						RecordingTracks.Add(FOVTrack);
					}
					else if ((RecordMode == MatineeConstants::ERecordMode::RECORD_MODE_DUPLICATE_TRACKS) && (HasATrackSelected()))
					{
						//duplicate all selected tracks in their respective groups, and clear them
						const bool bDeleteSelectedTracks = false;
						DuplicateSelectedTracksForRecording(bDeleteSelectedTracks);
					} 
					else if ((RecordMode == MatineeConstants::ERecordMode::RECORD_MODE_REPLACE_TRACKS) && (HasATrackSelected()))
					{
						const bool bDeleteSelectedTracks = true;
						DuplicateSelectedTracksForRecording(bDeleteSelectedTracks);
					}
					else
					{
						//failed to be in a valid recording state (no track selected, and duplicate or replace)
						StopRecordingInterpValues();
						return;
					}
					
					for (int32 i = 0; i < RecordingTracks.Num(); ++i)
					{
						RecordingTracks[i]->bIsRecording = true;
					}

					//Sample state at "Start Time"
					RecordKeys();

					//Save the parent offsets for next frame
					SaveRecordingParentOffsets();
				}
				break;
			case MatineeConstants::ERecordingState::RECORDING_ACTIVE:
				{
					//apply movement of any parent object to the child object as well (since that movement is no longer processed when recording)
					ApplyRecordingParentOffsets();

					//Sample state at "Start Time"
					RecordKeys();

					//update the parent offsets for next frame
					SaveRecordingParentOffsets();

					//see if we're done recording (accounting for slow mo)
					if (MatineeActor->InterpPosition >= GetRecordingEndTime())
					{
						//Set the new start time
						RecordingStateStartTime = CurrentTime;
						//change state
						StopRecordingInterpValues();

						// Stop time if it's playing.
						MatineeActor->Stop();
						// Move to proper start time
						SetInterpPosition(GetRecordingStartTime());
					}
				}
				break;
			default:
				//invalid state
				break;
		}
	}
}



/** Constrains the maximum frame rate to the fixed time step rate when playing back in that mode */
void FMatinee::ConstrainFixedTimeStepFrameRate()
{
	// Don't allow the fixed time step playback to run faster than real-time
	if( bSnapToFrames && bFixedTimeStepPlayback )
	{
		// NOTE: Its important that PlaybackStartRealTime and NumContinuousFixedTimeStepFrames are reset
		//    when anything timing-related changes, like FApp::GetFixedDeltaTime() or playback direction.

		double CurRealTime = FPlatformTime::Seconds();

		// Minor hack to handle changes to world TimeDilation.  We reset our frame rate gate state
		// when we detect a change to time dilation.
		static float LastTimeDilation =  MatineeActor->GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation();
		if( LastTimeDilation != MatineeActor->GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation() )
		{
			// Looks like time dilation has changed!
			NumContinuousFixedTimeStepFrames = 0;
			PlaybackStartRealTime = CurRealTime;

			LastTimeDilation = MatineeActor->GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation();
		}

		// How long should have it taken to get to the current frame?
		const double ExpectedPlaybackTime =
			NumContinuousFixedTimeStepFrames * FApp::GetFixedDeltaTime() * PlaybackSpeed;

		// How long has it been (in real-time) since we started playback?
		double RealTimeSincePlaybackStarted = CurRealTime - PlaybackStartRealTime;

		// If we're way ahead of schedule (more than 5 ms), then we'll perform a long sleep
		float WaitTime = ExpectedPlaybackTime - RealTimeSincePlaybackStarted;
		if( WaitTime > 5 / 1000.0f )
		{
			FPlatformProcess::Sleep( WaitTime - 3 / 1000.0f );

			// Update timing info after our little snooze
			CurRealTime = FPlatformTime::Seconds();
			RealTimeSincePlaybackStarted = CurRealTime - PlaybackStartRealTime;
			WaitTime = ExpectedPlaybackTime - RealTimeSincePlaybackStarted;
		}

		while( RealTimeSincePlaybackStarted < ExpectedPlaybackTime )
		{
			// OK, we're running ahead of schedule so we need to wait a bit before the next frame
			FPlatformProcess::Sleep( 0.0f );

			// Check the time again
			CurRealTime = FPlatformTime::Seconds();
			RealTimeSincePlaybackStarted = CurRealTime - PlaybackStartRealTime;
			WaitTime = ExpectedPlaybackTime - RealTimeSincePlaybackStarted;
		}

		// Increment number of continuous fixed time step frames
		++NumContinuousFixedTimeStepFrames;
	}
}

void FMatinee::SetSelectedFilter(class UInterpFilter* InFilter)
{
	if ( IData->SelectedFilter != InFilter )
	{
		IData->SelectedFilter = InFilter;

		if(InFilter != NULL)
		{
			// Start by hiding all groups and tracks
			for(int32 GroupIdx=0; GroupIdx<IData->InterpGroups.Num(); GroupIdx++)
			{
				UInterpGroup* CurGroup = IData->InterpGroups[ GroupIdx ];
				CurGroup->bVisible = false;

				for( int32 CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
				{
					UInterpTrack* CurTrack = CurGroup->InterpTracks[ CurTrackIndex ];
					CurTrack->bVisible = false;
				}
			}


			// Apply the filter.  This will mark certain groups and tracks as visible.
			InFilter->FilterData( MatineeActor );


			// Make sure folders that are parents to visible groups are ALSO visible!
			for(int32 GroupIdx=0; GroupIdx<IData->InterpGroups.Num(); GroupIdx++)
			{
				UInterpGroup* CurGroup = IData->InterpGroups[ GroupIdx ];
				if( CurGroup->bVisible )
				{
					// Make sure my parent folder group is also visible!
					if( CurGroup->bIsParented )
					{
						UInterpGroup* ParentFolderGroup = FindParentGroupFolder( CurGroup );
						if( ParentFolderGroup != NULL )
						{
							ParentFolderGroup->bVisible = true;
						}
					}
				}
			}
		}
		else
		{
			// No filter, so show all groups and tracks
			for(int32 GroupIdx=0; GroupIdx<IData->InterpGroups.Num(); GroupIdx++)
			{
				UInterpGroup* CurGroup = IData->InterpGroups[ GroupIdx ];
				CurGroup->bVisible = true;

				// Hide tracks
				for( int32 CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
				{
					UInterpTrack* CurTrack = CurGroup->InterpTracks[ CurTrackIndex ];
					CurTrack->bVisible = true;
				}

			}
		}
		

		// The selected group filter may have changed which directly affects the vertical size of the content
		// in the track window, so we'll need to update our scroll bars.
		UpdateTrackWindowScrollBars();

		// Update scroll position
		for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
		{
			if( (*GroupIt)->bVisible )
			{
				ScrollToGroup( *GroupIt );

				// Immediately break because we want to scroll only 
				// to the first selected group that's visible.
				break;
			}
		}
	}
}

/**
 * @return	true if there is at least one selected group. false, otherwise.
 */
bool FMatinee::HasAGroupSelected() const
{
	bool bHasAGroupSelected = false;

	for( FSelectedGroupConstIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		// If we reach here, then we have at least one 
		// group selected because the iterator is valid.
		bHasAGroupSelected = true;
		break;
	}

	return bHasAGroupSelected;
}

/**
* @param	GroupClass	The class type of interp group.
* @return	true if there is at least one selected group. false, otherwise.
*/
bool FMatinee::HasAGroupSelected( const UClass* GroupClass ) const
{
	// If the user didn't pass in a UInterpGroup derived class, then 
	// they probably made a typo or are calling the wrong function.
	check( GroupClass->IsChildOf(UInterpGroup::StaticClass()) );

	bool bHasAGroupSelected = false;

	for( FSelectedGroupConstIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		if( (*GroupIt)->IsA(GroupClass) )
		{
			bHasAGroupSelected = true;
			break;
		}
	}

	return bHasAGroupSelected;
}

/**
 * @return	true if there is at least one track in the Matinee; false, otherwise. 
 */
bool FMatinee::HasATrack() const
{
	bool bHasATrack = false;

	FAllTracksConstIterator AllTracks(IData->InterpGroups);

	// Upon construction, the track iterator will automatically iterate until reaching the first 
	// interp track. If the track iterator is valid, then we have at least one track in the Matinee. 
	if( AllTracks )
	{
		bHasATrack = true;
	}

	return bHasATrack;
}

/**
 * @return	true if there is at least one selected track. false, otherwise.
 */
bool FMatinee::HasATrackSelected() const
{
	bool bHasASelectedTrack = false;

	for( FSelectedTrackConstIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		bHasASelectedTrack = true;
		break;
	}

	return bHasASelectedTrack;
}

/**
 * @param	TrackClass	The type of interp track. 
 * @return	true if there is at least one selected track of the given class type. false, otherwise.
 */
bool FMatinee::HasATrackSelected( const UClass* TrackClass ) const
{
	// If the user didn't pass in a UInterpTrack derived class, then 
	// they probably made a typo or are calling the wrong function.
	check( TrackClass->IsChildOf(UInterpTrack::StaticClass()) );

	bool bHasASelectedTrack = false;

	for( FSelectedTrackConstIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		if( (*TrackIt)->IsA(TrackClass) )
		{
			bHasASelectedTrack = true;
			break;
		}
	}

	return bHasASelectedTrack;
}

/**
 * @param	OwningGroup	MatineeActor group to check for selected tracks.
 * @return	true if at least one interp track selected owned by the given group; false, otherwise.
 */
bool FMatinee::HasATrackSelected( const UInterpGroup* OwningGroup ) const
{
	bool bHasASelectedTrack = false;

	for( TArray<UInterpTrack*>::TConstIterator TrackIt(OwningGroup->InterpTracks); TrackIt; ++TrackIt )
	{
		if( (*TrackIt)->IsSelected() == true )
		{
			bHasASelectedTrack = true;
			break;
		}
	}

	return bHasASelectedTrack;
}

/**
 * @return	true if at least one folder is selected; false, otherwise.
 */
bool FMatinee::HasAFolderSelected() const
{
	bool bHasAFolderSelected = false;

	for( FSelectedGroupConstIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		if( (*GroupIt)->bIsFolder )
		{
			bHasAFolderSelected = true;
			break;
		}
	}

	return bHasAFolderSelected;
}

/**
 * @return	true if every single selected group is a folder. 
 */
bool FMatinee::AreAllSelectedGroupsFolders() const
{
	// Set return value based on whether a group is selected or not because in the event 
	// that there are no selected groups, then the internals of the loop will never 
	// evaluate. If no groups are selected, then no folders are selected.
	bool bAllFolders = HasAGroupSelected();

	for( FSelectedGroupConstIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		if( (*GroupIt)->bIsFolder == false )
		{
			bAllFolders = false;
			break;
		}
	}

	return bAllFolders;
}

/**
 * @return	true if every single selected group is parented.
 */
bool FMatinee::AreAllSelectedGroupsParented() const
{
	// Assume true until we find the first group to not be parented.
	bool bAllGroupsAreParented = true;

	for( FSelectedGroupConstIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		if( !(*GroupIt)->bIsParented )
		{
			// We found a group that is not parented. 
			// We can exit the loop now. 
			bAllGroupsAreParented = false;
			break;
		}
	}

	return bAllGroupsAreParented;
}

/**
 * @param	TrackClass	The class to check against each selected track.
 * @return	true if every single selected track is of the given UClass; false, otherwise.
 */
bool FMatinee::AreAllSelectedTracksOfClass( const UClass* TrackClass ) const
{
	bool bResult = true;

	for( FSelectedTrackConstIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		if( !(*TrackIt)->IsA(TrackClass) )
		{
			bResult = false;
			break;
		}
	}

	return bResult;
}

/**
 * @param	OwningGroup	The group to check against each selected track.
 * @return	true if every single selected track is of owned by the given group; false, otherwise.
 */
bool FMatinee::AreAllSelectedTracksFromGroup( const UInterpGroup* OwningGroup ) const
{
	bool bResult = true;

	for( FSelectedTrackConstIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		if( !(TrackIt.GetGroup() == OwningGroup) )
		{
			bResult = false;
			break;
		}
	}

	return bResult;
}

/**
 * @return	The number of the selected groups.
 */
int32 FMatinee::GetSelectedGroupCount() const
{
	int32 SelectedGroupCount = 0;

	for( FSelectedGroupConstIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		SelectedGroupCount++;
	}

	return SelectedGroupCount;
}

/**
 * @return	The number of selected tracks. 
 */
int32 FMatinee::GetSelectedTrackCount() const
{
	int32 SelectedTracksTotal = 0;

	for( FSelectedTrackConstIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		SelectedTracksTotal++;
	}

	return SelectedTracksTotal;
}

/**
 * Utility function for gathering all the selected tracks into a TArray.
 *
 * @param	OutSelectedTracks	[out] An array of all interp tracks that are currently-selected.
 */
void FMatinee::GetSelectedTracks( TArray<UInterpTrack*>& OutTracks )
{
	// Make sure there aren't any existing items in the array in case they are non-selected tracks.
	OutTracks.Empty();

	for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		OutTracks.Add(*TrackIt);
	}
}

/**
 * Utility function for gathering all the selected groups into a TArray.
 *
 * @param	OutSelectedGroups	[out] An array of all interp groups that are currently-selected.
 */
void FMatinee::GetSelectedGroups( TArray<UInterpGroup*>& OutSelectedGroups )
{
	// Make sure there aren't any existing items in the array in case they are non-selected groups.
	OutSelectedGroups.Empty();
	
	for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		OutSelectedGroups.Add(*GroupIt);
	}
}

/**
 * Selects the interp track at the given index in the given group's array of interp tracks.
 * If the track is already selected, this function will do nothing. 
 *
 * @param	OwningGroup				The group that stores the interp track to be selected. Cannot be NULL.
 * @param	TrackToSelect			The interp track to select. Cannot be NULL
 * @param	bDeselectPreviousTracks	If true, then all previously-selected tracks will be deselected. Defaults to true.
 */
void FMatinee::SelectTrack( UInterpGroup* OwningGroup, UInterpTrack* TrackToSelect, bool bDeselectPreviousTracks /*= true*/ )
{
	check( OwningGroup && TrackToSelect );

	const bool bTrackAlreadySelected = TrackToSelect->IsSelected();
	const bool bWantsOtherTracksDeselected = ( bDeselectPreviousTracks && ( GetSelectedTrackCount() > 1 ) );

	// Early out if we already have the track selected or if there are multiple 
	// tracks and the user does not want all but the given track selected.
	if( bTrackAlreadySelected && !bWantsOtherTracksDeselected )
	{
		return;
	}

	// By default, the previously-selected tracks should be deselected. However, the client 
	// code has the option of not deselecting, especially when multi-selecting tracks.
	if( bDeselectPreviousTracks )
	{
		DeselectAllTracks();
	}

	// By selecting a track, we must deselect all selected groups.
	// We can only have one or the other. 
	DeselectAllGroups(false);

	// Select the track (prior to selecting the actor)
	TrackToSelect->SetSelected( true );

	// Update the preview camera now the track has been selected
	UpdatePreviewCamera( TrackToSelect );

	// Update the actor selection based on the new track selection
	UpdateActorSelection();

	// Update the property window to reflect the properties of the selected track.
	UpdatePropertyWindow();

	// Highlight the selected curve.
	IData->CurveEdSetup->ChangeCurveColor(TrackToSelect, SelectedCurveColor);
	CurveEd->RefreshViewport();
}


/**
 * Selects the given group.
 *
 * @param	GroupToSelect			The desired group to select. Cannot be NULL.
 * @param	bDeselectPreviousGroups	If true, then all previously-selected groups will be deselected. Defaults to true.
 */
void FMatinee::SelectGroup( UInterpGroup* GroupToSelect, bool bDeselectPreviousGroups /*= true*/, bool bSelectGroupActors /*= true*/ )
{
	// Must be a valid interp group.
	check( GroupToSelect );

	// First, deselect the previously-selected groups by default. The client code has 
	// the option to prevent this, especially for case such as multi-group select.
	if( bDeselectPreviousGroups )
	{
		DeselectAllGroups(false);
	}

	// By selecting a group, we must deselect any selected tracks.
	DeselectAllTracks(false);

	// Select the group (prior to selecting the actor)
	GroupToSelect->SetSelected( true );

	// Update the preview camera now the group has been selected
	UpdatePreviewCamera( GroupToSelect );

	if (bSelectGroupActors)
	{
		// Update the actor selection based on the new group selection
		UpdateActorSelection();
	}

	// Update the property window according to the new selection.
	UpdatePropertyWindow();

	// Dirty the display
	InvalidateTrackWindowViewports();
}

/**
 * Deselects the interp track store in the given group at the given array index.
 *
 * @param	OwningGroup				The group that stores the interp track to be deselected. Cannot be NULL.
 * @param	TrackToDeselect			The track to deslect. Cannot be NULL
 * @param	bUpdateVisuals			If true, then all affected visual components related to track selections will be updated. Defaults to true.
 */
void FMatinee::DeselectTrack( UInterpGroup* OwningGroup, UInterpTrack* TrackToDeselect, bool bUpdateVisuals /*= true*/  )
{
	check( OwningGroup && TrackToDeselect );

	TrackToDeselect->SetSelected( false );

	// Update the preview camera now the track has been deselected
	UpdatePreviewCamera( TrackToDeselect );

	// The client code has the option of opting out of updating the 
	// visual components that are affected by selecting tracks. 
	if( bUpdateVisuals )
	{
		// Update the curve corresponding to this track
		IData->CurveEdSetup->ChangeCurveColor( TrackToDeselect, OwningGroup->GroupColor );
		CurveEd->RefreshViewport();

		// Update the actor selection based on the new track selection
		UpdateActorSelection();

		// Update the property window to reflect the properties of the selected track.
		UpdatePropertyWindow();
	}

	// Clear any keys related to this track.
	ClearKeySelectionForTrack( OwningGroup, TrackToDeselect, false );

	// Always invalidate track windows
	InvalidateTrackWindowViewports();
}

/**
 * Deselects every selected track. 
 *
 * @param	bUpdateVisuals	If true, then all affected visual components related to track selections will be updated. Defaults to true.
 */
void FMatinee::DeselectAllTracks( bool bUpdateVisuals /*= true*/ )
{
	// Deselect all selected tracks and remove their matching curves.
	for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		UInterpTrack* CurrentTrack = *TrackIt;
		IData->CurveEdSetup->ChangeCurveColor(CurrentTrack, TrackIt.GetGroup()->GroupColor);
		CurrentTrack->SetSelected( false );

		// Update the preview camera now the track has been deselected
		UpdatePreviewCamera( CurrentTrack );
	}

	// The client code has the option of opting out of updating the 
	// visual components that are affected by selecting tracks. 
	if( bUpdateVisuals )
	{
		// Update the curve editor to reflect the curve color change
		CurveEd->RefreshViewport();

		// Update the actor selection based on the new track selection
		UpdateActorSelection();

		// Make sure there is nothing selected in the property 
		// window or in the level editing viewports.
		UpdatePropertyWindow();
	}

	// Make sure all keys are cleared!
	ClearKeySelection();
}

/**
 * Deselects the given group.
 *
 * @param	GroupToDeselect	The desired group to deselect.
 * @param	bUpdateVisuals	If true, then all affected visual components related to group selections will be updated. Defaults to true.
 */
void FMatinee::DeselectGroup( UInterpGroup* GroupToDeselect, bool bUpdateVisuals /*= true*/ )
{
	GroupToDeselect->SetSelected( false );

	// Update the preview camera now the group has been deselected
	UpdatePreviewCamera( GroupToDeselect );

	// The client code has the option of opting out of updating the 
	// visual components that are affected by selecting groups. 
	if( bUpdateVisuals )
	{
		// Update the actor selection based on the new group selection
		UpdateActorSelection();

		// Make sure there is nothing selected in the property window
		UpdatePropertyWindow();

		// Request an update of the track windows
		InvalidateTrackWindowViewports();
	}
}

/**
 * Deselects all selected groups.
 *
 * @param	bUpdateVisuals	If true, then all affected visual components related to group selections will be updated. Defaults to true.
 */
void FMatinee::DeselectAllGroups( bool bUpdateVisuals /*= true*/ )
{
	for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		UInterpGroup* CurrentGroup = *GroupIt;
		CurrentGroup->SetSelected( false );

		// Update the preview camera now the group has been deselected
		UpdatePreviewCamera( CurrentGroup );
	}

	// The client code has the option of opting out of updating the 
	// visual components that are affected by selecting groups. 
	if( bUpdateVisuals )
	{
		// Update the actor selection based on the new group selection
		UpdateActorSelection();

		// Update the property window to reflect the group deselection
		UpdatePropertyWindow();

		// Request an update of the track windows
		InvalidateTrackWindowViewports();
	}
}

/**
 * Deselects all selected groups or selected tracks. 
 *
 * @param	bUpdateVisuals	If true, then all affected visual components related to group selections will be updated. Defaults to true.
 */
void FMatinee::DeselectAll( bool bUpdateVisuals /*= true*/ )
{
	// We either have one-to-many groups selected or one-to-many tracks selected. 
	// So, we need to check which one it is. 
	if( HasAGroupSelected() )
	{
		DeselectAllGroups(bUpdateVisuals);
	}
	else if( HasATrackSelected() )
	{
		DeselectAllTracks(bUpdateVisuals);
	}
}

void FMatinee::UpdateActorSelection() const
{
	// Ignore this selection notification if desired.
	if ( AMatineeActor::IgnoreActorSelection() )
	{
		return;
	}

	AMatineeActor::PushIgnoreActorSelection();

	GUnrealEd->SelectNone( true, true );

	// Loop through the instances rather than the groups themselves so that we select all the actors associated with a selected group
	for( auto GroupInstIt = MatineeActor->GroupInst.CreateConstIterator(); GroupInstIt; ++GroupInstIt )
	{
		UInterpGroupInst* const GroupInst = *GroupInstIt;
		UInterpGroup* const CurrentGroup = GroupInst->Group;
		if( CurrentGroup->IsSelected() || CurrentGroup->HasSelectedTracks() )
		{
			const bool bDeselectActors = false;
			CurrentGroup->SelectGroupActor( GroupInst, bDeselectActors );
		}
	}

	AMatineeActor::PopIgnoreActorSelection();
}

void FMatinee::ClearKeySelection()
{
	Opt->SelectedKeys.Empty();
	Opt->bAdjustingKeyframe = false;
	Opt->bAdjustingGroupKeyframes = false;

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();
}

/**
 * Clears all selected key of a given track.
 *
 * @param	OwningGroup			The group that owns the track containing the keys. 
 * @param	Track				The track holding the keys to clear. 
 * @param	bInvalidateDisplay	Sets the Matinee track viewport to refresh (Defaults to true).
 */
void FMatinee::ClearKeySelectionForTrack( UInterpGroup* OwningGroup, UInterpTrack* Track, bool bInvalidateDisplay )
{
	for( int32 SelectedKeyIndex = 0; SelectedKeyIndex < Opt->SelectedKeys.Num(); SelectedKeyIndex++ )
	{
		// Remove key selections only for keys matching the given group and track index.
		if( (Opt->SelectedKeys[SelectedKeyIndex].Group == OwningGroup) && (Opt->SelectedKeys[SelectedKeyIndex].Track == Track) )
		{
			Opt->SelectedKeys.RemoveAt(SelectedKeyIndex--);
		}
	}

	// If there are no more keys selected, then the user is not adjusting keyframes anymore.
	Opt->bAdjustingKeyframe = (Opt->SelectedKeys.Num() == 1);
	Opt->bAdjustingGroupKeyframes = (Opt->SelectedKeys.Num() > 1);

	// Dirty the track window viewports
	if( bInvalidateDisplay )
	{
		InvalidateTrackWindowViewports();
	}
}

void FMatinee::AddKeyToSelection(UInterpGroup* InGroup, UInterpTrack* InTrack, int32 InKeyIndex, bool bAutoWind)
{
	check(InGroup);
	check(InTrack);

	check( InKeyIndex >= 0 && InKeyIndex < InTrack->GetNumKeyframes() );

	// If the sequence is currently playing, stop it before selecting the key.
	// This check is necessary because calling StopPlaying if playback is stopped will zero
	// the playback position, which we don't want to do.
	if ( MatineeActor->bIsPlaying )
	{
		StopPlaying();
	}

	// If key is not already selected, add to selection set.
	if( !KeyIsInSelection(InGroup, InTrack, InKeyIndex) )
	{
		// Add to array of selected keys.
		Opt->SelectedKeys.Add( FInterpEdSelKey(InGroup, InTrack, InKeyIndex) );
	}

	// If this is the first and only keyframe selected, make track active and wind to it.
	if(Opt->SelectedKeys.Num() == 1 && bAutoWind)
	{
		float KeyTime = InTrack->GetKeyframeTime(InKeyIndex);
		SetInterpPosition(KeyTime);

		// When jumping to keyframe, update the pivot so the widget is in the right place.
		UInterpGroupInst* GrInst = MatineeActor->FindFirstGroupInst(InGroup);
		if(GrInst)
		{
			AActor* GrActor = GrInst->GetGroupActor();
			if (GrActor)
			{
				GEditor->SetPivot( GrActor->GetActorLocation(), false, true );
			}
		}

		Opt->bAdjustingKeyframe = true;
	}

	if(Opt->SelectedKeys.Num() != 1)
	{
		Opt->bAdjustingKeyframe = false;
	}

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();
}

void FMatinee::RemoveKeyFromSelection(UInterpGroup* InGroup, UInterpTrack* InTrack, int32 InKeyIndex)
{
	for(int32 i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		if( Opt->SelectedKeys[i].Group == InGroup && 
			Opt->SelectedKeys[i].Track == InTrack && 
			Opt->SelectedKeys[i].KeyIndex == InKeyIndex )
		{
			Opt->SelectedKeys.RemoveAt(i);

			// If there are no more keys selected, then the user is not adjusting keyframes anymore.
			Opt->bAdjustingKeyframe = (Opt->SelectedKeys.Num() == 1);
			Opt->bAdjustingGroupKeyframes = (Opt->SelectedKeys.Num() > 1);

			// Dirty the track window viewports
			InvalidateTrackWindowViewports();

			return;
		}
	}
}

bool FMatinee::KeyIsInSelection(UInterpGroup* InGroup, UInterpTrack* InTrack, int32 InKeyIndex)
{
	for(int32 i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		if( Opt->SelectedKeys[i].Group == InGroup && 
			Opt->SelectedKeys[i].Track == InTrack && 
			Opt->SelectedKeys[i].KeyIndex == InKeyIndex )
			return true;
	}

	return false;
}

/** Clear selection and then select all keys within the gree loop-section. */
void FMatinee::SelectKeysInLoopSection()
{
	ClearKeySelection();

	// Add keys that are within current section to selection
	for(int32 i=0; i<IData->InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = IData->InterpGroups[i];
		for(int32 j=0; j<Group->InterpTracks.Num(); j++)
		{
			UInterpTrack* Track = Group->InterpTracks[j];
			Track->Modify();

			for(int32 k=0; k<Track->GetNumKeyframes(); k++)
			{
				// Add keys in section to selection for deletion.
				float KeyTime = Track->GetKeyframeTime(k);
				if(KeyTime >= IData->EdSectionStart && KeyTime <= IData->EdSectionEnd)
				{
					// Add to selection for deletion.
					AddKeyToSelection(Group, Track, k, false);
				}
			}
		}
	}
}

/** Calculate the start and end of the range of the selected keys. */
void FMatinee::CalcSelectedKeyRange(float& OutStartTime, float& OutEndTime)
{
	if(Opt->SelectedKeys.Num() == 0)
	{
		OutStartTime = 0.f;
		OutEndTime = 0.f;
	}
	else
	{
		OutStartTime = BIG_NUMBER;
		OutEndTime = -BIG_NUMBER;

		for(int32 i=0; i<Opt->SelectedKeys.Num(); i++)
		{
			UInterpTrack* Track = Opt->SelectedKeys[i].Track;
			float KeyTime = Track->GetKeyframeTime( Opt->SelectedKeys[i].KeyIndex );

			OutStartTime = FMath::Min(KeyTime, OutStartTime);
			OutEndTime = FMath::Max(KeyTime, OutEndTime);
		}
	}
}

//Deletes keys if they are selected, otherwise will deleted selected tracks or groups
void FMatinee::DeleteSelection (void)
{
	if (Opt->SelectedKeys.Num() > 0)
	{
		DeleteSelectedKeys(true);
	}
	else if (GetSelectedTrackCount() > 0)
	{
		DeleteSelectedTracks();
	}
	else if (GetSelectedGroupCount())
	{
		DeleteSelectedGroups();
	}
}

void FMatinee::DeleteSelectedKeys(bool bDoTransaction)
{
	if(bDoTransaction)
	{
		InterpEdTrans->BeginSpecial( NSLOCTEXT( "UnrealEd", "DeleteSelectedKeys", "Delete Selected Keys" ) );
		MatineeActor->Modify();
		Opt->Modify();
	}

	TArray<UInterpTrack*> ModifiedTracks;

	bool bRemovedEventKeys = false;
	for(int32 i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys[i];
		UInterpTrack* Track = SelKey.Track;

		check(Track);
		check(SelKey.KeyIndex >= 0 && SelKey.KeyIndex < Track->GetNumKeyframes());

		if(bDoTransaction)
		{
			// If not already done so, call Modify on this track now.
			if( !ModifiedTracks.Contains(Track) )
			{
				Track->Modify();
				ModifiedTracks.Add(Track);
			}
		}
			

		FName OldKeyName = NAME_None;
		if(const UInterpTrackEvent* TrackEvent = Cast< UInterpTrackEvent >( Track ))
		{
			// If this is an event key - we update the connectors later.
			bRemovedEventKeys = true;


			// Take a copy of the key name before it's removed
			if( TrackEvent->EventTrack.IsValidIndex( SelKey.KeyIndex ))
			{
				OldKeyName = TrackEvent->EventTrack[SelKey.KeyIndex].EventName;
			}
		}

		
		Track->RemoveKeyframe(SelKey.KeyIndex);


		// If we have a valid name, check to see if it's last event key to be removed with this name? 
		if( !OldKeyName.IsNone() )
		{
			const bool bCommonName = IData->IsEventName( OldKeyName );
			if( !bCommonName )
			{
				// Fire a delegate so other places that use the name can also update
				TArray<FName> KeyNames;
				KeyNames.Add( OldKeyName );
				FMatineeDelegates::Get().OnEventKeyframeRemoved.Broadcast( MatineeActor, KeyNames );
			}
		}


		// If any other keys in the selection are on the same track but after the one we just deleted, decrement the index to correct it.
		for(int32 j=0; j<Opt->SelectedKeys.Num(); j++)
		{
			if( Opt->SelectedKeys[j].Group == SelKey.Group &&
				Opt->SelectedKeys[j].Track == SelKey.Track &&
				Opt->SelectedKeys[j].KeyIndex > SelKey.KeyIndex &&
				j != i)
			{
				Opt->SelectedKeys[j].KeyIndex--;
			}
		}
	}

	// Update positions at current time, in case removal of the key changed things.
	RefreshInterpPosition();

	// Select no keyframe.
	ClearKeySelection();

	if(bDoTransaction)
	{
		InterpEdTrans->EndSpecial();
	}

	// Make sure the curve editor is in sync
	CurveEd->CurveChanged();
}

void FMatinee::DuplicateSelectedKeys()
{
	InterpEdTrans->BeginSpecial( NSLOCTEXT( "UnrealEd", "DuplicateSelectedKeys", "Duplicate Selected Keys" ) );
	MatineeActor->Modify();
	Opt->Modify();

	TArray<UInterpTrack*> ModifiedTracks;

	for(int32 i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys[i];
		UInterpTrack* Track = SelKey.Track;

		check(Track);
		check(SelKey.KeyIndex >= 0 && SelKey.KeyIndex < Track->GetNumKeyframes());

		// If not already done so, call Modify on this track now.
		if( !ModifiedTracks.Contains(Track) )
		{
			Track->Modify();
			ModifiedTracks.Add(Track);
		}
		
		float CurrentKeyTime = Track->GetKeyframeTime(SelKey.KeyIndex);
		float NewKeyTime = CurrentKeyTime + (float)DuplicateKeyOffset/PixelsPerSec;

		int32 DupKeyIndex = Track->DuplicateKeyframe(SelKey.KeyIndex, NewKeyTime);

		// Change selection to select the new keyframe instead.
		SelKey.KeyIndex = DupKeyIndex;

		// If any other keys in the selection are on the same track but after the new key, increase the index to correct it.
		for(int32 j=0; j<Opt->SelectedKeys.Num(); j++)
		{
			if( Opt->SelectedKeys[j].Group == SelKey.Group &&
				Opt->SelectedKeys[j].Track == SelKey.Track &&
				Opt->SelectedKeys[j].KeyIndex >= DupKeyIndex &&
				j != i)
			{
				Opt->SelectedKeys[j].KeyIndex++;
			}
		}
	}

	InterpEdTrans->EndSpecial();
}

/** Adjust the view so the entire sequence fits into the viewport. */
void FMatinee::ViewFitSequence()
{
	ViewStartTime = 0.f;
	ViewEndTime = IData->InterpLength;

	CurveEd->FitViewVertically();
	SyncCurveEdView();
}

/** Adjust the view so the selected keys fit into the viewport. */
void FMatinee::ViewFitToSelected()
{
	if( Opt->SelectedKeys.Num() > 0 )
	{
		float NewStartTime = BIG_NUMBER;
		float NewEndTime = -BIG_NUMBER;

		for( int32 CurKeyIndex = 0; CurKeyIndex < Opt->SelectedKeys.Num(); ++CurKeyIndex )
		{
			FInterpEdSelKey& CurSelKey = Opt->SelectedKeys[ CurKeyIndex ];
			
			UInterpTrack* Track = CurSelKey.Track;
			check( Track != NULL );
			check( CurSelKey.KeyIndex >= 0 && CurSelKey.KeyIndex < Track->GetNumKeyframes() );

			NewStartTime = FMath::Min( Track->GetKeyframeTime( CurSelKey.KeyIndex ), NewStartTime );
			NewEndTime = FMath::Max( Track->GetKeyframeTime( CurSelKey.KeyIndex ), NewEndTime );
		}

		// Clamp the minimum size
		if( NewStartTime - NewEndTime < 0.001f )
		{
			NewStartTime -= 0.005f;
			NewEndTime += 0.005f;
		}

		ViewStartTime = NewStartTime;
		ViewEndTime = NewEndTime;

		CurveEd->FitViewVertically();
		SyncCurveEdView();
	}
}

/** Adjust the view so the looped section fits into the viewport. */
void FMatinee::ViewFitLoop()
{
	// Do nothing if loop section is too small!
	float LoopRange = IData->EdSectionEnd - IData->EdSectionStart;
	if(LoopRange > 0.01f)
	{
		ViewStartTime = IData->EdSectionStart;
		ViewEndTime = IData->EdSectionEnd;

		SyncCurveEdView();
	}
}

/** Adjust the view so the looped section fits into the entire sequence. */
void FMatinee::ViewFitLoopSequence()
{
	// Adjust the looped section
	IData->EdSectionStart = 0.0f;
	IData->EdSectionEnd = IData->InterpLength;

	// Adjust the view
	ViewStartTime = IData->EdSectionStart;
	ViewEndTime = IData->EdSectionEnd;

	CurveEd->FitViewVertically();
	SyncCurveEdView();
}

/** Move the view to the end of the currently selected track(s). */
void FMatinee::ViewEndOfTrack()
{
	float NewEndTime = 0.0f;
	
	if( GetSelectedTrackCount() > 0 )
	{
		FSelectedTrackIterator TrackIt(GetSelectedTrackIterator());
		for( ; TrackIt; ++TrackIt )
		{
			UInterpTrack* Track = *TrackIt;
			if (Track->GetTrackEndTime() > NewEndTime)
			{
				NewEndTime = Track->GetTrackEndTime();
			}
		}
	}
	else // If no track is selected, move to the end of the sequence
	{
		NewEndTime = IData->InterpLength;
	}
	
	ViewStartTime = NewEndTime - (ViewEndTime - ViewStartTime);
	ViewEndTime = NewEndTime;

	CurveEd->FitViewVertically();
	SyncCurveEdView();
}

/** Adjust the view by the defined range. */
void FMatinee::ViewFit(float StartTime, float EndTime)
{
	ViewStartTime = StartTime;
	ViewEndTime = EndTime;

	CurveEd->FitViewVertically();
	SyncCurveEdView();
}

/** Iterate over keys changing their interpolation mode and adjusting tangents appropriately. */
void FMatinee::ChangeKeyInterpMode(EInterpCurveMode NewInterpMode/*=CIM_Unknown*/)
{
	for(int32 i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys[i];
		UInterpTrack* Track = SelKey.Track;

		UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(Track);
		if(MoveTrack)
		{
			MoveTrack->PosTrack.Points[SelKey.KeyIndex].InterpMode = NewInterpMode;
			MoveTrack->EulerTrack.Points[SelKey.KeyIndex].InterpMode = NewInterpMode;

			MoveTrack->PosTrack.AutoSetTangents(MoveTrack->LinCurveTension);
			MoveTrack->EulerTrack.AutoSetTangents(MoveTrack->AngCurveTension);
		}

		UInterpTrackFloatBase* FloatTrack = Cast<UInterpTrackFloatBase>(Track);
		if(FloatTrack)
		{
			//Some FloatBase Types to not make use of FloatTrack (such as AnimControl).  
			//Only operate on those that do.
			if (FloatTrack->FloatTrack.Points.Num())
			{
				FloatTrack->FloatTrack.Points[SelKey.KeyIndex].InterpMode = NewInterpMode;

				FloatTrack->FloatTrack.AutoSetTangents(FloatTrack->CurveTension);
			}
		}

		UInterpTrackVectorBase* VectorTrack = Cast<UInterpTrackVectorBase>(Track);
		if(VectorTrack)
		{
			UInterpTrackSound* SoundTrack = Cast<UInterpTrackSound>(VectorTrack);
			if (SoundTrack == NULL)	// don't attempt to change interp in a vector track that is actually a sound track
			{
				VectorTrack->VectorTrack.Points[SelKey.KeyIndex].InterpMode = NewInterpMode;

				VectorTrack->VectorTrack.AutoSetTangents(VectorTrack->CurveTension);
			}
		}

		UInterpTrackLinearColorBase* LinearColorTrack = Cast<UInterpTrackLinearColorBase>(Track);
		if(LinearColorTrack)
		{
			LinearColorTrack->LinearColorTrack.Points[SelKey.KeyIndex].InterpMode = NewInterpMode;

			LinearColorTrack->LinearColorTrack.AutoSetTangents(LinearColorTrack->CurveTension);
		}
	}

	CurveEd->RefreshViewport();
}

/** Increments the cursor or selected keys by 1 interval amount, as defined by the toolbar combo. */
void FMatinee::IncrementSelection()
{
	bool bMoveMarker = true;

	if(Opt->SelectedKeys.Num())
	{
		BeginMoveSelectedKeys();
		{
			MoveSelectedKeys(SnapAmount);
		}
		EndMoveSelectedKeys();
		bMoveMarker = false;
	}


	// Move the interp marker if there are no keys selected.
	if(bMoveMarker)
	{
		float StartTime = MatineeActor->InterpPosition;
		if( bSnapToFrames && bSnapTimeToFrames )
		{
			StartTime = SnapTimeToNearestFrame( MatineeActor->InterpPosition );
		}

		SetInterpPosition( StartTime + SnapAmount );
	}
}

/** Decrements the cursor or selected keys by 1 interval amount, as defined by the toolbar combo. */
void FMatinee::DecrementSelection()
{
	bool bMoveMarker = true;

	if(Opt->SelectedKeys.Num())
	{
		BeginMoveSelectedKeys();
		{
			MoveSelectedKeys(-SnapAmount);
		}
		EndMoveSelectedKeys();
		bMoveMarker = false;
	}

	// Move the interp marker if there are no keys selected.
	if(bMoveMarker)
	{
		float StartTime = MatineeActor->InterpPosition;
		if( bSnapToFrames && bSnapTimeToFrames )
		{
			StartTime = SnapTimeToNearestFrame( MatineeActor->InterpPosition );
		}

		SetInterpPosition( StartTime - SnapAmount );
	}
}

void FMatinee::SelectNextKey()
{
	// Keyframe operations can only happen when only one track is selected
	if( GetSelectedTrackCount() == 1 )
	{
		FSelectedTrackIterator TrackIt(GetSelectedTrackIterator());
		UInterpTrack* Track = *TrackIt;

		int32 NumKeys = Track->GetNumKeyframes();

		if(NumKeys)
		{
			int32 i;
			for(i=0; i < NumKeys-1 && Track->GetKeyframeTime(i) < (MatineeActor->InterpPosition + KINDA_SMALL_NUMBER); i++);
	
			ClearKeySelection();
			AddKeyToSelection(TrackIt.GetGroup(), Track, i, true);
		}
	}
}

void FMatinee::SelectPreviousKey()
{
	// Keyframe operations can only happen when only one track is selected
	if( GetSelectedTrackCount() == 1 )
	{
		FSelectedTrackIterator TrackIt(GetSelectedTrackIterator());
		UInterpTrack* Track = *TrackIt;

		int32 NumKeys = Track->GetNumKeyframes();

		if(NumKeys)
		{
			int32 i;
			for(i=NumKeys-1; i > 0 && Track->GetKeyframeTime(i) > (MatineeActor->InterpPosition - KINDA_SMALL_NUMBER); i--);

			ClearKeySelection();
			AddKeyToSelection(TrackIt.GetGroup(), Track, i, true);
		}
	}
}

/** Turns snap on and off in Matinee. Updates state of snap button as well. */
void FMatinee::SetSnapEnabled(bool bInSnapEnabled)
{
	bSnapEnabled = bInSnapEnabled;

	if(bSnapToKeys)
	{
		CurveEd->SetInSnap(false, SnapAmount, bSnapToFrames);
	}
	else
	{
		CurveEd->SetInSnap(bSnapEnabled, SnapAmount, bSnapToFrames);
	}

	// Save to ini when it changes.
	GConfig->SetBool( TEXT("Matinee"), TEXT("SnapEnabled"), bSnapEnabled, GEditorPerProjectIni );
}


/** Toggles snapping the current timeline position to 'frames' in Matinee. */
void FMatinee::SetSnapTimeToFrames( bool bInValue )
{

	bSnapTimeToFrames = bInValue;

	// Save to ini when it changes.
	GConfig->SetBool( TEXT("Matinee"), TEXT("SnapTimeToFrames"), bSnapTimeToFrames, GEditorPerProjectIni );

	// Go ahead and apply the change right now if we need to
	if( IsInitialized() && bSnapToFrames && bSnapTimeToFrames )
	{
		SetInterpPosition( SnapTimeToNearestFrame( MatineeActor->InterpPosition ) );
	}
}



/** Toggles fixed time step mode */
void FMatinee::SetFixedTimeStepPlayback( bool bInValue )
{
	bFixedTimeStepPlayback = bInValue;
	
	// Save to ini when it changes.
	GConfig->SetBool( TEXT("Matinee"), TEXT("FixedTimeStepPlayback"), bFixedTimeStepPlayback, GEditorPerProjectIni );

	// Update fixed time step state
	UpdateFixedTimeStepPlayback();
}



/** Updates 'fixed time step' mode based on current playback state and user preferences */
void FMatinee::UpdateFixedTimeStepPlayback()
{
	// Turn on 'benchmarking' mode if we're using a fixed time step
	bool bIsBenchmarking = MatineeActor->bIsPlaying && bSnapToFrames && bFixedTimeStepPlayback;
	FApp::SetBenchmarking(bIsBenchmarking);

	// Set the time interval between fixed ticks
	FApp::SetFixedDeltaTime(SnapAmount);
}



/** Toggles 'prefer frame numbers' setting */
void FMatinee::SetPreferFrameNumbers( bool bInValue )
{
	bPreferFrameNumbers = bInValue;

	// Save to ini when it changes.
	GConfig->SetBool( TEXT("Matinee"), TEXT("PreferFrameNumbers"), bPreferFrameNumbers, GEditorPerProjectIni );
}



/** Toggles 'show time cursor pos for all keys' setting */
void FMatinee::SetShowTimeCursorPosForAllKeys( bool bInValue )
{
	bShowTimeCursorPosForAllKeys = bInValue;

	// Save to ini when it changes.
	GConfig->SetBool( TEXT("Matinee"), TEXT("ShowTimeCursorPosForAllKeys"), bShowTimeCursorPosForAllKeys, GEditorPerProjectIni );
}



/** Snaps the specified time value to the closest frame */
float FMatinee::SnapTimeToNearestFrame( float InTime ) const
{
	// Compute the new time value by rounding
	const int32 InterpPositionInFrames = FMath::RoundToInt( InTime / SnapAmount );
	const float NewTime = InterpPositionInFrames * SnapAmount;

	return NewTime;
}



/** Take the InTime and snap it to the current SnapAmount. Does nothing if bSnapEnabled is false */
float FMatinee::SnapTime(float InTime, bool bIgnoreSelectedKeys)
{
	if(bSnapEnabled)
	{
		if(bSnapToKeys)
		{
			// Iterate over all tracks finding the closest snap position to the supplied time.

			bool bFoundSnap = false;
			float BestSnapPos = 0.f;
			float BestSnapDist = BIG_NUMBER;
			for(int32 i=0; i<IData->InterpGroups.Num(); i++)
			{
				UInterpGroup* Group = IData->InterpGroups[i];
				for(int32 j=0; j<Group->InterpTracks.Num(); j++)
				{
					UInterpTrack* Track = Group->InterpTracks[j];

					// If we are ignoring selected keys - build an array of the indices of selected keys on this track.
					TArray<int32> IgnoreKeys;
					if(bIgnoreSelectedKeys)
					{
						for(int32 SelIndex=0; SelIndex<Opt->SelectedKeys.Num(); SelIndex++)
						{
							if( Opt->SelectedKeys[SelIndex].Group == Group && 
								Opt->SelectedKeys[SelIndex].Track == Track )
							{
								IgnoreKeys.AddUnique( Opt->SelectedKeys[SelIndex].KeyIndex );
							}
						}
					}

					float OutPos = 0.f;
					bool bTrackSnap = Track->GetClosestSnapPosition(InTime, IgnoreKeys, OutPos);
					if(bTrackSnap) // If we found a snap location
					{
						// See if its closer than the closest location so far.
						float SnapDist = FMath::Abs(InTime - OutPos);
						if(SnapDist < BestSnapDist)
						{
							BestSnapPos = OutPos;
							BestSnapDist = SnapDist;
							bFoundSnap = true;
						}
					}
				}
			}

			// Find how close we have to get to snap, in 'time' instead of pixels.
			float SnapTolerance = (float)KeySnapPixels/(float)PixelsPerSec;

			// If we are close enough to snap position - do it.
			if(bFoundSnap && (BestSnapDist < SnapTolerance))
			{
				bDrawSnappingLine = true;
				SnappingLinePosition = BestSnapPos;

				return BestSnapPos;
			}
			else
			{
				bDrawSnappingLine = false;

				return InTime;
			}
		}
		else
		{	
			// Don't draw snapping line when just snapping to grid.
			bDrawSnappingLine = false;

			return SnapTimeToNearestFrame( InTime );
		}
	}
	else
	{
		bDrawSnappingLine = false;

		return InTime;
	}
}

void FMatinee::BeginMoveMarker()
{
	if(GrabbedMarkerType == EMatineeMarkerType::ISM_SeqEnd)
	{
		UnsnappedMarkerPos = IData->InterpLength;
		InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "MoveEndMarker", "Move End Marker") );
		IData->Modify();
	}
	else if(GrabbedMarkerType == EMatineeMarkerType::ISM_LoopStart)
	{
		UnsnappedMarkerPos = IData->EdSectionStart;
		InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "MoveLoopStartMarker", "Move Loop Start Marker") );
		IData->Modify();
	}
	else if(GrabbedMarkerType == EMatineeMarkerType::ISM_LoopEnd)
	{
		UnsnappedMarkerPos = IData->EdSectionEnd;
		InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "MoveLoopEndMarker", "Move Loop End Marker") );
		IData->Modify();
	}
}

void FMatinee::EndMoveMarker()
{
	if(	GrabbedMarkerType == EMatineeMarkerType::ISM_SeqEnd || 
		GrabbedMarkerType == EMatineeMarkerType::ISM_LoopStart || 
		GrabbedMarkerType == EMatineeMarkerType::ISM_LoopEnd)
	{
		InterpEdTrans->EndSpecial();
	}
}

void FMatinee::SetInterpEnd(float NewInterpLength)
{
	// Ensure non-negative end time.
	IData->InterpLength = FMath::Max(NewInterpLength, 0.f);
	
	CurveEd->SetEndMarker(true, IData->InterpLength);

	// Ensure the current position is always inside the valid sequence area.
	if(MatineeActor->InterpPosition > IData->InterpLength)
	{
		SetInterpPosition(IData->InterpLength);
	}

	// Ensure loop points are inside sequence.
	IData->EdSectionStart = FMath::Clamp(IData->EdSectionStart, 0.f, IData->InterpLength);
	IData->EdSectionEnd = FMath::Clamp(IData->EdSectionEnd, 0.f, IData->InterpLength);
	CurveEd->SetRegionMarker(true, IData->EdSectionStart, IData->EdSectionEnd, RegionFillColor);

	// Update the CameraAnim if necessary
	AMatineeActorCameraAnim* const CamAnimMatineeActor = Cast<AMatineeActorCameraAnim>(MatineeActor);
	if ( (CamAnimMatineeActor != nullptr) && (CamAnimMatineeActor->CameraAnim != nullptr) )
	{
		CamAnimMatineeActor->CameraAnim->AnimLength = IData->InterpLength;
	}
}

void FMatinee::MoveLoopMarker(float NewMarkerPos, bool bIsStart)
{
	if(bIsStart)
	{
		IData->EdSectionStart = NewMarkerPos;
		IData->EdSectionEnd = FMath::Max(IData->EdSectionStart, IData->EdSectionEnd);				
	}
	else
	{
		IData->EdSectionEnd = NewMarkerPos;
		IData->EdSectionStart = FMath::Min(IData->EdSectionStart, IData->EdSectionEnd);
	}

	// Ensure loop points are inside sequence.
	IData->EdSectionStart = FMath::Clamp(IData->EdSectionStart, 0.f, IData->InterpLength);
	IData->EdSectionEnd = FMath::Clamp(IData->EdSectionEnd, 0.f, IData->InterpLength);

	CurveEd->SetRegionMarker(true, IData->EdSectionStart, IData->EdSectionEnd, RegionFillColor);
}

void FMatinee::BeginMoveSelectedKeys()
{
	InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "MoveSelectedKeys", "Move Selected Keys") );
	Opt->Modify();

	TArray<UInterpTrack*> ModifiedTracks;
	for(int32 i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys[i];

		UInterpTrack* Track = SelKey.Track;
		check(Track);

		// If not already done so, call Modify on this track now.
		if( !ModifiedTracks.Contains(Track) )
		{
			Track->Modify();
			ModifiedTracks.Add(Track);
		}

		SelKey.UnsnappedPosition = Track->GetKeyframeTime(SelKey.KeyIndex);
	}

	// When moving a key in time, turn off 'recording', so we dont end up assigning an objects location at one time to a key at another time.
	Opt->bAdjustingKeyframe = false;
	Opt->bAdjustingGroupKeyframes = false;
}

void FMatinee::EndMoveSelectedKeys()
{
	InterpEdTrans->EndSpecial();
}

void FMatinee::MoveSelectedKeys(float DeltaTime)
{
	for(int32 i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys[i];

		UInterpTrack* Track = SelKey.Track;
		check(Track);

		SelKey.UnsnappedPosition += DeltaTime;
		float NewTime = SnapTime(SelKey.UnsnappedPosition, true);

		// Do nothing if already at target time.
		if( Track->GetKeyframeTime(SelKey.KeyIndex) != NewTime )
		{
			int32 OldKeyIndex = SelKey.KeyIndex;
			int32 NewKeyIndex = Track->SetKeyframeTime(SelKey.KeyIndex, NewTime);
			SelKey.KeyIndex = NewKeyIndex;

			// If the key changed index we need to search for any other selected keys on this track that may need their index adjusted because of this change.
			int32 KeyMove = NewKeyIndex - OldKeyIndex;
			if(KeyMove > 0)
			{
				for(int32 j=0; j<Opt->SelectedKeys.Num(); j++)
				{
					if( j == i ) // Don't look at one we just changed.
						continue;

					FInterpEdSelKey& TestKey = Opt->SelectedKeys[j];
					if( TestKey.Track == SelKey.Track && 
						TestKey.Group == SelKey.Group &&
						TestKey.KeyIndex > OldKeyIndex && 
						TestKey.KeyIndex <= NewKeyIndex)
					{
						TestKey.KeyIndex--;
					}
				}
			}
			else if(KeyMove < 0)
			{
				for(int32 j=0; j<Opt->SelectedKeys.Num(); j++)
				{
					if( j == i )
						continue;

					FInterpEdSelKey& TestKey = Opt->SelectedKeys[j];
					if( TestKey.Track == SelKey.Track && 
						TestKey.Group == SelKey.Group &&
						TestKey.KeyIndex < OldKeyIndex && 
						TestKey.KeyIndex >= NewKeyIndex)
					{
						TestKey.KeyIndex++;
					}
				}
			}
		}

	} // FOR each selected key

	// Update positions at current time but with new keyframe times.
	RefreshInterpPosition();

	CurveEd->RefreshViewport();
}


void FMatinee::BeginDrag3DHandle(UInterpGroup* Group, int32 TrackIndex)
{
	if(TrackIndex < 0 || TrackIndex >= Group->InterpTracks.Num())
	{
		return;
	}

	UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>( Group->InterpTracks[TrackIndex] );
	if(MoveTrack)
	{
		InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "Drag3DTrajectoryHandle", "Drag 3D Trajectory Handle") );
		MoveTrack->Modify();
		bDragging3DHandle = true;
	}
}

void FMatinee::Move3DHandle(UInterpGroup* Group, int32 TrackIndex, int32 KeyIndex, bool bArriving, const FVector& Delta)
{
	if(!bDragging3DHandle)
	{
		return;
	}

	if(TrackIndex < 0 || TrackIndex >= Group->InterpTracks.Num())
	{
		return;
	}

	UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>( Group->InterpTracks[TrackIndex] );
	if(MoveTrack)
	{
		if(KeyIndex < 0 || KeyIndex >= MoveTrack->PosTrack.Points.Num())
		{
			return;
		}

		UInterpGroupInst* GrInst = MatineeActor->FindFirstGroupInst(Group);
		check(GrInst);
		check(GrInst->TrackInst.Num() == Group->InterpTracks.Num());
		UInterpTrackInstMove* MoveInst = CastChecked<UInterpTrackInstMove>( GrInst->TrackInst[TrackIndex] );

		FTransform RefTM = MoveTrack->GetMoveRefFrame( MoveInst );
		FVector LocalDelta = RefTM.InverseTransformVector(Delta);

		uint8 InterpMode = MoveTrack->PosTrack.Points[KeyIndex].InterpMode;

		if(bArriving)
		{
			MoveTrack->PosTrack.Points[KeyIndex].ArriveTangent -= LocalDelta;

			// If keeping tangents smooth, update the LeaveTangent
			if(InterpMode != CIM_CurveBreak)
			{
				MoveTrack->PosTrack.Points[KeyIndex].LeaveTangent = MoveTrack->PosTrack.Points[KeyIndex].ArriveTangent;
			}
		}
		else
		{
			MoveTrack->PosTrack.Points[KeyIndex].LeaveTangent += LocalDelta;

			// If keeping tangents smooth, update the ArriveTangent
			if(InterpMode != CIM_CurveBreak)
			{
				MoveTrack->PosTrack.Points[KeyIndex].ArriveTangent = MoveTrack->PosTrack.Points[KeyIndex].LeaveTangent;
			}
		}

		// If adjusting an 'Auto' keypoint, switch it to 'User'
		if(InterpMode == CIM_CurveAuto || InterpMode == CIM_CurveAutoClamped)
		{
			MoveTrack->PosTrack.Points[KeyIndex].InterpMode = CIM_CurveUser;
			MoveTrack->EulerTrack.Points[KeyIndex].InterpMode = CIM_CurveUser;
		}

		// Update the curve editor to see curves change.
		CurveEd->RefreshViewport();
	}
}

void FMatinee::EndDrag3DHandle()
{
	if(bDragging3DHandle)
	{
		InterpEdTrans->EndSpecial();
	}
}

void FMatinee::MoveInitialPosition(const FVector& Delta, const FRotator& DeltaRot)
{
	// If no movement track selected, do nothing. 
	if( !HasATrackSelected( UInterpTrackMove::StaticClass() ) )
	{
		return;
	}

	FRotationTranslationMatrix RotMatrix(DeltaRot, FVector(0));
	FTranslationMatrix TransMatrix(Delta);

	// Iterate only through selected movement tracks because those are the only relevant tracks. 
	for( TTrackClassTypeIterator<UInterpTrackMove> MoveTrackIter(GetSelectedTrackIterator<UInterpTrackMove>()); MoveTrackIter; ++MoveTrackIter  )
	{
		// To move the initial position, we have to track down the interp 
		// track instance corresponding to the selected movement track. 

		UInterpGroupInst* GroupInst = MatineeActor->FindFirstGroupInst( MoveTrackIter.GetGroup() );

		// Look for an instance of a movement track
		for( int32 TrackInstIndex = 0; TrackInstIndex < GroupInst->TrackInst.Num(); TrackInstIndex++ )
		{
			UInterpTrackInstMove* MoveInst = Cast<UInterpTrackInstMove>(GroupInst->TrackInst[TrackInstIndex]);

			if(MoveInst)
			{
				// Apply to reference frame of movement track.

				FMatrix ResetTM = FRotationTranslationMatrix(MoveInst->ResetRotation, MoveInst->ResetLocation);

				// Apply to reset information as well.

				FVector ResetOrigin = ResetTM.GetOrigin();
				ResetTM.SetOrigin(FVector(0));
				ResetTM = ResetTM * RotMatrix;
				ResetTM.SetOrigin(ResetOrigin);
				ResetTM = ResetTM * TransMatrix;

				MoveInst->ResetLocation = ResetTM.GetOrigin();
				MoveInst->ResetRotation = ResetTM.Rotator();
			}
		}
	}

	RefreshInterpPosition();

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();
}

// Small struct to help keep track of selected tracks
struct FSelectedTrackData
{
	UInterpTrack* Track;
	int32 SelectedIndex;
};

/**
 * Adds a keyframe to the selected track.
 *
 * There must be one and only one track selected for a keyframe to be added.
 */
void FMatinee::AddKey()
{
	// To add keyframes easier, if a group is selected with only one 
	// track, select the track so the keyframe can be placed. 
	if( GetSelectedGroupCount() == 1 )
	{
		UInterpGroup* SelectedGroup = *GetSelectedGroupIterator();

		if( SelectedGroup->InterpTracks.Num() == 1 )
		{
			// Note: We shouldn't have to deselect currently 
			// selected tracks because a group is selected. 
			const bool bDeselectPreviousTracks = false;
			const int32 FirstTrackIndex = 0;

			SelectTrack( SelectedGroup, SelectedGroup->InterpTracks[FirstTrackIndex], bDeselectPreviousTracks );
		}
	}

	if( !HasATrackSelected() )
	{
		FNotificationInfo NotificationInfo(NSLOCTEXT("UnrealEd", "NoTrackSelected", "No track selected. Select a track from the track view before trying again."));
		NotificationInfo.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(NotificationInfo);
		return;
	}

	// Array of tracks that were selected
	TArray<FSelectedTrackData> TracksToAddKeys;

	if( GetSelectedTrackCount() > 1 )
	{
		// Populate the list of tracks that we need to add keys to.
		FSelectedTrackIterator TrackIt(GetSelectedTrackIterator());
		for( ; TrackIt; ++TrackIt )
		{
			// Only allow keys to be added to multiple tracks at once if they are subtracks of a movement track.
			if( (*TrackIt)->IsA( UInterpTrackMoveAxis::StaticClass() ) ) 
			{
				FSelectedTrackData Data;
				Data.Track = *TrackIt;
				Data.SelectedIndex = TrackIt.GetTrackIndex();
				TracksToAddKeys.Add( Data );
			}
			else
			{
				TracksToAddKeys.Empty();
				break;
			}
		}

		if( TracksToAddKeys.Num() == 0 )
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "InterpEd_Track_TooManySelected", "Only 1 track can be selected for this operation.") );
		}
	}
	else
	{
		FSelectedTrackIterator TrackIt(GetSelectedTrackIterator());

		//  There is only one track selected.
		FSelectedTrackData Data;
		Data.Track = *TrackIt;
		Data.SelectedIndex = TrackIt.GetTrackIndex();
		TracksToAddKeys.Add( Data );
	}

	// A mapping of tracks to indices where keys were added
	TrackToNewKeyIndexMap.Empty();
	AddKeyInfoMap.Empty();

	if( TracksToAddKeys.Num() > 0 )
	{
		// Add keys to all tracks in the array
		for( int32 TrackIndex = 0; TrackIndex < TracksToAddKeys.Num(); ++TrackIndex )
		{
			UInterpTrack* Track = TracksToAddKeys[ TrackIndex ].Track;
			int32 SelectedTrackIndex = TracksToAddKeys[ TrackIndex ].SelectedIndex;

			UInterpTrackInst* TrInst = NULL;
			UInterpGroup* Group = Cast<UInterpGroup>(Track->GetOuter());
			if( Group )
			{
				UInterpGroupInst* GrInst = MatineeActor->FindFirstGroupInst(Group);
				check(GrInst);

				TrInst = GrInst->TrackInst[ SelectedTrackIndex ];
				check(TrInst);
			}
			else
			{
				// The track is a subtrack, get the tracks group from its parent track.
				UInterpTrack* ParentTrack = CastChecked<UInterpTrack>( Track->GetOuter() );

				Group = CastChecked<UInterpGroup>( ParentTrack->GetOuter() );
				int32 ParentTrackIndex = Group->InterpTracks.Find( ParentTrack );

				UInterpGroupInst* GrInst = MatineeActor->FindFirstGroupInst( Group );
				check(GrInst);

				TrInst = GrInst->TrackInst[ ParentTrackIndex ];
				check(TrInst);
			}

			UInterpTrackHelper* TrackHelper = NULL;
			UClass* Class = LoadObject<UClass>( NULL, *Track->GetSlateHelperClassName(), NULL, LOAD_None, NULL );
			if ( Class != NULL )
			{
				TrackHelper = Class->GetDefaultObject<UInterpTrackHelper>();
			}

			float fKeyTime = SnapTime( MatineeActor->InterpPosition, false );

			//Save off important info
			AddKeyInfo Info;
			Info.TrInst = TrInst;
			Info.TrackHelper = TrackHelper;
			Info.fKeyTime = fKeyTime;
			AddKeyInfoMap.Add(Track,Info);

			if ( (TrackHelper == NULL) || !TrackHelper->PreCreateKeyframe(Track, fKeyTime) )
			{
				//Slate window options should wind up here and return...
				return;
			}

			FinishAddKey(Track, false);
		}

		CommitAddedKeys();
	}
}

void FMatinee::FinishAddKey(UInterpTrack* Track, bool bCommitKeys)
{
	AddKeyInfo *Info = AddKeyInfoMap.Find(Track);
	if (!Info)
	{
		return;
	}

	UInterpTrackInst* TrInst = Info->TrInst;
	UInterpTrackHelper* TrackHelper = Info->TrackHelper;
	float fKeyTime = Info->fKeyTime;
	AddKeyInfoMap.Remove(Track);

	// Check if it's possible to add a keyframe to the track
	bool bAddKeyFrame = true;

	if( Track->SubTracks.Num() > 0 )
	{
		for( int32 SubTrackIndex = 0; SubTrackIndex < Track->SubTracks.Num(); ++SubTrackIndex )
		{
			bAddKeyFrame &= Track->CanAddChildKeyframe( TrInst );
		}
	}
	else
	{
		bAddKeyFrame = Track->CanAddKeyframe( TrInst );
	}

	if( bAddKeyFrame )
	{
		InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "AddKey", "Add Key")  );
		Track->Modify();
		Opt->Modify();

		if( Track->SubTracks.Num() > 0 )
		{
			// Add a keyframe to each subtrack.  We have to do this manually here because we need to know the indices where keyframes were added.
			for( int32 SubTrackIndex = 0; SubTrackIndex < Track->SubTracks.Num(); ++SubTrackIndex )
			{
				UInterpTrack* SubTrack = Track->SubTracks[ SubTrackIndex ];
				SubTrack->Modify();
				// Add key at current time, snapped to the grid if its on.
				int32 NewKeyIndex = Track->AddChildKeyframe( SubTrack, fKeyTime, TrInst, InitialInterpMode );
				check( NewKeyIndex != INDEX_NONE );
				TrackToNewKeyIndexMap.Add( SubTrack, NewKeyIndex );
			}
		}
		else
		{
			// Add key at current time, snapped to the grid if its on.
			int32 NewKeyIndex = Track->AddKeyframe( fKeyTime, TrInst, InitialInterpMode );
			checkf( NewKeyIndex != INDEX_NONE, TEXT("Could not add a key at %f to Track %s "), fKeyTime, *Track->GetName());


			// Check to see if this is going to be the event first key to have this name
			bool bCommonName = true;
			FName NewKeyName = TrackHelper->GetKeyframeAddDataName();
			if( !NewKeyName.IsNone() )
			{
				bCommonName = IData->IsEventName( NewKeyName );
			}				


			TrackHelper->PostCreateKeyframe( Track, NewKeyIndex );
			TrackToNewKeyIndexMap.Add( Track, NewKeyIndex );


			// Is this the first event key to be added with this name?
			if( !bCommonName )
			{
				// Fire a delegate so other places that use the name can also update
				FMatineeDelegates::Get().OnEventKeyframeAdded.Broadcast( MatineeActor, NewKeyName, NewKeyIndex );
			}
		}

		InterpEdTrans->EndSpecial();
	}
	else
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "NothingToKeyframe", "Nothing to keyframe, or selected object can't be keyframed on this type of track.\n") );
	}

	if (bCommitKeys)
	{
		CommitAddedKeys();	
	}
}

void FMatinee::CommitAddedKeys()
{
	if( TrackToNewKeyIndexMap.Num() > 0 )
	{
		// Select the newly added keyframes.
		ClearKeySelection();

		for( TMap<UInterpTrack*, int32>::TIterator It(TrackToNewKeyIndexMap); It; ++It)
		{
			UInterpTrack* Track = It.Key();
			int32 NewKeyIndex = It.Value();

			AddKeyToSelection(Track->GetOwningGroup(), Track, NewKeyIndex, true); // Probably don't need to auto-wind - should already be there!
		}

		// Update to current time, in case new key affects state of scene.
		RefreshInterpPosition();

	}

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();

	//empty out our temporily stored data
	TrackToNewKeyIndexMap.Empty();
}


/** 
 * Call utility to split an animation in the selected AnimControl track. 
 *
 * Only one interp track can be selected and it must be a anim control track for the function.
 */
void FMatinee::SplitAnimKey()
{
	// Only one track can be selected at a time when dealing with keyframes.
	// Also, there must be an anim control track selected.
	if( GetSelectedTrackCount() != 1 || !HasATrackSelected(UInterpTrackAnimControl::StaticClass()) )
	{
		return;
	}

	// Split keys only for anim tracks
	TTrackClassTypeIterator<UInterpTrackAnimControl> AnimTrackIt(GetSelectedTrackIterator<UInterpTrackAnimControl>());
	UInterpTrackAnimControl* AnimTrack = *AnimTrackIt;

	// Call split utility.
	int32 NewKeyIndex = AnimTrack->SplitKeyAtPosition(MatineeActor->InterpPosition);

	// If we created a new key - select it by default.
	if(NewKeyIndex != INDEX_NONE)
	{
		ClearKeySelection();
		AddKeyToSelection(AnimTrackIt.GetGroup(), AnimTrack, NewKeyIndex, false);
	}
}

/**
 * Copies the currently selected track.
 *
 * @param bCut	Whether or not we should cut instead of simply copying the track.
 */
void FMatinee::CopySelectedGroupOrTrack(bool bCut)
{
	const bool bHasAGroupSelected = HasAGroupSelected();
	const bool bHasATrackSelected = HasATrackSelected();

	if( !bHasAGroupSelected && !bHasATrackSelected )
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "InterpEd_Copy_NeedToSelectGroup", "No selected tracks or groups to copy.  Please highlight a track or group to copy by clicking on the track or group's name to the left.") );
		return;
	}

	// Sanity check. There should only be only tracks 
	// selected or only groups selected. Not both!
	check( bHasAGroupSelected ^ bHasATrackSelected );

	// Make sure to clear the buffer before adding to it again. 
	GUnrealEd->MatineeCopyPasteBuffer.Empty();

	// If no tracks are selected, copy the group.
	if( bHasAGroupSelected )
	{
		// Add all the selected groups to the copy-paste buffer
		for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
		{
			UObject* CopiedObject = (UObject*)StaticDuplicateObject( *GroupIt, GetTransientPackage() );
			GUnrealEd->MatineeCopyPasteBuffer.Add(CopiedObject);
		}

		// Delete the active group if we are doing a cut operation.
		if(bCut)
		{
			InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "InterpEd_Cut_SelectedTrackOrGroup", "Cut Selected Track or Group") );
			{
				DeleteSelectedGroups();
			}
			InterpEdTrans->EndSpecial();
		}
	}
	else
	{
		// Keep a list of all the tracks that should be deleted if the user is cutting, this doesn't include those who have selected keys
		TMultiMap<UInterpTrack*, int32> CutKeyframes;
		TArray<UInterpTrack*> DeleteTracks;

		for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
		{
			UInterpTrack* Track = *TrackIt;

			// Only allow base tracks to be copied.  Subtracks should never be copied because this could result in subtracks being pasted where they dont belong (like directly in groups).
			if( Track->GetOuter()->IsA( UInterpGroup::StaticClass() ) )
			{
				UInterpTrack* CopiedTrack = (UInterpTrack*)StaticDuplicateObject(Track, GetTransientPackage());
				
				// If we have keyframes selected in this track, make sure only those are included in the copy
				if ( Opt->SelectedKeys.Num() > 0 )
				{
					// Make a list of all the keys we want to keep
					TArray< int32 > ValidKeys;
					for( int32 iSelectedKey = 0; iSelectedKey < Opt->SelectedKeys.Num(); iSelectedKey++ )
					{
						const FInterpEdSelKey& rSelKey = Opt->SelectedKeys[iSelectedKey];
						if ( rSelKey.Track == Track )
						{
							ValidKeys.Add( rSelKey.KeyIndex );
						}
					}

					// Only remove superfluous keys if we have any for this track
					if ( ValidKeys.Num() > 0 )
					{
						check( CopiedTrack->GetNumKeyframes() == Track->GetNumKeyframes() );
						for( int32 iKeyIndex = CopiedTrack->GetNumKeyframes(); iKeyIndex >= 0; iKeyIndex-- )
						{
							if ( !ValidKeys.Contains( iKeyIndex ) )
							{
								CopiedTrack->RemoveKeyframe( iKeyIndex );
							}
							else if( bCut )
							{
								CutKeyframes.Add( Track, iKeyIndex );
							}
						}
					}
					else
					{
						DeleteTracks.Add( Track );
					}
				}
				else
				{
					DeleteTracks.Add( Track );
				}

				GUnrealEd->MatineeCopyPasteBuffer.Add( CopiedTrack );	
			}
		}

		// Delete the originating track if we are cutting and it hasn't had keys copied from it
		if( bCut && ( DeleteTracks.Num() > 0 || CutKeyframes.Num() > 0 ) )
		{
			if ( DeleteTracks.Num() > 0 )
			{
				// Deselect all tracks 
				DeselectAllTracks();

				// Only select the tracks that were valid to cut
				for( int32 TrackIndex = 0; TrackIndex < DeleteTracks.Num(); ++TrackIndex )
				{
					UInterpTrack* Track = DeleteTracks[ TrackIndex ];
					SelectTrack( Track->GetOwningGroup(), Track, false );
				}
			}

			if ( CutKeyframes.Num() > 0 )
			{
				// Deselect all keys
				ClearKeySelection();

				// Only select the keys that were valid to cut
				for(auto It(CutKeyframes.CreateIterator()); It; ++It)
				{
					UInterpTrack* Track = It.Key();
					const int32 KeyIndex = It.Value();
					AddKeyToSelection(Track->GetOwningGroup(), Track, KeyIndex, false);
				}
			}

			InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "InterpEd_Cut_SelectedTrackOrGroup", "Cut Selected Track or Group") );
			{
				// Transact all the cut key frames
				if ( CutKeyframes.Num() > 0 )
				{
					DeleteSelectedKeys(true);
				}

				// Followed by the deleted tracks
				if ( DeleteTracks.Num() > 0 )
				{
					DeleteSelectedTracks();
				}
			}
			InterpEdTrans->EndSpecial();
		}
	}
}

/**
 * Pastes the previously copied track.
 */
void FMatinee::PasteSelectedGroupOrTrack()
{
	// See if we are pasting a track or group.
	if(GUnrealEd->MatineeCopyPasteBuffer.Num())
	{
		// Variables only used when pasting tracks.
		UInterpGroup* GroupToPasteTracks = NULL;
		TArray<UInterpTrack*> TracksToSelect;
		FText ErrorMsg = FText::GetEmpty();
		
		for( TArray<UObject*>::TIterator BufferIt(GUnrealEd->MatineeCopyPasteBuffer); BufferIt; ++BufferIt )
		{
			UObject* CurrentObject = *BufferIt;

			if( CurrentObject->IsA(UInterpGroup::StaticClass()) )
			{
				DuplicateGroup(CastChecked<UInterpGroup>(CurrentObject));
			}
			else if( CurrentObject->IsA(UInterpTrack::StaticClass()) )
			{	
				UInterpTrack* CurrentTrack = CastChecked< UInterpTrack >( CurrentObject );

				int32 GroupsSelectedCount = GetSelectedGroupCount();

				if( GroupsSelectedCount == 1 )
				{
					UInterpTrack* TrackToPaste = CastChecked<UInterpTrack>(CurrentObject);

					InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "InterpEd_Paste_SelectedTrackOrGroup", "Paste Selected Track or Group.") );
					{
						if( NULL == GroupToPasteTracks )
						{
							GroupToPasteTracks = *GetSelectedGroupIterator();
						}

						GroupToPasteTracks->Modify();

						// Defer selection of the pasted track so the group is not deselected, 
						// which would cause all other tracks to fail when pasting. 
						const bool bSelectTrack = false;
						UInterpTrack* PastedTrack = AddTrackToSelectedGroup(TrackToPaste->GetClass(), TrackToPaste, bSelectTrack);

						// Save off the created track so we can select it later. 
						if( NULL != PastedTrack )
						{
							TracksToSelect.Add(PastedTrack);
						}
					}
					InterpEdTrans->EndSpecial();
				}
				else if ( CurrentTrack->GetNumKeyframes() == 1 )
				{
					// Special case pasting, if the track only has one keyframe, assume the user is just interested in pasting that
					TArray<UInterpTrack*> ValidTracks;
					for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
					{
						UInterpTrack* Track = *TrackIt;

						// Only allow this if the class the same type, due to uniqueness of the keys
						if ( Track->GetClass() == CurrentTrack->GetClass() )
						{
							ValidTracks.Add( Track );
						}
					}

					if ( ValidTracks.Num() > 0 )
					{
						// Make a list of any tracks which can be pasted into so we can setup a transaction
						InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "InterpEd_Paste_SelectedKeyframe", "Paste Selected Keyframe") );
						{
							// For each track, duplicate the keyframe into it
							for( int32 iTrackIndex = 0; iTrackIndex < ValidTracks.Num(); iTrackIndex++ )
							{
								UInterpTrack* Track = ValidTracks[iTrackIndex];

								// Check to see if there is already a key at the interp position in the destination track, and adjust it accordingly
								float KeyTime = MatineeActor->InterpPosition;
								while ( Track->GetKeyframeIndex( KeyTime ) != INDEX_NONE )
								{
									KeyTime += (float)DuplicateKeyOffset/PixelsPerSec;
								}

								// Add the keyframe to this track
								Track->Modify();
								CurrentTrack->DuplicateKeyframe( 0, KeyTime, Track );
							}
						}
						InterpEdTrans->EndSpecial();
					}
					else
					{
						ErrorMsg = NSLOCTEXT("UnrealEd", "InterpEd_Paste_NeedToSameTrack", "No track of similar type selected.  Please select a track of the same type as the keyframe was copied from.");
					}
				}
				else if( GroupsSelectedCount < 1 )
				{
					ErrorMsg = NSLOCTEXT("UnrealEd", "InterpEd_Paste_NeedToSelectGroup", "No selected groups to paste into.  Please highlight a group to copy by clicking on the group's name to the left.");
				}
				else if( GroupsSelectedCount > 1 )
				{
					ErrorMsg = NSLOCTEXT("UnrealEd", "InterpEd_Paste_OneGroup", "Can only have one group selected when pasting.");
				}
			}
		}

		// If an error occurred, display it now
		if ( !ErrorMsg.IsEmpty() )
		{
			FMessageDialog::Open( EAppMsgType::Ok, ErrorMsg );
		}

		// If we pasted tracks to a group, then we still need to select them.
		if( NULL != GroupToPasteTracks )
		{
			// Don't deselect previous tracks because (1) if a group was selected, then no other tracks 
			// were selected and (2) we don't want to deselect tracks we just selected in the loop.
			const bool bDeselectPreviousTracks = false;
			for( int32 TrackIndex = 0; TrackIndex < TracksToSelect.Num(); ++TrackIndex )
			{
				SelectTrack(GroupToPasteTracks, TracksToSelect[TrackIndex], bDeselectPreviousTracks);
			}
		}
	}
}

/**
 * @return Whether or not we can paste a track/group.
 */
bool FMatinee::CanPasteGroupOrTrack()
{
	bool bResult = false;

	// Make sure we at least have something in the buffer.
	// Camera anims cant paste items
	if( GUnrealEd->MatineeCopyPasteBuffer.Num() && !IsCameraAnim() )
	{
		// We don't currently support pasting on multiple groups or tracks.
		// So, we have have to make sure we have one group or one track selected.
		const bool bCanPasteOnGroup = (GetSelectedGroupCount() < 2);
		const bool bCanPasteOnTrack = (GetSelectedTrackCount() == 1);

		// Copy-paste can only happen if only one group OR only one track is selected.
		// We cannot paste if there is one track and one group selected.
		if( bCanPasteOnGroup ^ bCanPasteOnTrack )
		{
			bResult = true;

			// Can we paste on top of a group?
			if( bCanPasteOnGroup )
			{
				for( TArray<UObject*>::TIterator BufferIt(GUnrealEd->MatineeCopyPasteBuffer); BufferIt; ++BufferIt )
				{
					const bool bIsAGroup = (*BufferIt)->IsA(UInterpGroup::StaticClass());
					const bool bIsATrack = (*BufferIt)->IsA(UInterpTrack::StaticClass());

					// We can paste groups or tracks on top of selected groups. If there 
					// is one object in the buffer that isn't either, then we can't paste.
					if( !bIsAGroup && !bIsATrack )
					{
						bResult = false;
						break;
					}
				}
			}
			// Can we paste on top of a track?
			else
			{
				for( TArray<UObject*>::TIterator BufferIt(GUnrealEd->MatineeCopyPasteBuffer); BufferIt; ++BufferIt )
				{
					const bool bIsATrack = (*BufferIt)->IsA(UInterpTrack::StaticClass());

					// We can only paste tracks on top of tracks. If there exists any other 
					// objects in the buffer that aren't tracks, then we can't paste.
					if( !bIsATrack )
					{
						bResult = false;
						break;
					}
				}
			}
		}
		else if( bCanPasteOnTrack )
		{
			bResult = true;

			// Special case, allow pasting into tracks if the track we have copied only has one keyframe in it
			for( TArray<UObject*>::TIterator BufferIt(GUnrealEd->MatineeCopyPasteBuffer); BufferIt; ++BufferIt )
			{
				UInterpTrack* Track = Cast<UInterpTrack>(*BufferIt);

				// We can only paste keyframes into tracks. If there exists any other 
				// objects in the buffer that aren't tracks, or we have too many keyframes then we can't paste.
				if( !Track || Track->GetNumKeyframes() != 1 )
				{
					bResult = false;
					break;
				}
			}
		}
	}

	return bResult;
}


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
UInterpTrack* FMatinee::AddTrackToGroup( UInterpGroup* Group, UClass* TrackClass, UInterpTrack* TrackToCopy, bool bAllowPrompts, int32& OutNewTrackIndex, bool bSelectTrack /*= true*/ )
{
	OutNewTrackIndex = INDEX_NONE;

	if( Group == NULL )
	{
		return NULL;
	}

	UInterpGroupInst* GrInst = MatineeActor->FindFirstGroupInst( Group );
	check(GrInst);

	UInterpTrack* TrackDef = TrackClass->GetDefaultObject<UInterpTrack>();

	UInterpTrackHelper* TrackHelper = NULL;
	
	bool bCopyingTrack = (TrackToCopy!=NULL);

	UClass	*Class = LoadObject<UClass>( NULL, *TrackDef->GetSlateHelperClassName(), NULL, LOAD_None, NULL );
	if ( Class != NULL )
	{
		TrackHelper = Class->GetDefaultObject<UInterpTrackHelper>();
	}

	if ( (TrackHelper == NULL) || !TrackHelper->PreCreateTrack( Group, TrackDef, bCopyingTrack, bAllowPrompts ) )
	{
		return NULL;
	}

	Group->Modify();

	// Construct track and track instance objects.
	UInterpTrack* NewTrack = NULL;
	if(TrackToCopy)
	{
		NewTrack = Cast<UInterpTrack>(StaticDuplicateObject( TrackToCopy, Group ));
	}
	else
	{
		NewTrack = NewObject<UInterpTrack>(Group, TrackClass, NAME_None, RF_Transactional);
	}

	check(NewTrack);

	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.Matinee.NewTrack"), TEXT("Class"), TrackClass->GetName());
	}

	OutNewTrackIndex = Group->InterpTracks.Add(NewTrack);

	check( NewTrack->TrackInstClass );
	check( NewTrack->TrackInstClass->IsChildOf(UInterpTrackInst::StaticClass()) );

	TrackHelper->PostCreateTrack( NewTrack, bCopyingTrack, OutNewTrackIndex );

	if(bCopyingTrack == false)
	{
		NewTrack->SetTrackToSensibleDefault();
	}

	NewTrack->Modify();

	// We need to create a InterpTrackInst in each instance of the active group (the one we are adding the track to).
	for(int32 i=0; i<MatineeActor->GroupInst.Num(); i++)
	{
		UInterpGroupInst* GroupInst = MatineeActor->GroupInst[i];
		if(GroupInst->Group == Group)
		{
			GroupInst->Modify();

			UInterpTrackInst* NewTrackInst = NewObject<UInterpTrackInst>(GroupInst, NewTrack->TrackInstClass, NAME_None, RF_Transactional);

			const int32 NewInstIndex = GroupInst->TrackInst.Add(NewTrackInst);
			check(NewInstIndex == OutNewTrackIndex);

			// Initialize track, giving selected object.
			NewTrackInst->InitTrackInst(NewTrack);

			// Save state into new track before doing anything else (because we didn't do it on ed mode change).
			NewTrackInst->SaveActorState(NewTrack);
			NewTrackInst->Modify();
		}
	}

	if(bCopyingTrack == false)
	{
		// Bit of a hack here, but useful. Whenever you put down a movement track, add a key straight away at the start.
		// Should be ok to add at the start, because it should not be having its location (or relative location) changed in any way already as we scrub.
		UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(NewTrack);
		if(MoveTrack)
		{
			UInterpGroupInst* GroupInst = MatineeActor->FindFirstGroupInst(Group);
			UInterpTrackInst* TrInst = GroupInst->TrackInst[OutNewTrackIndex];
			MoveTrack->AddKeyframe( 0.0f, TrInst, InitialInterpMode );
		}
	}

	if ( bSelectTrack )
	{
		SelectTrack( Group, NewTrack );
	}
	return NewTrack;
}

/**
 * Adds a new track to the selected group.
 *
 * @param TrackClass		The class of the track we are adding.
 * @param TrackToCopy		A optional track to copy instead of instantiating a new one.  If NULL, a new track will be instantiated.
 * @param bSelectTrack		If true, select the track after adding it
 *
 * @return	The newly-created track if created; NULL, otherwise. 
 */
UInterpTrack* FMatinee::AddTrackToSelectedGroup(UClass* TrackClass, UInterpTrack* TrackToCopy, bool bSelectTrack /* = true */)
{
	// In order to add a track to a group, there can only be one group selected.
	check( GetSelectedGroupCount() == 1 );
	UInterpGroup* Group = *GetSelectedGroupIterator();

	return AddTrackToGroupAndRefresh(Group, NSLOCTEXT( "UnrealEd", "NewTrack", "NewTrack").ToString(), TrackClass, TrackToCopy, bSelectTrack);
}

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
UInterpTrack* FMatinee::AddTrackToGroupAndRefresh(UInterpGroup* Group, const FString& NewTrackName, UClass* TrackClass, UInterpTrack* TrackToCopy, bool bSelectTrack /* = true */)
{
	UInterpTrack* TrackDef = TrackClass->GetDefaultObject<UInterpTrack>();

	// If bOnePerGrouop - check we don't already have a track of this type in the group.
	if(TrackDef->bOnePerGroup)
	{
		DisableTracksOfClass(Group, TrackClass);
	}

	// Warn when creating dynamic track on a static actor, warn and offer to bail out.
	if(TrackDef->AllowStaticActors()==false)
	{
		UInterpGroupInst* GrInst = MatineeActor->FindFirstGroupInst(Group);
		check(GrInst);

		AActor* GrActor = GrInst->GetGroupActor();
		if(GrActor && GrActor->IsRootComponentStatic())
		{
			const bool bConfirm = EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo, NSLOCTEXT("UnrealEd", "WarnNewMoveTrackOnStatic", "WARNING: The track you are creating requires a Dynamic Actor, but the currently active group is using a Static Actor.\nAre you sure you want to create the track?") );
			if(!bConfirm)
			{
				return NULL;
			}
		}
	}

	InterpEdTrans->BeginSpecial( FText::FromString( NewTrackName ) );

	// Add the track!
	int32 NewTrackIndex = INDEX_NONE;
	UInterpTrack* ReturnTrack = AddTrackToGroup( Group, TrackClass, TrackToCopy, true, NewTrackIndex, bSelectTrack );
	if (ReturnTrack)
	{
		ReturnTrack->EnableTrack( true );
	}

	InterpEdTrans->EndSpecial();


	if( NewTrackIndex != INDEX_NONE )
	{
		// Make sure particle replay tracks have up-to-date editor-only transient state
		UpdateParticleReplayTracks();

		// A new track may have been added, so we'll need to update the scroll bar
		UpdateTrackWindowScrollBars();

		// Update graphics to show new track!
		InvalidateTrackWindowViewports();

		// If we added a movement track to this group, we'll need to make sure that the actor's transformations are captured
		// so that we can restore them later.
		MatineeActor->RecaptureActorState();
	}

	return ReturnTrack;
}

/** 
 * Deletes the currently active track. 
 */
void FMatinee::DeleteSelectedTracks()
{
	// This function should only be called if there is at least one selected track.
	check( HasATrackSelected() );

	InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "TrackDelete", "Track Delete") );
	MatineeActor->Modify();
	IData->Modify();

	// Deselect everything.
	ClearKeySelection();

	// Take a copy of all the valid event names
	TArray<FName> OldEventNames;
	MatineeActor->MatineeData->GetAllEventNames(OldEventNames);

	for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		UInterpTrack* ActiveTrack = *TrackIt;

		// Only allow base tracks to be deleted,  Subtracks will be deleted by their parent
		if( ActiveTrack->GetOuter()->IsA( UInterpGroup::StaticClass() ) )
		{
			if (FEngineAnalytics::IsAvailable())
			{
				FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.Matinee.DelTrack"), TEXT("Class"), ActiveTrack->GetClass()->GetName());
			}

			ActiveTrack->Modify();

			UInterpGroup* Group = TrackIt.GetGroup();
			Group->Modify();

			const int32 TrackIndex = TrackIt.GetTrackIndex();

			for(int32 i=0; i<MatineeActor->GroupInst.Num(); i++)
			{
				UInterpGroupInst* GrInst = MatineeActor->GroupInst[i]; 
				if( GrInst->Group == Group )
				{
					UInterpTrackInst* TrInst = GrInst->TrackInst[TrackIndex];

					GrInst->Modify();
					TrInst->Modify();

					// Before deleting this track - find each instance of it and restore state.
					TrInst->RestoreActorState( GrInst->Group->InterpTracks[TrackIndex] );

					// Clean up the track instance
					TrInst->TermTrackInst( GrInst->Group->InterpTracks[TrackIndex] );

					GrInst->TrackInst.RemoveAt(TrackIndex);
				}
			}

			AActor* GroupActor = MatineeActor->FindFirstGroupInst(Group)->GetGroupActor();

			// Remove from the Curve editor, if its there.
			IData->CurveEdSetup->RemoveCurve(ActiveTrack);
			// Remove any subtrack curves if the parent is being removed
			for( int32 SubTrackIndex = 0; SubTrackIndex < ActiveTrack->SubTracks.Num(); ++SubTrackIndex )
			{
				IData->CurveEdSetup->RemoveCurve( ActiveTrack->SubTracks[ SubTrackIndex ] );
			}

			// Finally, remove the track completely. 
			// WARNING: Do not deference or use this iterator after the call to RemoveCurrent()!
			TrackIt.RemoveCurrent();
		}
	}

	IData->UpdateEventNames();

	// Take another copy of all the valid event names
	TArray<FName> RemainingEventNames;
	MatineeActor->MatineeData->GetAllEventNames(RemainingEventNames);

	// Check to see which event name no longer exist
	TArray<FName> RemovedEventNames;
	for( int32 EventNameIndex = 0; EventNameIndex < OldEventNames.Num(); ++EventNameIndex )
	{
		const FName& OldEventName = OldEventNames[EventNameIndex];
		if ( !RemainingEventNames.Contains(OldEventName) )
		{
			RemovedEventNames.Add(OldEventName);
		}
	}
	if ( RemovedEventNames.Num() > 0 )
	{
		// Fire a delegate so other places that use the name can also update
		FMatineeDelegates::Get().OnEventKeyframeRemoved.Broadcast( MatineeActor, RemovedEventNames );
	}

	InterpEdTrans->EndSpecial();

	// Update the curve editor
	CurveEd->CurveChanged();

	// A track may have been deleted, so we'll need to update our track window scroll bar
	UpdateTrackWindowScrollBars();

	// Update the property window to reflect the change in selection.
	UpdatePropertyWindow();

	MatineeActor->RecaptureActorState();
}

/** 
 * Deletes all selected groups.
 */
void FMatinee::DeleteSelectedGroups()
{
	// There must be one group selected to use this funciton.
	check( HasAGroupSelected() );

	InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "GroupDelete", "Group Delete") );
	MatineeActor->Modify();
	IData->Modify();

	// Deselect everything.
	ClearKeySelection();

	// Take a copy of all the valid event names
	TArray<FName> OldEventNames;
	MatineeActor->MatineeData->GetAllEventNames(OldEventNames);

	for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		UInterpGroup* GroupToDelete = *GroupIt;
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.Matinee.DelGroup"), TEXT("Name"), GroupToDelete->GroupName.ToString());
		}

		// Mark InterpGroup and all InterpTracks as Modified.
		GroupToDelete->Modify();
		for(int32 j=0; j<GroupToDelete->InterpTracks.Num(); j++)
		{
			UInterpTrack* Track = GroupToDelete->InterpTracks[j];
			if (FEngineAnalytics::IsAvailable())
			{
				FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.Matinee.DelTrack"), TEXT("Class"), Track->GetClass()->GetName());
			}

			Track->Modify();

			// Remove from the Curve editor, if its there.
			IData->CurveEdSetup->RemoveCurve(Track);
		}
		
		// First, destroy any instances of this group.
		int32 i=0;
		while( i<MatineeActor->GroupInst.Num() )
		{
			UInterpGroupInst* GrInst = MatineeActor->GroupInst[i];
			if(GrInst->Group == GroupToDelete)
			{
				// Mark InterpGroupInst and all InterpTrackInsts as Modified.
				GrInst->Modify();
				for(int32 j=0; j<GrInst->TrackInst.Num(); j++)
				{
					GrInst->TrackInst[j]->Modify();
				}

				// Restore all state in this group before exiting
				GrInst->RestoreGroupActorState();

				// Clean up GroupInst
				GrInst->TermGroupInst(false);
				// Don't actually delete the TrackInsts - but we do want to call TermTrackInst on them.

				// Remove from the MatineeActor's list of GroupInsts
				MatineeActor->GroupInst.RemoveAt(i);
			}
			else
			{
				i++;
			}
		}

		MatineeActor->DeleteGroupinfo(GroupToDelete);

		// We're being deleted, so we need to unparent any child groups
		// @todo: Should we support optionally deleting all sub-groups when deleting the parent?
		for( int32 CurGroupIndex = IData->InterpGroups.Find( GroupToDelete ) + 1; CurGroupIndex < IData->InterpGroups.Num(); ++CurGroupIndex )
		{
			UInterpGroup* CurGroup = IData->InterpGroups[ CurGroupIndex ];
			if( CurGroup->bIsParented )
			{
				CurGroup->Modify();

				// Unparent this child
				CurGroup->bIsParented = false;
			}
			else
			{
				// We've reached a root object, so we're done processing children.  Bail!
				break;
			}
		}

		// Prevent group from being selected as well as any tracks associated to the group.
		// WARNING: Do not deference or use this iterator after the call to RemoveCurrent()!
		GroupIt.RemoveCurrent();
	}

	IData->UpdateEventNames();

	// Take another copy of all the valid event names
	TArray<FName> RemainingEventNames;
	MatineeActor->MatineeData->GetAllEventNames(RemainingEventNames);

	// Check to see which event name no longer exist
	TArray<FName> RemovedEventNames;
	for( int32 EventNameIndex = 0; EventNameIndex < OldEventNames.Num(); ++EventNameIndex )
	{
		const FName& OldEventName = OldEventNames[EventNameIndex];
		if ( !RemainingEventNames.Contains(OldEventName) )
		{
			RemovedEventNames.Add(OldEventName);
		}
	}
	if ( RemovedEventNames.Num() > 0 )
	{
		// Fire a delegate so other places that use the name can also update
		FMatineeDelegates::Get().OnEventKeyframeRemoved.Broadcast( MatineeActor, RemovedEventNames );
	}

	// Tell curve editor stuff might have changed.
	CurveEd->CurveChanged();

	// A group may have been deleted, so we'll need to update our track window scroll bar
	UpdateTrackWindowScrollBars();

	// Deselect everything.
	ClearKeySelection();

	InterpEdTrans->EndSpecial();

	// Stop having the camera locked to this group if it currently is.
	if( CamViewGroup && IsGroupSelected(CamViewGroup) )
	{
		LockCamToGroup(NULL);
	}

	// Update the property window to reflect the change in selection.
	UpdatePropertyWindow();

	// Reimage actor world locations.  This must happen after the group was removed.
	MatineeActor->RecaptureActorState();
}

/**
 * Disables all tracks of a class type in this group
 * @param Group - group in which to disable tracks of TrackClass type
 * @param TrackClass - Type of track to disable
 */
void FMatinee::DisableTracksOfClass(UInterpGroup* Group, UClass* TrackClass)
{
	for( int32 TrackIndex = 0; TrackIndex < Group->InterpTracks.Num(); TrackIndex++ )
	{
		if( Group->InterpTracks[TrackIndex]->GetClass() == TrackClass )
		{
			Group->InterpTracks[TrackIndex]->EnableTrack( false );
		}
	}
}

void FMatinee::UpdatePreviewCamera( UInterpGroup* AssociatedGroup ) const
{
	UInterpGroupDirector* DirGroup = Cast<UInterpGroupDirector>(AssociatedGroup);
	if ( DirGroup )
	{
		UInterpTrackDirector* DirTrack = DirGroup->GetDirectorTrack();
		if ( DirTrack )
		{
			UpdatePreviewCamera( DirTrack );
		}
	}
}

void FMatinee::UpdatePreviewCamera( UInterpTrack* AssociatedTrack ) const
{
	UInterpTrackDirector* DirTrack = Cast<UInterpTrackDirector>(AssociatedTrack);
	if ( DirTrack )
	{
		// If the track selection state has changed, update our camera actor
		UInterpGroupDirector* DirGroup = CastChecked<UInterpGroupDirector>( DirTrack->GetOuter() );
		const bool TrackOrGroupSelected = ( DirTrack->IsSelected() | DirGroup->IsSelected() );
		DirTrack->UpdatePreviewCamera( MatineeActor, TrackOrGroupSelected );
	}
}

/**
 * Duplicates the specified group
 *
 * @param GroupToDuplicate		Group we are going to duplicate.
 */
void FMatinee::DuplicateGroup(UInterpGroup* GroupToDuplicate)
{
	if(GroupToDuplicate==NULL)
	{
		return;
	}

	FName NewGroupName;

	// See if we are duplicating a director group.
	bool bDirGroup = GroupToDuplicate->IsA(UInterpGroupDirector::StaticClass());

	// If we are a director group, make sure we don't have a director group yet in our interp data.
	if(bDirGroup)
	{
		 UInterpGroupDirector* DirGroup = IData->FindDirectorGroup();

		 if(DirGroup)
		 {
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "UnableToPasteOnlyOneDirectorGroup", "Unable to complete paste operation.  You can only have 1 director group per UnrealMatinee.") );
			return;
		 }
	}
	else
	{
		const FText GroupName = FText::FromName( GroupToDuplicate->GroupName );

		const FText DialogTitle = GroupToDuplicate->bIsFolder ? NSLOCTEXT("UnrealEd", "NewFolderNameWindowTitle", "New Folder Name") : NSLOCTEXT("UnrealEd", "NewGroupNameWindowTitle", "New Group Name");

		FText ResultText = GenericTextEntryModal(DialogTitle, DialogTitle, GroupName);

		//@todo We shouldn't be changing the name out from under them. Instead let them know that spaces aren't valid when entering the new name [9/30/2013 justin.sargent]
		FString TempString = ResultText.ToString();
		TempString = TempString.Replace(TEXT(" "),TEXT("_"));
		NewGroupName = FName( *TempString );
	}

	

	// Begin undo transaction
	InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "NewGroup", "New Group")  );
	{
		MatineeActor->Modify();
		IData->Modify();

		// Create new InterpGroup.
		UInterpGroup* NewGroup = (UInterpGroup*)StaticDuplicateObject( GroupToDuplicate, IData, NAME_None, RF_Transactional );

		if(!bDirGroup)
		{
			NewGroup->GroupName = NewGroupName;
		}
		IData->InterpGroups.Add(NewGroup);


		// All groups must have a unique name.
		NewGroup->EnsureUniqueName();

		// Randomly generate a group colour for the new group.
		NewGroup->GroupColor = FColor::MakeRandomColor();
		NewGroup->Modify();

		// Pasted groups are always unparented.  If we wanted to support pasting a group into a folder, we'd
		// need to be sure to insert the new group in the appropriate place in the group list.
		NewGroup->bIsParented = false;

		// Create new InterpGroupInst
		UInterpGroupInst* NewGroupInst = NULL;

		if(bDirGroup)
		{
			NewGroupInst = NewObject<UInterpGroupInstDirector>(MatineeActor, NAME_None, RF_Transactional);
		}
		else
		{
			NewGroupInst = NewObject<UInterpGroupInst>(MatineeActor, NAME_None, RF_Transactional);
		}

		// Initialise group instance, saving ref to actor it works on.
		NewGroupInst->InitGroupInst(NewGroup, NULL);

		const int32 NewGroupInstIndex = MatineeActor->GroupInst.Add(NewGroupInst);
		MatineeActor->InitGroupActorForGroup(NewGroup, NULL);

		NewGroupInst->Modify();


		// If a director group, create a director track for it now.
		if(bDirGroup)
		{
			UInterpGroupDirector* DirGroup = Cast<UInterpGroupDirector>(NewGroup);
			check(DirGroup);

			// See if the director group has a director track yet, if not make one and make the corresponding track inst as well.
			UInterpTrackDirector* NewDirTrack = DirGroup->GetDirectorTrack();
			
			if(NewDirTrack==NULL)
			{
				NewDirTrack = NewObject<UInterpTrackDirector>(NewGroup, NAME_None, RF_Transactional);
				NewGroup->InterpTracks.Add(NewDirTrack);

				UInterpTrackInst* NewDirTrackInst = NewObject<UInterpTrackInstDirector>(NewGroupInst, NAME_None, RF_Transactional);
				NewGroupInst->TrackInst.Add(NewDirTrackInst);

				NewDirTrackInst->InitTrackInst(NewDirTrack);
				NewDirTrackInst->SaveActorState(NewDirTrack);

				// Save for undo then redo.
				NewDirTrackInst->Modify();
				NewDirTrack->Modify();
			}
		}

		// Select the group we just duplicated
		SelectGroup(NewGroup);
	}
	InterpEdTrans->EndSpecial();

	// A new group may have been added (via duplication), so we'll need to update our scroll bar
	UpdateTrackWindowScrollBars();

	// Update graphics to show new group.
	InvalidateTrackWindowViewports();

	// If adding a camera- make sure its frustum colour is updated.
	UpdateCamColours();

	// Reimage actor world locations.  This must happen after the group was created.
	MatineeActor->RecaptureActorState();
}

/**
 * Duplicates selected tracks in their respective groups and clears them to begin real time recording, and selects them 
 * @param bInDeleteSelectedTracks - true if the currently selected tracks should be destroyed when recording begins
 */

void FMatinee::DuplicateSelectedTracksForRecording (const bool bInDeleteSelectedTracks)
{
	TArray <UInterpGroup*> OwnerGroups;
	TArray <int32> RecordTrackIndex;
	TArray <UInterpTrack*> OldSelectedTracks;
	for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		UInterpTrack* TrackToCopy = *TrackIt;
		check(TrackToCopy);
		//make sure we support this type of track for duplication
		if ((Cast<UInterpTrackMove>(TrackToCopy)==NULL) && (Cast<UInterpTrackFloatProp>(TrackToCopy)==NULL))
		{
			//not supporting this track type for now
			continue;
		}
		
		OldSelectedTracks.Add(TrackToCopy);

		UInterpGroup* OwnerGroup = TrackIt.GetGroup();

		FString NewTrackName = FText::Format( NSLOCTEXT( "UnrealEd", "CaptureTrack", "{0}CT"), FText::FromString(TrackToCopy->GetSlateHelperClassName()) ).ToString();
		UInterpTrack* NewTrack = AddTrackToGroupAndRefresh(OwnerGroup, NewTrackName, TrackToCopy->GetClass(), TrackToCopy, false);
		if (NewTrack)
		{
			RecordingTracks.Add(NewTrack);
			OwnerGroups.Add(OwnerGroup);
			RecordTrackIndex.Add(OwnerGroup->InterpTracks.Find(NewTrack));
			
			//guard around movement tracks being relative
			int32 FinalIndex = 0;
			UInterpTrackMove* MovementTrack = Cast<UInterpTrackMove>(NewTrack);

			//remove all keys
			for (int32 KeyFrameIndex = NewTrack->GetNumKeyframes()-1; KeyFrameIndex >= FinalIndex; --KeyFrameIndex)
			{
				NewTrack->RemoveKeyframe(KeyFrameIndex);
			}

			// remove all subtrack keys.  We cant do this inside the parent tracks remove keyframe as the keyframe index does not
			// necessarily represent a valid index in a subtrack.
			for( int32 SubTrackIndex = 0; SubTrackIndex < NewTrack->SubTracks.Num(); ++SubTrackIndex )
			{
				UInterpTrack* SubTrack = NewTrack->SubTracks[ SubTrackIndex ];
				for (int32 KeyFrameIndex = SubTrack->GetNumKeyframes()-1; KeyFrameIndex >= FinalIndex; --KeyFrameIndex)
				{
					SubTrack->RemoveKeyframe(KeyFrameIndex);
				}
			}
			NewTrack->TrackTitle = NewTrackName;
			// Make sure the curve editor is in sync
			CurveEd->CurveChanged();
		}
	}

	if (bInDeleteSelectedTracks)
	{
		DeleteSelectedTracks();
	}

	//empty selection
	DeselectAllTracks(false);
	DeselectAllGroups(false);
	
	//add all copied tracks to selection
	const bool bDeselectOtherTracks = false;
	bool bNewGroupSelected = false;
	for (int32 i = 0; i < RecordingTracks.Num(); ++i)
	{
		UInterpTrack* TrackToSelect = RecordingTracks[i];
		UInterpGroup* OwnerGroup = OwnerGroups[i];

		UInterpTrackMove* TrackToSelectAsMoveTrack = Cast<UInterpTrackMove>(TrackToSelect);

		if (!bNewGroupSelected && OwnerGroup && (TrackToSelectAsMoveTrack!= NULL))
		{
			//set this group as the preview group
			LockCamToGroup(OwnerGroup);
			bNewGroupSelected = true;
		}

		SelectTrack( OwnerGroup, TrackToSelect, bDeselectOtherTracks);
	}

	// Update the property window to reflect the group deselection
	UpdatePropertyWindow();
	// Request an update of the track windows
	InvalidateTrackWindowViewports();
}

/**Used during recording to capture a key frame at the current position of the timeline*/
void FMatinee::RecordKeys(void)
{
	for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		UInterpTrack* TrackToSample = *TrackIt;
		UInterpGroup* ParentGroup = TrackIt.GetGroup();

		UInterpGroupInst* GrInst = MatineeActor->FindFirstGroupInst(ParentGroup);
		check(GrInst);
		UInterpTrackInst* TrInst = GrInst->TrackInst[TrackIt.GetTrackIndex()];
		check(TrInst);

		UInterpTrackHelper* TrackHelper = NULL;
		UClass* Class = LoadObject<UClass>( NULL, *TrackToSample->GetSlateHelperClassName(), NULL, LOAD_None, NULL );
		if ( Class != NULL )
		{
			TrackHelper = Class->GetDefaultObject<UInterpTrackHelper>();
		}

		float	fKeyTime = SnapTime( MatineeActor->InterpPosition, false );
		if ( (TrackHelper == NULL) || !TrackHelper->PreCreateKeyframe(TrackToSample, fKeyTime) )
		{
			continue;
		}

		TrackToSample->Modify();

		// Add key at current time, snapped to the grid if its on.
		int32 NewKeyIndex = TrackToSample->AddKeyframe( MatineeActor->InterpPosition, TrInst, InitialInterpMode );
		TrackToSample->UpdateKeyframe( NewKeyIndex, TrInst);
	}

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();
}


/**Store off parent positions so we can apply the parents delta of movement to the child*/
void FMatinee::SaveRecordingParentOffsets(void)
{
	RecordingParentOffsets.Empty();
	if (RecordMode == MatineeConstants::ERecordMode::RECORD_MODE_NEW_CAMERA_ATTACHED)
	{
		for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
		{
			UInterpTrack* TrackToSample = *TrackIt;
			UInterpGroup* ParentGroup = TrackIt.GetGroup();

			UInterpGroupInst* GrInst = MatineeActor->FindFirstGroupInst(ParentGroup);
			check(GrInst);
			UInterpTrackInst* TrInst = GrInst->TrackInst[TrackIt.GetTrackIndex()];
			check(TrInst);

			//get the actor that is currently recording
			AActor* Actor = TrInst->GetGroupActor();
			if(!Actor)
			{
				return;
			}

			//get the parent actor
			/** @todo UE4 no longer using general base attachment system
			AActor* ParentActor = Actor->Base;
			if (ParentActor)
			{
				//save the offsets
				RecordingParentOffsets.Set(ParentActor, ParentActor->GetActorLocation());
			}
			*/
		}
	}
}

/**Apply the movement of the parent to child during recording*/
void FMatinee::ApplyRecordingParentOffsets(void)
{
	if (RecordMode == MatineeConstants::ERecordMode::RECORD_MODE_NEW_CAMERA_ATTACHED)
	{
		//list of unique actors to apply parent transforms to
		TArray<AActor*> RecordingActorsWithParents;
		for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
		{
			UInterpTrack* TrackToSample = *TrackIt;
			UInterpGroup* ParentGroup = TrackIt.GetGroup();

			UInterpGroupInst* GrInst = MatineeActor->FindFirstGroupInst(ParentGroup);
			check(GrInst);

			//get the actor that is currently recording
			AActor* Actor = GrInst->GetGroupActor();
			if(!Actor)
			{
				return;
			}

			/** @todo UE4 no longer using general base attachment system
			//get the parent actor
			AActor* ParentActor = Actor->Base;
			if (ParentActor)
			{
				//keep a list of actors to apply offsets to.
				RecordingActorsWithParents.AddUniqueItem(Actor);
			}
			*/
		}

		/** @todo UE4 no longer using general base attachment system
		//now apply parent offsets to list
		for (int32 i = 0; i < RecordingActorsWithParents.Num(); ++i)
		{
			AActor* Actor = RecordingActorsWithParents(i);
			check(Actor);
			
			//get parent actor
			AActor* ParentActor = Actor->Base;
			check(ParentActor);
			
			//get the old position out of the map
			FVector OldOffset = RecordingParentOffsets.FindRef(ParentActor);

			//find the delta of the parent actor
			FVector Delta = ParentActor->GetActorLocation() - OldOffset;

			//apply the delta to the actor we're recoding
			Actor->DirectSetLocation(Actor->GetActorLocation() + Delta);

			//we have to move the level viewport as well.
			ACameraActor* CameraActor = Cast<ACameraActor>(Actor);
			if (CameraActor)
			{
				//add new camera
				FLevelEditorViewportClient* LevelVC = GetRecordingViewport();
				if (LevelVC)
				{
					LevelVC->ViewLocation += Delta;
				}
			}
		}
		*/
	}
}

/**
 * Returns the custom recording viewport if it has been created yet
 * @return - NULL, if no record viewport exists yet, or the current recording viewport
 */
FLevelEditorViewportClient* FMatinee::GetRecordingViewport(void)
{
	if( MatineeRecorderWindow.IsValid() )
	{
		return MatineeRecorderWindow.Pin()->GetViewport();
	}

	return NULL;
}

/** Call utility to crop the current key in the selected track. */
void FMatinee::CropAnimKey(bool bCropBeginning)
{
	// Check we have a group and track selected
	if( HasATrackSelected() )
	{
		return;
	}

	// Check an AnimControlTrack is selected to avoid messing with the transaction system preemptively. 
	if( HasATrackSelected(UInterpTrackAnimControl::StaticClass()) )
	{
		InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "CropAnimationKey", "Crop Animation Key") );
		{
			for( TTrackClassTypeIterator<UInterpTrackAnimControl> AnimTrackIt(GetSelectedTrackIterator<UInterpTrackAnimControl>()); AnimTrackIt; ++AnimTrackIt )
			{
				UInterpTrackAnimControl* AnimTrack = *AnimTrackIt;

				// Call crop utility.
				AnimTrack->Modify();
				AnimTrack->CropKeyAtPosition(MatineeActor->InterpPosition, bCropBeginning);
			}
		}
		InterpEdTrans->EndSpecial();
	}
}

/** Jump the position of the interpolation to the current time, updating Actors. */
void FMatinee::SetInterpPosition( float NewPosition, bool Scrubbing )
{
#if WITH_EDITORONLY_DATA
	MatineeActor->bIsScrubbing = Scrubbing;
#endif

	bool bTimeChanged = (NewPosition != MatineeActor->InterpPosition);

	// Make sure particle replay tracks have up-to-date editor-only transient state
	UpdateParticleReplayTracks();

	// Move preview position in interpolation to where we want it, and update any properties
	MatineeActor->UpdateInterp( NewPosition, true, bTimeChanged );

	// When playing/scrubbing, we release the current keyframe from editing
	if(bTimeChanged)
	{
		Opt->bAdjustingKeyframe = false;
		Opt->bAdjustingGroupKeyframes = false;
	}

	// If we are locking the camera to a group, update it here
	UpdateCameraToGroup(true);

	// Set the camera frustum colours to show which is being viewed.
	UpdateCamColours();

	// Redraw viewport.
	InvalidateTrackWindowViewports();

	// Update the position of the marker in the curve view.
	CurveEd->SetPositionMarker( true, MatineeActor->InterpPosition, PosMarkerColor );

#if WITH_EDITORONLY_DATA
	MatineeActor->bIsScrubbing = false;
#endif
}

/** Make sure particle replay tracks have up-to-date editor-only transient state */
void FMatinee::UpdateParticleReplayTracks()
{
	// Check to see if InterpData exists.
	if( MatineeActor->MatineeData )
	{
		for( int32 CurGroupIndex = 0; CurGroupIndex < MatineeActor->MatineeData->InterpGroups.Num(); ++CurGroupIndex )
		{
			UInterpGroup* CurGroup = MatineeActor->MatineeData->InterpGroups[ CurGroupIndex ];
			if( CurGroup != NULL )
			{
				for( int32 CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
				{
					UInterpTrack* CurTrack = CurGroup->InterpTracks[ CurTrackIndex ];
					if( CurTrack != NULL )
					{
						UInterpTrackParticleReplay* ParticleReplayTrack = Cast< UInterpTrackParticleReplay >( CurTrack );
						if( ParticleReplayTrack != NULL )
						{
							// Copy time step
							ParticleReplayTrack->FixedTimeStep = SnapAmount;
						}
					}
				}
			}
		}
	}
}



/** Refresh the Matinee position marker and viewport state */
void FMatinee::RefreshInterpPosition()
{
	SetInterpPosition( MatineeActor->InterpPosition );
}




/** 
 *	Get the actor that the camera should currently be viewed through.
 *	We look here to see if the viewed group has a Director Track, and if so, return that Group.
 */
AActor* FMatinee::GetViewedActor()
{
	if( CamViewGroup != NULL )
	{
		UInterpGroupDirector* DirGroup = Cast<UInterpGroupDirector>(CamViewGroup);
		if(DirGroup)
		{
			return MatineeActor->FindViewedActor();
		}
		else
		{
			UInterpGroupInst* GroupInst = MatineeActor->FindFirstGroupInst(CamViewGroup);
			if( GroupInst != NULL )
			{
				return GroupInst->GetGroupActor();
			}
		}
	}

	return NULL;
}

/** Can input NULL to unlock camera from all group. */
void FMatinee::LockCamToGroup(class UInterpGroup* InGroup, const bool bResetViewports)
{
	// If different from current locked group - release current.
	if(CamViewGroup && (CamViewGroup != InGroup))
	{
		// Reset viewports (clear roll etc).  But not when recording
		if (bResetViewports)
		{
			for(int32 i=0; i<GEditor->LevelViewportClients.Num(); i++)
			{
				FLevelEditorViewportClient* LevelVC =GEditor->LevelViewportClients[i];
				if(LevelVC && LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview() )
				{
					LevelVC->RemoveCameraRoll();
					LevelVC->ViewFOV = LevelVC->FOVAngle;
					LevelVC->bEnableFading = false;
					LevelVC->bEnableColorScaling = false;
					LevelVC->SetMatineeActorLock(nullptr);
				}
			}
		}

		CamViewGroup = NULL;
	}

	// If non-null new group - switch to it now.
	if(InGroup)
	{
		CamViewGroup = InGroup;

		// Move camera to track now.
		UpdateCameraToGroup(true);
	}
}

/** Update the colours of any CameraActors we are manipulating to match their group colours, and indicate which is 'active'. */
void FMatinee::UpdateCamColours()
{
	AActor* ViewedActor = MatineeActor->FindViewedActor();

	for(int32 i=0; i<MatineeActor->GroupInst.Num(); i++)
	{
		UInterpGroupInst* Inst = MatineeActor->GroupInst[i];
		if(ACameraActor* Cam = Cast<ACameraActor>(Inst->GetGroupActor()))
		{
			const FColor OverrideColor = (Inst->GetGroupActor() == ViewedActor) ? ActiveCamColor : Inst->Group->GroupColor;
			Cam->GetCameraComponent()->OverrideFrustumColor(OverrideColor);
		}
	}
}

/** 
 *	If we are viewing through a particular group - move the camera to correspond. 
 */
void FMatinee::UpdateCameraToGroup(const bool bInUpdateStandardViewports, bool bUpdateViewportTransform )
{
	bool bEnableColorScaling = false;
	FVector ColorScale(1.f,1.f,1.f);

	// If viewing through the director group, see if we have a fade track, and if so see how much fading we should do.
	float FadeAmount = 0.f;
	if(CamViewGroup)
	{
		UInterpGroupDirector* DirGroup = Cast<UInterpGroupDirector>(CamViewGroup);
		if(DirGroup)
		{
			UInterpTrackFade* FadeTrack = DirGroup->GetFadeTrack();
			if(FadeTrack && !FadeTrack->IsDisabled())
			{
				FadeAmount = FadeTrack->GetFadeAmountAtTime(MatineeActor->InterpPosition);
			}

			// Set TimeDilation in the LevelInfo based on what the Slomo track says (if there is one).
			UInterpTrackSlomo* SlomoTrack = DirGroup->GetSlomoTrack();
			if(SlomoTrack && !SlomoTrack->IsDisabled())
			{
				MatineeActor->GetWorld()->GetWorldSettings()->MatineeTimeDilation = SlomoTrack->GetSlomoFactorAtTime(MatineeActor->InterpPosition);
			}

			UInterpTrackColorScale* ColorTrack = DirGroup->GetColorScaleTrack();
			if(ColorTrack && !ColorTrack->IsDisabled())
			{
				bEnableColorScaling = true;
				ColorScale = ColorTrack->GetColorScaleAtTime(MatineeActor->InterpPosition);
			}
		}
	}

	AActor* DefaultViewedActor = GetViewedActor();

	if (bInUpdateStandardViewports)
	{
		// Move any perspective viewports to coincide with moved actor.
		for(int32 i=0; i<GEditor->LevelViewportClients.Num(); i++)
		{
			FLevelEditorViewportClient* LevelVC =GEditor->LevelViewportClients[i];
			if(LevelVC && LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview() )
			{
				UpdateLevelViewport(DefaultViewedActor, LevelVC, FadeAmount, ColorScale, bEnableColorScaling, bUpdateViewportTransform );
			}
		}
	}

}

/**
 * Updates a viewport from a given actor
 * @param InActor - The actor to track the viewport to
 * @param InViewportClient - The viewport to update
 * @param InFadeAmount - Fade amount for camera
 * @param InColorScale - Color scale for render
 * @param bInEnableColorScaling - whether to use color scaling or not
 */
void FMatinee::UpdateLevelViewport(AActor* InActor, FLevelEditorViewportClient* InViewportClient, const float InFadeAmount, const FVector& InColorScale, const bool bInEnableColorScaling, bool bUpdateViewportTransform )
{
	//if we're recording matinee and this is the proper recording window.  Do NOT update the viewport (it's being controlled by input)
	if ((RecordingState!=MatineeConstants::ERecordingState::RECORDING_COMPLETE) && InViewportClient->IsMatineeRecordingWindow())
	{
		//if this actor happens to be a camera, let's copy the appropriate viewport settings back to the camera
		ACameraActor* CameraActor = Cast<ACameraActor>(InActor);
		if (CameraActor)
		{
			CameraActor->GetCameraComponent()->FieldOfView = InViewportClient->ViewFOV;
			CameraActor->GetCameraComponent()->AspectRatio = InViewportClient->AspectRatio;
			CameraActor->SetActorLocation(InViewportClient->GetViewLocation(), false);
			CameraActor->SetActorRotation(InViewportClient->GetViewRotation() );
		}
		return;
	}

	ACameraActor* Cam = Cast<ACameraActor>(InActor);
	if (InActor)
	{
		if( bUpdateViewportTransform )
		{
			InViewportClient->SetViewLocation( InActor->GetActorLocation() );
			InViewportClient->SetViewRotation( InActor->GetActorRotation() );
		}				

		InViewportClient->FadeAmount = InFadeAmount;
		InViewportClient->bEnableFading = true;

		InViewportClient->bEnableColorScaling = bInEnableColorScaling;
		InViewportClient->ColorScale = InColorScale;

		if (PreviousCamera != Cam)
		{
			PreviousCamera = Cam;
			InViewportClient->SetIsCameraCut();
		}
	}
	else
	{
		InViewportClient->ViewFOV = InViewportClient->FOVAngle;

		InViewportClient->FadeAmount = InFadeAmount;
		InViewportClient->bEnableFading = true;
	}

	// Set the actor lock.
	InViewportClient->SetMatineeActorLock(InActor);

	// If viewing through a camera - enforce aspect ratio.
	if(Cam)
	{
		// If the Camera's aspect ratio is zero, put a more reasonable default here - this at least stops it from crashing
		// nb. the AspectRatio will be reported as a Map Check Warning
		if( Cam->GetCameraComponent()->AspectRatio == 0 )
		{
			InViewportClient->AspectRatio = 1.7f;
		}
		else
		{
			InViewportClient->AspectRatio = Cam->GetCameraComponent()->AspectRatio;
		}

		//if this isn't the recording viewport OR (it is the recording viewport and it's playing or we're scrubbing the timeline
		if ((InViewportClient != GetRecordingViewport()) || (MatineeActor && ( MatineeActor->bIsPlaying || (TrackWindow.IsValid() && TrackWindow->InterpEdVC->bGrabbingHandle ) ) ))
		{
			//don't stop the camera from zooming when not playing back
			InViewportClient->ViewFOV = Cam->GetCameraComponent()->FieldOfView;

			// If there are selected actors, invalidate the viewports hit proxies, otherwise they won't be selectable afterwards
			if ( InViewportClient->Viewport && GEditor->GetSelectedActorCount() > 0 )
			{
				InViewportClient->Viewport->InvalidateHitProxy();
			}
		}
	}

	// Update ControllingActorViewInfo, so it is in sync with the updated viewport
	bUpdatingCameraGuard = true;
	InViewportClient->UpdateViewForLockedActor();
	bUpdatingCameraGuard = false;
}

/** Restores a viewports' settings that were overridden by UpdateLevelViewport, where necessary. */
void FMatinee::SaveLevelViewports()
{
	for( int32 ViewIndex = 0; ViewIndex < GEditor->LevelViewportClients.Num(); ++ViewIndex )
	{
		FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[ ViewIndex ];
		if( LevelVC && LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview() )
		{
			FMatineeViewSaveData SaveData;
			SaveData.ViewIndex = ViewIndex;
			SaveData.ViewLocation = LevelVC->GetViewLocation();
			SaveData.ViewRotation = LevelVC->GetViewRotation();

			SavedViewportData.Add( SaveData );
		}
	}
}

/** Restores viewports' settings that were overridden by UpdateLevelViewport, where necessary. */
void FMatinee::RestoreLevelViewports()
{
	for( int32 SaveIndex = 0; SaveIndex < SavedViewportData.Num(); ++SaveIndex )
	{
		const FMatineeViewSaveData& SavedData = SavedViewportData[ SaveIndex ];
		if( SavedData.ViewIndex < GEditor->LevelViewportClients.Num() )
		{
			FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[ SavedData.ViewIndex ];
			if ( LevelVC && LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview() )
			{
				LevelVC->SetMatineeActorLock( nullptr );
				LevelVC->SetViewRotation( SavedData.ViewRotation );
				LevelVC->SetViewLocation( SavedData.ViewLocation );				
			}
		}
	}

	// Redraw
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

// Notification from the EdMode that a perspective camera has moves. 
// If we are locking the camera to a particular actor - we update its location to match.
void FMatinee::CamMoved(const FVector& NewCamLocation, const FRotator& NewCamRotation)
{
	// Don't update if we were in the middle of synchronizing the camera location.
	if ( bUpdatingCameraGuard )
	{
		return;
	}

	// If cam not locked to something, do nothing.
	AActor* ViewedActor = GetViewedActor();
	if(ViewedActor)
	{
		// Update actors location/rotation from camera
		ViewedActor->SetActorLocation(NewCamLocation, false);
		ViewedActor->SetActorRotation(NewCamRotation);

		// The camera was moved already we dont need to set it again
		bool bUpdateViewportTransform = false;

		// In case we were modifying a keyframe for this actor.
		ActorModified( bUpdateViewportTransform );
	}
}


void FMatinee::ActorModified( bool bUpdateViewportTransform )
{
	// We only see if we need to update a track if we have a keyframe selected.
	if(Opt->bAdjustingKeyframe || Opt->bAdjustingGroupKeyframes)
	{
		check(Opt->SelectedKeys.Num() > 0);

		// For sanitys sake, make sure all these keys are part of the same group
		FInterpEdSelKey& SelKey = Opt->SelectedKeys[0];
		for( int32 iSelectedKey = 1; iSelectedKey < Opt->SelectedKeys.Num(); iSelectedKey++ )
		{
			FInterpEdSelKey& rSelKey = Opt->SelectedKeys[iSelectedKey];
			if ( rSelKey.Group != SelKey.Group)
			{
				return;
			}
		}

		// Find the actor controlled by the selected group.
		UInterpGroupInst* GrInst = MatineeActor->FindFirstGroupInst(SelKey.Group);
		if( GrInst == NULL || GrInst->GetGroupActor() == NULL )
		{
			return;
		}
  
		// See if this is one of the actors that was just moved.
		bool bTrackActorModified = false;

		TArray<UObject*> NewObjects;
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if ( Actor == GrInst->GetGroupActor() )
			{
				bTrackActorModified = true;
				break;
			}
		}
		
	// If so, update the selected keyframe on the selected track to reflect its new position.
		if(bTrackActorModified)
		{
			InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "UpdateKeyframe", "Update Key Frame") );

			for( int32 iSelectedKey = 0; iSelectedKey < Opt->SelectedKeys.Num(); iSelectedKey++ )
			{
				FInterpEdSelKey& rSelKey = Opt->SelectedKeys[iSelectedKey];
				rSelKey.Track->Modify();

				UInterpTrack* Parent = Cast<UInterpTrack>( rSelKey.Track->GetOuter() );
				if( Parent )
				{
					// This track is a subtrack of some other track.  
					// Get the parent track index and let the parent update the childs keyframe 
					UInterpTrackInst* TrInst = GrInst->TrackInst[ rSelKey.Group->InterpTracks.Find( Parent ) ];
					Parent->UpdateChildKeyframe( rSelKey.Track, rSelKey.KeyIndex, TrInst );
				}
				else
				{
					// This track is a normal track parented to a group
					UInterpTrackInst* TrInst = GrInst->TrackInst[ rSelKey.Group->InterpTracks.Find( rSelKey.Track ) ];
					rSelKey.Track->UpdateKeyframe( rSelKey.KeyIndex, TrInst );
				}
			}

			InterpEdTrans->EndSpecial();
		}
	}

	// This might have been a camera propety - update cameras.
	UpdateCameraToGroup( true, bUpdateViewportTransform );
}

void FMatinee::ActorSelectionChange( const bool bClearSelectionIfInvalid /*= true*/ )
{
	// Ignore this selection notification if desired.
	if(AMatineeActor::IgnoreActorSelection())
	{
		return;
	}

	// When an actor selection changed and the interp groups associated to the selected actors does NOT match 
	// the selected interp groups (or tracks if only interp tracks are selected), that means the user selected 
	// an actor in the level editing viewport and we need to synchronize the selection in Matinee. 

	TArray<UInterpGroup*> ActorGroups;

	// First, gather all the interp groups associated to the selected actors 
	// so that we can compare them to selected groups or tracks in Matinee. 
	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		UInterpGroupInst* GroupInstance = MatineeActor->FindGroupInst(Actor);

		if(GroupInstance)
		{
			check(GroupInstance->Group);
			ActorGroups.AddUnique(GroupInstance->Group);
		}
	}

	if( ActorGroups.Num() > 0 )
	{
		// There are actors referenced in the opened Matinee. Now, figure 
		// out if selected actors matches the selection in Matinee.

		bool bSelectionIsOutOfSync = false;

		// If all selected groups or tracks match up to the selected actors, then the user 
		// selected a group or track in Matinee. In this case, we are in sync with Matinee. 

		if( HasATrackSelected() )
		{
			for( TArray<UInterpGroup*>::TIterator GroupIter(ActorGroups); GroupIter; ++GroupIter )
			{
				if( !HasATrackSelected(*GroupIter) )
				{
					// NOTE: Since one selected actor did not have a selected track, we will clear the track selection in favor 
					// of selecting the groups instead. We do this because we don't know if the actor without the selected
					// track even has an interp track. It is guaranteed, however, to have an interp group associated to it. 
					bSelectionIsOutOfSync = true;
					break;
				}
			}
		}
		else
		{
			for( TArray<UInterpGroup*>::TIterator GroupIter(ActorGroups); GroupIter; ++GroupIter )
			{
				if( !IsGroupSelected(*GroupIter) )
				{
					bSelectionIsOutOfSync = true;
					break;
				}
			}
		}

		// The selected actors don't match up to the selection state in Matinee!
		if( bSelectionIsOutOfSync )
		{
			// Clear out all selections because the user might have deselected something, which means 
			// it wouldn't be in the array of interp groups that was gathered in this function.
			DeselectAll(false);

			for( TArray<UInterpGroup*>::TIterator GroupIter(ActorGroups); GroupIter; ++GroupIter )
			{
				// We're updating the selection to match the selected actors; don't select the actors in this group
				SelectGroup(*GroupIter, false, false);

				ScrollToGroup( *GroupIter );
			}
		}
	}
	// If there are no interp groups associated to the selected 
	// actors, then clear out any existing Matinee selections.
	else if( bClearSelectionIfInvalid )
	{
		AMatineeActor::PushIgnoreActorSelection();
		DeselectAll();
		AMatineeActor::PopIgnoreActorSelection();
	}
}

bool FMatinee::ProcessKeyPress(FKey Key, bool bCtrlDown, bool bAltDown)
{
	return false;
}

/**
 * Zooms the curve editor and track editor in or out by the specified amount
 *
 * @param ZoomAmount			Amount to zoom in or out
 * @param bZoomToTimeCursorPos	True if we should zoom to the time cursor position, otherwise mouse cursor position
 */
void FMatinee::ZoomView( float ZoomAmount, bool bZoomToTimeCursorPos )
{
	// Proportion of interp we are currently viewing
	const float OldTimeRange = ViewEndTime - ViewStartTime;
	float CurrentZoomFactor = OldTimeRange / TrackViewSizeX;

	float NewZoomFactor = FMath::Clamp<float>(CurrentZoomFactor * ZoomAmount, 0.0003f, 1.0f);
	float NewTimeRange = NewZoomFactor * TrackViewSizeX;

	// zoom into scrub position
	if(bZoomToScrubPos)
	{
		float ViewMidTime = MatineeActor->InterpPosition;
		ViewStartTime = ViewMidTime - 0.5*NewTimeRange;
		ViewEndTime = ViewMidTime + 0.5*NewTimeRange;
	}
	else
	{
		bool bZoomedToCursorPos = false;

		if (TrackWindow.IsValid() && TrackWindow->IsHovered())
		{
			// Figure out where the mouse cursor is over the Matinee track editor timeline
			FIntPoint ClientMousePos = TrackWindow->GetMousePos();
			int32 ViewportClientAreaX = ClientMousePos.X;
			int32 MouseXOverTimeline = ClientMousePos.X - LabelWidth;

			if( MouseXOverTimeline >= 0 && MouseXOverTimeline < TrackViewSizeX )
			{
				// zoom into the mouse cursor's position over the view
				const float CursorPosInTime = ViewStartTime + ( MouseXOverTimeline / PixelsPerSec );
				const float CursorPosScalar = ( CursorPosInTime - ViewStartTime ) / OldTimeRange;

				ViewStartTime = CursorPosInTime - CursorPosScalar * NewTimeRange;
				ViewEndTime = CursorPosInTime + ( 1.0f - CursorPosScalar ) * NewTimeRange;

				bZoomedToCursorPos = true;
			}
		}
		
		// We'll only zoom to the middle if we weren't already able to zoom to the cursor position.  Useful
		// if the mouse is outside of the window but the window still has focus for the zoom event
		if( !bZoomedToCursorPos )
		{
			// zoom into middle of view
			float ViewMidTime = ViewStartTime + 0.5f*(ViewEndTime - ViewStartTime);
			ViewStartTime = ViewMidTime - 0.5*NewTimeRange;
			ViewEndTime = ViewMidTime + 0.5*NewTimeRange;
		}
	}

	SyncCurveEdView();
}

struct TopLevelGroupInfo
{
	/** Index in original list */
	int32 GroupIndex;

	/** Number of children */
	int32 ChildCount;
};

void FMatinee::MoveActiveBy(int32 MoveBy)
{
	const bool bOnlyOneGroupSelected = (GetSelectedGroupCount() == 1);
	const bool bOnlyOneTrackSelected = (GetSelectedTrackCount() == 1);

	// Only one group or one track can be selected for this operation
	if( !(bOnlyOneGroupSelected ^ bOnlyOneTrackSelected) )
	{
		return;
	}

	// We only support moving 1 unit in either direction
	check( FMath::Abs( MoveBy ) == 1 );
	
	InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "InterpEd_Move_SelectedTrackOrGroup", "Move Selected Track or Group") );

	// If no track selected, move group
	if( bOnlyOneGroupSelected )
	{
		UInterpGroup* SelectedGroup = *(GetSelectedGroupIterator());
		int32 SelectedGroupIndex = IData->InterpGroups.Find(SelectedGroup);

		// Is this a root group or a child group?  We'll only allow navigation through groups within the current scope.
		const bool bIsChildGroup = SelectedGroup->bIsParented;

		// If we're moving a child group, then don't allow it to move outside of it's current folder's sub-group list
		if( bIsChildGroup )
		{
			int32 TargetGroupIndex = SelectedGroupIndex + MoveBy;

			if( TargetGroupIndex >= 0 && TargetGroupIndex < IData->InterpGroups.Num() )
			{
				UInterpGroup* GroupToCheck = IData->InterpGroups[ TargetGroupIndex ];
				if( !GroupToCheck->bIsParented )
				{
					// Uh oh, we've reached the end of our parent group's list.  We'll deny movement.
					TargetGroupIndex = SelectedGroupIndex;
				}
			}

			if(TargetGroupIndex != SelectedGroupIndex && TargetGroupIndex >= 0 && TargetGroupIndex < IData->InterpGroups.Num())
			{
				IData->Modify();

				UInterpGroup* TempGroup = IData->InterpGroups[TargetGroupIndex];
				IData->InterpGroups[TargetGroupIndex] = IData->InterpGroups[SelectedGroupIndex];
				IData->InterpGroups[SelectedGroupIndex] = TempGroup;
			}
		}
		else
		{
			// We're moving a root group.  This is a bit tricky.  Our (single level) 'hierarchy' of groups is really just
			// a flat list of elements with a bool that indicates whether the element is a child of the previous non-child
			// element, so we need to be careful to skip over all child groups when reordering things.

			// Also, we'll also skip over the director group if we find one, since those will always appear immutable to the
			// user in the GUI.  The director group draws at the top of the group list and never appears underneath
			// another group or track, so we don't want to consider it when rearranging groups through the UI.

			// Digest information about the group list
			TArray< TopLevelGroupInfo > TopLevelGroups;
			int32 SelectedGroupTLIndex = INDEX_NONE;
			{
				int32 LastParentListIndex = INDEX_NONE;
				for( int32 CurGroupIndex = 0; CurGroupIndex < IData->InterpGroups.Num(); ++CurGroupIndex )
				{
					UInterpGroup* CurGroup = IData->InterpGroups[ CurGroupIndex ];

					if( CurGroup->bIsParented )
					{
						// Add a new child to the last top level group
						check( LastParentListIndex != INDEX_NONE );
						++TopLevelGroups[ LastParentListIndex ].ChildCount;
					}
					else
					{
						// A new top level group!
						TopLevelGroupInfo NewTopLevelGroup;
						NewTopLevelGroup.GroupIndex = CurGroupIndex;

						// Start at zero; we'll count these as we go along
						NewTopLevelGroup.ChildCount = 0;

						LastParentListIndex = TopLevelGroups.Add( NewTopLevelGroup );

						// If this is the active group, then keep track of that
						if( CurGroup == SelectedGroup )
						{
							SelectedGroupTLIndex = LastParentListIndex;
						}
					}
				}
			}

			// Make sure we found ourselves in the list
			check( SelectedGroupTLIndex != INDEX_NONE );



			// Determine our top-level list target
			int32 TargetTLIndex = SelectedGroupTLIndex + MoveBy;
			if( TargetTLIndex >= 0 && TargetTLIndex < TopLevelGroups.Num() )
			{
				// Skip over director groups if we need to
				if( IData->InterpGroups[ TopLevelGroups[ TargetTLIndex ].GroupIndex ]->IsA( UInterpGroupDirector::StaticClass() ) )
				{
					TargetTLIndex += MoveBy;
				}
			}

			// Make sure we're still in range
			if( TargetTLIndex >= 0 && TargetTLIndex < TopLevelGroups.Num() )
			{
				// Compute the list index that we'll be 'inserting before'
				int32 InsertBeforeTLIndex = TargetTLIndex;
				if( MoveBy > 0 )
				{
					++InsertBeforeTLIndex;
				}

				// Compute our list destination
				int32 TargetGroupIndex;
				if( InsertBeforeTLIndex < TopLevelGroups.Num() )
				{
					// Grab the top-level target group
					UInterpGroup* TLTargetGroup = IData->InterpGroups[ TopLevelGroups[ InsertBeforeTLIndex ].GroupIndex ];

					// Setup 'insert' target group index
					TargetGroupIndex = TopLevelGroups[ InsertBeforeTLIndex ].GroupIndex;
				}
				else
				{
					// We need to be at the very end of the list!
					TargetGroupIndex = IData->InterpGroups.Num();
				}


				// OK, time to move!
				const int32 NumChildGroups = CountGroupFolderChildren( SelectedGroup );
				const int32 NumGroupsToMove = NumChildGroups + 1;


				// We're about to modify stuff 
				IData->Modify();


				// Remove source groups from master list
				TArray< UInterpGroup* > GroupsToMove;
				for( int32 GroupToMoveIndex = 0; GroupToMoveIndex < NumGroupsToMove; ++GroupToMoveIndex )
				{
					GroupsToMove.Add( IData->InterpGroups[ SelectedGroupIndex ] );
					IData->InterpGroups.RemoveAt( SelectedGroupIndex );

					// Adjust our target index for removed groups
					if( TargetGroupIndex >= SelectedGroupIndex )
					{
						--TargetGroupIndex;
					}
				};


				// Reinsert source groups at destination index
				for( int32 GroupToMoveIndex = 0; GroupToMoveIndex < NumGroupsToMove; ++GroupToMoveIndex )
				{
					int32 DestGroupIndex = TargetGroupIndex + GroupToMoveIndex;
					IData->InterpGroups.Insert( GroupsToMove[ GroupToMoveIndex ], DestGroupIndex );
				};

				// Make sure the curve editor is in sync
				CurveEd->CurveChanged();
			}
			else
			{
				// Out of range, we can't move any further
			}
		}
	}
	// If a track is selected, move it instead.
	else
	{
		FSelectedTrackIterator TrackIt(GetSelectedTrackIterator());
		UInterpGroup* Group = TrackIt.GetGroup();
		int32 TrackIndex = TrackIt.GetTrackIndex();

		// Move the track itself.
		int32 TargetTrackIndex = TrackIndex + MoveBy;

		Group->Modify();

		if(TargetTrackIndex >= 0 && TargetTrackIndex < Group->InterpTracks.Num())
		{
			UInterpTrack* TempTrack = Group->InterpTracks[TargetTrackIndex];
			Group->InterpTracks[TargetTrackIndex] = Group->InterpTracks[TrackIndex];
			Group->InterpTracks[TrackIndex] = TempTrack;

			// Now move any track instances inside their group instance.
			for(int32 i=0; i<MatineeActor->GroupInst.Num(); i++)
			{
				UInterpGroupInst* GrInst = MatineeActor->GroupInst[i];
				if(GrInst->Group == Group)
				{
					check(GrInst->TrackInst.Num() == Group->InterpTracks.Num());

					GrInst->Modify();

					UInterpTrackInst* TempTrInst = GrInst->TrackInst[TargetTrackIndex];
					GrInst->TrackInst[TargetTrackIndex] = GrInst->TrackInst[TrackIndex];
					GrInst->TrackInst[TrackIndex] = TempTrInst;
				}
			}

			// Update selection to keep same track selected.
			TrackIt.MoveIteratorBy(MoveBy);

			// Selection stores keys by track index - safest to invalidate here.
			ClearKeySelection();
		}
	}

	InterpEdTrans->EndSpecial();

	UInterpGroup* Group = NULL;
	int32 LabelTop = 0;
	int32 LabelBottom = 0;

	if( HasATrackSelected() )
	{
		FSelectedTrackIterator TrackIter = GetSelectedTrackIterator();
		Group = TrackIter.GetGroup();
		GetTrackLabelPositions( Group, TrackIter.GetTrackIndex(), LabelTop, LabelBottom );
	}
	else
	{
		FSelectedGroupIterator GroupIter = GetSelectedGroupIterator();
		Group = *GroupIter;
		GetGroupLabelPosition( Group, LabelTop, LabelBottom );

	}

	// Attempt to autoscroll when the user moves a track or group label out of view. 
	if( Group != NULL )
	{
		// Figure out which window we are panning
		TSharedPtr<SMatineeViewport> CurrentWindow = Group->IsA(UInterpGroupDirector::StaticClass()) ? DirectorTrackWindow : TrackWindow;
		if (CurrentWindow.IsValid() && CurrentWindow->InterpEdVC.IsValid())
		{
			const int32 ThumbTop = CurrentWindow->GetThumbPosition();
			const uint32 ViewportHeight = CurrentWindow->InterpEdVC->Viewport->GetSizeXY().Y;
			const uint32 ContentHeight = CurrentWindow->InterpEdVC->ComputeGroupListContentHeight();
			const uint32 ContentBoxHeight =	CurrentWindow->InterpEdVC->ComputeGroupListBoxHeight( ViewportHeight );
			const int32 ThumbBottom = ThumbTop + ContentBoxHeight;
		

			// Start the scrollbar at the current location. If the 
			// selected track title is visible, nothing will be scrolled. 
			int32 NewScrollPosition = ThumbTop;

			// If the user moved the track title up and it's not viewable anymore,
			// move the scrollbar up so that the selected track is visible. 
			if( MoveBy < 0 && (LabelTop - ThumbTop) < 0 )
			{
				NewScrollPosition += (LabelTop - ThumbTop);
			}
			// If the user moved the track title down and it's not viewable anymore,
			// move the scrollbar down so that the selected track is visible. 
			else if( MoveBy > 0 && ThumbBottom < LabelBottom )
			{
				NewScrollPosition += (LabelBottom - ThumbBottom);
			}

			CurrentWindow->SetThumbPosition(NewScrollPosition);
			CurrentWindow->AdjustScrollBar();
		}
	}

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();
}

void FMatinee::MoveActiveUp()
{
	MoveActiveBy(-1);
}

void FMatinee::MoveActiveDown()
{
	MoveActiveBy(+1);
}

void FMatinee::InterpEdUndo()
{
	GEditor->Trans->Undo();
	
	if (IData != NULL)
	{
		CurveEd->SetRegionMarker(true, IData->EdSectionStart, IData->EdSectionEnd, RegionFillColor);
		CurveEd->SetEndMarker(true, IData->InterpLength);
	}

	Opt->bAdjustingKeyframe = false;
	Opt->bAdjustingGroupKeyframes = false;

	// A new group may have been added (via duplication), so we'll need to update our scroll bar
	UpdateTrackWindowScrollBars();

	// Make sure that the viewports get updated after the Undo operation
	InvalidateTrackWindowViewports();

	if(IData != NULL)
	{
		IData->UpdateEventNames();
	}

	if(MatineeActor != NULL)
	{
		MatineeActor->EnsureActorGroupConsistency();
		MatineeActor->RecaptureActorState();
	}
}

void FMatinee::InterpEdRedo()
{
	GEditor->Trans->Redo();

	if (IData != NULL)
	{
		CurveEd->SetRegionMarker(true, IData->EdSectionStart, IData->EdSectionEnd, RegionFillColor);
		CurveEd->SetEndMarker(true, IData->InterpLength);
	}

	Opt->bAdjustingKeyframe = false;
	Opt->bAdjustingGroupKeyframes = false;

	// A new group may have been added (via duplication), so we'll need to update our scroll bar
	UpdateTrackWindowScrollBars();

	// Make sure that the viewports get updated after the Undo operation
	InvalidateTrackWindowViewports();

	if(IData != NULL)
	{
		IData->UpdateEventNames();
	}

	if(MatineeActor != NULL)
	{
		MatineeActor->EnsureActorGroupConsistency();
		MatineeActor->RecaptureActorState();
	}
}

