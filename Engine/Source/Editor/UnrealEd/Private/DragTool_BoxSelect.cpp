// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "DragTool_BoxSelect.h"
#include "Components/PrimitiveComponent.h"
#include "CanvasItem.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "GameFramework/Volume.h"
#include "EngineUtils.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "ActorEditorUtils.h"
#include "ScopedTransaction.h"
#include "Engine/LevelStreaming.h"
#include "CanvasTypes.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FDragTool_BoxSelect
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Starts a mouse drag behavior.  The start location is snapped to the editor constraints if bUseSnapping is true.
 *
 * @param	InViewportClient	The viewport client in which the drag event occurred.
 * @param	InStart				Where the mouse was when the drag started.
 */
void FDragTool_ActorBoxSelect::StartDrag(FEditorViewportClient* InViewportClient, const FVector& InStart, const FVector2D& InStartScreen)
{
	FDragTool::StartDrag(InViewportClient, InStart, InStartScreen);
	
	FIntPoint MousePos;
	InViewportClient->Viewport->GetMousePos(MousePos);

	Start = FVector(MousePos);
	End = EndWk = Start;

	FLevelEditorViewportClient::ClearHoverFromObjects();

	// Create a list of bsp models to check for intersection with the box
	ModelsToCheck.Reset();
	// Do not select BSP if its not visible
	if( InViewportClient->EngineShowFlags.BSP)
	{
		UWorld* World = InViewportClient->GetWorld();
		check(World);
		// Add the persistent level always
		ModelsToCheck.Add( World->PersistentLevel->Model );
		// Add all streaming level models
		for( int32 LevelIndex = 0; LevelIndex < World->StreamingLevels.Num(); ++LevelIndex )
		{
			ULevelStreaming* StreamingLevel = World->StreamingLevels[LevelIndex];
			// Only add streaming level models if the level is visible
			if( StreamingLevel != NULL && StreamingLevel->bShouldBeVisibleInEditor )
			{	
				ULevel* Level = StreamingLevel->GetLoadedLevel();
				if ( Level != NULL )
				{
					ModelsToCheck.Add( Level->Model );
				}
			}
		}
	}
}

void FDragTool_ActorBoxSelect::AddDelta( const FVector& InDelta )
{
	FDragTool::AddDelta( InDelta );

	FIntPoint MousePos;
	LevelViewportClient->Viewport->GetMousePos(MousePos);

	End = FVector(MousePos); 
	EndWk = End;
	
	const bool bUseHoverFeedback = GEditor != NULL && GetDefault<ULevelEditorViewportSettings>()->bEnableViewportHoverFeedback;

	if( bUseHoverFeedback )
	{
		const bool bStrictDragSelection = GetDefault<ULevelEditorViewportSettings>()->bStrictBoxSelection;

		// If we are using over feedback calculate a new box from the one being dragged
		FBox SelBBox;
		CalculateBox( SelBBox );

		// Check every actor to see if it intersects the frustum created by the box
		// If it does, the actor will be selected and should be given a hover cue
		bool bSelectionChanged = false;
		UWorld* IteratorWorld = GWorld;
		for( FActorIterator It(IteratorWorld); It; ++It )
		{
			AActor& Actor = **It;
			const bool bActorHitByBox = IntersectsBox( Actor, SelBBox, bStrictDragSelection );

			if( bActorHitByBox )
			{
				// Apply a hover effect to any actor that will be selected
				AddHoverEffect( Actor );
			}
			else
			{
				// Remove any hover effect on this actor as it no longer will be selected by the current box
				RemoveHoverEffect( Actor );
			}
		}

		// Check each model to see if it will be selected
		for( int32 ModelIndex = 0; ModelIndex < ModelsToCheck.Num(); ++ModelIndex )
		{
			UModel& Model = *ModelsToCheck[ModelIndex];
			for (int32 NodeIndex = 0; NodeIndex < Model.Nodes.Num(); NodeIndex++)
			{
				if( IntersectsBox( Model, NodeIndex, SelBBox, bStrictDragSelection ) )
				{
					// Apply a hover effect to any bsp surface that will be selected
					AddHoverEffect( Model, Model.Nodes[NodeIndex].iSurf );
				}
				else
				{
					// Remove any hover effect on this bsp surface as it no longer will be selected by the current box
					RemoveHoverEffect( Model, Model.Nodes[NodeIndex].iSurf );
				}
			}
		}
	}
}
/**
* Ends a mouse drag behavior (the user has let go of the mouse button).
*/
void FDragTool_ActorBoxSelect::EndDrag()
{
	const bool bGeometryMode = ModeTools->IsModeActive(FBuiltinEditorModes::EM_Geometry);
	
	FScopedTransaction Transaction( NSLOCTEXT("ActorFrustumSelect", "MarqueeSelectTransation", "Marquee Select" ) );

	bool bShouldSelect = true;
	FBox SelBBox;
	CalculateBox( SelBBox );

	if( bControlDown )
	{
		// If control is down remove from selection
		bShouldSelect = false;
	}
	else if( !bShiftDown )
	{
		// If the user is selecting, but isn't hold down SHIFT, remove all current selections.
		ModeTools->SelectNone();
	}

	// Let the editor mode try to handle the box selection.
	const bool bEditorModeHandledBoxSelection = ModeTools->BoxSelect(SelBBox, bLeftMouseButtonDown);

	// If the edit mode didn't handle the selection, try normal actor box selection.
	if ( !bEditorModeHandledBoxSelection )
	{
		const bool bStrictDragSelection = GetDefault<ULevelEditorViewportSettings>()->bStrictBoxSelection;

		if( bControlDown )
		{
			// If control is down remove from selection
			bShouldSelect = false;
		}
		else if( !bShiftDown )
		{
			// If the user is selecting, but isn't hold down SHIFT, remove all current selections.
			GEditor->SelectNone( true, true );
		}

		// Select all actors that are within the selection box area.  Be aware that certain modes do special processing below.	
		bool bSelectionChanged = false;
		UWorld* IteratorWorld = GWorld;
		const TArray<FName>& HiddenLayers = LevelViewportClient->ViewHiddenLayers;
		for( FActorIterator It(IteratorWorld); It; ++It )
		{
			AActor* Actor = *It;
			
			bool bActorIsVisible = true;
			for ( auto Layer : Actor->Layers )
			{
				// Check the actor isn't in one of the layers hidden from this viewport.
				if( HiddenLayers.Contains( Layer ) )
				{
					bActorIsVisible = false;
					break;
				}
			}

			// Select the actor if we need to
			if( bActorIsVisible && IntersectsBox( *Actor, SelBBox, bStrictDragSelection ) )
			{
				GEditor->SelectActor( Actor, bShouldSelect, false );
				bSelectionChanged = true;
			}
		}

		// Check every model to see if its BSP surfaces should be selected
		for( int32 ModelIndex = 0; ModelIndex < ModelsToCheck.Num(); ++ModelIndex )
		{
			UModel& Model = *ModelsToCheck[ModelIndex];
			// Check every node in the model
			for (int32 NodeIndex = 0; NodeIndex < Model.Nodes.Num(); NodeIndex++)
			{
				if( IntersectsBox( Model, NodeIndex, SelBBox, bStrictDragSelection ) )
				{
					// If the node intersected the frustum select the corresponding surface
					GEditor->SelectBSPSurf( &Model, Model.Nodes[NodeIndex].iSurf, bShouldSelect, false );
					bSelectionChanged = true;
				}
			}
		}

		if ( bSelectionChanged )
		{
			// If any selections were made.  Notify that now.
			GEditor->NoteSelectionChange();
		}
	}

	// Clear any hovered objects that might have been created while dragging
	FLevelEditorViewportClient::ClearHoverFromObjects();

	// Clean up.
	FDragTool::EndDrag();
}

