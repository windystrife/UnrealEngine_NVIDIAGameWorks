// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	InterpolationDraw.cpp: Code for supporting interpolation of properties in-game.
=============================================================================*/

#include "CoreMinimal.h"
#include "Logging/LogScopedVerbosityOverride.h"
#include "CanvasItem.h"
#include "Engine/Texture2D.h"
#include "SceneManagement.h"
#include "Matinee/MatineeActor.h"
#include "Matinee/InterpData.h"
#include "Interpolation.h"
#include "Matinee/InterpGroup.h"
#include "Matinee/InterpGroupInst.h"
#include "CanvasTypes.h"
#include "Matinee/InterpTrack.h"
#include "Matinee/InterpTrackMove.h"
#include "Matinee/InterpTrackInstMove.h"
#include "Matinee/InterpTrackFloatBase.h"
#include "Matinee/InterpTrackMoveAxis.h"
#include "Matinee/InterpTrackToggle.h"
#include "Matinee/InterpTrackEvent.h"
#include "Matinee/InterpTrackFade.h"
#include "Matinee/InterpTrackDirector.h"
#include "Matinee/InterpTrackAnimControl.h"
#include "Matinee/InterpTrackFloatProp.h"
#include "Matinee/InterpTrackBoolProp.h"
#include "Matinee/InterpTrackVectorBase.h"
#include "Matinee/InterpTrackVectorProp.h"
#include "Matinee/InterpTrackLinearColorBase.h"
#include "Matinee/InterpTrackLinearColorProp.h"
#include "Matinee/InterpTrackColorProp.h"
#include "Matinee/InterpTrackSound.h"
#include "Matinee/InterpTrackSlomo.h"
#include "Matinee/InterpTrackColorScale.h"
#include "Matinee/InterpTrackAudioMaster.h"
#include "Matinee/InterpTrackVisibility.h"
#include "Matinee/InterpTrackParticleReplay.h"
#include "InterpolationHitProxy.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Animation/AnimSequence.h"
#include "Sound/SoundBase.h"

static const int32	KeyHalfTriSize = 6;
static const FColor KeyNormalColor(0,0,0);
static const FColor KeyCurveColor(100,0,0);
static const FColor KeyLinearColor(0,100,0);
static const FColor KeyConstantColor(0,0,100);
static const FColor	KeySelectedColor(255,128,0);
static const FColor KeyLabelColor(225,225,225);
static const int32	KeyVertOffset = 3;

static const float	DrawTrackTimeRes = 0.1f;
static const float	CurveHandleScale = 0.5f;


enum EInterpTrackAnimControlDragType
{
	ACDT_AnimBlockLeftEdge = 0,
	ACDT_AnimBlockRightEdge
};

enum EInterpTrackParticleReplayDragType
{
	PRDT_LeftEdge = 10,
	PRDT_RightEdge
};

	
/*-----------------------------------------------------------------------------
  UInterpTrack
-----------------------------------------------------------------------------*/

void UInterpTrack::DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params )
{
#if WITH_EDITORONLY_DATA
	int32 NumKeys = GetNumKeyframes();
	FCanvasTriangleItem TriItem( FVector2D( 0.0f, 0.0f ), FVector2D( 0.0f, 0.0f ), FVector2D( 0.0f, 0.0f ), GWhiteTexture );
	for(int32 i=0; i<NumKeys; i++)
	{
		float KeyTime = GetKeyframeTime(i);

		int32 PixelPos = FMath::TruncToInt((KeyTime - Params.StartTime) * Params.PixelsPerSec);

		FIntPoint A(PixelPos - KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset);
		FIntPoint B(PixelPos + KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset);
		FIntPoint C(PixelPos,					Params.TrackHeight - KeyVertOffset - KeyHalfTriSize);

		bool bKeySelected = false;
		for(int32 j=0; j<Params.SelectedKeys.Num() && !bKeySelected; j++)
		{
			if( Params.SelectedKeys[j].Group == Group && 
				Params.SelectedKeys[j].Track == this && 
				Params.SelectedKeys[j].KeyIndex == i )
				bKeySelected = true;
		}

		FColor KeyColor = GetKeyframeColor(i);
		
		if(Canvas->IsHitTesting()) 
		{
			Canvas->SetHitProxy( new HInterpTrackKeypointProxy(Group, this, i) );
		}

		TriItem.SetPoints( A+FIntPoint(-2,1), B+FIntPoint(2,1), C+FIntPoint(0,-2) );
		if(bKeySelected)
		{
			TriItem.SetColor( KeySelectedColor );
			Canvas->DrawItem( TriItem );
		}
		TriItem.SetPoints( A, B, C );
		TriItem.SetColor( KeyColor );
		Canvas->DrawItem( TriItem );

		if(Canvas->IsHitTesting()) 
		{
			Canvas->SetHitProxy( NULL );
		}
	}
#endif // WITH_EDITORONLY_DATA
}

FColor UInterpTrack::GetKeyframeColor(int32 KeyIndex) const
{
	return KeyNormalColor;
}

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrack::GetTrackIcon() const
{
	return TrackIcon;
}
#endif // WITH_EDITORONLY_DATA

/*-----------------------------------------------------------------------------
  UInterpTrackMove
-----------------------------------------------------------------------------*/

FColor UInterpTrackMove::GetKeyframeColor(int32 KeyIndex) const
{
	if( KeyIndex < 0 || KeyIndex >= PosTrack.Points.Num() )
	{
		return KeyNormalColor;
	}

	if( PosTrack.Points[KeyIndex].IsCurveKey() )
	{
		return KeyCurveColor;
	}
	else if( PosTrack.Points[KeyIndex].InterpMode == CIM_Linear )
	{
		return KeyLinearColor;
	}
	else
	{
		return KeyConstantColor;
	}
}


