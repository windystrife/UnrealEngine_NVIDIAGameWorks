// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "CanvasItem.h"
#include "Engine/Brush.h"
#include "Animation/SkeletalMeshActor.h"
#include "Camera/CameraActor.h"
#include "Particles/Emitter.h"
#include "Engine/Light.h"
#include "Engine/StaticMeshActor.h"
#include "Interpolation.h"
#include "CanvasTypes.h"
#include "Matinee/InterpTrack.h"
#include "MatineeHitProxy.h"
#include "MatineeOptions.h"
#include "MatineeViewportData.h"
#include "MatineeViewportClient.h"
#include "EngineGlobals.h"
#include "Editor.h"
#include "Engine/InterpCurveEdSetup.h"
#include "Matinee.h"

#include "InterpolationHitProxy.h"
#include "Slate/SceneViewport.h"

#include "Matinee/MatineeActor.h"
#include "Matinee/InterpGroupDirector.h"
#include "Matinee/InterpGroupInst.h"
#include "Matinee/InterpTrackFloatBase.h"
#include "Matinee/InterpTrackVectorBase.h"
#include "Matinee/InterpTrackLinearColorBase.h"
#include "Matinee/InterpTrackEvent.h"
#include "Matinee/InterpTrackMove.h"
#include "Materials/MaterialInstanceActor.h"

static const int32 GroupHeadHeight = 24;
static const int32 TrackHeight = 24;
static const int32 SubTrackHeight = 19;
static const int32 HeadTitleMargin = 4;

static const int32 TimelineHeight = 40;
static const int32 NavHeight = 24;
static const int32 TotalBarHeight = (TimelineHeight + NavHeight);

static const int32 TimeIndHalfWidth = 2;
static const int32 RangeTickHeight = 8;

static const FColor NullRegionColor(60, 60, 60);
static const FColor NullRegionBorderColor(255, 255, 255);

static const FColor InterpMarkerColor(255, 80, 80);
static const FColor SectionMarkerColor(80, 255, 80);

static const FColor KeyRangeMarkerColor(255, 183, 111);


namespace MatineeGlobals
{
	/** How far to indent the tree labels from the right side of the track editor scroll bar */
	static const int32 TreeLabelsMargin = HeadTitleMargin + 16;

	/** Number of pixels that child groups (and their tracks) should be indented */
	static const int32 NumPixelsToIndentChildGroups = 14;

	/** Number of pixels that track labels should be indented */
	static const int32 TrackTitleMargin = NumPixelsToIndentChildGroups;

	/** How far to offset the 'disable track' check box from the right side of the track editor scroll bar */
	static const int32 DisableTrackCheckBoxHorizOffset = 2;

	/** Size of the 'disable track' check box in pixels */
	static const FVector2D DisableTrackIconSize( 12, 12 );

	/** Horizontal offset for vertical separator line (between track check box and title names) */
	static const int32 TreeLabelSeparatorOffset = 17;

	/** Color of group label text */
	static const FColor GroupNameTextColor( 255, 255, 255 );

	/** The color of a selected group or track heading. (transparent) */
	static const FColor GroupOrTrackSelectedColor(255, 130, 30, 60);

	/** The color of a selected group or track border */
	static const FColor GroupOrTrackSelectedBorder(255, 130, 30);

	/** This is the color of a group heading label when a sub-track is currently selected. (transparent) */
	static const FColor GroupColorWithTrackSelected(255, 130, 30, 30);

	/** This is the border color of a group heading label when a sub-track is currently selected */
	static const FColor GroupBorderWithTrackSelected(128, 65, 15);

	/** Color of a folder label */
	static const FColor FolderLabelColor( 80, 80, 80 );

	/** Color of a default (uncategorized) group label */
	static const FColor DefaultGroupLabelColor(130, 130, 130);

	/** Color of a director group label */
	static const FColor DirGroupLabelColor(140, 130, 130);

	/** Color of background area on the left side of the track editor (where users can right click to summon a pop up menu) */
	static const FColor TrackLabelAreaBackgroundColor( 60, 60, 60 );
}


/* Draws shadowed text; ensures that the text is pixel-aligned for readability */
int32 FMatineeViewportClient::DrawLabel( FCanvas* Canvas, float StartX, float StartY, const TCHAR* Text, const FLinearColor& Color )
{
	int32 Result = 0;
	if( LabelFont != NULL )
	{
		Result = Canvas->DrawShadowedString( FMath::TruncToFloat(StartX), FMath::TruncToFloat(StartY), Text, LabelFont, Color );
	}
	return Result;
}


float FMatineeViewportClient::GetGridSpacing(int32 GridNum)
{
	if(GridNum & 0x01) // Odd numbers
	{
		return FMath::Pow( 10.f, 0.5f*((float)(GridNum-1)) + 1.f );
	}
	else // Even numbers
	{
		return 0.5f * FMath::Pow( 10.f, 0.5f*((float)(GridNum)) + 1.f );
	}
}

/** Calculate the best frames' density. */
uint32 FMatineeViewportClient::CalculateBestFrameStep(float SnapAmount, float PixelsPerSec, float MinPixelsPerGrid)
{
	uint32 FrameRate = FMath::CeilToInt(1.0f / SnapAmount);
	uint32 FrameStep = 1;
	
	// Calculate minimal-symmetric integer divisor.
	uint32 MinFrameStep = FrameRate;
	uint32 i = 2;	
	while ( i < MinFrameStep )
	{
		if ( MinFrameStep % i == 0 )
		{
			MinFrameStep /= i;
			i = 1;
		}
		i++;
	}	

	// Find the best frame step for certain grid density.
	while (	FrameStep * SnapAmount * PixelsPerSec < MinPixelsPerGrid )
	{
		FrameStep++;
		if ( FrameStep < FrameRate )
		{
			// Must be divisible by MinFrameStep and divisor of FrameRate.
			while ( !(FrameStep % MinFrameStep == 0 && FrameRate % FrameStep == 0) )
			{
				FrameStep++;
			}
		}
		else
		{
			// Must be multiple of FrameRate.
			while ( FrameStep % FrameRate != 0 )
			{
				FrameStep++;
			}
		}
	}

	return FrameStep;
}



/**
 * Locates the director group in our list of groups (if there is one)
 *
 * @param OutDirGroupIndex	The index of the director group in the list (if it was found)
 *
 * @return Returns true if a director group was found
 */
bool FMatinee::FindDirectorGroup( int32& OutDirGroupIndex ) const
{
	// @todo: For much better performance, cache the director group index

	// Check to see if we have a director group.  If so, we'll want to draw it on top of the other items!
	bool bHaveDirGroup = false;
	OutDirGroupIndex = 0;
	for( int32 i = 0; i < IData->InterpGroups.Num(); ++i )
	{
		const UInterpGroup* Group = IData->InterpGroups[ i ];

		bool bIsDirGroup = Group->IsA( UInterpGroupDirector::StaticClass() );

		if( bIsDirGroup )
		{
			// Found the director group; we're done!
			bHaveDirGroup = true;
			OutDirGroupIndex = i;
			break;
		}
	}

	return bHaveDirGroup;
}




/**
 * Remaps the specified group index such that the director's group appears as the first element
 *
 * @param DirGroupIndex	The index of the 'director group' in the group list
 * @param ElementIndex	The original index into the group list
 *
 * @return Returns the reordered element index for the specified element index
 */
int32 FMatinee::RemapGroupIndexForDirGroup( const int32 DirGroupIndex, const int32 ElementIndex ) const
{
	int32 NewElementIndex = ElementIndex;

	if( ElementIndex == 0 )
	{
		// The first element should always be the director group.  We want it displayed on top.
		NewElementIndex = DirGroupIndex;
	}
	else
	{
		// For any elements up to the director group in the list, we'll need to adjust their element index
		// to account for the director group being remapped to the top of the list
		if( ElementIndex <= DirGroupIndex )
		{
			NewElementIndex = ElementIndex - 1;
		}
	}

	return NewElementIndex;
}

/**
 * Calculates the viewport vertical location for the given group.
 *
 * @param	InGroup		The group that owns the track.
 * @param	LabelTopPosition	The viewport vertical location for the group's label top. 
 * @param	LabelBottomPosition	The viewport vertical location for the group's label bottom. This is not the height of the label.
 */