void FDragTool_ActorBoxSelect::Render(const FSceneView* View, FCanvas* Canvas)
{
	FCanvasBoxItem BoxItem(FVector2D(Start.X, Start.Y), FVector2D(End.X - Start.X, End.Y - Start.Y));
	BoxItem.SetColor(FLinearColor::White);
	Canvas->DrawItem(BoxItem);
}

void FDragTool_ActorBoxSelect::CalculateBox( FBox& OutBox )
{
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		LevelViewportClient->Viewport,
		LevelViewportClient->GetScene(),
		LevelViewportClient->EngineShowFlags)
		.SetRealtimeUpdate(LevelViewportClient->IsRealtime()));

	FSceneView* View = LevelViewportClient->CalcSceneView(&ViewFamily);

	FVector4 StartScreenPos = View->PixelToScreen(Start.X, Start.Y, 0);
	FVector4 EndScreenPos = View->PixelToScreen(End.X, End.Y, 0);

	FVector TransformedStart = View->ScreenToWorld(View->PixelToScreen(Start.X, Start.Y, 0.5f));
	FVector TransformedEnd = View->ScreenToWorld(View->PixelToScreen(End.X, End.Y, 0.5f));

	// Create a bounding box based on the start/end points (normalizes the points).
	OutBox.Init();
	OutBox += TransformedStart;
	OutBox += TransformedEnd;

	switch(LevelViewportClient->ViewportType)
	{
	case LVT_OrthoXY:
	case LVT_OrthoNegativeXY:
		OutBox.Min.Z = -WORLD_MAX;
		OutBox.Max.Z = WORLD_MAX;
		break;
	case LVT_OrthoXZ:
	case LVT_OrthoNegativeXZ:
		OutBox.Min.Y = -WORLD_MAX;
		OutBox.Max.Y = WORLD_MAX;
		break;
	case LVT_OrthoYZ:
	case LVT_OrthoNegativeYZ:
		OutBox.Min.X = -WORLD_MAX;
		OutBox.Max.X = WORLD_MAX;
		break;
	case LVT_OrthoFreelook:
	case LVT_Perspective:
		break;
	}
}


