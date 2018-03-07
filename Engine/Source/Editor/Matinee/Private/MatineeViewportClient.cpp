// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MatineeViewportClient.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "MatineeHitProxy.h"
#include "MatineeOptions.h"
#include "MatineeTransBuffer.h"
#include "Editor.h"
#include "Engine/InterpCurveEdSetup.h"
#include "Matinee.h"

#include "InterpolationHitProxy.h"
#include "Matinee/MatineeActor.h"
#include "Matinee/InterpTrackMove.h"
#include "Matinee/InterpTrackEvent.h"

/*-----------------------------------------------------------------------------
 FMatineeViewportClient
-----------------------------------------------------------------------------*/

FMatineeViewportClient::FMatineeViewportClient( class FMatinee* InMatinee )
	: FEditorViewportClient(nullptr)
{
	InterpEd = InMatinee;

	// This window will be 2D/canvas only, so set the viewport type to None
	ViewportType = LVT_None;

	// Set defaults for members.  These should be initialized by the owner after construction.
	bIsDirectorTrackWindow = false;
	bWantTimeline = false;

	// Scroll bar starts at the top of the list!
	ThumbPos_Vert = 0;

	OldMouseX = 0;
	OldMouseY = 0;

	DistanceDragged = 0;

	BoxStartX = 0;
	BoxStartY = 0;
	BoxEndX = 0;
	BoxEndY = 0;

	bPanning = false;
	bMouseDown = false;
	bGrabbingHandle = false;
	bBoxSelecting = false;
	bTransactionBegun = false;
	bNavigating = false;
	bGrabbingMarker	= false;

	DragObject = NULL;

	SetRealtime( false );

	// Cache the font to use for drawing labels.
	LabelFont = GEditor->EditorFont;
	
	CamLockedIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_View_On.MAT_Groups_View_On"), NULL, LOAD_None, NULL ));	
	CamUnlockedIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_View_Off.MAT_Groups_View_Off"), NULL, LOAD_None, NULL ));	
	ForwardEventOnTex = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Right_On.MAT_Groups_Right_On"), NULL, LOAD_None, NULL ));
	ForwardEventOffTex = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Right_Off.MAT_Groups_Right_Off"), NULL, LOAD_None, NULL ));
	BackwardEventOnTex = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Left_On.MAT_Groups_Left_On"), NULL, LOAD_None, NULL ));
	BackwardEventOffTex = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Left_Off.MAT_Groups_Left_Off"), NULL, LOAD_None, NULL ));
	DisableTrackTex = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/Cascade/CASC_ModuleEnable.CASC_ModuleEnable"), NULL, LOAD_None, NULL ));
	GraphOnTex = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Graph_On.MAT_Groups_Graph_On"), NULL, LOAD_None, NULL ));
	GraphOffTex = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Graph_Off.MAT_Groups_Graph_Off"), NULL, LOAD_None, NULL ));
	TrajectoryOnTex = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Graph_On.MAT_Groups_Graph_On"), NULL, LOAD_None, NULL ));
	
}

FMatineeViewportClient::~FMatineeViewportClient()
{

}

void FMatineeViewportClient::SetParentTab(TWeakPtr<SDockTab> InParentTab )
{
	ParentTab = InParentTab;
}

void FMatineeViewportClient::AddKeysFromHitProxy( HHitProxy* HitProxy, TArray<FInterpEdSelKey>& Selections )
{
	// Find how much (in time) 1.5 pixels represents on the screen.
	const float PixelTime = 1.5f/InterpEd->PixelsPerSec;

	if( !HitProxy )
	{
		return;
	}
	if( HitProxy->IsA( HInterpTrackSubGroupKeypointProxy::StaticGetType() ) )
	{
		HInterpTrackSubGroupKeypointProxy* KeyProxy = ( ( HInterpTrackSubGroupKeypointProxy* )HitProxy );
		float KeyTime = KeyProxy->KeyTime;
		UInterpTrack* Track = KeyProxy->Track;
		UInterpGroup* Group = Track->GetOwningGroup();
		int32 GroupIndex = KeyProxy->GroupIndex;

		// add all keyframes at the specified time
		if( Track->SubTracks.Num() > 0 )
		{
			if( GroupIndex == INDEX_NONE )
			{
				// The keyframe was drawn on the parent track, add all keyframes in all groups at the specified time
				for( int32 SubGroupIndex = 0; SubGroupIndex < Track->SubTrackGroups.Num(); ++SubGroupIndex )
				{
					FSubTrackGroup& SubTrackGroup = Track->SubTrackGroups[ SubGroupIndex ];
					for( int32 TrackIndex = 0; TrackIndex < SubTrackGroup.TrackIndices.Num(); ++TrackIndex )
					{
						UInterpTrack* SubTrack = Track->SubTracks[ SubTrackGroup.TrackIndices[ TrackIndex ] ];

						// Get the keyframe index from the specified time.  We cant directly store the index as each subtrack may have a keyframe with the same time at a different index
						int32 KeyIndex = SubTrack->GetKeyframeIndex( KeyTime );
						if( KeyIndex != INDEX_NONE )
						{
							Selections.AddUnique( FInterpEdSelKey(Group, SubTrack , KeyIndex) );
						}
					}
				}
			}
			else
			{
				// The keyframe was drawn on a sub track group, select all keyframes in that group's tracks at the specified time.
				FSubTrackGroup& SubTrackGroup = Track->SubTrackGroups[ GroupIndex ];
				for( int32 TrackIndex = 0; TrackIndex < SubTrackGroup.TrackIndices.Num(); ++TrackIndex )
				{
					UInterpTrack* SubTrack = Track->SubTracks[ SubTrackGroup.TrackIndices[ TrackIndex ] ];
					// Get the keyframe index from the specified time.  We cant directly store the index as each subtrack may have a keyframe with the same time at a different index
					int32 KeyIndex = SubTrack->GetKeyframeIndex( KeyTime );
					if( KeyIndex != INDEX_NONE )
					{
						Selections.AddUnique( FInterpEdSelKey(Group, SubTrack , KeyIndex) );
					}
				}
			}

		}
	}
	else if( HitProxy->IsA( HInterpTrackKeypointProxy::StaticGetType() ) )
	{
		HInterpTrackKeypointProxy* KeyProxy = ( ( HInterpTrackKeypointProxy* )HitProxy );
		UInterpGroup* Group = KeyProxy->Group;
		UInterpTrack* Track = KeyProxy->Track;
		const int32 KeyIndex = KeyProxy->KeyIndex;

		// Because AddKeyToSelection might invalidate the display, we just remember all the keys here and process them together afterwards.
		Selections.AddUnique( FInterpEdSelKey(Group, Track , KeyIndex) );

		// Slight hack here. We select any other keys on the same track which are within 1.5 pixels of this one.
		const float SelKeyTime = Track->GetKeyframeTime(KeyIndex);

		for(int32 i=0; i<Track->GetNumKeyframes(); i++)
		{
			const float KeyTime = Track->GetKeyframeTime(i);
			if( FMath::Abs(KeyTime - SelKeyTime) < PixelTime )
			{
				Selections.AddUnique( FInterpEdSelKey(Group, Track, i) );
			}
		}
	}
}

