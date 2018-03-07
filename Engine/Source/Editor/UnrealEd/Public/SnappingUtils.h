// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FEditorViewportSnapping;
class FLevelEditorViewportClient;
class FPrimitiveDrawInterface;
class FSceneView;

class UNREALED_API FSnappingUtils
{
public:
	/**
	 * @return true if snapping (translation) to the grid is enabled
	 */
	static bool IsSnapToGridEnabled();

	/**
	 * @return true if orientation snapping is enabled
	 */
	static bool IsRotationSnapEnabled();

	/**
	 * @return true if orientation snapping is enabled
	 */
	static bool IsScaleSnapEnabled();

	/**
	 * @return true if snapping actors to other actors is enabled
	 */
	static bool IsSnapToActorEnabled();

	/** Set user setting for actor snap. */
	static void EnableActorSnap(bool bEnable);

	/** Access user setting for distance. Fractional 0.0->100.0 */
	static float GetActorSnapDistance(bool bScalar = false);

	/** Set user setting for distance. Fractional 0.0->100.0 */
	static void SetActorSnapDistance(float Distance);

	/**
	 * Attempts to snap the selected actors to the nearest other actor
	 *
	 * @param DragDelta			The current world space drag amount
	 * @param ViewportClient	The viewport client the user is dragging in
	 */
	static bool SnapActorsToNearestActor( FVector& DragDelta, FLevelEditorViewportClient* ViewportClient );

	/**
	 * Snaps actors to the nearest vertex on another actor
	 *
	 * @param DragDelta			The current world space drag amount that will be modified to account for snapping to a vertex
	 * @param ViewportClient	The viewport client the user is dragging in
	 * @return true if anything was snapped
	 */
	static bool SnapDraggedActorsToNearestVertex( FVector& DragDelta, FLevelEditorViewportClient* ViewportClient );

	/**
	 * Snaps a delta drag movement to the nearest vertex
	 *
	 * @param BaseLocation		Location that should be snapped before any drag is applied
	 * @param DragDelta			Delta drag movement that should be snapped.  This value will be updated such that BaseLocation+DragDelta is the nearest snapped verted
	 * @param ViewportClient	The viewport client being dragged in.
	 * @return true if anything was snapped
	 */
	static bool SnapDragLocationToNearestVertex( const FVector& BaseLocation, FVector& DragDelta, FLevelEditorViewportClient* ViewportClient );

	/**
	 * Snaps a location to the nearest vertex
	 *
	 * @param Location			The location to snap
	 * @param MouseLocation		The current 2d mouse location.  Vertices closer to the mouse are favored
	 * @param ViewportClient	The viewport client being used
	 * @param OutVertexNormal	The normal at the closest vertex
	 * @return true if anything was snapped
	 */
	static bool SnapLocationToNearestVertex( FVector& Location, const FVector2D& MouseLocation, FLevelEditorViewportClient* ViewportClient, FVector& OutVertexNormal, bool bDrawVertHelpers );

	/**
	 * Snaps a scale value to the scale grid
	 *
	 * @param Point	The point to snap.  This value will be modified to account for snapping
	 * @param GridBase	Base grid offset
	 */
	static void SnapScale( FVector& Point, const FVector& GridBase );

	/**
	 * Snaps a point value to the positional grid
	 *
	 * @param Point	The point to snap.  This value will be modified to account for snapping
	 * @param GridBase	Base grid offset
	 */
	static void SnapPointToGrid( FVector& Point, const FVector& GridBase );

	/**
	 * Snaps a rotator to the rotational grid
	 *
	 * @param Rotation	The rotator to snap.  This value will be modified to account for snapping
	 */
	static void SnapRotatorToGrid( FRotator& Rotation );

	static bool SnapToBSPVertex( FVector& Location, FVector GridBase, FRotator& Rotation );

	/**
	 * Clears all vertices being drawn to help a user snap
	 *
	 * @param bClearImmediatley	true to clear helpers without fading them out
	 */
	static void ClearSnappingHelpers( bool bClearImmediately = false);

	/**
	 * Draws snapping helpers 
	 *
	 * @param View	The current view of the scene
	 * @param PDI	Drawing interface
	 */
	static void DrawSnappingHelpers(const FSceneView* View,FPrimitiveDrawInterface* PDI); 

	/**
	 * Initialize the snapping system
	 */
	static void InitEditorSnappingTools();

private:
	/** Built-in editor snap implementation */
	static TSharedPtr<class FEditorViewportSnapping> EditorViewportSnapper;

};