/** 
 * Returns true if the passed in Actor intersects with the provided box 
 *
 * @param InActor				The actor to check
 * @param InBox					The box to check against
 * @param bUseStrictSelection	true if the actor must be entirely within the frustum
 */
bool FDragTool_ActorBoxSelect::IntersectsBox( AActor& InActor, const FBox& InBox, bool bUseStrictSelection )
{
	bool bActorHitByBox = false;

	const bool bGeometryMode = ModeTools->IsModeActive(FBuiltinEditorModes::EM_Geometry);

	// Check for special cases (like certain show flags that might hide an actor)
	bool bActorIsHiddenByShowFlags = false;

	// Check to see that volume actors are visible in the viewport
	if( InActor.IsA(AVolume::StaticClass()) && (!LevelViewportClient->EngineShowFlags.Volumes || !LevelViewportClient->IsVolumeVisibleInViewport(InActor) ) )
	{
		bActorIsHiddenByShowFlags = true;
	}

	// Never drag-select hidden actors or builder brushes. Also, don't consider actors which haven't been recently rendered.
	//@TODO - replace with proper check for if this object was visible last frame.  This is viewport dependent and viewports can use different concepts of time
	//depending on if they are in "realtime" mode or not.  See FLevelEditorViewportClient::Draw for the differing concepts of time.
	const bool bActorRecentlyRendered = true;//Actor->LastRenderTime > ( GWorld->GetTimeSeconds() - 1.0f );
	if( !bActorIsHiddenByShowFlags && !InActor.IsHiddenEd() && !FActorEditorUtils::IsABuilderBrush(&InActor) && bActorRecentlyRendered )
	{
		// Iterate over all actor components, selecting out primitive components
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		InActor.GetComponents(PrimitiveComponents);

		for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			check(PrimitiveComponent != nullptr);
			if (PrimitiveComponent->IsRegistered() && PrimitiveComponent->IsVisibleInEditor())
			{
				if (PrimitiveComponent->ComponentIsTouchingSelectionBox(InBox, LevelViewportClient->EngineShowFlags, bGeometryMode, bUseStrictSelection))
				{
					bActorHitByBox = true;
					break;
				}
			}
		}
	}

	return bActorHitByBox;
}

/** 
 * Returns true if the provided BSP node intersects with the provided frustum 
 *
 * @param InModel				The model containing BSP nodes to check
 * @param NodeIndex				The index to a BSP node in the model.  This node is used for the bounds check.
 * @param InFrustum				The frustum to check against.
 * @param bUseStrictSelection	true if the node must be entirely within the frustum
 */
bool FDragTool_ActorBoxSelect::IntersectsBox( const UModel& InModel, int32 NodeIndex, const FBox& InBox, bool bUseStrictSelection ) const
{
	FBox NodeBB;
	InModel.GetNodeBoundingBox( InModel.Nodes[NodeIndex], NodeBB );

	bool bFullyContained = false;
	bool bIntersects = false;
	if( !bUseStrictSelection )
	{
		bIntersects = InBox.Intersect( NodeBB );
	}
	else
	{
		bIntersects = InBox.IsInside( NodeBB.Max ) && InBox.IsInside( NodeBB.Min );
	}

	return bIntersects;
}

/** Adds a hover effect to the passed in actor */
void FDragTool_ActorBoxSelect::AddHoverEffect( AActor& InActor )
{
	FViewportHoverTarget HoverTarget( &InActor );
	FLevelEditorViewportClient::AddHoverEffect( HoverTarget );
	FLevelEditorViewportClient::HoveredObjects.Add( HoverTarget );
}

/** Removes a hover effect from the passed in actor */
void FDragTool_ActorBoxSelect::RemoveHoverEffect( AActor& InActor  )
{
	FViewportHoverTarget HoverTarget( &InActor );
	FSetElementId Id = FLevelEditorViewportClient::HoveredObjects.FindId( HoverTarget );
	if( Id.IsValidId() )
	{
		FLevelEditorViewportClient::RemoveHoverEffect( HoverTarget );
		FLevelEditorViewportClient::HoveredObjects.Remove( Id );
	}
}

/** Adds a hover effect to the passed in bsp surface */
void FDragTool_ActorBoxSelect::AddHoverEffect( UModel& InModel, int32 SurfIndex )
{
	FViewportHoverTarget HoverTarget( &InModel, SurfIndex );
	FLevelEditorViewportClient::AddHoverEffect( HoverTarget );
	FLevelEditorViewportClient::HoveredObjects.Add( HoverTarget );
}

/** Removes a hover effect from the passed in bsp surface */
void FDragTool_ActorBoxSelect::RemoveHoverEffect( UModel& InModel, int32 SurfIndex )
{
	FViewportHoverTarget HoverTarget( &InModel, SurfIndex );
	FSetElementId Id = FLevelEditorViewportClient::HoveredObjects.FindId( HoverTarget );
	if( Id.IsValidId() )
	{
		FLevelEditorViewportClient::RemoveHoverEffect( HoverTarget );
		FLevelEditorViewportClient::HoveredObjects.Remove( Id );
	}
}