void UInterpTrackMove::DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params )
{
#if WITH_EDITORONLY_DATA
	int32 NumKeys = GetNumKeyframes();

	const bool bHitTesting = Canvas->IsHitTesting();
	const bool bAllowTextSelection = bHitTesting && Params.bAllowKeyframeTextSelection;

	FCanvasTriangleItem TriItem( FVector2D( 0.0f, 0.0f ), FVector2D( 0.0f, 0.0f ), FVector2D( 0.0f, 0.0f ), GWhiteTexture );
	for(int32 KeyIndex=0; KeyIndex<NumKeys; KeyIndex++)
	{
		float KeyTime = GetKeyframeTime(KeyIndex);

		int32 PixelPos = FMath::TruncToInt((KeyTime - Params.StartTime) * Params.PixelsPerSec);

		FIntPoint A(PixelPos - KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset);
		FIntPoint B(PixelPos + KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset);
		FIntPoint C(PixelPos,					Params.TrackHeight - KeyVertOffset - KeyHalfTriSize);

		bool bKeySelected = false;
		for(int32 SelectedKeyIndex=0; SelectedKeyIndex<Params.SelectedKeys.Num() && !bKeySelected; SelectedKeyIndex++)
		{
			if( Params.SelectedKeys[SelectedKeyIndex].Group == Group && 
				Params.SelectedKeys[SelectedKeyIndex].Track == this && 
				Params.SelectedKeys[SelectedKeyIndex].KeyIndex == KeyIndex )
				bKeySelected = true;
		}

		FColor KeyColor = GetKeyframeColor(KeyIndex);
		
		if( bHitTesting )
		{
			Canvas->SetHitProxy( new HInterpTrackKeypointProxy( Group, this, KeyIndex) );
		}
		
		TriItem.SetPoints( A+FIntPoint(-2,1), B+FIntPoint(2,1), C+FIntPoint(0,-2) );
		if(bKeySelected)
		{
			TriItem.SetColor( KeySelectedColor );
			Canvas->DrawItem( TriItem );
		}
		TriItem.SetPoints( A, B, C );
		TriItem.SetColor( KeyColor );
		Canvas->DrawItem( TriItem );

		if( bHitTesting ) 
		{
			Canvas->SetHitProxy( NULL );
		}

		// Draw lookup name if one exists for this key.
		FName LookupName = GetLookupKeyGroupName(KeyIndex);
		if(LookupName != NAME_None)
		{
			int32 XL, YL;
			FString Str = LookupName.ToString();
			StringSize( GEngine->GetSmallFont(), XL, YL, *Str );

			if ( bAllowTextSelection )
			{
				Canvas->SetHitProxy( new HInterpTrackKeypointProxy( Group, this, KeyIndex ) );
			}
			Canvas->DrawShadowedString( PixelPos - XL / 2, Params.TrackHeight - YL - KeyVertOffset - KeyHalfTriSize - 2, *Str, GEngine->GetSmallFont(), KeyLabelColor );
			if ( bAllowTextSelection )
			{
				Canvas->SetHitProxy( NULL );
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}


void UInterpTrackMove::Render3DTrack(UInterpTrackInst* TrInst, 
									 const FSceneView* View, 
									 FPrimitiveDrawInterface* PDI, 
									 int32 TrackIndex, 
									 const FColor& TrackColor, 
									 TArray<struct FInterpEdSelKey>& SelectedKeys)
{
#if WITH_EDITORONLY_DATA
	// Draw nothing if no points and no subtracks or if we are hiding the 3d track
	if( (PosTrack.Points.Num() == 0 && SubTracks.Num() == 0) || bHide3DTrack )
	{
		return;
	}

	LOG_SCOPE_VERBOSITY_OVERRIDE(LogAnimation, ELogVerbosity::NoLogging);

	const bool bHitTesting = PDI->IsHitTesting();
	UInterpGroup* Group = CastChecked<UInterpGroup>(GetOuter());

	// Create the 3d curve from data in the subtracks if this track has subtracks.
	if( SubTracks.Num() > 0 )
	{
		FVector OldKeyPos(0);
		float OldKeyTime = 0.f;

		float StartTime = 0.0f; 
		float EndTime = 0.0f;
		int32 MaxKeyframes = 0;
		for( int32 PosTrackIndex = 0; PosTrackIndex < 3; ++PosTrackIndex )
		{
			float TrackStart = 0.0f;
			float TrackEnd = 0.0f;
			SubTracks[ PosTrackIndex ]->GetTimeRange( TrackStart, TrackEnd );
			StartTime = FMath::Min( TrackStart, StartTime );
			EndTime = FMath::Max( TrackEnd, EndTime );
			MaxKeyframes = FMath::Max( SubTracks[ PosTrackIndex ]->GetNumKeyframes(), MaxKeyframes );
		}


		const float TotalTime = EndTime - StartTime;
		
		// Do nothing if the total time to draw is 0
		if( TotalTime > 0.0f )
		{
			// Determine the number of steps to draw.  More steps means a smoother curve
			int32 NumSteps = FMath::CeilToInt( TotalTime/DrawTrackTimeRes );
			// Ensure the number of steps to draw wont cause a rendering perf hit.
			NumSteps = FMath::Min( 100, NumSteps );
			float DrawSubstep = TotalTime/NumSteps;

			// True if this is the first time we draw anything
			bool bFirst = true;

			FVector OldPos(0);
			FRotator OldRot(0,0,0);

			// Start at StartTime and increment the time based on the number of substeps to draw
			for( float Time=StartTime; Time <= TotalTime; Time+=DrawSubstep )
			{
				FVector NewKeyPos(0);
				FRotator NewKeyRot(0,0,0);
				// Get the position and rotation at each time step
				GetLocationAtTime(TrInst, Time, NewKeyPos, NewKeyRot);

				// Draw a little point for each substep
				PDI->DrawPoint(NewKeyPos, TrackColor, 3.f, SDPG_Foreground);
				// If not the first keypoint, draw a line to the last keypoint.
				if( !bFirst )
				{
					PDI->DrawLine(OldKeyPos, NewKeyPos, TrackColor, SDPG_Foreground);					
				}
				bFirst = false;
				// Update the last keyframe for next iteration
				OldKeyPos = NewKeyPos;
			}
			
			// For each subtrack draw a point representing a keyframe on top of the 3d curve
			for( int32 SubTrackIndex = 0; SubTrackIndex < 3 ; ++SubTrackIndex )
			{
				// 	Draw keypoints on top of curve
				// 		
				UInterpTrackMoveAxis* SubTrack = Cast<UInterpTrackMoveAxis>( SubTracks[ SubTrackIndex ] );
				for(int32 KeyIndex = 0; KeyIndex < SubTrack->FloatTrack.Points.Num(); ++KeyIndex )
				{
					// Find if this key is one of the selected ones.
					bool bKeySelected = false;
					for( int32 SelKeyIndex = 0; SelKeyIndex < SelectedKeys.Num() && !bKeySelected; ++SelKeyIndex )
					{
						if( SelectedKeys[SelKeyIndex].Group == Group && 
							SelectedKeys[SelKeyIndex].Track == SubTrack && 
							SelectedKeys[SelKeyIndex].KeyIndex == KeyIndex )
						{
							bKeySelected = true;
						}

					}

					// Find the time, position and orientation of this Key.
					float NewKeyTime = SubTrack->FloatTrack.Points[KeyIndex].InVal;

					FVector NewKeyPos(0);
					FRotator NewKeyRot(0,0,0);
					GetLocationAtTime(TrInst, NewKeyTime, NewKeyPos, NewKeyRot);

					UInterpTrackInstMove* MoveTrackInst = CastChecked<UInterpTrackInstMove>(TrInst);
					FTransform RefTM = GetMoveRefFrame(MoveTrackInst);

					FColor KeyColor = bKeySelected ? KeySelectedColor : TrackColor;

					if(bHitTesting) 
					{
						PDI->SetHitProxy( new HInterpTrackKeypointProxy(Group, SubTrack, KeyIndex) );
					}

					PDI->DrawPoint(NewKeyPos, KeyColor, 6.f, SDPG_Foreground);

					// If desired, draw directional arrow at each keyframe.
					if(bShowArrowAtKeys)
					{
						FRotationTranslationMatrix ArrowToWorld(NewKeyRot,NewKeyPos);
						DrawDirectionalArrow(PDI, FScaleMatrix(FVector(16.f,16.f,16.f)) * ArrowToWorld, KeyColor, 3.f, 1.f, SDPG_Foreground );
					}

					if(bHitTesting) 
					{
						PDI->SetHitProxy( NULL );
					}

					UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
					AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
					UInterpGroupInst* FirstGrInst = MatineeActor->FindFirstGroupInst(Group);

					// If a selected key, and this is the 'first' instance of this group, draw handles.
					if(bKeySelected && (GrInst == FirstGrInst))
					{
						// TODO: need to figure out somthing for this since subtrack keyframes are not guaranteed to be aligned
						/*	
						float ArriveTangent = FloatTrack.Points(KeyIndex).ArriveTangent;
						float LeaveTangent = FloatTrack.Points(KeyIndex).LeaveTangent;

						uint8 PrevMode = (KeyIndex > 0)							? SubTrack->GetKeyInterpMode(KeyIndex-1) : 255;
						uint8 NextMode = (KeyIndex < FloatTrack.Points.Num()-1)	? SubTrack->GetKeyInterpMode(KeyIndex)	: 255;

						// If not first point, and previous mode was a curve type.
						if(PrevMode == CIM_CurveAuto || PrevMode == CIM_CurveAutoClamped || PrevMode == CIM_CurveUser || PrevMode == CIM_CurveBreak)
						{
						FVector ArriveTangentVec(0,0,0);
						ArriveTangentVec[ SubTrack->MoveAxis ] = ArriveTangent;
						FVector HandlePos = NewKeyPos - RefTM.TransformVector(ArriveTangentVec * CurveHandleScale);
						PDI->DrawLine(NewKeyPos, HandlePos, FColor(128,255,0), SDPG_Foreground);

						if(bHitTesting) 
						{
						PDI->SetHitProxy( new HInterpTrackKeyHandleProxy(Group, TrackIndex, KeyIndex, true) );
						}
						PDI->DrawPoint(HandlePos, FColor(128,255,0), 5.f, SDPG_Foreground);
						if(bHitTesting)
						{
						PDI->SetHitProxy( NULL );
						}

						// If next section is a curve, draw leaving handle.
						if(NextMode == CIM_CurveAuto || NextMode == CIM_CurveAutoClamped || NextMode == CIM_CurveUser || NextMode == CIM_CurveBreak)
						{
						FVector LeaveTangentVec(0,0,0);
						LeaveTangentVec[ SubTrack->MoveAxis ] = LeaveTangent;
						FVector HandlePos = NewKeyPos + RefTM.TransformVector(LeaveTangentVec * CurveHandleScale);
						PDI->DrawLine(NewKeyPos, HandlePos, FColor(128,255,0), SDPG_Foreground);

						if(bHitTesting) 
						{
						PDI->SetHitProxy( new HInterpTrackKeyHandleProxy(Group, TrackIndex, KeyIndex, false) );
						}
						PDI->DrawPoint(HandlePos, FColor(128,255,0), 5.f, SDPG_Foreground);
						if(bHitTesting) 
						{
						PDI->SetHitProxy( NULL );
						}
						}
						}*/
					}
				}
			}
		}
	}
	else
	{

		FVector OldKeyPos(0);
		float OldKeyTime = 0.f;

		for(int32 i=0; i<PosTrack.Points.Num(); i++)
		{
			float NewKeyTime = PosTrack.Points[i].InVal;

			FVector NewKeyPos(0);
			FRotator NewKeyRot(0,0,0);
			GetLocationAtTime(TrInst, NewKeyTime, NewKeyPos, NewKeyRot);

			// If not the first keypoint, draw a line to the last keypoint.
			if(i>0)
			{
				int32 NumSteps = FMath::CeilToInt( (NewKeyTime - OldKeyTime)/DrawTrackTimeRes );
								// Limit the number of steps to prevent a rendering performance hit
				NumSteps = FMath::Min( 100, NumSteps );
				float DrawSubstep = (NewKeyTime - OldKeyTime)/NumSteps;

				// Find position on first keyframe.
				float OldTime = OldKeyTime;

				FVector OldPos(0);
				FRotator OldRot(0,0,0);
				GetLocationAtTime(TrInst, OldKeyTime, OldPos, OldRot);

				// For constant interpolation - don't draw ticks - just draw dotted line.
				if(PosTrack.Points[i-1].InterpMode == CIM_Constant)
				{
					DrawDashedLine(PDI,OldPos, NewKeyPos, TrackColor, 20, SDPG_Foreground);
				}
				else
				{
					// Then draw a line for each substep.
					for(int32 j=1; j<NumSteps+1; j++)
					{
						float NewTime = OldKeyTime + j*DrawSubstep;

						FVector NewPos(0);
						FRotator NewRot(0,0,0);
						GetLocationAtTime(TrInst, NewTime, NewPos, NewRot);

						PDI->DrawLine(OldPos, NewPos, TrackColor, SDPG_Foreground);

						// Don't draw point for last one - its the keypoint drawn above.
						if(j != NumSteps)
						{
							PDI->DrawPoint(NewPos, TrackColor, 3.f, SDPG_Foreground);
						}

						OldTime = NewTime;
						OldPos = NewPos;
					}
				}
			}

			OldKeyTime = NewKeyTime;
			OldKeyPos = NewKeyPos;
		}

		// Draw keypoints on top of curve
		for(int32 i=0; i<PosTrack.Points.Num(); i++)
		{
			// Find if this key is one of the selected ones.
			bool bKeySelected = false;
			for(int32 j=0; j<SelectedKeys.Num() && !bKeySelected; j++)
			{
				if( SelectedKeys[j].Group == Group && 
					SelectedKeys[j].Track == this && 
					SelectedKeys[j].KeyIndex == i )
					bKeySelected = true;
			}

			// Find the time, position and orientation of this Key.
			float NewKeyTime = PosTrack.Points[i].InVal;

			FVector NewKeyPos(0);
			FRotator NewKeyRot(0,0,0);
			GetLocationAtTime(TrInst, NewKeyTime, NewKeyPos, NewKeyRot);

			UInterpTrackInstMove* MoveTrackInst = CastChecked<UInterpTrackInstMove>(TrInst);
			FTransform RefTM = GetMoveRefFrame(MoveTrackInst);

			FColor KeyColor = bKeySelected ? KeySelectedColor : TrackColor;

			if(bHitTesting) PDI->SetHitProxy( new HInterpTrackKeypointProxy(Group, this, i) );
			PDI->DrawPoint(NewKeyPos, KeyColor, 6.f, SDPG_Foreground);

			// If desired, draw directional arrow at each keyframe.
			if(bShowArrowAtKeys)
			{
				FRotationTranslationMatrix ArrowToWorld(NewKeyRot,NewKeyPos);
				DrawDirectionalArrow(PDI, FScaleMatrix(FVector(16.f,16.f,16.f)) * ArrowToWorld, KeyColor, 3.f, 1.f, SDPG_Foreground );
			}
			if(bHitTesting) PDI->SetHitProxy( NULL );

			UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
			AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
			UInterpGroupInst* FirstGrInst = MatineeActor->FindFirstGroupInst(Group);

			// If a selected key, and this is the 'first' instance of this group, draw handles.
			if(bKeySelected && (GrInst == FirstGrInst))
			{
				FVector ArriveTangent = PosTrack.Points[i].ArriveTangent;
				FVector LeaveTangent = PosTrack.Points[i].LeaveTangent;

				EInterpCurveMode PrevMode = (i > 0)							? GetKeyInterpMode(i-1) : EInterpCurveMode(255);
				EInterpCurveMode NextMode = (i < PosTrack.Points.Num()-1)	? GetKeyInterpMode(i)	: EInterpCurveMode(255);

				// If not first point, and previous mode was a curve type.
				if(PrevMode == CIM_CurveAuto || PrevMode == CIM_CurveAutoClamped || PrevMode == CIM_CurveUser || PrevMode == CIM_CurveBreak)
				{
					FVector HandlePos = NewKeyPos - RefTM.TransformVector(ArriveTangent * CurveHandleScale);
					PDI->DrawLine(NewKeyPos, HandlePos, FColor(128,255,0), SDPG_Foreground);

					if(bHitTesting) PDI->SetHitProxy( new HInterpTrackKeyHandleProxy(Group, TrackIndex, i, true) );
					PDI->DrawPoint(HandlePos, FColor(128,255,0), 5.f, SDPG_Foreground);
					if(bHitTesting) PDI->SetHitProxy( NULL );
				}

				// If next section is a curve, draw leaving handle.
				if(NextMode == CIM_CurveAuto || NextMode == CIM_CurveAutoClamped || NextMode == CIM_CurveUser || NextMode == CIM_CurveBreak)
				{
					FVector HandlePos = NewKeyPos + RefTM.TransformVector(LeaveTangent * CurveHandleScale);
					PDI->DrawLine(NewKeyPos, HandlePos, FColor(128,255,0), SDPG_Foreground);

					if(bHitTesting) PDI->SetHitProxy( new HInterpTrackKeyHandleProxy(Group, TrackIndex, i, false) );
					PDI->DrawPoint(HandlePos, FColor(128,255,0), 5.f, SDPG_Foreground);
					if(bHitTesting) PDI->SetHitProxy( NULL );
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackMove::GetTrackIcon() const
{
	return TrackIcon;
}
#endif // WITH_EDITORONLY_DATA

/*-----------------------------------------------------------------------------
	UInterpTrackFloatBase
-----------------------------------------------------------------------------*/

FColor UInterpTrackFloatBase::GetKeyframeColor(int32 KeyIndex) const
{
	if( KeyIndex < 0 || KeyIndex >= FloatTrack.Points.Num() )
	{
		return KeyNormalColor;
	}

	if( FloatTrack.Points[KeyIndex].IsCurveKey() )
	{
		return KeyCurveColor;
	}
	else if( FloatTrack.Points[KeyIndex].InterpMode == CIM_Linear )
	{
		return KeyLinearColor;
	}
	else
	{
		return KeyConstantColor;
	}
}

/*-----------------------------------------------------------------------------
	UInterpTrackFloatProp
-----------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackFloatProp::GetTrackIcon() const
{
	return TrackIcon;
}
#endif // WITH_EDITORONLY_DATA

/*-----------------------------------------------------------------------------
	UInterpTrackBoolProp
-----------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackBoolProp::GetTrackIcon() const
{
	return TrackIcon;
}
#endif // WITH_EDITORONLY_DATA


/*-----------------------------------------------------------------------------
	UInterpTrackToggle
-----------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackToggle::GetTrackIcon() const
{
	return TrackIcon;
}
#endif // WITH_EDITORONLY_DATA

void UInterpTrackToggle::DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params )
{
#if WITH_EDITORONLY_DATA
	int32 NumKeys = GetNumKeyframes();

	const bool bIsHitTesting = Canvas->IsHitTesting();
	const bool bAllowBarSelection = bIsHitTesting && Params.bAllowKeyframeBarSelection;

	// Draw the 'on' blocks in green
	int32 LastPixelPos = -1;
	bool bLastPosWasOn = false;
	for (int32 i=0; i<NumKeys; i++)
	{
		float KeyTime = GetKeyframeTime(i);
		int32 PixelPos = FMath::TruncToInt((KeyTime - Params.StartTime) * Params.PixelsPerSec);

		FToggleTrackKey& Key = ToggleTrack[i];
		if ((Key.ToggleAction == ETTA_Off) && bLastPosWasOn)
		{
			if ( bAllowBarSelection )
			{
				Canvas->SetHitProxy( new HInterpTrackKeypointProxy( Group, this, i ) );
			}
			Canvas->DrawTile( LastPixelPos, KeyVertOffset, PixelPos - LastPixelPos, Params.TrackHeight - (2 * KeyVertOffset), 
				0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(0.0f, 1.0f, 0.0f));
			if ( bAllowBarSelection )
			{
				Canvas->SetHitProxy( NULL );
			}
		}

		LastPixelPos = PixelPos;
		bLastPosWasOn = (Key.ToggleAction == ETTA_On) ? true : false;
	}

	FCanvasTriangleItem TriItem( FVector2D( 0.0f, 0.0f ), FVector2D( 0.0f, 0.0f ), FVector2D( 0.0f, 0.0f ), GWhiteTexture );
	// Draw the keyframe points after, so they are on top
	for (int32 i=0; i<NumKeys; i++)
	{
		float KeyTime = GetKeyframeTime(i);

		int32 PixelPos = FMath::TruncToInt((KeyTime - Params.StartTime) * Params.PixelsPerSec);

		FIntPoint A, A_Offset;
		FIntPoint B, B_Offset;
		FIntPoint C, C_Offset;

		FToggleTrackKey& Key = ToggleTrack[i];
		if (Key.ToggleAction == ETTA_Off)
		{
			// Point the triangle down...
			A = FIntPoint(PixelPos - KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset - KeyHalfTriSize);
			B = FIntPoint(PixelPos + KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset - KeyHalfTriSize);
			C = FIntPoint(PixelPos,						Params.TrackHeight - KeyVertOffset);

			A_Offset = FIntPoint(-2,-2);
			B_Offset = FIntPoint( 2,-2);
			C_Offset = FIntPoint( 0, 1);
		}
		else
		if (Key.ToggleAction == ETTA_Trigger)
		{
			// Point the triangle up
			A = FIntPoint(PixelPos - KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset);
			B = FIntPoint(PixelPos + KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset);
			C = FIntPoint(PixelPos,						Params.TrackHeight - KeyVertOffset - KeyHalfTriSize);

			A_Offset = FIntPoint(-2, 1);
			B_Offset = FIntPoint( 2, 1);
			C_Offset = FIntPoint( 0,-2);

			if ( bAllowBarSelection )
			{
				Canvas->SetHitProxy( new HInterpTrackKeypointProxy( Group, this, i ) );
			}
			Canvas->DrawTile( PixelPos - 4, KeyVertOffset, 7, Params.TrackHeight - (2 * KeyVertOffset), 
				0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(1.0f, 0.0f, 0.0f));
			if ( bAllowBarSelection )
			{
				Canvas->SetHitProxy( NULL );
			}
		}
		else
		{
			// Point the triangle up
			A = FIntPoint(PixelPos - KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset);
			B = FIntPoint(PixelPos + KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset);
			C = FIntPoint(PixelPos,						Params.TrackHeight - KeyVertOffset - KeyHalfTriSize);

			A_Offset = FIntPoint(-2, 1);
			B_Offset = FIntPoint( 2, 1);
			C_Offset = FIntPoint( 0,-2);
		}

		bool bKeySelected = false;
		for (int32 j=0; j<Params.SelectedKeys.Num() && !bKeySelected; j++)
		{
			if (Params.SelectedKeys[j].Group == Group && 
				Params.SelectedKeys[j].Track == this && 
				Params.SelectedKeys[j].KeyIndex == i)
			{
				bKeySelected = true;
			}
		}

		FColor KeyColor = GetKeyframeColor(i);
		
		if( bIsHitTesting )
		{
			Canvas->SetHitProxy( new HInterpTrackKeypointProxy(Group, this, i) );
		}
		
		TriItem.SetPoints( A + A_Offset, B + B_Offset, C + C_Offset );
		if(bKeySelected)
		{
			TriItem.SetColor( KeySelectedColor );
			Canvas->DrawItem( TriItem );
		}
		TriItem.SetPoints( A, B, C );
		TriItem.SetColor( KeyColor );
		Canvas->DrawItem( TriItem );

		if( bIsHitTesting )
		{
			Canvas->SetHitProxy( NULL );
		}
	}
#endif // WITH_EDITORONLY_DATA
}


/*-----------------------------------------------------------------------------
	UInterpTrackVectorBase
-----------------------------------------------------------------------------*/

FColor UInterpTrackVectorBase::GetKeyframeColor(int32 KeyIndex) const
{
	if( KeyIndex < 0 || KeyIndex >= VectorTrack.Points.Num() )
	{
		return KeyNormalColor;
	}

	if( VectorTrack.Points[KeyIndex].IsCurveKey() )
	{
		return KeyCurveColor;
	}
	else if( VectorTrack.Points[KeyIndex].InterpMode == CIM_Linear )
	{
		return KeyLinearColor;
	}
	else
	{
		return KeyConstantColor;
	}
}




/*-----------------------------------------------------------------------------
	UInterpTrackLinearColorBase
-----------------------------------------------------------------------------*/

FColor UInterpTrackLinearColorBase::GetKeyframeColor(int32 KeyIndex) const
{
	if( KeyIndex < 0 || KeyIndex >= LinearColorTrack.Points.Num() )
	{
		return KeyNormalColor;
	}

	if( LinearColorTrack.Points[KeyIndex].IsCurveKey() )
	{
		return KeyCurveColor;
	}
	else if( LinearColorTrack.Points[KeyIndex].InterpMode == CIM_Linear )
	{
		return KeyLinearColor;
	}
	else
	{
		return KeyConstantColor;
	}
}



/*-----------------------------------------------------------------------------
	UInterpTrackVectorProp
-----------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackVectorProp::GetTrackIcon() const
{
	return TrackIcon;
}
#endif // WITH_EDITORONLY_DATA

/*-----------------------------------------------------------------------------
	UInterpTrackColorProp
-----------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackColorProp::GetTrackIcon() const
{
	return TrackIcon;
}
#endif // WITH_EDITORONLY_DATA


/*-----------------------------------------------------------------------------
	UInterpTrackLinearColorProp
-----------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackLinearColorProp::GetTrackIcon() const
{
	return TrackIcon;
}
#endif // WITH_EDITORONLY_DATA


/*-----------------------------------------------------------------------------
	UInterpTrackEvent
-----------------------------------------------------------------------------*/


void UInterpTrackEvent::DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params )
{
	Super::DrawTrack( Canvas, Group, Params );

#if WITH_EDITORONLY_DATA
	const bool bHitTesting = Canvas->IsHitTesting();
	const bool bAllowTextSelection = bHitTesting && Params.bAllowKeyframeTextSelection;

	for(int32 i=0; i<EventTrack.Num(); i++)
	{
		float KeyTime = EventTrack[i].Time;
		int32 PixelPos = FMath::TruncToInt((KeyTime - Params.StartTime) * Params.PixelsPerSec);

		int32 XL, YL;
		FString Str = EventTrack[i].EventName.ToString();
		StringSize( GEngine->GetSmallFont(), XL, YL, *Str );

		if ( bAllowTextSelection )
		{
			Canvas->SetHitProxy( new HInterpTrackKeypointProxy( Group, this, i ) );
		}
		Canvas->DrawShadowedString( PixelPos + 2, Params.TrackHeight - YL - KeyVertOffset, *Str, GEngine->GetSmallFont(), KeyLabelColor );
		if ( bAllowTextSelection )
		{
			Canvas->SetHitProxy( NULL );
		}
	}
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackEvent::GetTrackIcon() const
{
	return TrackIcon;
}
#endif // WITH_EDITORONLY_DATA

/*-----------------------------------------------------------------------------
	UInterpTrackDirector
-----------------------------------------------------------------------------*/


void UInterpTrackDirector::DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params )
{
#if WITH_EDITORONLY_DATA
	UInterpData* Data = CastChecked<UInterpData>(Group->GetOuter());

	const bool bHitTesting = Canvas->IsHitTesting();
	const bool bAllowBarSelection = bHitTesting && Params.bAllowKeyframeBarSelection;
	const bool bAllowTextSelection = bHitTesting && Params.bAllowKeyframeTextSelection;

	// Draw background colored blocks for camera sections
	for(int32 i=0; i<CutTrack.Num(); i++)
	{
		float KeyTime = CutTrack[i].Time;

		float NextKeyTime;
		if(i < CutTrack.Num()-1)
		{
			NextKeyTime = FMath::Min( CutTrack[i+1].Time, Data->InterpLength );
		}
		else
		{
			NextKeyTime = Data->InterpLength;
		}

		// Find the group we are cutting to.
		int32 CutGroupIndex = Data->FindGroupByName( CutTrack[i].TargetCamGroup );

		// If its valid, and its not this track, draw a box over duration of shot.
		if((CutGroupIndex != INDEX_NONE) && (CutTrack[i].TargetCamGroup != Group->GroupName))
		{
			int32 StartPixelPos = FMath::TruncToInt((KeyTime - Params.StartTime) * Params.PixelsPerSec);
			int32 EndPixelPos = FMath::TruncToInt((NextKeyTime - Params.StartTime) * Params.PixelsPerSec);

			if ( bAllowBarSelection )
			{
				Canvas->SetHitProxy( new HInterpTrackKeypointProxy( Group, this, i ) );
			}
			Canvas->DrawTile( StartPixelPos, KeyVertOffset, EndPixelPos - StartPixelPos, FMath::TruncToFloat(Params.TrackHeight - 2.f*KeyVertOffset), 0.f, 0.f, 1.f, 1.f, Data->InterpGroups[CutGroupIndex]->GroupColor );
			if ( bAllowBarSelection )
			{
				Canvas->SetHitProxy( NULL );
			}
		}
	}

	// Use base-class to draw key triangles
	Super::DrawTrack( Canvas, Group, Params );

	// Draw group name for each shot.
	for(int32 i=0; i<CutTrack.Num(); i++)
	{
		float KeyTime = CutTrack[i].Time;
		int32 PixelPos = FMath::TruncToInt((KeyTime - Params.StartTime) * Params.PixelsPerSec);

		int32 XL, YL;
		//Append the shot name to the target group name
		FString ShotName = GetFormattedCameraShotName(i);
		FString Str = FString::Printf(TEXT("%s [%s]"),*(CutTrack[i].TargetCamGroup.ToString()),*ShotName);
		
		StringSize( GEngine->GetSmallFont(), XL, YL, *Str );
		if ( bAllowTextSelection )
		{
			Canvas->SetHitProxy( new HInterpTrackKeypointProxy( Group, this, i ) );
		}

		Canvas->DrawShadowedString( PixelPos + 2, Params.TrackHeight - YL - KeyVertOffset, *Str, GEngine->GetSmallFont(), KeyLabelColor );
		if ( bAllowTextSelection )
		{
			Canvas->SetHitProxy( NULL );
		}
	}
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackDirector::GetTrackIcon() const
{
	return TrackIcon; 
}
#endif // WITH_EDITORONLY_DATA

/*-----------------------------------------------------------------------------
	UInterpTrackAnimControl
-----------------------------------------------------------------------------*/

void UInterpTrackAnimControl::BeginDrag(FInterpEdInputData &InputData)
{
	// Store temporary data.
	if((InputData.InputType == ACDT_AnimBlockLeftEdge || InputData.InputType == ACDT_AnimBlockRightEdge) && AnimSeqs.IsValidIndex(InputData.InputData))
	{
		// Store our starting position.
		InputData.TempData = new FAnimControlTrackKey(AnimSeqs[InputData.InputData]);
	}
}

void UInterpTrackAnimControl::EndDrag(FInterpEdInputData &InputData)
{
	// Clean up our temporary data.
	if(InputData.TempData)
	{
		FAnimControlTrackKey* InterpKey = (FAnimControlTrackKey*)InputData.TempData;
		delete InterpKey;
		InputData.TempData = NULL;
	}
}

EMouseCursor::Type UInterpTrackAnimControl::GetMouseCursor(FInterpEdInputData &InputData)
{
	EMouseCursor::Type Result = EMouseCursor::Default;

	switch(InputData.InputType)
	{
	case ACDT_AnimBlockLeftEdge: case ACDT_AnimBlockRightEdge:
		Result = EMouseCursor::ResizeLeftRight;
		break;
	}

	return Result;
}

void UInterpTrackAnimControl::ObjectDragged(FInterpEdInputData& InputData)
{
#if WITH_EDITORONLY_DATA
	if(AnimSeqs.IsValidIndex(InputData.InputData) && InputData.TempData)
	{
		FAnimControlTrackKey* OriginalKey = (FAnimControlTrackKey*)InputData.TempData;
		FAnimControlTrackKey &AnimSeq = AnimSeqs[InputData.InputData];
		FIntPoint Delta = InputData.MouseCurrent - InputData.MouseStart;
		float TimeDelta = Delta.X / InputData.PixelsPerSec;
		UAnimSequence* Seq = AnimSeq.AnimSeq;
		if(Seq)
		{
			float ActualLength = (Seq->SequenceLength - (OriginalKey->AnimStartOffset+OriginalKey->AnimEndOffset));
			float ActualLengthScaled =  ActualLength / OriginalKey->AnimPlayRate;
			switch(InputData.InputType)
			{
			case ACDT_AnimBlockLeftEdge:
				
				// If ctrl is down we are scaling play time, otherwise we are clipping.
				if(InputData.bCtrlDown)
				{
					float NewLength = FMath::Max<float>(KINDA_SMALL_NUMBER, (ActualLengthScaled - TimeDelta));
					AnimSeq.AnimPlayRate = FMath::Max<float>(KINDA_SMALL_NUMBER, ActualLength / NewLength);
					AnimSeq.StartTime = OriginalKey->StartTime - (ActualLength / AnimSeq.AnimPlayRate - ActualLengthScaled);
				}
				else if(InputData.bAltDown)
				{
					// We are changing the offset but then scaling the animation proportionately so that the start and end times don't change
					AnimSeq.AnimStartOffset = OriginalKey->AnimStartOffset + TimeDelta * AnimSeq.AnimPlayRate;
					AnimSeq.AnimStartOffset = FMath::Clamp<float>(AnimSeq.AnimStartOffset, 0, Seq->SequenceLength-AnimSeq.AnimEndOffset);

					// Fix the play rate to keep the start and end times the same depending on how much the length of the clip actually changed by.
					float ActualTimeChange = (AnimSeq.AnimStartOffset - OriginalKey->AnimStartOffset) / AnimSeq.AnimPlayRate;
					float NewLength = FMath::Max<float>(KINDA_SMALL_NUMBER, (ActualLengthScaled + ActualTimeChange));
					AnimSeq.AnimPlayRate = FMath::Max<float>(KINDA_SMALL_NUMBER, ActualLength / NewLength);
				}
				else
				{
					AnimSeq.AnimStartOffset = OriginalKey->AnimStartOffset + TimeDelta * AnimSeq.AnimPlayRate;
					AnimSeq.AnimStartOffset = FMath::Clamp<float>(AnimSeq.AnimStartOffset, 0, Seq->SequenceLength-AnimSeq.AnimEndOffset);
					AnimSeq.StartTime = OriginalKey->StartTime + (AnimSeq.AnimStartOffset - OriginalKey->AnimStartOffset) / AnimSeq.AnimPlayRate;
				}
				break;
			case ACDT_AnimBlockRightEdge:

				// If ctrl is down we are scaling play time, otherwise we are clipping.
				if(InputData.bCtrlDown)
				{
					float NewLength = FMath::Max<float>(KINDA_SMALL_NUMBER, (ActualLengthScaled + TimeDelta));
					AnimSeq.AnimPlayRate = FMath::Max<float>(KINDA_SMALL_NUMBER, ActualLength / NewLength);
				}
				else if(InputData.bAltDown)
				{
					// We are changing the offset but then scaling the animation proportionately so that the start and end times don't change
					AnimSeq.AnimEndOffset = OriginalKey->AnimEndOffset - TimeDelta * AnimSeq.AnimPlayRate;
					AnimSeq.AnimEndOffset = FMath::Clamp<float>(AnimSeq.AnimEndOffset, 0, Seq->SequenceLength-AnimSeq.AnimStartOffset);

					// Fix the play rate to keep the start and end times the same depending on how much the length of the clip actually changed by.
					float ActualTimeChange = (AnimSeq.AnimEndOffset - OriginalKey->AnimEndOffset) / AnimSeq.AnimPlayRate;
					float NewLength = FMath::Max<float>(KINDA_SMALL_NUMBER, (ActualLengthScaled + ActualTimeChange));
					AnimSeq.AnimPlayRate = FMath::Max<float>(KINDA_SMALL_NUMBER, ActualLength / NewLength);
				}
				else
				{
					AnimSeq.AnimEndOffset = OriginalKey->AnimEndOffset - TimeDelta * AnimSeq.AnimPlayRate;
					AnimSeq.AnimEndOffset = FMath::Clamp<float>(AnimSeq.AnimEndOffset, 0, Seq->SequenceLength-AnimSeq.AnimStartOffset);
				}
				break;
			}

			// @todo: Support Undo/Redo for drag-based edits
			MarkPackageDirty();
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void UInterpTrackAnimControl::DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params )
{
#if WITH_EDITORONLY_DATA
	UInterpData* Data = CastChecked<UInterpData>(Group->GetOuter());

	const bool bHitTesting = Canvas->IsHitTesting();
	const bool bAllowBarSelection = bHitTesting && Params.bAllowKeyframeBarSelection;
	const bool bAllowTextSelection = bHitTesting && Params.bAllowKeyframeTextSelection;
	
	const FColor NormalBlockColor(0,100,200);
	const FColor ReversedBlockColor(100,50,200);

	// Draw the colored block for each animation.
	FCanvasLineItem LineItem;
	LineItem.SetColor( FLinearColor::Black );
	FString TimeCursorString;
	for(int32 i=0; i<AnimSeqs.Num(); i++)
	{
		const FAnimControlTrackKey& CurKey = AnimSeqs[i];

		float SeqStartTime = CurKey.StartTime;
		float SeqEndTime = SeqStartTime;

		float SeqLength = 0.f;
		UAnimSequence* Seq = CurKey.AnimSeq;
		if(Seq)
		{
			SeqLength = FMath::Max((Seq->SequenceLength - (CurKey.AnimStartOffset + CurKey.AnimEndOffset)) / CurKey.AnimPlayRate, 0.01f);
			SeqEndTime += SeqLength;
		}

		// If there is a sequence following this one - we stop drawing this block where the next one begins.
		float LoopEndTime = SeqEndTime;
		if(i < AnimSeqs.Num()-1)
		{
			LoopEndTime = AnimSeqs[i+1].StartTime;
			SeqEndTime = FMath::Min( AnimSeqs[i+1].StartTime, SeqEndTime );
		}
		else
		{
			LoopEndTime = Data->InterpLength;
		}

		int32 StartPixelPos = FMath::TruncToInt((SeqStartTime - Params.StartTime) * Params.PixelsPerSec);
		int32 EndPixelPos = FMath::TruncToInt((SeqEndTime - Params.StartTime) * Params.PixelsPerSec);

		// Find if this key is one of the selected ones.
		bool bKeySelected = false;
		for(int32 j=0; j<Params.SelectedKeys.Num() && !bKeySelected; j++)
		{
			if( Params.SelectedKeys[j].Group == Group && 
				Params.SelectedKeys[j].Track == this && 
				Params.SelectedKeys[j].KeyIndex == i )
				bKeySelected = true;
		}

		// Draw border orange if animation is selected.
		FColor BorderColor = bKeySelected ? KeySelectedColor : FColor(0,0,0);

		if( Seq && CurKey.bLooping )
		{
			int32 LoopEndPixelPos = FMath::CeilToInt((LoopEndTime - Params.StartTime) * Params.PixelsPerSec);

			if ( bAllowBarSelection )
			{
				Canvas->SetHitProxy( new HInterpTrackKeypointProxy( Group, this, i ) );
			}
			Canvas->DrawTile( StartPixelPos, KeyVertOffset, LoopEndPixelPos - StartPixelPos, FMath::TruncToFloat(Params.TrackHeight - 2.f*KeyVertOffset), 0.f, 0.f, 1.f, 1.f, FColor(0,0,0) );
			Canvas->DrawTile( StartPixelPos+1, KeyVertOffset+1, LoopEndPixelPos - StartPixelPos - 1, FMath::TruncToFloat(Params.TrackHeight - 2.f*KeyVertOffset) - 2, 0.f, 0.f, 1.f, 1.f, FColor(0,75,150) );
			if ( bAllowBarSelection )
			{
				Canvas->SetHitProxy( NULL );
			}
		
			check(CurKey.AnimPlayRate > KINDA_SMALL_NUMBER);
			float LoopTime = SeqEndTime + SeqLength;

			while(LoopTime < LoopEndTime)
			{
				int32 DashPixelPos = FMath::TruncToInt((LoopTime - Params.StartTime) * Params.PixelsPerSec);
				LineItem.Draw( Canvas, FVector2D(DashPixelPos, KeyVertOffset + 2), FVector2D(DashPixelPos, Params.TrackHeight - KeyVertOffset - 2) );
				LoopTime += SeqLength;
			}
		}

		if( bAllowBarSelection )
		{
			Canvas->SetHitProxy( new HInterpTrackKeypointProxy(Group, this, i ) );
		}
		
		// Draw background blocks
		Canvas->DrawTile( StartPixelPos, KeyVertOffset, EndPixelPos - StartPixelPos + 1, FMath::TruncToFloat(Params.TrackHeight - 2.f*KeyVertOffset), 0.f, 0.f, 1.f, 1.f, BorderColor );

		// If the current key is reversed then change the color of the block.
		FColor BlockColor;

		if(CurKey.bReverse)
		{
			BlockColor = ReversedBlockColor;
		}
		else
		{
			BlockColor = NormalBlockColor;
		}

		Canvas->DrawTile( StartPixelPos+1, KeyVertOffset+1, EndPixelPos - StartPixelPos - 1, FMath::TruncToFloat(Params.TrackHeight - 2.f*KeyVertOffset) - 2, 0.f, 0.f, 1.f, 1.f,  BlockColor);

		if ( bAllowBarSelection )
		{
			Canvas->SetHitProxy( NULL );
		}

		// Draw edge hit proxies if we are selected.
		if(bKeySelected)
		{
			// Left Edge
			Canvas->SetHitProxy(new HInterpEdInputInterface(this, FInterpEdInputData(ACDT_AnimBlockLeftEdge, i)));
			Canvas->DrawTile( StartPixelPos-2, KeyVertOffset, 4, FMath::TruncToFloat(Params.TrackHeight - 2.f*KeyVertOffset), 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );

			// Right Edge
			Canvas->SetHitProxy(new HInterpEdInputInterface(this, FInterpEdInputData(ACDT_AnimBlockRightEdge, i)));
			Canvas->DrawTile( EndPixelPos-1, KeyVertOffset, 4, FMath::TruncToFloat(Params.TrackHeight - 2.f*KeyVertOffset), 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );

			Canvas->SetHitProxy(NULL);
		}


		// Check to see if we should draw a positional info about this key next to the time cursor
		if( Params.bShowTimeCursorPosForAllKeys || bKeySelected )
		{
			const float VisibleEndTime = CurKey.bLooping ? LoopEndTime : SeqEndTime;
			if( Params.TimeCursorPosition >= CurKey.StartTime && Params.TimeCursorPosition <= VisibleEndTime )
			{
				const float CursorPosWithinAnim =
					( Params.TimeCursorPosition - CurKey.StartTime ) + CurKey.AnimStartOffset;

				// Does the user want us to draw frame numbers instead of time values?
				if( Params.bPreferFrameNumbers && Params.SnapAmount > KINDA_SMALL_NUMBER )
				{
					// Convert to the animation time values to frame numbers
					const int32 CursorFrameWithinAnim = FMath::TruncToInt( CursorPosWithinAnim / Params.SnapAmount );
					TimeCursorString = FString::Printf( TEXT( "%i" ), CursorFrameWithinAnim );
				}
				else
				{
					TimeCursorString = FString::Printf( TEXT( "%2.2f" ), CursorPosWithinAnim );
				}
			}
		}
	}

	// Use base-class to draw key triangles
	Super::DrawTrack( Canvas, Group, Params );

	// Draw anim sequence name for each block on top.
	for(int32 i=0; i<AnimSeqs.Num(); i++)
	{
		const FAnimControlTrackKey& CurKey = AnimSeqs[i];

		bool bKeySelected = false;
		for(int32 j=0; j<Params.SelectedKeys.Num() && !bKeySelected; j++)
		{
			if( Params.SelectedKeys[j].Group == Group && 
				Params.SelectedKeys[j].Track == this && 
				Params.SelectedKeys[j].KeyIndex == i )
				bKeySelected = true;
		}

		float SeqStartTime = CurKey.StartTime;
		int32 PixelPos = FMath::TruncToInt((SeqStartTime - Params.StartTime) * Params.PixelsPerSec);

		UAnimSequence* Seq = CurKey.AnimSeq;
		FString SeqString = Seq != NULL ? Seq->GetName() : FString::Printf(TEXT("NULL"));

		if(bKeySelected && Seq)
		{
			if(CurKey.AnimStartOffset > 0.f || CurKey.AnimEndOffset > 0.f)
			{
				// Does the user want us to draw frame numbers instead of time values?
				if( Params.bPreferFrameNumbers && Params.SnapAmount > KINDA_SMALL_NUMBER )
				{
					// Convert to the animation time values to frame numbers
					const int32 AnimFrameOffsetFromStart = FMath::RoundToInt( CurKey.AnimStartOffset / Params.SnapAmount );
					const int32 AnimFrameOffsetFromEnd = FMath::RoundToInt( ( Seq->SequenceLength - CurKey.AnimEndOffset ) / Params.SnapAmount );
					SeqString += FString::Printf( TEXT(" (%i->%i)"), AnimFrameOffsetFromStart, AnimFrameOffsetFromEnd );
				}
				else
				{
					SeqString += FString::Printf( TEXT(" (%2.2f->%2.2f)"), CurKey.AnimStartOffset, Seq->SequenceLength - CurKey.AnimEndOffset );
				}
			}

			if(CurKey.AnimPlayRate != 1.f)
			{
				SeqString += FString::Printf( TEXT(" x%2.2f"), CurKey.AnimPlayRate );
			}

			if(CurKey.bReverse)
			{
				SeqString += NSLOCTEXT("UnrealEd", "Reverse", "Reverse").ToString();
			}
		}

		int32 XL, YL;
		StringSize( GEngine->GetSmallFont(), XL, YL, *SeqString );

		if ( bAllowTextSelection )
		{
			Canvas->SetHitProxy( new HInterpTrackKeypointProxy( Group, this, i ) );
		}
		Canvas->DrawShadowedString( PixelPos + 2, Params.TrackHeight - YL - KeyVertOffset, *SeqString, GEngine->GetSmallFont(), KeyLabelColor );
		if ( bAllowTextSelection )
		{
			Canvas->SetHitProxy( NULL );
		}
	}


	// Draw the time cursor's position relative to the start of this animation.  We'll draw this
	// right next to the time cursor, on top of this anim track
	if( TimeCursorString.Len() > 0 )
	{
		// Visual settings
		const FLinearColor BackgroundColor( 0.0f, 0.015f, 0.05f, 0.75f );
		const FLinearColor BorderColor( 0.35f, 0.35f, 0.4f, 1.0f );	// NOTE: Opacity is ignored for lines
		const FColor TextColor( 255, 255, 255 );
		const float TextScale = 0.9f;
		const int32 TextHorizOffset = 6;
		const int32 TextVertOffset = 6;


		const int32 TimeCursorPixelPos =
			FMath::TruncToInt( ( Params.TimeCursorPosition - Params.StartTime ) * Params.PixelsPerSec );

		int32 XL, YL;
		StringSize( GEngine->GetTinyFont(), XL, YL, *TimeCursorString );
		const float TextWidth = XL * TextScale;
		const float TextHeight = YL * TextScale;

		// Draw background
		const float BoxTop = TextVertOffset -( TextHeight + 2 );
		const float BoxLeft = TimeCursorPixelPos + TextHorizOffset - 2;
		Canvas->DrawTile(
			BoxLeft,
			BoxTop,
			TextWidth + 4,
			TextHeight + 3,
			0.0f, 0.0f,
			1.0f, 1.0f,
			BackgroundColor );

		// Draw border
		FCanvasBoxItem BoxItem( FVector2D( BoxLeft, BoxTop ), FVector2D( TextWidth + 4, TextHeight + 3 ) );
		BoxItem.SetColor( BorderColor );
		Canvas->DrawItem( BoxItem );
// 		Canvas->DrawBox2D(
// 			FVector2D( BoxLeft, BoxTop ),
// 			FVector2D( BoxLeft + TextWidth + 4, BoxTop + TextHeight + 3 ),
// 			BorderColor );

		// Draw text
		FCanvasTextItem TextItem( FVector2D( TimeCursorPixelPos + TextHorizOffset, TextVertOffset - TextHeight ), FText::FromString( TimeCursorString ), GEngine->GetTinyFont(), TextColor );
		TextItem.Scale = FVector2D(TextScale,TextScale);
		Canvas->DrawItem( TextItem );		
	}
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackAnimControl::GetTrackIcon() const
{
	return TrackIcon;
}
#endif // WITH_EDITORONLY_DATA

/*-----------------------------------------------------------------------------
	UInterpTrackSound
-----------------------------------------------------------------------------*/

void UInterpTrackSound::DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params )
{
#if WITH_EDITORONLY_DATA
	UInterpData* Data = CastChecked<UInterpData>(Group->GetOuter());

	const bool bHitTesting = Canvas->IsHitTesting();
	const bool bAllowBarSelection = bHitTesting && Params.bAllowKeyframeBarSelection;
	const bool bAllowTextSelection = bHitTesting && Params.bAllowKeyframeTextSelection;

	// Draw the coloured block for each sound.
	for (int32 i = 0; i < Sounds.Num(); i++)
	{
		float SoundStartTime = Sounds[i].Time;
		float SoundEndTime = SoundStartTime;

		// Make block as long as the Sound is.
		USoundBase* Sound = Sounds[i].Sound;
		if (bPlayOnReverse)
		{
			if (Sound != NULL)
			{
				SoundEndTime -= Sound->GetDuration();
			}
			if (i > 0)
			{
				SoundEndTime = FMath::Max(Sounds[i - 1].Time, SoundEndTime);
			}
		}
		else
		{
			if (Sound != NULL)
			{
				SoundEndTime += Sound->GetDuration();
			}

			// Truncate sound at next sound in the track.
			if (i < Sounds.Num() - 1)
			{
				SoundEndTime = FMath::Min( Sounds[i+1].Time, SoundEndTime );
			}
		}

		int32 StartPixelPos = FMath::TruncToInt((SoundStartTime - Params.StartTime) * Params.PixelsPerSec);
		int32 EndPixelPos = FMath::TruncToInt((SoundEndTime - Params.StartTime) * Params.PixelsPerSec);

		// Find if this sound is one of the selected ones.
		bool bKeySelected = false;
		for (int32 j = 0; j < Params.SelectedKeys.Num() && !bKeySelected; j++)
		{
			if( Params.SelectedKeys[j].Group == Group && 
				Params.SelectedKeys[j].Track == this && 
				Params.SelectedKeys[j].KeyIndex == i )
				bKeySelected = true;
		}

		// Draw border orange if sound is selected.
		FColor BorderColor = bKeySelected ? KeySelectedColor : FColor(0,0,0);

		if( bAllowBarSelection ) 
		{
			Canvas->SetHitProxy( new HInterpTrackKeypointProxy( Group, this, i ) );
		}
		Canvas->DrawTile( StartPixelPos, KeyVertOffset, EndPixelPos - StartPixelPos + 1, FMath::TruncToFloat(Params.TrackHeight - 2.f*KeyVertOffset), 0.f, 0.f, 1.f, 1.f, BorderColor );
		Canvas->DrawTile( StartPixelPos+1, KeyVertOffset+1, EndPixelPos - StartPixelPos - 1, FMath::TruncToFloat(Params.TrackHeight - 2.f*KeyVertOffset) - 2, 0.f, 0.f, 1.f, 1.f, FColor(0,200,100) );
		if( bAllowBarSelection )
		{
			Canvas->SetHitProxy( NULL );
		}
	}

	// Use base-class to draw key triangles
	Super::DrawTrack( Canvas, Group, Params );

	// Draw sound cue name for each block on top.
	for (int32 i = 0; i < Sounds.Num(); i++)
	{
		float SoundStartTime = Sounds[i].Time;
		int32 PixelPos = FMath::TruncToInt((SoundStartTime - Params.StartTime) * Params.PixelsPerSec);

		USoundBase* Sound = Sounds[i].Sound;
	
		FString SoundString( TEXT("None") );
		if(Sound)
		{
			SoundString = FString( *Sound->GetName() );
			if ( Sounds[i].Volume != 1.0f )
			{
				SoundString += FString::Printf( TEXT(" v%2.2f"), Sounds[i].Volume );
			}
			if ( Sounds[i].Pitch != 1.0f )
			{
				SoundString += FString::Printf( TEXT(" p%2.2f"), Sounds[i].Pitch );
			}
		}
		
		int32 XL, YL;
		StringSize( GEngine->GetSmallFont(), XL, YL, *SoundString );

		if ( bAllowTextSelection )
		{
			Canvas->SetHitProxy( new HInterpTrackKeypointProxy( Group, this, i ) );
		}
		Canvas->DrawShadowedString( bPlayOnReverse ? (PixelPos - 2 - XL) : (PixelPos + 2), Params.TrackHeight - YL - KeyVertOffset, *SoundString, GEngine->GetSmallFont(), KeyLabelColor );
		if ( bAllowTextSelection )
		{
			Canvas->SetHitProxy( NULL );
		}
	}
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackSound::GetTrackIcon() const
{
	return TrackIcon;
}
#endif // WITH_EDITORONLY_DATA

/*-----------------------------------------------------------------------------
	UInterpTrackFade
-----------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackFade::GetTrackIcon() const
{
	return TrackIcon;
}
#endif // WITH_EDITORONLY_DATA


/*-----------------------------------------------------------------------------
	UInterpTrackSlomo
-----------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackSlomo::GetTrackIcon() const
{
	return TrackIcon;
}
#endif // WITH_EDITORONLY_DATA

/*-----------------------------------------------------------------------------
	UInterpTrackColorScale
-----------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackColorScale::GetTrackIcon() const
{
	return TrackIcon;
}
#endif // WITH_EDITORONLY_DATA


/*-----------------------------------------------------------------------------
	UInterpTrackAudioMaster
-----------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackAudioMaster::GetTrackIcon() const
{
	return TrackIcon;
}
#endif // WITH_EDITORONLY_DATA



/*-----------------------------------------------------------------------------
	UInterpTrackVisibility
-----------------------------------------------------------------------------*/
#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackVisibility::GetTrackIcon() const
{
	return TrackIcon;
}
#endif // WITH_EDITORONLY_DATA

void UInterpTrackVisibility::DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params )
{
#if WITH_EDITORONLY_DATA
	int32 NumKeys = GetNumKeyframes();

	const bool bHitTesting = Canvas->IsHitTesting();
	const bool bAllowBarSelection = bHitTesting && Params.bAllowKeyframeBarSelection;

	// Draw the 'on' blocks in green
	int32 LastPixelPos = -1;
	bool bLastPosWasOn = false;
	for (int32 i=0; i<NumKeys; i++)
	{
		float KeyTime = GetKeyframeTime(i);
		int32 PixelPos = FMath::TruncToInt((KeyTime - Params.StartTime) * Params.PixelsPerSec);

		FVisibilityTrackKey& Key = VisibilityTrack[i];
		if ((Key.Action == EVTA_Hide) && bLastPosWasOn)
		{
			if ( bAllowBarSelection )
			{
				Canvas->SetHitProxy( new HInterpTrackKeypointProxy( Group, this, i ) );
			}
			Canvas->DrawTile( LastPixelPos, KeyVertOffset, PixelPos - LastPixelPos, Params.TrackHeight - (2 * KeyVertOffset), 
				0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(0.0f, 1.0f, 0.0f));
			if ( bAllowBarSelection )
			{
				Canvas->SetHitProxy( NULL );
			}
		}

		LastPixelPos = PixelPos;
		bLastPosWasOn = (Key.Action == EVTA_Show) ? true : false;
	}

	FCanvasTriangleItem TriItem( FVector2D( 0.0f, 0.0f ), FVector2D( 0.0f, 0.0f ), FVector2D( 0.0f, 0.0f ), GWhiteTexture );
	// Draw the keyframe points after, so they are on top
	for (int32 i=0; i<NumKeys; i++)
	{
		float KeyTime = GetKeyframeTime(i);

		int32 PixelPos = FMath::TruncToInt((KeyTime - Params.StartTime) * Params.PixelsPerSec);

		FIntPoint A, A_Offset;
		FIntPoint B, B_Offset;
		FIntPoint C, C_Offset;

		FVisibilityTrackKey& Key = VisibilityTrack[i];
		if (Key.Action == EVTA_Hide)
		{
			// Point the triangle down...
			A = FIntPoint(PixelPos - KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset - KeyHalfTriSize);
			B = FIntPoint(PixelPos + KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset - KeyHalfTriSize);
			C = FIntPoint(PixelPos,						Params.TrackHeight - KeyVertOffset);

			A_Offset = FIntPoint(-2,-2);
			B_Offset = FIntPoint( 2,-2);
			C_Offset = FIntPoint( 0, 1);
		}
		else if (Key.Action == EVTA_Toggle)
		{
			// Point the triangle up
			A = FIntPoint(PixelPos - KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset);
			B = FIntPoint(PixelPos + KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset);
			C = FIntPoint(PixelPos,						Params.TrackHeight - KeyVertOffset - KeyHalfTriSize);

			A_Offset = FIntPoint(-2, 1);
			B_Offset = FIntPoint( 2, 1);
			C_Offset = FIntPoint( 0,-2);

			if ( bAllowBarSelection )
			{
				Canvas->SetHitProxy( new HInterpTrackKeypointProxy( Group, this, i ) );
			}
			Canvas->DrawTile( PixelPos - 4, KeyVertOffset, 7, Params.TrackHeight - (2 * KeyVertOffset), 
				0.0f, 0.0f, 0.0f, 0.0f, FLinearColor(1.0f, 0.0f, 0.0f));
			if ( bAllowBarSelection )
			{
				Canvas->SetHitProxy( NULL );
			}
		}
		else
		{
			// Point the triangle up
			A = FIntPoint(PixelPos - KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset);
			B = FIntPoint(PixelPos + KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset);
			C = FIntPoint(PixelPos,						Params.TrackHeight - KeyVertOffset - KeyHalfTriSize);

			A_Offset = FIntPoint(-2, 1);
			B_Offset = FIntPoint( 2, 1);
			C_Offset = FIntPoint( 0,-2);
		}

		bool bKeySelected = false;
		for (int32 j=0; j<Params.SelectedKeys.Num() && !bKeySelected; j++)
		{
			if (Params.SelectedKeys[j].Group == Group && 
				Params.SelectedKeys[j].Track == this && 
				Params.SelectedKeys[j].KeyIndex == i)
			{
				bKeySelected = true;
			}
		}

		FColor KeyColor = GetKeyframeColor(i);
		
		if ( bHitTesting )
		{
			Canvas->SetHitProxy( new HInterpTrackKeypointProxy(Group, this, i) );
		}
		
		TriItem.SetPoints( A + A_Offset, B + B_Offset, C + C_Offset );
		if(bKeySelected)
		{
			TriItem.SetColor( KeySelectedColor );
			Canvas->DrawItem( TriItem );
		}
		TriItem.SetPoints( A, B, C );
		TriItem.SetColor( KeyColor );
		Canvas->DrawItem( TriItem );
		
		if ( bHitTesting )
		{
			Canvas->SetHitProxy( NULL );
		}
	}
#endif // WITH_EDITORONLY_DATA
}


/*-----------------------------------------------------------------------------
	UInterpTrackParticleReplay
-----------------------------------------------------------------------------*/
#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackParticleReplay::GetTrackIcon() const
{
	return (UTexture2D*)StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_ParticleReplay.MAT_Groups_ParticleReplay"), NULL, LOAD_None, NULL );
}
#endif // WITH_EDITORONLY_DATA


void UInterpTrackParticleReplay::BeginDrag(FInterpEdInputData &InputData)
{
	// Store temporary data.
	if((InputData.InputType == PRDT_LeftEdge || InputData.InputType == PRDT_RightEdge) && TrackKeys.IsValidIndex(InputData.InputData))
	{
		// Store our starting position.
		FParticleReplayTrackKey* SavedKey = new FParticleReplayTrackKey();
		*SavedKey = TrackKeys[ InputData.InputData ];
		InputData.TempData = SavedKey;
	}
}

void UInterpTrackParticleReplay::EndDrag(FInterpEdInputData &InputData)
{
	// Clean up our temporary data.
	if(InputData.TempData)
	{
		FParticleReplayTrackKey* InterpKey = (FParticleReplayTrackKey*)InputData.TempData;
		delete InterpKey;
		InputData.TempData = NULL;
	}
}

EMouseCursor::Type UInterpTrackParticleReplay::GetMouseCursor(FInterpEdInputData &InputData)
{
	EMouseCursor::Type Result = EMouseCursor::Default;

	switch(InputData.InputType)
	{
		case PRDT_LeftEdge:
		case PRDT_RightEdge:
			Result = EMouseCursor::ResizeLeftRight;
			break;
	}

	return Result;
}

void UInterpTrackParticleReplay::ObjectDragged(FInterpEdInputData& InputData)
{
#if WITH_EDITORONLY_DATA
	if(TrackKeys.IsValidIndex(InputData.InputData) && InputData.TempData)
	{
		const FParticleReplayTrackKey& OriginalKey = *(FParticleReplayTrackKey*)InputData.TempData;
		FParticleReplayTrackKey& SelectedKey = TrackKeys[InputData.InputData];
		FIntPoint Delta = InputData.MouseCurrent - InputData.MouseStart;
		float TimeDelta = Delta.X / InputData.PixelsPerSec;
		{
			switch(InputData.InputType)
			{
				case PRDT_LeftEdge:
					{
						SelectedKey.Time = OriginalKey.Time + TimeDelta;

						// Snap the new time position
						if( FixedTimeStep > SMALL_NUMBER )
						{
							const int32 InterpPositionInFrames = FMath::RoundToInt( SelectedKey.Time / FixedTimeStep );
							SelectedKey.Time = InterpPositionInFrames * FixedTimeStep;
						}

						if( SelectedKey.Time > OriginalKey.Time + OriginalKey.Duration )
						{
							SelectedKey.Time = OriginalKey.Time + OriginalKey.Duration;
						}
						float NewDelta = SelectedKey.Time - OriginalKey.Time;

						SelectedKey.Duration = OriginalKey.Duration - NewDelta;
						if( SelectedKey.Duration < 0.0f )
						{
							SelectedKey.Duration = 0.0f;
						}
					}
					break;

				case PRDT_RightEdge:
					{
						SelectedKey.Duration = OriginalKey.Duration + TimeDelta;
						if( SelectedKey.Duration < 0.0f )
						{
							SelectedKey.Duration = 0.0f;
						}

						// Snap the new end position
						if( FixedTimeStep > SMALL_NUMBER )
						{
							float EndTime = SelectedKey.Time + SelectedKey.Duration;

							const int32 InterpPositionInFrames = FMath::RoundToInt( EndTime / FixedTimeStep );
							EndTime = InterpPositionInFrames * FixedTimeStep;

							if( EndTime < SelectedKey.Time )
							{
								EndTime = SelectedKey.Time;
							}
							SelectedKey.Duration = EndTime - SelectedKey.Time;
						}
					}
					break;
			}
		}

		// @todo: Support Undo/Redo for drag-based edits
		MarkPackageDirty();
	}
#endif // WITH_EDITORONLY_DATA
}


void UInterpTrackParticleReplay::DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params )
{
#if WITH_EDITORONLY_DATA
	const bool bHitTesting = Canvas->IsHitTesting();
	const bool bAllowBarSelection = bHitTesting && Params.bAllowKeyframeBarSelection;
	const bool bAllowTextSelection = bHitTesting && Params.bAllowKeyframeTextSelection;
	FCanvasTriangleItem TriItem( FVector2D( 0.0f, 0.0f ), FVector2D( 0.0f, 0.0f ), FVector2D( 0.0f, 0.0f ), GWhiteTexture );
	const int32 NumKeys = GetNumKeyframes();
	for( int32 CurKeyIndex = 0; CurKeyIndex < NumKeys; ++CurKeyIndex )
	{
		const FParticleReplayTrackKey& CurKey = TrackKeys[ CurKeyIndex ];

		float KeyTime = GetKeyframeTime( CurKeyIndex );

		const int32 StartPixelPos = FMath::TruncToInt( ( KeyTime - Params.StartTime ) * Params.PixelsPerSec );
		const int32 EndPixelPos = FMath::TruncToInt( ( KeyTime - Params.StartTime + CurKey.Duration ) * Params.PixelsPerSec );


		// Is this key selected?
		bool bKeySelected = false;
		for (int32 j=0; j<Params.SelectedKeys.Num() && !bKeySelected; j++)
		{
			if( Params.SelectedKeys[j].Group == Group && 
				Params.SelectedKeys[j].Track == this && 
				Params.SelectedKeys[j].KeyIndex == CurKeyIndex )
			{
				bKeySelected = true;
			}
		}


		// Draw background tile for the capture/playback range
		{
			FColor BackgroundTileColor = bKeySelected ? KeySelectedColor : FColor(80,00,80);
			if( bIsCapturingReplay )
			{
				// When capturing make the background color more red
				BackgroundTileColor.R = 200;
			}
			
			if( bAllowBarSelection )
			{
				Canvas->SetHitProxy( new HInterpTrackKeypointProxy( Group, this, CurKeyIndex ) );
			}

			Canvas->DrawTile(
				StartPixelPos,
				KeyVertOffset,
				EndPixelPos - StartPixelPos + 1,
				FMath::TruncToFloat( Params.TrackHeight - 2.0f * KeyVertOffset ),
				0.f, 0.f, 1.f, 1.f,
				BackgroundTileColor );

			if ( bAllowBarSelection )
			{
				Canvas->SetHitProxy( NULL );
			}
		}


		// Draw edge hit proxies if we are selected.
		if( bKeySelected )
		{
			// Left Edge
			Canvas->SetHitProxy(new HInterpEdInputInterface(this, FInterpEdInputData(PRDT_LeftEdge, CurKeyIndex)));
			Canvas->DrawTile( StartPixelPos-2, KeyVertOffset, 4, FMath::TruncToFloat(Params.TrackHeight - 2.f*KeyVertOffset), 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );

			// Right Edge
			Canvas->SetHitProxy(new HInterpEdInputInterface(this, FInterpEdInputData(PRDT_RightEdge, CurKeyIndex)));
			Canvas->DrawTile( EndPixelPos-1, KeyVertOffset, 4, FMath::TruncToFloat(Params.TrackHeight - 2.f*KeyVertOffset), 0.f, 0.f, 1.f, 1.f, FLinearColor::Black );

			Canvas->SetHitProxy(NULL);
		}


		// Draw key frame information text
		{
			const int32 DurationInFrames = FMath::RoundToInt( CurKey.Duration / FixedTimeStep );
			FString InfoText = FString::Printf( TEXT( "[Clip %i] %i frames (%.2f s)" ), CurKey.ClipIDNumber, DurationInFrames, CurKey.Duration );

			int32 XL, YL;
			StringSize( GEngine->GetSmallFont(), XL, YL, *InfoText );

			if ( bAllowTextSelection )
			{
				Canvas->SetHitProxy( new HInterpTrackKeypointProxy( Group, this, CurKeyIndex ) );
			}
			Canvas->DrawShadowedString( StartPixelPos + 2, Params.TrackHeight - YL - KeyVertOffset, *InfoText, GEngine->GetSmallFont(), KeyLabelColor );
			if ( bAllowTextSelection )
			{
				Canvas->SetHitProxy( NULL );
			}
		}


		// Draw key frame triangle (selectable)
		{
			FIntPoint A, A_Offset;
			FIntPoint B, B_Offset;
			FIntPoint C, C_Offset;

			A = FIntPoint(StartPixelPos - KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset);
			B = FIntPoint(StartPixelPos + KeyHalfTriSize,	Params.TrackHeight - KeyVertOffset);
			C = FIntPoint(StartPixelPos,					Params.TrackHeight - KeyVertOffset - KeyHalfTriSize);

			A_Offset = FIntPoint(-2, 1);
			B_Offset = FIntPoint( 2, 1);
			C_Offset = FIntPoint( 0,-2);

			FColor KeyColor = GetKeyframeColor( CurKeyIndex );
			
			if( bHitTesting )
			{
				Canvas->SetHitProxy( new HInterpTrackKeypointProxy(Group, this, CurKeyIndex) );
			}
			
			TriItem.SetPoints( A + A_Offset, B + B_Offset, C + C_Offset );
			if(bKeySelected)
			{
				TriItem.SetColor( KeySelectedColor );
				Canvas->DrawItem( TriItem );
			}
			TriItem.SetPoints( A, B, C );
			TriItem.SetColor( KeyColor );
			Canvas->DrawItem( TriItem );

			if( bHitTesting )
			{
				Canvas->SetHitProxy( NULL );
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