void FMatinee::GetGroupLabelPosition( UInterpGroup* InGroup, int32& LabelTopPosition, int32& LabelBottomPosition ) const
{
	int32 TopPosition = 0;

	if(InGroup != NULL && InGroup->bVisible)
	{
		// The director group is always visually at the top of the list, so we don't need to do any scrolling
		// if the caller asked us for that group.
		if( !InGroup->IsA( UInterpGroupDirector::StaticClass() ) )
		{
			// Check to see if we have a director group.  If so, we'll want to draw it on top of the other items!
			int32 DirGroupIndex = 0;
			bool bHaveDirGroup = FindDirectorGroup( DirGroupIndex ); // Out

			const UInterpGroup* CurParentGroup = NULL;

			// Loop through groups adding height contribution till we find the group that we are scrolling to.
			for(int32 Idx = 0; Idx < IData->InterpGroups.Num(); ++Idx )
			{
				int32 CurGroupIndex = Idx;

				// If we have a director group then remap the group indices such that the director group is always drawn first
				if( bHaveDirGroup )
				{
					CurGroupIndex = RemapGroupIndexForDirGroup( DirGroupIndex, Idx );
				}

				const UInterpGroup* CurGroup = IData->InterpGroups[ CurGroupIndex ];
				check(CurGroup);

				// If this is the group we are looking for, stop searching
				if( CurGroup == InGroup)
				{
					break;
				}
				else
				{
					// Just skip the director group if we find it, it's never visible with the rest of the groups
					if( !CurGroup->IsA( UInterpGroupDirector::StaticClass() ) )
					{
						bool bIsGroupVisible = CurGroup->bVisible;
						if( CurGroup->bIsParented )
						{
							// If we're parented then we're only visible if our parent group is not collapsed
							check( CurParentGroup != NULL );
							if( CurParentGroup->bCollapsed )
							{
								// Parent group is collapsed, so we should not be rendered
								bIsGroupVisible = false;
							}
						}
						else
						{
							// If this group is not parented, then we clear our current parent
							CurParentGroup = NULL;
						}

						// If the group is visible, add the number of tracks in it as visible as well if it is not collapsed.
						if( bIsGroupVisible )
						{
							TopPosition += GroupHeadHeight;

							if( CurGroup->bCollapsed == false )
							{
								// Account for visible tracks in this group
								for( int32 CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
								{
									if( CurGroup->InterpTracks[ CurTrackIndex ]->bVisible )
									{
										TopPosition += TrackHeight;
									}
								}
							}
						}
					}
				}

				// If the current group is not parented, then it becomes our current parent group
				if( !CurGroup->bIsParented )
				{
					CurParentGroup = CurGroup;
				}
			}
		}
	}

	LabelTopPosition = TopPosition;
	LabelBottomPosition = LabelTopPosition + GroupHeadHeight;
}

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
void FMatinee::GetTrackLabelPositions( UInterpGroup* InGroup, int32 InTrackIndex, int32& LabelTopPosition, int32& LabelBottomPosition ) const
{
	int32 TopPosition = 0;

	if(InGroup != NULL)
	{
		// First find the position of the group that owns the track. 
		int32 GroupLabelBottom = 0;
		GetGroupLabelPosition( InGroup, TopPosition, GroupLabelBottom );

		// Now, add the height of all the tracks that come before this one. 
		if( InGroup->bCollapsed == false )
		{
			// Start from the bottom of the group label instead. 
			TopPosition = GroupLabelBottom;

			// Account for visible tracks in this group
			for( int32 TrackIndex = 0; TrackIndex < InGroup->InterpTracks.Num(); ++TrackIndex )
			{
				// If we found the track we were looking for, 
				// we don't need to add anymore height pixels. 
				if( TrackIndex == InTrackIndex )
				{
					break;
				}
				else if( InGroup->InterpTracks[ TrackIndex ]->bVisible )
				{
					TopPosition += TrackHeight;
				}
			}
		}
	}

	LabelTopPosition = TopPosition;
	LabelBottomPosition = LabelTopPosition + TrackHeight;
}

/**
 * Scrolls the view to the specified group if it is visible, otherwise it scrolls to the top of the screen.
 *
 * @param InGroup	Group to scroll the view to.
 */
void FMatinee::ScrollToGroup(class UInterpGroup* InGroup)
{

	int32 ScrollPos = 0;
	int32 GroupLabelBottom = 0;
	GetGroupLabelPosition(InGroup, ScrollPos, GroupLabelBottom);

	// Set final scroll pos.
	if(TrackWindow.IsValid() && TrackWindow->ScrollBar_Vert.IsValid())
	{
		// Adjust our scroll position by the size of the viewable area.  This prevents us from scrolling
		// the list such that there are elements above the the top of the window that cannot be reached
		// with the scroll bar.  Plus, it just feels better!
		{
			const uint32 ViewportHeight = TrackWindow->Viewport->GetSizeXY().Y;
			int32 ContentBoxHeight = TrackWindow->InterpEdVC->ComputeGroupListBoxHeight( ViewportHeight );

			ScrollPos -= ContentBoxHeight - GroupHeadHeight;
			if( ScrollPos < 0 )
			{
				ScrollPos = 0;
			}
		}

		TrackWindow->InterpEdVC->ThumbPos_Vert = -ScrollPos;

		TrackWindow->AdjustScrollBar();
	}
}



/**
 * Updates the track window list scroll bar's vertical range to match the height of the window's content
 */
void FMatinee::UpdateTrackWindowScrollBars()
{
	// Simply ask our track window to update its scroll bar
	if( TrackWindow.IsValid())
	{
		TrackWindow->AdjustScrollBar();
	}
	if( DirectorTrackWindow.IsValid())
	{
		DirectorTrackWindow->AdjustScrollBar();
	}
}



/**
 * Dirty the contents of the track window viewports
 */
void FMatinee::InvalidateTrackWindowViewports()
{
	if( TrackWindow.IsValid() )
	{
		TrackWindow->InterpEdVC->Viewport->Invalidate();
	}
	if( DirectorTrackWindow.IsValid() )
	{
		DirectorTrackWindow->InterpEdVC->Viewport->Invalidate();
	}
}



/**
 * Creates a string with timing/frame information for the specified time value in seconds
 *
 * @param InTime The time value to create a timecode for
 * @param bIncludeMinutes true if the returned string should includes minute information
 *
 * @return The timecode string
 */
FString FMatinee::MakeTimecodeString( float InTime, bool bIncludeMinutes ) const
{
	// SMPTE-style timecode
	const int32 MinutesVal = FMath::TruncToInt( InTime / 60.0f );
	const int32 SecondsVal = FMath::TruncToInt( InTime );
	const float Frames = InTime / SnapAmount;
	const int32 FramesVal = FMath::RoundToInt( Frames );
	const float Subseconds = FMath::Fractional( InTime );
	const float SubsecondFrames = Subseconds / SnapAmount;
	const int32 SubsecondFramesVal = FMath::RoundToInt( SubsecondFrames );
	const float SubframeDiff = Frames - ( float )FramesVal;

	// Are we currently between frames?
	const bool bIsBetweenFrames = !FMath::IsNearlyEqual( SubframeDiff, 0.0f );

	const TCHAR SubframeSignChar = ( SubframeDiff >= 0.0f ) ? TCHAR( '+' ) : TCHAR( '-' );

	FString TimecodeString;
	if( bIncludeMinutes )
	{
		TimecodeString =
			FString::Printf(
				TEXT( "%02i:%02i:%02i %s" ),
				MinutesVal,
				SecondsVal,
				SubsecondFramesVal,
				bIsBetweenFrames ? *FString::Printf( TEXT( "%c%.2f" ), SubframeSignChar, FMath::Abs( SubframeDiff ) ) : TEXT( "" ) );
	}
	else
	{
		TimecodeString =
			FString::Printf(
				TEXT( "%02i:%02i %s" ),
				SecondsVal,
				SubsecondFramesVal,
				bIsBetweenFrames ? *FString::Printf( TEXT( "%c%.2f" ), SubframeSignChar, FMath::Abs( SubframeDiff ) ) : TEXT( "" ) );
	}

	return TimecodeString;
}



/** Draw gridlines and time labels. */
void FMatineeViewportClient::DrawGrid(FViewport* InViewport, FCanvas* Canvas, bool bDrawTimeline)
{
	const int32 ViewX = InViewport->GetSizeXY().X;
	const int32 ViewY = InViewport->GetSizeXY().Y;

	// Calculate desired grid spacing.
	int32 MinPixelsPerGrid = 35;
	float MinGridSpacing = 0.001f;
	float GridSpacing = MinGridSpacing;
	uint32 FrameStep = 1; // Important frames' density.
	uint32 AuxFrameStep = 1; // Auxiliary frames' density.
	
	// Time.
	if (!InterpEd->bSnapToFrames)
	{
		int32 GridNum = 0;
		while( GridSpacing * InterpEd->PixelsPerSec < MinPixelsPerGrid )
		{
			GridSpacing = MinGridSpacing * GetGridSpacing(GridNum);
			GridNum++;
		}
	} 	
	else // Frames.
	{
		GridSpacing  = InterpEd->SnapAmount;	
		FrameStep = CalculateBestFrameStep(InterpEd->SnapAmount, InterpEd->PixelsPerSec, MinPixelsPerGrid);
		AuxFrameStep = CalculateBestFrameStep(InterpEd->SnapAmount, InterpEd->PixelsPerSec, 6);
	}

	FCanvasLineItem LineItem;									
	FCanvasTextItem TextItem( FVector2D::ZeroVector, FText::GetEmpty(), GEditor->GetSmallFont(), FLinearColor::Gray );
	
	int32 LineNum = FMath::FloorToInt(InterpEd->ViewStartTime/GridSpacing);
	while( LineNum*GridSpacing < InterpEd->ViewEndTime )
	{
		float LineTime = LineNum*GridSpacing;
		int32 LinePosX = InterpEd->LabelWidth + (LineTime - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
		
		FColor LineColor(110, 110, 110);
		
		// Change line color for important frames.
		if ( InterpEd->bSnapToFrames && LineNum % FrameStep == 0 )
		{
			LineColor = FColor(140,140,140);
		}

		if(bDrawTimeline)
		{
			
			// Show time or important frames' numbers (based on FrameStep).
			if ( !InterpEd->bSnapToFrames || FMath::Abs(LineNum) % FrameStep == 0 )
			{				
				// Draw grid lines and labels in timeline section.
				if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( new HMatineeTimelineBkg() );						


				FString Label;
				if (InterpEd->bSnapToFrames)
				{
					// Show frames' numbers.
					Label = FString::Printf( TEXT("%d"), LineNum );
				}
				else
				{
					// Show time.
					Label = FString::Printf( TEXT("%3.2f"), LineTime);
				}
				LineItem.SetColor( LineColor );
				LineItem.Draw( Canvas, FVector2D(LinePosX, ViewY - TotalBarHeight), FVector2D(LinePosX, ViewY) );
				TextItem.Text = FText::FromString( Label );
				TextItem.SetColor( FColor(175, 175, 175));
				TextItem.Draw( Canvas, LinePosX + 2, ViewY - NavHeight - 17 );

				if( InterpEd->bSnapToFrames )
				{
					// Draw timecode info above the frame number
					const bool bIncludeMinutesInTimecode = false;
					FString TimecodeString = InterpEd->MakeTimecodeString( LineTime, bIncludeMinutesInTimecode );
					TextItem.Text = FText::FromString( TimecodeString );
					TextItem.SetColor( FColor(140, 140, 140));
					TextItem.Scale = FVector2D( 0.9f, 0.9f );
					TextItem.Draw( Canvas, LinePosX + 2, ViewY - NavHeight - 32 );
				}

				if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( NULL );
			}						
		}
		else
		{
			// Draw grid lines in track view section. 
			if (!InterpEd->bSnapToFrames || (FMath::Abs(LineNum) % AuxFrameStep == 0))
			{
				int32 TrackAreaHeight = ViewY;
				if( bDrawTimeline )
				{
					TrackAreaHeight -= TotalBarHeight;
				}
				FCanvasLineItem CanvasLineItem( FVector2D(LinePosX, 0), FVector2D(LinePosX, TrackAreaHeight) );
				CanvasLineItem.SetColor( LineColor );
				Canvas->DrawItem( CanvasLineItem );
			}
		}		
		LineNum++;
	}
	
}

/** Draw the timeline control at the bottom of the editor. */
void FMatineeViewportClient::DrawTimeline(FViewport* InViewport, FCanvas* Canvas)
{
	int32 ViewX = InViewport->GetSizeXY().X;
	int32 ViewY = InViewport->GetSizeXY().Y;

	//////// DRAW TIMELINE
	// Entire length is clickable.

	if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( new HMatineeTimelineBkg() );
	Canvas->DrawTile(InterpEd->LabelWidth, ViewY - TotalBarHeight, ViewX - InterpEd->LabelWidth, TimelineHeight, 0.f, 0.f, 0.f, 0.f, FColor(80,80,80) );
	if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( NULL );

	DrawGrid(InViewport, Canvas, true);

	// Draw black line separating nav from timeline.
	Canvas->DrawTile(0, ViewY - TotalBarHeight, ViewX, 1, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );

	DrawMarkers(InViewport, Canvas);

	//////// DRAW NAVIGATOR
	{
		int32 ViewStart = InterpEd->LabelWidth + InterpEd->ViewStartTime * InterpEd->NavPixelsPerSecond;
		int32 ViewEnd = InterpEd->LabelWidth + InterpEd->ViewEndTime * InterpEd->NavPixelsPerSecond;

		// Background
		if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( new HMatineeNavigatorBackground() );
		Canvas->DrawTile(InterpEd->LabelWidth, ViewY - NavHeight, ViewX - InterpEd->LabelWidth, NavHeight, 0.f, 0.f, 0.f, 0.f, FColor(140,140,140) );
		Canvas->DrawTile(0, ViewY - NavHeight, ViewX, 1, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );
		if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( NULL );

		// Foreground
		if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( new HMatineeNavigator() );
		Canvas->DrawTile( ViewStart, ViewY - NavHeight, ViewEnd - ViewStart, NavHeight, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );
		Canvas->DrawTile( ViewStart+1, ViewY - NavHeight + 1, ViewEnd - ViewStart - 2, NavHeight - 2, 0.f, 0.f, 1.f, 1.f, FLinearColor::White );

		// Tick indicating current position in global navigator
		Canvas->DrawTile( InterpEd->LabelWidth + InterpEd->MatineeActor->InterpPosition * InterpEd->NavPixelsPerSecond, ViewY - 0.5*NavHeight - 4, 2, 8, 0.f, 0.f, 0.f, 0.f, FColor(80,80,80) );
		if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( NULL );
	}



	//////// DRAW INFO BOX

	Canvas->DrawTile( 0, ViewY - TotalBarHeight, InterpEd->LabelWidth, TotalBarHeight, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );

	// Draw current time in bottom left.
	int32 XL, YL;

	UFont* Font = GEngine->GetSmallFont();
	FString PosString = FString::Printf( TEXT("%3.3f / %3.3f %s"), InterpEd->MatineeActor->InterpPosition, InterpEd->IData->InterpLength, *NSLOCTEXT("UnrealEd", "InterpEd_TimelineInfo_Seconds", "Seconds").ToString() );
	StringSize( Font, XL, YL, *PosString );
	
	FCanvasTextItem TextItem( FVector2D( HeadTitleMargin, ViewY - YL - HeadTitleMargin ), FText::FromString(PosString), Font, FLinearColor::Green );
	Canvas->DrawItem( TextItem );
	
	FString SnapPosString = "";

	//const int32 SelIndex = InterpEd->ToolBar->SnapCombo->GetSelection(); 
	int32 SelIndex = InterpEd->SnapSelectionIndex;

	// Determine if time should be drawn including frames or keys
	if(SelIndex == ARRAY_COUNT(FMatinee::InterpEdSnapSizes)+ARRAY_COUNT(FMatinee::InterpEdFPSSnapSizes))
	{
		UInterpTrack* Track = NULL;
		int32 SelKeyIndex = 0;

		// keys
		SnapPosString = FString::Printf( TEXT("%3.0f %s"), 0.0, *NSLOCTEXT("UnrealEd", "KeyFrames", "Keys").ToString() );

		// Work with the selected keys in a given track for a given group
		// Show the timeline if only 1 track is selected.
		if( InterpEd->GetSelectedTrackCount() == 1 )
		{
			Track = *InterpEd->GetSelectedTrackIterator();

			if(InterpEd->Opt->SelectedKeys.Num() > 0)
			{
				FInterpEdSelKey& SelKey = InterpEd->Opt->SelectedKeys[0];
				SelKeyIndex = SelKey.KeyIndex + 1;
			}

			if(Track)
			{
				SnapPosString = FString::Printf( TEXT("%3.0f / %3.0f %s"), SelKeyIndex * 1.0, Track->GetNumKeyframes() * 1.0, *NSLOCTEXT("UnrealEd", "KeyFrames", "Keys").ToString() );
			}
		}

		StringSize( Font, XL, YL, *SnapPosString );
	}
	else if(SelIndex < ARRAY_COUNT(FMatinee::InterpEdFPSSnapSizes)+ARRAY_COUNT(FMatinee::InterpEdSnapSizes) && SelIndex >= ARRAY_COUNT(FMatinee::InterpEdSnapSizes))
	{
		// frames

		// Timecode string
		SnapPosString = InterpEd->MakeTimecodeString(InterpEd->MatineeActor->InterpPosition);
		StringSize(Font, XL, YL, *SnapPosString);
		TextItem.SetColor(FLinearColor::Yellow);
		TextItem.Text = FText::FromString(SnapPosString);
		Canvas->DrawItem(TextItem, HeadTitleMargin, ViewY - YL - int32(1.7 * YL) - HeadTitleMargin);

		// Frame counts
		SnapPosString = FString::Printf( TEXT("%3.1f / %3.1f %s"), (1.0 / InterpEd->SnapAmount) * InterpEd->MatineeActor->InterpPosition, (1.0 / InterpEd->SnapAmount) * InterpEd->IData->InterpLength, *NSLOCTEXT("UnrealEd", "InterpEd_TimelineInfo_Frames", "frames").ToString() );
		StringSize( Font, XL, YL, *SnapPosString );
	}
	else if(SelIndex < ARRAY_COUNT(FMatinee::InterpEdSnapSizes))
	{
		// seconds
		SnapPosString = "";
	}
	else
	{
		// nothing
		SnapPosString = "";
	}
	TextItem.SetColor( FLinearColor::Green );
	TextItem.Text = FText::FromString( SnapPosString );
	Canvas->DrawItem( TextItem, HeadTitleMargin, ViewY - YL - int32(2.5 * YL) - HeadTitleMargin );

	// If adjusting current keyframe - draw little record message in bottom-left
	if(InterpEd->Opt->bAdjustingKeyframe)
	{
		check(InterpEd->Opt->SelectedKeys.Num() == 1);

		FInterpEdSelKey& rSelKey = InterpEd->Opt->SelectedKeys[0];
		FString KeyTitle = FString::Printf( TEXT("%s%d"), ( rSelKey.Track ? *rSelKey.Track->TrackTitle : TEXT( "?" ) ), rSelKey.KeyIndex );
		FString AdjustString = FText::Format( NSLOCTEXT("UnrealEd", "Key_F", "KEY {0}"), FText::FromString(KeyTitle) ).ToString();

		Canvas->DrawNGon(FIntPoint(HeadTitleMargin + 5, ViewY - 1.1 * YL - 2 * HeadTitleMargin), FColor::Red, 12, 5);
		TextItem.SetColor( FLinearColor::Red );
		TextItem.Text = FText::FromString( AdjustString );
		Canvas->DrawItem(TextItem, 2 * HeadTitleMargin + 10, (int32)( ViewY - 1.6 * YL - 2 * HeadTitleMargin ));
	}
	else if(InterpEd->Opt->bAdjustingGroupKeyframes)
	{
		check(InterpEd->Opt->SelectedKeys.Num() > 1);

		// Make a list of all the unique subgroups within the selection, cache for fast lookup
		TArray<FString> UniqueSubGroupNames;
		TArray<FString> KeySubGroupNames;
		TArray<FString> KeyTitles;
		for( int32 iSelectedKey = 0; iSelectedKey < InterpEd->Opt->SelectedKeys.Num(); iSelectedKey++ )
		{
			FInterpEdSelKey& rSelKey = InterpEd->Opt->SelectedKeys[iSelectedKey];
			FString SubGroupName = rSelKey.GetOwningTrackSubGroupName();
			UniqueSubGroupNames.AddUnique( SubGroupName );
			KeySubGroupNames.Add( SubGroupName );
			FString KeyTitle = FString::Printf( TEXT("%s%d"), ( rSelKey.Track ? *rSelKey.Track->TrackTitle : TEXT( "?" ) ), rSelKey.KeyIndex );
			KeyTitles.Add( KeyTitle );
		}

		// Order the string in the format subgroup[tracktrack] subgroup[track]
		FString AdjustString( "Keys_F " );
		for( int32 iUSubGroupName = 0; iUSubGroupName < UniqueSubGroupNames.Num(); iUSubGroupName++ )
		{
			FString& rUniqueSubGroupName = UniqueSubGroupNames[iUSubGroupName];
			AdjustString += rUniqueSubGroupName;
			AdjustString += TEXT( "[" );
			for( int32 iKSubGroupName = 0; iKSubGroupName < KeySubGroupNames.Num(); iKSubGroupName++ )
			{
				FString& KeySubGroupName = KeySubGroupNames[ iKSubGroupName ];
				if ( rUniqueSubGroupName == KeySubGroupName )
				{
					AdjustString += KeyTitles[ iKSubGroupName ];
				}
			}
			AdjustString += TEXT( "] " );
		}

		Canvas->DrawNGon(FIntPoint(HeadTitleMargin + 5, ViewY - 1.1*YL - 2 * HeadTitleMargin), FColor::Red, 12, 5);
		TextItem.SetColor( FLinearColor::Red );
		TextItem.Text = FText::FromString( AdjustString );
		Canvas->DrawItem(TextItem, 2 * HeadTitleMargin + 10, (int32)(ViewY - 1.6*YL - 2 * HeadTitleMargin));
	}

	///////// DRAW SELECTED KEY RANGE

	if(InterpEd->Opt->SelectedKeys.Num() > 0)
	{
		float KeyStartTime, KeyEndTime;
		InterpEd->CalcSelectedKeyRange(KeyStartTime, KeyEndTime);

		float KeyRange = KeyEndTime - KeyStartTime;
		FCanvasLineItem LineItem;
		LineItem.SetColor( KeyRangeMarkerColor );
		if( KeyRange > KINDA_SMALL_NUMBER && (KeyStartTime < InterpEd->ViewEndTime) && (KeyEndTime > InterpEd->ViewStartTime) )
		{
			// Find screen position of beginning and end of range.
			int32 KeyStartX = InterpEd->LabelWidth + (KeyStartTime - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
			int32 ClipKeyStartX = FMath::Max(KeyStartX, InterpEd->LabelWidth);

			int32 KeyEndX = InterpEd->LabelWidth + (KeyEndTime - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
			int32 ClipKeyEndX = FMath::Min(KeyEndX, ViewX);

			// Draw vertical ticks
			if(KeyStartX >= InterpEd->LabelWidth)
			{
				LineItem.Draw( Canvas, FVector2D(KeyStartX, ViewY - TotalBarHeight - RangeTickHeight), FVector2D(KeyStartX, ViewY - TotalBarHeight) );

				// Draw time above tick.
				FString StartString = FString::Printf( TEXT("%3.2fs"), KeyStartTime );
				if ( InterpEd->bSnapToFrames )
				{
					StartString += FString::Printf( TEXT(" / %df"), FMath::RoundToInt(KeyStartTime / InterpEd->SnapAmount));
				}
				StringSize( LabelFont, XL, YL, *StartString);
				DrawLabel(Canvas, KeyStartX - XL, ViewY - TotalBarHeight - RangeTickHeight - YL - 2, *StartString, KeyRangeMarkerColor);
			}

			if(KeyEndX <= ViewX)
			{
				LineItem.Draw( Canvas, FVector2D(KeyEndX, ViewY - TotalBarHeight - RangeTickHeight), FVector2D(KeyEndX, ViewY - TotalBarHeight) );

				// Draw time above tick.
				FString EndString = FString::Printf( TEXT("%3.2fs"), KeyEndTime );
				if ( InterpEd->bSnapToFrames )
				{
					EndString += FString::Printf( TEXT(" / %df"), FMath::RoundToInt(KeyEndTime / InterpEd->SnapAmount));
				}

				StringSize( LabelFont, XL, YL, *EndString);
				DrawLabel(Canvas, KeyEndX, ViewY - TotalBarHeight - RangeTickHeight - YL - 2, *EndString, KeyRangeMarkerColor);
			}

			// Draw line connecting them.
			int32 RangeLineY = ViewY - TotalBarHeight - 0.5f*RangeTickHeight;
			LineItem.Draw( Canvas, FVector2D(ClipKeyStartX, RangeLineY), FVector2D(ClipKeyEndX, RangeLineY) );

			// Draw range label above line
			// First find size of range string
			FString RangeString = FString::Printf( TEXT("%3.2fs"), KeyRange );
			if ( InterpEd->bSnapToFrames )
			{
				RangeString += FString::Printf( TEXT(" / %df"), FMath::RoundToInt(KeyRange / InterpEd->SnapAmount));
			}

			StringSize( LabelFont, XL, YL, *RangeString);

			// Find X position to start label drawing.
			int32 RangeLabelX = ClipKeyStartX + 0.5f*(ClipKeyEndX-ClipKeyStartX) - 0.5f*XL;
			int32 RangeLabelY = ViewY - TotalBarHeight - RangeTickHeight - YL;

			DrawLabel(Canvas,RangeLabelX, RangeLabelY, *RangeString, KeyRangeMarkerColor);
		}
		else
		{
			UInterpGroup* Group = InterpEd->Opt->SelectedKeys[0].Group;
			UInterpTrack* Track = InterpEd->Opt->SelectedKeys[0].Track;
			float KeyTime = Track->GetKeyframeTime( InterpEd->Opt->SelectedKeys[0].KeyIndex );

			int32 KeyX = InterpEd->LabelWidth + (KeyTime - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
			if((KeyX >= InterpEd->LabelWidth) && (KeyX <= ViewX))
			{
				LineItem.Draw( Canvas, FVector2D(KeyX, ViewY - TotalBarHeight - RangeTickHeight), FVector2D(KeyX, ViewY - TotalBarHeight) );

				FString KeyString = FString::Printf( TEXT("%3.2fs"), KeyTime );				
				if ( InterpEd->bSnapToFrames )
				{
					KeyString += FString::Printf( TEXT(" / %df"), FMath::RoundToInt(KeyTime / InterpEd->SnapAmount));
				}

				StringSize( LabelFont, XL, YL, *KeyString);

				int32 KeyLabelX = KeyX - 0.5f*XL;
				int32 KeyLabelY = ViewY - TotalBarHeight - RangeTickHeight - YL - 3;

				DrawLabel(Canvas,KeyLabelX, KeyLabelY, *KeyString, KeyRangeMarkerColor);
			}
		}
	}
}

/** Draw various markers on the timeline */
void FMatineeViewportClient::DrawMarkers(FViewport* InViewport, FCanvas* Canvas)
{
	int32 ViewX = InViewport->GetSizeXY().X;
	int32 ViewY = InViewport->GetSizeXY().Y;
	int32 ScaleTopY = ViewY - TotalBarHeight + 1;

	// Calculate screen X position that indicates current position in track.
	int32 TrackPosX = InterpEd->LabelWidth + (InterpEd->MatineeActor->InterpPosition - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;

	FCanvasTileItem TileItem( FVector2D::ZeroVector, FVector2D::ZeroVector, FColor(10,10,10) );
	TileItem.BlendMode = SE_BLEND_Translucent;
	FCanvasLineItem LineItem;	
	FCanvasTriangleItem TriItem( FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, GWhiteTexture );
	
	// Draw position indicator and line (if in viewed area)
	if( TrackPosX + TimeIndHalfWidth >= InterpEd->LabelWidth && TrackPosX <= ViewX)
	{
		if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( new HMatineeTimelineBkg() );
		TileItem.SetColor( FColor(10,10,10) );
		TileItem.Size = FVector2D( (2*TimeIndHalfWidth) + 1, TimelineHeight );
		TileItem.Draw( Canvas, FVector2D( TrackPosX - TimeIndHalfWidth - 1, ScaleTopY ) );
		if( Canvas->IsHitTesting() ) Canvas->SetHitProxy( NULL );
	}

	int32 MarkerArrowSize = 8;
	TileItem.Size = FVector2D( MarkerArrowSize, MarkerArrowSize );
	TileItem.SetColor( FLinearColor( 0.0f, 0.0f, 0.0f, 0.01f ) );

	FIntPoint StartA = FIntPoint(0,					ScaleTopY);
	FIntPoint StartB = FIntPoint(0,					ScaleTopY+MarkerArrowSize);
	FIntPoint StartC = FIntPoint(-MarkerArrowSize,	ScaleTopY);

	FIntPoint EndA = FIntPoint(0,					ScaleTopY);
	FIntPoint EndB = FIntPoint(MarkerArrowSize,		ScaleTopY);
	FIntPoint EndC = FIntPoint(0,					ScaleTopY+MarkerArrowSize);


	// NOTE:	Each marker is drawn with an invisible square behind it to increase the clickable 
	//			space for marker selection. However, the markers are represented visually as triangles.

	// Draw loop section start/end
	FIntPoint EdStartPos( InterpEd->LabelWidth + (InterpEd->IData->EdSectionStart - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec, MarkerArrowSize );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HMatineeMarker(EMatineeMarkerType::ISM_LoopStart) );
	TileItem.Draw( Canvas, FVector2D( EdStartPos.X - MarkerArrowSize, EdStartPos.Y + ScaleTopY ) );
	TriItem.SetColor( SectionMarkerColor );
	TriItem.SetPoints( StartA + EdStartPos, StartB + EdStartPos, StartC + EdStartPos );
	TriItem.Draw( Canvas );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );


	FIntPoint EdEndPos( InterpEd->LabelWidth + (InterpEd->IData->EdSectionEnd - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec, MarkerArrowSize );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HMatineeMarker(EMatineeMarkerType::ISM_LoopEnd) );
	TileItem.Draw( Canvas, FVector2D() );
	TriItem.SetColor( SectionMarkerColor );
	TriItem.SetPoints( EndA + EdEndPos,EndB + EdEndPos, EndC + EdEndPos );
	TriItem.Draw( Canvas );	
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );

	// Draw sequence start/end markers.
	FIntPoint StartPos( InterpEd->LabelWidth + (0.f - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec, 0 );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HMatineeMarker(EMatineeMarkerType::ISM_SeqStart) );
	TileItem.Draw( Canvas, FVector2D( StartPos.X - MarkerArrowSize, StartPos.Y + ScaleTopY ) );
	TriItem.SetColor( InterpMarkerColor );
	TriItem.SetPoints( StartA + StartPos,StartB + StartPos, StartC + StartPos );
	TriItem.Draw( Canvas );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );

	FIntPoint EndPos( InterpEd->LabelWidth + (InterpEd->IData->InterpLength - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec, 0 );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HMatineeMarker(EMatineeMarkerType::ISM_SeqEnd) );
	TileItem.Draw( Canvas, FVector2D( EndPos.X, EndPos.Y + ScaleTopY ) );
	TriItem.SetColor( InterpMarkerColor );
	TriItem.SetPoints( EndA + EndPos,EndB + EndPos, EndC + EndPos );
	TriItem.Draw( Canvas );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );

	// Draw little tick indicating path-building time.
	int32 PathBuildPosX = InterpEd->LabelWidth + (InterpEd->IData->PathBuildTime - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
	if( PathBuildPosX >= InterpEd->LabelWidth && PathBuildPosX <= ViewX)
	{
		TileItem.SetColor(FColor(200, 200, 255));
		TileItem.Size = FVector2D( 1.0f, 11.0f );
		TileItem.Draw( Canvas, FVector2D( PathBuildPosX, ViewY - NavHeight - 10 ) );
	}
}

namespace
{
	const FColor TabColorNormal(128,128,128);
	const FColor TabColorSelected(192,160,128);
	const int32 TabPadding = 1;
	const int32 TabSpacing = 4;
	const int32 TabRowHeight = 22;
}



/** 
 * Returns the vertical size of the entire group list for this viewport, in pixels
 */
int32 FMatineeViewportClient::ComputeGroupListContentHeight() const
{
	int32 HeightInPixels = 0;

	// Loop through groups adding height contribution
	if( InterpEd->IData != NULL )
	{
		for( int32 GroupIdx = 0; GroupIdx < InterpEd->IData->InterpGroups.Num(); ++GroupIdx )
		{
			const UInterpGroup* CurGroup = InterpEd->IData->InterpGroups[ GroupIdx ];

			// If this is a director group and the current window is not a director track window, then we'll skip over
			// the director group.  Similarly, for director track windows we'll skip over all non-director groups.
			if( CurGroup->IsA( UInterpGroupDirector::StaticClass() ) == bIsDirectorTrackWindow )
			{
				// If the group is visible, add the number of tracks in it as visible as well if it is not collapsed.
				if( CurGroup->bVisible )
				{
					HeightInPixels += GroupHeadHeight;

					// Also count the size of any expanded tracks in this group
					if( CurGroup->bCollapsed == false )
					{
						// Account for visible tracks in this group
						for( int32 CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
						{
							UInterpTrack* Track = CurGroup->InterpTracks[ CurTrackIndex ];
							if( Track->bVisible )
							{
								HeightInPixels += TrackHeight;

								if( Track->IsA( UInterpTrackMove::StaticClass() ) && Track->SubTracks.Num() > 0 )
								{
									// Move tracks have a 'group' for translation and rotation which increases the total height by 2 times track height
									HeightInPixels += (TrackHeight * 2);
								}

								// Increase height based on how many sub tracks are visible 
								for( int32 SubTrackIndex = 0; SubTrackIndex < Track->SubTracks.Num(); ++SubTrackIndex )
								{
									UInterpTrack* SubTrack = Track->SubTracks[ SubTrackIndex ];
									if( SubTrack->bVisible )
									{
										HeightInPixels += SubTrackHeight;
									}
								}
							}
						}
					}
				}
			}
		}

		// For non-director track windows, we add some additional height so that we have a small empty area beneath
		// the list of groups that the user can right click on to summon the pop up menu to add new groups with.
		if( !bIsDirectorTrackWindow )
		{
			HeightInPixels += GroupHeadHeight;
		}
	}

	return HeightInPixels;
}

/** 
 * Returns the height of the viewable group list content box in pixels
 *
 *  @param ViewportHeight The size of the viewport in pixels
 *
 *  @return The height of the viewable content box (may be zero!)
 */
int32 FMatineeViewportClient::ComputeGroupListBoxHeight( const int32 ViewportHeight ) const
{
	int32 HeightOfExtras = 0;

	if( bWantTimeline )
	{
		HeightOfExtras += TotalBarHeight;	// TimelineHeight + NavHeight
	}

	// Compute the height of the group list viewable area
	int32 GroupListHeight = ViewportHeight - HeightOfExtras;
	return FMath::Max( 0, GroupListHeight );
}


/** 
 * Draws a subtrack group label.
 *
 *  @param Canvas	Canvas to draw on
 *	@param Track	Track which owns this group
 *  @param InGroup	Group to draw
 * 	@param GroupIndex	Index of the group in the ParentTrack's sub group array
 *	@param LabelDrawParams	Parameters for how to draw the label
 */
void FMatineeViewportClient::DrawSubTrackGroup( FCanvas* Canvas, UInterpTrack* Track, const FSubTrackGroup& InGroup, int32 GroupIndex, const FInterpTrackLabelDrawParams& LabelDrawParams, UInterpGroup* Group )
{
	// Track title block on left.
	if( Canvas->IsHitTesting() ) 
	{
		Canvas->SetHitProxy( new HMatineeSubGroupTitle( Track, GroupIndex ) );
	}

	// Darken sub group labels
	FColor LabelColor = LabelDrawParams.GroupLabelColor;
	LabelColor.B -= 10;
	LabelColor.R -= 10;
	LabelColor.G -= 10;

	FCanvasTileItem TileItem( FVector2D::ZeroVector, FVector2D::ZeroVector, LabelColor );
	TileItem.BlendMode = SE_BLEND_Translucent;
	TileItem.Size = FVector2D( InterpEd->LabelWidth - MatineeGlobals::TreeLabelSeparatorOffset, TrackHeight - 1 );
	TileItem.Draw( Canvas, FVector2D( -InterpEd->LabelWidth + MatineeGlobals::TreeLabelSeparatorOffset, 0) );
	TileItem.SetColor( MatineeGlobals::TrackLabelAreaBackgroundColor );
	TileItem.Size = FVector2D( MatineeGlobals::TreeLabelSeparatorOffset, TrackHeight );
	TileItem.Draw( Canvas, FVector2D( -InterpEd->LabelWidth, 0 ) );

	FCanvasLineItem LineItem;
	LineItem.SetColor( FLinearColor::Black );
	LineItem.Draw( Canvas, FVector2D( -InterpEd->LabelWidth + MatineeGlobals::TreeLabelSeparatorOffset, 0 ), FVector2D( -InterpEd->LabelWidth + MatineeGlobals::TreeLabelSeparatorOffset, TrackHeight - 1 ) );

	// Highlight selected groups
	if( InGroup.bIsSelected )
	{
		// For the rectangle around the selection
		int32 MinX = -InterpEd->LabelWidth + 1;
		int32 MinY = 0;
		int32 MaxX = -1;
		int32 MaxY = TrackHeight - 1;

		TileItem.SetColor( MatineeGlobals::GroupOrTrackSelectedColor );
		TileItem.Size = FVector2D( InterpEd->LabelWidth, TrackHeight - 1 );
		TileItem.Draw( Canvas, FVector2D( -InterpEd->LabelWidth, 0 ) );
		FCanvasBoxItem BoxItem( FVector2D(MinX, MinY), FVector2D(MinX, MinY) - FVector2D(MaxX, MaxY) );
		BoxItem.SetColor( MatineeGlobals::GroupOrTrackSelectedBorder );
		BoxItem.Draw( Canvas );		
	}


	int32 IndentPixels = LabelDrawParams.IndentPixels;

	// Draw some 'tree view' lines to indicate the track is parented to a group
	{
		const float	HalfTrackHight = 0.5 * TrackHeight;
		const FLinearColor TreeNodeColor( 0.025f, 0.025f, 0.025f );
		const int32 TreeNodeLeftPos = -InterpEd->LabelWidth + IndentPixels + 6;
		const int32 TreeNodeTopPos = 2;
		const int32 TreeNodeRightPos = -InterpEd->LabelWidth + IndentPixels + MatineeGlobals::NumPixelsToIndentChildGroups;
		const int32 TreeNodeBottomPos = HalfTrackHight;

		LineItem.SetColor( TreeNodeColor );
		LineItem.Draw( Canvas, FVector2D( TreeNodeLeftPos, TreeNodeTopPos ), FVector2D( TreeNodeLeftPos, TreeNodeBottomPos ) );
		LineItem.Draw( Canvas, FVector2D( TreeNodeLeftPos, TreeNodeBottomPos ), FVector2D( TreeNodeRightPos, TreeNodeBottomPos ) );
	}

	const int32 TrackIconSize = 16;
	const int32 PaddedTrackIconSize = 20;
	int32 TrackTitleIndentPixels = MatineeGlobals::TrackTitleMargin + PaddedTrackIconSize + IndentPixels;

	// Draw Track Icon
	FString Text = InGroup.GroupName;
	// Truncate from front if name is too long
	int32 XL, YL;
	StringSize(GEditor->GetSmallFont(), XL, YL, *Text);

	// If too long to fit in label - truncate. TODO: Actually truncate by necessary amount!
	if(XL > InterpEd->LabelWidth - TrackTitleIndentPixels - 2)
	{
		Text = FString::Printf( TEXT("...%s"), *(Text.Right(13)) );
		StringSize(LabelFont, XL, YL, *Text);
	}

	// Ghost out disabled groups
	FLinearColor TextColor;
	if( Track->IsDisabled() == false )
	{
		TextColor = FLinearColor::White;
	}
	else
	{
		TextColor = FLinearColor(0.5f,0.5f,0.5f);
	}

	DrawLabel(Canvas, -InterpEd->LabelWidth + TrackTitleIndentPixels, FMath::TruncToFloat(0.5*TrackHeight - 0.5*YL), *Text, TextColor);
	if( Canvas->IsHitTesting() )
	{
		Canvas->SetHitProxy( NULL );
	}

	// Draw line under each track
	TileItem.SetColor( FLinearColor::Black );
	TileItem.Position = FVector2D( -InterpEd->LabelWidth, TrackHeight - 1 );
	TileItem.Size = FVector2D( LabelDrawParams.ViewX, 1 );
	TileItem.Draw( Canvas );

	Canvas->PushRelativeTransform( FTranslationMatrix( FVector(-InterpEd->LabelWidth, 0, 0 ) ) );
	const int32 HalfColArrowSize = 6;
	IndentPixels += MatineeGlobals::NumPixelsToIndentChildGroups;
	// Draw little collapse widget.
	FIntPoint A,B,C;
	if( InGroup.bIsCollapsed )
	{
		int32 HorizOffset = IndentPixels - 4;	// Extra negative offset for centering
		int32 VertOffset = 0.5 * GroupHeadHeight;
		A = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset - HalfColArrowSize);
		B = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset + HalfColArrowSize);
		C = FIntPoint(HorizOffset + 2*HalfColArrowSize, VertOffset );
	}
	else
	{
		int32 HorizOffset = IndentPixels;
		int32 VertOffset = 0.5 * GroupHeadHeight - 3; // Extra negative offset for centering
		A = FIntPoint(HorizOffset, VertOffset);
		B = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset + HalfColArrowSize);
		C = FIntPoint(HorizOffset + 2*HalfColArrowSize, VertOffset);
	}

	FCanvasTriangleItem TriItem( A, B, C, GWhiteTexture );
	TriItem.SetColor( FLinearColor::Black );
	Canvas->DrawItem( TriItem );

	// Invisible hit test geometry for the collapse/expand widget
	if( Canvas->IsHitTesting() ) 
	{
		Canvas->SetHitProxy( new HMatineeTrackCollapseBtn( Track, GroupIndex ) );
	}

	TileItem.SetColor( FLinearColor( 0.0f, 0.0f, 0.0f, 0.01f ) );
	TileItem.Position = FVector2D( IndentPixels, 0.5 * GroupHeadHeight - HalfColArrowSize );
	TileItem.Size = FVector2D( 2 * HalfColArrowSize, 2 * HalfColArrowSize );
	TileItem.Draw( Canvas );
	if( Canvas->IsHitTesting() ) 
	{
		Canvas->SetHitProxy( NULL );
	};

	Canvas->PopTransform();

	CreatePushPropertiesOntoGraphButton( Canvas, Track, Group, GroupIndex, LabelDrawParams, true );
}

/** 
 * Populates a list of drawing information for a specified sub track group.  
 * This is so all of the keyframes in all of the tracks in the group can be drawn directly on the group
 *
 * @param InterpEd			The interp editor which contains information for drawing
 * @param SubGroupOwner		The track which owns the sub group
 * @param InSubTrackGroup	The track to get drawing information for
 * @param KeySize			The size of each keyframe
 * @param OutDrawInfo		Array of drawing information that was created
 */
static void GetSubTrackGroupKeyDrawInfos( FMatinee& InterpEd, UInterpTrack* SubGroupOwner, const FSubTrackGroup& InSubTrackGroup, const FVector2D& KeySize, TArray<FKeyframeDrawInfo>& OutDrawInfos )
{
	UInterpGroup* TrackGroup = SubGroupOwner->GetOwningGroup();
	for( int32 TrackIndex = 0; TrackIndex < InSubTrackGroup.TrackIndices.Num(); ++TrackIndex )
	{
		// For each track in the subgroup create a drawing information for each keyframe in each track.
		UInterpTrack* Track = SubGroupOwner->SubTracks[ InSubTrackGroup.TrackIndices[ TrackIndex ] ];
		for(int32 KeyframeIdx=0; KeyframeIdx<Track->GetNumKeyframes(); KeyframeIdx++)
		{
			// Create a new draw info
			FKeyframeDrawInfo DrawInfo;		
			DrawInfo.KeyTime = Track->GetKeyframeTime(KeyframeIdx);
			// Check to see if a keyframe at this position is already being drawn
			// We will not draw keyframes at the same location more than once
			int32 ExistingInfoIndex = OutDrawInfos.Find( DrawInfo );
			if( ExistingInfoIndex == INDEX_NONE )
			{
				// This is a new keyframe that has not been found yet
				DrawInfo.KeyColor = Track->GetKeyframeColor(KeyframeIdx);
				FVector2D TickPos;

				DrawInfo.KeyPos.X =  -KeySize.X / 2.0f + DrawInfo.KeyTime * InterpEd.PixelsPerSec;
				DrawInfo.KeyPos.Y =  (GroupHeadHeight - 1 - KeySize.Y) / 2.0f;

				// Is the keyframe selected
				DrawInfo.bSelected = InterpEd.KeyIsInSelection( TrackGroup, Track, KeyframeIdx );
				OutDrawInfos.Add( DrawInfo );
			}
			else
			{
				// A keyframe at this time is already being drawn, determine if it should be selected.  Group keyframes should only be selected if all tracks with a keyframe at that time are selected
				FKeyframeDrawInfo& ExistingInfo = OutDrawInfos[ExistingInfoIndex];
				ExistingInfo.bSelected = ExistingInfo.bSelected && InterpEd.KeyIsInSelection( TrackGroup, Track, KeyframeIdx );
			}

		}
	}
}

/** 
 * Draws a track in the interp editor
 * @param Canvas		Canvas to draw on
 * @param Track			Track to draw
 * @param Group			Group containing the track to draw
 * @param TrackDrawParams	Params for drawing the track
 * @params LabelDrawParams	Params for drawing the track label
 */
int32 FMatineeViewportClient::DrawTrack( FCanvas* Canvas, UInterpTrack* Track, UInterpGroup* Group, const FInterpTrackDrawParams& TrackDrawParams, const FInterpTrackLabelDrawParams& LabelDrawParams )
{
	bool bIsSubtrack = Track->GetOuter()->IsA( UInterpTrack::StaticClass() );
	// If we are drawing a subtrack, use the subtracks height and not the track height
	int32 TrackHeightToUse = bIsSubtrack ? SubTrackHeight : TrackHeight; 
	int32 TotalHeightAdded = 0;
	
	bool bIsTrackWithinScrollArea =
		( LabelDrawParams.YOffset + TrackHeightToUse >= 0 ) && ( LabelDrawParams.YOffset <= LabelDrawParams.TrackAreaHeight );

	if( bIsTrackWithinScrollArea )
	{
		if( Canvas->IsHitTesting() )
		{
			Canvas->SetHitProxy( new HMatineeTrackTimeline(Group, Track) );
		}
		Canvas->DrawTile(  -InterpEd->LabelWidth + MatineeGlobals::TreeLabelSeparatorOffset, 0, LabelDrawParams.ViewX - MatineeGlobals::TreeLabelSeparatorOffset, TrackHeight - 1, 0.f, 0.f, 1.f, 1.f, FLinearColor( 0.0f, 0.0f, 0.0f, 0.01f ) );
		if( Canvas->IsHitTesting() ) 
		{
			Canvas->SetHitProxy( NULL );
		}

		Track->DrawTrack( Canvas, Group, TrackDrawParams );

		// If the track is in the visible scroll area, draw the tracks label
		DrawTrackLabel( Canvas, Track, Group, TrackDrawParams, LabelDrawParams );
	}

	TotalHeightAdded += TrackHeightToUse;

	// A list of keyframes to draw on track sub groups
	TArray< TArray<FKeyframeDrawInfo> > GroupDrawInfos;

	// Draw subtracks
	int32 IndentPixels = LabelDrawParams.IndentPixels;
	if( Track->SubTracks.Num() > 0 )
	{
		// Track has subtracks, indent all subtracks
		IndentPixels += MatineeGlobals::NumPixelsToIndentChildGroups;

		// Get all sub track keyframe drawing information
		const FVector2D KeySize(3.0f, GroupHeadHeight * 0.5f);
		for( int32 GroupIndex = 0; GroupIndex < Track->SubTrackGroups.Num(); ++GroupIndex )
		{
			FSubTrackGroup& SubGroup = Track->SubTrackGroups[ GroupIndex ];
			GroupDrawInfos.Add( TArray<FKeyframeDrawInfo>() );
			GetSubTrackGroupKeyDrawInfos( *InterpEd, Track, SubGroup, KeySize, GroupDrawInfos[GroupIndex] );
		}

		// Draw subtracks if the track is not collapsed
		if( !Track->bIsCollapsed )
		{
			FInterpTrackLabelDrawParams SubLabelDrawParams = LabelDrawParams;
			FInterpTrackDrawParams SubTrackDrawParams = TrackDrawParams;

			// Draw subtracks based on grouping if there are any subgroups
			if( Track->SubTrackGroups.Num() > 0 )
			{
				IndentPixels += MatineeGlobals::NumPixelsToIndentChildGroups;
				for( int32 GroupIndex = 0; GroupIndex < Track->SubTrackGroups.Num(); ++GroupIndex )
				{
					FSubTrackGroup& SubGroup = Track->SubTrackGroups[ GroupIndex ];
					SubLabelDrawParams.IndentPixels = IndentPixels;
					SubLabelDrawParams.YOffset = TotalHeightAdded;

					// Determine if all tracks in a group are selected
					bool bAllSubTracksSelected = true;
			
					for( int32 TrackIndex  = 0; TrackIndex < SubGroup.TrackIndices.Num(); ++TrackIndex )
					{
						if( !Track->SubTracks[ SubGroup.TrackIndices[ TrackIndex ] ]->IsSelected() )
						{
							bAllSubTracksSelected = false;
							break;
						}
					}
					SubGroup.bIsSelected = bAllSubTracksSelected;

					bool bIsGroupWithinScrollArea = ( SubLabelDrawParams.YOffset + TrackHeightToUse >= 0 ) && ( SubLabelDrawParams.YOffset <= LabelDrawParams.TrackAreaHeight );

					if( bIsGroupWithinScrollArea )
					{
						// Draw the group if it should be visible
						Canvas->PushRelativeTransform(FTranslationMatrix(FVector(0,SubLabelDrawParams.YOffset,0)));
						DrawSubTrackGroup( Canvas, Track, SubGroup, GroupIndex, SubLabelDrawParams, Group );
						Canvas->PopTransform();

						// Draw keys on the group
						FVector2D TrackPos(InterpEd->LabelWidth - InterpEd->ViewStartTime * InterpEd->PixelsPerSec, SubLabelDrawParams.YOffset );
						Canvas->PushRelativeTransform(FTranslationMatrix(FVector(TrackPos.X-InterpEd->LabelWidth,TrackPos.Y,0)));
						DrawSubTrackGroupKeys( Canvas, Track, GroupIndex, GroupDrawInfos[GroupIndex], TrackPos, KeySize );
						Canvas->PopTransform();
					}

					// Further indent subtracks which are grouped
					IndentPixels += MatineeGlobals::NumPixelsToIndentChildGroups;
					TotalHeightAdded += TrackHeight;

					if( !SubGroup.bIsCollapsed )
					{
						// Draw each track.  This part is recursive.
						for( int32 TrackIndex  = 0; TrackIndex < SubGroup.TrackIndices.Num(); ++TrackIndex )
						{
							UInterpTrack* SubTrack = Track->SubTracks[ SubGroup.TrackIndices[ TrackIndex ] ];
							SubTrackDrawParams.TrackHeight = SubTrackHeight;
							SubLabelDrawParams.IndentPixels = IndentPixels;
							SubLabelDrawParams.bTrackSelected = SubTrack->IsSelected();
							
							Canvas->PushRelativeTransform(FTranslationMatrix(FVector(0,TotalHeightAdded,0)));
							TotalHeightAdded += DrawTrack( Canvas, SubTrack, Group, SubTrackDrawParams, SubLabelDrawParams );
							Canvas->PopTransform();
						}
					}
		
					IndentPixels -= MatineeGlobals::NumPixelsToIndentChildGroups;
				}
			}
			else
			{
				// The track has no sub groups, just draw each subtrack directly.
				SubLabelDrawParams.IndentPixels = IndentPixels;
				for( int32 SubTrackIndex = 0; SubTrackIndex < Track->SubTracks.Num(); ++SubTrackIndex )
				{
					UInterpTrack* SubTrack = Track->SubTracks[ SubTrackIndex ];
					SubTrackDrawParams.TrackHeight = SubTrackHeight;
					SubLabelDrawParams.bTrackSelected = SubTrack->IsSelected();

					Canvas->PushRelativeTransform(FTranslationMatrix(FVector(0,TotalHeightAdded,0)));
					TotalHeightAdded += DrawTrack( Canvas, SubTrack, Group, SubTrackDrawParams, SubLabelDrawParams );
					Canvas->PopTransform();
				}
			}
		}

		if( bIsTrackWithinScrollArea && GroupDrawInfos.Num() > 0 )
		{
			// Draw keys on the parent track which correspond to all keyframes in subtracks
			const FVector2D TickSize(3.0f, GroupHeadHeight * 0.5f);
			FVector2D TrackPos(InterpEd->LabelWidth - InterpEd->ViewStartTime * InterpEd->PixelsPerSec, LabelDrawParams.YOffset );
			Canvas->PushAbsoluteTransform(FTranslationMatrix(FVector(TrackPos.X,TrackPos.Y,0)));
		
			for( int32 GroupIndex = 0; GroupIndex < Track->SubTrackGroups.Num(); ++GroupIndex )
			{
				FSubTrackGroup& SubGroup = Track->SubTrackGroups[ GroupIndex ];
				DrawSubTrackGroupKeys( Canvas, Track, INDEX_NONE, GroupDrawInfos[GroupIndex], TrackPos, KeySize );
			}
 
			Canvas->PopTransform();
		}
	}

	return TotalHeightAdded;
}

void FMatineeViewportClient::CreatePushPropertiesOntoGraphButton( FCanvas* Canvas, UInterpTrack* Track, UInterpGroup* InGroup, int32 GroupIndex, const FInterpTrackLabelDrawParams& LabelDrawParams, bool bIsSubTrack )
{
	int32 TrackHeightToUse = bIsSubTrack ? SubTrackHeight : TrackHeight;

	if(
		Track->IsA(UInterpTrackFloatBase::StaticClass()) ||
		Track->IsA(UInterpTrackVectorBase::StaticClass()) || 
		Track->IsA(UInterpTrackMove::StaticClass()) || 
		Track->IsA(UInterpTrackLinearColorBase::StaticClass())
	)
	{
		UTexture2D* GraphTex = NULL;
		if( Track->SubTracks.Num() )
		{
			bool bSubtracksInCurveEd = false;
			// see if any subtracks are in the curve editor
			if( GroupIndex == -1 )
			{
				for( int32 SubTrackIndex = 0; SubTrackIndex < Track->SubTracks.Num(); ++SubTrackIndex )
				{
					if( InterpEd->IData->CurveEdSetup->ShowingCurve( Track->SubTracks[SubTrackIndex] ) )
					{
						bSubtracksInCurveEd = true;
						break;
					}
				}
			}
			else
			{
				FSubTrackGroup& Group = Track->SubTrackGroups[ GroupIndex ];
				for( int32 Index = 0; Index < Group.TrackIndices.Num(); ++Index )
				{
					if( InterpEd->IData->CurveEdSetup->ShowingCurve( Track->SubTracks[ Group.TrackIndices[ Index ] ] ) )
					{
						bSubtracksInCurveEd = true;
						break;
					}
				}

			}
			GraphTex = bSubtracksInCurveEd ? LabelDrawParams.GraphOnTex : LabelDrawParams.GraphOffTex;
		}
		else
		{
			GraphTex = InterpEd->IData->CurveEdSetup->ShowingCurve(Track) ? LabelDrawParams.GraphOnTex : LabelDrawParams.GraphOffTex;
		}

		// Draw button for pushing properties onto graph view.
		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HMatineeTrackGraphPropBtn( InGroup, GroupIndex, Track ) );
		Canvas->DrawTile( -14, TrackHeightToUse-11, 8, 8, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, GraphTex->Resource );
		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
	}
}

