// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Matinee/InterpTrack.h"

#include "Matinee/InterpTrackMoveAxis.h"
#include "Matinee/InterpTrackColorProp.h"

#include "MatineeTransBuffer.h"
#include "Engine/InterpCurveEdSetup.h"
#include "Matinee.h"

/** Ensure the curve editor is synchronised with the track editor. */
void FMatinee::SyncCurveEdView()
{
	/*
	CurveEd->StartIn = ViewStartTime;
	CurveEd->EndIn = ViewEndTime;
	CurveEd->CurveEdVC->Viewport->Invalidate();
	*/
	CurveEd->SetViewInterval(ViewStartTime,ViewEndTime);

}

/** Add the property being controlled by this track to the graph editor. */
void FMatinee::AddTrackToCurveEd( FString GroupName, FColor GroupColor, UInterpTrack* InTrack, bool bShouldShowTrack )
{
	FString TrackTitle = InTrack->TrackTitle;

	// Slight hack for movement tracks.  Subtracks that translate are prepended with a T and subtracks that rotate are prepended with an R
	// We do this to conserve space on the curve title bar.
	UInterpTrackMoveAxis* MoveAxisTrack = Cast<UInterpTrackMoveAxis>(InTrack);
	if( MoveAxisTrack )
	{
		uint8 MoveAxis = MoveAxisTrack->MoveAxis;
		if( MoveAxis == AXIS_TranslationX || MoveAxis == AXIS_TranslationY || MoveAxis == AXIS_TranslationZ )
		{
			TrackTitle = FString(TEXT("T")) + TrackTitle;
		}
		else
		{
			TrackTitle = FString(TEXT("R")) + TrackTitle;
		}
	}
	FString CurveName = GroupName + FString(TEXT("_")) + TrackTitle;

	// Toggle whether this curve is edited in the Curve editor.
	if( !bShouldShowTrack )
	{
		IData->CurveEdSetup->RemoveCurve(InTrack);
	}
	else
	{
		FColor CurveColor = GroupColor;

		// If we are adding selected curve - highlight it.
		if( InTrack->IsSelected() )
		{
			CurveColor = SelectedCurveColor;
		}

		// Add track to the curve editor.
		bool bColorTrack = false;

		if( InTrack->IsA(UInterpTrackColorProp::StaticClass() ) )
		{
			bColorTrack = true;
		}

		IData->CurveEdSetup->AddCurveToCurrentTab(InTrack, CurveName, CurveColor, NULL, bColorTrack, bColorTrack);
	}

	CurveEd->CurveChanged();
}

/**
 * FCurveEdNotifyInterface: Called by the Curve Editor when a Curve Label is clicked on
 *
 * @param	CurveObject	The curve object whose label was clicked on
 */
void FMatinee::OnCurveLabelClicked( UObject* CurveObject )
{
	check( CurveObject != NULL );

	// Is this curve an interp track?
	UInterpTrack* Track = Cast<UInterpTrack>( CurveObject );
	if( Track != NULL )
	{
		// Select the track!
		SelectTrack( Track->GetOwningGroup(), Track );
		ClearKeySelection();
	}
}


///////////////////////////////////////////////////////////////////////////////////////
// Curve editor notify stuff

/** Implement Curve Editor notify interface, so we can back up state before changes and support Undo. */
void FMatinee::PreEditCurve(TArray<UObject*> CurvesAboutToChange)
{
	InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "CurveEdit", "Curve Edit") );

	// Call Modify on all tracks with keys selected
	for(int32 i=0; i<CurvesAboutToChange.Num(); i++)
	{
		// If this keypoint is from an InterpTrack, call Modify on it to back up its state.
		UInterpTrack* Track = Cast<UInterpTrack>( CurvesAboutToChange[i] );
		if(Track)
		{
			Track->Modify();
		}
	}
}

void FMatinee::PostEditCurve()
{
	InterpEdTrans->EndSpecial();
}



void FMatinee::MovedKey()
{
	// Update interpolation to the current position - but thing may have changed due to fiddling on the curve display.
	RefreshInterpPosition();
}

void FMatinee::DesireUndo()
{
	InterpEdUndo();
}

void FMatinee::DesireRedo()
{
	InterpEdRedo();
}
