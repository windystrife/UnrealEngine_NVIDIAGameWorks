// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SnappingUtils.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/Actor.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Editor/GroupActor.h"
#include "GameFramework/PhysicsVolume.h"
#include "Engine/PostProcessVolume.h"
#include "GameFramework/WorldSettings.h"
#include "EngineUtils.h"
#include "LevelEditorViewport.h"
#include "Engine/Selection.h"
#include "EditorModeManager.h"
#include "ScopedTransaction.h"
#include "EdMode.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "VertexSnapping.h"
#include "ISnappingPolicy.h"
#include "ViewportSnappingModule.h"
#include "ActorGroupingUtils.h"

//////////////////////////////////////////////////////////////////////////
// FEditorViewportSnapping

class FEditorViewportSnapping : public ISnappingPolicy
{
public:
	// FEditorViewportSnapping interface
	virtual void SnapScale(FVector& Point, const FVector& GridBase) override;
	virtual void SnapPointToGrid(FVector& Point, const FVector& GridBase) override;
	virtual void SnapRotatorToGrid(FRotator& Rotation) override;
	virtual void ClearSnappingHelpers(bool bClearImmediately = false) override;
	virtual void DrawSnappingHelpers(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	// End of FEditorViewportSnapping interface

	bool IsSnapToGridEnabled();
	bool IsSnapRotationEnabled();
	bool IsSnapScaleEnabled();

	/**
	 * @return true if snapping to vertices is enabled
	 */
	bool IsSnapToVertexEnabled();

	/**
	 * @return true if snapping actors to other actors is enabled
	 */
	bool IsSnapToActorEnabled();

	/** Set user setting for actor snap. */
	void EnableActorSnap(bool bEnable);

	/** Access user setting for distance. Fractional 0.0->100.0 */
	float GetActorSnapDistance(bool bScalar = false);

	/** Set user setting for distance. Fractional 0.0->100.0 */
	void SetActorSnapDistance(float Distance);

	/**
	 * Attempts to snap the selected actors to the nearest other actor
	 *
	 * @param DragDelta			The current world space drag amount
	 * @param ViewportClient	The viewport client the user is dragging in
	 */
	bool SnapActorsToNearestActor( FVector& DragDelta, FLevelEditorViewportClient* ViewportClient );

	/**
	 * Snaps actors to the nearest vertex on another actor
	 *
	 * @param DragDelta			The current world space drag amount that will be modified to account for snapping to a vertex
	 * @param ViewportClient	The viewport client the user is dragging in
	 * @return true if anything was snapped
	 */
	bool SnapDraggedActorsToNearestVertex( FVector& DragDelta, FLevelEditorViewportClient* ViewportClient );

	/**
	 * Snaps a delta drag movement to the nearest vertex
	 *
	 * @param BaseLocation		Location that should be snapped before any drag is applied
	 * @param DragDelta			Delta drag movement that should be snapped.  This value will be updated such that BaseLocation+DragDelta is the nearest snapped verted
	 * @param ViewportClient	The viewport client being dragged in.
	 * @return true if anything was snapped
	 */
	bool SnapDragLocationToNearestVertex( const FVector& BaseLocation, FVector& DragDelta, FLevelEditorViewportClient* ViewportClient );

	/**
	 * Snaps a location to the nearest vertex
	 *
	 * @param Location			The location to snap
	 * @param MouseLocation		The current 2d mouse location.  Vertices closer to the mouse are favored
	 * @param ViewportClient	The viewport client being used
	 * @param OutVertexNormal	The normal at the closest vertex
	 * @return true if anything was snapped
	 */
	bool SnapLocationToNearestVertex( FVector& Location, const FVector2D& MouseLocation, FLevelEditorViewportClient* ViewportClient, FVector& OutVertexNormal, bool bDrawVertHelpers );

	bool SnapToBSPVertex( FVector& Location, FVector GridBase, FRotator& Rotation );

private:

	/** Vertex snapping implementation */
	FVertexSnappingImpl VertexSnappingImpl;
};

//////////////////////////////////////////////////////////////////////////
// FEditorViewportSnapping

bool FEditorViewportSnapping::IsSnapToGridEnabled()
{
	return GetDefault<ULevelEditorViewportSettings>()->GridEnabled && !IsSnapToVertexEnabled();
}

bool FEditorViewportSnapping::IsSnapRotationEnabled()
{	
	// Ask Current Editor Mode if Rotation Snap is enabled
	bool bSnapEnabled = false;
	TArray<FEdMode*> ActiveModes; 
	GLevelEditorModeTools().GetActiveModes( ActiveModes );
	for( int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bSnapEnabled |= ActiveModes[ModeIndex]->IsSnapRotationEnabled();
	}
	return bSnapEnabled;
}

bool FEditorViewportSnapping::IsSnapScaleEnabled()
{
	return GetDefault<ULevelEditorViewportSettings>()->SnapScaleEnabled;
}

bool FEditorViewportSnapping::IsSnapToVertexEnabled()
{
	if( GetDefault<ULevelEditorViewportSettings>()->bSnapVertices )
	{
		return true;
	}
	else if( GCurrentLevelEditingViewportClient )
	{
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
		const FLevelEditorCommands& Commands = LevelEditor.GetLevelEditorCommands();
		bool bIsChordPressed = false;
		for (uint32 i = 0; i < static_cast<uint8>(EMultipleKeyBindingIndex::NumChords); ++i)
		{
			EMultipleKeyBindingIndex ChordIndex = static_cast<EMultipleKeyBindingIndex> (i);
			const FInputChord& Chord = *Commands.HoldToEnableVertexSnapping->GetActiveChord(ChordIndex);

			bIsChordPressed |= (Chord.NeedsControl() == GCurrentLevelEditingViewportClient->IsCtrlPressed())
				&& (Chord.NeedsAlt() == GCurrentLevelEditingViewportClient->IsAltPressed())
				&& (Chord.NeedsShift() == GCurrentLevelEditingViewportClient->IsShiftPressed())
				&& GCurrentLevelEditingViewportClient->Viewport->KeyState(Chord.Key) == true;
		}
		return bIsChordPressed;
	}
	else
	{
		return false;
	}
}

bool FEditorViewportSnapping::IsSnapToActorEnabled()
{
	return GetDefault<ULevelEditorViewportSettings>()->bEnableActorSnap && !IsSnapToVertexEnabled();
}

void FEditorViewportSnapping::EnableActorSnap(bool bEnable)
{
	ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
	ViewportSettings->bEnableActorSnap = bEnable;
	ViewportSettings->PostEditChange();
}

float FEditorViewportSnapping::GetActorSnapDistance(bool bScalar)
{
	ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();

	// If the user has purposefully exceeded the maximum scale, convert this to the range so that it can be more easily adjusted in the editor
	if (ViewportSettings->ActorSnapScale > 1.0f)
	{
		ViewportSettings->ActorSnapDistance *= ViewportSettings->ActorSnapScale;
		ViewportSettings->ActorSnapScale = 1.0f;
		ViewportSettings->PostEditChange();
	}

	if (bScalar)
	{
		// Clamp to within range (just so slider looks correct)
		return FMath::Clamp(ViewportSettings->ActorSnapScale, 0.0f, 1.0f);
	}

	// Multiply by the max distance allowed to convert to range
	return FMath::Max(0.0f, ViewportSettings->ActorSnapScale) * ViewportSettings->ActorSnapDistance;
}

void FEditorViewportSnapping::SetActorSnapDistance(float Distance)
{
	ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
	ViewportSettings->ActorSnapScale = Distance;
	ViewportSettings->PostEditChange();
}

bool FEditorViewportSnapping::SnapActorsToNearestActor( FVector& Drag, FLevelEditorViewportClient* ViewportClient )
{
	FEditorModeTools& Tools = GLevelEditorModeTools();

	// Does the user have actor snapping enabled?
	bool bSnapped = false;
	if ( IsSnapToActorEnabled() )
	{		
		// Are there selected actors?
		USelection* Selection = GEditor->GetSelectedActors();
		if ( Selection->Num() > 0 )
		{
			// Nearest results
			const AActor* BestActor = NULL;
			FVector BestPoint = FVector::ZeroVector;
			float BestSqrdDist = 0.0f;

			// Find the nearest actor to the pivot point that isn't part of the selection
			const FVector PivotLocation = Tools.PivotLocation;
			for (FActorIterator It(ViewportClient->GetWorld()); It; ++It)	// Actor iterator :( [Note: Also, can't use BoxOverlapMulti to as the pivot may lie outside the bounds of the actor!]
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );

				// Make sure this isn't an invalid actor type or one of the selected actors
				const FString tmp = Actor->GetActorLabel();
				if ( !Actor->IsA(AWorldSettings::StaticClass()) 
					&& !Actor->IsA(APhysicsVolume::StaticClass()) 
					&& !Actor->IsA(APostProcessVolume::StaticClass()) 
					&& !Selection->IsSelected( Actor ) )
				{
					// Group Actors don't appear in the selected actors list!
					if (UActorGroupingUtils::IsGroupingActive())
					{
						// Valid snaps: locked groups (not self or actors within locked groups), actors within unlocked groups (not the group itself), other actors
						const AGroupActor* GroupActor = Cast<AGroupActor>( Actor ); // AGroupActor::GetRootForActor( Actor );
						if ( GroupActor && ( !GroupActor->IsLocked() || GroupActor->HasSelectedActors() ) )
						{
							continue;
						}
					}

					// Is this the nearest actor to the pivot?
					const FVector Point = Actor->GetActorLocation();
					const float SqrdDist = FVector::DistSquared( PivotLocation, Point );
					if ( BestActor == NULL || SqrdDist < BestSqrdDist )
					{
						BestActor = Actor;
						BestPoint = Point;
						BestSqrdDist = SqrdDist;
					}
				}
			}

			// Did we find an actor?
			const FString tmp = BestActor ? BestActor->GetActorLabel() : TEXT( "None" );
			if ( BestActor )
			{				
				// Are we within the threshold or exitting it?
				const float Dist = GetActorSnapDistance();
				if ( BestSqrdDist < FMath::Square( Dist )  )
				{
					bSnapped = true;

					// Are we no already snapped, or is it different to our current location
					if ( !Tools.SnappedActor || !Tools.CachedLocation.Equals( BestPoint ) )
					{
						// Calculate the delta between the snapped location and the current pivot and apply to all the selected actors
						const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SnapActorsToActor", "Snap Actors To Actor") );
						const FVector PivotDelta = ( BestPoint - PivotLocation );
						ViewportClient->ApplyDeltaToActors( PivotDelta, FRotator::ZeroRotator, FVector::ZeroVector );
						Tools.SetPivotLocation( BestPoint, false );	// Overwrite the location for next time we check
						Drag = FVector::ZeroVector;	// Reset the drag so the pivot doesn't jump
					}
				}
				else if ( Tools.SnappedActor && !Tools.CachedLocation.Equals( PivotLocation ) )
				{
					const FVector PivotDelta = ( PivotLocation - BestPoint );
					ViewportClient->ApplyDeltaToActors( PivotDelta, FRotator::ZeroRotator, FVector::ZeroVector );
					//GUnrealEd->UpdatePivotLocationForSelection();	// Calling this ends up forcing the pivot back inside the threshold?!
					Tools.SetPivotLocation( PivotLocation, false );	// Overwrite the location for next time we check
					Drag = FVector::ZeroVector;	// Reset the drag so the pivot doesn't jump
				}
			}
		}
	}

	Tools.SnappedActor = bSnapped;
	return bSnapped;	// Whether or not the selection is snapped in place
}