/** 
 * Draws a track label for a track
 * @param Canvas		Canvas to draw on
 * @param Track			Track that needs a label drawn for it.
 * @param Group			Group containing the track to draw
 * @param TrackDrawParams	Params for drawing the track
 * @params LabelDrawParams	Params for drawing the track label
 */
void FMatineeViewportClient::DrawTrackLabel( FCanvas* Canvas, UInterpTrack* Track, UInterpGroup* Group, const FInterpTrackDrawParams& TrackDrawParams, const FInterpTrackLabelDrawParams& LabelDrawParams )
{
	const int32 TrackIndex = TrackDrawParams.TrackIndex;

	const bool bIsSubTrack = Track->GetOuter()->IsA( UInterpTrack::StaticClass() );
	int32 TrackHeightToUse = bIsSubTrack ? SubTrackHeight : TrackHeight;

	// The track color will simply be a brighter copy of the group color.  We do this so that the colors will match.
	FColor TrackLabelColor = LabelDrawParams.GroupLabelColor;
	TrackLabelColor += FColor( 40, 40, 40 );

	// Track title block on left.
	if( Canvas->IsHitTesting() ) 
	{
		Canvas->SetHitProxy( new HMatineeTrackTitle( Group, Track ) );
	}

	FCanvasTileItem TileItem( FVector2D::ZeroVector, FVector2D::ZeroVector, TrackLabelColor );
	TileItem.BlendMode = SE_BLEND_Translucent;
	FCanvasLineItem LineItem;

	TileItem.Size = FVector2D( InterpEd->LabelWidth - MatineeGlobals::TreeLabelSeparatorOffset, TrackHeightToUse - 1 );
	TileItem.Draw( Canvas, FVector2D( -InterpEd->LabelWidth + MatineeGlobals::TreeLabelSeparatorOffset, 0 ) );

	TileItem.SetColor( MatineeGlobals::TrackLabelAreaBackgroundColor );
	TileItem.Size = FVector2D( MatineeGlobals::TreeLabelSeparatorOffset, TrackHeightToUse );
	TileItem.Draw( Canvas, FVector2D( -InterpEd->LabelWidth, 0 ) );

	LineItem.SetColor( FLinearColor::Black );
	LineItem.Draw( Canvas, FVector2D( -InterpEd->LabelWidth + MatineeGlobals::TreeLabelSeparatorOffset, 0 ), FVector2D( -InterpEd->LabelWidth + MatineeGlobals::TreeLabelSeparatorOffset, TrackHeightToUse - 1 ) );
	
	if( LabelDrawParams.bTrackSelected )
	{
		// Also, we'll draw a rectangle around the selection
		int32 MinX = -InterpEd->LabelWidth + 1;
		int32 MinY = 0;
		int32 MaxX = -1;
		int32 MaxY = TrackHeightToUse - 1;

		TileItem.SetColor( MatineeGlobals::GroupOrTrackSelectedColor );
		TileItem.Size = FVector2D( InterpEd->LabelWidth, TrackHeightToUse - 1 );
		TileItem.Draw( Canvas, FVector2D( -InterpEd->LabelWidth, 0 ) );

		FCanvasBoxItem BoxItem( FVector2D(MinX, MinY), FVector2D(MinX, MinY) - FVector2D(MaxX, MaxY) );
		BoxItem.SetColor( MatineeGlobals::GroupOrTrackSelectedBorder );
		BoxItem.Draw( Canvas );		
	}


	int32 IndentPixels = LabelDrawParams.IndentPixels;

	// Draw some 'tree view' lines to indicate the track is parented to a group
	{
		const float	HalfTrackHight = 0.5 * TrackHeightToUse;
		const FLinearColor TreeNodeColor( 0.025f, 0.025f, 0.025f );
		const int32 TreeNodeLeftPos = -InterpEd->LabelWidth + IndentPixels + 6;
		const int32 TreeNodeTopPos = 2;
		const int32 TreeNodeRightPos = -InterpEd->LabelWidth + IndentPixels + (Track->SubTracks.Num() > 0 ? MatineeGlobals::NumPixelsToIndentChildGroups : MatineeGlobals::NumPixelsToIndentChildGroups*2);
		const int32 TreeNodeBottomPos = HalfTrackHight;

		LineItem.SetColor( TreeNodeColor );
		LineItem.Draw( Canvas, FVector2D( TreeNodeLeftPos, TreeNodeTopPos ), FVector2D( TreeNodeLeftPos, TreeNodeBottomPos ) );
		LineItem.Draw( Canvas, FVector2D( TreeNodeLeftPos, TreeNodeBottomPos ), FVector2D( TreeNodeRightPos, TreeNodeBottomPos ) );	
	}

	if( !bIsSubTrack )
	{
		IndentPixels += MatineeGlobals::NumPixelsToIndentChildGroups;
	}


	const int32 TrackIconSize = 16;
	const int32 PaddedTrackIconSize = 20;
	int32 TrackTitleIndentPixels = MatineeGlobals::TrackTitleMargin + PaddedTrackIconSize + IndentPixels;

	// Draw Track Icon
	UTexture2D* TrackIconTex = Track->GetTrackIcon();
	if( TrackIconTex )
	{
		TileItem.SetColor( FLinearColor::White );
		TileItem.Texture = TrackIconTex->Resource;
		TileItem.Size = FVector2D( TrackIconSize, TrackIconSize );
		TileItem.Draw( Canvas, FVector2D( -InterpEd->LabelWidth + TrackTitleIndentPixels - PaddedTrackIconSize, 0.5*(TrackHeightToUse - TrackIconSize) ) );		
		TileItem.Texture = GWhiteTexture;
	}

	// Truncate from front if name is too long
	FString TrackTitle = FString( *Track->TrackTitle );
	int32 XL, YL;
	StringSize(GEditor->GetSmallFont(), XL, YL, *TrackTitle);

	if(XL > InterpEd->LabelWidth - TrackTitleIndentPixels - 2)
	{
		TrackTitle = FString::Printf( TEXT("...%s"), *(TrackTitle.Right(13)) );
		StringSize(LabelFont, XL, YL, *TrackTitle);
	}

	FLinearColor TextColor;

	if(Track->IsDisabled() == false)
	{
		TextColor = FLinearColor::White;
	}
	else
	{
		TextColor = FLinearColor(0.5f,0.5f,0.5f);
	}

	DrawLabel(Canvas, -InterpEd->LabelWidth + TrackTitleIndentPixels, 0.5*TrackHeightToUse - 0.5*YL, *TrackTitle, TextColor);
	if( Canvas->IsHitTesting() )
	{
		Canvas->SetHitProxy( NULL );
	}

	UInterpTrackEvent* EventTrack = Cast<UInterpTrackEvent>(Track);
	if(EventTrack)
	{
		UTexture2D* ForwardTex = (EventTrack->bFireEventsWhenForwards) ? LabelDrawParams.ForwardEventOnTex : LabelDrawParams.ForwardEventOffTex;
		UTexture2D* BackwardTex = (EventTrack->bFireEventsWhenBackwards) ? LabelDrawParams.BackwardEventOnTex : LabelDrawParams.BackwardEventOffTex;

		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HMatineeEventDirBtn(Group, TrackIndex, EMatineeEventDirection::IED_Backward) );
		Canvas->DrawTile( -24, TrackHeightToUse-11, 8, 8, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, BackwardTex->Resource );
		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );

		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HMatineeEventDirBtn(Group, TrackIndex, EMatineeEventDirection::IED_Forward) );
		Canvas->DrawTile( -14, TrackHeightToUse-11, 8, 8, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, ForwardTex->Resource );
		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
	}

	// For Movement tracks, draw a button that toggles display of the 3D trajectory for this track
	if( Track->IsA( UInterpTrackMove::StaticClass() ) )
	{
		UInterpTrackMove* MovementTrack = CastChecked<UInterpTrackMove>( Track );
		UTexture2D* TrajectoryButtonTex = MovementTrack->bHide3DTrack ? LabelDrawParams.GraphOffTex : LabelDrawParams.TrajectoryOnTex;

		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HMatineeTrackTrajectoryButton( Group, Track ) );
		if(MovementTrack->bHide3DTrack)
		{
			Canvas->DrawTile( -24, TrackHeightToUse-11, 8, 8, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, TrajectoryButtonTex->Resource );
		}
		else
		{
			Canvas->DrawTile( -24, TrackHeightToUse-11, 8, 8, 0.f, 0.f, 1.f, 1.f, FLinearColor( 0.0f, 1.0f, 0.0f ) );
		}
		if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
	}

	CreatePushPropertiesOntoGraphButton( Canvas, Track, Group, -1, LabelDrawParams, bIsSubTrack );

	// Draw line under each track
	Canvas->DrawTile( -InterpEd->LabelWidth, TrackHeightToUse - 1, LabelDrawParams.ViewX, 1, 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );

	if( !bIsSubTrack )
	{
		// Draw an icon to let the user enable/disable a track.
		if( Canvas->IsHitTesting() ) 
		{
			Canvas->SetHitProxy( new HMatineeTrackDisableTrackBtn( Group, Track ) );
		}

		float YPos = (TrackHeightToUse - MatineeGlobals::DisableTrackIconSize.Y) / 2.0f;
		Canvas->DrawTile( -InterpEd->LabelWidth + MatineeGlobals::DisableTrackCheckBoxHorizOffset, YPos, MatineeGlobals::DisableTrackIconSize.X, MatineeGlobals::DisableTrackIconSize.Y, 0,0,1,1, FLinearColor::Black);

		if( Track->IsDisabled() == false )
		{
			Canvas->DrawTile( -InterpEd->LabelWidth + MatineeGlobals::DisableTrackCheckBoxHorizOffset, YPos, MatineeGlobals::DisableTrackIconSize.X, MatineeGlobals::DisableTrackIconSize.Y, 0,0,1,1, FLinearColor::White, LabelDrawParams.DisableTrackTex->Resource);
		}

		if( Canvas->IsHitTesting() )
		{
			Canvas->SetHitProxy( NULL );
		}
	}

	// If the track has subtracks, draw a collapse widget to collapse the track
	if( Track->SubTracks.Num() > 0 )
	{
		Canvas->PushRelativeTransform( FTranslationMatrix( FVector(-InterpEd->LabelWidth, 0, 0 ) ) );
		const int32 HalfColArrowSize = 6;

		FIntPoint A,B,C;
		if( Track->bIsCollapsed )
		{
			int32 HorizOffset = IndentPixels - 4;	// Extra negative offset for centering
			int32 VertOffset = 0.5 * GroupHeadHeight;
			A = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset - HalfColArrowSize);
			B = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset + HalfColArrowSize);
			C = FIntPoint(HorizOffset + 2*HalfColArrowSize, VertOffset );
		}
		else
		{
			int32 HorizOffset = IndentPixels;
			int32 VertOffset = 0.5 * GroupHeadHeight - 3; // Extra negative offset for centering
			A = FIntPoint(HorizOffset, VertOffset);
			B = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset + HalfColArrowSize);
			C = FIntPoint(HorizOffset + 2*HalfColArrowSize, VertOffset);
		}

		FCanvasTriangleItem TriItem( A, B, C, GWhiteTexture );
		TriItem.SetColor( FLinearColor::Black );
		Canvas->DrawItem( TriItem );
		
		// Invisible hit test geometry for the collapse/expand widget
		if( Canvas->IsHitTesting() ) 
		{
			Canvas->SetHitProxy( new HMatineeTrackCollapseBtn( Track ) );
		}
		TileItem.SetColor( FLinearColor( 0.0f, 0.0f, 0.0f, 0.01f ) );
		TileItem.Size = FVector2D( 2 * HalfColArrowSize, 2 * HalfColArrowSize );
		TileItem.Draw( Canvas, FVector2D( IndentPixels, 0.5 * GroupHeadHeight - HalfColArrowSize ) );

		if( Canvas->IsHitTesting() ) 
		{
			Canvas->SetHitProxy( NULL );
		};

		Canvas->PopTransform();
	}
}