bool FMatineeViewportClient::InputKey(FViewport* InViewport, int32 ControllerId, FKey Key, EInputEvent Event,float /*AmountDepressed*/,bool /*Gamepad*/)
{
	UpdateAndApplyCursorVisibility();

	const bool bCtrlDown = InViewport->KeyState(EKeys::LeftControl) || InViewport->KeyState(EKeys::RightControl);
	const bool bShiftDown = InViewport->KeyState(EKeys::LeftShift) || InViewport->KeyState(EKeys::RightShift);
	const bool bAltDown = InViewport->KeyState(EKeys::LeftAlt) || InViewport->KeyState(EKeys::RightAlt);
	const bool bCmdDown = InViewport->KeyState(EKeys::LeftCommand) || InViewport->KeyState(EKeys::RightCommand);
	const bool bCapsDown = InViewport->KeyState(EKeys::CapsLock);

	const int32 HitX = InViewport->GetMouseX();
	const int32 HitY = InViewport->GetMouseY();

	bool bClickedTrackViewport = false;

	if( Key == EKeys::LeftMouseButton )
	{
		switch( Event )
		{
		case IE_Pressed:
			{
				if(DragObject == NULL)
				{
					HHitProxy*	HitResult = InViewport->GetHitProxy(HitX,HitY);

					if(HitResult)
					{
						if(HitResult->IsA(HMatineeGroupTitle::StaticGetType()))
						{
							UInterpGroup* Group = ((HMatineeGroupTitle*)HitResult)->Group;

							if( bCtrlDown && !InterpEd->HasATrackSelected() )
							{ 
								if( InterpEd->IsGroupSelected(Group) )
								{
									// Deselect a selected group when ctrl + clicking it.
									InterpEd->DeselectGroup(Group);
								}
								else
								{
									// Otherwise, just select the group, but don't deselect other selected tracks.
									InterpEd->SelectGroup(Group, false, true);
								}
							}
							else
							{
								// Since ctrl is not down, select this group only. 
								InterpEd->SelectGroup(Group, true, true);
							}
						}
						else if(HitResult->IsA(HMatineeGroupCollapseBtn::StaticGetType()))
						{
							UInterpGroup* Group = ((HMatineeGroupCollapseBtn*)HitResult)->Group;

							// Deselect existing selected groups only if ctrl is not down. 
							InterpEd->SelectGroup(Group, !bCtrlDown, true);
							Group->bCollapsed = !Group->bCollapsed;

							// A group has been expanded or collapsed, so we need to update our scroll bar
							InterpEd->UpdateTrackWindowScrollBars();
						}
						else if(HitResult->IsA(HMatineeTrackCollapseBtn::StaticGetType()))
						{
							// A collapse widget on a track with subtracks was hit
							HMatineeTrackCollapseBtn* Proxy = ((HMatineeTrackCollapseBtn*)HitResult);
							UInterpTrack* Track = Proxy->Track;
							// GroupIndex indicates what sub group to collapse.  INDEX_NONE means collapse the whole track
							int32 GroupIndex = Proxy->SubTrackGroupIndex;
							
							if( GroupIndex != INDEX_NONE )
							{
								// A subgroup was collapsed
								FSubTrackGroup& Group = Track->SubTrackGroups[ GroupIndex ];
								Group.bIsCollapsed = !Group.bIsCollapsed;
							}
							else
							{
								// A track was collapsed
								InterpEd->SelectTrack( Track->GetOwningGroup(), Track, !bCtrlDown );
								Track->bIsCollapsed = !Track->bIsCollapsed;
							}

							// Recompute the track window scroll bar position.
							InterpEd->UpdateTrackWindowScrollBars();
						}
						else if(HitResult->IsA(HMatineeGroupLockCamBtn::StaticGetType()))
						{
							UInterpGroup* Group = ((HMatineeGroupLockCamBtn*)HitResult)->Group;

							if(Group == InterpEd->CamViewGroup)
							{
								InterpEd->LockCamToGroup(NULL);
							}
							else
							{
								InterpEd->LockCamToGroup(Group);
							}
						}
						else if(HitResult->IsA(HMatineeTrackTitle::StaticGetType()))
						{
							HMatineeTrackTitle* HitProxy = (HMatineeTrackTitle*)HitResult;
							UInterpGroup* Group = HitProxy->Group;
							UInterpTrack* TrackToSelect = HitProxy->Track;
							check( TrackToSelect );

							if( bCtrlDown && !InterpEd->HasAGroupSelected() )
							{
								if( TrackToSelect->IsSelected() )
								{
									// Deselect a selected track when ctrl + clicking it. 
									InterpEd->DeselectTrack( Group, TrackToSelect );
								}
								else
								{
									// Otherwise, select the track, but don't deselect any other selected tracks.
									InterpEd->SelectTrack( Group, TrackToSelect, false);
								}
							}
							else
							{
								// Since ctrl is not down, just select this track only.
								InterpEd->SelectTrack( Group, TrackToSelect, true);
							}
							
						}
						else if( HitResult->IsA(HMatineeSubGroupTitle::StaticGetType()) )
						{
							// A track sub group was hit
							HMatineeSubGroupTitle* HitProxy = ( (HMatineeSubGroupTitle*)HitResult );
							UInterpTrack* Track = HitProxy->Track;
							UInterpGroup* TrackGroup = Track->GetOwningGroup();
							int32 SubGroupIndex = HitProxy->SubGroupIndex;

							if( bCtrlDown && !InterpEd->HasAGroupSelected() )
							{
								// Get the sub group from the hit proxy index
								FSubTrackGroup& SubGroup = Track->SubTrackGroups[ SubGroupIndex ];
								if( SubGroup.bIsSelected )
								{
									// SubGroup was already selected, unselect it and all of the tracks in the group.
									SubGroup.bIsSelected = false;
									for( int32 TrackIndex = 0; TrackIndex < SubGroup.TrackIndices.Num(); ++TrackIndex )
									{
										InterpEd->DeselectTrack( TrackGroup, Track->SubTracks[ SubGroup.TrackIndices[ TrackIndex ] ] );
									}
								}
								else
								{
									// SubGroup was not selected, select it and all of the tracks in the group.
									SubGroup.bIsSelected = true;
									for( int32 TrackIndex = 0; TrackIndex < SubGroup.TrackIndices.Num(); ++TrackIndex )
									{
										InterpEd->SelectTrack( TrackGroup, Track->SubTracks[ SubGroup.TrackIndices[ TrackIndex ] ], false );
									}
								}
							}
							else
							{
								// control is not down, we should empty our selection
								InterpEd->DeselectAllTracks();
								for( int32 GroupIndex = 0; GroupIndex < Track->SubTrackGroups.Num(); ++GroupIndex )
								{
									FSubTrackGroup& SubGroup = Track->SubTrackGroups[ GroupIndex ];
									SubGroup.bIsSelected = false;
								}

								// Now select the group and all of its tracks
								FSubTrackGroup& SubGroup = Track->SubTrackGroups[ SubGroupIndex ];
								SubGroup.bIsSelected = true;
								for( int32 TrackIndex = 0; TrackIndex < SubGroup.TrackIndices.Num(); ++TrackIndex )
								{
									InterpEd->SelectTrack( TrackGroup, Track->SubTracks[ SubGroup.TrackIndices[ TrackIndex ] ], false );
								}
							}
						}
						// Did the user select the space in the track viewport associated to a given track?
						else if( HitResult->IsA(HMatineeTrackTimeline::StaticGetType()) )
						{
							// When the user first clicks in this space, Matinee should interpret this as if the 
							// user clicked on the empty space in the track viewport that doesn't belong to any 
							// track. This enables panning and box-selecting in addition to the ability to select a 
							// track just by clicking on the space in the track viewport associated to a given track. 
							bClickedTrackViewport = true;
						}
						else if(HitResult->IsA(HMatineeTrackTrajectoryButton::StaticGetType()))
						{
							UInterpGroup* Group = ((HMatineeTrackTrajectoryButton*)HitResult)->Group;
							UInterpTrack* Track = ((HMatineeTrackTrajectoryButton*)HitResult)->Track;

							// Should always be a movement track
							UInterpTrackMove* MovementTrack = Cast<UInterpTrackMove>( Track	);
							if( MovementTrack != NULL )
							{
								// Toggle the 3D trajectory for this track
								InterpEd->InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "InterpEd_Undo_ToggleTrajectory", "Toggle 3D Trajectory for Track") );
								MovementTrack->Modify();
								MovementTrack->bHide3DTrack = !MovementTrack->bHide3DTrack;
								InterpEd->InterpEdTrans->EndSpecial();
							}
						}
						else if(HitResult->IsA(HMatineeTrackGraphPropBtn::StaticGetType()))
						{
							UInterpGroup* Group = ((HMatineeTrackGraphPropBtn*)HitResult)->Group;
							UInterpTrack* Track = ((HMatineeTrackGraphPropBtn*)HitResult)->Track;

							// create an array of tracks that we're interested in - either subtracks or just the main track
							TArray<UInterpTrack*> TracksArray;
							int32 SubTrackGroupIndex = ((HMatineeTrackGraphPropBtn*)HitResult)->SubTrackGroupIndex;
							if( SubTrackGroupIndex != -1 )
							{
								FSubTrackGroup& SubTrackGroup = Track->SubTrackGroups[ SubTrackGroupIndex ];
								for( int32 Index = 0; Index < SubTrackGroup.TrackIndices.Num(); ++Index )
								{
									int32 SubTrackIndex = SubTrackGroup.TrackIndices[ Index ];
									TracksArray.Add( Track->SubTracks[ SubTrackIndex ] );
								}
							}
							else
							{
								if( Track->SubTracks.Num() > 0 )
								{
									// If the track has subtracks, add all subtracks instead
									for( int32 SubTrackIndex = 0; SubTrackIndex < Track->SubTracks.Num(); ++SubTrackIndex )
									{
										TracksArray.Add( Track->SubTracks[ SubTrackIndex ] );
									}
								}
								else
								{
									TracksArray.Add( Track );
								}
							}

							// find out whether or not ALL of the tracks are currently shown in the curve editor
							// start by assuming that all are - then make this false if any of them aren't
							bool bAllSubtracksShown = true;
							for( int32 Index = 0; Index < TracksArray.Num(); ++Index )
							{
								if( !InterpEd->IData->CurveEdSetup->ShowingCurve( TracksArray[ Index ] ) )
								{
									bAllSubtracksShown = false;
									break;
								}
							}

							// toggle tracks ON if NONE or SOME of them are currently shown
							// toggle tracks OFF if ALL of them are currently shown
							bool bToggleTracksOn = ( !bAllSubtracksShown );

							// add tracks to the curve editor
							for( int32 Index = 0; Index < TracksArray.Num(); ++Index )
							{
								InterpEd->AddTrackToCurveEd( *Group->GroupName.ToString(), Group->GroupColor, TracksArray[ Index ], bToggleTracksOn );
							}
						}
						else if(HitResult->IsA(HMatineeEventDirBtn::StaticGetType()))
						{
							UInterpGroup* Group = ((HMatineeEventDirBtn*)HitResult)->Group;
							const int32 TrackIndex = ((HMatineeEventDirBtn*)HitResult)->TrackIndex;
							EMatineeEventDirection::Type Dir = ((HMatineeEventDirBtn*)HitResult)->Dir;

							UInterpTrackEvent* EventTrack = CastChecked<UInterpTrackEvent>( Group->InterpTracks[TrackIndex] );

							if(Dir == EMatineeEventDirection::IED_Forward)
							{
								EventTrack->bFireEventsWhenForwards = !EventTrack->bFireEventsWhenForwards;
							}
							else
							{
								EventTrack->bFireEventsWhenBackwards = !EventTrack->bFireEventsWhenBackwards;
							}
						}
						else if( ( HitResult->IsA( HInterpTrackKeypointProxy::StaticGetType() ) ) )
						{
							TArray<FInterpEdSelKey>	NewSelection;
							AddKeysFromHitProxy( HitResult, NewSelection );

							for( int32 Index = 0; Index < NewSelection.Num(); Index++ )
							{
								FInterpEdSelKey& SelKey = NewSelection[ Index ];

								UInterpGroup* Group = SelKey.Group;
								UInterpTrack* Track = SelKey.Track;
								const int32 KeyIndex = SelKey.KeyIndex;

								if(!bCtrlDown)
								{
									// NOTE: Clear previously-selected tracks because ctrl is not down. 
									InterpEd->SelectTrack( Group, Track );
									InterpEd->ClearKeySelection();
									InterpEd->AddKeyToSelection(Group, Track, KeyIndex, !bShiftDown);
								}
							}
						}
						else if( HitResult->IsA( HInterpTrackSubGroupKeypointProxy::StaticGetType() ) )
						{
							TArray<FInterpEdSelKey>	NewSelection;
							AddKeysFromHitProxy( HitResult, NewSelection );

							if( !bCtrlDown )
							{
								InterpEd->DeselectAllTracks();
								InterpEd->ClearKeySelection();
							}

							for( int32 Index = 0; Index < NewSelection.Num(); Index++ )
							{
								FInterpEdSelKey& SelKey = NewSelection[ Index ];

								UInterpGroup* Group = SelKey.Group;
								UInterpTrack* Track = SelKey.Track;
								const int32 KeyIndex = SelKey.KeyIndex;

								if( !InterpEd->KeyIsInSelection( Group, Track, KeyIndex ) )
								{
									InterpEd->SelectTrack( Group, Track, false );
									InterpEd->AddKeyToSelection( Group, Track, KeyIndex, !bShiftDown );
								}
							}

							if(InterpEd->Opt->SelectedKeys.Num() > 1)
							{
								InterpEd->Opt->bAdjustingGroupKeyframes = true;
							}
						}
						else if(HitResult->IsA(HMatineeTrackBkg::StaticGetType()))
						{
							InterpEd->DeselectAll();
						}
						else if(HitResult->IsA(HMatineeTimelineBkg::StaticGetType()))
						{
							float NewTime = InterpEd->ViewStartTime + ((HitX - InterpEd->LabelWidth) / InterpEd->PixelsPerSec);
							if( InterpEd->bSnapToFrames && InterpEd->bSnapTimeToFrames )
							{
								NewTime = InterpEd->SnapTimeToNearestFrame( NewTime );
							}

							// When jumping to location by clicking, stop playback.
							InterpEd->MatineeActor->Stop();
							SetRealtime( false );
							InterpEd->SetAudioRealtimeOverride( false );

							//make sure to turn off recording
							InterpEd->StopRecordingInterpValues();

							// Move to clicked on location
							InterpEd->SetInterpPosition(NewTime);

							// Act as if we grabbed the handle as well.
							bGrabbingHandle = true;
						}
						else if(HitResult->IsA(HMatineeNavigatorBackground::StaticGetType()))
						{
							// Clicked on the navigator background, so jump directly to the position under the
							// mouse cursor and wait for a drag
							const float JumpToTime = ((HitX - InterpEd->LabelWidth)/InterpEd->NavPixelsPerSecond);
							const float ViewWindow = (InterpEd->ViewEndTime - InterpEd->ViewStartTime);

							InterpEd->ViewStartTime = JumpToTime - (0.5f * ViewWindow);
							InterpEd->ViewEndTime = JumpToTime + (0.5f * ViewWindow);
							InterpEd->SyncCurveEdView();

							bNavigating = true;
						}
						else if(HitResult->IsA(HMatineeNavigator::StaticGetType()))
						{
							// Clicked on the navigator foreground, so just start the drag immediately without
							// jumping the timeline
							bNavigating = true;
						}
						else if(HitResult->IsA(HMatineeMarker::StaticGetType()))
						{
							InterpEd->GrabbedMarkerType = ((HMatineeMarker*)HitResult)->Type;

							InterpEd->BeginMoveMarker();
							bGrabbingMarker = true;
						}
						else if(HitResult->IsA(HMatineeTrackDisableTrackBtn::StaticGetType()))
						{
							HMatineeTrackDisableTrackBtn* TrackProxy = ((HMatineeTrackDisableTrackBtn*)HitResult);

							if(TrackProxy->Group != NULL && TrackProxy->Track != NULL )
							{
								UInterpTrack* Track = TrackProxy->Track;

								InterpEd->InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "InterpEd_Undo_ToggleTrackEnabled", "Enable/Disable Track") );

								//if only one per group is allowed to be active (And this is ABOUT to become active), ensure that no other is active
								if (Track->bOnePerGroup && (Track->IsDisabled() == true))
								{
									InterpEd->DisableTracksOfClass(TrackProxy->Group, Track->GetClass());
								}

								Track->Modify();
								Track->EnableTrack( Track->IsDisabled() ? true : false, true );

								InterpEd->InterpEdTrans->EndSpecial();

								// Update the preview and actor states
								InterpEd->MatineeActor->RecaptureActorState();
								
							}
						}
						else if(HitResult->IsA(HInterpEdInputInterface::StaticGetType()))
						{
							HInterpEdInputInterface* Proxy = ((HInterpEdInputInterface*)HitResult);

							DragObject = Proxy->ClickedObject;
							DragData = Proxy->InputData;
							DragData.PixelsPerSec = InterpEd->PixelsPerSec;
							DragData.MouseStart = FIntPoint(HitX, HitY);
							DragData.bCtrlDown = bCtrlDown;
							DragData.bAltDown = bAltDown;
							DragData.bShiftDown = bShiftDown;
							DragData.bCmdDown = bCmdDown;
							Proxy->ClickedObject->BeginDrag(DragData);
						}
					}
					else
					{
						// The user clicked on empty space that doesn't have a hit proxy associated to it.
						bClickedTrackViewport = true;
					}

					// When the user clicks on empty, non-hit proxy space, allow 
					// features such as box-selection and panning of the viewport. 
					if( bClickedTrackViewport )
					{
						// Enable box selection if CTRL + ALT held down.
						if(bCtrlDown && bAltDown)
						{
							BoxStartX = BoxEndX = HitX;
							BoxStartY = BoxEndY = HitY;

							bBoxSelecting = true;
						}
						// The last option is to simply pan the viewport
						else
						{
							bPanning = true;
						}
					}

					InViewport->LockMouseToViewport(true);

					bMouseDown = true;
					OldMouseX = HitX;
					OldMouseY = HitY;
					DistanceDragged = 0;
				}
			}
			break;
		case IE_DoubleClick:
			{
				InViewport->InvalidateHitProxy();

				HHitProxy*	HitResult = InViewport->GetHitProxy(HitX,HitY);

				if(HitResult)
				{
					if(HitResult->IsA(HMatineeGroupTitle::StaticGetType()))
					{
						UInterpGroup* Group = ((HInterpTrackKeypointProxy*)HitResult)->Group;

						Group->bCollapsed = !Group->bCollapsed;

						// A group has been expanded or collapsed, so we need to update our scroll bar
						InterpEd->UpdateTrackWindowScrollBars();
					}
				}
			}
			break;
		case IE_Released:
			{
				InViewport->InvalidateHitProxy();

				if(bBoxSelecting)
				{
					const int32 MinX = FMath::Max(0, FMath::Min(BoxStartX, BoxEndX));
					const int32 MinY = FMath::Max(0, FMath::Min(BoxStartY, BoxEndY));
					const int32 MaxX = FMath::Min(Viewport->GetSizeXY().X - 1, FMath::Max(BoxStartX, BoxEndX));
					const int32 MaxY = FMath::Min(Viewport->GetSizeXY().Y - 1, FMath::Max(BoxStartY, BoxEndY));
					const int32 TestSizeX = MaxX - MinX + 1;
					const int32 TestSizeY = MaxY - MinY + 1;

					// Find how much (in time) 1.5 pixels represents on the screen.
					const float PixelTime = 1.5f/InterpEd->PixelsPerSec;

					// We read back the hit proxy map for the required region.
					TArray<HHitProxy*> ProxyMap;
					InViewport->GetHitProxyMap(FIntRect(MinX, MinY, MaxX + 1, MaxY + 1), ProxyMap);

					TArray<FInterpEdSelKey>	NewSelection;

					// Find any keypoint hit proxies in the region - add the keypoint to selection.
					for( int32 Y = 0; Y < TestSizeY; Y++ )
					{
						for( int32 X = 0; X < TestSizeX; X++ )
						{
							AddKeysFromHitProxy( ProxyMap[ Y * TestSizeX + X ], NewSelection );
						}
					}

					// If the SHIFT key is down, then the user wants to preserve 
					// the current selection in Matinee during box selection.
					if(!bShiftDown)
					{
						// NOTE: This will clear all keys
						InterpEd->DeselectAllTracks();
					}

					for(int32 i=0; i<NewSelection.Num(); i++)
					{
						UInterpGroup* Group = NewSelection[i].Group;
						UInterpTrack* TrackToSelect = NewSelection[i].Track;
						InterpEd->SelectTrack( Group, TrackToSelect, false );
						InterpEd->AddKeyToSelection( Group, TrackToSelect, NewSelection[i].KeyIndex, false );
					}
				}
				else if(DragObject)
				{
					HHitProxy*	HitResult = InViewport->GetHitProxy(HitX,HitY);

					if(HitResult)
					{
						if(HitResult->IsA(HInterpEdInputInterface::StaticGetType()))
						{
							HInterpEdInputInterface* Proxy = ((HInterpEdInputInterface*)HitResult);
							
							//@todo: Do dropping.
						}
					}

					DragData.PixelsPerSec = InterpEd->PixelsPerSec;
					DragData.MouseCurrent = FIntPoint(HitX, HitY);
					DragObject->EndDrag(DragData);
					DragObject = NULL;
				}
				else if(DistanceDragged < 4)
				{
					HHitProxy*	HitResult = InViewport->GetHitProxy(HitX,HitY);

					// If mouse didn't really move since last time, and we released over empty space, deselect everything.
					if(!HitResult)
					{
						InterpEd->ClearKeySelection();
					}
					// Allow track selection if the user selects the space in the track viewport that is associated to a given track.
					else if(HitResult->IsA(HMatineeTrackTimeline::StaticGetType()))
					{
						UInterpGroup* Group = ((HMatineeTrackTimeline*)HitResult)->Group;
						UInterpTrack* TrackToSelect = ((HMatineeTrackTimeline*)HitResult)->Track;

						if( bCtrlDown && !InterpEd->HasAGroupSelected() )
						{
							if( TrackToSelect->IsSelected() )
							{
								// Deselect a selected track when ctrl + clicking it. 
								InterpEd->DeselectTrack( Group, TrackToSelect );
							}
							else
							{
								// Otherwise, select the track, but don't deselect any other selected tracks.
								InterpEd->SelectTrack(  Group, TrackToSelect, false);
							}
						}
						else
						{
							// Always clear the key selection when single-clicking on the track viewport background. 
							// If the user is CTRL-clicking, we don't want to clear the key selection of other tracks.
							InterpEd->ClearKeySelection();

							// Since ctrl is not down, just select this track only.
							InterpEd->SelectTrack( Group, TrackToSelect, true);
						}
					}
					else if(bCtrlDown && HitResult->IsA(HInterpTrackKeypointProxy::StaticGetType()))
					{
						HInterpTrackKeypointProxy* KeyProxy = ((HInterpTrackKeypointProxy*)HitResult);
						UInterpGroup* Group = KeyProxy->Group;
						UInterpTrack* Track = KeyProxy->Track;
						const int32 KeyIndex = KeyProxy->KeyIndex;
	
						const bool bAlreadySelected = InterpEd->KeyIsInSelection(Group, Track, KeyIndex);
						if(bAlreadySelected)
						{
							InterpEd->RemoveKeyFromSelection(Group, Track, KeyIndex);
						}
						else
						{
							// NOTE: Do not clear previously-selected tracks because ctrl is down. 
							InterpEd->SelectTrack( Group, Track, false );
							InterpEd->AddKeyToSelection(Group, Track, KeyIndex, !bShiftDown);
						}
					}
				}

				if(bTransactionBegun)
				{
					InterpEd->EndMoveSelectedKeys();
					bTransactionBegun = false;
				}

				if(bGrabbingMarker)
				{
					InterpEd->EndMoveMarker();
					bGrabbingMarker = false;
				}

				InViewport->LockMouseToViewport(false);

				DistanceDragged = 0;

				bPanning = false;
				bMouseDown = false;
				bGrabbingHandle = false;
				bNavigating = false;
				bBoxSelecting = false;
			}
			break;
		}
	}
	else if( Key == EKeys::RightMouseButton )
	{
		switch( Event )
		{
		case IE_Pressed:
			{
				HHitProxy*	HitResult = InViewport->GetHitProxy(HitX,HitY);

				if(HitResult)
				{
					// User right-click somewhere in the track editor
					TSharedPtr<SWidget> Menu = InterpEd->CreateContextMenu( InViewport, HitResult, bIsDirectorTrackWindow );
					if (Menu.IsValid())
					{
						// Redraw the viewport so the user can see which object was right clicked on
						InViewport->Draw();
						FlushRenderingCommands();

						TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow(); 
						if ( Parent.IsValid() )
						{
							FSlateApplication::Get().PushMenu(
								Parent.ToSharedRef(),
								FWidgetPath(),
								Menu.ToSharedRef(), 
								FSlateApplication::Get().GetCursorPos(), 
								FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
						}
					}
				}
			}
			break;

		case IE_Released:
			{
				InViewport->InvalidateHitProxy();
			}
			break;
		}
	}

	if(Event == IE_Pressed)
	{
		if(Key == EKeys::MouseScrollDown)
		{
			InterpEd->ZoomView( FMatinee::InterpEditor_ZoomIncrement, InterpEd->bZoomToScrubPos );
		}
		else if(Key == EKeys::MouseScrollUp)
		{
			InterpEd->ZoomView( 1.0f / FMatinee::InterpEditor_ZoomIncrement, InterpEd->bZoomToScrubPos );
		}

		FModifierKeysState ModKeys(bShiftDown, bShiftDown, bCtrlDown, bCtrlDown, bAltDown, bAltDown, bCmdDown, bCmdDown, bCapsDown);
		FKeyEvent KeyEvent(Key, ModKeys, 0/*UserIndex*/, false, 0, 0);
		InterpEd->ProcessCommandBindings(KeyEvent);
	}

	// Handle viewport screenshot.
	InputTakeScreenshot( InViewport, Key, Event );

	return true;
}