void FEditorViewportSnapping::SnapPointToGrid(FVector& Point, const FVector& GridBase)
{
	if( IsSnapToGridEnabled() )
	{
		Point = (Point - GridBase).GridSnap( GEditor->GetGridSize() ) + GridBase;
	}
}

void FEditorViewportSnapping::SnapRotatorToGrid(FRotator& Rotation)
{
	if( IsSnapRotationEnabled() )
	{		
		TArray<FEdMode*> ActiveModes; 
		GLevelEditorModeTools().GetActiveModes( ActiveModes );
		for( int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
		{
			if( ActiveModes[ModeIndex]->SnapRotatorToGridOverride( Rotation ) == true )
			{
				return;
			}
		}

		Rotation = Rotation.GridSnap( GEditor->GetRotGridSize() );		
	}
}

void FEditorViewportSnapping::SnapScale(FVector& Point, const FVector& GridBase)
{
	if( IsSnapScaleEnabled() )
	{
		if( GEditor->UsePercentageBasedScaling() )
		{
			Point = (Point - GridBase).GridSnap( GEditor->GetGridSize() ) + GridBase;
		}
		else
		{
			if (GetDefault<ULevelEditorViewportSettings>()->PreserveNonUniformScale)
			{
				// when using 'auto-precision', we take the max component & snap its scale, then proportionally scale the other components
				float MaxComponent = Point.GetAbsMax();
				if(MaxComponent == 0.0f)
				{
					MaxComponent = 1.0f;
				}
				const float SnappedMaxComponent = FMath::GridSnap(MaxComponent, GEditor->GetScaleGridSize());
				Point = Point * (SnappedMaxComponent / MaxComponent);
			}
			else
			{
				Point = Point.GridSnap( GEditor->GetScaleGridSize() );
			}
		}
	}
}


bool FEditorViewportSnapping::SnapToBSPVertex(FVector& Location, FVector GridBase, FRotator& Rotation)
{
	bool bSnapped = false;
	SnapRotatorToGrid( Rotation );
	if( IsSnapToVertexEnabled() )
	{
		FVector	DestPoint;
		int32 Temp;
		if( GWorld->GetModel()->FindNearestVertex( Location, DestPoint, GetDefault<ULevelEditorViewportSettings>()->SnapDistance, Temp ) >= 0.0)
		{
			Location = DestPoint;
			bSnapped = true;
		}
	}

	if( !bSnapped )
	{
		SnapPointToGrid( Location, GridBase );
	}

	return bSnapped;
}

void FEditorViewportSnapping::ClearSnappingHelpers( bool bClearImmediately )
{
	VertexSnappingImpl.ClearSnappingHelpers(bClearImmediately);
}

void FEditorViewportSnapping::DrawSnappingHelpers(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	VertexSnappingImpl.DrawSnappingHelpers( View, PDI );
}



bool FEditorViewportSnapping::SnapLocationToNearestVertex( FVector& Location, const FVector2D& MouseLocation, FLevelEditorViewportClient* ViewportClient, FVector& OutVertexNormal, bool bDrawVertHelpers )
{
	bool bSnapped = false;
	if( IsSnapToVertexEnabled() )
	{
		bSnapped = VertexSnappingImpl.SnapLocationToNearestVertex( Location, MouseLocation, ViewportClient, OutVertexNormal, bDrawVertHelpers );
	}
	else
	{
		OutVertexNormal = FVector(EForceInit::ForceInitToZero);
	}

	return bSnapped;
}


bool FEditorViewportSnapping::SnapDraggedActorsToNearestVertex( FVector& DragDelta, FLevelEditorViewportClient* ViewportClient )
{
	bool bSnapped = false;
	if( IsSnapToVertexEnabled() && !DragDelta.IsNearlyZero() )
	{
		bSnapped = VertexSnappingImpl.SnapDraggedActorsToNearestVertex( DragDelta, ViewportClient );
	}
	return bSnapped;
}

bool FEditorViewportSnapping::SnapDragLocationToNearestVertex( const FVector& BaseLocation, FVector& DragDelta, FLevelEditorViewportClient* ViewportClient )
{
	bool bSnapped = false;
	if( IsSnapToVertexEnabled() && !DragDelta.IsNearlyZero() )
	{
		bSnapped = VertexSnappingImpl.SnapDragLocationToNearestVertex( BaseLocation, DragDelta, ViewportClient );
	}
	return bSnapped;
}

//////////////////////////////////////////////////////////////////////////
// FSnappingUtils

TSharedPtr<FEditorViewportSnapping> FSnappingUtils::EditorViewportSnapper;

bool FSnappingUtils::IsSnapToGridEnabled()
{
	return EditorViewportSnapper->IsSnapToGridEnabled();
}

bool FSnappingUtils::IsRotationSnapEnabled()
{
	return EditorViewportSnapper->IsSnapRotationEnabled();
}

bool FSnappingUtils::IsScaleSnapEnabled()
{
	return EditorViewportSnapper->IsSnapScaleEnabled();
}

bool FSnappingUtils::IsSnapToActorEnabled()
{
	return EditorViewportSnapper->IsSnapToActorEnabled();
}

void FSnappingUtils::EnableActorSnap(bool bEnable)
{
	EditorViewportSnapper->EnableActorSnap(bEnable);
}

float FSnappingUtils::GetActorSnapDistance(bool bScalar)
{
	return EditorViewportSnapper->GetActorSnapDistance(bScalar);
}

void FSnappingUtils::SetActorSnapDistance(float Distance)
{
	EditorViewportSnapper->SetActorSnapDistance(Distance);
}

bool FSnappingUtils::SnapActorsToNearestActor(FVector& DragDelta, FLevelEditorViewportClient* ViewportClient)
{
	return EditorViewportSnapper->SnapActorsToNearestActor(DragDelta, ViewportClient);
}

bool FSnappingUtils::SnapDraggedActorsToNearestVertex(FVector& DragDelta, FLevelEditorViewportClient* ViewportClient)
{
	return EditorViewportSnapper->SnapDraggedActorsToNearestVertex(DragDelta, ViewportClient);
}

bool FSnappingUtils::SnapDragLocationToNearestVertex(const FVector& BaseLocation, FVector& DragDelta, FLevelEditorViewportClient* ViewportClient)
{
	return EditorViewportSnapper->SnapDragLocationToNearestVertex(BaseLocation, DragDelta, ViewportClient);
}

bool FSnappingUtils::SnapLocationToNearestVertex(FVector& Location, const FVector2D& MouseLocation, FLevelEditorViewportClient* ViewportClient, FVector& OutVertexNormal, bool bDrawVertHelpers )
{
	return EditorViewportSnapper->SnapLocationToNearestVertex(Location, MouseLocation, ViewportClient, OutVertexNormal, bDrawVertHelpers );
}

void FSnappingUtils::SnapScale(FVector& Point, const FVector& GridBase)
{
	IViewportSnappingModule::GetSnapManager()->SnapScale(Point, GridBase);
}

void FSnappingUtils::SnapPointToGrid(FVector& Point, const FVector& GridBase)
{
	IViewportSnappingModule::GetSnapManager()->SnapPointToGrid(Point, GridBase);
}

void FSnappingUtils::SnapRotatorToGrid(FRotator& Rotation)
{
	IViewportSnappingModule::GetSnapManager()->SnapRotatorToGrid(Rotation);
}

bool FSnappingUtils::SnapToBSPVertex(FVector& Location, FVector GridBase, FRotator& Rotation)
{
	return EditorViewportSnapper->SnapToBSPVertex(Location, GridBase, Rotation);
}

void FSnappingUtils::ClearSnappingHelpers(bool bClearImmediately)
{
	IViewportSnappingModule::GetSnapManager()->ClearSnappingHelpers(bClearImmediately);
}

void FSnappingUtils::DrawSnappingHelpers(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	IViewportSnappingModule::GetSnapManager()->DrawSnappingHelpers(View, PDI);
}

void FSnappingUtils::InitEditorSnappingTools()
{
	EditorViewportSnapper = MakeShareable(new FEditorViewportSnapping);

	IViewportSnappingModule& Module = FModuleManager::LoadModuleChecked<IViewportSnappingModule>("ViewportSnapping");
	Module.RegisterSnappingPolicy(EditorViewportSnapper);
}