/** Draw the track editor using the supplied 2D RenderInterface. */
void FMatineeViewportClient::Draw(FViewport* InViewport, FCanvas* Canvas)
{
	if (!ParentTab.IsValid())
	{
		//Don't draw if our parent has closed.
		return;
	}

	Canvas->PushAbsoluteTransform(FMatrix::Identity);

	// Erase background
	Canvas->Clear( FColor(162,162,162) );

	const int32 ViewX = InViewport->GetSizeXY().X;
	const int32 ViewY = InViewport->GetSizeXY().Y;

	// @todo frick: Weird to compute this here and storing it in parent
	InterpEd->TrackViewSizeX = ViewX - InterpEd->LabelWidth;

	// Calculate ratio between screen pixels and elapsed time.
	// @todo frick: Weird to compute this here and storing it in parent
	InterpEd->PixelsPerSec = FMath::Max( 1.0f, float(ViewX - InterpEd->LabelWidth)/float(InterpEd->ViewEndTime - InterpEd->ViewStartTime) );
	InterpEd->NavPixelsPerSecond = FMath::Max( 0.0f, float(ViewX - InterpEd->LabelWidth)/InterpEd->IData->InterpLength );

	DrawGrid(InViewport, Canvas, false);

	FCanvasTileItem TileItem( FVector2D::ZeroVector, FVector2D::ZeroVector, NullRegionColor );
	TileItem.BlendMode = SE_BLEND_Translucent;
	FCanvasLineItem LineItem;

	// Draw 'null regions' if viewing past start or end.
	int32 StartPosX = InterpEd->LabelWidth + (0.f - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
	int32 EndPosX = InterpEd->LabelWidth + (InterpEd->IData->InterpLength - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
	TileItem.SetColor( NullRegionColor );
	if(InterpEd->ViewStartTime < 0.f)
	{
		TileItem.Size = FVector2D(StartPosX, ViewY);
		TileItem.Draw( Canvas, FVector2D::ZeroVector );
	}

	if(InterpEd->ViewEndTime > InterpEd->IData->InterpLength)
	{
		TileItem.Size = FVector2D(ViewX-EndPosX, ViewY);
		TileItem.Draw( Canvas, FVector2D(EndPosX, 0.0f) );
	}

	// Draw lines on borders of 'null area'
	int32 TrackAreaHeight = ViewY;
	if( bWantTimeline )
	{
		TrackAreaHeight -= TotalBarHeight;
	}
	LineItem.SetColor( NullRegionBorderColor );
	if(InterpEd->ViewStartTime < 0.f)
	{
		LineItem.Draw( Canvas, FVector2D(StartPosX, 0), FVector2D(StartPosX, TrackAreaHeight) );
	}

	if(InterpEd->ViewEndTime > InterpEd->IData->InterpLength)
	{
		LineItem.Draw( Canvas, FVector2D(EndPosX, 0), FVector2D(EndPosX, TrackAreaHeight) );
	}

	// Draw loop region.	
	int32 EdStartPosX = InterpEd->LabelWidth + (InterpEd->IData->EdSectionStart - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
	int32 EdEndPosX = InterpEd->LabelWidth + (InterpEd->IData->EdSectionEnd - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
	TileItem.SetColor( FLinearColor( InterpEd->RegionFillColor ) );
	TileItem.Size = FVector2D( EdEndPosX - EdStartPosX, TrackAreaHeight );
	TileItem.Draw( Canvas, FVector2D( EdStartPosX, 0 ) );

	// Draw titles block down left.
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HMatineeTrackBkg() );
	TileItem.SetColor( MatineeGlobals::TrackLabelAreaBackgroundColor );
	TileItem.Size = FVector2D( InterpEd->LabelWidth, TrackAreaHeight );
	TileItem.Draw( Canvas, FVector2D::ZeroVector );
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );

	FInterpTrackLabelDrawParams LabelDrawParams;

	LabelDrawParams.ViewX = ViewX;
	LabelDrawParams.ViewY = ViewY;
	
	// Get textures for cam-locked icon.
	LabelDrawParams.CamLockedIcon = CamLockedIcon;
	check(LabelDrawParams.CamLockedIcon);

	LabelDrawParams.CamUnlockedIcon = CamUnlockedIcon;
	check(LabelDrawParams.CamUnlockedIcon);

	// Get textures for Event direction buttons.
	LabelDrawParams.ForwardEventOnTex = ForwardEventOnTex;
	check(LabelDrawParams.ForwardEventOnTex);

	LabelDrawParams.ForwardEventOffTex = ForwardEventOffTex;
	check(LabelDrawParams.ForwardEventOffTex);

	LabelDrawParams.BackwardEventOnTex = BackwardEventOnTex;
	check(LabelDrawParams.BackwardEventOnTex);

	LabelDrawParams.BackwardEventOffTex = BackwardEventOffTex;
	check(LabelDrawParams.BackwardEventOffTex);

	LabelDrawParams.DisableTrackTex = DisableTrackTex;
	check(LabelDrawParams.DisableTrackTex);


	// Get textures for sending to curve editor
	LabelDrawParams.GraphOnTex = GraphOnTex;
	check(LabelDrawParams.GraphOnTex);

	LabelDrawParams.GraphOffTex = GraphOffTex;
	check(LabelDrawParams.GraphOffTex);

	
	// Get textures for toggle trajectories
	LabelDrawParams.TrajectoryOnTex = TrajectoryOnTex;
	check( LabelDrawParams.TrajectoryOnTex != NULL );

	// Check to see if we have a director group.  If so, we'll want to draw it on top of the other items!
	int32 DirGroupIndex = 0;
	bool bHaveDirGroup = InterpEd->FindDirectorGroup( DirGroupIndex ); // Out

	// Compute vertical start offset
	int32 StartYOffset = ThumbPos_Vert;
	int32 YOffset = StartYOffset;

	
	// Setup draw params which will be passed to the track rendering function for every visible track.  We'll
	// make additional changes to this after each track is rendered.
	FInterpTrackDrawParams TrackDrawParams;
	TrackDrawParams.TrackIndex = INDEX_NONE;
	TrackDrawParams.TrackWidth = ViewX - InterpEd->LabelWidth;
	TrackDrawParams.TrackHeight = TrackHeight - 1;
	TrackDrawParams.StartTime = InterpEd->ViewStartTime;
	TrackDrawParams.PixelsPerSec = InterpEd->PixelsPerSec;
	TrackDrawParams.TimeCursorPosition = InterpEd->MatineeActor->InterpPosition;
	TrackDrawParams.SnapAmount = InterpEd->SnapAmount;
	TrackDrawParams.bPreferFrameNumbers = InterpEd->bSnapToFrames && InterpEd->bPreferFrameNumbers;
	TrackDrawParams.bShowTimeCursorPosForAllKeys = InterpEd->bShowTimeCursorPosForAllKeys;
	TrackDrawParams.bAllowKeyframeBarSelection = InterpEd->IsKeyframeBarSelectionAllowed();
	TrackDrawParams.bAllowKeyframeTextSelection = InterpEd->IsKeyframeTextSelectionAllowed();
	TrackDrawParams.SelectedKeys = InterpEd->Opt->SelectedKeys;


	UInterpGroup* CurParentGroup = NULL;

	// Draw visible groups/tracks
	for( int32 CurGroupIndex = 0; CurGroupIndex < InterpEd->IData->InterpGroups.Num(); ++CurGroupIndex )
	{
		// Draw group header
		UInterpGroup* Group = InterpEd->IData->InterpGroups[ CurGroupIndex ];

		bool bIsGroupVisible = Group->bVisible;
		if( Group->bIsParented )
		{
			// If we're parented then we're only visible if our parent group is not collapsed
			check( CurParentGroup != NULL );
			if( CurParentGroup->bCollapsed )
			{
				// Parent group is collapsed, so we should not be rendered
				bIsGroupVisible = false;
			}
		}
		else
		{
			// If this group is not parented, then we clear our current parent
			CurParentGroup = NULL;
		}

		// If this is a director group and the current window is not a director track window, then we'll skip over
		// the director group.  Similarly, for director track windows we'll skip over all non-director groups.
		bool bIsGroupAppropriateForWindow =
		  ( Group->IsA( UInterpGroupDirector::StaticClass() ) == bIsDirectorTrackWindow );

		// Only draw if the group isn't filtered and isn't culled
		if( bIsGroupVisible && bIsGroupAppropriateForWindow )
		{
			// If this is a child group then we'll want to indent everything a little bit
			int32 IndentPixels = MatineeGlobals::TreeLabelsMargin;  // Also extend past the 'track enabled' check box column
			if( Group->bIsParented )
			{
				IndentPixels += MatineeGlobals::NumPixelsToIndentChildGroups;
			}


			// Does the group have an actor associated with it?
			AActor* GroupActor = NULL;
			{
				// @todo Performance: Slow to do a linear search here in the middle of our draw call
				UInterpGroupInst* GrInst = InterpEd->MatineeActor->FindFirstGroupInst(Group);
				if( GrInst )
				{
					GroupActor = GrInst->GroupActor;
				}
			}

			// Select color for group label
			FColor GroupLabelColor = ChooseLabelColorForGroupActor( Group, GroupActor );
			
			// Check to see if we're out of view (scrolled away).  If so, then we don't need to draw!
			bool bIsGroupWithinScrollArea =
				( YOffset + GroupHeadHeight >= 0 ) && ( YOffset <= TrackAreaHeight );
			if( bIsGroupWithinScrollArea )
			{
				Canvas->PushRelativeTransform(FTranslationMatrix(FVector(0,YOffset,0)));

				if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HMatineeGroupTitle(Group) );
				int32 MinTitleX = Group->bIsFolder ? 0 : MatineeGlobals::TreeLabelSeparatorOffset;
				TileItem.SetColor( GroupLabelColor );
				TileItem.Size = FVector2D( ViewX - MinTitleX, GroupHeadHeight );
				TileItem.Draw( Canvas, FVector2D( MinTitleX, 0.0f ) );
				
				if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
				if( !Group->bIsFolder )
				{
					TileItem.SetColor( MatineeGlobals::TrackLabelAreaBackgroundColor );
					TileItem.Size = FVector2D( MatineeGlobals::TreeLabelSeparatorOffset, GroupHeadHeight );
					TileItem.Draw( Canvas, FVector2D::ZeroVector );
					LineItem.SetColor( FLinearColor::Black );
					LineItem.Draw( Canvas, FVector2D( MatineeGlobals::TreeLabelSeparatorOffset, 0 ), FVector2D( MatineeGlobals::TreeLabelSeparatorOffset, GroupHeadHeight - 1 ) );
				}

				// Select color for group label
				if( InterpEd->IsGroupSelected(Group) )
				{
					FColor GroupColor = MatineeGlobals::GroupOrTrackSelectedColor;
					FColor GroupBorder = MatineeGlobals::GroupOrTrackSelectedBorder;

					if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HMatineeGroupTitle(Group) );
					{
						// Also, we'll draw a rectangle around the selection
						int32 MinX = 1;
						int32 MinY = 0;
						int32 MaxX = ViewX - 1;
						int32 MaxY = GroupHeadHeight - 1;
						TileItem.SetColor( GroupColor );
						TileItem.Size = FVector2D( ViewX, GroupHeadHeight );
						TileItem.Draw( Canvas, FVector2D::ZeroVector );

						FCanvasBoxItem BoxItem( FVector2D(MinX, MinY), FVector2D(MaxX, MaxY)-FVector2D(MinX, MinY) );
						BoxItem.SetColor( GroupBorder );
						BoxItem.Draw( Canvas );
					}
					if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
				}


				// Peek ahead to see if we have any tracks or groups parented to this group
				int32 NumChildGroups = 0;
				if( !Group->bIsParented )
				{
					for( int32 OtherGroupIndex = CurGroupIndex + 1; OtherGroupIndex < InterpEd->IData->InterpGroups.Num(); ++OtherGroupIndex )
					{
						UInterpGroup* OtherGroup = InterpEd->IData->InterpGroups[ OtherGroupIndex ];

						// If this is a director group and the current window is not a director track window, then we'll skip over
						// the director group.  Similarly, for director track windows we'll skip over all non-director groups.
						bool bIsOtherGroupAppropriateForWindow =
							( OtherGroup->IsA( UInterpGroupDirector::StaticClass() ) == bIsDirectorTrackWindow );

						// Only consider the group if it isn't filtered and isn't culled
						if( OtherGroup->bVisible && bIsOtherGroupAppropriateForWindow )
						{
							if( OtherGroup->bIsParented )
							{
								++NumChildGroups;
							}
							else
							{
								// We've reached a group that isn't parented (thus it's a root), so we can just bail
								break;
							}
						}
					}
				}


				// Does the group have anything parented to it?  If so we'll draw a widget that can be used to expand or
				// collapse the group.
				int32 HalfColArrowSize = 6;
				bool bCurGroupHasAnyChildTracksOrGroups = ( Group->InterpTracks.Num() > 0 || NumChildGroups > 0 );
				if( bCurGroupHasAnyChildTracksOrGroups )
				{
					// Draw little collapse-group widget.
					FIntPoint A,B,C;
					if(Group->bCollapsed)
					{
						int32 HorizOffset = IndentPixels - 4;	// Extra negative offset for centering
						int32 VertOffset = 0.5 * GroupHeadHeight;
						A = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset - HalfColArrowSize);
						B = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset + HalfColArrowSize);
						C = FIntPoint(HorizOffset + 2*HalfColArrowSize, VertOffset );
					}
					else
					{
						int32 HorizOffset = IndentPixels;
						int32 VertOffset = 0.5 * GroupHeadHeight - 3; // Extra negative offset for centering
						A = FIntPoint(HorizOffset, VertOffset);
						B = FIntPoint(HorizOffset + HalfColArrowSize, VertOffset + HalfColArrowSize);
						C = FIntPoint(HorizOffset + 2*HalfColArrowSize, VertOffset);
					}

					FCanvasTriangleItem TriItem( A, B, C, GWhiteTexture );
					TriItem.SetColor( FLinearColor::Black );
					TriItem.Draw( Canvas );

					// Invisible hit test geometry for the collapse/expand widget
					if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HMatineeGroupCollapseBtn(Group) );
					TileItem.SetColor( FLinearColor( 0.0f, 0.0f, 0.0f, 0.01f ) );
					TileItem.Size = FVector2D( 2 * HalfColArrowSize, 2 * HalfColArrowSize );
					TileItem.Draw( Canvas, FVector2D( IndentPixels, 0.5 * GroupHeadHeight - HalfColArrowSize ) );
					if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
				}


				// If this is child group, then draw some 'tree view' lines to indicate that
				if( Group->bIsParented )
				{
					const float	HalfHeadHeight = 0.5 * GroupHeadHeight;
					const FLinearColor TreeNodeColor( 0.025f, 0.025f, 0.025f );
					const int32 TreeNodeLeftPos = MatineeGlobals::TreeLabelsMargin + 6;
					const int32 TreeNodeTopPos = 2;
					const int32 TreeNodeBottomPos = HalfHeadHeight;

					// If we're drawing an expand/collapse widget, then we'll make sure the line doesn't extend beyond that
					int32 TreeNodeRightPos = MatineeGlobals::TreeLabelsMargin + MatineeGlobals::NumPixelsToIndentChildGroups + 1;
					if( !bCurGroupHasAnyChildTracksOrGroups )
					{
						TreeNodeRightPos += HalfColArrowSize * 2;
					}
					LineItem.SetColor( TreeNodeColor );
					LineItem.Draw( Canvas, FVector2D( TreeNodeLeftPos, TreeNodeTopPos ), FVector2D( TreeNodeLeftPos, TreeNodeBottomPos ) );
					LineItem.Draw( Canvas, FVector2D( TreeNodeLeftPos, TreeNodeBottomPos ), FVector2D( TreeNodeRightPos, TreeNodeBottomPos ) );
				}


				// Draw the group name
				int32 XL, YL;
				StringSize(LabelFont, XL, YL, *Group->GroupName.ToString());
				DrawLabel(Canvas, IndentPixels + HeadTitleMargin + 2*HalfColArrowSize, 0.5*GroupHeadHeight - 0.5*YL, *Group->GroupName.ToString(), MatineeGlobals::GroupNameTextColor);
				

				// Draw button for binding camera to this group, but only if we need to.  If the group has an actor bound to
				// it, or is a director group, then it gets a camera!
				if( GroupActor != NULL || Group->IsA( UInterpGroupDirector::StaticClass() ) )
				{
					UTexture2D* ButtonTex = (Group == InterpEd->CamViewGroup) ? LabelDrawParams.CamLockedIcon : LabelDrawParams.CamUnlockedIcon;
					if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HMatineeGroupLockCamBtn(Group) );
					TileItem.SetColor( FLinearColor::White );
					TileItem.Texture = ButtonTex->Resource;
					TileItem.Size = FVector2D( 16, 16 );
					TileItem.Draw( Canvas, FVector2D( InterpEd->LabelWidth - 26, (0.5*GroupHeadHeight)-8 ) );
					TileItem.Texture = GWhiteTexture;
					if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
				}
				if( !Group->bIsFolder )
				{
					TileItem.SetColor( Group->GroupColor );
					TileItem.Texture = InterpEd->BarGradText->Resource;
					TileItem.Size = FVector2D( 6, GroupHeadHeight );
					TileItem.Draw( Canvas, FVector2D( InterpEd->LabelWidth - 6, 0 ) );
					TileItem.Texture = GWhiteTexture;
				}
				TileItem.SetColor( FLinearColor::Black );
				TileItem.Size = FVector2D( ViewX, 1 );
				TileItem.Draw( Canvas, FVector2D( 0, GroupHeadHeight-1 ) );

				Canvas->PopTransform();
			}

			// Advance vertical position passed group row
			YOffset += GroupHeadHeight;

			if(!Group->bCollapsed)
			{
				// Draw each track in this group.
				for(int32 CurTrackIndex=0; CurTrackIndex<Group->InterpTracks.Num(); CurTrackIndex++)
				{
					UInterpTrack* Track = Group->InterpTracks[CurTrackIndex];
					int32 TotalHeightAdded = 0;
					// Is this track visible?  It might be filtered out
					if( Track->bVisible )
					{
						bool bTrackSelected = Track->IsSelected();

						// Setup additional draw parameters
						TrackDrawParams.TrackIndex = CurTrackIndex;

						LabelDrawParams.IndentPixels = IndentPixels;
						LabelDrawParams.YOffset = YOffset;
						LabelDrawParams.GroupLabelColor = GroupLabelColor;
						LabelDrawParams.bTrackSelected = bTrackSelected;
						LabelDrawParams.TrackAreaHeight = TrackAreaHeight;

						Canvas->PushRelativeTransform(FTranslationMatrix(FVector(InterpEd->LabelWidth,LabelDrawParams.YOffset,0)));
						TotalHeightAdded = DrawTrack( Canvas, Track, Group, TrackDrawParams, LabelDrawParams );
						Canvas->PopTransform();
						
						// Advance vertical position
						YOffset += TotalHeightAdded;
					}
				}
			}
			else
			{
				if( bIsGroupWithinScrollArea )
				{
					const FVector2D TickSize(2.0f, GroupHeadHeight * 0.5f);

					// We'll iterate not only over ourself, but also all of our child groups
					for( int32 CollapsedGroupIndex = CurGroupIndex; CollapsedGroupIndex < InterpEd->IData->InterpGroups.Num(); ++CollapsedGroupIndex )
					{
						UInterpGroup* CurCollapsedGroup = InterpEd->IData->InterpGroups[ CollapsedGroupIndex ];

						// We're interested either in ourselves or any of our children
						if( CurCollapsedGroup == Group || CurCollapsedGroup->bIsParented )
						{
							// Since the track is collapsed, draw ticks for each of the track's keyframes.
							for(int32 TrackIdx=0; TrackIdx<CurCollapsedGroup->InterpTracks.Num(); TrackIdx++)
							{
								UInterpTrack* Track = CurCollapsedGroup->InterpTracks[TrackIdx];

								FVector2D TrackPos(InterpEd->LabelWidth - InterpEd->ViewStartTime * InterpEd->PixelsPerSec, YOffset - GroupHeadHeight);

								Canvas->PushRelativeTransform(FTranslationMatrix(FVector(TrackPos.X,TrackPos.Y,0)));
								DrawCollapsedTrackKeys( Canvas, Track, TrackPos, TickSize );
								Canvas->PopTransform();
							}
						}
						else
						{
							// Not really a child, but instead another root group.  We're done!
							break;
						}
					}
				}
			}
		}

		// If the current group is not parented, then it becomes our current parent group
		if( !Group->bIsParented )
		{
			CurParentGroup = Group;
		}
	}


	if( bWantTimeline )
	{
		// Draw grid and timeline stuff.
		DrawTimeline(InViewport, Canvas);
	}

	// Draw line between title block and track view for empty space
	TileItem.SetColor( FLinearColor::Black );
	TileItem.Size = FVector2D( 1, ViewY - YOffset );
	TileItem.Draw( Canvas, FVector2D( InterpEd->LabelWidth, YOffset-1 ) );


	// Draw snap-to line, if mouse button is down.
	bool bMouseDownInAnyViewport =
		InterpEd->TrackWindow->InterpEdVC->bMouseDown ||
		InterpEd->DirectorTrackWindow->InterpEdVC->bMouseDown;
	if(bMouseDownInAnyViewport && InterpEd->bDrawSnappingLine)
	{
		int32 SnapPosX = InterpEd->LabelWidth + (InterpEd->SnappingLinePosition - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
		LineItem.SetColor( FLinearColor::Black );
		LineItem.Draw( Canvas, FVector2D(SnapPosX, 0), FVector2D(SnapPosX, TrackAreaHeight) );
	}
	else
	{
		InterpEd->bDrawSnappingLine = false;
	}

	// Draw vertical position line
	int32 TrackPosX = InterpEd->LabelWidth + (InterpEd->MatineeActor->InterpPosition - InterpEd->ViewStartTime) * InterpEd->PixelsPerSec;
	if( TrackPosX >= InterpEd->LabelWidth && TrackPosX <= ViewX)
	{
		LineItem.SetColor( InterpEd->PosMarkerColor );
		LineItem.Draw( Canvas, FVector2D(TrackPosX, 0), FVector2D(TrackPosX, TrackAreaHeight) );
	}

	// Draw the box select box
	if(bBoxSelecting)
	{
		int32 MinX = FMath::Min(BoxStartX, BoxEndX);
		int32 MinY = FMath::Min(BoxStartY, BoxEndY);
		int32 MaxX = FMath::Max(BoxStartX, BoxEndX);
		int32 MaxY = FMath::Max(BoxStartY, BoxEndY);
		FCanvasBoxItem BoxItem( FVector2D(MinX, MinY), FVector2D(MaxX, MaxY)-FVector2D(MinX, MinY) );
		BoxItem.SetColor( FLinearColor::Red );
		BoxItem.Draw( Canvas );
	}

	Canvas->PopTransform();
}