// X and Y here are the new screen position of the cursor.
void FMatineeViewportClient::MouseMove(FViewport* InViewport, int32 X, int32 Y)
{
	bool bCtrlDown = InViewport->KeyState(EKeys::LeftControl) || InViewport->KeyState(EKeys::RightControl);

	int32 DeltaX = OldMouseX - X;
	int32 DeltaY = OldMouseY - Y;

	if(bMouseDown)
	{
		DistanceDragged += ( FMath::Abs<int32>(DeltaX) + FMath::Abs<int32>(DeltaY) );
	}

	OldMouseX = X;
	OldMouseY = Y;


	if(bMouseDown)
	{
		if(DragObject != NULL)
		{
			DragData.PixelsPerSec = InterpEd->PixelsPerSec;
			DragData.MouseCurrent = FIntPoint(X, Y);
			DragObject->ObjectDragged(DragData);
		}
		else if(bGrabbingHandle)
		{
			float NewTime = InterpEd->ViewStartTime + ((X - InterpEd->LabelWidth) / InterpEd->PixelsPerSec);
			if( InterpEd->bSnapToFrames && InterpEd->bSnapTimeToFrames )
			{
				NewTime = InterpEd->SnapTimeToNearestFrame( NewTime );
			}

			InterpEd->SetInterpPosition( NewTime, true );
		}
		else if(bBoxSelecting)
		{
			BoxEndX = X;
			BoxEndY = Y;
		}
		else if( bCtrlDown && InterpEd->Opt->SelectedKeys.Num() > 0 )
		{
			if(DistanceDragged > 4)
			{
				if(!bTransactionBegun)
				{
					InterpEd->BeginMoveSelectedKeys();
					bTransactionBegun = true;
				}

				float DeltaTime = -DeltaX / InterpEd->PixelsPerSec;
				InterpEd->MoveSelectedKeys(DeltaTime);
			}
		}
		else if(bNavigating)
		{
			float DeltaTime = -DeltaX / InterpEd->NavPixelsPerSecond;
			InterpEd->ViewStartTime += DeltaTime;
			InterpEd->ViewEndTime += DeltaTime;
			InterpEd->SyncCurveEdView();
		}
		else if(bGrabbingMarker)
		{
			float DeltaTime = -DeltaX / InterpEd->PixelsPerSec;
			InterpEd->UnsnappedMarkerPos += DeltaTime;

			if(InterpEd->GrabbedMarkerType == EMatineeMarkerType::ISM_SeqEnd)
			{
				InterpEd->SetInterpEnd( InterpEd->SnapTime(InterpEd->UnsnappedMarkerPos, false) );
			}
			else if(InterpEd->GrabbedMarkerType == EMatineeMarkerType::ISM_LoopStart || InterpEd->GrabbedMarkerType == EMatineeMarkerType::ISM_LoopEnd)
			{
				InterpEd->MoveLoopMarker( InterpEd->SnapTime(InterpEd->UnsnappedMarkerPos, false), InterpEd->GrabbedMarkerType == EMatineeMarkerType::ISM_LoopStart );
			}
		}
		else if(bPanning)
		{
			const bool bInvertPanning = InterpEd->IsInvertPanToggled();

			float DeltaTime = (bInvertPanning ? -DeltaX : DeltaX) / InterpEd->PixelsPerSec;
			InterpEd->ViewStartTime -= DeltaTime;
			InterpEd->ViewEndTime -= DeltaTime;

			// Handle vertical scrolling if the user moved the mouse up or down.
			// Vertical scrolling is handled by modifying the scroll bar attributes
			// because the scroll bar drives the vertical position of the viewport. 
			if( DeltaY != 0 )
			{
				// Account for the 'Drag Moves Canvas' option, which determines if panning 
				// is inverted, when figuring out the desired destination thumb position.
				const int32 TargetThumbPosition = bInvertPanning ? ThumbPos_Vert - DeltaY : ThumbPos_Vert + DeltaY;

				// Figure out which window we are panning
				TSharedPtr<SMatineeViewport> WindowToPan = bIsDirectorTrackWindow ? InterpEd->DirectorTrackWindow : InterpEd->TrackWindow;

				// Determine the maximum scroll position in order to prevent the user from scrolling beyond the 
				// valid scroll range. The max scroll position is not equivalent to the max range of the scroll 
				// bar. We must subtract the size of the thumb because the thumb position is the top of the thumb.
				int32 MaxThumbPosition = ComputeGroupListContentHeight(); 

				// Make sure the max thumb position is not negative. This is possible if the amount 
				// of scrollable space is less than the scroll bar (i.e. no scrolling possible). 
				MaxThumbPosition = FMath::Max<int32>( 0, MaxThumbPosition );

				// For some reason, the thumb position is always negated. So, instead 
				// of clamping from zero - max, we clamp from -max to zero.  
				ThumbPos_Vert = FMath::Clamp<int32>( TargetThumbPosition, -MaxThumbPosition, 0 );

				// Redraw the scroll bar so that it is at its new position. 
				WindowToPan->AdjustScrollBar();
			}

			InterpEd->SyncCurveEdView();
		}
	}
}