/** 
 * Draws keyframes for all subtracks in a subgroup.  The keyframes are drawn directly on the group.
 * @param Canvas		Canvas to draw on
 * @param SubGroupOwner		Track that owns the subgroup 			
 * @param GroupIndex		Index of a subgroup to draw
 * @param DrawInfos		An array of draw information for each keyframe that needs to be drawn
 * @param TrackPos		Starting position where the keyframes should be drawn.  
 * @param KeySize		Draw size of each keyframe
 */
void FMatineeViewportClient::DrawSubTrackGroupKeys( FCanvas* Canvas, UInterpTrack* SubGroupOwner, int32 GroupIndex, const TArray<FKeyframeDrawInfo>& KeyDrawInfos, const FVector2D& TrackPos, const FVector2D& KeySize )
{
	UInterpGroup* TrackGroup = SubGroupOwner->GetOwningGroup();
	// Determine if we are drawing on something that is collapsed.  INDEX_NONE for GroupIndex indicates we are drawing on the parent track.
	bool bDrawCollapsed = GroupIndex == INDEX_NONE ? SubGroupOwner->bIsCollapsed : SubGroupOwner->SubTrackGroups[ GroupIndex ].bIsCollapsed;
	// If the track is collapsed draw each keyframe with no transparency.  If the track is not collapsed, blend each keyframe with the background
	// This reduces clutter when there are lots of keyframes
	const uint8 Alpha = bDrawCollapsed ? 255 : 85;

	FCanvasTriangleItem TriItem( FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, GWhiteTexture );
	// Draw each keyframe
	for( int32 DrawInfoIndex = 0; DrawInfoIndex < KeyDrawInfos.Num(); ++DrawInfoIndex )
	{
		const FKeyframeDrawInfo& DrawInfo = KeyDrawInfos[ DrawInfoIndex ];
		const FVector2D& KeyPos = DrawInfo.KeyPos;
	
		// Draw a tick mark.
		if(KeyPos.X >= InterpEd->ViewStartTime * InterpEd->PixelsPerSec)
		{
			FColor KeyColor = DrawInfo.KeyColor;
			KeyColor.A = Alpha;
			FColor SelectedColor = FColor(255,128,0,Alpha);
			if(Canvas->IsHitTesting()) 
			{
				Canvas->SetHitProxy( new HInterpTrackSubGroupKeypointProxy(SubGroupOwner, DrawInfo.KeyTime, GroupIndex ) );
			}
			if( bDrawCollapsed )
			{
				// If the group is collapsed draw each keyframe as a triangle
				const int32 KeyHalfTriSize = 6;
				const int32 KeyVertOffset = 3;
				int32 PixelPos = FMath::TruncToInt(DrawInfo.KeyTime * InterpEd->PixelsPerSec);

				FIntPoint A(PixelPos - KeyHalfTriSize,	TrackHeight - KeyVertOffset);
				FIntPoint B(PixelPos + KeyHalfTriSize,	TrackHeight - KeyVertOffset);
				FIntPoint C(PixelPos,					TrackHeight - KeyVertOffset - KeyHalfTriSize);
				
				if(DrawInfo.bSelected)
				{
					TriItem.SetColor( SelectedColor );
					TriItem.SetPoints( A+FIntPoint(-2,1), B+FIntPoint(2,1), C+FIntPoint(0,-2) );
					TriItem.Draw( Canvas );
				}

				TriItem.SetColor( KeyColor );
				TriItem.SetPoints( A, B, C );
				TriItem.Draw( Canvas );
				if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
			}
			else
			{
				// Draw each keyframe is a vertical bar if the group is not collapsed
				if( DrawInfo.bSelected )
				{
					Canvas->DrawTile( KeyPos.X-1,KeyPos.Y-1, KeySize.X+2, KeySize.Y+2, 0.f, 0.f, 1.f, 1.f, SelectedColor );
				}
				Canvas->DrawTile( KeyPos.X, KeyPos.Y, KeySize.X, KeySize.Y, 0.f, 0.f, 1.f, 1.f, KeyColor );
			}
			
			if( Canvas->IsHitTesting() )
			{
				Canvas->SetHitProxy( NULL );
			}
		}
	}
}

void FMatineeViewportClient::DrawCollapsedTrackKeys( FCanvas* Canvas, UInterpTrack* Track, const FVector2D& TrackPos, const FVector2D& TickSize )
{
	for(int32 KeyframeIdx=0; KeyframeIdx<Track->GetNumKeyframes(); KeyframeIdx++)
	{
		float KeyframeTime = Track->GetKeyframeTime(KeyframeIdx);
		FColor KeyframeColor = Track->GetKeyframeColor(KeyframeIdx);
		FVector2D TickPos;

		TickPos.X =  -TickSize.X / 2.0f + KeyframeTime * InterpEd->PixelsPerSec;
		TickPos.Y =  (GroupHeadHeight - 1 - TickSize.Y) / 2.0f;

		// Draw a tick mark.
		if(TickPos.X >= InterpEd->ViewStartTime * InterpEd->PixelsPerSec)
		{
			Canvas->DrawTile( TickPos.X, TickPos.Y, TickSize.X, TickSize.Y, 0.f, 0.f, 1.f, 1.f, KeyframeColor );
		}
	}

	for( int32 SubTrackIndex = 0; SubTrackIndex < Track->SubTracks.Num(); ++SubTrackIndex )
	{
		DrawCollapsedTrackKeys( Canvas, Track->SubTracks[ SubTrackIndex ], TrackPos, TickSize );
	}
}