bool FMatineeViewportClient::InputAxis(FViewport* InViewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
{
	if ( Key == EKeys::MouseX || Key == EKeys::MouseY )
	{
		int32 X = InViewport->GetMouseX();
		int32 Y = InViewport->GetMouseY();
		MouseMove(InViewport, X, Y);
		return true;
	}
	return false;
}

EMouseCursor::Type FMatineeViewportClient::GetCursor(FViewport* InViewport,int32 X,int32 Y)
{
	EMouseCursor::Type Result = EMouseCursor::Crosshairs;

	if(DragObject==nullptr)
	{
		HHitProxy*	HitProxy = InViewport->GetHitProxy(X,Y);
		
		if(HitProxy)
		{
			Result = HitProxy->GetMouseCursor();
		}
	}
	else
	{
		Result = EMouseCursor::Default;
	}

	return Result;
}

void FMatineeViewportClient::Tick(float DeltaSeconds)
{
	// Only the main track window is allowed to tick the root object.  We never want the InterpEd object to be
	// ticked more than once per frame.
	if( !bIsDirectorTrackWindow )
	{
		InterpEd->TickInterp(DeltaSeconds);
	}

	// If curve editor is shown - sync us with it.
	if(InterpEd->CurveEd->GetVisibility() == EVisibility::Visible)
	{
		InterpEd->ViewStartTime = InterpEd->CurveEd->GetStartIn();
		InterpEd->ViewEndTime = InterpEd->CurveEd->GetEndIn();
	}

	if(bNavigating || bPanning)
	{
		const int32 ScrollBorderSize = 20;
		const float	ScrollBorderSpeed = 500.f;
		const int32 PosX = Viewport->GetMouseX();
		const int32 PosY = Viewport->GetMouseY();
		const int32 SizeX = Viewport->GetSizeXY().X;
		const int32 SizeY = Viewport->GetSizeXY().Y;

		float DeltaTime = FMath::Clamp(DeltaSeconds, 0.01f, 1.0f);

		if(PosX < ScrollBorderSize)
		{
			ScrollAccum.X += (1.f - ((float)PosX/(float)ScrollBorderSize)) * ScrollBorderSpeed * DeltaTime;
		}
		else if(PosX > SizeX - ScrollBorderSize)
		{
			ScrollAccum.X -= ((float)(PosX - (SizeX - ScrollBorderSize))/(float)ScrollBorderSize) * ScrollBorderSpeed * DeltaTime;
		}
		else
		{
			ScrollAccum.X = 0.f;
		}

		// Apply integer part of ScrollAccum to the curve editor view position.
		const int32 DeltaX = FMath::FloorToInt(ScrollAccum.X);
		ScrollAccum.X -= DeltaX;

		if(bNavigating)
		{
			DeltaTime = -DeltaX / InterpEd->NavPixelsPerSecond;
			InterpEd->ViewStartTime += DeltaTime;
			InterpEd->ViewEndTime += DeltaTime;

			InterpEd->SyncCurveEdView();
		}
		else
		{
			DeltaTime = -DeltaX / InterpEd->PixelsPerSec;
			InterpEd->ViewStartTime -= DeltaTime;
			InterpEd->ViewEndTime -= DeltaTime;
			InterpEd->SyncCurveEdView();
		}
	}

	Viewport->Draw();

	//check to see if we need to update the scrollbars due to a size change
	int32 CurrentHeight = Viewport->GetSizeXY().Y;
	if (CurrentHeight != PrevViewportHeight)
	{
		PrevViewportHeight = CurrentHeight;
		InterpEd->UpdateTrackWindowScrollBars();
	}
}


void FMatineeViewportClient::Serialize(FArchive& Ar) 
{ 
	// Drag object may be a instance of UObject, so serialize it if it is.
	if(DragObject && DragObject->GetUObject())
	{
		UObject* DragUObject = DragObject->GetUObject();
		Ar << DragUObject;
	}
}

/** Exec handler */
void FMatineeViewportClient::Exec(const TCHAR* Cmd)
{
	const TCHAR* Str = Cmd;

	if(FParse::Command(&Str, TEXT("MATINEE")))
	{
		if(FParse::Command(&Str, TEXT("Undo")))
		{
			InterpEd->InterpEdUndo();
		}
		else if(FParse::Command(&Str, TEXT("Redo")))
		{
			InterpEd->InterpEdRedo();
		}
		else if(FParse::Command(&Str, TEXT("Cut")))
		{
			InterpEd->CopySelectedGroupOrTrack(true);
		}
		else if(FParse::Command(&Str, TEXT("Copy")))
		{
			InterpEd->CopySelectedGroupOrTrack(false);
		}
		else if(FParse::Command(&Str, TEXT("Paste")))
		{
			InterpEd->PasteSelectedGroupOrTrack();
		}
		else if(FParse::Command(&Str, TEXT("Play")))
		{
			InterpEd->StartPlaying( false, true );
		}
		else if(FParse::Command(&Str, TEXT("PlayReverse")))
		{
			InterpEd->StartPlaying( false, false );
		}
		else if(FParse::Command(&Str, TEXT("Stop")))
		{
			if(InterpEd->MatineeActor->bIsPlaying)
			{
				InterpEd->StopPlaying();
			}
		}
		else if(FParse::Command(&Str, TEXT("Rewind")))
		{
			InterpEd->SetInterpPosition(0.f);
		}
		else if(FParse::Command(&Str, TEXT("TogglePlayPause")))
		{
			if(InterpEd->MatineeActor->bIsPlaying)
			{
				InterpEd->StopPlaying();
			}
			else
			{
				// Start playback and retain whatever direction we were already playing
				InterpEd->StartPlaying( false, true );
			}
		}
		else if( FParse::Command( &Str, TEXT( "ZoomIn" ) ) )
		{
			const bool bZoomToTimeCursorPos = true;
			InterpEd->ZoomView( 1.0f / FMatinee::InterpEditor_ZoomIncrement, bZoomToTimeCursorPos );
		}
		else if( FParse::Command( &Str, TEXT( "ZoomOut" ) ) )
		{
			const bool bZoomToTimeCursorPos = true;
			InterpEd->ZoomView( FMatinee::InterpEditor_ZoomIncrement, bZoomToTimeCursorPos );
		}
		else if(FParse::Command(&Str, TEXT("DeleteSelection")))
		{
			InterpEd->DeleteSelection();
		}
		else if(FParse::Command(&Str, TEXT("MarkInSection"))) 
		{
			InterpEd->MoveLoopMarker(InterpEd->MatineeActor->InterpPosition, true);
		}
		else if(FParse::Command(&Str, TEXT("MarkOutSection"))) 
		{
			InterpEd->MoveLoopMarker(InterpEd->MatineeActor->InterpPosition, false);
		}
		else if(FParse::Command(&Str, TEXT("CropAnimationBeginning")))
		{
			InterpEd->CropAnimKey(true);
		}
		else if(FParse::Command(&Str, TEXT("CropAnimationEnd")))
		{
			InterpEd->CropAnimKey(false);
		}
		else if(FParse::Command(&Str, TEXT("IncrementPosition")))
		{
			InterpEd->IncrementSelection();
		}
		else if(FParse::Command(&Str, TEXT("DecrementPosition")))
		{
			InterpEd->DecrementSelection();
		}
		else if(FParse::Command(&Str, TEXT("MoveToNextKey")))
		{
			InterpEd->SelectNextKey();
		}
		else if(FParse::Command(&Str, TEXT("MoveToPrevKey")))
		{
			InterpEd->SelectPreviousKey();
		}
		else if(FParse::Command(&Str, TEXT("SplitAnimKey")))
		{
			InterpEd->SplitAnimKey();
		}
		else if(FParse::Command(&Str, TEXT("ToggleSnap")))
		{
			InterpEd->SetSnapEnabled(!InterpEd->bSnapEnabled);
		}
		else if(FParse::Command(&Str, TEXT("ToggleSnapTimeToFrames")))
		{
			InterpEd->SetSnapTimeToFrames(!InterpEd->bSnapTimeToFrames);
		}
		else if(FParse::Command(&Str, TEXT("ToggleFixedTimeStepPlayback")))
		{
			InterpEd->SetFixedTimeStepPlayback( !InterpEd->bFixedTimeStepPlayback );
		}
		else if(FParse::Command(&Str, TEXT("TogglePreferFrameNumbers")))
		{
			InterpEd->SetPreferFrameNumbers( !InterpEd->bPreferFrameNumbers );
		}
		else if(FParse::Command(&Str, TEXT("ToggleShowTimeCursorPosForAllKeys")))
		{
			InterpEd->SetShowTimeCursorPosForAllKeys( !InterpEd->bShowTimeCursorPosForAllKeys );
		}
		else if(FParse::Command(&Str, TEXT("MoveActiveUp")))
		{
			InterpEd->MoveActiveUp();
		}
		else if(FParse::Command(&Str, TEXT("MoveActiveDown")))
		{
			InterpEd->MoveActiveDown();
		}
		else if(FParse::Command(&Str, TEXT("AddKey")))
		{
			InterpEd->AddKey();
		}
		else if(FParse::Command(&Str, TEXT("DuplicateSelectedKeys")) )
		{
			InterpEd->DuplicateSelectedKeys();
		}
		else if(FParse::Command(&Str, TEXT("ViewFitSequence")) )
		{
			InterpEd->ViewFitSequence();
		}
		else if(FParse::Command(&Str, TEXT("ViewFitToSelected")) )
		{
			InterpEd->ViewFitToSelected();
		}
		else if(FParse::Command(&Str, TEXT("ViewFitLoop")) )
		{
			InterpEd->ViewFitLoop();
		}
		else if(FParse::Command(&Str, TEXT("ViewFitLoopSequence")) )
		{
			InterpEd->ViewFitLoopSequence();
		}
		else if(FParse::Command(&Str, TEXT("ViewEndOfTrack")) )
		{
			InterpEd->ViewEndOfTrack();
		}
		else if(FParse::Command(&Str, TEXT("ChangeKeyInterpModeAUTO")) )
		{
			InterpEd->ChangeKeyInterpMode(CIM_CurveAuto);
		}
		else if(FParse::Command(&Str, TEXT("ChangeKeyInterpModeAUTOCLAMPED")) )
		{
			InterpEd->ChangeKeyInterpMode(CIM_CurveAutoClamped);
		}
		else if(FParse::Command(&Str, TEXT("ChangeKeyInterpModeUSER")) )
		{
			InterpEd->ChangeKeyInterpMode(CIM_CurveUser);
		}
		else if(FParse::Command(&Str, TEXT("ChangeKeyInterpModeBREAK")) )
		{
			InterpEd->ChangeKeyInterpMode(CIM_CurveBreak);
		}
		else if(FParse::Command(&Str, TEXT("ChangeKeyInterpModeLINEAR")) )
		{
			InterpEd->ChangeKeyInterpMode(CIM_Linear);
		}
		else if(FParse::Command(&Str, TEXT("ChangeKeyInterpModeCONSTANT")) )
		{
			InterpEd->ChangeKeyInterpMode(CIM_Constant);
		}
	}
}