/**
 * Selects a color for the specified group (bound to the given group actor)
 *
 * @param Group The group to select a label color for
 * @param GroupActorOrNull The actor currently bound to the specified, or NULL if none is bounds
 *
 * @return The color to use to draw the group label
 */
FColor FMatineeViewportClient::ChooseLabelColorForGroupActor( UInterpGroup* Group, AActor* GroupActorOrNull ) const
{
	check( Group != NULL );

	FColor GroupLabelColor = MatineeGlobals::DefaultGroupLabelColor;

	if( Group->IsA( UInterpGroupDirector::StaticClass() ) )
	{
		GroupLabelColor = MatineeGlobals::DirGroupLabelColor;
	}
	else if( Group->bIsFolder )
	{
		GroupLabelColor = MatineeGlobals::FolderLabelColor;
	}
	else if( GroupActorOrNull != NULL )
	{
		AActor* GroupActor = GroupActorOrNull;

		if( GroupActor->IsA( ACameraActor::StaticClass() ) )
		{
			// Camera actor
			GroupLabelColor = FColor( 130, 130, 150 );
		}
		else if( GroupActor->IsA( ASkeletalMeshActor::StaticClass() ) )
		{
			// Skeletal mesh actor
			GroupLabelColor = FColor( 130, 150, 130 );
		}
		else if( GroupActor->IsA( AStaticMeshActor::StaticClass() ) )
		{
			// Static mesh actor
			GroupLabelColor = FColor( 150, 130, 130 );
		}
		else if( GroupActor->IsA( ABrush::StaticClass() ) )
		{
			// Brush actor
			GroupLabelColor = FColor( 130, 145, 145 );
		}
		else if( GroupActor->IsA( ALight::StaticClass() ) )
		{
			// Light actor
			GroupLabelColor = FColor( 145, 145, 130 );
		}
		else if( GroupActor->IsA( AMaterialInstanceActor::StaticClass() ) )
		{
			// Material instance actor
			GroupLabelColor = FColor( 145, 130, 145 );
		}
		else if( GroupActor->IsA( AEmitter::StaticClass() ) )
		{
			// Emitter
			GroupLabelColor = FColor( 115, 95, 150 );
		}
		else
		{
			// Unrecognized actor type
		}
	}

	return GroupLabelColor;
}
